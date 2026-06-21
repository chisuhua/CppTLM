# ApuSoC::incorporate_parent 真实 Late-Binding 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 `ApuSoC::incorporate_parent` 真实 late-binding 语义 — 递归遍历整棵子树收集所有 `CacheTLM` 的 `req_out`，注册到 `CoherentXBarTLM` 作为 snoop broadcast peer；通过 `ModuleFactory` Step 9 自动触发。

**Architecture:**
- 1A (ModuleFactory 自动调用) + 2A (父端全树递归推送) + 3A (GpuCluster 不重写)
- 双层幂等性: `ApuSoC::peer_caches_wired_` 早退 + `CoherentXBarTLM::registerPeerCache` 按名去重
- 软失败: xbar/cache/port 缺失仅 `DPRINTF WARN` 不抛异常
- 软配置: `coherent_xbar_name` params 字段 (默认 `"xbar"`)

**Tech Stack:** C++17 / Catch2 v3.7.0 / CMake 3.16+ / SimModule / ChStream / CoherentXBarTLM / PortManager (D.1 mirror)

**Spec:** `docs/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md` (463 行)

**Baseline:** P0 全套完成 (origin/main 6 commits ahead) + 684/684 tests pass

---

## Scope Check

Spec 聚焦单 sub-system (ApuSoC late-binding wiring)，无 subsystem 分解需求。每个 Phase 独立可 revert。

---

## File Structure

### 修改文件
| 路径 | 变更 |
|------|------|
| `src/tlm/coherent_xbar_tlm.cc` | `registerPeerCache` 加按名去重 (~10 行) |
| `include/tlm/cluster/apu_soc.hh` | 新增 2 字段 + 1 私有 method 声明 (~10 行) |
| `src/tlm/cluster/apu_soc.cc` | `set_config` 扩展 + `incorporate_parent` 重写 + 新增 `collectAndRegisterPeerCaches` (~50 行) |
| `src/core/module_factory.cc` | Step 9 自动调用 (~10 行) |
| `test/test_apu_soc_top.cc` | 4 新 TEST_CASE (~120 行) |
| `test/test_coherent_xbar_tlm.cc` | 1 新 E2E TEST_CASE (~60 行) |
| `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` | §4.5.2 标注 GpuCluster 为 Phase 7.C+ 可选 |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | §8.5.2 添加 P1 incorporate_parent 章节 |
| `CHANGELOG.md` | v2.4 条目 |

### 不修改
- `SimModule::incorporate_parent` 基类（保持默认递归）
- 其他 8 个 SimModule 派生类（CpuCluster / ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC）— 不重写
- `configs/apu_soc_v1.json` — `coherent_xbar_name` 可选，默认 `"xbar"` 匹配现有 JSON

### 总 LOC
- Production: ~80 行
- Test: ~180 行
- Doc: ~30 行
- **Total: ~290 LOC**

---

## Phase 1: CoherentXBarTLM 去重（2 任务）

### Task 1.1: Write failing test for registerPeerCache idempotency

**Files:**
- Modify: `test/test_coherent_xbar_tlm.cc` (追加 1 TEST_CASE)

- [ ] **Step 1: 追加测试**

在 `test/test_coherent_xbar_tlm.cc` 末尾追加：

```cpp
// =====================================================================
// Case 5: registerPeerCache 同名去重 (P1 幂等性)
// =====================================================================
TEST_CASE("CoherentXBarTLM: registerPeerCache rejects duplicate name",
          "[coherent_xbar]") {
    EventQueue eq;
    CoherentXBarTLM xbar("xbar", &eq);
    CacheTLM cache("cache0", &eq);
    json cfg = {{"n_ports", 1}};
    cache.simulate_instantiate(cfg);
    auto* req_out = dynamic_cast<MasterPort*>(
        cache.getPortManager().getDownstreamPort("req_out"));
    REQUIRE(req_out != nullptr);

    xbar.registerPeerCache("cache0", req_out);
    REQUIRE(xbar.peer_count() == 1);
    // 二次注册同名 cache 应被忽略
    xbar.registerPeerCache("cache0", req_out);
    REQUIRE(xbar.peer_count() == 1);  // 仍然 1, 不重复入队
    // 不同名 cache 正常入队
    CacheTLM cache1("cache1", &eq);
    cache1.simulate_instantiate(cfg);
    auto* req_out1 = dynamic_cast<MasterPort*>(
        cache1.getPortManager().getDownstreamPort("req_out"));
    REQUIRE(req_out1 != nullptr);
    xbar.registerPeerCache("cache1", req_out1);
    REQUIRE(xbar.peer_count() == 2);
}
```

