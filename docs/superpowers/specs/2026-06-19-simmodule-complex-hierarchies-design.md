# SimModule 复杂层次设计 (Complex SimModule Hierarchies)

**Status**: Draft v0.1 · **Date**: 2026-06-19 · **Branch**: main · **Author**: Sisyphus (brainstorming)
**Scope**: v2.2 SimModule 体系扩展，参考 gem5 复杂 SoC 配置模式

---

## 0. 概述

### 0.1 目标

为 CppTLM `SimModule` 体系引入**复杂多级层次**支持，参考 gem5 的 `SubSystem` + `incorporate_cache(board)` late-binding + 动态 per-core 子模块生成模式，使 SoC 集成可使用可复用 JSON 模板进行层次化实例化。

### 0.2 范围

- **新增 8 个 SimModule 派生类**：`ComputeCluster`, `TpcCluster`, `GpcCluster`, `GpuCluster`, `CacheCluster`, `MemoryCluster`, `GpuNoC`, `ApuSoC`
- **JSON 模板复用机制**：`cu_template` + `cu_count` 字段，从单一蓝图生成 N 份实例
- **关键 API 修复**：`SimModule::findInternalPath` 递归（D.4）+ `tick` 默认递归（D.5）
- **可选 P3/P5 增强**：ChStream helper 方法 + `incorporate_parent` late-binding 钩子

### 0.3 非目标 (YAGNI)

- 不引入新 JSON 字段（如 `$inherit`、端口类型系统）—— 待 P5 后视用户反馈
- 不重写现有 `CpuCluster` —— 仅扩展接口，保留向后兼容
- 不触碰 GPU 端 Phase 7.B/7.D 已规划模块（`ComputeUnitTLM`/`KernelLaunchTLM`/`TCC_TLM`）—— 仅作为 SimModule 的子 leaf
- 不重命名 `CpuCluster` —— 用户已确认是笔误，应为 `GpuCluster`

### 0.4 核心决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 设计方案 | **方案 C：增量分层** | 5 阶段递进，每阶段独立可发布，风险最低 |
| API 修复优先级 | **P1 先修 D.4 + D.5** | 解锁现有 3 层 JSON，端到端跑通 |
| JSON 模板位置 | **`configs/templates/`** | 与 `$include` 一致，版本可控 |
| 命名约定 | **CamelCase 后缀 `Cluster`** | 与现有 `CpuCluster` 一致；`GpuNoC`/`ApuSoC` 不用 `Cluster` 因角色特殊 |
| ChStream helper | **P3 延后（依赖 D.1 修复）** | D.1 修复成本高，P2 后再评估 |
| `incorporate_parent` 钩子 | **P5 引入（gem5 late-binding）** | P1-P4 阶段靠 `outputs`/`inputs` 显式连接即可 |

---

## 1. 背景与动机

### 1.1 现状 (v2.2, 2026-06-19)

- `SimModule` 基类已存在 (`include/core/sim_module.hh`)，MAX_DEPTH=8
- 唯一活跃派生类：`CpuCluster` (`include/tlm/cluster/cpu_cluster.hh`)
- JSON 嵌套示例：`configs/example_simmodule_nested_{2level,3level_static}.json`
- 测试覆盖：`test/test_simmodule_nested.cc` 7 用例 + `test/python/test_simmodule_emitter.py` 6 用例
- SoC 蓝图：`docs/soc_arch/specs/apu-soc-design.md §2.2` 显式画出 CPU/GPU Cluster 图表

### 1.2 痛点

1. **唯一 SimModule 派生类**：`MemoryCluster`/`CacheCluster` 等在 `include/AGENTS.md` 明文承诺但未实现
2. **3 层 JSON 配置存在但未端到端测试**：`example_simmodule_nested_3level_static.json` 的 `outer.outputs[0].internal = "mid.cpu0_to_bus"` 因 `findInternalPath` 不递归，实际**无法工作**
3. **gem5 复杂 SoC 模式未借鉴**：`incorporate_cache(board)` late-binding、动态 per-core 子模块生成、`connectCPU/connectBus` helper 均未引入
4. **GPU 端层次缺失**：`GpuCluster → GpcCluster → TpcCluster → ComputeCluster → CU` 4 级层次完全没有容器
5. **JSON 模板复用能力弱**：现有 `$extends` / `$include` 灵活但缺乏"单蓝图生成 N 份"机制

### 1.3 参考：gem5 模式

调研来源：`docs/research/gem5-soc-survey.md` + `/workspace/project/gem5` 源码。

**4 层 SubSystem 层次**（gem5）：

```
SimObject
  └─ SubSystem
       ├─ AbstractBoard (L0)
       ├─ AbstractCacheHierarchy / AbstractProcessor (L1)
       │    └─ PrivateL1PrivateL2CacheHierarchy (L2)
       └─ AbstractCore / SimpleProcessor (L1)
            └─ SimpleCore (L2)
```

**关键模式**：

- **Late-Binding Incorporation**：`board._connect_things()` 阶段调 `cache_hierarchy.incorporate_cache(board)`，子 Cluster 在此阶段知道父对象
- **动态子模块生成**：`for i, cpu in enumerate(board.get_processor().get_cores())` 在 `incorporate_cache` 内部为每个 core 创建 L1+L2 cache
- **Helper 连接方法**：`L1Cache.connectCPU(cpu)` / `connectBus(bus)` / `L2Cache.connectCPUSideBus(bus)` / `connectMemSideBus(bus)` 隐藏端口方向
- **参数化模板**：`SimpleProcessor(cpu_type, num_cores, isa)` 内部循环创建 N 个 SimpleCore

