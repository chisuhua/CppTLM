# ADR-CEP-15: CppTLM CacheTLM 与 CEP Stub Cache 的对齐边界

> **状态**: ✅ **已签发**(v1.0,2026-11-09 —— 见 `## Verification` 章与其后的签发 Status Update)
> **日期**: 2026-10-21 / **2026-11-09**(签发)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **既有 `CacheTLM` 与 CEP Stub Cache Core 的关系 / 冻结面 / 未来"真实缓存 + CEP 协议"的路径**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §3.2(Stub Cache RN 侧投影)+ §5.4(Bundle 约束)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §5.4 F12(Stub 是协议状态投影,非缓存模型)
>
> **一手锚点(代码实证)**:
> - `include/tlm/cache_tlm.hh:35` — `class CacheTLM: public ChStreamModuleBase`(174 行)
> - `include/tlm/cache_tlm.hh:42` — `std::map<uint64_t, uint64_t> cache_lines_`
> - `include/tlm/cache_tlm.hh:89` — `uint64_t access_latency = hit ? 5: 50;`
> - `include/tlm/cache_tlm.hh` — **全文 0 匹配** `snoop|coheren|mesi|moesi|directory`

---

## 1. 背景

### 1.1 问题

仓内已有 `CacheTLM`(174 行,`ChStreamModuleBase` 派生),CEP 又要引入 **Stub Cache Core**。二者都叫"缓存",极易被误认为同类,进而产生两类错误:

| 错误 | 后果 |
|------|------|
| **把 CacheTLM 当成 CEP 的 Stub** | 以为 CEP 已有缓存实现,实际二者语义不同 |
| **为"复用"而改 CacheTLM** | 破坏 dGPU 业务侧(phase-9 依赖),且违反冻结面 |

### 1.2 代码实证:二者**定位同向、粒度不同**

| 维度 | `CacheTLM`(既有) | CEP **Stub Cache Core**(新) |
|------|-----------------|--------------------------|
| 基类 | `ChStreamModuleBase` | `ChStreamModuleBase`(同类) |
| 状态载体 | `std::map<uint64_t, uint64_t> cache_lines_` | 4 个稳定权限态 `NONE/I/S/E/M` |
| 延迟模型 | hit=5 / miss=50 **周期**(硬编码) | **无性能模型**(参数化 snoop 响应延迟) |
| 协议知识 | **0**(无 snoop / coherence / mesi / moesi / directory) | **0**(per `ADR-CEP-05 §2` 定位澄清) |
| 职责 | **缓存性能模型** | **客户端侧协议状态投影** |
| 与 Coherence Interface 的关系 | **无关**(不实现 10 流) | **RN 侧投影**(6 接口,5 条 canonical 流) |

**关键结论**:二者的**"协议无关"取向同向**,但**粒度与职责完全不同**:
- `CacheTLM` 回答「**这个访问要花多久**」
- CEP Stub 回答「**这个访问在协议上意味着什么**」

---

## 2. 决策

### 决策 1:二者**共存,不替换、不合并**

**决策**:`CacheTLM` 与 CEP Stub Cache Core 是**两个不同的类**,互不替代、互不继承。

**理由**:
- 职责不同(上表)
- 生命周期不同:`CacheTLM` 属 dGPU 业务侧(phase-9);Stub Cache Core 属 CEP 探索平台
- 合并会产出一个"既无协议知识、又有协议知识"的两不像类

### 决策 2:`CacheTLM` **0 diff**(冻结面)

