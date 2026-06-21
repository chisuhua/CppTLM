# ApuSoC::incorporate_parent 真实 Late-Binding 设计

**Status**: Draft v0.1 · **Date**: 2026-06-20 · **Branch**: main · **Author**: Sisyphus (brainstorming)
**Scope**: 实现 `ApuSoC::incorporate_parent` 真实 late-binding wiring，解锁完整 APU SoC 拓扑（CPU 集群 + GPU 集群 + xbar 跨域 snoop 广播）
**Baseline**: P0 全套完成 + 推 origin/main (commits fb56cc3 / 5abba12 / 4964619 / 5a964c5)
**前置**: `docs/superpowers/specs/2026-06-19-p0-fixes-design.md` (D.1 + CoherentXBarTLM), `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` (§4.5.2 P5 设计)

---

## 0. 概述

### 0.1 目标

实现 `ApuSoC::incorporate_parent` 真实 late-binding 语义，让 ApuSoC 在 JSON 实例化完成后，**递归遍历整棵子树** 收集所有 `CacheTLM` 的 `req_out` 端口，注册到内部 `CoherentXBarTLM` 作为 snoop broadcast peer。

### 0.2 范围

**In scope** (本 spec):
- `ApuSoC::incorporate_parent` 重写（不再 passthrough）
- `ApuSoC::collectAndRegisterPeerCaches` 私有递归 helper
- `CoherentXBarTLM::registerPeerCache` 加按名去重（幂等性）
- `ModuleFactory::instantiateAll` 新增 Step 9（自动调用根 SimModule 的 `incorporate_parent`）
- `ApuSoC::set_config` 支持 `coherent_xbar_name` 参数（默认 `"xbar"`）
- 测试：5 个新 TEST_CASE（4 单元 + 1 E2E snoop）

**Out of scope (YAGNI / 后续阶段)**:
- `GpuCluster::incorporate_parent` override（spec §4.5.2 提议，本 spec 不实现 — 父端全树遍历已覆盖）
- `ApuSoC::connectGpuToCoherentBus` 等 GPU 专用 API（不引入）
- `WiringEvent` 总线模式（过度抽象）
- 多 xbar 实例（Phase 7.C+ 考虑）
- 6×6 coherence state table（Phase 7.C，本 spec 只解锁 wiring path）

### 0.3 非目标

- 不改变 `SimModule::incorporate_parent` 基类默认实现（其他 8 个 SimModule 派生类保持基类递归）
- 不改变 `ModuleFactory` 现有 8 步流程（仅在末尾追加 Step 9）
- 不引入新 JSON 字段（除 `coherent_xbar_name` 软配置）

### 0.4 核心决策（Oracle 综合推荐）

| 决策 | 选择 | 理由 |
|------|------|------|
| 调用模型 | **1A: ModuleFactory Step 9 自动调用** | grep 证实当前是死代码（0 个调用点），不自动 = 功能永不触发 |
| Wiring 算法 | **2A: 父端全树递归推送** | spec §3.3 伪代码只扫一层会漏掉 `cpu.cache` / `gpu.gpc0.tpc0.cu0.cache` 等深层 cache |
| GpuCluster 范围 | **3A: 不重写** | 父端全树递归已覆盖 GPU 深层 cache，无需 GPU 专用 API；spec §4.5.2 标为 Phase 7.C+ 可选 |
| 幂等性 | **`peer_caches_wired_` 早退 + `registerPeerCache` 按名去重** | 防止 ModuleFactory 未来重构二次调用导致重复注册 |
| 错误处理 | **软失败（DPRINTF WARN + skip）** | xbar 缺失 / cache 无 req_out / dynamic_cast 失败均不抛；只有 `registerPeerCache(nullptr)` 抛（已有） |
| 命名 | **`coherent_xbar_name_` 可配置（默认 `"xbar"`）** | 避免 spec 假设 `"top_xbar"` vs JSON 实际 `"xbar"` 命名漂移 |
| 实施顺序 | 5 步渐进（dedupe → helper → ModuleFactory → 测试 → 文档） | TDD 模式：先单测 helper，再接 ModuleFactory |

---

## 1. 背景与动机

### 1.1 现状（P0 完成后的状态）

