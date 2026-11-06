# CEP 逐协议端到端测试覆盖梳理（2026-11-04）

**目的**：回答「14 个 change 归档后，6 个协议族各自的验证到底做到了哪一层」。
**方法**：逐文件实测（`TEST_CASE` 名称 + 装配/驱动/抵达断言计数），不按文件名推测。

---

## 1. 覆盖层次定义（本文使用的口径）

| 层 | 含义 | 可机器判定的标志 |
|---|---|---|
| **L1 契约** | constexpr 判定、表逐格比对、文本扫描、schema 校验 | 无实例、无事务 |
| **L2 声明** | `protocol_instances.json` 的实例声明字段齐备且与 traits 一致 | 读 JSON |
| **L3 装配** | `ModuleFactory::instantiateAll(cfg)` + JSON 配置实例化 | 有 factory / config |
| **L4-direct 事务级** | `submit → 派发 → 引擎内记账/状态机`（**进程内派发**,无 wire、无 `tick()`、无对端） | 有 `dispatch_cohif` / 直调 `on_*` + 计数器/状态断言 |
| **L4-wire 事务级 E2E** | `submit → tick → 对端收到`（ChStream wire 真实穿过） | 有 `tick()` + 抵达断言 |
| **L5 实验级** | metric 采集 + 假设结论 | `metrics.json` + 结论文档 |

⚠️ **L1 ≠ L4**。`test_*=0 装配 / 0 抵达` 的协议即使有 500 行测试，也只覆盖 L1。