**决策**:`include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 CEP 全生命周期内 **0 修改**。

**理由**:
- phase-9(dGPU Phase 2/3)依赖它;CEP 是**独立探索平台**,不接管 dGPU 业务(per `ADR-CEP-07 §2 Decision 6`)
- 既有 ADR 已把它列入冻结面(per `ADR-CEP-05 §9` 的 A13 议题 = 本 ADR)

**机器检查**:`test/test_cep_module_factory_seam.cc` 的冻结面守门断言之一(per `ADR-CEP-07` 决策 3 / Stage 0 umbrella Task 5.1)。

### 决策 3:术语共享,实现不共享

**决策**:二者共享**术语**(cache line / hit / miss / latency / evict),但**不共享代码**。

| 共享的术语 | CEP 中的对应 |
|-----------|------------|
| cache line (64B,地址对齐) | 相同(per `07 号 §3.3`) |
| hit / miss | 语义相近但**度量对象不同**:CacheTLM 测延迟;CEP 测协议语义(`INV-SC6` hit 路径零网络流量) |
| latency | CacheTLM 是硬编码周期;CEP 无 latency 概念(latency 属 Fabric & Transport) |

### 决策 4:未来"真实缓存 + CEP 协议"的路径 —— **给 CacheTLM 加 adapter,而非改 CacheTLM**

**决策**:若将来需要"用真实缓存模型跑 CEP 协议",路径是:

```
CacheTLM(性能模型) ──adapter──> Coherence Interface(10 流)
```

即 **CacheTLM 侧加一个投影 adapter**,把 CacheTLM 的 hit/miss/latency 语义映射到 canonical 10 流。**CacheTLM 本身仍 0 改动**。

**理由**:
- 保持"协议无关"取向在两侧对称(CacheTLM 无协议知识;adapter 做翻译)
- 避免把协议知识注入性能模型
- adapter 是**产品代码**(进 `include/tlm/cep/`),不是测试替身(per `ADR-CEP-14` 决策 5 的对照)

**该 adapter 的落地**:属 **Stage 4+ 维护态**(per `ADR-CEP-18` 决策 5 的 T1~T4 判据,手写 adapter 在 adapter 数 ≤ 14 时仍是最优解);**Stage 1~3 不承诺**。

> ⚠️ **2026-10-22 限定注记**(per 台账 **Entry #12** [adr-amendment];**正文不改**,限定见 §8):
> 上述「adapter 数 ≤ **14**」的阈值**已被越过** —— `moesi_gpu` / `mesif` 登记后为 **16**
> (per `07 号 §5.1.1`)。本判据的**意图**是「手写 adapter 在**实例数不大**时仍是最优解」
> —— 该结论**需按 16 重新评估**(per `ADR-CEP-18` 决策 5 的 T1~T4 判据);
> **但这不改变**本 ADR 的其余结论(决策 1~5 全部不依赖该计数)。详见 §8。

### 决策 5:显式排除

| 排除项 | 理由 |
|--------|------|
| **不**把 `CacheTLM` 重命名 / 改造成 Stub | 决策 1 + 决策 2 |
| **不**让 Stub Cache Core **继承** `CacheTLM` | 二者无 is-a 关系;继承会带入性能模型语义 |
| **不**在 CEP 的 `include/tlm/cep/` 下放 `CacheTLM` 的副本 | 决策 2 |
| **不**让 CacheTLM 实现 canonical 10 流 | CacheTLM 无 snoop 概念(代码实证);强行实现会破坏其性能模型定位 |
| Stage 1~3 **不**交付"真实缓存 + CEP 协议"的 adapter | 决策 4(Stage 4+) |

---

## 3. 不变量

- **INV-CEP-15-IC1**:`include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 CEP 生命周期内 **0 diff**(机器可检查)
- **INV-CEP-15-IC2**:CEP **不得**定义 `CacheTLM` 的派生类或副本;CEP 的缓存侧实现**只有** Stub Cache Core 一个
- **INV-CEP-15-IC3**:`CacheTLM` **不得**实现 canonical 10 流的任何一条(它无协议知识);CEP 与 `CacheTLM` 之间**只允许**通过 adapter 交互

