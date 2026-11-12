# tasks —— `cpptlm-cep-full-green-run-evidence`

> 顺序 = **A0(授权门) → A1(前置核对 P-2/P-3) → A2(挂载点 + 内存样本路径定位) → A3(场景扩写 + 检测器挂载) → A4(真跑 + 4 件套) → A5(IC3 通过态 + 报告) → A6(证据登记 + 五步链) → A7(DoD 读法) → A8(新闸 + 变异) → A9(0 回归 + 归档)**。
> 门:**G1 = A0** · **G2 = A3**(违例类被实测)· **G3 = A5**(IC3 通过态)· **G4 = A6**(证据登记)· **G5 = A8**(新闸有牙)· **G6 = A9**(0 回归 + 归档)。
> ⚠️ **A1/A2 是「核对与定位」,⛔ 未完成不得改代码**(design §D-2)。

## A0 · 授权门(G1)
- [ ] A0.1 台账 Entry 授权实施(占 1 slot,KPI **0/3 → 1/3**)
- [ ] A0.2 improvement + architect 方案已落盘
- [ ] A0.3 四件套已建 + `openspec validate --strict` rc=0
- [ ] A0.4 基线实测:两个已有 run 的 `memory_violations` = `not-collected`、`deadlock_violations` = 0(复核 design §D-1)

## A1 · 前置核对(⛔ 不许跳过 · design §D-2)
- [ ] A1.1 **P-2**:读 `ADR-CEP-06` retention 条款 ⇒ 结论落盘(「4 件套齐备但 `trace.h5` 不入库」是否允许)
- [ ] A1.2 **P-3**:读 `obj §6.2` B4 段 + `ADR-CEP-06` SU#4 §① ⇒ 「全绿 run」的完整判据范围落盘
- [ ] A1.3 ⛔ 若 P-2 / P-3 任一不成立 ⇒ **停**并回报(⛔ 不许自行发明口径)

## A2 · 挂载点 + 内存样本路径定位(⛔ 只读)
- [ ] A2.1 定位 harness 的 `violations` 填充链 + **检测器挂载点**
- [ ] A2.2 定位 E2E 场景里**真实内存读写**的产生路径(`stub_cache_core` 的 `memory_read/memory_write` 等)
- [ ] A2.3 判定「真实 run」与「合成 fixture」在**文件形态**上是否可区分(⇒ design §D-5 的 G4 可判性)
- [ ] A2.4 ⛔ 若 A2.3 为否 ⇒ **停**并回报(⛔ 不得用「我说是真的」作判据)

## A3 · 场景扩写 + 检测器挂载(G2)
- [ ] A3.1 目标场景**真的产生内存操作样本** ⇒ 检测器**有样本可检**
- [ ] A3.2 检测器**挂上**且**实际参与计数** ⇒ 输出**实测数值**
- [ ] A3.3 ⛔ **不得**删 `not_collected` 的键而不接产出者(裸 0 假绿,Entry #222 ④)
- [ ] A3.4 ⛔ 不改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json`

## A4 · 真跑 + 4 件套
- [ ] A4.1 用 `cep_experiment_run.sh` / `tools/run_sweep.py` 跑出目标 run 目录
- [ ] A4.2 4 件套按 **A1.1 的结论**齐备(⚠️ `trace.h5` 的 retention 逐字遵守)
- [ ] A4.3 `cep_experiment_run.sh` 的 4 件套校验绿

## A5 · IC3 通过态(G3)
- [ ] A5.1 两违例类实测值 **== 0**(⚠️ **> 0 ⇒ 停止**并按缺陷另立处置 —— 硬边界 1)
- [ ] A5.2 `report.html` 渲染「**✓ 一致性闸门通过**」
- [ ] A5.3 `cep_ic3_report_states_check.sh` 绿

## A6 · 证据登记 + 五步链(G4)
- [ ] A6.1 `07 号` **新 SU**(插在 `## §末 文档索引` **之前**;⛔ 不改正文)⇒ 索引重生成
- [ ] A6.2 五步链:Entry 先落 → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验
- [ ] A6.3 冻结清单 provenance 成立

## A7 · DoD 读法
- [ ] A7.1 `obj §6.2` B4 证据行**追加**(append-only)「IC3 在**真实 run** 上达成通过态」+ 指向新 SU 与 run 目录
- [ ] A7.2 `cep_docs_sync_check.sh --strict` 绿

## A8 · 新闸 + 变异(G5)
- [ ] A8.1 `scripts/test/cep_full_green_run_check.sh`:G1~G5
- [ ] A8.2 ≥3 条决定性变异:① 违例类改回 `not-collected` ⇒ `违例类未实测` ② 实测值改非 0 ⇒ `违例类非零` ③ run 换成 fixture ⇒ `合成 fixture 冒充真实 run` ④ 移除 `07 号` 新 SU ⇒ `证据登记缺失`
- [ ] A8.3 全部**按签名红** + 还原 ⇒ 绿;真实仓字节指纹前后相等
- [ ] A8.4 `test/CMakeLists.txt` +1 ⇒ ctest **175 → 176**;`scripts/README.md` 守门表 **36 → 37**

## A9 · 0 回归 + 归档(G6)
- [ ] A9.1 `ctest --test-dir build -j4` **176/176**
- [ ] A9.2 **42** 个 `scripts/test/cep_*.sh` 逐个 rc=0
- [ ] A9.3 format / docs_sync / doc_ref / agents_md_lint / freeze 全 rc=0
- [ ] A9.4 `openspec archive`(✅ 已完成归档语义)⇒ delta **ADDED** 合并进 `openspec/specs/cep-full-green-run-evidence/`
- [ ] A9.5 KPI 活口径 **1/3 → 0/3**

## ⛔ 明确不做
- ⛔ 为凑 IC3 掩盖真实违例 · ⛔ 用合成 fixture 冒充真实 run · ⛔ 删 `not_collected` 的键而不接产出者 · ⛔ 改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款 · ⛔ 改 `B3b-2` · ⛔ 新建 ADR · ⛔ 动 `§4.0`
