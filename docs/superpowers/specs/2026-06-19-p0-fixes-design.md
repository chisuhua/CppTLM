# P0 全套修复设计 (D.1 Fix + CoherentXBarTLM Skeleton + Dead Code Cleanup)

**Status**: Draft v0.1 · **Date**: 2026-06-19 · **Branch**: main · **Author**: Sisyphus (brainstorming)
**Scope**: 解锁 P3 connectCPU + P5 incorporate_parent 真实 wiring + CoherentXBarTLM snoop 广播
**Baseline**: 24 commits / 673/673 C++ tests + 222/222 Python tests / 10 SimModule 类全部注册
**前置 spec**: `2026-06-19-simmodule-complex-hierarchies-design.md` (5 Phase 已落地)
**遵循 ADR**: `ADR-SOC-01-coherence-protocol-strategy.md` (Phase 7.A/7.B write-through → 7.C 6×6 表)

---

## 0. 概述

### 0.1 目标

完成 P0 全套 3 项修复，解锁后续 P1/P2/P3 阶段：

| ID | 名称 | 解锁什么 |
|----|------|---------|
| **D.1** | PortManager mirror API | `getInternalOutputPort("cpu0.req_out")` 对 ChStream 模块返回非空 |
| **CoherentXBarTLM** | APU 顶层跨域 snoop 广播类 | apu_soc_v1.json 顶层从 CrossbarTLM 升级为支持 snoop |
| **Dead Code Cleanup** | 删 helper 中的 lazy registration + 死 throw | 3 个 helper 文件从"静默 fallback"转为"配置错误早暴露" |

### 0.2 非目标 (YAGNI)

- **不上 6×6 coherence 状态表** — Phase 7.C 任务；P0 仅做 write-through 透传
- **不实现 MOESI 5 态** — ADR-SOC-01 §2 明文选择 "简化为 3 态起步 → 6×6 升级"，P0 走第一步
- **不做 connectCPU helper 实现** — P0 修复 D.1 后，connectCPU 是 P1 任务（PENDING TASKS §4-6）
- **不做 incorporate_parent 真实 late-binding 语义** — P1 任务，本 spec 仅解锁路径
- **不触碰 legacy 模块** — `include/modules/legacy/` 按架构约定只修严重 bug
- **不重命名任何类** — P0 全套不引入命名变更

### 0.3 核心决策（Oracle 综合推荐）

| 决策 | 选择 | 理由 |
|------|------|------|
| D.1 修复方案 | **方案 A-1（仅 mirror 入 map，不入 vector）** | Oracle 工程验证：PortManager 是非拥有型注册表（无析构函数，已是查找表语义），mirror 是注册的合法形式；A-2 会破坏 `getDownstreamPorts().size()` 断言（5-10 测试）；B 是过早抽象 |
| D.1 label 命名 | **不带模块名前缀**（仅 `"req_out"`，非 `"cpu0.req_out"`） | 与 `SimModule::parsePortSpec` 二元解析对齐（`getInternalOutputPort("cpu0.req_out")` 解析为 `("cpu0", "req_out")`） |
| CoherentXBarTLM 范围 | **骨架 + write-through 透传** | 对齐 ADR-SOC-01 §3 Phase 7.A/7.B；6×6 状态表留 Phase 7.C |
| CoherentXBarTLM 继承 | **`class CoherentXBarTLM : public CrossbarTLM`** | 复用所有路由/VC 仲裁逻辑，仅增量加 `snoop_broadcast` + `registerPeerCache` |
| 死代码清理选项 | **(b) 删 lazy registration + 删 throw** | 符合零债务原则："配置错误早暴露"优于"静默 fallback 到 `{4}` buffer" |
| 实施顺序 | **D.1 → CoherentXBarTLM → 死代码（串行）** | D.1 是其他两项的前置；后两项无相互依赖但串行便于 review |
| 总工期 | **1.5-2 天 / ~225 LOC** | D.1 (~15+30 测试) + CoherentXBarTLM (~80+50 测试) + 死代码清理 (~30+20 测试) |

---

## 1. 背景与动机

### 1.1 现状（24 commits 已落地后的 baseline）