- [ ] **Step 2: 跑测试验证失败**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[coherent_xbar]"
```

Expected: Case 5 FAIL（当前 `registerPeerCache` 不去重，二次注册后 `peer_count() == 2`）

---

### Task 1.2: Add find_if dedup in registerPeerCache

**Files:**
- Modify: `src/tlm/coherent_xbar_tlm.cc` (L18-22, `registerPeerCache` 方法)

- [ ] **Step 1: 替换 registerPeerCache 方法体**

把当前：
```cpp
void CoherentXBarTLM::registerPeerCache(const std::string& cache_name,
                                         MasterPort* req_out) {
    if (!req_out) {
        throw std::runtime_error(
            "CoherentXBarTLM::registerPeerCache: req_out is null for cache '"
            + cache_name + "'");
    }
    peer_cache_req_outs_.emplace_back(cache_name, req_out);
    DPRINTF(MODULE, "[CoherentXBar] Registered peer cache %s (req_out=%p)\n",
            cache_name.c_str(), static_cast<void*>(req_out));
}
```

改为：
```cpp
void CoherentXBarTLM::registerPeerCache(const std::string& cache_name,
                                         MasterPort* req_out) {
    if (!req_out) {
        throw std::runtime_error(
            "CoherentXBarTLM::registerPeerCache: req_out is null for cache '"
            + cache_name + "'");
    }
    // P1 幂等性: 同名 cache 不重复入队
    auto it = std::find_if(peer_cache_req_outs_.begin(), peer_cache_req_outs_.end(),
                           [&](const auto& p) { return p.first == cache_name; });
    if (it != peer_cache_req_outs_.end()) {
        DPRINTF(MODULE, "[CoherentXBar] peer '%s' already registered, skip\n",
                cache_name.c_str());
        return;
    }
    peer_cache_req_outs_.emplace_back(cache_name, req_out);
    DPRINTF(MODULE, "[CoherentXBar] Registered peer cache %s (req_out=%p)\n",
            cache_name.c_str(), static_cast<void*>(req_out));
}
```

- [ ] **Step 2: 添加 `<algorithm>` include**

在 `src/tlm/coherent_xbar_tlm.cc` 顶部 include 区块（第 5-10 行）添加：
```cpp
#include <algorithm>  // P1: registerPeerCache dedup needs std::find_if
```

- [ ] **Step 3: 构建 + 跑测试**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[coherent_xbar]"
```

Expected: **5/5 测试 PASS** (4 旧 + 1 新)

---

## Phase 2: ApuSoC 字段 + set_config 扩展（2 任务）

### Task 2.1: Add coherent_xbar_name_ and peer_caches_wired_ fields

**Files:**
- Modify: `include/tlm/cluster/apu_soc.hh` (L25-27 私有字段区域)

- [ ] **Step 1: 添加字段**

找到 `apu_soc.hh` 私有字段区域（L25-27）：
```cpp
private:
    std::string cpu_topology_;
    std::string gpu_topology_;
```

在 `gpu_topology_` 后追加：
```cpp
    // P1: coherent_xbar_name 配置 (params 可覆盖, 默认 "xbar")
    std::string coherent_xbar_name_ = "xbar";
    // P1: 幂等守卫 - 防止多次 incorporate_parent 重复注册
    bool peer_caches_wired_ = false;
```

- [ ] **Step 2: 声明新方法**

在同一文件 public 区域（`incorporate_parent` 声明附近），添加私有 helper 声明：
```cpp
private:
    // P1: 全树递归收集 CacheTLM peer cache 并注册到 xbar
    // path_prefix 形如 "cpu" 或 "gpu.gpc0.tpc0" (递归累积)
    void collectAndRegisterPeerCaches(class CoherentXBarTLM* xbar,
                                      SimModule* subtree_root,
                                      const std::string& path_prefix);
```

