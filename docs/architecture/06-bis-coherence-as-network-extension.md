# CppTLM Inter-Module Network §06-bis — Coherence 作为网络层扩展的演进讨论 (v0.3 Revision)

> **文档状态**: 🟡 v0.3 Revision, 与 [`06-inter-module-network.md`](06-inter-module-network.md) v0.3 同期修订
> **目标存档**: `docs/architecture/06-bis-coherence-as-network-extension.md`
> **作者**: CppTLM build agent (architect mode)

---

## ⚠️ v0.3 修订说明(reviewer 请读)

v0.3 对 v0.2 做了 **9 大类事实校正 + 6 个跨仓 sync 契约**,基于 Oracle + Metis 3 轮评审:

1. **§3.2 8 端口装配实证** — `module_factory.cc:705-731` 3 分支只能 cover 单/多(N 端口同 Bundle)/双(硬编码 4 Bundle)路径,**CoherenceBridge 8 端口不可行**
2. **§3.3 3 候选装配路径 + 选 C(programmatic bridge)** — 与 `VramControllerTLM::set_fabric_source(GpuInternalFabric*)` 模式对位
3. **§3.5 P3.5 选项 C(JSON 端口路径 + 工厂)与 §3.4 "永久放弃" 兼容修订**
4. **§3.4 与 active Draft `cpptlm-dgpu-soc-physical-ip-port-boundary` / `control-hub` 方向冲突声明豁免条款** (per oracle Q-α 报告)
5. **§4.2 C++ 端 `coherence_domains` 实际是 stub + DomainRegistry 生命周期管理 + 真实 API 对齐** (per oracle Q-γ)
6. **§5.1-5.4 SnoopBundle 类型改写** (`std::string` → `ch_uint<8>`) + **struct 改名 SnoopRespBundle** + **跨仓 ArchForge STATUS UPDATE 同步契约** (per oracle Q-β 报告)
7. **§6.2 `SoC.add_coherence_domain` passthrough + 示例 `add_module` 位置修正** (per oracle Q-ε)
8. **§7 P2 拆 P2.0 + P2.1, P3 拆 P3 + P3.5** (per oracle Q-δ) + **KPI 口径修正"串行激活, 并发 ≤ 1"** (per 06 §11.5 修订)
9. **§8.2 风险 R12 新增 — 与 active Draft 冲突 + 豁免条款**

---

## 目录(v0.3 修订标记)

- §0 阅读引导
- §1 Coherence 蓝图回顾(沿用 v0.1, 修订 ArchForge 状态)
- §2 CppTLM-side 实施切分
- **§3 CoherenceBridge ChStream 接入契约 — v0.3 BLOCKER 修订**
  - §3.1 现状(沿用 v0.1)
  - **§3.2 8 端口装配实证 — v0.3 NEW** (oracle + Metis 共识 blocker)
  - **§3.3 3 候选装配路径 — v0.3 NEW**
  - **§3.4 选 C: programmatic bridge + 与 active Draft 豁免 — v0.3 NEW**
  - **§3.5 P3.5 选项 C 与 §3.4 兼容修订 — v0.3 NEW**
- §4 CoherenceDomain Spec 实施 — **v0.3 完全改写**(承认 stub, 修订 API)
- §5 SnoopBundle 字段定义 — **v0.3 重写**(类型改写 + struct 改名 + 跨仓 sync)
- §6 Python 拓扑 Coherence 字段 — **v0.3 修订**(SoC passthrough + 示例修正)
- §7 推进路径 — **v0.3 修订 KPI 拆分**
- §8 KPI 与风险 — **v0.3 修订 R12**
- §9 评审问题清单 — **v0.3 修订(基于 Q-α 到 Q-ε + Metis)**
- §10 决策请求 — **v0.3 修订 7 项**
- §11 关联文档
- §12 修订记录

---

## §0 阅读引导

- 想快速理解 Coherence 范围 → §1
- 想理解 C++ 端实施切分 → §2 + §3
- 想理解 CoherenceDomain Spec → §4
- 想理解 SnoopProbe Bundle 字段 → §5
- 想理解 Python 端 Coherence → §6
- 想理解推进时序 → §7 + §8
- 想理解 reviewer 问题 → §9

---

## §1 Coherence 蓝图回顾(仅引用 ArchForge, 不重复)

### 1.1 ArchForge 状态(修订:从 v0.1 "🔵 Implemented" 改 "🟡 接口草案")

**v0.3 修订**: 06 v0.3 §10.1 表明 ArchForge 9 份 coherence 文档**只有 markdown 草案, 没有 C++ 实现**。

| ArchForge 文档 | 真实状态 |
|----------------|---------|
| `coherence-protocol.md` (CoherenceProtocol 接口 + SnoopProbe Bundle) | 🟡 markdown 草案, **`include/core/coherence_protocol.hh` 不存在** |
| `coherence-bridge.md` (CoherenceBridge 8 端口) | 🟡 markdown 草案, CoherenceBridge C++ 实现不存在 |
| `coherence-domain.md` | 🟡 markdown 草案, **`include/core/coherence_domain.hh` 已存在**(Phase 4.2 早期 stub) |
| `coherent_xbar.md` (CoherentXBarTLM) | 🟡 CoherentXBarTLM **TDD stub 已存在** (per `coherent_xbar_tlm.hh` 文件头) |
| `cache-protocol.md` (Cache 6×6) | 🟡 markdown 草案 |

**06 + 06-bis v0.3 含义**: 跨仓引用 ArchForge 仅作**设计参考**, **实际 C++ 实施全部从零开始**(包括 CoherenceProtocol、CoherenceBridge、SnoopFilter 等)。

### 1.2 v1.0 MVP / v1.1 完整版范围矩阵(沿用 v0.1, 仅做 ArchForge 状态修订)

(per `09-coherence-protocol.md` §9.2, 但 ArchForge 自身 v1.0 仍处 🟡 草案):

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| MOESI/GPU 6×6 状态机 | ✅ | ✅ |
| CoherenceDomain | ✅ | ✅ + 优化 |
| CoherenceBridge | ✅ 基础 | ✅ 完整 + IOMMU |
| SnoopFilter | ✅ 基础 | ✅ 完整 + directory-based |
| CoherentXBar | ✅ | ✅ + 性能优化 |
| Phase 7.D (Cache Protocol 升级) | ❌ 推迟 | ✅ 完整 |

### 1.3 ADR-SOC-01 分步走策略(沿用 v0.1)

(per ArchForge ADR-SOC-01):
- Phase 7.A-7.B 黑盒 GPU: write-through 直写
- Phase 7.C Coherence 集成: CacheTLM 升级为 protocol-aware
- Phase 7.D 完整 Cache Protocol 升级: 推迟

### 1.4 ADR-SOC-09 D4 双 vendor coherence(沿用 v0.1)

(per ArchForge ADR-SOC-09):
- NVIDIA 路径: 通常不需要 CPU↔GPU coherence(per USRI)
- AMD 路径: AMD Infinity Fabric 需要 CPU↔GPU coherence(per ROCm)
- 共享 MOESI/GPU 6×6: 跨 vendor 复用同一状态机, 仅 Domain ID 区分

### 1.5 永不复制 Gem5 SLICC DSL(沿用 v0.1)

(per ADR-SOC-01 §2 决策 + 06 §12.2 DSL 拒绝诱惑):
- SLICC 是 Gem5 特定语言, 5000+ 行不可读
- CppTLM 用 C++ `switch` 表驱动状态转换
- 避免维护负担

---

## §2 CppTLM-side 实施切分(沿用 v0.1, 微调范围)

### 2.1 实施范围边界