---

## 2. 关键 API 限制 (修复目标)

| ID | 限制 | 证据 | 修复阶段 | 修复成本 |
|----|------|------|----------|----------|
| **D.1** | `getInternalOutputPort` 对 ChStream 模块永远返回 nullptr | `test_simmodule_nested.cc:194-207` WARN | P5 (可选) | 高（需重构 ChStream 端口表） |
| **D.4** | `findInternalPath` 不递归，3 层 JSON 暴露端口无法工作 | `sim_module.hh:149-161` + `example_simmodule_nested_3level_static.json:13` | **P1** | 低（~15 行） |
| **D.5** | `tick` 链只对顶层注册，嵌套 SimModule 在第一个 cycle 后链断 | `cpu_cluster.hh:49-55` + `module_factory.cc:802-806` | **P1** | 低（~10 行） |
| **D.8** | 每 SimModule 创建独立 ModuleFactory，5+ 层嵌套开销大 | `sim_module.hh:53` unique_ptr | 暂不修 | 中（需重构工厂共享） |
| **D.9/D.10** | `set_config` 嵌套时不递归 | `sim_module.hh:81-109` | **P2**（在子类 `set_config` 显式处理） | 低 |
| **D.11** | CpuCluster 无 `get_param_schema` 校验 | `sim_object.hh:305-307` | 暂不修 | 低 |

---

## 3. 5 Phase 路线图

| Phase | 名称 | API 改动 | 新增类 | 完成判定 | 预计 LOC |
|-------|------|----------|--------|----------|----------|
| **P1** | 修 D.4 + D.5 | `sim_module.hh` 递归 findInternalPath + 默认 tick 递归 | 0 | 3 层 JSON 端到端跑通 | ~25 |
| **P2** | GPU 端 4 核心类 | 0 | ComputeCluster, TpcCluster, GpcCluster, GpuCluster | `gpu_2gpc_2tpc_2cu.json` 跑通 | ~200 |
| **P3** | ChStream helper | `cache_tlm.hh` / `crossbar_tlm.hh` 加 `connectCPU` 等 | 0 | 现有 82 测试 + 新 helper 测试通过 | ~150 |
| **P4** | 基础设施 3 类 | 0 | CacheCluster, MemoryCluster, GpuNoC | APU_SoC 不含顶层跑通 | ~250 |
| **P5** | 顶层 + incorporate 钩子 | `sim_module.hh` 加 `incorporate_parent` | ApuSoC | 完整 APU JSON 跑通 + 文档同步 | ~150 |

**总预计**: ~775 行 C++ + ~600 行测试 + ~500 行 JSON + ~200 行 Python + ~400 行文档

---

## 4. 详细组件设计

### 4.1 P1 - API 修复

#### 4.1.1 Fix D.4: 递归 `findInternalPath`

**文件**：`include/core/sim_module.hh:149-161`

**当前实现**（仅查当前层）：
```cpp
std::string findInternalPath(const std::string& external_label) const override {
    auto it = internal_to_external_map.find(external_label);
    if (it != internal_to_external_map.end()) return it->second;
    return "";
}
```

**修复后**（递归到子 SimModule）：
```cpp
std::string findInternalPath(const std::string& external_label) const override {
    // 1. 先查当前层
    auto it = internal_to_external_map.find(external_label);
    if (it != internal_to_external_map.end()) return it->second;
    // 2. 递归到子 SimModule（返回 "<子模块名>.<内部路径>"）
    for (const auto& kv : internal_factory->getAllInstances()) {
        if (auto* sub = dynamic_cast<SimModule*>(kv.second)) {
            std::string sub_path = sub->findInternalPath(external_label);
            if (!sub_path.empty()) return kv.first + "." + sub_path;
        }
    }
    return "";
}
```

**性能影响**：O(N) 遍历，N 为内部子模块数。在 MAX_DEPTH=8 限制下最坏 O(8) × 每层 O(N)，可接受。

#### 4.1.2 Fix D.5: 默认递归 `tick`

**文件**：`include/core/sim_module.hh`（新增方法）

**当前问题**：`CpuCluster::tick` 手动实现但不通用（`cpu_cluster.hh:49-55`），嵌套 SimModule 子类无默认实现。

**修复后**（在 SimModule 基类提供默认递归）：
```cpp
// sim_module.hh 新增
virtual void tick() override {
    for (const auto& kv : internal_factory->getAllInstances()) {
        if (kv.second) kv.second->tick();
    }
}
```

**向后兼容**：CpuCluster 现有 `tick()` 显式实现保留，编译时基类默认实现对其无影响。

### 4.2 P2 - 4 个 GPU 端 SimModule

#### 4.2.1 `ComputeCluster` (P2 核心)

**文件**：`include/tlm/cluster/compute_cluster.hh` + `src/tlm/cluster/compute_cluster.cc`

**职责**：从 JSON 蓝图模板复制 N 份 CU 子模块

**接口**：
```cpp
class ComputeCluster : public SimModule {
public:
    explicit ComputeCluster(const std::string& n, EventQueue* eq)
        : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params) override {
        SimModule::set_config(params);
        if (params.contains("cu_template")) cu_template_path_ = params["cu_template"];
        if (params.contains("cu_count")) cu_count_ = params["cu_count"].get<int>();
        if (cu_count_ < 1) cu_count_ = 1;
        if (cu_count_ > 64) { cu_count_ = 64; WARN("cu_count clamped to 64"); }
    }

    std::string get_module_type() const override { return "ComputeCluster"; }

    void simulate_instantiate(const nlohmann::json& cfg) override;

private:
    std::string cu_template_path_;
    int cu_count_ = 1;
};
```