> **编号位说明**:`INV-CEP-15-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **冻结面**:`git diff --name-only <CEP 基线 commit> -- include/tlm/cache_tlm.hh src/tlm/cache_tlm.cc` **必须为空**(per `INV-CEP-15-IC1`)
   > ⚠️ **不能用** `git diff HEAD` —— CEP commit 落地后 HEAD 已含改动 → diff 恒为空 → 守门在 CI(检出 PR 分支)里**恒绿**。
   > 契约说的是「**在 CEP 生命周期内** 0 diff」,即相对 **CEP 起始基线 commit**。
   > 基线钉死在 `test/test_cep_module_factory_seam.cc` 的 `kCepBaselineCommit`(可用环境变量 `CEP_BASELINE_COMMIT` 覆盖);
   > 机器守门见 `test_cep_module_factory_seam` 的「冻结面守门机制自证」用例(故意用真实改动验证守门非恒绿)。
2. **无副本**:`include/tlm/cep/` 下**不得**出现 `CacheTLM` 的派生或复制(per `INV-CEP-15-IC2`)
3. **无协议污染**:`CacheTLM` 源码**不得**出现 `snoop` / `coherence` / `MESI` / `MOESI` / `directory` 标识符(per `INV-CEP-15-IC3`)
4. **术语不混淆**:`07 号 §3.3` 的"缓存核心协议无关性"段**必须**说明与 `CacheTLM` 的边界

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-05`(Stub Cache)| ✅ v0.7 | Stub 的 6 接口 + 定位澄清(协议状态投影,非缓存模型) |
| `ADR-CEP-07`(仿真 + seam)| ✅ v0.7 | 决策 6 的冻结面边界 |
| `ADR-CEP-12`(能力映射表)| ✅ v0.1 | `combination_capabilities` 的 `concurrent` 维度与"真实缓存"路线相关 |
| `ADR-CEP-14`(Stub 合同测试)| ✅ v0.1 | 合同测试**不测**缓存性能(与 `CacheTLM` 的性能模型职责分离) |
| `ADR-CEP-18`(协议生成路线)| ✅ v0.1 | 决策 4 的 Stage 4+ 归属 |
| `ADR-CEP-07A`(06-bis 产物处置)| ✅ v0.1 | `coherence_domain.hh` / `coherent_xbar_tlm.hh` 的处置与本 ADR 的边界一致 |
| `cpptlm-cep-stub-cache-core`(Stage 1.1) | 📋 Planned | 新建 Stub Cache Core(**不**改 `CacheTLM`) |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 共存不替换 | 两个不同的类,互不替代 / 互不继承 | ✅ 已拍板 |
| `CacheTLM` 0 diff | 冻结面,机器可检查 | ✅ 已拍板 |
| 术语共享实现不共享 | 共享 hit/miss/latency 术语,代码独立 | ✅ 已拍板 |
| 未来路径 | 给 CacheTLM 加 **adapter**,不改 CacheTLM;Stage 4+ | ✅ 已拍板 |
| 显式排除 | 不重命名 / 不继承 / 不复制 / 不实现 10 流 / Stage 1~3 不交付 adapter | ✅ 已拍板 |

---

## Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)

