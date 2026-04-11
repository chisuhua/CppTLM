# CppTLM v2.1 分层融合架构 — 实施计划

> **版本**: 1.0  
> **日期**: 2026-04-12  
> **前置文档**: `docs/architecture/01-hybrid-architecture-v2.1.md`  
> **目标**: 将 v2.1 分层融合架构从文档转化为可编译、可运行的代码

---

## 一、实施概述

**核心思路**: 模块内部 ch_stream 握手 ⇄ StreamAdapter 桥梁 ⇄ 外层 PortPair 通信

**实施策略**: 增量式推进，每 Phase 可独立编译验证，不破坏现有功能。

**总工作量估算**: ~40-50 人时，分 6 个 Phase 完成

**质量承诺**: 零债务原则 — 每个 Phase 完成即编译通过、测试覆盖。

---

## 二、Phase 分解

### Phase 0: 基础设施就绪 — Bundle + StreamAdapter + 基类

**目标**: 创建 ch_stream 通信所需的所有底层组件

**预估工时**: ~6-8 小时

**前置条件**: 无

**具体任务**:

#### 0.1 创建 Bundle 定义层 (include/bundles/)

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/bundles/cache_bundles.hh` | ~40 | CacheReqBundle, CacheRespBundle |
| `include/bundles/noc_bundles.hh` | ~40 | NoCReqBundle, NoCRespBundle |
| `include/bundles/fragment_bundles.hh` | ~30 | FragmentBundle (分片传输) |
| `include/bundles/bundle_serialization.hh` | ~80 | Bundle ↔ Packet payload 序列化/反序列化 |

**关键设计决策**:
- Bundle 集成 CppHDL 的 `ch::core::bundle_base<T>` 和 `CH_BUNDLE_FIELDS` 宏
- 添加 `bundle_serialization.hh`，提供 `bundle_to_packet()` 和 `packet_to_bundle()` 模板函数
- 通过 `tlm_generic_payload->get_data_ptr()` 存储序列化后的 Bundle 数据
- **不**直接使用 CppHDL 的仿真运行时，仅借用其 Bundle 类型系统

**CMake 变更**:
```cmake
# CMakeLists.txt — 添加 CppHDL 头路径
target_include_directories(cpptlm_core PUBLIC ${CMAKE_SOURCE_DIR}/external/CppHDL/include)
# 注意：不链接 CppHDL 库，仅使用头文件中的类型定义
```

#### 0.2 创建 StreamAdapter 桥梁 (include/framework/)

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/framework/stream_adapter.hh` | ~200 | StreamAdapter 基类 + 模板实现 |
| `include/framework/stream_adapter_registry.hh` | ~80 | StreamAdapter 注册表和管理 |

**关键接口**:

```cpp
class StreamAdapterBase {
    virtual ~StreamAdapterBase() = default;
    virtual void tick() = 0;
    virtual void bind_ports(MasterPort* out_port, SlavePort* in_port) = 0;
    virtual std::size_t get_input_count() const = 0;
    virtual std::size_t get_output_count() const = 0;
};

template<typename ReqBundle, typename RespBundle>
class StreamAdapter : public StreamAdapterBase {
    // 双向转换: Packet ↔ Bundle
    // 握手同步: ch_stream ready ←→ Port send/recv
};
```

**StreamAdapter 工作流程** (tick 周期):
1. **输入方向**: SlavePort 收到 Packet → 反序列化 Bundle → 设置 `ch_stream.valid = true`, `payload = bundle`
2. **输出方向**: 检查 `ch_stream.valid && ready` → 序列化 Bundle → MasterPort.send(Packet)
3. **反压传递**: `ch_stream.ready` 为 false 时，通知上游 Port 暂停发送

#### 0.3 创建 ChStreamModule 基类 (include/core/)

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/core/chstream_module.hh` | ~50 | ChStreamModuleBase 纯虚基类 |
| `include/modules.hh` | 修改 | 注册宏扩展 |

**关键接口**:
```cpp
class ChStreamModuleBase : public SimObject {
    virtual void set_adapter(StreamAdapterBase* adapter) = 0;
    virtual std::size_t get_input_port_count() const = 0;
    virtual std::size_t get_output_port_count() const = 0;
};