**`simulate_instantiate` 实现**（`src/tlm/cluster/compute_cluster.cc`）：
```cpp
void ComputeCluster::simulate_instantiate(const nlohmann::json& cfg) {
    // 1. 调用基类处理基础子模块 + outputs/inputs
    SimModule::simulate_instantiate(cfg);
    // 2. 加载 cu_template 并复制 cu_count_ 份
    if (cu_template_path_.empty()) return;
    auto tmpl = JsonIncluder::loadAndInclude(cu_template_path_);
    if (!tmpl.contains("modules")) {
        throw std::runtime_error("ComputeCluster: cu_template must contain 'modules' array: " + cu_template_path_);
    }
    for (int i = 0; i < cu_count_; ++i) {
        auto cu = tmpl;
        cu["name"] = "cu" + std::to_string(i);
        // 3. 内部 factory 实例化（递归触发子 SimModule 嵌套）
        internal_factory->instantiateAll(cu);
    }
}
```

**JSON 模板** (`configs/templates/compute_unit_v1.json`)：
```json
{
  "description": "Single Compute Unit blueprint (AMD-style SIMD with ScalarCache)",
  "modules": [
    { "name": "scalar_cache", "type": "CacheTLM",
      "params": { "size": "16KB", "assoc": 4 } },
    { "name": "vector_reg",   "type": "VectorRegFileTLM",
      "params": { "num_regs": 64, "lanes": 16 } },
    { "name": "wavefront",    "type": "WavefrontTLM",
      "params": { "max_wavefronts": 10 } }
  ]
}
```

**JSON 使用示例**（`configs/gpu_2gpc_2tpc_2cu.json` 片段）：
```json
{
  "modules": [{
    "name": "compute_grp",
    "type": "ComputeCluster",
    "params": {
      "cu_template": "./templates/compute_unit_v1.json",
      "cu_count": 2
    }
  }]
}
```

#### 4.2.2 `TpcCluster` (P2)

**职责**：1 个 ComputeCluster + 共享 TextureUnit + Frontend 调度

**关键 params**：`tpc_id`, `cu_per_tpc`, 内部 `modules[]` 含 `ComputeCluster` + `TextureUnit`

**实现模式**：与 ComputeCluster 类似，但固定 `cu_count = cu_per_tpc` (单一 ComputeCluster 内含多 CU)

#### 4.2.3 `GpcCluster` (P2)

**职责**：M 个 TpcCluster + 共享 Frontend (Geometry/Rasterizer)

**关键 params**：`gpc_id`, `tpc_per_gpc`, 内部 `modules[]` 动态生成 TpcCluster

#### 4.2.4 `GpuCluster` (P2)

**职责**：K 个 GpcCluster + TCC_TLM + KernelLaunchTLM

**关键 params**：`gpc_count`, `tpc_per_gpc`, `cu_per_tpc`, `cu_template`

**JSON 使用**（完整 GPU 顶层）：
```json
{
  "name": "gpu",
  "type": "GpuCluster",
  "params": {
    "gpc_count": 2,
    "tpc_per_gpc": 2,
    "cu_per_tpc": 2,
    "cu_template": "./templates/compute_unit_v1.json"
  },
  "modules": [
    { "name": "kernel_launch", "type": "KernelLaunchTLM",
      "params": { "max_kernels": 8 } },
    { "name": "tcc", "type": "TCC_TLM",
      "params": { "size": "1MB", "line_size": 64 } }
  ],
  "outputs": [
    { "internal": "gpc0.tpc0.compute_grp.cu0.req_out",
      "external": "gpu_to_noc" }
  ],
  "inputs": [
    { "internal": "gpc0.tpc0.compute_grp.cu0.resp_in",
      "external": "noc_to_gpu" }
  ]
}
```

### 4.3 P3 - ChStream Helper 连接方法

**文件**：`include/tlm/cache_tlm.hh`, `include/tlm/crossbar_tlm.hh`

**新增方法**（借鉴 gem5 `caches.py:46-135`）：
```cpp
class CacheTLM : public ChStreamModuleBase {
public:
    // P3: helper 方法 - 隐藏 cpu_side/mem_side 端口方向
    void connectCPU(SimModule* cluster, int core_idx);
    void connectBus(SimModule* bus);
};

class CrossbarTLM : public ChStreamModuleBase {
public:
    void connectCPUSideBus(SimModule* bus);
    void connectMemSideBus(SimModule* bus);
};
```

**实现**（`connectCPU` 示例）：
```cpp
void CacheTLM::connectCPU(SimModule* cluster, int core_idx) {
    // 查找 cluster 的 cu<core_idx>.req_out 端口
    std::string path = "cu" + std::to_string(core_idx) + ".req_out";
    auto* master = cluster->getInternalOutputPort(path);
    if (!master) {
        throw std::runtime_error("CacheTLM::connectCPU: port not found: " + path);
    }
    // 建立连接 (cpu_side = 输入)
    auto* slave = dynamic_cast<SlavePort*>(getPortManager().getUpstreamPort("cpu_side"));
    slave->bind(master);
}
```

**依赖**：D.1 修复（`getInternalOutputPort` 对 ChStream 可见）。如果 D.1 未修，P3 必须延后或绕开。