> **口径**:本章是**签发前置**(per 台账 Entry #316 · oracle C-3.3)。每条给**载体** + **反向 fixture(毒化 ⇒ 必红 + 归因 id)**;归因 id **一律复用**既有体系 ⇒ ⛔ **禁**新造平行 id 体系。**载体状态**逐条标注:✅ 可执行 / **⏭-unmodelled**(既有登记,禁当鉴别力判据)/ **id 待建立**(机检登记)/ **文档层**(人工评审)。

| # | 判据 | 载体 | 反向 fixture(毒化 ⇒ 必红 + 归因 id) |
|---|---|---|---|
| **V-15-1** | `include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 **CEP 生命周期内 0 diff**(相对 `kCepBaselineCommit`;⛔ **不能**用 `git diff HEAD` —— HEAD 已含改动 ⇒ 恒空)(§3 `IC1` · 抓取点 1) | `test/test_cep_module_factory_seam.cc`(4 个 frozen headers 零 diff + **守门机制自证**用例) | 归因 id **`INV-CEP-15-IC1`** · 毒化:对 `cache_tlm.hh` 做真实改动并提交 ⇒ 必红(自证用例证明守门**非恒绿**) |
| **V-15-2** | CEP **不得**定义 `CacheTLM` 的派生类或副本;CEP 的缓存侧实现**只有** Stub Cache Core 一个(§3 `IC2` · 抓取点 2) | `test/test_cep_module_factory_seam.cc`(CEP 侧只有一个缓存实现) | 归因 id **`INV-CEP-15-IC2`** · 毒化:在 `include/tlm/cep/` 下新增 `CacheTLM` 派生 ⇒ 必红 |
| **V-15-3** | `CacheTLM` **不得**实现 canonical 10 流的任何一条(无协议知识);CEP 与它之间**只允许**经 adapter 交互(§3 `IC3` · 抓取点 3) | `test/test_cep_module_factory_seam.cc`(`CacheTLM` 零协议知识) | 归因 id **`INV-CEP-15-IC3`** · 毒化:在 `cache_tlm` 源码里加 `snoop` / `coherence` / `MESI` / `directory` 标识符 ⇒ 必红 |
| **V-15-4** | `07 号 §3.3`「缓存核心协议无关性」段**必须**说明与 `CacheTLM` 的边界(抓取点 4) | ⚠️ **文档层判据**(无机器载体)⇒ 人工评审 | 归因 id **待建立** ⇒ 登记于 `test/cep_adr_structural_deferrals.json` 的 `pending_invariant_ids`;反向 fixture **无** ⇒ ⛔ **禁**当作鉴别力判据 |

**载体状态汇总**:V-15-1 ~ V-15-3 的载体**已存在且逐条实证** ⇒ **可执行**;V-15-4 为**文档层判据**(人工评审)⇒ 不计入「有牙载体」。

**签发前置自检**:① 本章位于**正文**(首个 `## Status Update` 之前)✅ ② 每条判据均有**载体 + 归因 id + 反向 fixture**(文档层项已显式标注)✅ ③ 归因 id **全部复用**(⛔ 零新造)✅ ④ 载体**逐条实证存在** ✅

---
## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 8. Status Update(2026-10-22,per 台账 Entry #12 [adr-amendment])

> **授权**:台账 `objective-cep-platform §11` **Entry #12**;触发 = R-13(`moesi_gpu` / `mesif` 登记进
> `07 号 §5.1.1`,adapter 实例数 14 → 16)。**正文不改**(per ADR 不可变约定),限定记录于本节。

### 8.1 决策 4 末条的「adapter 数 ≤ 14」阈值 —— **已越过,需重评**

| 项 | 内容 |
|----|------|
| **原文**(决策 4 末条) | 「手写 adapter 在 adapter 数 ≤ **14** 时仍是最优解」 |
| **事实变化** | `moesi_gpu` / `mesif` 登记进 `07 号 §5.1.1` 后,adapter 实例数 **14 → 16**(工业 6 族 8 实例 + Ruby 1 族 8 实例)→ **阈值已被越过** |
| **限定后的表述** | 「手写 adapter 在 adapter 数 ≤ **16** 时是否仍是最优解,**待按新数重评**」(重评归属 `ADR-CEP-18` 决策 5 的 T1~T4 判据) |
| **不影响本 ADR 的结论** | 决策 1(共存不替换)· 决策 2(`CacheTLM` 0 diff 冻结面)· 决策 3(术语共享实现不共享)· 决策 4 的**路径方向**(给 CacheTLM 加 adapter 而非改 CacheTLM)· 决策 5(显式排除)—— **全部不依赖该计数**,零改动 |
| **对 `ADR-CEP-18` 的连带** | `ADR-CEP-18:68` 的「为 **16~20** 个实例建编译器,投入产出倒挂」是**同一问题**的另一次表述 —— 两处口径**已同步** |

