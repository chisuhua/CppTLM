# tasks: cpptlm-cep-tilelink-home-state-driver

> 单元 = 登记册 §6.4 队列 2 的 **I-7b**（父编号 `I-7`）· 形态/行内容由裁决 `.rddf/reviews/cep-i7b-adjudication-2026-11-21.md` 锁定（C-1 ~ C-5 / D-1 ~ D-3）。

## 1 实施

- [x] 1.1 新建 `include/tlm/cep/tilelink_home_state_driver.hh`（header-only · 消息簿记层 + 驱动边界观测）
- [x] 1.2 逐消息安装语义表 **恰 10 行**，行序 == 消息枚举值；`Cap` 行不入表 ⇒ 任何需 `Cap` 的输入恒 `Undecidable`
- [x] 1.3 安装解析**只读消费**规约层（`t3_consumable_event_mask` / `transition` / `mask_of`）⇒ 零自有映射表副本
- [x] 1.4 观测载体 `TlHomeObservation`（⛔ 不复用族 1 的 `EventObservation`：tracker 侧无引擎态迁移）
- [x] 1.5 观测面**默认关闭 + 有界**；溢出递增丢弃计数；丢弃非零 / 前提不成立 ⇒ 判不可判（⛔ 不记 0）
- [x] 1.6 安装台账（`addr → 位掩码`）⇒ 「他人 sharer」位可溯源；绕过驱动的位判 `provenance_broken`

## 2 判定级载体

- [x] 2.1 `test/test_cep_tilelink_home_driver.cc`：AC-1 ~ AC-13 逐条断言 + 反 fixture
- [x] 2.2 AC-14 ③ 载体内容判据：驱动确实经 `TreeManager::set_sharer` 装表（含删调用的影子反 fixture）
- [x] 2.3 三段分离：驱动只落原始事实，判定（`detect()`）全在 test 侧（族 1 先例）

## 3 工具侧（档 2 登记面 + 闸）

- [x] 3.1 `test/cep_evidence_tiers.json`：新增 `tilelink_c-observation`（`carrier_kind = "tracker-observation"`）
- [x] 3.2 `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」限定（既有文字逐字保留）
- [x] 3.3 `scripts/test/cep_e2e_reconcile_check.sh`：新增 **T8**（两表双向一致）+ **T9**（`carrier_kind` 封闭词表 + 两值互异）
- [x] 3.4 变异基座 **+2 条**（`j` = 条目族指向不适用族 ⇒ T8 红 · `k` = 载体形态撞值 ⇒ T9 红）· 计数 9 → 11 同步
- [x] 3.5 `DOC_UNITS` 追加新 SU 段（否则新 SU 不受 T6 保护）
- [x] 3.6 I-7a 的两处口径随授权扩面订正（`AC-7` 的 `size()==1` 计数 / `AC-8` 的禁改面含 `test/cep_evidence_tiers.json` 与 `docs/architecture`）

## 4 文档

- [x] 4.1 `docs/architecture/07-coherence-exploration-platform.md` 追加 SU（append-only · 索引按生成器重生成）
- [x] 4.2 delta spec：Requirement 主体首句改写（两载体形态）+ 新 Scenario（tracker 侧 per-installation）· 族 1 的 7 个 Scenario 逐字保留
- [x] 4.3 `openspec validate <change> --strict` rc=0

## 5 机关与收口

- [x] 5.1 载体表 `scripts/test/rddf_ac_carrier.tsv` 追加 I-7b 行（父编号 `I-7`）
- [x] 5.2 台账 Entry（编号 = 实测最大 + 1）
- [x] 5.3 冻结面五步链（`--update` ×2 + 清单单独 commit + 无参复验 · `artifact_count` 不变）
- [x] 5.4 全量回归（ctest + 全部 `scripts/test/cep_*.sh` + 机关三路径 + `docs_sync_check` + `agents_md_lint` + format）
- [x] 5.5 验收报告 `.rddf/reviews/cpptlm-cep-tilelink-home-state-driver-verify.md`
- [x] 5.6 归档（force-add + `archive_verify_baseline.txt` 34 → 35）+ 登记册 §6.4 I-7 行回填