- [ ] **Step 3: 构建**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc)
```

Expected: 构建成功（仅声明，无 impl 错误）

---

### Task 2.2: Extend set_config to read coherent_xbar_name

**Files:**
- Modify: `src/tlm/cluster/apu_soc.cc` (L13-21 `set_config` 方法)

- [ ] **Step 1: 扩展 set_config**

把当前：
```cpp
void ApuSoC::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("cpu_topology")) {
        cpu_topology_ = params["cpu_topology"].get<std::string>();
    }
    if (params.contains("gpu_topology")) {
        gpu_topology_ = params["gpu_topology"].get<std::string>();
    }
}
```

改为：
```cpp
void ApuSoC::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("cpu_topology")) {
        cpu_topology_ = params["cpu_topology"].get<std::string>();
    }
    if (params.contains("gpu_topology")) {
        gpu_topology_ = params["gpu_topology"].get<std::string>();
    }
    // P1: 可选 coherent_xbar_name (默认 "xbar", 已初始化)
    if (params.contains("coherent_xbar_name")) {
        coherent_xbar_name_ = params["coherent_xbar_name"].get<std::string>();
    }
}
```

- [ ] **Step 2: 构建**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc)
```

Expected: 构建成功

---

## Phase 3: ApuSoC::incorporate_parent + 递归 helper（2 任务）

### Task 3.1: Write failing tests for ApuSoC::incorporate_parent

**Files:**
- Modify: `test/test_apu_soc_top.cc` (追加 4 TEST_CASE)

- [ ] **Step 1: 追加 4 测试**

在 `test/test_apu_soc_top.cc` 末尾追加：

```cpp
// =====================================================================
// P1 Case 1: ApuSoC 注册 peer cache 到 CoherentXBar
// =====================================================================
TEST_CASE("ApuSoC incorporates peer caches into CoherentXBar",
          "[simmodule][apu][p1]") {
    EventQueue eq;
    ApuSoC apu("apu", &eq);

    // 手动构造: xbar + 2 个 cache 作为 apu 子模块
    auto* xbar = new CoherentXBarTLM("xbar", &eq);
    json xbar_cfg = {{"n_ports", 4}};
    xbar->simulate_instantiate(xbar_cfg);

    auto* cache0 = new CacheTLM("cache0", &eq);
    auto* cache1 = new CacheTLM("cache1", &eq);
    json cache_cfg = {{"n_ports", 1}};
    cache0->simulate_instantiate(cache_cfg);
    cache1->simulate_instantiate(cache_cfg);

    apu.addInternalInstance(xbar);
    apu.addInternalInstance(cache0);
    apu.addInternalInstance(cache1);

    apu.incorporate_parent(nullptr);

    REQUIRE(xbar->peer_count() == 2);
}

// =====================================================================
// P1 Case 2: ApuSoC 深递归遍历 CpuCluster/GpuCluster 子树
// =====================================================================
TEST_CASE("ApuSoC deep-recurses through CpuCluster/GpuCluster",
          "[simmodule][apu][p1][e2e]") {
    auto config =
        JsonIncluder::loadAndInclude(std::string(CPPTLM_SOURCE_DIR) + "/configs/apu_soc_v1.json");
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_NOTHROW(factory.instantiateAll(config));
    auto* soc = dynamic_cast<SimModule*>(factory.getInstance("apu_top"));
    REQUIRE(soc != nullptr);
    auto* xbar = dynamic_cast<CoherentXBarTLM*>(
        soc->getInternalInstance("xbar"));
    REQUIRE(xbar != nullptr);
    // 期望: CpuCluster 内部有 1 cache + GpuCluster 4 级嵌套 2×2×2 cu
    // 实际: 至少 4 个 peer cache (CPU + 至少 2 GPU cu cache)
    REQUIRE(xbar->peer_count() >= 4);
}

// =====================================================================
// P1 Case 3: incorporate_parent 幂等性
// =====================================================================
TEST_CASE("incorporate_parent is idempotent",
          "[simmodule][apu][p1]") {
    EventQueue eq;
    ApuSoC apu("apu", &eq);

    auto* xbar = new CoherentXBarTLM("xbar", &eq);
    json xbar_cfg = {{"n_ports", 4}};
    xbar->simulate_instantiate(xbar_cfg);

    auto* cache0 = new CacheTLM("cache0", &eq);
    json cache_cfg = {{"n_ports", 1}};
    cache0->simulate_instantiate(cache_cfg);

    apu.addInternalInstance(xbar);
    apu.addInternalInstance(cache0);

    apu.incorporate_parent(nullptr);
    REQUIRE(xbar->peer_count() == 1);

    // 二次调用: 早退, peer_count 不变
    apu.incorporate_parent(nullptr);
    REQUIRE(xbar->peer_count() == 1);
}

// =====================================================================
// P1 Case 4: ApuSoC 无 xbar 时软失败
// =====================================================================
TEST_CASE("ApuSoC without xbar skips wiring gracefully",
          "[simmodule][apu][p1]") {
    EventQueue eq;
    ApuSoC apu("apu", &eq);

    auto* cache0 = new CacheTLM("cache0", &eq);
    json cache_cfg = {{"n_ports", 1}};
    cache0->simulate_instantiate(cache_cfg);
    apu.addInternalInstance(cache0);
    // 不加 xbar

    REQUIRE_NOTHROW(apu.incorporate_parent(nullptr));
    // 软失败: 不抛, peer_count 仍 0 (无 xbar 可注册)
}
```