### 4.4 P4 - 基础设施 3 类

#### 4.4.1 `CacheCluster`

**职责**：L1 × N (私有) + L2 (共享) cache 子系统

**关键 params**：`l1_count`, `l1_size`, `l2_size`, `l1_template`

**实现模式**：
1. 实例化 L2CacheTLM (单例)
2. 实例化 L1CacheTLM × l1_count (从模板)
3. 自动连接：每个 L1.mem_side → L2.cpu_side

#### 4.4.2 `MemoryCluster`

**职责**：多通道 HBM/DDR5 + Arbiter + QoS

**关键 params**：`channel_count`, `channel_size`, `memory_type` (`HBM`/`DDR5`/`DDR4`)

**实现模式**：
1. 实例化 MemoryTLM × channel_count
2. 实例化 ArbiterTLM (N→1)
3. 自动连接：MemoryTLM.cpu_side → Arbiter.mem_side

#### 4.4.3 `GpuNoC`

**职责**：Garnet 风格 N×N mesh NoC (RouterTLM + LinkTLM + NICTLM)

**关键 params**：`mesh_size` (如 4 = 4x4), `routing` (`XY`/`west_first`)

**实现模式**：
1. 实例化 RouterTLM × (mesh_size²)
2. 实例化 LinkTLM × (邻接 × 2 双向)
3. 自动连接：Router[i][j] ↔ Router[i+1][j] / Router[i][j+1]
4. 暴露 `north/south/east/west`/`local` 端口供 NIC 接入

### 4.5 P5 - 顶层 + incorporate 钩子

#### 4.5.1 `ApuSoC`

**职责**：CPU cluster + GPU cluster + 跨域 CoherentXBar + Memory cluster 顶层容器

**关键 params**：`cpu_topology` (引用), `gpu_topology` (引用)

**JSON 使用**：
```json
{
  "name": "apu_top",
  "type": "ApuSoC",
  "params": {
    "cpu_topology": "./templates/cpu_cluster_2level.json",
    "gpu_topology": "./templates/gpu_2gpc_2tpc_2cu.json"
  },
  "modules": [
    { "name": "xbar", "type": "CoherentXBarTLM",
      "params": { "ports": 8, "protocol": "MOESI" } }
  ]
}
```

#### 4.5.2 `SimModule::incorporate_parent` 钩子 (P5)

**文件**：`include/core/sim_module.hh`（新增方法）

**灵感**：gem5 `AbstractCacheHierarchy.incorporate_cache(board)` / `AbstractProcessor.incorporate_processor(board)`

```cpp
class SimModule {
public:
    // P5: 父对象在挂载后调用 - 默认递归到子模块
    virtual void incorporate_parent(SimModule* parent) {
        for (const auto& kv : internal_factory->getAllInstances()) {
            if (auto* sub = dynamic_cast<SimModule*>(kv.second)) {
                sub->incorporate_parent(this);
            }
        }
    }
};
```

**`GpuCluster::incorporate_parent` 重写**（示例）：
```cpp
void GpuCluster::incorporate_parent(SimModule* parent) override {
    SimModule::incorporate_parent(parent);
    if (auto* soc = dynamic_cast<ApuSoC*>(parent)) {
        // gem5 模式: 父对象问"你的 CU 在哪?", 自动建立跨域连接
        auto* req_port = this->getInternalOutputPort("gpc0.tpc0.compute_grp.cu0.req_out");
        if (req_port) soc->connectGpuToCoherentBus(req_port);
    }
}
```

---

## 5. 数据流

### 5.1 实例化流程 (5 Phase 完整路径)

```
JSON (apu_soc_v1.json)
  ↓ JsonIncluder::loadAndInclude (处理 $include, cu_template 路径)
  ↓ VarResolver::resolveAll (处理 ${var} 占位符)
  ↓
ModuleFactory::instantiateAll (顶层)
  ↓ Step 0: validateConfig
  ↓ Step 2: 实例化 ApuSoC (set_config + on_config_loaded)
  ↓ Step 4.5: 触发 ApuSoC.simulate_instantiate
      ↓ ApuSoC::simulate_instantiate
          ↓ 1. SimModule::simulate_instantiate (处理基础子模块)
          ↓ 2. instantiateAll(cpu_topology)
          ↓    → CpuCluster::simulate_instantiate
          ↓        → CacheCluster::simulate_instantiate (P4 递归)
          ↓            → 创 L2CacheTLM + L1CacheTLM × N
          ↓ 3. instantiateAll(gpu_topology)
          ↓    → GpuCluster::simulate_instantiate
          ↓        → GpcCluster × K::simulate_instantiate
          ↓            → TpcCluster × M::simulate_instantiate
          ↓                → ComputeCluster::simulate_instantiate
          ↓                    → load cu_template.json (P2 模板机制)
          ↓                    → for i in [0..cu_count):
          ↓                        - clone 蓝图
          ↓                        - 改 name 为 "cu<i>"
          ↓                        - internal_factory->instantiateAll(cloned)
          ↓                            → ComputeUnitTLM × cu_count 实例化
          ↓ 4. P5: incorporate_parent(parent=ApuSoC) 跨域布线
  ↓ Step 5: resolveConnections (顶层 connections)
  ↓ Step 7: StreamAdapter 注入
  ↓ Step 8: StatGroup 注册
```

### 5.2 模板复用数据流 (P2 核心)