**06-bis v0.3 在 CppTLM 仓内实施的范围**(修订自 v0.1):
| 范围 | 来源 | 实施 |
|------|------|------|
| **`CoherenceProtocol` C++ 实现** | ArchForge `microarchitecture/coherence-protocol.md` 完整接口草案 | **必须由 CppTLM 实施**(ArchForge 是设计仓, 不存 C++ 源, 跨仓镜像到 `src/core/`) |
| **`CoherenceDomain` C++ 实施** | 仓内 stub 已存(per `include/core/coherence_domain.hh`), v0.3 §4 升级为真实消费 | **必须由 CppTLM 实施** |
| **`CoherenceBridge` C++ 实施** | ArchForge `microarchitecture/coherence-bridge.md` 完整接口 | **必须由 CppTLM 实施**(走 programmatic bridge 路径 per §3.4) |
| **`CoherentXBar` C++ 实施** | ArchForge §8 + `coherent_xbar_tlm.hh` TDD stub | **必须由 CppTLM 实施** |
| **`SnoopFilter` C++ 实施** | ArchForge §7 | **必须由 CppTLM 实施** |
| **Python `cpptlm.library.coherence_bridge()` 工厂** | **新增**(per 06 §7.5 缺失) | **P3.5 实施**(per §3.5 选项 C 修订) |
| **`layer.py::add_coherence_domain` Spec** | 已存(per `layer.py:43-49`) | v0.3 §4.1 加 `_all_coherence_domains()` 递归 |
| **JSON 配置 Schema `coherence_domains` 字段** | 06 §8.1 已有 schema | 06-bis 实施时落地 + v0.3 §4.1 修订 |

### 2.2 与 ArchForge 的同步契约(修订自 v0.1)

**v0.3 修订**:
1. **设计源头**: ArchForge `coherence-protocol.md` / `coherence-bridge.md` 是**唯一权威**, CppTLM 仓内 C++ 源不允许**编辑既有 ArchForge 接口签名**(违反契约), 只允许: `新加 default 实现` / `新加 free function` / `新加 overload`
2. **C++ 接口绑定**: ArchForge 接口变更 → 必须先在 ArchForge 仓内合并 → 然后 CppTLM 这边 `git submodule update` 同步 → 然后实施
3. **CppTLM-only 扩展**: 若 CppTLM 实施需要的细节 ArchForge 没规定(如类型改写 `std::string` → `ch_uint<8>` 协议 ID), 在 06-bis v0.3 §5 明确标注为 **CppTLM 类型改写**, **必须**同步 ArchForge markdown 加 `## STATUS UPDATE 2026-10` 段
4. **差异 detection**: 季度 CppTLM 跨仓审计 — 比较 ArchForge 接口签名 vs CppTLM 实施, 确保兼容

### 2.3 命名空间一致性(修订自 v0.1)

| 元素 | ArchForge | CppTLM |
|------|----------|--------|
| 命名空间 | `tlm::` (per `coherence-protocol.md:109`) | **`bundles::`** (v0.3 修订 — 沿 CppTLM 既有约定, ArchForge markdown 同步) |
| 头文件目录 | (ArchForge 不存头) | `include/bundles/coherence_bundles_tlm.hh` (per 06-bis 范围) |
| 源文件目录 | (ArchForge 不存源) | `src/core/coherence_*.cc` (per AGENTS.md core/ 源) |
| 测试目录 | (ArchForge 不存测试) | `test/test_coherence_*.cc` |

**ArchForge markdown 同步契约**: 06-bis P2.0 实施前, ArchForge `coherence-protocol.md` §3 接口段加 `## STATUS UPDATE 2026-10` 备注命名空间 `tlm::` → CppTLM 镜像到 `bundles::`、struct 命名 `SnoopResp` → `SnoopRespBundle`、类型 `std::string` → `ch_uint<8>` 协议 ID。

---

## §3 CoherenceBridge ChStream 接入契约 — v0.3 BLOCKER 修订

### §3.1 现状(沿用 v0.1)

ArchForge `coherence-bridge.md` §3 接口草案 — `CoherenceBridge : public ChStreamModuleBase`, 8 端口 design (4 + 4), 4 类 Bundle (CacheReq/Resp + SnoopProbe/SnoopResp)。

### §3.2 8 端口装配实证 — v0.3 NEW

per Metis 评审 + 实地 grep 实证 `module_factory.cc:617-731`:

```cpp
// src/core/module_factory.cc:705-731 (Step 7a 装配分支)
if (is_dual) {  // dual_port_types_ set has this type
    auto* dual = static_cast<cpptlm::DualPortStreamAdapter<
        ChStreamModuleBase, bundles::CacheReqBundle, bundles::CacheRespBundle,
        bundles::NoCFlitBundle, bundles::NoCFlitBundle>*>(adapter);
    // ^^ 4 个 BundleTemplate 参数硬编码 cast — UB on different types
    dual->bind_pe_ports(req_out_vec[0], ..., req_in_vec[0]);
    dual->bind_net_ports(req_out_vec[1], ..., req_in_vec[1]);
} else if (is_multi) {
    // multi 分支用 MultiPortStreamAdapter<M, Rq, Rp, N>
    // 仅支持 1 对 (Rq, Rp) — 不支持 2 对 + 2 snoop 通道
} else {
    // single
}
```

**硬约束实证**(per oracle Q-α 实地调研 + 跨仓 grep):

| 维度 | 实证 | 含义 |
|------|------|------|
| `ChStreamAdapterFactory` 注册模板 | `chstream_adapter_factory.hh:33-110` | **4 个**模板方法 (oracle 修订: 多了 `registerBidirectionalPortAdapter`): `registerAdapter<M, ReqB, RespB>`(单端口) / `registerMultiPortAdapter<M, ReqB, RespB, N>`(N 端口同 Bundle 对) / `registerDualPortAdapter<M, PE_B, PE_B, Net_B, Net_B>`(硬编码 4 BundleTemplate) / `registerBidirectionalPortAdapter<M, BundleT, N>`(RouterTLM 用) |
| `port_count_[type]` | `chstream_adapter_factory.hh:114` | 仅 `MultiPort` 与 `Dual` 注册时赋值; Dual 固定 = 2 |
| `is_multi` / `is_dual` | `module_factory.cc:644-645` | 二选一, 无双标 |
| `num_ports()` | `chstream_module.hh:54` virtual | 生产代码零调用(per critic NIT 1, test 中大量调用但不影响生产路径) |

**结论**:
- CoherenceBridge 8 端口 = 2 域 × 4 端口(2 CacheReq/Resp + 2 SnoopProbe/SnoopResp) — **4 类 Bundle 4 个端口** — **现有 Step 7a 装配路径不支撑**
- `register_chstream_module<...>` 是**虚构宏**(v0.1 §3.4 引用), 真实宏是 `REGISTER_CHSTREAM`(3 参数 tuple, 与模板绑多 Bundle 不匹配)
- `module_factory.cc:707-709` dual 分支硬编码 cast **对 NICTLM 已经是形式 UB** — 实测能跑是因为 dual adapter 内部全是 MasterPort* 裸指针, **布局与模板参数无关**; `bind_pe_ports/bind_net_ports` 不解引用 `module_`

### §3.3 3 候选装配路径 — v0.3 NEW

候选 A: **Step 7a 扩展 + 新增 Adapter 类型** — 注册 `CoherenceBridgeStreamAdapter<M, A_r, A_p, B_r, B_p, ...>`(但 BundleTemplate 数 ≥4 触发与现有 Dual 硬编码 cast 相同的脆弱性)

候选 B: **SimModule 嵌套 JSON** — 走 `simulate_instantiate(top_cfg)` 递归机制, 与 §6.3 用法 B 对位。但 CoherenceBridge 作为 `ApuSoC` 内嵌模块, **内层 Step 7a 同样只有 3 分支**, 需与分解(候选 D)组合才可行

候选 D(v0.3 NEW, oracle Q-α 建议): **2 个 dual 半桥分解** — ArchForge coherence-bridge.md §2.2 的 8 端口恰可拆为 **CacheHalfBridge**(`dual<CacheReq,CacheResp,CacheReq,CacheResp>`, A 侧 req_in/resp_out ↔ B 侧 req_out/resp_in) 与 **SnoopHalfBridge**(`dual<SnoopProbe,SnoopResp,SnoopProbe,SnoopResp>`), **今天就能走 JSON 接线**, 零 ModuleFactory 改动。代价: `snoop_inflight_` 跨通道状态内聚需要共享 context 对象 — 引回 programmatic 链接

