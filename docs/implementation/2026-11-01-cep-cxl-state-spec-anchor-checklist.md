# CXL.cache 规约逐格核对表(4 态 × 17 事件 = 68 格)

> **来源**:CppTLM 仓 change `cpptlm-cep-cxl-nonsymmetric-state-spec`(队列 #10 的**第 0 个 change** = 规约层)
> **规范性载体**:`include/tlm/cep/cxl_state_spec.hh`(**唯一真源** —— 逐格 `basis` 只在该文件)
> **basis 唯一人工审计面**:`docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`
> **生成方式**:本表的 68 格由**载体直接生成** —— 生成器已入库:
> `scripts/test/generate_cxl_anchor_checklist.sh`(+ `.cc`)
> ⇒ **载体变更 ⇒ 跑一次生成器**;一致性由 `test_cep_cxl_state_spec.cc` 的 **CSS-06** 机器判据把守,
> 生成器另有 `--check` 模式(只比对不写)可作提交前自检。
> **日期**:2026-11-01

---

## 0. 一句话

CXL.cache 的**逐状态合法性表**在公开来源**不可得**(残余风险 **R-2**:该表位于规范 §3.2.5.15,
而规范正文受 Evaluation Copy Agreement §3 + AI Acceptable Use Policy 约束 ⇒ **AI 不得读取**)。

⇒ 本表按**三态格语义**登记 68 格:

| 格语义 | 含义 | 当前格数 |
|---|---|---|
| **`Known`** | 已知**目标态**(证据强度由 `kind` + `pending` 承载) | **26** |
| **`Illegal`** | 已知**不合法** —— ⚠️ **恒 0 格**,不是「没查」而是**来源不给** | **0** |
| **`Unknown`** | **不知道**(≠ 非法)—— SHALL 携带 `R-n` 缺失项编号 | **42** |

⇒ 覆盖率 **26/68**;其中 `pending = false`(可作已裁决依据)**22 格**,**全部为 D1**(CC-BY 4.0)。

⚠️ **「填满 68 格、无证据的格标 `self-derived`」不采用** —— 那仍是「我断言这个转移」⇒ 批量杜撰。
⚠️ **「非法」必须举证** —— D1 p.7 证实规范**存在**该类约束(`certain requests are only legal when the
device's cache is in a certain state`)但**不给具体是哪些** ⇒ 填任何 `Illegal` 都是杜撰。

---

## 1. 逐格核对表(载体直接生成)

<!-- CXL-TABLE:MATRIX:START -->
| # | 事件 | 态 | 格语义 | 目标态 | kind | pending | ref | note |
|---|---|---|---|---|---|---|---|---|
| 1 | RdShared | I | Known | S | public-direct | false | D1 p.5 Fig.4 InvalidLoad1 | I + Load ⇒ 发 RdShared;态 := ISAD ⇒ 完成后 S |
| 2 | RdShared | S | Unknown | - | self-derived | true |  | R-2: S 态读命中应静默无消息(可能根本没有规则 ⇒ 不可读作 Illegal) |
| 3 | RdShared | E | Unknown | - | self-derived | true |  | R-2: 同上;D1 合并 E/M ⇒ E 无独立证据 |
| 4 | RdShared | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 5 | RdCurr | I | Known | I | public-direct | false | D1 p.4 | 逐字:does not affect ownership (nor coherence) |
| 6 | RdCurr | S | Known | S | public-direct | false | D1 p.4 | 不改态 |
| 7 | RdCurr | E | Known | E | public-direct | false | D1 p.4 | 不改态 |
| 8 | RdCurr | M | Known | M | public-direct | false | D1 p.4 | 不改态 |
| 9 | RdOwn | I | Known | M | public-direct | false | D1 p.4 Fig.4 InvalidStore1 | I + Store ⇒ 发 RdOwn;IMAD 释义给 I→M 的 A/D 等待 |
| 10 | RdOwn | S | Known | M | self-derived | true | D1 p.4 瞬时态命名 | 结构性推断:SMAD/SMD/SMA + IMAD 同构 ⇒ 目标 M;⚠️ 无逐字规则 |
| 11 | RdOwn | E | Unknown | - | self-derived | true |  | R-2: E 已含写权限(D1 p.3 exclusive (write-access and clean))⇒ 语义冗余,但无来源断言合法性 |
| 12 | RdOwn | M | Unknown | - | self-derived | true |  | R-2: 无来源 |
| 13 | RdAny | I | Unknown | - | self-derived | true |  | R-2: D1 只说功能被 RdOwn+RdShared 覆盖,**不指明取哪个** ⇒ 不得臆断目标态 |
| 14 | RdAny | S | Unknown | - | self-derived | true |  | R-2: 同上 |
| 15 | RdAny | E | Unknown | - | self-derived | true |  | R-2: 同上 |
| 16 | RdAny | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 17 | RdOwnNoData | I | Known | M | public-direct | false | D1 p.4 | 定义式等价于 RdOwn ⇒ 继承其 I 格 |
| 18 | RdOwnNoData | S | Known | M | self-derived | true | D1 p.4 | 继承 RdOwn 的 S 格(结构性推断) |
| 19 | RdOwnNoData | E | Unknown | - | self-derived | true |  | R-2: 继承 RdOwn 的 E 格 |
| 20 | RdOwnNoData | M | Unknown | - | self-derived | true |  | R-2: 继承 RdOwn 的 M 格 |
| 21 | CLFlush | I | Unknown | - | self-derived | true |  | R-2: D1 p.5/p.12 显式排除(**证明相关性判断**,非状态语义断言) |
| 22 | CLFlush | S | Unknown | - | self-derived | true |  | R-2: 同上 |
| 23 | CLFlush | E | Unknown | - | self-derived | true |  | R-2: 同上 |
| 24 | CLFlush | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 25 | CacheFlushed | I | Unknown | - | self-derived | true |  | R-2: 无来源 |
| 26 | CacheFlushed | S | Unknown | - | self-derived | true |  | R-2: 无来源 |
| 27 | CacheFlushed | E | Unknown | - | self-derived | true |  | R-2: 无来源 |
| 28 | CacheFlushed | M | Unknown | - | self-derived | true |  | R-2: 无来源 |
| 29 | ItoMWr | I | Unknown | - | self-derived | true |  | R-2: D1 显式排除;⚠️ 该排除不得推出「不改态」 |
| 30 | ItoMWr | S | Unknown | - | self-derived | true |  | R-2: 同上 |
| 31 | ItoMWr | E | Unknown | - | self-derived | true |  | R-2: 同上;⚠️ D1 合并 E/M ⇒ 严禁复制 M 规则 |
| 32 | ItoMWr | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 33 | WrCur | I | Unknown | - | self-derived | true |  | R-2: D1 显式排除;⚠️ 同上 |
| 34 | WrCur | S | Unknown | - | self-derived | true |  | R-2: 同上 |
| 35 | WrCur | E | Unknown | - | self-derived | true |  | R-2: 同上 |
| 36 | WrCur | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 37 | WOWrInv | I | Unknown | - | self-derived | true |  | R-2: D1 显式排除;⚠️ 同上 |
| 38 | WOWrInv | S | Unknown | - | self-derived | true |  | R-2: 同上 |
| 39 | WOWrInv | E | Unknown | - | self-derived | true |  | R-2: 同上 |
| 40 | WOWrInv | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 41 | WrInv | I | Unknown | - | self-derived | true |  | R-2: D1 显式排除;⚠️ 同上 |
| 42 | WrInv | S | Unknown | - | self-derived | true |  | R-2: 同上 |
| 43 | WrInv | E | Unknown | - | self-derived | true |  | R-2: 同上 |
| 44 | WrInv | M | Unknown | - | self-derived | true |  | R-2: 同上 |
| 45 | DirtyEvict | I | Unknown | - | self-derived | true |  | R-2: I 态无脏数据可逐出 |
| 46 | DirtyEvict | S | Unknown | - | self-derived | true |  | R-2: S 态无脏数据 |
| 47 | DirtyEvict | E | Unknown | - | self-derived | true |  | R-2: E 无脏数据(D1 的 M+DirtyEvict 路径依赖 dirty payload ⇒ 不可移植) |
| 48 | DirtyEvict | M | Known | I | public-direct | false | D1 p.9 Table 2 ModifiedEvict1 / MIAGO_WritePull1 | M + Evict ⇒ DirtyEvict;收 GO_WritePull 后回 Data 并转 I |
| 49 | CleanEvict | I | Known | I | public-direct | false | D1 p.8 §5.1 | 逐字:Subsequent Evicts have no effect ... already in invalid |
| 50 | CleanEvict | S | Known | I | public-direct | false | D1 p.9 Table 1 SharedEvict1 / SIA_GO_WritePullDrop1 | S + Evict ⇒ CleanEvict ⇒ SIA ⇒ 收 GO_WritePullDrop 转 I |
| 51 | CleanEvict | E | Known | I | public-direct | true | D2 p.10 | D2 分类式表述 clean Data (E or S state) 给合法性;⚠️ 目标态为自明语义推断;⚠️ **D2 许可 absent** ⇒ pending |
| 52 | CleanEvict | M | Unknown | - | self-derived | true |  | R-2: M 态走 dirty 变体;无来源禁止 CleanEvict |
| 53 | CleanEvictNoData | I | Known | I | public-direct | false | D1 p.4 | 定义式:同 CleanEvict 但无数据 ⇒ 继承其 I 格 |
| 54 | CleanEvictNoData | S | Known | I | public-direct | false | D1 p.4 | 继承 CleanEvict 的 S 格 |
| 55 | CleanEvictNoData | E | Known | I | public-direct | true | D2 p.10 | 继承 CleanEvict 的 E 格(⚠️ D2 许可 absent ⇒ pending) |
| 56 | CleanEvictNoData | M | Unknown | - | self-derived | true |  | R-2: 继承 CleanEvict 的 M 格 |
| 57 | SnpInv | I | Unknown | - | self-derived | true |  | R-2: D1 显式不建模 I 态收 snoop(RspIHitI is not used)⇒ **未知,非非法** |
| 58 | SnpInv | S | Known | I | public-direct | false | D1 p.5 Fig.4 SharedSnpInv1 | 回 RspIHitSE(S 干净 ⇒ 不带数据) |
| 59 | SnpInv | E | Known | I | public-direct | false | D1 p.5 通用义务 + RspIHitSE 定义含 E | E 干净 ⇒ 回 RspIHitSE 且不带数据 |
| 60 | SnpInv | M | Known | I | public-direct | false | D1 p.5 | 逐字:from M to I and is also forwarding its dirty data(RspIFwdM) |
| 61 | SnpData | I | Unknown | - | self-derived | true |  | R-2: 同 SnpInv 的 I 格(D1 显式不建模) |
| 62 | SnpData | S | Known | S | public-direct | false | D1 p.5 | 值域 {S,I};干净行无需失效 ⇒ 取 S(⚠️ S-vs-I 选择为推断,降级本身 direct) |
| 63 | SnpData | E | Known | S | public-direct | false | D1 p.5 | 同上;E 干净 ⇒ 不回数据 |
| 64 | SnpData | M | Known | S | public-direct | false | D1 p.5 | 逐字:from M to S and is also forwarding its dirty data(RspSFwdM) |
| 65 | SnpCurr | I | Known | I | public-direct | false | D1 p.5 | 不改态 |
| 66 | SnpCurr | S | Known | S | public-direct | false | D1 p.5 | 不改态 |
| 67 | SnpCurr | E | Known | E | public-direct | false | D1 p.5 | 不改态 |
| 68 | SnpCurr | M | Known | M | public-direct | false | D1 p.5 | 不改态;脏则回数据 |
<!-- CXL-TABLE:MATRIX:END -->

---

## 2. 判据与反向 fixture

| 判据 | 内容 | 反向 fixture(必须**因正确原因红**) |
|---|---|---|
| **CSS-06 ①** | 载体与本表**逐格一致** | 手改载体任一格 ⇒ `--check` rc=1 |
| **CSS-06 ②** | 载体变更未重生成本表 ⇒ 红 | 改载体后直接跑 `--check` ⇒ rc=1 |
| **CSS-06 ③** | 摘要 SHALL **不**整段复制任何来源原文 | 摘要出现 > 200 字符的连续引文 ⇒ ctest 红 |
| **AC-3 / AC-9** | 每格 `ref` 解析到摘要 §8 真实条目号;`note` 的 `R-n` 解析到摘要 §6 真实编号 | 摘要删掉 `D17` 行 ⇒ 引用它的格变孤儿 ⇒ 生成器 rc≠0 |
| **AC-9** | 覆盖率 SHALL 与**逐格实际统计**相等 | 改载体硬编码的覆盖率常量 ⇒ ctest 红 |

---

## 3. 使用纪律

- ⛔ 本表**不得**整段复制 CXL 规范正文(合规禁令,per `ADR-CEP-11` Status Update ⑤)——
  `ref` **只指向公开非规范来源**的条目号(`D1`~`D18`)。
- ⚠️ `Unknown` 格**不是**「非法」⇒ 消费者 SHALL 走**显式降级**路径(保守行为 + 事件登记),
  SHALL NOT 静默当作 `Illegal`。
- ⚠️ `E` 列的 D1 证据量为**零**(D1 把 E 并入 M)⇒ `E` 格只接受**态无关**义务或独立 E 证据,
  **严禁**把 `M` 规则复制成 `E` 规则。

---

**关联**:台账 `obj-cep-platform.md` Entry #122 / #125 / #126 ·
`ADR-CEP-11` 的 `## Status Update(2026-11-01)` ·
`docs/architecture/07-coherence-exploration-platform.md` §5.1.1(CXL 行,措辞待实现层对齐)