### 8.2 未变更项(明示)

> §2 决策 1~5 的全部结论 · §3 的 3 条不变量 · §4 的抓取点 · `CacheTLM` 的 0 diff 冻结面 —— **全部零改动**。

## Status Update(2026-10-27,per 台账 Entry #74 [scope-change]) —— §1.2 表格「Stub Cache Core 基类 = `ChStreamModuleBase`」与代码不符(基类漂移登记)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文一律不改**,仅登记状态变化。
> **登记 —— §1.2 的基类漂移**:§1.2 的对照表写「基类 | `ChStreamModuleBase` | `ChStreamModuleBase`(同类)」—— 左侧(`CacheTLM`,`include/tlm/cache_tlm.hh:35` = `class CacheTLM: public ChStreamModuleBase`)**属实**;**右侧不属实** —— 实测 `include/tlm/cep/stub_cache_core.hh:154` = `class StubCacheCore {`(**普通 class**,非 `ChStreamModuleBase`)。⇒ 本 ADR 的**核心结论(决策 1「二者共存,不替换、不合并」)** **不受影响**(两者仍是两个不同的类,粒度与职责对照表逐行仍成立)—— 漂移的只是「基类」一栏的**实现现状**。
> **为什么会漂移**:队列 #3 `cpptlm-cep-stub-cache-core`(已完成归档,2026-10-25)把 stub 实现为 **testbench 定位**的组件 —— `stub_cache_core.hh:39-40` 明写「**不**经 `ModuleFactory` 注册(本类是 **testbench 定位**的组件)」;`openspec/specs/cep-stub-cache-core/spec.md`(M1 冻结成员)亦写「`net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)」⇒ **实现按队列 #3 的裁决落地,而本 ADR 的表格按「最终形态」写** ⇒ 二者不同步。
> **与 `ADR-CEP-07` 决策 3 的关系**:该决策同样规定 StubCacheCore **应当**是 `ChStreamModuleBase` + `registerObject` ⇒ 本漂移是**同一个三叉矛盾**的一面(另两面:`ADR-CEP-07:64` 规定 ProtocolEngine 应是 `ChStreamModuleBase`,而仓内尚无 ProtocolEngine;`stub_cache_core.hh:39-40` 的「不注册」与 `ADR-CEP-07:67` 的「应注册」直接冲突)。同日登记于 `ADR-CEP-07` Status Update。
> **不在本 ADR 的处置范围**:`ADR-CEP-19` 决策 9 明确**不动** `StubCacheCore`(stub 侧端点延期到 `cpptlm-cep-stub-cache-chstream-surface`,需 MODIFIES 冻结 main spec)⇒ **本漂移的修复不在队列 #7 范围内**,仅登记备查。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #74**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #74)。

---

## Status Update(2026-11-09,per 台账 Entry #316)—— **签发**(v1.0)

> **授权**:台账 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 **Entry #316**(`[adr-amendment]` + 冻结面五步授权链)。
> **本次签发 = oracle 裁决 C-3.2 的**两步**动作**:① **补顶层 `## Verification` 章**(签发前置,per C-3.3 —— 该章此前缺失 ⇒ 本 ADR 长期停在 🟡);② **原地更新 header 的 `> **状态**:` 元数据行**(唯一豁免行)+ 追加本段。⛔ **决策内容一字未动**。

### ① 签发前置(逐条达成)