#define REGISTER_CHSTREAM_MODULE(T, Name) \
    ModuleFactory::registerObject<T>(Name);
```

#### 0.4 更新 CMakeLists.txt

| 变更 | 说明 |
|------|------|
| 添加 CppHDL 头路径 (`external/CppHDL/include`) | 供 Bundle 类型使用 |
| 确认 nlohmann/json 路径 | 现有已配置 |
| 新增 `bundled/` 编译目标（可选） | 如果有独立测试 |

**验收标准**:

```
□ include/bundles/ 下 4 个文件编译通过
□ include/framework/stream_adapter.hh 编译通过
□ include/core/chstream_module.hh 编译通过
□ CMakeLists.txt 更新后 cmake --build 无错误
□ 不改变现有模块的编译行为
```

---

### Phase 1: CacheTLM 单模块试点

**目标**: 创建第一个 ch_stream 模块，验证 StreamAdapter 桥梁

**预估工时**: ~6-8 小时

**前置条件**: Phase 0 完成

**具体任务**:

#### 1.1 创建 CacheTLM 模块

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/tlm/cache_tlm.hh` | ~150 | CacheTLM 新模块（纯 ch_stream 业务逻辑） |
| `include/tlm/memory_tlm.hh` | ~100 | MemoryTLM 下游模块（简化端） |

**CacheTLM 设计**:
```cpp
class CacheTLM : public ChStreamModuleBase {
private:
    ch_stream<CacheReqBundle>  req_in;    // ← StreamAdapter 注入数据
    ch_stream<CacheRespBundle> resp_out;  // → StreamAdapter 消费数据
    // 业务状态
    std::map<uint64_t, uint64_t> cache_lines_;
    StreamAdapterBase* adapter_ = nullptr;
public:
    void tick() override;  // ✓ valid/ready 握手业务逻辑
    // ...
};
```

#### 1.2 创建 JSON 配置示例

| 文件 | 说明 |
|------|------|
| `configs/cache_chstream_test.json` | CacheTLM + MemoryTLM 连接配置 |

```json
{
  "modules": [
    { "name": "cache", "type": "CacheTLM", "mode": "chstream",
      "req_bundle": "CacheReqBundle", "resp_bundle": "CacheRespBundle" }
  ],
  "connections": []
}
```

#### 1.3 创建单元测试

| 文件 | 行数 | 说明 |
|------|------|------|
| `test/test_chstream_basic.cc` | ~150 | StreamAdapter + ch_stream 握手测试 |
| `test/test_cache_tlm.cc` | ~200 | CacheTLM 模块逻辑测试 |

**验收标准**:

```
□ CacheTLM 模块编译通过
□ CacheTLM JSON 配置能被 ModuleFactory 解析
□ StreamAdapter tick 逻辑正确（手动验证）
□ 单元测试 test_chstream_basic 通过
□ 单元测试 test_cache_tlm 通过
□ LSP diagnostics 无 ERROR
```

---

### Phase 2: ModuleFactory 扩展 — JSON 驱动 ch_stream 模块加载

**目标**: ModuleFactory 支持 `mode: "chstream"` 配置，自动创建 StreamAdapter

**预估工时**: ~8-10 小时

**前置条件**: Phase 0, Phase 1 完成

**具体任务**:

#### 2.1 修改 ModuleFactory::instantiateAll

**修改文件**: `src/core/module_factory.cc`, `include/core/module_factory.hh`

**变更逻辑**:

```cpp
void ModuleFactory::instantiateAll(const json& config) {
    // ... [现有逻辑: 1-5 步不变] ...
    
    // [新增] 6. 为 chstream 模块创建 StreamAdapter 和虚拟 Port
    for (auto& mod : final_config["modules"]) {
        std::string mode = mod.value("mode", "legacy");
        if (mode != "chstream") continue;
        
        std::string name = mod["name"];
        SimObject* obj = object_instances[name];
        auto* chstream_mod = dynamic_cast<ChStreamModuleBase*>(obj);
        if (!chstream_mod) continue;
        
        // 6a. 创建 StreamAdapter（根据 bundle 类型）
        auto* adapter = create_stream_adapter_for_module(chstream_mod, mod);
        chstream_mod->set_adapter(adapter);
        stream_adapters_.push_back( ... );
        
        // 6b. 为 StreamAdapter 创建虚拟 MasterPort/SlavePort
        // 这些 Port 会被 ConnectionResolver 当作普通 Port 处理
        auto* req_port = adapter->create_req_port(name);
        auto* resp_port = adapter->create_resp_port(name);
        port_map_[name + "_req"] = req_port;
        port_map_[name + "_resp"] = resp_port;
    }
}
```