**P0 已落地**（origin/main 6 commits ahead）:
- **D.1 PortManager Mirror** — `cache.getPortManager().getDownstreamPort("req_out")` 返回非空 `MasterPort*`
- **CoherentXBarTLM Skeleton** — `registerPeerCache(name, MasterPort*)` + `snoop_broadcast(Packet*)` + `peer_count()` API 可用
- **死代码清理** — helper 抛清晰错误而非静默 fallback

**P0 留下的技术债**（明确记录在 `2026-06-19-p0-fixes.md` Task 2.7）:
> "P0 阶段 CoherentXBarTLM 类骨架可用，但 ApuSoC::incorporate_parent **尚未**注册 peer cache（依赖 P5 incorporate_parent 真实 late-binding 语义，留 P1 实施）"

**P1 完整 APU SoC 拓扑目标**（用户明确）：
> "Oracle 给出全面的建议，最终要完整实现 apu 复杂的 soc 拓扑"

### 1.2 痛点

#### 痛点 1：incorporate_parent 是死代码

`grep -n incorporate_parent src/core/module_factory.cc` → 0 匹配。
- `ModuleFactory::instantiateAll` Step 1-7b 完成后，**从不**触发 `incorporate_parent`
- `SimModule::incorporate_parent` 基类只递归到子 SimModule，不消费 `parent` 参数
- `ApuSoC::incorporate_parent` (L78-80) 是 passthrough，无任何 wiring 逻辑
- 结果：spec §4.5.2 提议的 GpuCluster override 示例**永远不会跑**

#### 痛点 2：spec §3.3 伪代码不完整

`p0-fixes-design.md:335-342` 的 ApuSoC::incorporate_parent 伪代码：
```cpp
for (const auto& [name, mod] : internal_factory->getAllInstances()) {
    auto* cache = dynamic_cast<CacheTLM*>(mod);
    if (cache) { ... xbar->registerPeerCache(name, req_out); }
}
```

**问题**：`internal_factory->getAllInstances()` 只返回**直接子项**。但 `apu_soc_v1.json` 的 ApuSoC 子模块是 `CpuCluster` / `GpuCluster`（非 `CacheTLM`）。`dynamic_cast<CacheTLM*>` 对它们返回 nullptr。

真实 peer cache 路径：
- `apu_soc.cpu.cache` （CpuCluster 内部的 CacheTLM）
- `apu_soc.gpu.gpc0.tpc0.compute_grp.cu0.cache` （GpuCluster 4 级嵌套内部的 CacheTLM）
- `apu_soc.gpu.gpc0.tpc0.compute_grp.cu1.cache` （每 cu 2 个 CacheTLM）

**必须递归下钻整棵子树**，否则 `peer_count()` 永远是 0。

#### 痛点 3：命名漂移

- spec §3.3 用 `getInternalInstance("top_xbar")`
- `configs/apu_soc_v1.json:12` 实际是 `"xbar"`
- 硬编码任一都会在另一处断裂

### 1.3 Oracle 综合分析要点

| 维度 | Oracle 判断 |
|------|------------|
| 调用模型 | 1A 唯一可行（2/3 都让功能永不触发） |
| Wiring 算法 | 2A 必须下钻（2B/C 过度抽象） |
| GpuCluster 范围 | 3A 简洁（3B 需新 API，3C 过度） |
| 幂等性 | 必须去重（防止未来重构二次调用导致 snoop 重复） |
| 软失败 | 合理（xbar 缺失是合法拓扑，单元测试场景） |
| 向后兼容 | 安全（`test_simmodule_nested.cc` 只调 `simulate_instantiate`，绕过 `instantiateAll`，不触发新 Step 9） |
| 工期 | 1-2 天（含测试 + 文档） |

---

## 2. 设计方案

### 2.1 调用模型（Step 9 in ModuleFactory）

**位置**: `module_factory.cc` 末尾（Step 7b PortPairs 之后，Step 8 StatsManager 之前或之后均可）

**触发对象**: 顶层 `SimModule` 实例（即 `getModuleRegistry()` 创建的、非嵌套于其他 SimModule 的实例）

**伪代码**:
```cpp
// ========================
// 9. P1: 触发 SimModule::incorporate_parent late-binding
// 对每个顶层 SimModule 调用一次，parent 传 nullptr
// ========================
for (auto& [name, mod] : module_instances) {  // 顶层 SimModule
    if (!mod) continue;
    if (auto* sm = dynamic_cast<SimModule*>(mod)) {
        sm->incorporate_parent(nullptr);
    }
}
```