| 前置 | 达成证据 |
|---|---|
| ① **顶层 Verification 章**存在且位于**正文**(首个 `## Status Update` 之前) | 本文件 `## Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)` 章 · ADR 结构闸 `scripts/test/cep_adr_structural_check.sh` 判**合规** |
| ② 判据的**载体 + 归因 id 逐条实证** | 载体与断言位置已对 `test/cep_invariant_assertion_map.json` 逐条核对;⏭ 项对 `test/cep_invariant_deferrals.json`;id 债务对 `test/cep_adr_structural_deferrals.json` 的 `pending_invariant_ids`(闸 ④b **机检**:ADR 存在 + 章节锚点 + id 命名空间 + 台账 Entry) |
| ③ **oracle → critic 双审** | oracle **PASS-WITH-NOTES**(2 项必修已落:N1 签发凭据显式声明「零可执行」· N2 双闸实跑)· critic **APPROVE**(0 BLOCKER · 0 SHOULD_FIX · 1 NIT 记录) |
| ④ 台账授权 | **Entry #316** |
| ⑤ 冻结面五步授权链 | 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #316"` → **清单单独 commit** → **无参复验** |

### ② 签发后的边界(逐字)

- ⛔ **决策内容未动**:既有正文与既有 `## Status Update` 段**逐字未变**;正文的**唯一**变更是**新增** `## Verification` 章(签发前置)。
- ⛔ **不宣称判据全部可执行**:可执行 / **⏭-unmodelled** / **归因 id 待建立** / **文档层(人工评审)** 四类在本文件 Verification 章的「载体状态汇总」中**逐条标注**。
- ⛔ `B3b-2` **维持 0/4**。
- 📌 **本 ADR 自此不可变**:此后任何修改**只**能以追加 `## Status Update` 段的形式进行,且走冻结面授权链。

**关联**:台账 **Entry #316**(授权 + 双审处置)· **#315 / #314**(第 2/1 批)· **#307**(4 项裁决)· **#313**(T3 登记表)· `test/cep_invariant_deferrals.json`(⏭ 真源)· `test/cep_adr_structural_deferrals.json`(id 债务)· `scripts/test/cep_adr_structural_check.sh`

---

## Status Update(2026-11-18,per 台账 Entry #478 [scope-change])—— **决策 2 改判**:`CacheTLM` 由「文件 0 diff 冻结面」改为「**协议语义边界**冻结」

> **性质**:追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例 + 本文件 §8 签发后边界「此后任何修改**只**能以追加 `## Status Update` 段的形式进行」)—— **§1 ~ §7 正文一律不改**,仅登记状态变化。
> **授权**:台账 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 **Entry #478**(`[scope-change]` + 冻结面五步授权链)。
> **触发**:dGPU 侧「配置里的参数必须有确定效果」的语义落地(`ADR-X.17` F-14 家族)要求 `CacheTLM` 消费 6 个配置键(`size` / `line_size` / `associativity` / `assoc` / `level` / `protocol`);而 §2 决策 2 把该类列为 **0 diff 冻结面** ⇒ 实施期实测「改一行即红闸」(`test/test_cep_module_factory_seam.cc` 的 0 diff 断言)⇒ **必须先解冻**。
> **裁决**:由用户(架构裁决权)直接裁定 —— **解冻该类,在 `cache_tlm.hh` 内补容量模型 + 6 键读取面**。

### ① 决策 2 的改判(逐字对照)