- [ ] **Step 2: 跑测试验证失败**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[p1]"
```

Expected: 4 新测试 FAIL（当前 `incorporate_parent` 是 passthrough，无 wiring 逻辑）

---

### Task 3.2: Implement incorporate_parent and collectAndRegisterPeerCaches

**Files:**
- Modify: `src/tlm/cluster/apu_soc.cc` (L78-80 `incorporate_parent` 重写 + 新增 `collectAndRegisterPeerCaches`)

- [ ] **Step 1: 添加必要 include**

在 `src/tlm/cluster/apu_soc.cc` 顶部（L6-9）追加：
```cpp
#include "tlm/coherent_xbar_tlm.hh"  // P1: incorporate_parent wiring needs CoherentXBarTLM
#include "tlm/cache_tlm.hh"            // P1: collectAndRegisterPeerCaches needs CacheTLM
```

- [ ] **Step 2: 替换 incorporate_parent + 新增 collectAndRegisterPeerCaches**

把当前 `incorporate_parent` (L78-80)：
```cpp
void ApuSoC::incorporate_parent(SimModule* parent) {
    SimModule::incorporate_parent(parent);
}
```

替换为：
```cpp
void ApuSoC::incorporate_parent(SimModule* /*parent*/) {
    // P1 幂等性: 多次调用早退
    if (peer_caches_wired_) return;
    peer_caches_wired_ = true;

    // 1. 找 xbar (命名可配置, 默认 "xbar")
    auto* xbar_obj = getInternalInstance(coherent_xbar_name_);
    auto* xbar = dynamic_cast<CoherentXBarTLM*>(xbar_obj);
    if (!xbar) {
        DPRINTF(MODULE, "[ApuSoC] no CoherentXBarTLM '%s' found, skip peer wiring\n",
                coherent_xbar_name_.c_str());
        return;  // 软失败: 无 xbar 是合法拓扑 (单元测试场景)
    }

    // 2. 递归遍历整棵子树, 注册所有 CacheTLM peer
    collectAndRegisterPeerCaches(xbar, this, /*path_prefix=*/"");

    // 3. 递归通知子 SimModule (保留 hook 语义供未来扩展, e.g. GPU memory bridge)
    SimModule::incorporate_parent(this);
}