候选 C(沿用 v0.2): **Programmatic bridge**(继承 D-AXI vram-controller + GpuInternalFabric 模式)— `CoherenceBridge` 不走 JSON 端口, 通过 `DGpuBoard::bind_memory_backings()` 或 `topology_parser` 解析时显式调用 `coh_bridge->set_domain_a(domain_a)` + 端口自动创建时走 `set_*_target` 模式

### §3.4 选 C: programmatic bridge + 与 active Draft 豁免条款 — v0.3 NEW

**v0.3 选定候选 C(programmatic bridge)** (per oracle Q-α 实证), 与 `VramControllerTLM::set_fabric_source(GpuInternalFabric*)` + `GpuInternalFabric::set_vram_target(VramControllerTLM*)` 模式对位(per `include/tlm/vram_controller_tlm.hh:125` + `include/tlm/cluster/gpu_internal_fabric.hh:65` 实证)。

**CoherenceBridge 8 端口设计修订**:
```cpp
// 选定路径: ChStreamModuleBase + 4 programmatic 端口 setter
namespace tlm {
class CoherenceBridge : public ChStreamModuleBase {
public:
    // 8 ChStreamInputStreamAdapter / OutputStreamAdapter 通过 programmatic bridge
    // 而非 JSON 端口索引
    void bind_domain_a(
        InputStreamAdapter<CacheReqBundle>* req_in,
        OutputStreamAdapter<CacheRespBundle>* resp_out,
        OutputStreamAdapter<SnoopProbe>* snoop_out,
        InputStreamAdapter<SnoopResp>* snoop_in);
    void bind_domain_b(...);

    // set_*_target 等价于 setter 注入 (per port_contract_asserts.hh:42-45)
    void set_domain_a(CoherenceDomain*);
    void set_domain_b(CoherenceDomain*);

    // JSON 配置 Spec: 仅参数化 (协议对 + 地址映射函数 + latency)
    // 端口内部连接由 programmatic bridge 在 DGpuBoard::init_xxx() 阶段创建
};
}

// JSON 配置 Schema (P2 实施):
{
  "name": "coh_bridge",
  "type": "CoherenceBridge",
  "params": {
    "protocol_a": "MOESI_AMD",        // v0.3 §5 协议名真源表
    "protocol_b": "MOESI_AMD",
    "bridge_latency": 200,
    "enable_snoop_forwarding": true
    // 端口不在 JSON, 通过 programmatic bridge 装配
  }
}
```

**ModuleFactory Step 7a 处理**:
- CoherenceBridge **走单端口 StreamAdapter 路径**(`is_multi=false`, `is_dual=false`)**只为 `set_stream_adapter(adapter)` + dummy 端口**
- 真正的 8 端口由 programmatic bridge 创建
- **ModuleFactory 不知道 8 端口** — 通过 `getInternalInstance("coh_bridge")` 找 `CoherenceBridge*` 即可

**与 active Draft 豁免条款**(per oracle Q-α 关键盲点):

**事实**: `openspec/changes/cpptlm-dgpu-soc-physical-ip-port-boundary` 与 `cpptlm-dgpu-soc-control-hub` 两个 Proposed Draft 明确目标是把所有 programmatic bridge 全部 JSON 化、移除至 0 处(per `physical-ip-port-boundary/tasks.md:135` "取消 set_vram_target/set_fabric_source programmatic bridge" + `control-hub/proposal.md:366` "移除 6 处 programmatic bridge → 0 处, 全部 JSON 化")。

**冲突**: 06-bis P2.1 此时**新增**一套 programmatic bridge, 是在反向游泳。

**06-bis v0.3 声明**:
- **CoherenceBridge programmatic bridge 是永久豁免**(类似 GMMU fallback 被保留的处理)
- 理由: CoherenceBridge 8 端口(2 域 × 4 端口) 的 snoop_inflight_ 状态内聚需要共享 context 对象, 强行 JSON 化需要把 1 个模块拆成 2 个, 反而引回 programmatic 链接
- 后续: P3.5 评估 candidate D 分解成熟后, 转为 JSON 路径 (per §3.5 选项 C 修订)
- **依赖**: 必须在这 3 份提案之间互引并写明: CoherenceBridge 的 programmatic 装配是永久豁免还是临时态 (per oracle Q-α 横向建议 1)

**P2.1 任务表**(修订自 v0.1):
1. `include/core/coherence_bridge.hh` / `src/core/coherence_bridge.cc` (~350 行)
2. **ApuSoC 路径** (v0.3 修订): `apu_soc.cc:84-103 incorporate_parent` 装配代码, 与 DGpuBoard 路径并行
3. `coherence_bridge()` programmatic 装配(per `dgpu_board_shell.cc:735-878` 8 处 getInternalInstance 实证)
4. **`topology_parser.cc:107-118` stub 替换**
5. **`module_factory.cc:165-182` 真实调用** (CoherenceDomain 实例化)
6. 测试 `[coherence]` 标签

### §3.5 P3.5 选项 C 与 §3.4 兼容修订 — v0.3 NEW

**v0.3 修订 P3.5** (per oracle Q-δ 报告 + §3.4 programmatic 路径选定):

**选项 C (v0.3 选定)**: P3.5 = "JSON 端口路径打通 + Python 工厂"
- 实施 `registerCoherenceAdapter<4 BundleTemplate>`(扩 Step 7a 第 4 类 Adapter)
- 实施 Python `coherence_bridge()` 工厂(走 JSON 端口路径)
- 修订 §3.4 "永久放弃 Step 7a 通用 Adapter" → "条件放弃: 同协议桥走 programmatic, 跨协议桥走 JSON 端口路径(P3.5)"

**理由**:
- programmatic 路径只 cover **同协议桥**(MoesiAmd ↔ MoesiAmd)
- **跨协议桥**(MOESI_AMD ↔ MESI_GPU) 需要 JSON 端口路径, 因为 2 域不同协议需要分别显式 config
- §3.4 "永久放弃" 措辞过于激进, 实际是 "条件放弃 — 同协议不需, 跨协议才需"

**v0.3 §3.4 修订原文**:

> ❌ **v0.2 永久放弃**:
> 永久放弃: 全 Step 7a 通用 Adapter(违反既有类型擦除硬编码与 06 §7 "不引入通用化 Dual")
>
> ✅ **v0.3 条件放弃**:
> 同协议桥(MoesiAmd ↔ MoesiAmd)走 programmatic 路径, 不需要 JSON 端口路径。跨协议桥(MOESI_AMD ↔ MESI_GPU)走 JSON 端口路径(P3.5 实施), 通过扩 Step 7a registerCoherenceAdapter<4 BundleTemplate> 实现。

### §3.6 与 06 §6.3 用法一致性

用法 C(programmatic bridge) + 06 §6.3 用法 A(flat + name_prefix 隔离 NoC) 互补:

| 用法 | 适用模块类型 |
|------|--------------|
| A — flat + name_prefix | RouterTLM / CrossbarTLM / LinkTLM(NICTLM 桥接的 client) |
| B — SimModule 嵌套 | ApuSoC + 跨域 cluster 嵌套 |
| **C — programmatic bridge** | CoherenceBridge / VramControllerTLM / GpuInternalFabric / SocInternalNoc |

---

## §4 CoherenceDomain Spec 实施 — v0.3 完全改写

### §4.1 Python 端(已落地的现状, v0.3 修订)

**v0.1 误**: §4.2 标 "`to_dict()` 增量"任务。**v0.3 实证**: `cpptlm/topo/emitter.py:74-77 + 196-204` 已经做:

```python
# cpptlm/topo/emitter.py (v0.3 引用现行实现, 不重发明)
def emit(self, root: TopoLayer) -> Dict[str, Any]:
    # ...
    if root.coherence_domains:
        result["coherence_domains"] = [
            self._serialize_coherence(d) for d in root.coherence_domains
        ]
    # ...

def _serialize_coherence(self, d: CoherenceDomainSpec) -> Dict[str, Any]:
    out: Dict[str, Any] = {"name": d.name}
    if d.protocol:
        out["protocol"] = d.protocol
    if d.members:
        out["members"] = list(d.members)
    if d.bridges:
        out["bridges"] = dict(d.bridges)
    return out
```

→ **Python 端完全已落地**, **v0.3 撤销 `to_dict()` 增量任务**

**v0.3 修订**:
1. `_serialize_coherence` 当前**仅处理 root.coherence_domains, 不递归 sublayers**。v0.3 P2 加 `_all_coherence_domains()` 递归方法(per `layer.py:105-110 _all_modules` 模式) + `emitter.py` 递归调用
2. **emitter 是唯一 canonical 出口** — `to_dict()` 路径已确认死路径(per oracle Q-ε 报告), 文档明示 "emitter 是唯一 canonical 序列化", 防后人踩坑

**次级隐患**(per oracle Q-ε 报告):
- 递归提升后**重名/成员解析策略未定义** — 两个 sublayer 各定义同名 domain 时, hoist 到顶层后冲突; 同名**报错而非静默合并**
- `to_dict()`(layer.py:237-241) 不输出 `bridges` 字段, emitter 输出 — **两条序列化路径字段集不同**, 既然 to_dict 是死路径, 文档明示 canonical 即可

### §4.2 C++ 端消费(沿用 v0.1 但承认是 STUB)— v0.3 修订

**v0.1 误**: §4.3 写 "消费路径已存在"。**v0.3 实证**(per oracle Q-γ 报告 + 实地 grep):
- `topology_parser.cc:110-115` `DPRINTF(PARSER, "[STUB] coherence_domains field accepted but not consumed; see ADR-X.14 ...")` ✓
- `module_factory.cc:167-182` Step 0.5 调用 + `(void)hierarchy_root`(:174) ✓
- ADR-X.14 存在且明确记录 "validate_domain_boundary not called from any code path"

**v0.3 修订 P2 任务表**:
1. **删 stub**: `topology_parser.cc:107-118` 替换为真实 JSON 解析
2. **真实消费**: `module_factory.cc:165-182` 调用 + 创建 CoherenceDomain 实例
3. **API 真实对齐**(per oracle Q-γ 报告): `coherence_domain->add_member(module_name)` → **`set_members(const std::vector<std::string>&)` 批量式** (per `coherence_domain.hh:31`)
4. **DomainRegistry 生命周期管理**(per oracle Q-γ 关键风险): 静态 `unordered_map`(per `coherence_domain.hh:46-49`); 多 config 串行加载 / destroy→init 重入会累积脏状态 (项目前科: DGpuBoard destroy→init SIGSEGV, AGENTS.md Step 0 P0 fix); P2 必须把 `DomainRegistry::clear()` 挂进 ModuleFactory 析构/teardown
5. **既有 8+4 测试的回归锁定**: `test_coherence_domain.cc` 8 个 TEST_CASE + `test_domain_boundary.cc` 4 个全绿 (per oracle Q-γ 实证)
6. **bridge_name 对齐**: `coherence_domain.hh:39` 真实签名是 `void register_bridge(const std::string& target_domain, const std::string& bridge_name)` (两个 string, 不是 CoherenceDomain* target + CoherenceBridge* bridge)

**CoherenceDomain 现有 C++ 接口**(v0.3 修订自 v0.1 假设):

```cpp
// 真实接口 per include/core/coherence_domain.hh (P2 升级 stub 时参考)
class CoherenceDomain {
    CoherenceDomain(const std::string& name, EventQueue* eq);
    bool set_members(const std::vector<std::string>& members);  // 批量式, 返回 bool
    void register_bridge(const std::string& target_domain, const std::string& bridge_name);
    // 真实 API 不是 add_member 而是 set_members, 不是 register_bridge(Domain*, Bridge*) 而是 (string, string)
};
```

### §4.3 CoherenceDomain 跨仓同步契约(沿用 §2.2)

ArchForge `coherence-domain.md` 提供**设计意图** + API 草案(markdown); CppTLM `include/core/coherence_domain.hh` 提供**真实 C++ 实现** (v0.3 P2 升级).

**契约**:
- ArchForge `coherence-domain.md` 接口签名需 P2 实施前同步加 `## STATUS UPDATE 2026-10` 段(标注真实 API 与 ArchForge draft 差异)
- 06-bis v0.3 跨仓同步责任: 06-bis P2 实施前由 owner 在 ArchForge 仓加 STATUS UPDATE
- **.cc 端修改**: 06-bis 实施时, 先读 ArchForge draft, 定稿 interface; 再写 .cc

---

## §5 SnoopBundle 字段定义 — v0.3 重写

### §5.1 ArchForge 实地(per `microarchitecture/coherence-protocol.md:177-184`)

```cpp
// ArchForge coherence-protocol.md:177-184 实地 6 字段
struct SnoopProbe {
    uint64_t addr;                  // 字段 1
    uint64_t transaction_id;        // 字段 2
    SnoopType type;                 // 字段 3 — 强类型 enum, 不是 uint8
    std::string source_protocol;    // 字段 4 — std::string (违反 frozen POD 约束)
    std::string target_protocol;    // 字段 5 — std::string
    uint32_t requestor_cache_id;    // 字段 6
};
```

**3 个 v0.1 错误 + 2 个 v0.3 修订**:
1. **ArchForge 是 6 字段**(含 `target_protocol`), v0.1 标"5→6"是字段计数错
2. v0.1 自写 `msg_type` 为 `ch_uint<8>`, ArchForge 用强类型 `SnoopType` enum
3. v0.1 把 `source_protocol/target_protocol` 写成 `ch_uint<8>` 与 ArchForge `std::string` 不一致
4. **v0.3 修订**: struct 名 `SnoopResp` → `SnoopRespBundle` (per oracle Q-β, 与 ArchForge 一致)
5. **v0.3 修订**: 命名空间 `bundles::` 沿 CppTLM 既有约定(ArchForge `tlm::` markdown 同步 STATUS UPDATE)

### §5.2 v0.3 CppTLM SnoopBundle 字段定义(类型改写路径)

**关键约束**: 既有 frozen Bundle 必须满足 `is_trivially_copyable` + `bundle_base` 空基类无虚函数 — **`std::string` 不能作为 Bundle 字段**(per oracle Q-β 实证 + 编译期拦截)

**v0.3 字段定义**(类型改写 + struct 命名 + 命名空间, per oracle Q-β 报告):

```cpp
// include/bundles/coherence_bundles_tlm.hh (P2 实施)
namespace bundles {

struct SnoopProbe : public bundle_base {
    // 业务地址 + 事务 ID (ArchForge 1:1)
    ch_uint<64> addr;
    ch_uint<64> transaction_id;

    // SnoopType 4 类(per ArchForge §3.5 snoop_type)
    ch_uint<8>  type;  // 0=INVALIDATE, 1=DOWNGRADE_TO_S, 2=READ_SHARED, 3=READ_EXCLUSIVE
    // 静态常量
    static constexpr uint8_t INVALIDATE = 0;
    static constexpr uint8_t DOWNGRADE_TO_S = 1;
    static constexpr uint8_t READ_SHARED = 2;
    static constexpr uint8_t READ_EXCLUSIVE = 3;

    // 协议 ID (v0.3 修订: 类型改写 std::string → ch_uint<8>)
    ch_uint<8>  source_protocol_id;   // CppTLM-only: 协议枚举 ID
    ch_uint<8>  target_protocol_id;   // CppTLM-only: 协议枚举 ID

    // Sharer 更新(per ArchForge §6 requestor_cache_id)
    ch_uint<32> requestor_cache_id;
};
BUNDLE_FIELD_WIDTH(bundles::SnoopProbe, addr, 64);
// ... 编译期锁
}
```

**v0.3 协议名 → ID 真源表**(per oracle Q-β 关键盲点 4 处拼写不一致):

