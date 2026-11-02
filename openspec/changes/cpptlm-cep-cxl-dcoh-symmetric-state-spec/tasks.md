# Tasks — cpptlm-cep-cxl-dcoh-symmetric-state-spec v1.0(Stage 2 · 第 0 个 change · 规约层 · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#12** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-cxl-dcoh-symmetric-state-spec/spec.md`](specs/cep-cxl-dcoh-symmetric-state-spec/spec.md)
> **口径**: 台账 **Entry #154**(4 项 architect 裁决)· **#153**(Task 0 普查)· **#122**(CXL 口径裁决)
> **审计面**: `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`
> **估时**: 5 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 本 change = **规约层**;第 1 个 change = `cpptlm-cep-cxl-dcoh-symmetric`(实现层)

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-02) | 台账 Entry #154(4 项裁决 + 3 件冻结面授权)· improvement 草稿 HARD-GATE rc=0 · 4 件套已建 · **转正裁决 Entry #155** · **冻结面实测 = 0 件**(0.6)· **KPI 回填 8 处**(0.7) |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-02) | `cxl_dcoh_state_spec.hh`(新)—— 3 套状态空间 + 6+3 通道 + `{BISnp}` + `perm` 按域分离 + 5 谓词定义 + **18 条**编译期断言(带可归因 id) |
| 2 逐格 basis + 编译期锁 | ✅ **已完成**(2026-11-02) | ✅ **28 格**逐格 `basis`(两轴)+ **来源许可表**(6 来源)+ **19 条** CDS-06 编译期锁 + **10 项语义 pending** 登记(CDS-07)+ 运行期判据 **7 cases / 291 assertions**;⬜ **4 组反向 fixture** 归 Phase 3 |
| 3 判据收口 | ⬜ 待实施 | 逐格核对表 + 4 组反向 fixture + `pending` 10 项登记 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + KPI 1/3 → 0/3 + 归档 |

---

## Phase 0 — 治理前置

- [x] 0.1 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(per Entry #153)⇒ **有条件通过**(4/5 项可结构钉死;**零规范正文暴露**)
- [x] 0.2 **architect 4 项裁决**(per Entry #154):聚合点二元 ⇒ `HOME` · `DcohSymmetric` traits ⇒ `HOME` · 07 号 **6 处**口径订正 · adapter 形态 ⇒ 独立 `CxlDcohAdapter`
- [x] 0.3 **improvement 草稿**落 `.rddf/improvements/cpptlm-cep-cxl-dcoh-symmetric.md` ⇒ `pre_create_brainstorm_check.sh` **rc=0**
- [x] 0.4 **4 件套建档**(proposal + design + delta spec + tasks)
- [x] 0.5 **转正裁决**(per §4.7.2 step 1:DRI 裁决「① 建 change」)⇒ 台账 Entry 登记
- [x] 0.6 **冻结面实测确认**:新增载体 `include/tlm/cep/cxl_dcoh_state_spec.hh` 不在 M1 冻结集内 ⇒ **0 件** ⇒ 无需两步授权
- [x] 0.7 **KPI baseline 回填**:7 处(`obj §5.2` / `obj §5.3` / `obj §10` / `phase-10.md` / `roadmap.md` / `AGENTS.md` / umbrella improvement)

---

## Phase 1 — 载体形态定稿

- [x] 1.1 **3 套状态空间**:`DevCacheState`(4)· `DcohViewState`(3)· `HomeDirState`(3)—— **按域**互斥,**不**按名字
- [x] 1.2 **通道词表**:CXL.mem **6**(枚举名)+ CXL.cache 双向各 3
- [x] 1.3 ⭐ **`device_initiated_snoop_set == {BISnp}`**
- [x] 1.4 **`perm` 投影**:仅从 `DevCacheState` → 4→4;`A` 显式标「不可单值投影」;**维持 4 值锁**
- [x] 1.5 **窄义 5 谓词**「对称」定义(常量 + 判别式)
- [x] 1.6 **零既有代码改动**自查(去注释后文本不含 `State::`/`Event::`/`Perm::` 之外的越界记号 —— 载体**是**真源 ⇒ 该约束**只**适用于第 1 个 change 的 adapter)

---

## Phase 2 — 逐格 basis + 编译期锁

- [x] 2.1 **逐格 `basis{kind, pending, ref}`** ⇒ `ref` 指向锚点摘要条目号(**不**指向规范原文)
- [x] 2.2 **两轴规则**:`pending=false` SHALL 同时满足 `pin=strong` **且** 许可已人工确认(本轮仅 S5 / S7 可支撑)
- [x] 2.3 **编译期锁**(V-1 ~ V-6)+ **11 条** `static_assert`
- [x] 2.4 **TDD 红 ⇒ 绿**:先写判据用例 ⇒ 在**未实现**载体上必红

---

## Phase 3 — 判据收口

- [x] 3.1 **逐格 basis 核对表**(机器可解析:目标掩码 + 分桶独立成列)
- [x] 3.2 **4 组反向 fixture**:① 合并状态空间 ⇒ 红 ② `device_initiated_snoop_set` 置空 ⇒ 红 ③ 5 谓词置假 ⇒ 红 ④ 去掉 `pending` 标记 ⇒ 红 —— **每条须因正确原因红**(铁律 ②)
- [x] 3.3 **10 项 `pending`** 逐条登记 + **禁止推理填空**的可判定断言
- [x] 3.4 **反向 fixture 形态**登记:本组为「**毒化 ⇒ 必红**」(鉴别力)⇒ 与队列 #11 修误报的那一组(「毒化 ⇒ 仍绿」)**形态相反**,两者都要登记

---

## Phase 4 — 验收 + 归档

- [ ] 4.1 `ctest` 全绿(含 `[cep][cxl-dcoh]` 新用例)
- [ ] 4.2 `openspec validate cpptlm-cep-cxl-dcoh-symmetric-state-spec --strict` rc=0
- [ ] 4.3 门禁全 rc=0 + `format.sh --check` rc=0
- [ ] 4.4 KPI **1/3 → 0/3** + 7 处 baseline 同步
- [ ] 4.5 **完成归档** ⇒ delta capability 合并进 `openspec/specs/cep-cxl-dcoh-symmetric-state-spec/spec.md`
- [ ] 4.6 **移交**:规约层归档后即可建第 1 个 change(实现层 `cpptlm-cep-cxl-dcoh-symmetric`)