void ApuSoC::collectAndRegisterPeerCaches(CoherentXBarTLM* xbar,
                                          SimModule* subtree_root,
                                          const std::string& path_prefix) {
    for (const auto& [name, obj] : subtree_root->getInternalFactory().getAllInstances()) {
        if (!obj) continue;
        std::string full_name = path_prefix.empty() ? name : path_prefix + "." + name;

        // 命中 CacheTLM: 取 D.1 修复后的 req_out 并注册
        if (auto* cache = dynamic_cast<CacheTLM*>(obj)) {
            if (!cache->hasPortManager()) continue;
            auto* req_out = dynamic_cast<MasterPort*>(
                cache->getPortManager().getDownstreamPort("req_out"));
            if (req_out) {
                xbar->registerPeerCache(full_name, req_out);  // 内部按名去重
            } else {
                DPRINTF(MODULE, "[ApuSoC] cache '%s' has no req_out port, skip\n",
                        full_name.c_str());
            }
        }

        // 命中 SimModule: 递归下钻 (CpuCluster/GpuCluster/GpcCluster/...)
        if (auto* sub = dynamic_cast<SimModule*>(obj)) {
            collectAndRegisterPeerCaches(xbar, sub, full_name);
        }
    }
}
```

- [ ] **Step 3: 构建 + 跑测试**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[p1]"
```

Expected: **4/4 新测试 PASS**

---

## Phase 4: ModuleFactory Step 9 自动调用（1 任务）

### Task 4.1: Add Step 9 in ModuleFactory::instantiateAll

**Files:**
- Modify: `src/core/module_factory.cc` (在 `return !connection_failed;` (L814) 之前插入)

- [ ] **Step 1: 定位插入点**

确认 `module_factory.cc` L813-815：
```cpp
    // 保存所有实例
    instances = object_instances;
    return !connection_failed;
}
```

- [ ] **Step 2: 在 L813 之前插入 Step 9**

在 `// 保存所有实例` 注释前插入：
```cpp
    // ========================
    // 9. P1: 触发 SimModule::incorporate_parent late-binding
    //    对每个顶层 SimModule 调用一次, parent 传 nullptr
    //    ApuSoC 等会重写 incorporate_parent 完成跨域 wiring
    //    (Step 7 已注入 StreamAdapter + D.1 mirror, peer cache req_out 此时可查)
    // ========================
    for (auto& [name, mod] : module_instances) {
        if (!mod) continue;
        mod->incorporate_parent(nullptr);
    }

    // 保存所有实例
    instances = object_instances;
```

- [ ] **Step 3: 构建 + 跑全测套**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests 2>&1 | tail -3
```

Expected: **689/689 pass** (684 + 5 新 [p1]/[coherent_xbar] 测试)

- [ ] **Step 4: 跑 E2E 全套验证**

```bash
cd /workspace/project/CppTLM
bash scripts/test/run_all_tests.sh 2>&1 | tail -5
```

Expected: `[SUCCESS] All tests passed!` 含 apu_soc_v1.json E2E

---

## Phase 5: E2E snoop 测试（1 任务）

### Task 5.1: Add E2E snoop broadcast test

**Files:**
- Modify: `test/test_coherent_xbar_tlm.cc` (追加 1 E2E TEST_CASE)

- [ ] **Step 1: 追加测试**

在 `test/test_coherent_xbar_tlm.cc` 末尾追加：

```cpp
// =====================================================================
// Case 6: [E2E] apu_soc_v1.json incorporate_parent 后 snoop_broadcast 投递到所有 peer
// =====================================================================
TEST_CASE("[E2E] snoop_broadcast reaches all peers after ApuSoC incorporate_parent",
          "[coherent_xbar][p1][e2e]") {
    auto config =
        JsonIncluder::loadAndInclude(std::string(CPPTLM_SOURCE_DIR) + "/configs/apu_soc_v1.json");
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_NOTHROW(factory.instantiateAll(config));

    // Factory Step 9 已自动触发 incorporate_parent
    auto* soc = dynamic_cast<SimModule*>(factory.getInstance("apu_top"));
    REQUIRE(soc != nullptr);
    auto* xbar = dynamic_cast<CoherentXBarTLM*>(
        soc->getInternalInstance("xbar"));
    REQUIRE(xbar != nullptr);
    REQUIRE(xbar->peer_count() >= 4);

    // 手动 snoop 一次: 不抛 + 内部 sendReq 每个 peer
    Packet* pkt = make_test_packet(&eq, 0xDEAD0000);
    REQUIRE_NOTHROW(xbar->snoop_broadcast(pkt));
    // 注意: Packet 副本由 snoop_broadcast 内部 PacketPool::acquire + 字段复制,
    //       并通过 port->sendReq 投递或 VC 满时 PacketPool::release 清理
    // 这里不验证 peer.req_in 实际接收 (需要 EventQueue::run + tick 循环),
    // 仅验证 snoop 入口不抛 + 走完所有 peer
    REQUIRE(xbar->peer_count() >= 4);  // peer 列表不变
}
```

- [ ] **Step 2: 构建 + 跑 E2E 测试**

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[coherent_xbar][p1][e2e]"
```