**为什么放 ModuleFactory 而不是 ApuSoC::simulate_instantiate**:
- 通用性：未来其他顶层 SimModule 也可享受自动 late-binding
- 与 gem5 `board._connect_things()` 阶段对齐
- 不依赖 ApuSoC 主动调用，子模块无需知道父对象

**为什么是 Step 7b 之后**:
- Step 7 注入 StreamAdapter + D.1 mirror 注册到 PortManager（peer cache `req_out` 此时已可查）
- Step 7b 创建 PortPairs（用户显式 connections 已在）
- Step 8 注册 StatGroup（独立）
- Step 9 触发 late-binding（此时所有 ChStream 端口已可见，peer cache 可发现）

**为什么 parent 传 nullptr**:
- 根 SimModule 没有"父"对象
- `SimModule::incorporate_parent(parent)` 的 `parent` 参数在基类默认实现中**未使用**（仅做递归）
- ApuSoC 的 wiring 不需要 parent（向下递归收集 peer cache）

### 2.2 Wiring 算法（父端全树递归）

**`ApuSoC::incorporate_parent` 重写**:
```cpp
void ApuSoC::incorporate_parent(SimModule* /*parent*/) {
    if (peer_caches_wired_) return;                  // 幂等早退
    peer_caches_wired_ = true;

    // 1. 找 xbar（命名可配置，默认 "xbar"）
    auto* xbar_obj = getInternalInstance(coherent_xbar_name_);
    auto* xbar = dynamic_cast<CoherentXBarTLM*>(xbar_obj);
    if (!xbar) {
        DPRINTF(MODULE, "[ApuSoC] no CoherentXBarTLM '%s' found, skip peer wiring\n",
                coherent_xbar_name_.c_str());
        return;  // 软失败：无 xbar 是合法拓扑（单元测试场景）
    }

    // 2. 递归遍历整棵子树，注册所有 CacheTLM
    collectAndRegisterPeerCaches(xbar, this, /*path_prefix=*/"");

    // 3. 递归通知子 SimModule（保留 hook 语义，供未来 GPU memory bridge 等扩展）
    SimModule::incorporate_parent(this);
}
```

**`ApuSoC::collectAndRegisterPeerCaches` 私有 helper**:
```cpp
void ApuSoC::collectAndRegisterPeerCaches(CoherentXBarTLM* xbar,
                                          SimModule* subtree_root,
                                          const std::string& path_prefix) {
    for (const auto& [name, obj] : subtree_root->getInternalFactory().getAllInstances()) {
        if (!obj) continue;
        std::string full_name = path_prefix.empty() ? name : path_prefix + "." + name;

        // 命中 CacheTLM：取 req_out 并注册
        if (auto* cache = dynamic_cast<CacheTLM*>(obj)) {
            if (!cache->hasPortManager()) continue;
            auto* req_out = dynamic_cast<MasterPort*>(
                cache->getPortManager().getDownstreamPort("req_out"));
            if (req_out) {
                xbar->registerPeerCache(full_name, req_out);  // 内部按 name 去重
            } else {
                DPRINTF(MODULE, "[ApuSoC] cache '%s' has no req_out port, skip\n",
                        full_name.c_str());
            }
        }

        // 命中 SimModule：递归下钻（CpuCluster/GpuCluster/GpcCluster/...）
        if (auto* sub = dynamic_cast<SimModule*>(obj)) {
            collectAndRegisterPeerCaches(xbar, sub, full_name);
        }
    }
}
```

**关键设计点**:
- **递归方向**: 从 ApuSoC（根）向下扫描到所有 SimModule 子树
- **路径拼接**: 用 `.` 分隔（如 `cpu.cache`、`gpu.gpc0.tpc0.cu0.cache`），保证 peer cache 名全局唯一
- **CacheTLM 命中**: 取 D.1 修复后的 `getDownstreamPort("req_out")`
- **SimModule 命中**: 递归下钻（CpuCluster 内部还有 CacheTLM / GpuCluster 4 级嵌套）
- **不调 `SimModule::incorporate_parent(parent)` 在递归中**: 因为我们在自己实现更深的遍历，基类递归对 CacheTLM 没意义（CacheTLM 不重写 incorporate_parent）

### 2.3 幂等性（双层保护）

**第一层**: `ApuSoC::peer_caches_wired_` 早退
```cpp
if (peer_caches_wired_) return;  // 二次调用直接 return
peer_caches_wired_ = true;
```

