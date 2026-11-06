# 锚点摘要 · UALink `request` / `completion` 两流的**公开非规范**语义依据

> **目的**:为 `ualink_4` 实例的 **canonical 10 流**中 `request` / `completion` 两条寻找**语义依据**(per 台账 Entry #263 ④ 的前置:「无依据自造语义 = SU · 四 D-1 禁止的推理填空」)。
> **形态**:与 `ADR-CEP-04` SU 2026-11-06 的 `C-10` 锚点**同型** —— **4 个互相独立的公开非规范来源 + 一步推断**留档。
> **证据档位**:**公开非规范**(`public_nonnormative`)⇒ ⛔ **不可计 `Direct`**、⛔ **不计入 B3b**(per `07 号` SU · 四 D-1)。

---

## ⓿ 合规声明(⚠️ **必读**:本轮检索的**被动接触**记录)

2026-11-07 的 web 检索中,检索工具在结果摘要里**自动返回**了 `ualinkconsortium.org` 托管的
`UALink200_Specification_v1.0_Evaluation_Copy.pdf`(标注 **Evaluation Copy**)的正文片段
(可见 §1.5 UALink Coherency / §2.7.3 UPLI Transactions-Channel usage 等)。

- ⚠️ 该片段是**检索工具自动附带**的,**本仓未主动下载、未主动阅读**该 ECA 文件;
- ⛔ **本摘要不使用该 ECA 片段作为任何依据**(不引用其条款号、不据其作语义判断、不据其提升证据档位);
- ✅ 本摘要的**全部**结论**只**取自下节列出的 4 个**公开非规范来源**(白皮书 / 联盟博客 / 第三方厂商分析 / 公开 webinar 材料);
- ⇒ **证据档位锁定为 `public_nonnormative`**;若后续任何条目被发现源自 ECA 片段 ⇒ 该条目 SHALL 作废并重做。

## 一、4 个互相独立的公开非规范来源

| # | 来源 | 类型 | 关键主张(与本 change 相关部分) |
|---|---|---|---|
| **S-1** | **UALink 1.0 White Paper**(`ualinkconsortium.org/wp-content/uploads/2025/04/UALink-1.0-White_Paper_FINAL_UPDATED.pdf`,2025-04) | 联盟公开白皮书 | UALink 定位为**开放标准**的 scale-up 网络方案;面向 **Accelerator↔Accelerator** 通信 |
| **S-2** | **UALink Consortium 博客「UALink™ 200G 1.0 Specification Overview」**(`ualinkconsortium.org/blog/ualink-200g-1-0-specification-overview-802/`,2025-05-15) | 联盟公开博客 | 内存语义 = **read / write / atomic**;**一致性由软件维护**;**split request/response**;每端口 transaction tag 支持多笔未完成;事务 **64–256 B**;**请求按 source-destination 有序,completion 不保证有序**;源→目的**有序请求地址** |
| **S-3** | **Cadence 博客「Breaking Down UPLI: A Protocol-Level Perspective on UALink 200」**(`community.cadence.com/…/breaking-down-upli…`,2026-02-12) | 第三方厂商分析 | UPLI = **4 通道**(Request / Originator Data / Read Response-Data / Write Response);事务 = **Request + 配对 Response**;**single-copy atomicity**;**请求按 source-destination 对有序**;credit-based 流控;split req/rsp |
| **S-4** | **UALink Deep Dive Webinar Slides**(`ualinkconsortium.org/wp-content/uploads/2026/08/UALink-Deep-Dive-Webinar-Slides_FINAL.pdf`,2026-08-17) | 联盟公开 webinar 材料 | UPLI 4 通道(同 S-3);**Collective Primitives** = `ReadReduce` / `WriteMulticast` / `WriteFullMulticast` / `AtomicNRMulticast` —— 交换机复制并下发到各 rank;**归约结果回传请求方**;BROADCAST/REDUCE/ALLREDUCE 由 `WriteMulticast`+`ReadReduce` 循环实现 |

> ⚠️ 4 个来源**互相独立**(联盟白皮书 / 联盟博客 / 第三方厂商 / 联盟 webinar)⇒ **非**同一文本的转述。

## 二、一步推断(留档,逐条可追)

| # | 推断 | 依据 | 强度 |
|---|---|---|---|
| **I-1** | canonical 流的 **`request`** ↔ UPLI **Request**(Read / Write / Atomic / UPLI Write Message / Vendor Defined Command) | S-2 + S-3 的「Request + Response 配对成事务」 | 一步推断(术语不同 ⇒ 映射非直读) |
| **I-2** | canonical 流的 **`completion`** ↔ UPLI **Response**(Read Response-Data 或 Write Response 二选一,取决于事务类型) | S-3 的 4 通道 + 「Completer 以 Response 应答」 | 一步推断 |
| **I-3** | `ualink_4` 的 **`snoop = false`** 与公开来源一致(UALink **不支持** snoop 事务 ⇒ 一致性由软件维护) | S-1 + S-2 + S-3(S-3 明确 4 通道中无 snoop 通道) | 多源一致(≥3) |
| **I-4** | `ualink_4` 的 **`query`** 类 ↔ collective primitive 的 **ReadReduce**(读 + 归约结果回传) | S-4 的 collective primitives 描述 | 一步推断 |
| **I-5** | `ualink_4` 的 **`atomic`** 类 ↔ **AtomicR / AtomicNR**(单拷贝原子性) | S-3 的 single-copy atomicity + S-2 的 atomic 操作 | 一步推断 |
| **I-6** | `ualink_4` 的 **`response`** 类 ↔ 归约/写回的**结果返回**(与 `completion` 的区别 = 语义层级:一次请求的完成 vs 一次 collective 的结果) | S-4「归约结果回传请求方」 | 一步推断(⚠️ **本条最弱** —— 依赖仓内 `ualink_inc_state_spec` 的 INC 原语定义) |

## 三、与既有 `ualink_4` 声明的**一致性核对**(不新增声明)

| 既有声明项 | 值 | 与公开来源核对 |
|---|---|---|
| `flow_support.snoop = false` | false | ✅ **一致**(I-3,3 源) |
| `flow_support.request / completion = true` | true | ✅ **可支撑**(I-1 / I-2) |
| `flow_support.forward_data / back_invalidate / address_resolve / barrier / snoop_completion_ack = false` | false | ✅ **一致**(UPLI 无对应通道,S-3 的 4 通道清单) |
| `perm = [NONE]` | 仅 NONE | ✅ **一致**(软件一致性 ⇒ 无硬件权限授予) |
| `vc_binding.ualink_fabric` | request:0 · atomic:1 · query:2 · response:3 · snoop_resp:3 | ⚠️ **部分可支撑**:`request`/`atomic`/`query`/`response` 有公开来源支撑(I-1/I-4/I-5/I-6)。⚠️ `snoop_resp` 与 `response` **同 VC(3)**,但按声明自身口径,`vc_binding` 是「消息类 → VC 的分配映射」(**不是** `flow_support` 的重复声明)⇒ 该键**不代表该实例承载 snoop_resp 流**(承载与否由 `flow_support.snoop_resp = false` 独占)⇒ ⛔ 本摘要**不为 `snoop_resp` 提供任何语义依据**(该类语义**仍待定**) |
| `ordering_assumption.ualink_fabric` | vc0/1/2 = `unordered` · vc3 = `p2p_ordered` | ⚠️ **须区分「建模假设」与「协议保证」**:S-2/S-3 只支持「**请求**按 source-destination 有序、**completion 不保证有序**」;而仓内排序点判据 **J1** 要求「**响应向** vc 全部 `p2p_ordered`」⇒ **J1 的 `p2p_ordered` 是建模假设**(假设 completion 有序以便定位排序点),**不是**协议保证 ⇒ ⛔ **引擎的 completion 记账不得依赖有序性假设**(否则结论只在「假设成立」下有效) |

## 四、边界(⛔ 不得越界)

1. ⛔ 本摘要**不**构成 T1(转移矩阵真源)/ T2(可判定)⇒ **不计入 B3b**;`ualink_4` 仍是 B3b-2 的受限族;
2. ⛔ 本摘要**不**提供条款级(`spec_clause`)依据 ⇒ 凡引用本摘要的格,`basis` SHALL 保持**弱档**(`SelfDerived` / `pending = true`)+ **fail-closed 不被引擎消费**(per SU · 四 D-1 与 `07 号` §6.4 的「弱档不消费」纪律);
3. ⛔ `snoop_resp` 与 I-6 是**最弱环节** ⇒ 引擎实现 SHALL 把 `snoop_resp` 与 collective 结果回传**显式标为 pending / 不消费**;
4. ⚠️ 本摘要的来源**均为公开非规范** ⇒ 即便 4 源一致,也**不**满足 D-1 的「条款级」要求 ⇒ 依据档位不可升格。

**关联**:台账 **Entry #263** ②④(第 2 项立项与依据口径)· `ADR-CEP-04` SU 2026-11-06(`C-10` 锚点同型先例)· `07 号` SU 2026-11-06 · 四(D-1 来源效力)· `ADR-CEP-12` SU 2026-11-06(认识论三档)