```
1. 父 JSON 声明: "cu_template": "./templates/compute_unit_v1.json"
2. ComputeCluster::set_config 存储路径 + cu_count
3. ComputeCluster::simulate_instantiate:
   a. SimModule::simulate_instantiate(cfg)  // 基础子模块
   b. JsonIncluder::loadAndInclude(tmpl_path)  // 加载蓝图
   c. for i in [0..cu_count):
      - auto cu = tmpl  // 深拷贝
      - cu["name"] = "cu<i>"  // 唯一命名
      - internal_factory->instantiateAll(cu)  // 独立实例化
4. 每个 cu<i> 完全独立: 自己的 ScalarCache + VectorRegFile + Wavefront
5. 跨 cu<i> 端口连接 (如 cu0.req_out → L1Cache) 走标准 outputs/inputs 机制
```

### 5.3 端口连接数据流 (端到端)

```
cu0.req_out (MasterPort, ChStream)
  → ComputeCluster.cu0.req_out (经 parsePortSpec 切分)
  → TpcCluster.compute_grp.cu0.req_out (跨 SimModule 引用)
  → GpcCluster.tpc0.compute_grp.cu0.req_out
  → GpuCluster.gpc0.tpc0.compute_grp.cu0.req_out
  → 通过 outputs[{internal: "gpc0.tpc0.compute_grp.cu0.req_out",
                  external: "gpu0_cu0_to_noc"}] 暴露
  → ApuSoC 顶层 connections: [{src: "gpu.gpu0_cu0_to_noc", dst: "noc.router0.cpu_side"}]
  → 经 Fix D.4 递归 findInternalPath 解析路径
    → GpuCluster.findInternalPath("gpu0_cu0_to_noc")
      → 当前层未找到
      → 递归到子 SimModule:
        → GpcCluster0.findInternalPath("gpu0_cu0_to_noc")
          → 当前层未找到
          → 递归到 TpcCluster0... → ComputeCluster... → cu0
          → 找到 → 返回 "gpc0.tpc0.compute_grp.cu0.req_out"
      → 返回 "gpc0.tpc0.compute_grp.cu0.req_out"
  → 最终 parsePortSpec → MasterPort::bind(SlavePort) 建立 PortPair
```

---

## 6. 错误处理

### 6.1 配置错误

| 错误 | 检测点 | 处理 |
|------|--------|------|
| `cu_template` 文件不存在 | `JsonIncluder::loadAndInclude` | 抛 `std::runtime_error("ComputeCluster: cu_template not found: " + path)` |
| `cu_template` JSON 缺少 `modules` 字段 | `simulate_instantiate` 验证 | 抛 `std::runtime_error("cu_template must contain 'modules' array")` |
| `cu_count <= 0` | `set_config` 验证 | 静默设为 1 + WARN |
| `cu_count > 64` (硬限制) | `set_config` 验证 | 静默截断到 64 + WARN |
| 嵌套超过 MAX_DEPTH=8 | `simulate_instantiate` 入口 | 抛 `std::runtime_error` + RAII 恢复 depth_ |
| `cu_template` 引用自身 (循环) | 模板加载期检测 | 抛 `std::runtime_error("template circular reference")` |
| 暴露端口找不到 internal_path | `findInternalPath` 递归 | 返回 "" + WARN（仅开发期） |

### 6.2 运行时错误

| 错误 | 检测点 | 处理 |
|------|--------|------|
| 父端口已被占用 | `ConnectionResolver` 阶段 | 抛 `std::runtime_error("port already bound")` |
| `set_config` 字段类型错误 | `set_config` 严格 JSON 解析 | 抛 `nlohmann::json::type_error` |
| 子 SimModule 找不到暴露端口 | `ConnectionResolver` Step 5 | 抛 `std::runtime_error("internal_path not found: " + path)` |

### 6.3 测试期已知限制 (WARN 而非 REQUIRE)

- ChStream 端口在 PortManager 之外：`getInternalOutputPort` 对 ChStream 模块返回 nullptr（沿用现有 `test_simmodule_nested.cc:194` 模式，P3 修复前保持）

---

## 7. 测试策略

### 7.1 测试文件清单

| 文件 | Phase | 测试用例数 | 内容 |
|------|-------|-----------|------|
| `test/test_simmodule_recursive_bug.cc` | P1 | 3 | D.4 递归 + D.5 tick 递归 + 3 层 JSON E2E |
| `test/test_simmodule_gpu_hierarchy.cc` | P2 | 4 | ComputeCluster + TpcCluster + GpcCluster + GpuCluster |
| `test/test_chstream_helpers.cc` | P3 | 3 | connectCPU + connectBus + 82 测试回归 |
| `test/test_simmodule_infra_clusters.cc` | P4 | 3 | CacheCluster + MemoryCluster + GpuNoC |
| `test/test_apu_soc_top.cc` | P5 | 3 | ApuSoC 组合 + incorporate_parent + E2E 1000 cycles |
| `test/python/test_apu_soc_emitter.py` | P5 | 2 | Python 模板加载 + 仿真 100 cycles |

### 7.2 关键测试用例 (示例)