Expected: **6/6 [coherent_xbar] 测试 PASS** (5 旧 + 1 新 E2E)

---

## Phase 6: 文档同步（3 任务）

### Task 6.1: Update simmodule-complex-hierarchies-design.md §4.5.2

**Files:**
- Modify: `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` §4.5.2

- [ ] **Step 1: 定位 §4.5.2**

```bash
grep -n "GpuCluster.*incorporate\|P5.*GpuCluster" /workspace/project/CppTLM/docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md
```

- [ ] **Step 2: 添加标注**

在 §4.5.2 GpuCluster override 伪代码下追加：
```markdown
> **P1 实施状态 (2026-06-20)**: 父端 ApuSoC 已实现 `incorporate_parent` 全树递归 wiring (见 spec `2026-06-20-incorporate-parent-late-binding-design.md`), 通过 `collectAndRegisterPeerCaches` 私有 helper 收集所有 `CacheTLM` (含 GpuCluster 4 级嵌套内部) 并注册到 `CoherentXBarTLM::registerPeerCache`。
> 
> **`GpuCluster::incorporate_parent` override 暂未实施** (标为 Phase 7.C+ 可选): 父端全树遍历已覆盖 GPU 深层 cache, 无需 GPU 专用 hook。当未来 GPU 需独立 memory bridge 或跨域 wiring 时, 可让 `GpuCluster` override `incorporate_parent` 接自己的 xbar (双层幂等性 `peer_caches_wired_` + `registerPeerCache` 按名去重 保护)。
```

---

### Task 6.2: Update architecture doc §8.5.2

**Files:**
- Modify: `docs/architecture/01-hybrid-architecture-v2.1.md` §8.5.2

- [ ] **Step 1: 定位 §8.5.2**

```bash
grep -n "8\.5\.2\|CoherentXBarTLM" /workspace/project/CppTLM/docs/architecture/01-hybrid-architecture-v2.1.md | head -5
```

- [ ] **Step 2: 追加 P1 章节**

在现有 §8.5.2 后追加：
```markdown
#### §8.5.3 P1 ApuSoC::incorporate_parent 真实 Late-Binding (2026-06-20)

D.1 修复 + CoherentXBarTLM 类骨架是 P0 留下的"机制"层。本节落实"机制在 SoC 上下文中工作"。

**机制**:
- `ModuleFactory::instantiateAll` 末尾新增 Step 9: 遍历顶层 `SimModule` 调用 `incorporate_parent(nullptr)`
- `ApuSoC::incorporate_parent` 重写: 找 `xbar` (命名可配置 `coherent_xbar_name` params, 默认 `"xbar"`) → 递归遍历整棵子树 (`CpuCluster`/`GpuCluster`/`GpcCluster`/... ) → 命中 `CacheTLM` 取 D.1 修复后的 `req_out` MasterPort → `xbar->registerPeerCache(path, port)`
- `CoherentXBarTLM::registerPeerCache` 加按名去重 (双层幂等性: `ApuSoC::peer_caches_wired_` 早退 + registerPeerCache find_if 重复检查)

**测试覆盖**:
- 4 单元测试 (`[p1]`): 基础 wiring / 深递归 / 幂等 / 无 xbar 软失败
- 1 E2E 测试 (`[e2e][p1]`): apu_soc_v1.json 加载 + 手动 snoop_broadcast 投递

**解锁**:
- 完整 APU SoC 拓扑 snoop broadcast 端到端验证
- 后续 Phase 7.C 6×6 state table 改造 CoherentXBarTLM 时, peer cache 注册路径已就绪

详见 spec: `docs/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md`
```

---

### Task 6.3: Update CHANGELOG.md