- **SimModule 体系**：10 个派生类（P2 4 GPU 端 + P4 3 基础设施 + P5 1 顶层 + CpuCluster），全部 REGISTER_MODULE 注册
- **ChStream helper**：3 个（connectBus / connectCPUSideBus / connectMemSideBus），P3 partial 实现
- **D.4 + D.5 修复**：`findInternalPath` 递归 + `tick` 默认递归已落地（上一 spec）
- **测试现状**：673/673 C++ + 222/222 Python + 5 个新 SimModule 测试文件 + 16 新 TEST_CASE

### 1.2 痛点（为什么必须 P0 修复）

#### 痛点 1：`getInternalOutputPort` 对 ChStream 模块永远返回 nullptr

```cpp
// test/test_simmodule_nested.cc:195-206 现状 (软断言)
auto* req_out_port = cluster->getInternalOutputPort("cpu0.req_out");
if (req_out_port == nullptr) {
    WARN("getInternalOutputPort returned nullptr for CPUTLM.req_out "
         "(ChStream 端口在 PortManager 之外, 这是已知架构限制).");
}
```

**根本原因**（已读源码确认）：
- `module_factory.cc:637-649` 创建 ChStream 端口后，**仅**存入 `ch_initiator_ports_/ch_target_ports_` 工厂成员
- 子模块的 `PortManager` 完全不知道这些端口存在
- `getInternalOutputPort` 走 `getPortManager().getDownstreamPort(port_name)` 查 map → null

**影响**：
- ❌ P3 `connectCPU` helper 无法找到 CPU 的 `req_out`
- ❌ P5 `incorporate_parent` 真实 late-binding 拿不到跨 cluster 的端口
- ❌ CoherentXBarTLM snoop broadcast 不知道 peer cache 的 `req_out`

#### 痛点 2：apu_soc_v1.json 顶层用 CrossbarTLM 占位

```json
// configs/apu_soc_v1.json 当前状态
{
  "modules": [
    { "name": "top_xbar", "type": "CrossbarTLM", ... },  // ← 占位
    ...
  ]
}
```

**设计意图**：顶层总线应支持 snoop broadcast（CPU 写请求触发所有 peer cache 失效）。当前用 CrossbarTLM 占位，逻辑上不具备 snoop 能力。

#### 痛点 3：死代码 + 静默 fallback

```cpp
// src/tlm/cache_tlm.cc:22-36 现状
if (!getPortManager().getUpstreamPort("mem_side")) {
    getPortManager().addUpstreamPort(this, {4}, {}, "mem_side");  // ← lazy
}
auto* mem_side = getPortManager().getUpstreamPort("mem_side");
if (!mem_side || !bus_port) {
    throw std::runtime_error("CacheTLM::connectBus: port not found");  // ← 死代码
}
```

**根本原因**：
- 第 22 行 lazy 注册 → 第 25 行必然非 null
- 第 34-36 行 `!mem_side || !bus_port` 永远为 false
- throw 路径**永远不可达**

**设计缺陷**：lazy registration 让"忘记配 port"成为**静默错误**——helper 默默给默认 `{4}` buffer，违反零债务原则。

---

## 2. 修复 1：D.1 — PortManager Mirror API

### 2.1 设计方案 A-1（Oracle 综合推荐）

**核心思路**：新增 2 个 mirror 方法到 `PortManager`，让 ChStream 端口能被注册到子模块的 map（**仅 map，不入 vector**）。

### 2.2 API 设计

#### port_manager.hh 新增方法（L180 附近，紧邻 addDownstreamPort）

```cpp
// ========================
// P0 D.1 fix: 镜像外部拥有的端口（非拥有注册, 仅查找用）
// 设计意图: PortManager 是非拥有型注册表（无析构函数, 已存在的事实）,
// mirror 仅扩展查找能力. 调用方保证被 mirror 的指针生命周期 ≥ 此 PortManager.
// 不入 vector 的理由: ChStream 端口由 StreamAdapter 驱动, 不需 PortManager 的
// tick/stats 迭代; 避免 ChStreamInitiatorPort::tick 调用空 output_vcs 崩.
// ========================
void mirrorExistingDownstreamPort(const std::string& label, MasterPort* port) {
    if (!label.empty()) downstream_map[label] = port;  // 不动 downstream_ports vector
}
void mirrorExistingUpstreamPort(const std::string& label, SlavePort* port) {
    if (!label.empty()) upstream_map[label] = port;
}
```

#### 调用点（module_factory.cc L649 后插入 ~5 行）