**第二层**: `CoherentXBarTLM::registerPeerCache` 按名去重
```cpp
auto it = std::find_if(peer_cache_req_outs_.begin(), peer_cache_req_outs_.end(),
                       [&](const auto& p) { return p.first == cache_name; });
if (it != peer_cache_req_outs_.end()) {
    DPRINTF(MODULE, "[CoherentXBar] peer '%s' already registered, skip\n",
            cache_name.c_str());
    return;
}
peer_cache_req_outs_.emplace_back(cache_name, req_out);
```

**为什么双层**:
- 第一层: 防止 ApuSoC::incorporate_parent 多次调用
- 第二层: 防止未来其他调用方（如 `GpuCluster::incorporate_parent` override）重复注册同名 peer

### 2.4 命名配置（避免硬编码）

**`ApuSoC` 新增字段**:
```cpp
private:
    std::string coherent_xbar_name_ = "xbar";   // 软配置（params 覆盖）
    bool peer_caches_wired_ = false;             // 幂等守卫
```

**`ApuSoC::set_config` 扩展**:
```cpp
void ApuSoC::set_config(const nlohmann::json& params) {
    if (params.contains("coherent_xbar_name")) {
        coherent_xbar_name_ = params["coherent_xbar_name"].get<std::string>();
    }
    // ... 现有 cpu_topology / gpu_topology 处理
}
```

**JSON 用法**:
```json
{
  "name": "apu_top",
  "type": "ApuSoC",
  "params": {
    "cpu_topology": "configs/templates/cpu_cluster_2level.json",
    "gpu_topology": "configs/templates/gpu_2gpc_2tpc_2cu.json",
    "coherent_xbar_name": "xbar"   // 可选，默认 "xbar"
  }
}
```

### 2.5 软失败策略

| 失败场景 | 行为 | 日志级别 |
|---------|------|---------|
| `getInternalInstance("xbar")` 返回 nullptr | `DPRINTF(MODULE, ...)` + `return` | WARN |
| `dynamic_cast<CoherentXBarTLM*>` 失败（其他类型） | 同上 | WARN |
| `cache->hasPortManager()` 返回 false | `continue`（不递归） | 无 |
| `getPortManager().getDownstreamPort("req_out")` 返回 nullptr | `DPRINTF(MODULE, ...)` + `continue` | WARN |
| `dynamic_cast<MasterPort*>` 失败 | 同上 | WARN |
| `registerPeerCache(name, nullptr)` | 抛 `std::runtime_error`（已有逻辑） | ERROR |

**为什么软失败**: 单元测试场景可能构造部分 ApuSoC（无 xbar），不应让 CI 崩溃。生产场景下 WARN 日志能定位缺失端口。

### 2.6 测试计划

#### 单元测试（5 个新 TEST_CASE）

| 测试名 | 文件 | 验证 |
|--------|------|------|
| `ApuSoC incorporates peer caches into CoherentXBar` | `test/test_apu_soc_top.cc` 扩展 | 手构 2 cache + 1 xbar → `peer_count() == 2` |
| `ApuSoC deep-recurses through CpuCluster/GpuCluster` | 同上 | 加载 apu_soc_v1.json → `peer_count() >= 4`（2 CPU cache + 2 GPU cu cache） |
| `incorporate_parent is idempotent` | 同上 | 连续调 2 次 `incorporate_parent` → `peer_count()` 不变 |
| `ApuSoC without xbar skips wiring gracefully` | 同上 | 删 xbar config → 不抛，`peer_count() == 0` |
| `[E2E] snoop_broadcast reaches all peers after incorporate_parent` | `test/test_coherent_xbar_tlm.cc` 扩展 | 完整 apu_soc_v1.json → 跑 100 cycles → 手动 snoop → 每个 peer cache req_in 收到副本 |

#### 回归测试

- 全测套 684/684 → 689/689 (+5 新测试)
- `[simmodule]` `[apu]` `[coherent_xbar]` `[chstream]` 标签全过
- `bash scripts/test/run_all_tests.sh` → apu_soc_v1.json 端到端 [SUCCESS]

---

## 3. 实施步骤

### Step 1: CoherentXBarTLM::registerPeerCache 加去重 (<30min)

**Files**:
- Modify: `src/tlm/coherent_xbar_tlm.cc` (L18-22)

**变更**: 在 `emplace_back` 之前加 `find_if` 去重检查 + WARN 日志。