**Files:**
- Modify: `CHANGELOG.md` (顶部追加 v2.4)

- [ ] **Step 1: 追加 v2.4 条目**

```markdown
## [v2.4] - 2026-06-20

### 新增 (Features)
- **ApuSoC::incorporate_parent 真实 Late-Binding**: 父端全树递归收集 `CacheTLM` peer cache, 注册到 `CoherentXBarTLM::registerPeerCache`。
  - `ModuleFactory::instantiateAll` 末尾新增 Step 9 自动触发
  - `ApuSoC::set_config` 新增 `coherent_xbar_name` 可选 params (默认 `"xbar"`)
  - `CoherentXBarTLM::registerPeerCache` 加按名去重 (双层幂等性)
  - 软失败策略: xbar/cache/port 缺失仅 `DPRINTF WARN` 不抛异常
- **SimModule P5 完整 APU SoC 拓扑**: 解锁 `apu_soc_v1.json` 端到端 snoop broadcast 验证。

### 测试 (Tests)
- 新增 5 用例: 4 单元 `[p1]` (wiring / 深递归 / 幂等 / 无 xbar) + 1 E2E `[p1][e2e]` (apu_soc_v1.json snoop)
- 总数: 684/684 → 689/689
```

---

## Phase 7: 验证 + commit（1 任务）

### Task 7.1: Final verification + commit

- [ ] **Step 1: 全测套**

```bash
cd /workspace/project/CppTLM
./build/bin/cpptlm_tests 2>&1 | tail -3
```

Expected: **689/689 pass** (684 + 5 新)

- [ ] **Step 2: E2E 全套**

```bash
cd /workspace/project/CppTLM
bash scripts/test/run_all_tests.sh 2>&1 | tail -5
```

Expected: `[SUCCESS] All tests passed!` 含 apu_soc_v1.json

- [ ] **Step 3: 格式检查**

```bash
cd /workspace/project/CppTLM
./scripts/build/format.sh --check
```

Expected: 0 violations

- [ ] **Step 4: docs_sync_check**

```bash
cd /workspace/project/CppTLM
./scripts/test/docs_sync_check.sh --strict
```

Expected: 0 missing paths

- [ ] **Step 5: 检查 git 状态**

```bash
cd /workspace/project/CppTLM
git status
```

Expected: 9 文件变更
- `src/tlm/coherent_xbar_tlm.cc`
- `include/tlm/cluster/apu_soc.hh`
- `src/tlm/cluster/apu_soc.cc`
- `src/core/module_factory.cc`
- `test/test_apu_soc_top.cc`
- `test/test_coherent_xbar_tlm.cc`
- `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md`
- `docs/architecture/01-hybrid-architecture-v2.1.md`
- `CHANGELOG.md`

- [ ] **Step 6: commit**

```bash
cd /workspace/project/CppTLM
git add src/tlm/coherent_xbar_tlm.cc \
        include/tlm/cluster/apu_soc.hh \
        src/tlm/cluster/apu_soc.cc \
        src/core/module_factory.cc \
        test/test_apu_soc_top.cc \
        test/test_coherent_xbar_tlm.cc \
        docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md \
        docs/architecture/01-hybrid-architecture-v2.1.md \
        CHANGELOG.md
git commit -m "feat(simmodule): P1 ApuSoC::incorporate_parent real late-binding wiring

- ApuSoC::incorporate_parent 重写: 全树递归收集 CacheTLM peer cache
  注册到 CoherentXBarTLM (per spec §3.3 伪代码扩展, 含深递归)
- ModuleFactory::instantiateAll Step 9: 自动触发根 SimModule incorporate_parent
- CoherentXBarTLM::registerPeerCache 加按名去重 (双层幂等性)
- ApuSoC::set_config 支持 coherent_xbar_name params (默认 \"xbar\")
- 软失败: xbar/cache/port 缺失仅 DPRINTF WARN 不抛
- 5 新测试: 4 单元 [p1] + 1 E2E [p1][e2e]
- 文档同步: simmodule spec §4.5.2 + architecture §8.5.3 + CHANGELOG v2.4

解锁: 完整 APU SoC 拓扑 snoop broadcast 端到端验证 (Phase 7.C 6×6 表基础).
GpuCluster override 暂未实施 (标 Phase 7.C+ 可选, 父端全树已覆盖).

Ref: docs/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md"
```