```cpp
// D.1 fix: 让 SimModule::getInternalOutputPort 能查到 ChStream 端口
// 上下文: 在 for (unsigned i = 0; i < n_ports; i++) 循环内, ch_mod 已 dynamic_cast
if (ch_mod->hasPortManager()) {
    auto& pm = ch_mod->getPortManager();
    std::string suffix = (n_ports > 1)
        ? (std::string("[") + std::to_string(i) + "]")
        : std::string("");
    pm.mirrorExistingDownstreamPort("req_out" + suffix, req_out_vec[i]);
    pm.mirrorExistingUpstreamPort("resp_in" + suffix, resp_in_vec[i]);
    pm.mirrorExistingUpstreamPort("req_in" + suffix, req_in_vec[i]);
    pm.mirrorExistingDownstreamPort("resp_out" + suffix, resp_out_vec[i]);
}
```

### 2.3 关键决策细节

#### 为什么 mirror 不入 vector？

Oracle 工程分析：
- ChStream 模块 tick 路径是 `CPUTLM::tick → StreamAdapter::tick → ChStreamInitiatorPort`（通过 factory 的 `ch_initiator_ports_` 向量驱动）
- `SimModule::tick`（`sim_module.hh:165-169`）调用的是子对象的 `tick()`，而非端口级 tick
- **mirror 到 map 仅服务于 `getInternalOutputPort` 的 `getDownstreamPort(label)` 查找**
- map 不被任何 tick/stats 路径遍历

A-2 风险（Oracle 量化）：
- 破坏 `test_connection_resolution.cc:172` 等 `getDownstreamPorts().size()` 断言
- `getDownstreamStats()` 聚合值被 ChStream 端口的"零 PortStats"拉低

#### 为什么 label 不带模块名前缀？

```cpp
// SimModule::getInternalOutputPort 调用路径:
getInternalOutputPort("cpu0.req_out")
  → parsePortSpec("cpu0.req_out")  → ("cpu0", "req_out")
  → obj = getInternalInstance("cpu0")
  → obj->getPortManager().getDownstreamPort("req_out")  ← 查 "req_out"
```

所以 mirror 时 `downstream_map["req_out"]` 必须命中 — 模块名前缀由 `parsePortSpec` 拆解，mirror 只负责 `port_name` 部分。

#### 为什么生命周期安全？

Oracle 工程分析：
- `PortManager` 类全文（L152-240）**无析构函数** — 已是事实上的非拥有型
- `addDownstreamPort` 创建 `new DownstreamPort<Owner>` 但不 delete — 同语义
- factory 与 sub-object 同生命周期（factory 持有 `instances` map，子模块指针先于 mirror 失效不可能发生）
- **非真实风险**，仅需注释明确："Non-owning; caller must outlive this PortManager"

### 2.4 测试增强

#### 测试 1：升级现有 WARN → REQUIRE

**文件**: `test/test_simmodule_nested.cc:195-206`

```cpp
// P0 D.1 fix: 由 WARN 升级为 REQUIRE (验证 mirror 生效)
auto* req_out_port = cluster->getInternalOutputPort("cpu0.req_out");
REQUIRE(req_out_port != nullptr);  // ← 原 WARN, 升级为硬断言
REQUIRE(req_out_port->getName() == "cpu0.req_out");  // 类型 + 名称双重验证

auto* resp_in_port = cluster->getInternalInputPort("cpu0.resp_in");
REQUIRE(resp_in_port != nullptr);
```

#### 测试 2：新 test_simmodule_d1_chstream_port_visibility.cc

**8 用例**覆盖 4 类 ChStream 模块：

| 用例 | 模块类型 | 验证 |
|------|---------|------|
| D.1.1 | CPUTLM | `getInternalOutputPort("cpu0.req_out")` 非空 + 是 MasterPort |
| D.1.2 | CPUTLM | `getInternalInputPort("cpu0.resp_in")` 非空 + 是 SlavePort |
| D.1.3 | CacheTLM | `getInternalOutputPort("cache0.req_out")` 非空 |
| D.1.4 | MemoryTLM | `getInternalOutputPort("mem0.resp_out")` 非空 |
| D.1.5 | CrossbarTLM (multi-port) | `getInternalOutputPort("xbar.req_out[2]")` 非空（验证 suffix 解析） |
| D.1.6 | DualPortCacheTLM | `getInternalOutputPort("cache0.req_out[0]")` + `[1]` 都非空 |
| D.1.7 | 多 SimModule 嵌套 | 3 层 JSON E2E：`outer.getInternalOutputPort("mid.inner.cpu0.req_out")` 命中 |
| D.1.8 | negative | `getInternalOutputPort("nonexistent")` 仍返回 nullptr（行为不变） |