| ID | 协议名(string, JSON) | C++ 类 | 备注 |
|----|---------------------|---------|------|
| 0 | `MOESI_AMD` | `MoesiAmdProtocol` | ArchForge canonical |
| 1 | `MESI_GPU` | `MesiGpuProtocol` | ArchForge canonical |
| 2 | `MESI_THREE_LEVEL` | `MesiThreeLevelProtocol` | ArchForge canonical |
| 3 | `MESI_AMD` | (P3 候选) | per `coherence_protocol.md:108` |
| 255 | `WRITE_THROUGH_SIMPLIFIED` | (per ADR-X.14 配置) | Phase 7.A-7.B 简化 |

**既存 C++ 端** `include/core/coherence_domain.hh:14-17` 已有 `enum Protocol { MESI, MOESI }` 2 值 — **v0.3 P2.0 必须决定**: 替换它还是并存(并存 = 两套协议标识, 正是 06 要消灭的坏味道)

**v0.3 跨仓 sync 契约**:
- P2.0 实施前, ArchForge `coherence-protocol.md` §3 接口段加 `## STATUS UPDATE 2026-10` 备注:
  - 命名空间 `tlm::` → CppTLM 镜像到 `bundles::`
  - 类型 `std::string source_protocol/target_protocol` → CppTLM 镜像到 `ch_uint<8> source_protocol_id/target_protocol_id`
  - struct `SnoopResp` → CppTLM 镜像到 `SnoopRespBundle`
  - 协议名 → ID 真源表 (§5.2 表)
- 协议名 canonical 化(MOESI_AMD_6_STATE → MOESI_AMD, NONE → WRITE_THROUGH_SIMPLIFIED)

### §5.3 SnoopRespBundle 字段定义(v0.3 修订命名)

per ArchForge §3 SnoopRespBundle + 06 v0.3 §5 + v0.3 命名修订:

```cpp
struct SnoopRespBundle : public bundle_base {  // v0.3 改名
    ch_uint<64> addr;
    ch_uint<64> transaction_id;
    ch_uint<8>  resp;  // 4 类: ACK/NACK/DATA/DATA_AND_ACK
    ch_uint<64> data;  // 可选 (如 DATA 类型)
    ch_uint<8>  source_protocol_id;  // v0.3 类型改写
    static constexpr uint8_t ACK = 0;
    static constexpr uint8_t NACK = 1;
    static constexpr uint8_t DATA = 2;
    static constexpr uint8_t DATA_AND_ACK = 3;
};
BUNDLE_FIELD_WIDTH(bundles::SnoopRespBundle, addr, 64);
```

### §5.4 CoherenceMsg Bundle(扩展)

per ArchForge §3 CoherenceMsg:

```cpp
struct CoherenceMsg : public bundle_base {
    ch_uint<64> addr;
    ch_uint<64> transaction_id;
    ch_uint<8>  msg_type;  // protocol-specific (协议定)
    ch_uint<64> data;
    ch_uint<8>  source_protocol_id;   // v0.3 增量
    ch_uint<8>  target_protocol_id;   // v0.3 增量 (ArchForge 需 STATUS UPDATE 同步)
};
BUNDLE_FIELD_WIDTH(bundles::CoherenceMsg, addr, 64);
```

**跨仓同步**: `CoherenceMsg` 增 2 字段(`source_protocol_id`/`target_protocol_id`)是 CppTLM-only 扩展, ArchForge 需 STATUS UPDATE 同步声明。

### §5.5 命名空间 + 命名 + 类型 一致性

| 元素 | ArchForge 草案 | CppTLM v0.3 镜像 | 同步契约 |
|------|---------------|------------------|---------|
| 命名空间 | `tlm::` (per `coherence-protocol.md:109`) | **`bundles::`** (沿 CppTLM 既有) | ArchForge markdown STATUS UPDATE 备注 |
| `SnoopProbe` struct 名 | `SnoopProbe` | `SnoopProbe` (一致) | — |
| `SnoopRespBundle` struct 名 | `SnoopRespBundle` | `SnoopRespBundle` (v0.3 修订) | — |
| `std::string protocol` | 强类型 string | `ch_uint<8> protocol_id` + 真源表 | ArchForge markdown STATUS UPDATE 备注 |
| 协议名 → ID 真源 | (ArchForge 无) | CppTLM §5.2 表 (单点定义) | ArchForge markdown STATUS UPDATE 引用 |

### §5.6 与 06 §5 二分类协调

SnoopProbe / SnoopRespBundle / CoherenceMsg 是 **网络 Bundle**(per 06 §3.2 判定: 网络层关心 + 项目级), 定义在 `bundles::` 命名空间 → 与 06 §3 一致。

---

## §6 Python 拓扑 Coherence 字段 — v0.3 修订

### §6.1 `cpptlm.library.coherence_bridge()` 工厂 — P3.5 实施 (per §3.5 选项 C)

**v0.3 修订**: 06 v0.3 §3.4 选定路径后, **P2 阶段不实施 Python 工厂**(programmatic 路径不需要); **P3.5 实施**(走 JSON 端口路径)

**P2 阶段 Python 任务**(减载自 v0.1, per oracle Q-ε):
1. `emitter.py` 加 `_all_coherence_domains()` 递归(per §4.1)
2. `SoC.add_coherence_domain` passthrough 加 1 行(per §6.2 + oracle Q-ε)
3. `library/interconnect.py` 不动 — CoherenceBridge 端口由 C++ programmatic 装配

**P3.5 Python 工厂草案**:

```python
# P3.5 实施时(cpptlm/library/interconnect.py)
def coherence_bridge(
    protocol_a: str = "MOESI_AMD",   # 走 §5.2 真源表
    protocol_b: str = "MOESI_AMD",
    bridge_latency: int = 200,
    enable_snoop_forwarding: bool = True,
    name: str = "coh_bridge",
) -> TopoLayer:
    """P3.5 走 JSON 端口路径时启用(per §3.5 选项 C)."""
    if name in RESERVED:
        raise ValueError(f"reserved module name '{name}'")
    l = TopoLayer(name=f"coh_bridge_wrap_{name}")
    l.metadata["bridge_name"] = name
    l.add_module(
        name,
        "CoherenceBridge",
        protocol_a=protocol_a,
        protocol_b=protocol_b,
        bridge_latency=bridge_latency,
        enable_snoop_forwarding=enable_snoop_forwarding,
    )
    return l
```

### §6.2 End-to-End 用法 — v0.3 修订

```python
# examples/dgpu_soc_with_coherence.py (P2 时新增)

from cpptlm.library import SoC, mesh_cluster
from cpptlm.library.standard import cpu_l1_cluster

soc = SoC("dgpu_soc_with_coherence")

# v0.3.1 修订: CoherenceBridge module 节点走 §3.4 programmatic 路径
# 注: soc.add_module 必须在 add_cluster 之前(per soc.py:35-58 _last_target 语义)
# 否则 coh_bridge 落到最后一个 add_cluster 子层而非 root
soc.add_module("coh_bridge", "CoherenceBridge",
               protocol_a="MOESI_AMD",    # 走 §5.2 真源表
               protocol_b="MOESI_AMD",
               bridge_latency=200)

# 注: SoC.add_coherence_domain() 是 v0.3 新增 passthrough (per §4.1)
soc.add_coherence_domain(
    name="cpu_gpu_domain",
    protocol="MOESI",  # 走 §5.2 真源表
    members=[
        "cluster0_cpu0", "cluster0_cpu1", "cluster0_cpu2", "cluster0_cpu3",
        "cluster1_cpu0", "cluster1_cpu1", "cluster1_cpu2", "cluster1_cpu3",
    ],
    bridges={"gpu_domain": "coh_bridge"},
)

soc.add_cluster(cpu_l1_cluster(0, n_cores=4)).tag("cpu_cluster")
soc.add_cluster(cpu_l1_cluster(1, n_cores=4, cluster_id="gpu")).tag("gpu_cluster")

# v0.3.1 修订: CoherenceBridge module 节点走 §3.4 programmatic 路径
# 在 P2 阶段, 端口由 DGpuBoard::bind_memory_backings() / ApuSoC::incorporate_parent 装配
# Python 端不写 connections -> coh_bridge.* 端口
# (v0.3.1 add_module 移到示例开头, 见 line 561-564)

soc.save("configs/dgpu_soc_with_coherence.json")
```