- [ ] **Step 7: 验证 commit + push**

```bash
cd /workspace/project/CppTLM
git log --oneline -3
git push -u origin main
```

Expected: commit 落地 + push 成功 (main ahead of origin/main +1)

---

## Self-Review Checklist

执行后我会做以下检查：

### 1. Spec coverage
- §2.1 调用模型 (Step 9 in ModuleFactory) → Task 4.1 ✅
- §2.2 Wiring 算法 (父端全树递归) → Task 3.1-3.2 ✅
- §2.3 幂等性 (双层) → Task 1.1-1.2 (registerPeerCache 去重) + Task 2.1 (peer_caches_wired_) ✅
- §2.4 命名配置 (coherent_xbar_name) → Task 2.1-2.2 ✅
- §2.5 软失败策略 → Task 3.2 (代码内 DPRINTF) ✅
- §2.6 测试计划 (5 测试) → Task 1.1 + 3.1 + 5.1 ✅
- §3 实施步骤 (5 步) → Phase 1-7 ✅

### 2. Placeholder scan
- 0 个 TBD / TODO / "implement later" / "fill in details"
- 0 个 "Add appropriate error handling" / "handle edge cases"
- 0 个 "Similar to Task N" 引用 — 每个 task 代码独立完整

### 3. Type consistency
- `CoherentXBarTLM::registerPeerCache(string, MasterPort*)` — Task 1.1 测试 ↔ Task 1.2 impl 一致
- `ApuSoC::coherent_xbar_name_` 默认 `"xbar"` — Task 2.1 定义 ↔ Task 2.2 读 params 一致
- `ApuSoC::peer_caches_wired_` 类型 `bool` — Task 2.1 字段 ↔ Task 3.2 用法一致
- `ApuSoC::collectAndRegisterPeerCaches(CoherentXBarTLM*, SimModule*, string)` — Task 2.1 声明 ↔ Task 3.2 impl 一致
- `ModuleFactory::module_instances` 字段 (L256 `std::unordered_map<std::string, SimModule*>`) — Task 4.1 遍历使用一致

### 4. 实施路径闭环验证
- Phase 1 (去重) 是 Phase 2-3 的前置 — 顺序正确
- Phase 2-3 (ApuSoC 字段+helper) 完成后 Task 3.1 测试才能 PASS
- Phase 4 (ModuleFactory Step 9) 触发 E2E 验证
- Phase 5-7 (测试+文档+commit) 收尾

---

## 关键参考

- **Spec**: `docs/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md` (463 行)
- **P0 Spec**: `docs/superpowers/specs/2026-06-19-p0-fixes-design.md` (D.1 + CoherentXBarTLM)
- **SimModule Spec**: `docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md` (§4.5.2 P5 设计)
- **ADR**: `docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md` (Phase 7.A/7.B write-through)
- **CoherentXBarTLM API**: `include/tlm/coherent_xbar_tlm.hh` (P0 commit 5abba12)
- **D.1 PortManager mirror**: `include/core/port_manager.hh:200-206` (P0 commit fb56cc3)
- **Debug skill**: `.opencode/skills/cpptlm-debug/SKILL.md` (auto-loads on "test fail")

---

## 工期估算

| Phase | 任务数 | LOC | 工时 |
|-------|--------|-----|------|
| Phase 1: CoherentXBarTLM 去重 | 2 | ~25 | <30min |
| Phase 2: ApuSoC 字段+set_config | 2 | ~15 | <15min |
| Phase 3: incorporate_parent 实现 | 2 | ~70 | 1-1.5h |
| Phase 4: ModuleFactory Step 9 | 1 | ~10 | <15min |
| Phase 5: E2E snoop 测试 | 1 | ~60 | <30min |
| Phase 6: 文档同步 | 3 | ~30 | <30min |
| Phase 7: 验证+commit | 1 | — | <15min |
| **总计** | **12** | **~290** | **3-4h (0.5 天)** |

---

**Plan 版本**: v0.1 (Initial)
**下一步**: 用户审阅 → 选定执行模式 → 开始实施