### 2.5 验收标准

- [ ] `port_manager.hh` 新增 2 个 mirror 方法
- [ ] `module_factory.cc` L649 后插入 mirror 调用（4 行/port × 1/port slot）
- [ ] `cmake --build build -j$(nproc)` 通过
- [ ] `./build/bin/cpptlm_tests 2>&1 | tail -3` → **673/673** pass
- [ ] WARN 升级为 REQUIRE 后无回归
- [ ] 新 test_d1_chstream_port_visibility.cc 8 用例全 pass
- [ ] `bash scripts/test/run_all_tests.sh` → **[SUCCESS]**
- [ ] `./scripts/test/docs_sync_check.sh --strict` 通过

---

## 3. 修复 2：CoherentXBarTLM 骨架 + Write-Through 透传

### 3.1 设计依据：ADR-SOC-01 §2-3 分步走策略

> ✅ **Phase 7.A–7.B**（当前 P0）：GPU 请求走 write-through 直写策略，CacheTLM 不需要 protocol-aware 改造
> ✅ **Phase 7.C**（未来）：CacheTLM 升级为 protocol-aware，引入 6×6 state transition switch 表

**P0 范围**：CoherentXBarTLM 作为类骨架存在，但**不下场做 coherence 决策** — 仅做 snoop broadcast 透传。

### 3.2 类设计

#### 继承结构

```cpp
// include/tlm/coherent_xbar_tlm.hh
class CoherentXBarTLM : public CrossbarTLM {
public:
    explicit CoherentXBarTLM(const std::string& n, EventQueue* eq);
    ~CoherentXBarTLM() override = default;

    // P5 incorporate_parent 钩子: 注册 peer cache 的 req_out
    // snoop broadcast 触发时转发到所有 peer
    void registerPeerCache(const std::string& cache_name, MasterPort* req_out);

    // snoop 入口: 当前阶段 = 透传到所有 peer (不下场做 coherence 决策)
    void snoop_broadcast(Packet* pkt);

private:
    // peer cache 的 req_out 端口列表 (D.1 修复后才能 populate)
    std::vector<std::pair<std::string, MasterPort*>> peer_cache_req_outs_;
};
```

#### 关键方法实现

```cpp
// include/tlm/coherent_xbar_tlm.cc
void CoherentXBarTLM::registerPeerCache(const std::string& cache_name,
                                         MasterPort* req_out) {
    if (!req_out) {
        throw std::runtime_error("CoherentXBarTLM::registerPeerCache: req_out is null");
    }
    peer_cache_req_outs_.emplace_back(cache_name, req_out);
    DPRINTF(MODULE, "[CoherentXBar] Registered peer cache %s (req_out=%p)\n",
            cache_name.c_str(), (void*)req_out);
}

void CoherentXBarTLM::snoop_broadcast(Packet* pkt) {
    if (!pkt) return;
    // P0 阶段: 透传到所有 peer cache (Phase 7.C 将引入 6×6 状态表判断是否真要广播)
    for (auto& [cache_name, port] : peer_cache_req_outs_) {
        // 复制 pkt (避免多个 peer 共享同一指针)
        Packet* copy = new Packet(*pkt);
        if (!port->sendReq(copy)) {
            DPRINTF(MODULE, "[CoherentXBar] Snoop to %s dropped (VC full)\n",
                    cache_name.c_str());
            delete copy;
        } else {
            DPRINTF(MODULE, "[CoherentXBar] Snoop to %s sent\n", cache_name.c_str());
        }
    }
}
```

### 3.3 注册 + 集成

#### REGISTER_MODULE 加到 modules_cluster.hh

```cpp
// include/modules_cluster.hh (P2-T2.4 已重构为参数化宏)
const bool _reg_coherent_xbar = (REGISTER_MODULE(CoherentXBarTLM), true);
```

#### apu_soc_v1.json 顶层替换

```json
// configs/apu_soc_v1.json
{
  "modules": [
    {
      "name": "top_xbar",
      "type": "CoherentXBarTLM",   // ← 从 CrossbarTLM 升级
      "n_ports": 4,
      ...
    },
    ...
  ]
}
```

#### P5 incorporate_parent 接入（D.1 后才能写）