#### 2.2 扩展现有注册机制

**修改文件**: `include/modules.hh`

```cpp
#define REGISTER_MODULES_FOR_V2_1 \
    REGISTER_CHSTREAM_MODULE(CacheTLM, "CacheTLM"); \
    REGISTER_CHSTREAM_MODULE(CrossbarTLM, "CrossbarTLM"); \
    REGISTER_CHSTREAM_MODULE(MemoryTLM, "MemoryTLM"); \
    REGISTER_CHSTREAM_MODULE(NICTLM, "NICTLM");
```

#### 2.3 更新 ConnectionResolver 支持 ch_stream 端口

**修改文件**: `include/core/connection_resolver.hh`, `src/core/connection_resolver.cc`

**变更**: 端口类型标记扩展，识别由 StreamAdapter 创建的虚拟 Port

#### 2.4 主程序入口更新

**修改文件**: `src/main.cpp`

```cpp
int main(int argc, char* argv[]) {
    EventQueue eq;

    // 注册所有模块
    REGISTER_OBJECT              // Legacy 模块
    REGISTER_MODULE              // Legacy SimModule
    REGISTER_MODULES_FOR_V2_1;   // [新增] ChStream 模块

    // 加载配置 + 运行
    json config = JsonIncluder::loadAndInclude(argv[1]);
    ModuleFactory factory(&eq);
    factory.instantiateAll(config);
    factory.startAllTicks();

    eq.run(10000);
    std::cout << "\n[INFO] Simulation finished.\n";
    return 0;
}
```

#### 2.5 创建集成测试配置

| 文件 | 说明 |
|------|------|
| `configs/cache_system_v21.json` | CPUSim(Legacy) → CacheTLM → MemoryTLM |

```json
{
  "modules": [
    { "name": "cpu0", "type": "CPUSim", "mode": "legacy" },
    { "name": "l1", "type": "CacheTLM", "mode": "chstream",
      "req_bundle": "CacheReqBundle", "resp_bundle": "CacheRespBundle" },
    { "name": "mem", "type": "MemoryTLM", "mode": "chstream",
      "req_bundle": "CacheReqBundle", "resp_bundle": "CacheRespBundle" }
  ],
  "connections": [
    { "src": "cpu0", "dst": "l1", "latency": 2 },
    { "src": "l1", "dst": "mem", "latency": 10 }
  ]
}
```

**验收标准**:

```
□ ModuleFactory 能加载 legacy + chstream 混合配置
□ StreamAdapter 自动创建（无需代码手动创建）
□ CPUSim(Legacy) → CacheTLM(chstream) 连接成功
□ cache_system_v21.json 端到端运行
□ 现有 legacy 配置（cpu_simple.json 等）不受影响
```

---

### Phase 3: Legacy 模块迁移 — 移入 legacy/ 子目录

**目标**: 将旧 Port 回调模块迁入 `modules/legacy/`，保持向后兼容

**预估工时**: ~4-6 小时

**前置条件**: Phase 2 完成

**具体任务**:

#### 3.1 目录结构重组

```
# 迁移前（现状）
include/modules/
├── cache_sim.hh
├── cpu_sim.hh
├── memory_sim.hh
├── crossbar.hh
├── router.hh
├── arbiter.hh
├── traffic_gen.hh
├── cpu_cluster.hh
├── crossbar_rr.hh
├── stream_consumer.hh
└── stream_producer.hh

# 迁移后
include/modules/
├── modules_v2.hh           # [新增] 统一注册头（同时包含 legacy + v2 模块）
├── example_tlm_module.hh   # [保留] TLM 示例（非 legacy）
└── legacy/                 # [新增] Legacy 子目录
    ├── cache_sim.hh
    ├── cpu_sim.hh
    ├── memory_sim.hh
    ├── crossbar.hh
    ├── router.hh
    ├── arbiter.hh
    ├── crossbar_rr.hh
    ├── traffic_gen.hh
    ├── cpu_cluster.hh
    ├── stream_consumer.hh
    └── stream_producer.hh
```