**P1 Case 1**: D.4 递归验证
```cpp
TEST_CASE("D.4 findInternalPath recurses nested SimModule") {
    // 构建 outer(CpuCluster) → mid(CpuCluster) → inner(CpuCluster) → cu0(CPUTLM)
    // inner.outputs: [{internal: "cu0.req_out", external: "inner_cpu0_to_bus"}]
    // mid.outputs: [{internal: "inner.inner_cpu0_to_bus", external: "mid_cpu0_to_bus"}]
    // outer.outputs: [{internal: "mid.mid_cpu0_to_bus", external: "outer_cpu0_to_bus"}]
    
    // 验证:
    REQUIRE(outer.findInternalPath("outer_cpu0_to_bus") == "mid.mid_cpu0_to_bus");
    REQUIRE(outer.findInternalPath("mid_cpu0_to_bus") == "mid.inner.inner_cpu0_to_bus");
    REQUIRE(outer.findInternalPath("inner_cpu0_to_bus") == "mid.inner.cu0.req_out");
}
```

**P2 Case 1**: ComputeCluster 模板加载
```cpp
TEST_CASE("ComputeCluster loads cu_template and instantiates N copies") {
    // configs/templates/compute_unit_v1.json 含 scalar_cache + vector_reg
    // cu_count=4 → 期望 4 个 cu0..cu3，每个含 2 个子模块 = 8 个
    ComputeCluster cluster("compute_grp", &eq);
    cluster.set_config({{"cu_template", "./configs/templates/compute_unit_v1.json"},
                        {"cu_count", 4}});
    cluster.simulate_instantiate({});
    REQUIRE(cluster.getInternalFactory().getAllInstances().size() == 4 * 2);
}
```

**P5 Case 1**: ApuSoC 端到端
```cpp
TEST_CASE("ApuSoC composes full APU and runs 1000 cycles") {
    auto config = JsonIncluder::loadAndInclude("configs/apu_soc_v1.json");
    ModuleFactory factory(&eq);
    factory.instantiateAll(config);
    factory.startAllTicks();
    for (int i = 0; i < 1000; ++i) eq.tick();
    // 验证: 至少 N 个事件被处理 (具体数字基于 metrics_reporter)
    REQUIRE(metrics_reporter.get_total_events() > 1000);
}
```

### 7.3 回归测试

每 Phase 完成后必须：
1. 跑完整 82 测试套件：`./build/bin/cpptlm_tests`
2. 跑 CMake 注册测试：`ctest --test-dir build --output-on-failure -j4`
3. 跑文档同步检查：`./scripts/test/docs_sync_check.sh --strict`

---

## 8. 文档同步要求

| 文档 | Phase | 同步内容 |
|------|-------|----------|
| `AGENTS.md`（根） | P1-P5 | STRUCTURE 节加 `include/tlm/cluster/` 8 个新文件 + `configs/templates/` |
| `include/AGENTS.md` | P2-P5 | 注册宏体系表加 8 个新 REGISTER_MODULE |
| `include/tlm/AGENTS.md` | P2-P5 | cluster/ 子目录表（现有仅 CpuCluster） |
| `configs/AGENTS.md` | P1-P5 | JSON Schema 段加 `cu_template` / `cu_count` 字段说明 + 模板目录约定 |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | P5 | 加 GPU 层次图（参考 gem5 4-level SubSystem） |
| `examples/CMakeLists.txt` | P2-P5 | 注册 2 个新 example target |
| `CHANGELOG.md` | P1-P5 | 每个 Phase 完成后追加版本号 + 简述 |
| `openspec/specs/` | P5 | 加 `simmodule-complex-hierarchies/spec.md` |

---

## 9. 文件清单 (新增 + 修改)

### 9.1 新增文件

| 文件 | Phase | 行数估算 |
|------|-------|----------|
| `include/tlm/cluster/compute_cluster.hh` | P2 | ~60 |
| `src/tlm/cluster/compute_cluster.cc` | P2 | ~50 |
| `include/tlm/cluster/tpc_cluster.hh` | P2 | ~60 |
| `src/tlm/cluster/tpc_cluster.cc` | P2 | ~50 |
| `include/tlm/cluster/gpc_cluster.hh` | P2 | ~60 |
| `src/tlm/cluster/gpc_cluster.cc` | P2 | ~50 |
| `include/tlm/cluster/gpu_cluster.hh` | P2 | ~80 |
| `src/tlm/cluster/gpu_cluster.cc` | P2 | ~70 |
| `include/tlm/cluster/cache_cluster.hh` | P4 | ~70 |
| `src/tlm/cluster/cache_cluster.cc` | P4 | ~60 |
| `include/tlm/cluster/memory_cluster.hh` | P4 | ~70 |
| `src/tlm/cluster/memory_cluster.cc` | P4 | ~60 |
| `include/tlm/cluster/gpu_noc_cluster.hh` | P4 | ~80 |
| `src/tlm/cluster/gpu_noc_cluster.cc` | P4 | ~80 |
| `include/tlm/cluster/apu_soc.hh` | P5 | ~60 |
| `src/tlm/cluster/apu_soc.cc` | P5 | ~50 |
| `include/modules_cluster.hh` | P2 | ~50 (8 个 REGISTER_MODULE 集中) |
| `test/test_simmodule_recursive_bug.cc` | P1 | ~150 |
| `test/test_simmodule_gpu_hierarchy.cc` | P2 | ~300 |
| `test/test_chstream_helpers.cc` | P3 | ~200 |
| `test/test_simmodule_infra_clusters.cc` | P4 | ~250 |
| `test/test_apu_soc_top.cc` | P5 | ~300 |
| `test/python/test_apu_soc_emitter.py` | P5 | ~150 |
| `examples/example_simmodule_2gpc_2tpc_2cu.cc` | P2 | ~100 |
| `examples/generate_apu_soc_2gpc.py` | P5 | ~150 |
| `configs/templates/compute_unit_v1.json` | P2 | ~20 |
| `configs/gpu_2gpc_2tpc_2cu.json` | P2 | ~50 |
| `configs/apu_soc_v1.json` | P5 | ~80 |
| `configs/templates/cpu_cluster_2level.json` | P4 | ~30 |
| `configs/templates/gpu_2gpc_2tpc_2cu.json` | P5 | ~50 |
| `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` | P0 | 本文件 |