| 项 | 原(§2 决策 2) | 改判后(自本 SU 起生效) |
|---|---|---|
| **冻结对象** | `include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 CEP 全生命周期内 **0 修改** | **协议语义**不得进入本类 —— 几何 / 容量 / 层级参数**可配**;文件本身**不再冻结** |
| **理由** | phase-9(dGPU Phase 2/3)依赖它;CEP 是**独立探索平台**,不接管 dGPU 业务 | 该类**本就是 dGPU 业务侧**的性能模型,不是 CEP 产物 ⇒ 用「文件 0 diff」保护它,**保护错了对象**:真正要保护的是「**协议语义不进性能模型**」这条边界,而不是「文件一个字节都不能动」 |
| **机器检查** | `git diff --name-only <CEP 基线> -- include/tlm/cache_tlm.hh src/tlm/cache_tlm.cc` **必须为空** | ① **`protocol` 键 ⇒ 装配期失败且错误含键名**(行为判据,载体 `test/test_cep_module_factory_seam.cc`)② **源码不含协议标识符**(`snoop` / `coherence` / `MESI` / `MOESI` / `directory`;载体同上)③ **几何键不得引入协议语义**(`size` / `line_size` / `associativity` / `level` 只能是容量与延迟语义) |

### ② `INV-CEP-15-IC1` 的判据改判(**id 不变**,载体迁移)

| 项 | 原 | 改判后 |
|---|---|---|
| **判据** | `include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 CEP 生命周期内 **0 diff**(机器可检查) | **`protocol` 类键不得成为本类的可配置语义** —— 配置写了 ⇒ 装配期失败且报出键名(机器可检查) |
| **载体** | `test/test_cep_module_factory_seam.cc` 的 4 个 frozen headers 零 diff case 内两条 `REQUIRE_FALSE(file_modified(...))` | 同文件新增独立 case(装配失败判据)+ **保留**既有「零协议标识符」case(`INV-CEP-15-IC3`) |
| **为何是行为判据而非文本判据** | — | 文本判据只挡「已知的协议名」;行为判据挡**全部**协议名 ⇒ 新协议族登记时**无需**改闸 |

> ⛔ **`## Verification` 章 V-15-1 同步改判**:该行原文写「`cache_tlm.hh` / `cache_tlm.cc` 在 CEP 生命周期内 **0 diff**」⇒ **正文不改**(per 本文件 §8「此后任何修改只能以追加 `## Status Update` 段的形式进行」)⇒ **以本 SU 为准**;V-15-1 的**载体**改指本 SU ① 的「行为判据」行。
>
> ⛔ **归因 id 零新造**:`INV-CEP-15-IC1` **沿用原 id**(只改判据内容)⇒ `07 号 §3.3` 的 `IC1 ~ IC3` 编号位、`test/cep_canonical_invariants.cc` 的 id 清单、`test/cep_invariant_assertion_map.json` 的 id 集合**全部不变**。

### ③ 决策 2 之外,**未变更项**(明示)

- **决策 1**(二者共存,不替换、不合并)—— **不变**;`CacheTLM` 仍是 dGPU 侧类,CEP 侧仍只有 Stub Cache Core 一个。
- **决策 3**(术语共享,实现不共享)—— **不变**;新增的容量 / 组相联 / 层级术语**不引入** CEP 侧实现。
- **决策 4**(未来「真实缓存 + CEP 协议」的路径 = **给 CacheTLM 加 adapter,而非改 CacheTLM**)—— **方向不变**,但**适用面被本 SU 收窄**:本 SU 允许改的是「**性能模型自身**的几何参数」,**不是**「把协议语义写进 `CacheTLM`」⇒ 决策 4 的路径**依然是**唯一路径。
- **决策 5**(显式排除:不改造成 Stub / 不让 Stub 继承 / 不放副本 / **不让 `CacheTLM` 实现 canonical 10 流**)—— **全部不变**;`protocol` 键**显式拒绝**正是决策 5 第 4 条的机器化。
- **§3 `IC2` / `IC3`** —— **不变**(`IC3` 的源码标识符检查是本次改判的**守门底线**)。
- **§4 抓取点 1** —— **改判**(见 ① 的机器检查行);抓取点 2/3/4 **不变**。

### ④ 被否决的替代方案(明示,避免事后重开)