**验证**:
- 现有 `[coherent_xbar]` 4 个测试仍 PASS
- 新增 "registerPeerCache rejects duplicate name" 测试 PASS

### Step 2: ApuSoC 字段 + set_config + helper + incorporate_parent (1-2h)

**Files**:
- Modify: `include/tlm/cluster/apu_soc.hh` (L25-27 新增字段)
- Modify: `src/tlm/cluster/apu_soc.cc` (L41-76 set_config + L78-80 incorporate_parent 重写 + 新增 collectAndRegisterPeerCaches)

**变更**:
1. 新增 `coherent_xbar_name_` + `peer_caches_wired_` 字段
2. `set_config` 增加 `coherent_xbar_name` 参数读取
3. `incorporate_parent` 重写为全树递归 wiring
4. 新增 `collectAndRegisterPeerCaches` 私有 helper

**验证**:
- 现有 `test_apu_soc_top.cc` 3 个测试仍 PASS（passthrough 移除不影响递归验证）
- 新增 4 个新 TEST_CASE PASS（incorporate / recurse / idempotent / no-xbar）

### Step 3: ModuleFactory Step 9 自动调用 (<30min)

**Files**:
- Modify: `src/core/module_factory.cc` (末尾添加 Step 9)

**变更**: 在 Step 8 之后追加 Step 9 循环，遍历顶层 SimModule 调 `incorporate_parent(nullptr)`。

**验证**:
- 现有 684/684 仍 PASS（`test_simmodule_nested.cc` 只调 `simulate_instantiate`，不触发 Step 9）
- apu_soc_v1.json E2E PASS

### Step 4: 测试扩展（1-2h）

**Files**:
- Modify: `test/test_apu_soc_top.cc` (添加 4 单元测试)
- Modify: `test/test_coherent_xbar_tlm.cc` (添加 1 E2E snoop 测试)

**验证**:
- 689/689 全过
- 重点验证 `peer_count() >= 4`（深递归）
- E2E snoop 测试：每个 peer cache 收到 1 个包副本

### Step 5: 文档同步（<30min）

**Files**:
- Modify: `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` §4.5.2 (标注 GpuCluster override 为 Phase 7.C+ 可选)
- Modify: `docs/architecture/01-hybrid-architecture-v2.1.md` §8.5.2 (添加 P1 incorporate_parent 章节)
- Modify: `CHANGELOG.md` (v2.4 条目)

**验证**:
- `docs_sync_check.sh --strict` 通过
- `clang-format --check` 0 违规

---

## 4. 文件结构

### 修改文件

| 路径 | 变更 |
|------|------|
| `src/tlm/coherent_xbar_tlm.cc` | `registerPeerCache` 加去重 (~10 行) |
| `include/tlm/cluster/apu_soc.hh` | 新增 2 字段 + 1 私有 method 声明 |
| `src/tlm/cluster/apu_soc.cc` | `set_config` 扩展 + `incorporate_parent` 重写 + 新增 helper (~50 行) |
| `src/core/module_factory.cc` | Step 9 自动调用 (~10 行) |
| `test/test_apu_soc_top.cc` | 4 新 TEST_CASE |
| `test/test_coherent_xbar_tlm.cc` | 1 新 E2E TEST_CASE |
| `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` | §4.5.2 标注 |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | §8.5.2 P1 章节 |
| `CHANGELOG.md` | v2.4 条目 |

### 不修改

- `SimModule::incorporate_parent` 基类（保持默认递归实现）
- 其他 8 个 SimModule 派生类（CpuCluster / ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC）— 不重写 incorporate_parent
- `configs/apu_soc_v1.json` — `coherent_xbar_name` 是可选 params，默认 "xbar" 已匹配现有 JSON
- `include/tlm/coherent_xbar_tlm.hh` — 不改 header（去重是 impl detail）
- `PortManager` / `SimModule` header — 不改

### 总 LOC

- Production: ~70 行
- Test: ~150 行 (5 新测试)
- Doc: ~30 行
- **总计: ~250 LOC**

---

## 5. 验收标准