⚠️ **L4 两档的区分(2026-11-06,per critic 复核 · Entry #261)**:**L4-direct** 是**进程内派发**(引擎自己记账/迁移),
⛔ **不**经过 ChStream wire、**不**含 `tick()`、**不**有对端 ⇒ 「抵达」= 引擎内部计数器/状态机;
**L4-wire** 才是本文档原口径的「wire 真穿过 + tick + 对端收到」。
⇒ 矩阵中**逐族**给出两档,⛔ 不得把 L4-direct 读成 L4-wire。

---

## 2. 覆盖矩阵（实测）

| 协议族 | L1 契约 | L2 声明 | L3 装配 | L4 事务 E2E | L5 实验 | 运行时引擎 |
|---|:---:|:---:|:---:|:---:|:---:|---|
| **CHI RN-F** | ✅ `test_cep_chi_rnf_subset.cc`(1322 行) | ✅ | ✅ CR-10③ | ✅ `submit → tick → 分派` | — | `chi_rnf_protocol_engine.hh`(648) |
| **MESIF** | ✅ `_adapter` / `_state_spec` / `_ratification` | ✅ | ✅ `instantiateAll` | ✅ `submit_local_request → tick → 对端` | — | `mesif_protocol_engine.hh`(667) |
| **CohIf 载体** | ✅ `test_cep_cohif_carrier.cc` | ✅ | ✅ `_json_assembly`(11 处) | ✅ | — | `CohIfNullProtocolEngine`（**Null 语义**） |
| **MOESI-GPU** | ✅ `_adapter`(442) / `_compensation`(324) + `_engine`(6 cases,2026-11-06) | ✅ **`moesi_gpu`** | ✅ `cep_moesi_gpu_pe_minimal.json` + **真跑 `instantiateAll`**(per Entry #259/#261) | ◐ **L4-direct** ✅(`submit_local_request → 状态机` + 10 流记账)/ **L4-wire** ❌(引擎按决策 M-6「只记账」,**零发射** ⇒ 无 wire/无 tick/无对端;per Entry #259/#261) | — | `moesi_gpu_protocol_engine.hh`(306) |
| **CXL 2.0 非对称** | ✅ `test_cep_cxl_cache_adapter.cc`(209) + `_state_spec`(451) | ✅ `cxl_2_0` | ❌ | ❌ | — | ❌ **无** |
| **CXL 3.0 对称 DCOH** | ✅ `_adapter`(188) / `_tracker`(355) / `_state_spec`(329) | ✅ | ❌ | ❌ | — | ❌（`DcohSymmetric` tracker 有行为面） |
| **TileLink-C** | ✅ `_adapter`(450) / `_state_spec`(488) + `_engine`(7 cases) | ✅ | ✅ `cep_tilelink_pe_minimal.json` | ✅ `dispatch_cohif` 5 流(2026-11-06,per 台账 **Entry #246**) | — | ✅ `tilelink_protocol_engine.hh`(**B3b 第 2 个实例**) |
| **NVLink SHARP** | ✅ `test_cep_nvlink_sharp.cc`(479) / `_state_spec`(489) | ✅ `nvlink_4` | ❌ | ❌ | — | ❌（`BroadcastNull` tracker） |
| **UALink INC** | ✅ `test_cep_ualink_inc.cc`(501) / `_state_spec`(361) | ✅ **`ualink_4` 已声明**(2026-11-06,per 台账 Entry #239) | ❌ | ❌（`plane.send/recv` 为 API 级） | — | ❌（`FabricCollectivePlane` 行为面） |

**补充**：L5 实验只有本轮新增的 H2/H3/H6（`test_cep_h2_h3_experiments.cc` /
`test_cep_h6_vc_isolation.cc`），且其协议范围是「home 侧目录策略 + VC 仲裁」，**不对应**任何单一协议族。

---

## 3. 缺口分类（按性质，不按协议）

### A. 有运行时引擎、但无事务级 E2E
- ~~**MOESI-GPU**~~：**✅ 已闭合**(2026-11-06,per 台账 **Entry #259** —— change `cpptlm-cep-moesi-gpu-l4-e2e`):
  引擎**已注册**(短名 `MoesiGpuProtocolEngine`,此前**未注册** ⇒ 「instantiate」走不通)+ 装配拓扑
  `configs/cep_moesi_gpu_pe_minimal.json` + L4 判据 `test/test_cep_moesi_gpu_engine.cc`(5 cases / 89 assertions:
  MGE-01 实例化 · MGE-02 10 流逐条记账(5 条 CohIf + 5 条直调)· MGE-03 表驱动迁移 · MGE-04 补偿拒绝路径 · MGE-05 装配闸 + channel 闸)。
- 性质:**E2E 测试缺口已闭合**。
- ⚠️ **诚实边界**:引擎的 10 个 `on_*` 按决策 **M-6「只记账」**(零发射)⇒ 本次闭合的是「**实例化 + 记账 + 迁移 + 补偿**」,
  ⛔ **不是**「跨节点协议事件语义」(那需要专用引擎)⇒ 该行「协议事件语义」列仍为 **—**。

### B. 有 tracker 行为、无协议引擎、无事务 E2E（**2026-11-06 逐族改判**，per 台账 Entry #238）
- **CXL 3.0 DCOH**（`DcohSymmetric`）、**NVLink**（`BroadcastNull`）。
- 性质：**载体形态缺口**。两者都有真实的目录/跟踪行为，但没有把它们接进一条事务路径。
- ⚠️ **本文原把 TileLink-C 与上两者并列**，理由写「规约载体无转移表真源」
  ⇒ **按 2026-11-06 实测，该理由对 TileLink-C 不成立**，故**已单列**为 **B-2**（见下）。
- 说明：oracle 2026-11-04 的裁决是「不补协议引擎」—— 理由是**规约载体无转移表真源**，
  补引擎只会诱发推理填空。⇒ 该缺口**不建议**用「造引擎」补，应记为**已知范围限制**。
  ⇒ 逐族的「为什么现在不做」已登记为**可审计的 ⏭**（reason / trigger_condition / ledger_entry），
  per 台账 **Entry #238** §⑤。

### B-2. **TileLink-C** —— ✅ **已闭合**（2026-11-06 改判 + 同日收口）：T3 普查完成 + **运行时引擎落地**
- **实测**（`include/tlm/cep/tilelink_state_spec.hh`，per 台账 Entry #237 §1.1）：
  转移矩阵 **12 格全 `Known`**、逐格 `basis` **全 `Direct`**（规范 `tilelink-c-1.7.1`，SiFive 免费开放标准，
  PDF sha256 钉版）、**0 格 `Unknown`** ⇒ **该载体确实有转移表真源** ⇒ 2026-11-04 的阻塞理由**不成立**。
- **判定式**（`07 号` Status Update 2026-11-06 的 **T1–T4**）：**T1✅ T2✅ T4✅ T3❌**
  ⇒ **触发事件已客观发生**，剩余前置 = 「CohIf 消息/意图面 → `Cap`/`Grow`/`Prune`/`Report` 事件轴」的映射
  （⛔ 该映射**在仓内任何载体都不存在**）。
- **为什么不是「立即造引擎」**：T3 未锚定前造引擎 = 换一种推理填空（2026-11-04 裁决禁止的正是这个）。
- **为什么不是「继续不做」**：规范正文**可读** ⇒ T3 属**低成本**普查，**不属**填空。
- ⇒ **2026-11-06 收口**:T3 普查**已实施完成**(11 行全条款级 `Direct`,per 台账 **Entry #242**)+ **运行时引擎已落地**
  (`tilelink_protocol_engine.hh` + JSON 装配 + L4 事务级 E2E,per 台账 **Entry #246**)⇒ **B3b = 2/6**
  (CHI RN-F + TileLink-C)⇒ **TileLink-C 记为已达成**;⛔ 其余三族(CXL DCOH / NVLink / UALink)**仍无引擎**,
  且 CXL 2.0 **显式冻结** ⇒ **不得**据此宣称 B3b / 终态 DoD Go 已达成。

### C. 只有 adapter 契约，无运行时载体
- **CXL 2.0 非对称**（`CxlCacheAdapter`）：连 tracker 都无独立行为实现（复用 MESIF 机制）。
- 性质：**实现深度缺口**，与 DoD §6.2 B3「6 个 ProtocolEngine 实施完成」的表述**不符**。
- ⚠️ **2026-11-06 补记**：CXL 2.0 的规约载体**有** 68 格转移矩阵，但 **42 格 `Unknown`**
  （覆盖率 `kLegalTransitionCoverageNum/Den` = **26/68**）⇒ 按 **T4 完备性**不成立
  ⇒ **不是转移表真源** ⇒ 该族**显式冻结**，⛔ **不得**因「有 68 格矩阵」被误推进（per Entry #238 §⑤）。

### D. 声明面缺口 —— ✅ **已闭合**（2026-11-06，per 台账 **Entry #239** 路径 A）
- **原缺口**：`ualink_4` **未声明**（用户裁决 2026-11-04，选项 A）—— 当时的理由是「`flow_support` / `perm` /
  `sharer_tracker.impl` 取值不可判定 ⇒ 不声明」。
- **为何已可闭合**：① `N-8`（有无 snoop）由 **4 个互相独立的公开非规范来源**闭合（⇒ `snoop=false` / `snoop_resp=false`，
  basis = `public-nonnormative-derived`，⛔ **非** spec-pinned，⛔ 未读任何规范正文）；② `N-9`（`perm` 子集）
  在 UALink 侧**无对应物** ⇒ 登记为 **CEP 建模声明**（`perm = ["NONE"]`）；③ 其余 9 键由 `ADR-CEP-12`
  **SU 2026-11-06** 补裁（含**订正 #1**）⇒ **13 键全部有据**（逐键的认识论分级见该 SU §②）。
- **闭合后的残余**：⚠️ **声明 ≠ 装配** —— UALink 的**实例级装配**（把 `ualink_4` 挂进拓扑 + collective plane 绑定）
  **仍待 backlog**；⇒ 本节的「**声明面**缺口」已闭合，**装配面**缺口未闭合（后者不在 B3a 范围内）。

### E. 反向 fixture 覆盖（这层反而是最齐的）
- 6 个协议族**全部**有 `cep_*_reverse_verify.sh` 并注册进 ctest。
- ⇒ 结论：**「判据有没有牙」覆盖良好；「判据有没有真实路径可跑」覆盖不足**。

---

## 4. 结论

1. **契约层（L1/L2）与反向 fixture 是全面覆盖的** —— 14 个 change 的交付在这一层是扎实的。
2. **事务级 E2E（L4）只有 3 条路径**：CHI RN-F、MESIF、CohIf（Null 语义）—— **2026-11-06 更新**：TileLink-C 新增第 4 条(`dispatch_cohif` 5 流,per 台账 **Entry #246**)。
3. **DoD §6.2 B3「6 个 ProtocolEngine」与实现不符**：运行时实际 = 3 个 RN 侧引擎
   （`chi_rnf` / `mesif` / `moesi_gpu`）+ 1 个 Null 外壳；CXL DCOH / TileLink-C / NVLink / UALink
   **只有 adapter 契约**。该口径差异需 DRI 裁决：
   - ⚠️ **2026-11-06 更新**（per 台账 **Entry #246**）：新增 **TileLink-C 运行时引擎**
     （`tilelink_protocol_engine.hh`）⇒ B3b 达成度 **1/6 → 2/6**；上文的「TileLink-C 只有 adapter 契约」
     **已被本条取代**(其余三族不变:CXL DCOH / NVLink / UALink 仍**无引擎**)。
   - 选项 ① 把 B3 重新定义为「6 协议族的 **adapter 契约**实施完成」（则已达成）；
   - 选项 ② 承认 B3 未达成，登记为待补项。
   - ✅ **2026-11-06 已裁**：走**选项 ①** ⇒ B3 **拆成两行** = **B3a**（6 族 adapter 契约 ✅）/ **B3b**（运行时引擎 **1/6** ⏳）
     （per `ADR-CEP-07` Status Update 2026-11-04 ① + `07 号` §5.1.1 + 台账 Entry #231；B3b 的口径细则见 Entry #238）。
4. **不建议**为消灭缺口 B/C 而批量造引擎（oracle 已论证 ⇒ 推理填空风险）。
   ✅ **A（MOESI-GPU）已于 2026-11-06 闭合**(per 台账 **Entry #259**)—— 实现已存在,只缺驱动 ⇒ 驱动已补(`test_cep_moesi_gpu_engine.cc`);下次的优先级为 **B 类**(载体形态缺口:CXL 3.0 DCOH / NVLink)。

---

## 5. 复现本梳理

```bash
# 装配/抵达/驱动 三维计数
for f in test/test_cep_*.cc; do
  printf "%-44s asm=%s arr=%s tick=%s\n" "$(basename $f)" \
    "$(grep -cE 'instantiateAll|ModuleFactory|JSON 装配' $f)" \
    "$(grep -cE '抵达|到达|对端|received' $f)" \
    "$(grep -cE '\.tick\(\)' $f)"
done
```