```cpp
// include/tlm/cluster/apu_soc.cc - ApuSoC::incorporate_parent override
void ApuSoC::incorporate_parent(SimModule* parent) override {
    SimModule::incorporate_parent(parent);  // 递归子模块
    // P0 阶段: 注册 peer cache 到 CoherentXBar (依赖 D.1)
    auto* xbar = dynamic_cast<CoherentXBarTLM*>(
        getInternalInstance("top_xbar"));
    if (xbar) {
        for (const auto& [name, mod] : internal_factory->getAllInstances()) {
            auto* cache = dynamic_cast<CacheTLM*>(mod);
            if (cache) {
                auto* req_out = dynamic_cast<MasterPort*>(
                    cache->getPortManager().getDownstreamPort("req_out"));
                if (req_out) xbar->registerPeerCache(name, req_out);
            }
        }
    }
}
```

### 3.4 测试设计

#### test_coherent_xbar_tlm.cc (5-8 用例)

| 用例 | 验证 |
|------|------|
| X.1 | `registerPeerCache` 注册 1 个 peer, `snoop_broadcast` 触发后 peer.req_out 收到 1 包 |
| X.2 | 注册 3 个 peer, 广播 1 个包, 3 个 peer 各收到 1 包 |
| X.3 | 注册 peer 后 null port 抛 `std::runtime_error` |
| X.4 | `snoop_broadcast(nullptr)` 是 no-op, 不崩 |
| X.5 | VC 满时包被丢弃, 不泄漏 (DPRINTF 验证) |
| X.6 | 继承 CrossbarTLM 路由功能: 验证原有 XY 路由仍工作 |
| X.7 | apu_soc_v1.json E2E: top_xbar 是 CoherentXBarTLM 类型, peer cache 注册成功 |
| X.8 | snoop 包是 Packet 副本 (修改 peer 端 pkt 不影响原 pkt) |

### 3.5 验收标准

- [ ] `include/tlm/coherent_xbar_tlm.{hh,cc}` 新增（约 80 LOC）
- [ ] `modules_cluster.hh` 加 `REGISTER_MODULE(CoherentXBarTLM)`
- [ ] `apu_soc_v1.json` 顶层 type 改为 `CoherentXBarTLM`
- [ ] `cmake --build build` 通过
- [ ] `./build/bin/cpptlm_tests "[coherent_xbar]"` → 5-8 用例全 pass
- [ ] 全部 673 用例 + 新增 5-8 用例 = **~681/681** pass
- [ ] `bash scripts/test/run_all_tests.sh` → apu_soc E2E [SUCCESS]

---

## 4. 修复 3：死代码清理（选项 b — 零债务原则）

### 4.1 当前状态

#### cache_tlm.cc:22-36

```cpp
if (!getPortManager().getUpstreamPort("mem_side")) {
    getPortManager().addUpstreamPort(this, {4}, {}, "mem_side");  // ← lazy
}
auto* mem_side = getPortManager().getUpstreamPort("mem_side");
if (!mem_side || !bus_port) {
    throw std::runtime_error("CacheTLM::connectBus: port not found");  // ← 死代码
}
helper_pairs_.emplace_back(std::make_unique<PortPair>(mem_side, bus_port));
```

#### crossbar_tlm.cc:21-37 (connectCPUSideBus) + 41-61 (connectMemSideBus)

类似 lazy + 死 throw 模式，重复 2 次。

### 4.2 设计：删 lazy + 删 throw + 显式报错

#### 改后 cache_tlm.cc

```cpp
void CacheTLM::connectBus(ChStreamModuleBase* bus) {
    if (!bus) {
        throw std::runtime_error("CacheTLM::connectBus: bus is null");
    }

    // P0 修复: 删 lazy registration. port 必须预注册.
    auto* mem_side = getPortManager().getUpstreamPort("mem_side");
    if (!mem_side) {
        throw std::runtime_error(
            "CacheTLM::connectBus: upstream port 'mem_side' not registered. "
            "Ensure JSON config defines 'mem_side' or call addUpstreamPort() "
            "before connectBus().");
    }
    auto* bus_port = bus->getPortManager().getUpstreamPort("cpu_side");
    if (!bus_port) {
        throw std::runtime_error(
            "CacheTLM::connectBus: bus upstream port 'cpu_side' not registered. "
            "Bus must declare 'cpu_side' port in JSON or addUpstreamPort().");
    }

    helper_pairs_.emplace_back(std::make_unique<PortPair>(mem_side, bus_port));
}
```