### §6.3 实施步骤 — v0.3 修订

| 步骤 | ArchForge | CppTLM v0.3 |
|------|-----------|-------------|
| 1 | `CoherenceProtocol` 接口 | P2.0 实施 + 类型改写(`std::string` → `ch_uint<8>`) + 协议 ID 真源表 |
| 2 | `CoherenceDomain` 接口 | 已存在(`include/core/coherence_domain.hh`), P2.1 集成从 stub 升级到真实解析 + DomainRegistry 生命周期 |
| 3 | `CoherenceBridge` 接口 | P2.1 实施, **走 programmatic bridge 路径**(per §3.4) |
| 4 | — | `topology_parser.cc:107-118` stub 替换 + API 真实对齐 |
| 5 | — | `module_factory.cc:165-182` 真实调用 + `set_members` 真实 API |
| 6 | — | `emitter.py` 加 `_all_coherence_domains()` 递归 + dedup 报错策略 + canonical 声明 |
| 7 | — | `SoC.add_coherence_domain` passthrough + `apu_soc.cc:84-103 incorporate_parent` 装配代码 |
| 8 | 测试 | `test/test_coherence_*.cc` series |

### §6.4 ArchForge STATUS UPDATE 同步契约

P2.0 实施前, ArchForge `coherence-protocol.md` + `coherence-bridge.md` 同步加 `## STATUS UPDATE 2026-10` 段, 备注:
1. 命名空间镜像 `tlm::` → `bundles::`
2. 类型改写 `std::string source_protocol/target_protocol` → `ch_uint<8> source_protocol_id/target_protocol_id`
3. struct 命名 `SnoopResp` → `SnoopRespBundle`
4. 协议名 → ID 真源表(06-bis §5.2 表)
5. `CoherenceMsg` 增 2 字段声明

---

## §7 推进路径 — v0.3 修订 (KPI 拆分 + §3.5 选项 C)

### §7.1 P0(沿用 v0.1)

[沿用 v0.1 — 3d doc-only, 0 KPI]

### §7.2 P1(沿用 06 v0.3 §11.2 — shared KPI 路径)

[沿用 06 v0.3 — 10-15d, 1 KPI slot]

### §7.3 P2(拆分 P2.0 + P2.1, per oracle Q-δ + 06 v0.3 §11.5 串行激活)

| P2.0 (30-45d) | P2.1 (30-45d) |
|----------------|----------------|
| CoherenceProtocol C++ 实现 + 类型改写 | CoherenceBridge C++ 实施(走 programmatic bridge 路径, per §3.4) |
| MoesiAmdProtocol 6 状态 × 6 事件 switch 表 | DGpuBoard::bind_memory_backings() + ApuSoC::incorporate_parent 装配 |
| Bundle 字段定义(SnoopProbe/RespBundle/CoherenceMsg) | topology_parser.cpp:107-118 stub 替换 + API 真实对齐 |
| `include/ext/coherence_protocol.hh` + `include/bundles/coherence_bundles_tlm.hh` | `include/core/coherence_bridge.hh` + `src/core/coherence_bridge.cc` |
| 测试 6×6 = 36 转换 | 测试 programmatic bridge + JSON 解析 + DomainRegistry 生命周期 → 落 `test/test_coherence_bridge.cc`(per §11.5 路径), 模式参照 `test_realip_pin_contract.cc:57-74` programmatic bridge round-trip |
| **跨仓 ArchForge STATUS UPDATE 同步契约**(per §5.5 + §6.4) | 跨仓 STATUS UPDATE 同步契约 + ApuSoC 路径实现 |

**KPI**: 2 slots (串行激活, 并发 ≤ 1 per 06 v0.3 §11.5 KPI 口径修订)

### §7.4 P3 — SnoopFilter + MesiGpu + translate_snoop

[沿用 v0.1 — 30-45d, 1 slot]

### §7.5 P3.5 — JSON 端口路径打通 + Python 工厂 (per §3.5 选项 C)

[沿用 v0.1 — 30-45d, 1 slot]
- 扩 Step 7a `registerCoherenceAdapter<4 BundleTemplate>` (候选 C 实施)
- Python `coherence_bridge()` 工厂实施(走 JSON 端口路径)
- 修订 06 v0.3 §3.4 "永久放弃" → "条件放弃"

### §7.6 P4 — CHI 完整协议

[沿用 v0.1 — 30-45d, 1 slot]

### §7.7 P5 — IOMMU + 跨 vendor

[沿用 v0.1 — 60-90d, 1-2 slot]

### §7.8 v0.3 总 KPI 调整 — 串行激活, 并发 ≤ 1

| 阶段 | v0.1 估时 | v0.3 估时 | KPI 串行激活 | 并发占用 |
|------|---------|---------|--------------|---------|
| P0 | 3d | 3d | 0 (doc-only) | 0 |
| P1 | 10-15d | 10-15d | 1 | ≤ 1 |
| P2.0(协议接口) | 30-45d (合并) | 30-45d | 1 | ≤ 1 |
| P2.1(桥与集成) | (合并) | 30-45d | 1 | ≤ 1 |
| P3(多协议) | 30-45d | 30-45d | 1 | ≤ 1 |
| P3.5(JSON 端口) | (合并) | 30-45d | 1 | ≤ 1 |
| P4(CHI) | 30-45d | 30-45d | 1 | ≤ 1 |
| P5(IOMMU) | 60-90d | 60-90d | 1-2 | ≤ 1 |
| **累计串行** | **6-7 slots** | **6-7 slots** | (非并发!) | (≤ 1) |

**v0.3 关键修订**: 06-bis 总 KPI **与 v0.1 同 (6-7 slots)**, 但**口径修订**:
- "slots" 是 **串行激活次数** 不是 **同时占用并发数**
- 任何 1 时刻最多占 1 slot, 不存在"6-7 slot 同时被占"的情况 (per 06 v0.3 §11.5 + oracle Q-δ)
- 实际排队时间由 slot 释放顺序决定, 不是总 slot 数

---

## §8 KPI 与风险

### §8.1 KPI 现状(沿用 06 v0.3 §11.5)

| 状态 | 计数 | 备注 |
|------|------|------|
| 当前 OpenSpec active | **5** | control-hub / physical-ip-port-boundary / msix-generator-ip / p2-integration-unblock / pcie-tlp-entry-layer |
| Proposed(计入 KPI) | **2** | p2-integration-unblock + pcie-tlp-entry-layer |
| Proposed Draft(不计 KPI) | 3 | control-hub / physical-ip-port-boundary / msix-generator-ip |
| 上限 | 3 (Proposed 并发) | per AGENTS.md |
| **当前 Proposed 并发占用** | **2** | **有 1 个空 slot** |

**真约束**: 排队治理(slot 释放顺序) 不是 slot 总数 (per 06 v0.3 §11.5 + oracle Q-δ)

### §8.2 风险清单 — v0.3 修订 R12

R1-R11 沿用 06 v0.3 §12.1 表。

**R12 — v0.3 NEW** (per oracle Q-α 关键盲点):

| 风险 | 等级 | 缓解 |
|------|------|------|
| **R12** — **06-bis P2.1 programmatic bridge 与 active Draft (`cpptlm-dgpu-soc-physical-ip-port-boundary` / `cpptlm-dgpu-soc-control-hub`) 方向冲突** — 后两者明确目标 "移除所有 programmatic bridge → 0 处, 全部 JSON 化" (per `physical-ip-port-boundary/tasks.md:135` + `control-hub/proposal.md:366`) | 🟡 中 | **06-bis v0.3 §3.4 声明永久豁免条款**: CoherenceBridge programmatic bridge 是永久豁免(类似 GMMU fallback 保留), 理由: 8 端口(2 域 × 4 端口)snoop_inflight_ 状态内聚需共享 context, 强行 JSON 化需拆 2 个模块, 反而引回 programmatic 链接。P3.5 评估 candidate D 分解成熟后, 转为 JSON 路径(per §3.5 选项 C) |

