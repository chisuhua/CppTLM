# ADR-INC-01: ApuSoC::incorporate_parent 真实 Late-Binding 语义

> **版本**: 1.0
> **日期**: 2026-06-22
> **状态**: ✅ 已实施 (commit `04399c8`, P1 阶段)
> **影响**: 解锁完整 APU SoC 拓扑 (CPU + GPU 跨域 snoop 广播)
> **前置依赖**: P0 CoherentXBarTLM skeleton (`fb56cc3` D.1 + registerPeerCache API)

---

## 1. Context (背景)

### 1.1 P0 遗留技术债

P0 全套修复 (`fb56cc3` + 5 个 follow-ups) 完成 D.1 PortManager Mirror + CoherentXBarTLM skeleton + 死代码清理。但 `ApuSoC::incorporate_parent` **仍是 passthrough 占位符**（无 wiring 逻辑），导致：

```cpp
// 实际代码 (P0 结束时):
void ApuSoC::incorporate_parent(SimModule* /*parent*/) {
    // TODO: P1 实施 - 收集 peer cache 注册到 xbar
}
```

**调用链断裂**：`ModuleFactory::instantiateAll` 完成 Step 1-8 后，从不触发 `incorporate_parent`，整个 late-binding hook 是**死代码**（`grep -n incorporate_parent src/core/module_factory.cc` → 0 匹配）。

### 1.2 拓扑未解锁的后果

`configs/apu_soc_v1.json` 配置的 ApuSoC 子模块是 `CpuCluster` / `GpuCluster`，不是 `CacheTLM`。spec §3.3 伪代码 `dynamic_cast<CacheTLM*>` 对它们返回 `nullptr`。真实 peer cache 位于深层：

| 路径 | 类型 | 嵌套深度 |
|------|------|:---:|
| `apu_soc.cpu.cache` | `CacheTLM` | 2 |
| `apu_soc.gpu.gpc0.tpc0.compute_grp.cu0.cache` | `CacheTLM` | 5 |
| `apu_soc.gpu.gpc0.tpc0.compute_grp.cu1.cache` | `CacheTLM` | 5 |

只扫一层 → `peer_count()` 永远是 0 → snoop broadcast 不工作 → CoherenceDomain 集成（Phase 7.C）无法启动。

### 1.3 命名漂移风险

- spec §3.3 用 `getInternalInstance("top_xbar")`
- `configs/apu_soc_v1.json:12` 实际是 `"xbar"`
- 硬编码任一都会在另一处断裂

---

## 2. Decision (决策)

P1 实施采用 **Oracle 综合推荐** 的 1A+2A+3A 组合方案 + 双层幂等 + 软失败 + 命名可配置。

### 2.1 决策表

| 决策 | 选择 | 备选 | 理由 |
|------|------|------|------|
| **调用模型** | **1A: ModuleFactory Step 9 自动调用** | 2A (子 SimModule 主动调) / 2B (instantiate 时调) | grep 证实当前 0 调用点；不自动 = 功能永不触发 |
| **Wiring 算法** | **2A: 父端全树递归** | 2B (WiringEvent 事件总线) / 2C (中间层) | spec §3.3 伪代码只扫一层会漏深层 cache；2B/2C 过度抽象 |
| **GpuCluster 范围** | **3A: 不重写 incorporate_parent** | 3B (GPU 专用 API) / 3C (4 级手动展平) | 父端全树递归已覆盖 GPU 深层 cache；3B 需新 API，3C 重复劳动 |
| **幂等性** | **双层：peer_caches_wired_ 早退 + registerPeerCache 按名去重** | 无幂等 | 防止 ModuleFactory 未来重构二次调用导致重复注册 |
| **错误处理** | **软失败：DPRINTF WARN + skip** | 硬抛异常 | xbar 缺失是合法拓扑（单元测试场景）；只有 `registerPeerCache(nullptr)` 硬抛 |
| **命名** | **`coherent_xbar_name_` 可配置（默认 `"xbar"`）** | 硬编码 `"xbar"` 或 `"top_xbar"` | 避免 spec 假设与 JSON 实际命名漂移 |

### 2.2 实施概览

**新代码**（按执行顺序）：

1. **`ApuSoC::incorporate_parent` 重写** (`include/tlm/cluster/apu_soc.hh`)
   - 加入 `peer_caches_wired_` 标志（早退幂等）
   - 加入 `coherent_xbar_name_` 成员（默认 `"xbar"`）
   - 加入 `collectAndRegisterPeerCaches` 私有 helper 声明

2. **`ApuSoC::collectAndRegisterPeerCaches` 实现** (`src/tlm/cluster/apu_soc.cc`)
   - 递归遍历整棵 SimModule 子树
   - 对每个 `dynamic_cast<CacheTLM*>` 命中：取 `req_out` → `xbar->registerPeerCache(full_name, req_out)`
   - 对每个 `dynamic_cast<SimModule*>` 命中：递归下钻
   - 软失败：`hasPortManager()` false / `req_out` null / `dynamic_cast` 失败 → DPRINTF WARN + skip