#### 改后 crossbar_tlm.cc (2 处同理)

`connectCPUSideBus` 和 `connectMemSideBus` 应用同样模式 — port 必须预注册，否则报清晰错误。

### 4.3 测试同步

#### 删 3 个旧测试（PENDING TASKS §8）

- 删任何 `REQUIRE_THROWS("port not found")` 断言 — 死代码路径不存在
- 改：若旧测试是验证"配置缺失时报错"，改用新错误信息断言

#### 加 3 个新测试

| 用例 | 验证 |
|------|------|
| D.3.1 | `CacheTLM::connectBus(bus)` 且 `mem_side` 未注册 → 抛 "mem_side not registered" |
| D.3.2 | `CrossbarTLM::connectCPUSideBus(bus)` 且 `cpu_side` 未注册 → 抛清晰错误 |
| D.3.3 | `CrossbarTLM::connectMemSideBus(bus)` 且 `bus.cpu_side` 未注册 → 抛清晰错误 |

### 4.4 验收标准

- [ ] cache_tlm.cc 改后无 lazy + 无死 throw
- [ ] crossbar_tlm.cc 改后无 lazy + 无死 throw (2 处)
- [ ] 3 个新错误信息测试全 pass
- [ ] 旧 "port not found" REQUIRE_THROWS 测试删除
- [ ] 全测套 pass (~684/684)
- [ ] CMake 警告/错误检查通过

---

## 5. 实施序列与时间表

### 5.1 串行顺序（推荐）

```
Day 1 上午:
  Step 1 (D.1) ~1h
    ↓ 验证: 673/673 → 681/681 (8 新测试)
  Step 2 (CoherentXBarTLM) ~0.5 天
    ↓ 验证: 681/681 → 689/689 (8 新测试)

Day 1 下午 / Day 2:
  Step 3 (死代码清理) ~0.5 天
    ↓ 验证: 689/689 → 692/692 (3 新测试 - 3 旧测试)
```

**总工期**：1.5-2 天
**总 LOC**：~225 (production) + ~120 (tests)
**总测试新增**：8 + 8 + 3 = 19 用例（净增，因死代码测试被替换）

### 5.2 风险登记表

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| D.1 mirror 后某个测试因 PortManager 状态变化失败 | 低 | 中 | 单 commit revert, 验证后转 A-2 |
| CoherentXBarTLM snoop 透传误用为真 coherence | 中 | 高 | API doc + ADR-SOC-01 引用明确"Phase 7.A 透传, 7.C 上 6×6 表" |
| 死代码清理破坏现有 lazy 行为依赖 | 低 | 中 | 全测套验证, 任何依赖 lazy 的测试都应改为预注册 |
| 实施顺序并行引入 merge conflict | 低 | 低 | 串行执行避免; 三步各自分立 commit |
| 测试覆盖率提升触发覆盖率门禁误判 | 低 | 低 | 19 个新测试都是用例数净增, 不影响现有覆盖率 |

### 5.3 回滚策略

每步独立 commit + 可逆：
- D.1: `git revert <commit>` 恢复 mirror 方法 + 调用点
- CoherentXBarTLM: `git revert <commit>` 删类 + 改回 CrossbarTLM 占位
- 死代码清理: `git revert <commit>` 恢复 lazy + throw

**最坏情况**：3 步全部回滚 → 回到 24 commits baseline（673/673 pass），无遗留代码。

---

## 6. 与现有架构的一致性

### 6.1 遵循的约定

- **AGENTS.md 注册宏体系**：CoherentXBarTLM 用 `REGISTER_MODULE(T)` 参数化宏（P2-T2.4 已重构）
- **ChStream 注册宏**：CoherentXBarTLM 继承 CrossbarTLM 后自动有 ChStream 行为（无额外 REGISTER_CHSTREAM）
- **SimModule::simulate_instantiate virtual**：CoherentXBarTLM 不覆盖（继承 CrossbarTLM 行为）
- **零债务原则**：死代码清理 (b) 体现此原则
- **ADR 不可变**：ADR-SOC-01 已确认 Phase 7.A/7.B 分步走，本 spec 仅引用不修改

### 6.2 不违反的边界