### 9.2 修改文件

| 文件 | Phase | 改动 |
|------|-------|------|
| `include/core/sim_module.hh` | P1, P5 | D.4 递归 + D.5 tick 默认实现 + P5 `incorporate_parent` |
| `include/tlm/cache_tlm.hh` | P3 | `connectCPU/Bus` helper 方法 |
| `include/tlm/crossbar_tlm.hh` | P3 | `connectCPUSideBus/MemSideBus` helper 方法 |
| `include/modules.hh` | P2-P5 | 8 个新 REGISTER_MODULE |
| `examples/CMakeLists.txt` | P2, P5 | 注册新 example targets |
| `CMakeLists.txt` | P2-P5 | 添加新 .cc 源到 cpptlm_core |
| `AGENTS.md` (根) | P1-P5 | STRUCTURE 节 |
| `include/AGENTS.md` | P2-P5 | 注册宏体系表 |
| `include/tlm/AGENTS.md` | P2-P5 | cluster/ 子目录表 |
| `configs/AGENTS.md` | P1-P5 | JSON Schema 段 |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | P5 | GPU 层次图 |
| `CHANGELOG.md` | P1-P5 | 版本号追加 |

---

## 10. 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| D.1 修复复杂（ChStream 端口表全局重构） | P3 可能延期 | P3 helper 方法降级为可选，P5 之后再补 |
| `MAX_DEPTH=8` 不够（APU 顶层 + CpuCluster + CacheCluster + GpuCluster + GpcCluster + TpcCluster + ComputeCluster + 内部 CU = 7 层） | 7 层刚好够用，但边界紧 | 编译宏 `-DCPPTLM_SIMMODULE_MAX_DEPTH=10` 留余量 |
| 模板 JSON 跨平台路径 | Windows/Linux 路径分隔符 | 仅支持正斜杠（POSIX 标准） |
| 现有 82 测试套件在 P1 后回归 | 可能暴露 D.4/D.5 在其他场景的 bug | 立即跑 ctest 验证 |
| `tick` 默认递归在 5+ 层嵌套时开销 | 每 cycle O(总子模块数) | 复杂度可接受（MAX_DEPTH=10 × 64 子模块 = ~640 ops/cycle） |
| `findInternalPath` 递归性能 | O(N) 遍历 | 编译期可内联，runtime 影响可忽略 |
| P3 helper 方法与现有 ConnectionResolver 双重接线 | 端口被绑两次 | P3 实现时显式检查 + 抛异常 |

### 10.1 缓解措施详述

#### 10.1.1 `MAX_DEPTH` 余量

**当前**: `MAX_DEPTH = 8`（`sim_module.hh:38`）
**APU 完整深度**: ApuSoC(1) + GpuCluster(2) + GpcCluster(3) + TpcCluster(4) + ComputeCluster(5) + cu(6) = **6 层**
**风险**: 加上 CpuCluster + CacheCluster 分支 = 7 层，刚好 ≤ 8。
**缓解**: P5 阶段编译时 `-DCPPTLM_SIMMODULE_MAX_DEPTH=10` 留 3 层余量。

#### 10.1.2 P3 双重接线检查

```cpp
void CacheTLM::connectCPU(SimModule* cluster, int core_idx) {
    auto* cpu_side = getPortManager().getUpstreamPort("cpu_side");
    if (cpu_side->isBound()) {
        throw std::runtime_error("CacheTLM::connectCPU: cpu_side already bound");
    }
    // ... 正常连接
}
```

---

## 11. 开放问题 (待用户决策)

### 11.1 模板路径基准

**问题**：`cu_template: "./templates/compute_unit_v1.json"` 路径基准是什么？
- (a) 当前 JSON 文件目录（与 `$include` 一致）
- (b) 工作目录（cwd）
- (c) 固定 `configs/templates/` 目录

**推荐**: (a) - 与现有 `$include` 行为一致，跨项目复用友好。

### 11.2 模板内 `${var}` 占位符

**问题**：`compute_unit_v1.json` 内的 `params.size` 是否支持 `${l1_size}` 占位符？
- (a) 支持（VarResolver 替换）
- (b) 不支持（保持模板静态）

**推荐**: (a) - 与 gem5 SimpleProcessor 参数透传一致。

### 11.3 P3 优先级

**问题**：如果 P1 完成后 D.1 修复成本高，P3 helper 方法如何处理？
- (a) P3 整体延后至 D.1 修复后
- (b) P3 部分先行（仅实现不依赖 D.1 的 `connectBus/connectCPUSideBus` 等）
- (c) P3 跳过，P5 再补

**推荐**: (b) - 渐进交付，已知收益部分先实现。

---

## 12. 迁移计划

### 12.1 Phase P1 详细任务

1. 修改 `include/core/sim_module.hh:149-161` 实现 D.4 递归
2. 修改 `include/core/sim_module.hh` 新增默认 `tick()` 递归实现
3. 创建 `test/test_simmodule_recursive_bug.cc` 含 3 个测试用例
4. 跑 82 测试套件验证无回归
5. 同步 `AGENTS.md` + `configs/AGENTS.md` 文档

