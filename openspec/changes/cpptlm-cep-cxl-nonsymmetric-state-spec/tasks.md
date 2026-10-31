# Tasks — cpptlm-cep-cxl-nonsymmetric-state-spec v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#10** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-cxl-nonsymmetric-state-spec/spec.md`](specs/cep-cxl-nonsymmetric-state-spec/spec.md)
> **口径**: 台账 **Entry #122** + `ADR-CEP-11` Status Update(2026-11-01)
> **普查**: `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`(Task 0 已完成 ⇒ 硬门**有条件通过**)
> **估时**: 4 周(规约层)· **KPI**: Proposed 并发 **0/3 → 1/3**

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-01) | 口径裁决签发(Entry #122 + ADR Status Update)· Task 0 公开来源普查(硬门有条件通过)· change 目录 4 件套已建 |
| 1 钉版补齐 | 🟡 **有条件完成**(2026-11-01) | **7 `pin=strong`**(sha256 / D14 commit SHA)/ **1 `pin=weak`**(D3,Cloudflare 403 无哈希)/ **6 `pin=transcribed`**(live 403 + archive.org 全站不可达)⇒ 摘要 §8.1 · ⚠️ 新增载体规则「只有 `pin=strong` 可 `public-direct` + `pending=false`」 |
| 2a 规约载体(结构层) | ✅ **已完成**(2026-11-01) | `cxl_state_spec.hh`:4 态 / 事件词表 / 4→4 投影 / 秩 / 3 条结构断言 / `basis` 4 档 / 8 项残余风险;`test_cep_cxl_state_spec.cc` 8 cases / 48 assertions 全绿(含 5 条反向 fixture) |
| 2b 规约载体(转换表) | ✅ **已完成**(2026-11-01) | 68 格**三态格语义**:`Known` **26** / `Illegal` **0** / `Unknown` **42**;`pending=false` **22** 格(全 D1);覆盖率 `26/68` + 逐格 `basis`;`transition()` 对 `Unknown` 返回显式态 |
| 3 判据收口 | ⬜ 待实施 | 契约测试 + 反向 fixture + 核对表生成器 + `--check` |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + 零冻结面 diff + 归档 |

---

## Phase 0 — 治理前置 ✅

- [x] 0.1 **口径裁决签发**(architect)⇒ 台账 **Entry #122**(含授权记录)+ `ADR-CEP-11` `## Status Update(2026-11-01)`(7 条裁决)
- [x] 0.2 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`
      ⇒ **有条件通过**(态数 / 事件词表 / 投影形状 / 非对称定义 / 编译期断言 **5 项已钉死**;8 项 `pending`)
- [x] 0.3 **improvement 草稿修订**(10 大项 / 16 编辑点)⇒ `.rddf/improvements/cpptlm-cep-cxl-nonsymmetric.md`
- [x] 0.4 change 目录 4 件套已建 + `openspec validate --strict` rc=0
- [x] 0.5 **冻结面两步授权闭环**(针对 `ADR-CEP-11` Status Update):台账 entry → `--update --authorized-by "Entry #122"` → commit → `--update`(provenance 对齐)→ commit

## Phase 1 — 钉版补齐(⚠️ 阻塞载体**逐格**编码)

> **状态**:🟡 **有条件完成**(2026-11-01)—— 7 `pin=strong` / 1 `pin=weak` / 6 `pin=transcribed`
> ⇒ 按摘要 **§8.2 两轴规则**载体逐格编码**已可启动**;下列 1.6~1.8 为**新增前置**(可与载体并行)。

- [x] 1.1 6 个 PDF 的 **sha256** + 字节数 —— ✅ **5/6 取得**(D1 / D2 / D4 / D17 / D18);
      ⚠️ **D3 取不到**(Cloudflare JS 挑战 403,4 个路径变体全 403)⇒ **无哈希**,如实登记
- [x] 1.2 7 个 blog / 网页类来源的 Wayback 快照 —— ❌ **0/7 取得**(archive.org **全站不可达**:
      curl + webfetch 双通道 · 80/443 · 120s 全超时;`arquivo.pt`/`timetravel`/`archive.ph`/`archive-it` 亦不可达)
      ⇒ 全部降 `pin=transcribed`;
      ⚠️ **D10 例外**:该 URL 本身即**静态内容寻址 PDF** ⇒ 取字节 + sha256 成功,已升 `pin=strong`(无需 Wayback)
- [x] 1.3 D14 Isabelle artifact 的 **HEAD commit SHA** —— ✅ `f729052e1032980561f2c0cc5e341859228ded26`
      (2026-09-27,31 commits,走 **GitHub API**,未 clone);⚠️ **license = null**(未声明)·
      ⚠️ artifact **≠ 论文快照**(含 ASPLOS'25 之后的 guard 改动 + `Liveness/` 扩展)
- [x] 1.4 摘要 §8 钉版状态列已更新为**分级**(`pin=strong`/`weak`/`transcribed`)
- [x] 1.5 ⚠️ **诚实登记**已执行:取不到的项如实记「取不到 + 原因」,**零编造哈希**
- [ ] 1.6 **D1 / D2 重锚「H2D Req 只含 snoop」**(二者 `pin=strong` 且字节已在本地
      `/tmp/opencode/cxlpin/A.pdf` / `B.pdf`)⇒ 找到等价表述则该格由 `pending=true` 翻 `false`;
      **复核不出则维持 `pending=true`**(不影响结构断言锁定效力)
- [ ] 1.7 **D1 重锚 `E + 写 → M` 静默升级**(现为 `analogical`,摘要 §4)⇒ 找到对应规则则升 `direct`
- [ ] 1.8 **许可确认**:D14(`license = null`)与 D4(© Consortium,「引用需节制」)的
      引用 / 再分发范围人工确认 ⇒ 未确认前**不得** `pending = false`(摘要 §8.2 许可必要条件)

## Phase 2 — 规约载体(TDD 5-step)

- [x] 2.1 **先写红**:测试骨架(`test/test_cep_cxl_state_spec.cc`)+ 反向 fixture
      (① 加回 `F` 行 ⇒ 必红 ② 毒化 `H2DReqOp` 集合 ⇒ 必红 ③ 强度升格 ⇒ 必红 ④ 删头注释消歧 ⇒ 必红)
- [x] 2.2 `include/tlm/cep/cxl_state_spec.hh` 骨架:`kStates`(4 态)/ `kEvents`(枚举名)/ `kPermProjection`(4 行)
- [x] 2.3 **转换表(三态格语义,per `design.md D-γbis`)**:每格 ∈ {`Known(target,basis)`,
      `Illegal(basis)`, `Unknown(pending → R-2)`};⚠️ `Unknown` SHALL **显式区分**于 `Illegal`,
      **不复用**「未定义空洞值」;逐格 `basis{kind, pending, ref, note}`,`ref` **只指向锚点摘要条目号**,
      `pending` 格 `note` **带 R-1~R-8 编号**
      ⇒ **实测结果**:`Known` 26 / `Illegal` 0 / `Unknown` 42 —— ⚠️ `Illegal` 恒 0 是**来源不给**
      (D1 p.7 证实规范存在该类约束但不说哪些;逐状态合法性表在 §3.2.5.15,禁读)⇒ 填任何 `Illegal` 即杜撰
- [x] 2.3b **覆盖率**:`kLegalTransitionCoverageNum` / `Den`(已确证格 / 68)+ 测试断言与逐格统计**相等**
       ⇒ 台账每次推进复核**单调不减**(回退必红 + 须授权)
- [x] 2.4 **编译期锁**:① 态数恰 4 ② 投影恰 4 行 ③ `F` 行不存在 ④ `H2DReqOp ⊆ {SnpInv,SnpData,SnpCurr}`
      ⑤ `D2HReqOp ∩ H2DReqOp = ∅` ⑥ 设备侧无 snoop 发起型事件 ⑦ `kind`/`pending` 一致
      ⑧ `public-analogical` / `self-derived` ⇒ `pending = true` ⑨ 头注释含「≠ dGPU SoC 实现路径」
      ⑩ **两轴不混读**:`pin=transcribed` 的格 ⇒ 降 `public-analogical` + `pending=true`
- [x] 2.5 **偏离清单** + **8 项残余风险登记**(与摘要 §6 逐项一致)
- [x] 2.6 跑绿 + **零冻结面 diff** 复核(`git diff --stat` 只含本 change 载体 + 测试 + 文档)

## Phase 3 — 判据收口

- [ ] 3.1 `scripts/test/generate_cxl_anchor_checklist.sh` + `.cc`:生成「载体 ↔ 摘要」逐格核对表(+ `--check`)
- [ ] 3.2 `--check` 模式接入 CI(手改核对表 ⇒ 必红;载体变更未重生成 ⇒ 必红;摘要与载体不一致 ⇒ 必红)
- [ ] 3.3 **反向 fixture 机器化**:每条 fixture 声明**期望失败模式** + 命中**指定的判据 id**(per `cep_mesif_adapter_reverse_verify.sh` 先例)
- [ ] 3.4 ctest 注册 + `ctest -R cep` 全绿
- [ ] 3.5 门禁:13 项 CEP 脚本 + `docs_sync --strict` + `doc_ref --strict` + `gate_selftest` + `format --check` 全 rc=0

## Phase 4 — 验收 + 归档

- [ ] 4.1 `ctest --test-dir build -j4` 全绿(**不排除任何测试**)
- [ ] 4.2 **零冻结面 diff** 断言 + `cep_freeze_check` provenance 通过
- [ ] 4.3 `openspec validate --strict` rc=0 + `openspec archive cpptlm-cep-cxl-nonsymmetric-state-spec`
      ⇒ delta spec 合并进 `openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`
- [ ] 4.4 台账追加 Entry(**append-only**):本 change 完成登记 + §4.0 队列 #10 行状态更新
      ⇒ KPI 并发 **1/3 → 0/3** + **6 处活文档 baseline 同步**(per Entry #121 的教训)
- [ ] 4.5 `git commit`(**不 push**)

---

## 非目标(Non-Goals)

| # | 不做 | 理由 |
|---|---|---|
| 1 | 不实现 adapter / `SparseSnoopFilter` 行为类 / 实例声明 / 装配 | 属**第 1 个** change;且 R11 归属已被 **Entry #112** 裁决去重 |
| 2 | 不扩 `perm` 4 值锁 / `CATEGORY_*` 3 类 / 不改 `CohIfBundle` / 不改冻结 schema | 平台级不变量 |
| 3 | 不新增 invariant 编号数 | 除非 ADR 显式新增并同步编号位 |
| 4 | 不建模 transient state | 公开来源明说其「not officially part of the CXL.cache specification」 |
| 5 | 不预支 CXL 3.0 的 BI 通道 / DCOH | 属队列 **#12** |
| 6 | 不产出任何性能结论 | CXL 非对称 workload 落在 **H7 声明范围之外** ⇒ R-B 失效区 |
| 7 | 不把 CXL 规范正文投喂任何 AI | ECA §3 + AI AUP(覆盖 past/current/future) |
| 8 | 不镜像实例声明进 ArchForge `32 号 §6.1` | 产品侧注册表 ⇒ 污染 + 与其立法冲突 |