- **不触碰 legacy**：`include/modules/legacy/` 不涉及
- **不重命名**：无类/方法重命名
- **不改 JSON schema 顶层**：仅替换 `type` 字段值（`"CrossbarTLM"` → `"CoherentXBarTLM"`），schema 不变
- **不改 CMake 显式源列表**：新增 .cc/.hh 加入 `CORE_SOURCES` 或 `TLM_SOURCES`（按现有模式）
- **不破坏双注册表**：CoherentXBarTLM 继承 CrossbarTLM（ChStreamModuleBase → SimObject），用 `REGISTER_MODULE` 而非 `REGISTER_OBJECT`

### 6.3 文档同步

P0 全套完成后需同步：
- `docs/architecture/01-hybrid-architecture-v2.1.md` — 标注 D.1 修复 + CoherentXBarTLM 章节
- `docs/soc_arch/specs/apu-soc-design.md` §2.2 — 顶层 xbar 类型变更
- `configs/AGENTS.md` — apu_soc_v1.json 顶层 type 说明
- `CHANGELOG.md` — 三个修复条目（v2.2 → v2.3 标记）

---

## 7. 验证矩阵（终态）

| 验证项 | 命令 | 期望 |
|--------|------|------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| C++ 测试 | `./build/bin/cpptlm_tests 2>&1 \| tail -3` | ~692/692 pass |
| CTest | `ctest --test-dir build --output-on-failure -j4` | 100% |
| 新 D.1 测试 | `./build/bin/cpptlm_tests "[d1_port_visibility]"` | 8/8 |
| 新 CoherentXBar 测试 | `./build/bin/cpptlm_tests "[coherent_xbar]"` | 8/8 |
| 新死代码测试 | `./build/bin/cpptlm_tests "[helper_safety]"` | 3/3 (替换 3 个旧) |
| Python 工具链 | `python3 -m pytest test/python/ -v` | 222/222 |
| E2E 全套 | `bash scripts/test/run_all_tests.sh` | [SUCCESS] all configs |
| 格式检查 | `./scripts/build/format.sh --check` | 0 violations |
| 文档同步 | `./scripts/test/docs_sync_check.sh --strict` | 0 missing |

---

## 8. 关键决策日志（Decision Log）

| # | 决策 | 替代方案 | 选定理由 |
|---|------|---------|---------|
| 1 | D.1 用 A-1 (mirror only to map) | A-2 / B / C | Oracle 工程验证：A-1 规避双重驱动 + 零回归；A-2 破坏 size 断言；B 是过早抽象；C 阻塞 P3/P5/snoop |
| 2 | mirror label 不带模块名前缀 | `"cpu0.req_out"` 带前缀 | 与 `parsePortSpec` 二元解析对齐 |
| 3 | mirror 不入 vector | 入 vector (A-2) | ChStream 端口由 StreamAdapter 驱动，非 PortManager tick/stats |
| 4 | CoherentXBarTLM 继承 CrossbarTLM | 独立继承 SimObject + 复用 ChStream | 最大化复用 ~800 行路由/VC 仲裁代码 |
| 5 | CoherentXBarTLM 范围 = 骨架 + 透传 | 直接上 MOESI / 6×6 表 | ADR-SOC-01 §2 明文：Phase 7.A/7.B 简化为 write-through |
| 6 | 死代码清理选 (b) 删 lazy | (a) 保留 lazy 仅删 throw | 零债务原则：配置错误早暴露优于静默 fallback |
| 7 | 实施顺序 = D.1 → CoherentXBar → 死代码（串行） | 并行 | D.1 是其他两项前置；后两项无相互依赖但串行 review 清晰 |
| 8 | 新增 19 测试用例 | 仅升级 WARN→REQUIRE | 净覆盖率提升 + 回归保护 |

---

## 9. 验收签字

| 角色 | 期望 |
|------|------|
| **架构** | 3 修复不破坏任何现有 ADR，遵循 ADR-SOC-01 分步走策略 |
| **测试** | 692/692 全 pass，新增 19 用例无 flake |
| **代码质量** | clang-format 0 违规，零 TODO 残留，零 .disabled 测试新增 |
| **文档** | 4 个核心文档同步（架构 + apu-soc-design + configs/AGENTS.md + CHANGELOG） |
| **性能** | D.1 mirror 5 行代码，开销可忽略（每次模块 instantiate 多 <1μs） |

---

**Spec 版本**: v0.1 (Draft)
**下一步**: 用户 review → writing-plans 写实施计划
**预期开工**: 2026-06-19 ~2026-06-20 (1.5-2 天)