- [ ] `cmake --build build -j$(nproc)` 通过
- [ ] `./build/bin/cpptlm_tests` → 689/689 PASS
- [ ] 5 新测试 PASS（4 单元 + 1 E2E snoop）
- [ ] `bash scripts/test/run_all_tests.sh` → apu_soc_v1.json E2E [SUCCESS]
- [ ] `xbar->peer_count() >= 4` 验证深递归
- [ ] 二次调用 `incorporate_parent` 不增加 peer_count（幂等）
- [ ] 删 xbar config 不抛异常（软失败）
- [ ] E2E snoop：手动 broadcast 后每个 peer cache 收到 1 副本
- [ ] `docs_sync_check.sh --strict` 通过
- [ ] `clang-format --check` 0 违规
- [ ] working tree clean + 1 commit 落地

---

## 6. 风险登记表

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| `ModuleFactory::module_instances` 字段名不准 | 低 | 中 | 实施者 grep 验证（spec 标注"需确认"） |
| Step 9 触发时机不当（StreamAdapter 未注入） | 低 | 中 | 已确认 Step 7 mirror 注入后 → Step 9 |
| 双重 wiring（ApuSoC + 未来 GpuCluster override） | 低 | 中 | `peer_caches_wired_` 早退 + `registerPeerCache` 按名去重 |
| `dynamic_cast<MasterPort*>` 失败（ChStream 端口未必是 MasterPort） | 低 | 低 | 软失败 + WARN 日志 |
| 测试 apu_soc_v1.json 加载后 `peer_count()` 不达 4 | 中 | 中 | 实际是 2 CPU + 2 GPU cache（每个 ComputeCluster 有 1 cache），需实测 |
| `ModuleFactory::Step 9` 影响其他 SimModule 测试 | 低 | 中 | `test_simmodule_nested.cc` 验证无副作用 |
| 命名 `"xbar"` 未来可能改 | 低 | 低 | `coherent_xbar_name` 参数可配置 |
| 多 xbar 实例（Phase 7.C+ 需求） | 低 | 中 | 当前单 xbar 设计，多 xbar 留 Phase 7.C 扩展 |

---

## 7. 关键决策日志

| # | 决策 | 替代方案 | 选定理由 |
|---|------|---------|---------|
| 1 | 调用模型 1A (ModuleFactory Step 9) | 1B 手动 / 1C lazy | 1B 用户负担；1C debug 复杂；1A 唯一保证功能触发 |
| 2 | Wiring 算法 2A (父端全树递归) | 2B pull / 2C visitor | 2B boilerplate；2C 过度；2A 简洁 + spec §3.3 基础 + 必须下钻 |
| 3 | GpuCluster 范围 3A (不重写) | 3B spec §4.5.2 / 3C WiringEvent | 3B 需新 API 且 ApuSoC 已覆盖；3C 过度；3A YAGNI |
| 4 | 幂等性双层保护 | 单层 / 无 | 防止 future 重构 + 多个调用方 |
| 5 | 软失败 (DPRINTF + skip) | 抛异常 | 单元测试场景无 xbar 合法；config 错误早暴露已由 P0 死代码清理保证 |
| 6 | `coherent_xbar_name_` 可配置 | 硬编码 "xbar" | spec 假设 "top_xbar" vs JSON 实际 "xbar" 漂移 |
| 7 | 实施顺序 5 步渐进 | 单次大改 | TDD 模式 + 早期失败早修复 |
| 8 | 不引入 `connectGpuToCoherentBus` 等 GPU 专用 API | 引入 | 父端已覆盖，YAGNI |

---

## 8. 引用

- **P0 Spec**: `docs/superpowers/specs/2026-06-19-p0-fixes-design.md` §3.3 ApuSoC wiring 伪代码
- **SimModule Hierarchy Spec**: `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` §4.5.2 P5 GpuCluster override 提议（标为 Phase 7.C+ 可选）
- **Handoff**: `docs/superpowers/handoffs/2026-06-19-p0-discussion-handoff.md` PENDING TASKS §4
- **ADR**: `docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md` (Phase 7.A/7.B write-through → 7.C 6×6 表)
- **CoherentXBarTLM API**: `include/tlm/coherent_xbar_tlm.hh` (P0 commit 5abba12)
- **D.1 PortManager mirror**: `include/core/port_manager.hh:200-206` (P0 commit fb56cc3)
- **ApuSoC stub**: `src/tlm/cluster/apu_soc.cc:78-80` (passthrough incorporate_parent)
- **Debug skill**: `.opencode/skills/cpptlm-debug/SKILL.md` (auto-loads on "test fail")

---

**Spec 版本**: v0.1 (Draft)
**下一步**: 用户 review → writing-plans 写实施计划
**预期开工**: 2026-06-20 (1-2 天)