3. **`CoherentXBarTLM::registerPeerCache` 按名去重** (`include/tlm/coherent_xbar_tlm.hh`)
   - 内部维护 `std::unordered_set<std::string> registered_names_`
   - 重复注册同 name → 早退（DPRINTF DEBUG）

4. **`ModuleFactory::instantiateAll` Step 9** (`src/core/module_factory.cc`)
   - 在 Step 7b (PortPairs) 之后追加 Step 9
   - 遍历顶层 `module_instances`（非嵌套于其他 SimModule）
   - 对每个顶层 `SimModule*` 调用 `sm->incorporate_parent(nullptr)`
   - parent 传 nullptr：根 SimModule 没有"父"对象；基类默认实现不消费 parent 参数

5. **`ApuSoC::set_config` 支持 `coherent_xbar_name` 参数** (`src/tlm/cluster/apu_soc.cc`)
   - 解析 JSON `params.coherent_xbar_name`，存入 `coherent_xbar_name_`
   - 缺省 → `"xbar"`

**测试**（5 new TEST_CASEs in `test/test_apu_soc_incorporate_parent.cc`）：

| # | 名称 | 验证 |
|---|------|------|
| T1 | `incorporate_parent no xbar` | xbar 缺失时软失败（DPRINTF WARN），不抛 |
| T2 | `incorporate_parent one peer` | `peer_count() == 1` |
| T3 | `incorporate_parent deep GPU cache` | 5 级嵌套 cache 全部注册（peer_count ≥ 2） |
| T4 | `incorporate_parent idempotent` | 二次调用 `peer_count()` 不变 |
| T5 | `incorporate_parent snoop broadcast E2E` | `xbar.snoop_broadcast(pkt)` → 所有 peer 收到 |

---

## 3. Consequences (后果)

### 3.1 解锁的能力

- ✅ **完整 APU SoC 拓扑**：CpuCluster + GpuCluster + CoherentXBarTLM 跨域 snoop 广播
- ✅ **P1 主线推进**：4 阶段全部完成 (`04399c8`)
- ✅ **P1.5 GPU cu_template 完整传播** (`e8c2a97`)：peer_count 3 → 16+
- ✅ **Phase 7.C CoherentXBarTLM 6×6 state table 改造** (后续 F4) 拥有 wiring 基础

### 3.2 已知技术债

- ⚠️ **`coherent_xbar_name_` 单 xbar 限制**：仅支持一个 CoherentXBarTLM。多 xbar 实例需 Phase 7.C+ 扩展（per P1 spec §7 风险表）
- ⚠️ **`registerPeerCache` 按名去重**：当前只对 CacheTLM 生效，其他 peer 类型（如 TCC）需 Phase 7.D+ 添加
- ⚠️ **`hasPortManager()` 软失败**：CacheTLM 缺失 PortManager 配置时跳过（DPRINTF WARN）。后续 Phase 7.C 强制要求
- ⚠️ **Step 9 在 ModuleFactory 末尾**：未来若有 Step 10+ 应保持 Step 9 在 PortPairs 之后、StatsManager 之前

### 3.3 Phase 7+ 扩展点

- **Phase 7.C (F4)**：CoherenceDomain 集成时，可在 Step 9 末尾追加 CoherenceDomain 自动注册
- **Phase 7.D (F13)**：TCC Bridge 加 `registerPeerCache` 重载（支持 GPU-side peer）
- **Phase 7.E (F14)**：多 CU + NoC 场景，`coherent_xbar_name_` 可改为 `xbar_instances: [...]` 数组
- **未来 GpuCluster::incorporate_parent override**：spec §4.5.2 提议保留。本 spec 选择 3A 不实现，因父端全树递归已覆盖 GPU 深层 cache

---

## 4. References (参考)

### 4.1 设计来源

- **`docs-archived/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md`** — Draft v0.1 本 ADR 的设计基础（含 Oracle 综合分析、伪代码、5 阶段实施计划）
- **`docs-archived/superpowers/plans/2026-06-19-simmodule-complex-hierarchies.md`** — §4.5.2 P5 incorporate_parent 设计原始提议

### 4.2 关联 ADR

- **`ADR-X.13-stub-multi-extension.md`** — 多 TLM 扩展 stub 标记（snoop broadcast 依赖 multi-extension reset）
- **`ADR-X.14-coherence-domains-stub.md`** — `coherence_domains` 字段 stub（Phase 7.C CoherentXBarTLM state table 基础）

### 4.3 实施追溯

- **P0 commit `fb56cc3`** — D.1 PortManager Mirror + CoherentXBarTLM skeleton + registerPeerCache API
- **P1 commit `04399c8`** — ApuSoC::incorporate_parent 真实 late-binding wiring（本 ADR 实施）
- **P1.5 commit `e8c2a97`** — GPU cu_template 完整传播（peer_count 3 → 16+）

### 4.4 关联任务

- `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` §F3 (本 ADR) + §F4 (Phase 7.C 6×6 state table)

---

## 5. Status Update

无（首次签发即已实施完成）。