R3 v0.3 修订: "ArchForge 只有 markdown 草案 + CppTLM 实施需类型改写" — P2.0 时同步提交 ArchForge markdown [STATUS UPDATE 2026-10] 备注类型改写原因(per §5.5 + §6.4)

R7 v0.3 修订: "CoherenceBridge 8 端口 + Step 7a 装配不可行已落地" — P2.1 走 programmatic bridge 路径(候选 C), R7 风险等级降到 🟢 低

---

## §9 评审问题清单 — v0.3 修订(基于 Q-α 到 Q-ε + Metis + Critic)

### §9.1 (沿用 v0.1 §9.1)

### §9.2 (沿用 v0.1 §9.2)

### §9.3 (沿用 v0.1 §9.3)

### §9.4 v0.3 新增评审问题(基于 Q-α 到 Q-ε + Critic BLOCKERS)

1. §3.4 programmatic bridge 路径放弃 Step 7a 装配 → 与 06 §6.4 "不通用化 Dual" 原则一致, 但**有没有 CoherenceBridge 走 JSON 端口的真实需求**? (per oracle §9.4 问题 1, 如只为 MoesiAmd↔MoesiAmd 透传, P2-P3 都用 programmatic 够用, P3.5 不必实施) — **§3.5 选项 C 修订选择 P3.5 实施**
2. §5.2 `std::string` → `ch_uint<8>` 类型改写需 ArchForge 同步 — 这是**真 ADR 同步**还是**只在 CppTLM 实施**即可? (per §5.5 同步契约要求 STATUS UPDATE 同步)
3. §5.5 `bundles::` 命名空间 vs ArchForge `tlm::` — 是否需 ArchForge markdown 加 [STATUS UPDATE 命名空间镜像] (per §5.5 已要求)
4. §4.1 emitter.py 不递归子层的 coherence_domains — 加 `_all_coherence_domains()` 后是否破坏既有 emit 行为? (per oracle Q-ε 报告, P2 加递归同时加 dedup 报错策略)
5. §7.3 P2.0/P2.1 拆 KPI slot 是否过度细分? 06 P2 的 "网络 Bundle 演进" 如果加进 P2.1, 可以共享 1 slot (per oracle Q-δ 评, 暂定不共享)
6. §3.4 programmatic bridge 与 2 active Draft 方向冲突的永久豁免条款 — 你接受吗? (per oracle Q-α 关键盲点)

### §9.5 v0.3 必评审问题(Metis + Critic BLOCKERS 全部覆盖)

- **8 端口装配问题** (§3.2-3.4) — 已 3 候选 + 选 1 + 选 C 永久豁免 — **你认可候选 C 永久豁免吗?**
- **SnoopProbe 字段算术错** (§5.1) — 已实证 ArchForge 6 字段 — **v0.3 §5.2 类型改写路径你认可吗?**
- **`std::string` 字段非 POD** (§5.2) — v0.3 §5.2 改写为 `ch_uint<8>` 协议 ID + 真源表 — **你认可吗?**
- **struct 命名 SnoopResp → SnoopRespBundle** (§5.3) — **你认可吗?**
- **C++ coherence_domains 是 stub** (§4.2) — 已承认 + P2.1 替换 — **你接受 P2.0/P2.1 时替换 stub 吗?**
- **Python `to_dict()` 增量是 no-op** (§4.1) — v0.3 撤销任务 + 加 `_all_coherence_domains()` 递归 + canonical 声明 — **你接受吗?**
- **`SoC.add_coherence_domain` passthrough** (§6.2) — v0.3 加 1 行 — **你接受吗?**
- **§11.2 P1 `addScalar` API** (06 v0.3 修订) — v0.3 §11.2 改用真 API + `+=` 累加写入 — **你接受吗?**

---

## §10 决策请求 — v0.3 修订 7 项

### §10.1 需要批准的 5 + 2 个关键决策

1-5. **沿用 v0.1**

6. **CoherenceBridge 8 端口装配路径候选 C (programmatic bridge) 是否接受 + 永久豁免条款** (per oracle Q-α + §3.4)?
7. **SnoopProbe 类型改写 + struct 命名 SnoopRespBundle + 协议 ID 真源表** 是否接受 (per oracle Q-β + §5)?

### §10.2 后续行动

**如果批准**:
- 06 v0.3 + 06-bis v0.3 同步落地 (本节已完成 v0.3 起草)
- 起草 OpenSpec `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/proposal.md` + `cpptlm-coherence-extension/proposal.md` (K-D doc-only 路径)
- supersede Status Update 加到 PNN v0.1 + 05-doc v0.1 + ADR-DGPU-13 + ADR-DGPU-14
- P0 阶段启动 (1 周)
- **跨仓 ArchForge STATUS UPDATE 同步契约**: P2.0 实施前, ArchForge 仓内 9 份 coherence 文档加 `## STATUS UPDATE 2026-10` 段(per §5.5 + §6.4 同步契约)

**如果不批准**:
- 收集具体反对意见(哪一条不通过)
- 修订本方案 v0.3 → v0.4 → ...
- 重新评审

---

## §11 关联文档 — v0.3 修订

### §11.1 主从关系

- **主**: [`docs/architecture/06-inter-module-network.md`](06-inter-module-network.md) v0.3 (本文件同期, 精炼引用 06-bis §15.1)
- **次**: 本文件(实施蓝图详细化)

### §11.2 跨仓 ArchForge(权威设计, 06-bis 0 改, 仅引用)