#### 3.2 更新所有 include 路径

| 原路径 | 新路径 |
|--------|--------|
| `#include "modules/cache_sim.hh"` | `#include "modules/legacy/cache_sim.hh"` |
| `#include "modules/crossbar.hh"` | `#include "modules/legacy/crossbar.hh"` |

**需要更新的文件**:
- `include/modules.hh` — 修改所有 #include 路径
- `src/core/module_factory.cc` — 检查间接依赖
- `test/test_*.cc` — 更新 include 路径
- `src/main.cpp`, `src/main_hierarchy.cpp` — 更新 include 路径

#### 3.3 更新 modules.hh（统一入口）

```cpp
// include/modules.hh
#include "modules/legacy/cache_sim.hh"
#include "modules/legacy/cpu_sim.hh"
#include "modules/legacy/memory_sim.hh"
#include "modules/legacy/crossbar.hh"
#include "modules/legacy/router.hh"
#include "modules/legacy/arbiter.hh"
#include "modules/legacy/traffic_gen.hh"
#include "modules/legacy/cpu_cluster.hh"

#define REGISTER_OBJECT \
    ModuleFactory::registerObject<CPUSim>("CPUSim"); \
    ModuleFactory::registerObject<CacheSim>("CacheSim"); \
    ModuleFactory::registerObject<MemorySim>("MemorySim"); \
    ModuleFactory::registerObject<MemorySim>("Crossbar"); \
    ModuleFactory::registerObject<MemorySim>("Router"); \
    ModuleFactory::registerObject<MemorySim>("Arbiter"); \
    ModuleFactory::registerObject<TrafficGenerator>("TrafficGenerator");

#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");
```

#### 3.4 验证向后兼容

| 配置 | 验证方法 | 预期结果 |
|------|---------|---------|
| `configs/cpu_simple.json` | `./cpptlm configs/cpu_simple.json` | 原有行为不变 |
| `configs/ring_bus.json` | `./cpptlm configs/ring_bus.json` | 原有行为不变 |
| `configs/noc_mesh.json` | `./cpptlm configs/noc_mesh.json` | 原有行为不变 |

**验收标准**:

```
□ Legacy 模块全部移入 modules/legacy/ 子目录
□ 所有 include 路径更新完毕
□ 所有现有 JSON 配置运行结果与迁移前一致
□ 编译无警告（至少无新增警告）
□ 现有测试全部通过
```

---

### Phase 4: 多端口模块 — CrossbarTLM + 完整系统

**目标**: 实现多端口 ch_stream 模块，搭建完整仿真系统

**预估工时**: ~8-10 小时

**前置条件**: Phase 0, Phase 1, Phase 2 完成

**具体任务**:

#### 4.1 创建 CrossbarTLM（多端口）

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/tlm/crossbar_tlm.hh` | ~200 | 多端口路由器，地址路由 |

```cpp
class CrossbarTLM : public ChStreamModuleBase {
private:
    static constexpr int NUM_PORTS = 4;
    std::array<ch_stream<NoCReqBundle>, NUM_PORTS> req_in;
    std::array<ch_stream<NoCRespBundle>, NUM_PORTS> resp_out;
    // 轮询路由
public:
    void tick() override {
        for (int i = 0; i < NUM_PORTS; i++) {
            if (req_in[i].valid() && req_in[i].ready()) {
                int dst = route_by_address(req_in[i].payload.address.read());
                forward(i, dst);
            }
        }
    }
};
```

#### 4.2 多端口 StreamAdapter

StreamAdapter 需支持多端口映射：
- 每个 ch_stream 输入端口 → 一个 SlavePort
- 每个 ch_stream 输出端口 → 一个 MasterPort
```cpp
template<typename ReqBundle, typename RespBundle, int N>
class MultiPortStreamAdapter : public StreamAdapterBase {
    std::array<ch_stream<ReqBundle>*, N> req_streams_;
    std::array<ch_stream<RespBundle>*, N> resp_streams_;
    std::array<MasterPort*, N> req_ports_;
    std::array<SlavePort*, N> resp_ports_;
};
```

#### 4.3 创建 MemoryTLM（简化端）

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/tlm/memory_tlm.hh` | ~80 | MemorySim 的 ch_stream 版本 |