### 12.2 Phase P2 详细任务

1. 创建 `include/tlm/cluster/compute_cluster.hh` + `src/tlm/cluster/compute_cluster.cc`
2. 创建 tpc_cluster / gpc_cluster / gpu_cluster 类似文件
3. 创建 `include/modules_cluster.hh` 集中 4 个 REGISTER_MODULE
4. 修改 `include/modules.hh` 包含新宏文件
5. 创建 `configs/templates/compute_unit_v1.json`
6. 创建 `configs/gpu_2gpc_2tpc_2cu.json`
7. 创建 `test/test_simmodule_gpu_hierarchy.cc` 含 4 个测试
8. 创建 `examples/example_simmodule_2gpc_2tpc_2cu.cc`
9. 跑完整测试套件
10. 同步文档

### 12.3 Phase P3 详细任务

1. 修改 `include/tlm/cache_tlm.hh` 加 helper 方法
2. 修改 `include/tlm/crossbar_tlm.hh` 加 helper 方法
3. 实现 helper 方法逻辑（依赖 D.1 状态决策）
4. 创建 `test/test_chstream_helpers.cc`
5. 跑完整测试套件验证无回归
6. 同步文档

### 12.4 Phase P4 详细任务

类似 P2，创建 CacheCluster / MemoryCluster / GpuNoC。

### 12.5 Phase P5 详细任务

1. 修改 `include/core/sim_module.hh` 加 `incorporate_parent` 虚函数
2. 创建 ApuSoC 顶层类
3. 创建 `configs/apu_soc_v1.json` + 模板
4. 创建 `test/test_apu_soc_top.cc` + `test/python/test_apu_soc_emitter.py`
5. 创建 `examples/generate_apu_soc_2gpc.py`
6. 跑完整测试套件
7. 同步文档（`AGENTS.md`, `docs/architecture/01-hybrid-architecture-v2.1.md`）

---

## 13. 验收标准

每个 Phase 完成必须满足：

### 13.1 编译通过

```bash
cmake --build build -j$(nproc)  # exit 0
```

### 13.2 测试通过

```bash
./build/bin/cpptlm_tests                    # 82 + 新增全部 pass
./build/bin/cpptlm_tests "[simmodule]"      # 7 (现有) + 新增 simmodule 用例 pass
ctest --test-dir build --output-on-failure -j4
```

### 13.3 文档同步

```bash
./scripts/test/docs_sync_check.sh --strict  # 365+/365+ 路径有效
./scripts/build/format.sh --check           # clang-format 校验
```

### 13.4 零债务

- 无 TODO 残留
- 无 `.disabled` 测试新增
- 无 `// FIXME` 临时代码
- `git status` 干净

---

## 14. 参考资料

### 14.1 内部文档

- `docs/research/gem5-soc-survey.md` (59KB, gem5 调研)
- `docs/soc_arch/specs/apu-soc-design.md` §2.2 (APU 蓝图)
- `docs/soc_arch/modules/gpu.common.md` (GPU 5 层抽象)
- `docs/soc_arch/modules/gpu-compute-unit.md` (CU 设计)
- `docs/architecture/01-hybrid-architecture-v2.1.md` (主架构)
- `include/AGENTS.md` (注册宏体系 + 未来 Cluster 扩展承诺)
- `include/tlm/AGENTS.md` (cluster/ 子目录意图)
- `include/core/sim_module.hh` (SimModule 基类)

### 14.2 gem5 源码参考

- `src/python/gem5/components/cachehierarchies/classic/private_l1_private_l2_cache_hierarchy.py` (核心模式参考)
- `src/python/gem5/components/processors/simple_processor.py` (动态 N-core 生成)
- `src/python/gem5/components/boards/abstract_board.py:_connect_things` (late-binding 模式)
- `configs/learning_gem5/part1/caches.py` (helper 方法)
- `configs/learning_gem5/part1/two_level.py` (典型 2 级 cache)

### 14.3 现有 CppTLM 资源

- `include/core/sim_module.hh` (223 行, v2.2 重写)
- `include/tlm/cluster/cpu_cluster.hh` (61 行, 唯一活跃 SimModule 派生类)
- `configs/example_simmodule_nested_2level.json` (2 层示例)
- `configs/example_simmodule_nested_3level_static.json` (3 层示例, 已知 bug)
- `test/test_simmodule_nested.cc` (574 行, 7 用例)

---

## 15. 设计决策日志

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-06-19 | 选 C (增量分层) 而非 A (保守) 或 B (gem5-Faithful) | 风险最低，每 Phase 独立可发布 |
| 2026-06-19 | 修复 D.4 + D.5 在 P1 立即进行 | 解锁现有 3 层 JSON bug，立即可见价值 |
| 2026-06-19 | `cu_template` 路径相对当前 JSON 目录 | 与 `$include` 一致，跨项目复用友好 |
| 2026-06-19 | 命名 `ComputeCluster` / `TpcCluster` / `GpcCluster` / `GpuCluster` | 与现有 `CpuCluster` 命名一致；用户已确认 |
| 2026-06-19 | 复用机制：`cu_template` + `cu_count` | 用户确认：JSON 蓝图 + 多份例化 |
| 2026-06-19 | P3 ChStream helper 延后至 D.1 修复后 | 避免卡在 helper 依赖上 |
| 2026-06-19 | P5 `incorporate_parent` 钩子 | 渐进 gem5 late-binding 模式 |

---

**End of Design Document**