- [`main/ArchForge/docs/architecture/09-coherence-protocol.md`](../../../main/ArchForge/docs/architecture/09-coherence-protocol.md) — v1.0-draft L7 Coherence 子系统架构
- [`main/ArchForge/docs/microarchitecture/coherence-protocol.md`](../../../main/ArchForge/docs/microarchitecture/coherence-protocol.md) — CoherenceProtocol C++ 接口 + MoesiAmdProtocol/MesiGpuProtocol 实现草案
- [`main/ArchForge/docs/microarchitecture/coherence-bridge.md`](../../../main/ArchForge/docs/microarchitecture/coherence-bridge.md) — CoherenceBridge ChStream 8 端口接口
- [`main/ArchForge/docs/microarchitecture/coherence-domain.md`](../../../main/ArchForge/docs/microarchitecture/coherence-domain.md) — CoherenceDomain per-cache 状态机 + snoop callback
- [`main/ArchForge/docs/microarchitecture/coherent_xbar.md`](../../../main/ArchForge/docs/microarchitecture/coherent_xbar.md) — CoherentXBar 实现
- [`main/ArchForge/docs/microarchitecture/snoop_filter.md`](../../../main/ArchForge/docs/microarchitecture/snoop_filter.md) — SnoopFilter directory-based
- [`main/ArchForge/docs/microarchitecture/cache-protocol.md`](../../../main/ArchForge/docs/microarchitecture/cache-protocol.md) — Cache 6×6 单 cache 行
- [`main/ArchForge/adr/ADR-SOC-01-coherence-protocol-strategy.md`](../../../main/ArchForge/docs/adr/ADR-SOC-01-coherence-protocol-strategy.md) — 分步走策略
- [`main/ArchForge/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../../main/ArchForge/docs/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) — v1.0 双 vendor 跨域 coherence

### §11.3 相关 CppTLM 文档

- [`docs/architecture/01-hybrid-architecture-v2.1.md`](01-hybrid-architecture-v2.1.md) v2.1.10 §1.1 "通信框架层 vs 总线层"
- [`docs/designs/dgpu-soc/communication-network.md`](../designs/dgpu-soc/communication-network.md) v1.0 §1 6 层全貌
- [`include/tlm/cluster/AGENTS.md`](../../include/tlm/cluster/AGENTS.md) "SimModule vs ChStreamModule 端口能力对照表"
- [`cpptlm/AGENTS.md`](../../cpptlm/AGENTS.md) — Python 拓扑工具链
- [`docs/designs/dgpu-soc/architecture.md`](../designs/dgpu-soc/architecture.md) — dGPU 应用三视角(包含 coherence 完整设计)

### §11.4 相关 OpenSpec change

- `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/` — 已归档(提供 06-bis doc-only 豁免先例)
- `openspec/changes/cpptlm-dgpu-soc-control-hub/` Proposed Draft(06-bis §3.4 永久豁免条款, programmatic bridge 永久保留 CoherenceBridge 路径)
- `openspec/changes/cpptlm-dgpu-soc-physical-ip-port-boundary/` Proposed Draft(同上)
- `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/` — 06 主 change(待起草, K-D 路径)
- `openspec/changes/2026-10-16-cpptlm-coherence-extension/` — 06-bis change(待起草, K-D 路径)

### §11.5 相关测试

实施时新增:
- `test/test_coherence_protocol.cc` — 单元测试覆盖 6×6 = 36 个转换
- `test/test_coherence_bridge.cc` — 端到端跨域测试
- `test/test_coherence_domain.cc` — 既有 + P2.1 stub 替换回归锁定
- `test/test_coherent_xbar.cc` — coherence 互连
- `test/test_snoop_filter.cc` — directory-based
- `test/test_translate_snoop.cc` — 协议转换
- `test/test_network_protocol_ext.cc` — 06 P1 增量(per 06 v0.3 §11.2)
- `test/test_module_factory_bridge_id_assignment.cc` — 06 P1 增量

### §11.6 调研参考(06 v0.3 + 06-bis v0.3 不重发明)

- [`docs/research/gem5-ruby-protocol-bridge-survey.md`](../../research/gem5-ruby-protocol-bridge-survey.md) — Gem5 mainline CHI 落地
- [`docs/research/gem5-slicc-chi-survey.md`](../../research/gem5-slicc-chi-survey.md) — SLICC → C++ switch 借鉴
- [`docs/research/gem5-coherence-protocols-survey.md`](../../research/gem5-coherence-protocols-survey.md) — 11 类 coherence 协议族对比
- [`docs/research/gem5-coherence-network-evolution.md`](../../research/gem5-coherence-network-evolution.md) — 20+ 年演进史
- [`docs/research/gem5-messagebuffer-network-interface.md`](../../research/gem5-messagebuffer-network-interface.md)
- [`docs/research/gem5-garnet-network-survey.md`](../../research/gem5-garnet-network-survey.md)

---

## §12 修订记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-10-16 | v0.1 (replaced) | build agent (architect) | 初版 |
| 2026-10-17 | v0.2 (replaced) | build agent (architect) | 修 v0.1 BLOCKER, 引入新问题 |
| 2026-10-18 | **v0.3 (本版)** | build agent (architect) | **3 轮评审 22 项修订** + **8 端口装配实证** + **programmatic 路径选定** + **类型改写 + struct 改名 + 协议 ID 真源** + **KPI 口径修正** + **跨仓 sync 契约** |

### §12.1 v0.3 修订明细 (vs v0.2)

| # | 来源 | v0.2 bug | v0.3 修订位置 |
|---|------|--------|--------------|
| 1 | Metis 8-端口不可行 | §3.4 候选 A 不可行但未列 D | §3.2 实证 + §3.3 4 候选 + §3.4 选 C + 与 active Draft 永久豁免条款 |
| 2 | Metis SnoopProbe 算术 | 5→6 字段计数错 | §5.1 ArchForge 实地 6 字段 |
| 3 | Metis std::string 非 POD | 未触动 | §5.2 类型改写 + 真源表 |
| 4 | Metis module_factory stub | 错为 "已落地" | §4.2 承认 + P2.0/P2.1 替换 + 真实 API 对齐 |
| 5 | Metis to_dict 增量 no-op | 错为 "to_dict 增量" | §4.1 撤销 + 加 `_all_coherence_domains()` 递归 |
| 6 | Metis 跨仓 contract | 不成立 | §2.2 + §5.5 + §6.4 加 STATUS UPDATE 同步契约 |
| 7 | Oracle Q-α programmatic 冲突 | 未声明 | §3.4 永久豁免 + §8.2 R12 修订 |
| 8 | Oracle Q-β SnoopRespBundle 命名 | SnoopResp struct | §5.3 改名 |
| 9 | Oracle Q-β 协议 ID 真源 | 4 处拼写 | §5.2 真源表 |
| 10 | Oracle Q-γ DomainRegistry 生命周期 | 未管理 | §4.2 P2.1 任务加 |
| 11 | Oracle Q-δ KPI 口径 | 累计 6-7 slots 表述错 | §7.8 串行激活, 并发 ≤ 1 |
| 12 | Oracle Q-δ P3.5 vs §3.4 矛盾 | 自相矛盾 | §3.5 选项 C 修订 |
| 13 | Oracle Q-ε SoC passthrough | 私有成员访问 | §6.2 加 1 行 passthrough |
| 14 | Oracle Q-ε 递归提升重名 | 策略未定义 | §4.1 dedup 报错 |
| 15 | Oracle Q-ε canonical 声明 | 两条路径字段集不同 | §4.1 emitter canonical 声明 |
| 16 | Oracle Q-ε 示例 bug | add_module 落子层 (v0.3 声称修但未全修 — 仅 06 §6.3 修, 06 §7.4 + §15.7 + 06-bis §6.2 漏) | v0.3.1 修订, 三处统一前移 add_module |
| 17 | critic B-α §2.1 L4/L5 | 编号 +1 位移 | 06 v0.3 §2.1 重画 0-8 |
| 18 | critic B-β add_scalar | API 错 | 06 v0.3 §11.2 改 addScalar + `+=` 累加写入 |
| 19 | critic B-γ 48 处 v1 占位 | 全文重写 | 06 v0.3 全文重写, 0 占位 |
| 20 | critic NIT TopOrchestrator | typo | 06 v0.3 §7.1 改 TopoOrchestrator |
| 21 | critic NIT n_ports=2 | CrossbarTLM 无效 | 06 v0.3 §6.3 删 n_ports |
| 22 | critic NIT 协议名 canonical | 3 处拼写 | 06-bis v0.3 §5.2 真源表 + ArchForge STATUS UPDATE |

### §12.2 v0.3 仍未修订(待你定夺)

| # | 项 | 我的判断 | 你决定? |
|---|----|----------|---------|
| 1 | ArchForge 跨仓 markdown 镜像路径在仓内不可 grep 验证 | 降级为"ArchForge 设计参考, 实现在 CppTLM 仓" | — |
| 2 | `cpptlm_config` deprecated 处置由既有 OpenSpec change 管 | 06 不主动修 | — |
| 3 | KPI slot 实际可释放时间表依赖 archive 决策 | 06 P0 doc-only 立即可启动, P1+ 等 slot | — |

### §12.3 v0.3 评审通过后下一步

1. 起草 `openspec/changes/2026-10-16-cpptlm-inter-module-network-mvp/proposal.md` + `2026-10-16-cpptlm-coherence-extension/proposal.md` (K-D doc-only 路径)
2. P0 任务启动(1 周)
3. 同步 supersede Status Update 段到 PNN / 05-doc / ADR-DGPU-13 / ADR-DGPU-14
4. 实施 06 v0.3 §11.2 P1 任务(等 KPI slot 释放)
5. **跨仓 ArchForge STATUS UPDATE 同步**: P2.0 实施前, ArchForge 仓内 9 份 coherence 文档加 `## STATUS UPDATE 2026-10` 段(per §5.5 + §6.4 同步契约)

---

**版本**: v0.3 Revision
**最后更新**: 2026-10-18
**批准状态**: 🟡 待逐项评审 (Oracle + Metis + Critic 3 轮已给修订判断, 等你最终 B 路径)
**维护者**: CppTLM build agent (architect mode)
**下次更新**: 你最终 B 路径反馈后 → v1.0 PASS