#### 4.4 完整系统配置

| 文件 | 说明 |
|------|------|
| `configs/full_system_v21.json` | CPU → CacheTLM → CrossbarTLM → MemoryTLM |
| `configs/cpu_cluster_v21.json` | CpuCluster(Legacy) → 跨范式连接 |

**验收标准**:

```
□ CrossbarTLM 编译通过
□ 多端口 StreamAdapter 正确工作
□ full_system_v21.json 端到端运行
□ 交易追踪 (TransactionTracker) 在 ch_stream 路径上工作
□ 性能数据：ch_stream 路径 vs legacy 路径延迟对比
```

---

### Phase 5: NICTLM (Fragment 分片) + Mapper 层

**目标**: 实现分片传输和协议映射

**预估工时**: ~6-8 小时

**前置条件**: Phase 0-4 完成

**具体任务**:

#### 5.1 创建 NICTLM（分片处理）

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/tlm/nic_tlm.hh` | ~150 | Fragment 分片/重组 |

#### 5.2 FragmentMapper

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/mapper/fragment_mapper.hh` | ~120 | 分片/重组逻辑 |

#### 5.3 创建 Bundle 示例配置文件

| 文件 | 说明 |
|------|------|
| `include/bundles/axi4_bundles.hh` | AXI4 协议 Bundle 定义 |
| `include/bundles/chi_bundles.hh` | CHI 协议 Bundle 定义 |

**验收标准**:

```
□ NICTLM 编译通过
□ FragmentMapper 分片/重组正确
□ FragmentBundle 定义完整
□ 单元测试覆盖分片边界情况
```

---

### Phase 6: 测试、文档、RTL 集成准备

**目标**: 全面测试、文档更新、为 RTL 集成预留接口

**预估工时**: ~4-6 小时

**前置条件**: Phase 0-5 完成

**具体任务**:

#### 6.1 全面的测试覆盖

| 文件 | 说明 |
|------|------|
| 新增 `test/test_stream_adapter.cc` | StreamAdapter 序列化/反序列化 |
| 新增 `test/test_crossbar_tlm.cc` | CrossbarTLM 路由逻辑 |
| 新增 `test/test_fragment_mapper.cc` | FragmentMapper 分片/重组 |
| 更新 `test/Makefile.in` | 确保新测试被编译 |

#### 6.2 RTL 模块集成准备

| 文件 | 说明 |
|------|------|
| `include/rtl/cache_component.hh` | CppHDL 的 RTL Component 框架 |
| `include/rtl/wrapper/rtl_module_wrapper.hh` | RTL 模块包装器 |
| `docs/architecture/05-rtl-integration.md` | RTL 集成设计文档 |

#### 6.3 文档更新

| 文档 | 变更 |
|------|------|
| `docs/README.md` | 更新文档导航，添加 v2.1 |
| `docs/guide/DEVELOPER_GUIDE.md` | 添加 ch_stream 模块开发指南 |
| `plans/implementation_plan_v2.md` | 更新 Phase 状态 |

#### 6.4 架构文档归档

| 文档 | 说明 |
|------|------|
| `docs/architecture/01-hybrid-architecture-v2.md` | **保留**，作为历史版本 |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | **新增**，当前活跃版本 |

**验收标准**:

```
□ 所有新测试通过
□ 代码覆盖率 >80% (ch_stream 相关代码)
□ 文档更新完毕
□ 架构决策记录 (ADR) 更新
□ 遗留 TODO = 0
□ 最终演示: legacy + ch_stream 模块共存运行
```

---

## 三、Phase 实施顺序与依赖

```
Phase 0: 基础设施 (Bundle + Adapter + ChStreamModule)
  │
  ├─→ Phase 1: CacheTLM 单模块试点
  │      │
  │      └─→ Phase 2: ModuleFactory 扩展 (JSON 驱动) ──→ Phase 3: Legacy 迁移
  │                                                 │
  │                                                 └─→ Phase 4: CrossbarTLM + 完整系统
  │                                                      │
  │                                                      └─→ Phase 5: NICTLM + Mapper
  │                                                           │
  │                                                           └─→ Phase 6: 测试 + 文档 + RTL 准备
  │
  └─→ (Phase 3 可独立于 1/2 进行，但建议在 2 完成后执行)
```