| 备选 | 否决理由 |
|---|---|
| **A. 不解冻,容量模型另起新类**（如 `SetAssocCacheTLM`),`CacheTLM` 保持 0 diff | ① 用户已裁定「解冻 + 在 `cache_tlm.hh` 内补」;② 更关键:另起新类 ⇒ **存量 37 处 `CacheTLM` 配置实例**全部要迁移类型名 ⇒ 配置面冲击远大于「改一个类的实现」;③ 同一职责两个类 ⇒ 后续每个缓存场景都要先问「用哪个」 |
| **B. 解冻并让 `protocol` 成为**可选标签**(读进来不生效) | 那等于把零效果键换个地方存 ⇒ 违反 `ADR-X.17` D-6(「两个不同合法值 ⇒ 可观测指标不同」不成立)⇒ 只能有唯一合法值 ⇒ 仍是伪参数 ⇒ 仍应**显式拒绝** |
| **C. 解冻并把 `protocol` 变成真正的协议选择器** | 决策 5 第 4 条 + `IC3` **明禁**(`CacheTLM` 不得实现 canonical 10 流、不得有协议知识);且协议侧实现与状态机在 CEP 层 ⇒ 双写协议状态机 = 两个真源 |
| **D. 保持 0 diff,只把 6 个键登记成「本类永不支持」** | 合法但**不完整**:6 个键里 5 个(`size` / `line_size` / `associativity` / `assoc` / `level`)是**纯几何 / 延迟语义**,与协议无关 ⇒ 把它们一并拒掉是**能力损失**而非边界保护 |

### ⑤ 连带面(同步处置清单)

| # | 产物 | 处置 |
|:--:|---|---|
| 1 | `test/test_cep_module_factory_seam.cc` | 0 diff 断言**移除**;新增 `protocol` 拒绝的**行为**判据(`INV-CEP-15-IC1` 载体迁移) |
| 2 | `docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md` 决策 6 表行 | 追加 Status Update 指针(「`cache_tlm.hh` 不修改」→ 指向本 SU) |
| 3 | `openspec/specs/cep-core-contracts/spec.md`(`Requirement: cep-cachetlm-alignment-contract` · Scenario「CacheTLM 零 diff(IC1)」) | 场景内容改判(⛔ **场景名保留** —— openspec 的 `MODIFIED` 按场景名精确匹配且 archive 拒绝丢弃既有场景名);**改判走冻结面授权**(M1 冻结成员) |
| 4 | `docs/architecture/cep/fabric-transport.md` 冻结面声明行 | 追加指针(「不破 `include/tlm/cache_tlm.hh`」→ CEP 侧仍不改;该文件已由 dGPU 侧解冻) |
| 5 | `docs/architecture/cep/parity-baseline.md`(H7 对拍基准规格) | 「`CacheTLM` 是**无限容量 + 精确地址 map**」的口径**需限定**(默认配置下仍成立;显式配 `size` 后为有限容量 + 组相联替换) |
| 6 | `openspec/changes/cpptlm-module-param-effect-landing/`(实施载体) | T1.1 由「⛔ 阻塞」转为「已落地」;`design.md` D-1.6.4 补 `CacheTLM` 行;`proposal.md` R-4 记解除 |
| 7 | `docs/implementation/module-param-baseline.md` | 表 B **B-1** 状态改「已消费 / 已显式拒绝」 |

### ⑥ 容量模型的**记账口径**(明示,避免与教科书「行粒度」混淆)

- **命中判定 = 精确地址**(沿用改动前口径)⇒ **默认配置(未配 `size`)下行为逐位不变**(零回归);
- **容量模型由 `size` 启用** ⇒ 容量槽数 = `size / line_size`,组数 = `max(1, 槽数 / associativity)`,组满则**逐出该组最近最少使用者**;
- `line_size` / `associativity` / `assoc` **必须**与 `size` 同时出现(否则它们没有任何可观测效果 ⇒ **显式失败**,不静默忽略);
- `level` **独立生效** ⇒ 决定命中延迟(1 → 5 / 2 → 10 / ≥3 → 20 cycle;未指定 → 5,沿用原值);
- `protocol` **显式拒绝**。

**关联**:台账 **Entry #478** · change `openspec/changes/cpptlm-module-param-effect-landing/` · `ADR-X.17-module-param-read-declares.md`(D-1 / D-6)· `ADR-CEP-07` 决策 6(见 ⑤-2)· `test/test_cep_module_factory_seam.cc`(IC1 / IC3 载体)· `test/cep_invariant_assertion_map.json`(载体映射)