**独立并行机会**:

| 并行组 | 任务 | 依赖 |
|--------|------|------|
| **A** | Phase 0.1 (Bundle) + Phase 0.3 (ChStreamModule) | 无 |
| | Phase 0.2 (StreamAdapter) | 0.1 完成后 |
| | Phase 3 目录重组（仅移动文件+更新路径） | 无 |
| **B** | Phase 4.2 (多端口 StreamAdapter) | Phase 0 + 0.2 |
| | Phase 5.2 (FragmentMapper) | Phase 0.1 |
| **C** | Phase 6.3 (文档更新) | 所有 Phase 完成后 |

---

## 四、风险与缓解措施

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| CppHDL Bundle 宏与现有代码不兼容 | 中 | 高 | Phase 0.1 单独验证，不成功则用简化 Bundle 定义 |
| StreamAdapter 序列化开销过大 | 中 | 中 | 预留 `serialize` 接口，允许零拷贝优化 |
| ModuleFactory 扩展破坏向后兼容 | 低 | 高 | Phase 2 完成后先跑所有 legacy 配置验证 |
| Legacy 迁移导致 include 路径混乱 | 中 | 低 | Phase 3 先跑完整测试套件再提交 |

---

## 五、验收标准汇总

### 编译验证
```
Phase 0-6 每个 Phase 完成时: cmake --build 无错误, linter 无新增 ERROR
```

### 测试验证
```
Phase 1: test/test_chstream_basic 通过
Phase 2: cache_system_v21.json 端到端运行
Phase 3: 所有 legacy 配置运行正常
Phase 4: full_system_v21.json 端到端运行
Phase 5: FragmentMapper 单元测试通过
Phase 6: 所有测试通过, 覆盖率 >80%
```

### 功能验证
```
v Legacy 模块 + ChStream 模块共存于同一仿真
v JSON 配置驱动加载 chstream 模块
v StreamAdapter 自动创建 Port ↔ ch_stream 绑定
v TransactionTracker 在 ch_stream 路径上工作
v tlm_generic_payload 内嵌 Bundle 序列化
```

---

## 六、技术细节附录

### A. Bundle ↔ Packet 序列化策略

```cpp
// include/bundles/bundle_serialization.hh
template<typename BundleT>
size_t serialize_bundle(const BundleT& bundle, void* data, size_t max_len) {
    // CppHDL Bundle 字段序列化
    // CacheReqBundle: 64+64+8+1+64 = 201 bits → 26 bytes
    // 简单 memcpy 所有 POD 字段
    size_t offset = 0;
    // ...
    return offset;
}

template<typename BundleT>
BundleT deserialize_bundle(const void* data, size_t len) {
    BundleT bundle;
    // ...
    return bundle;
}
```

### B. ModuleFactory 类型注册表扩展

**现有双注册表** (SimObjects + SimModules):
```cpp
static std::unordered_map<std::string, CreateSimObjectFunc>& getObjectRegistry();
static std::unordered_map<std::string, CreateSimModuleFunc>& getModuleRegistry();
```

**无需修改** — ChStream 模块注册为 SimObject，在 instantiateAll 中通过 `dynamic_cast<ChStreamModuleBase*>` 识别。

### C. tick 调度优化

**现有**: EventQueue 调度每个 SimObject::tick()
**新增**: StreamAdapter::tick() 需要在模块 tick **之前**执行（准备输入），在模块 tick **之后**执行（消费输出）

```cpp
// EventQueue 调度顺序
void tick_all() {
    // 阶段1: StreamAdapter 输入方向 (Packet → ch_stream)
    for (auto& adapter : stream_adapters_) adapter->tick_input();
    
    // 阶段2: 所有 SimObject::tick()
    for (auto* obj : scheduled_objects_) obj->tick();
    
    // 阶段3: StreamAdapter 输出方向 (ch_stream → Packet)
    for (auto& adapter : stream_adapters_) adapter->tick_output();
}
```

---

**文档状态**: ✅ 完整  
**可执行状态**: 待用户确认  

---

**维护**: CppTLM 开发团队  
**版本**: 1.0  
**最后更新**: 2026-04-12

