# tasks —— `cpptlm-cep-iag-gate-execution-arm`

> 顺序 = **A0(授权门) → A1(共用加载器) → A2(离线 CLI) → A3(装配路径臂) → A4(target 第二段) → A5(真装配测试) → A6(对账闸) → A7(ctest + 守门表) → A8(0 回归) → A9(归档 + KPI 回填)**。
> 门:**G1 = A0** · **G2 = A3**(装配臂有牙)· **G3 = A6**(对账闸有牙)· **G4 = A8**(0 回归)· **G5 = A9**(归档)。

## A0 · 授权门(G1)
- [ ] A0.1 台账 Entry #379 授权实施(占 1 slot,KPI **0/3 → 1/3**)
- [ ] A0.2 improvement + architect 方案已落盘
- [ ] A0.3 change 四件套已建 + `openspec validate --strict` rc=0

## A1 · 共用加载器
- [ ] A1.1 `include/tlm/cep/instance_assembly_loader.hh`:`load_instance_assembly_inputs` + 加载诊断(source_error / skip / planes_unavailable)
- [ ] A1.2 路径解析口径与 `parse_channel_declaration_from_config` **逐条对齐**
- [ ] A1.3 ⛔ `instance_assembly_gate.hh` **零 diff**

## A2 · 离线 CLI
- [ ] A2.1 `examples/cep_instance_assembly_gate.cc`:config 路径参数 + 退出码 `0/1/2`
- [ ] A2.2 9 份 CEP config 逐个 ⇒ 8 PASS + 1 SKIP(`e2e_a`)

## A3 · 装配路径臂(G2)
- [ ] A3.1 `module_factory.cc` **Step 0.26** 调 `run_instance_assembly_gate` ⇒ FAIL ⇒ `return false`
- [ ] A3.2 errors 走 **stdout**(与 Step 0.25 同通道)⇒ 可按签名捕获
- [ ] A3.3 臂位置早于 **Step 2**(模块创建)⇒ 零副作用

## A4 · target 第二段
- [ ] A4.1 `CPPTLM_CEP_VALIDATION_CONFIGS` = 9 份 `configs/cep_*.json`
- [ ] A4.2 `validate_topology` 命令面含 `${CPPTLM_CEP_VALIDATION_COMMANDS}`
- [ ] A4.3 `cmake --build build --target validate_topology` ⇒ IAG CLI 实跑 **9** 次

## A5 · 真装配测试
- [ ] A5.1 `test/test_cep_iag_execution_arm.cc`:dup2 捕获 stdout
- [ ] A5.2 正控绿 ⇒ `[IAG-02]` 红 ⇒ 还原绿;`[IAG-06]` 变体
- [ ] A5.3 装配路径与装配闸结论一致(禁双实现证明)

## A6 · 对账闸(G3)
- [ ] A6.1 `scripts/test/cep_iag_gate_reconcile_check.sh`:C1~C7
- [ ] A6.2 M1(破坏 config ⇒ `[IAG-02]`)· M2(摘掉 IAG 段 ⇒ C6 红)
- [ ] A6.3 真实仓字节指纹前后相等

## A7 · ctest + 守门表
- [ ] A7.1 `test/CMakeLists.txt` +1 `add_test`(对账闸)⇒ ctest **172 → 174**
- [ ] A7.2 `scripts/README.md` 守门表 +1 行 ⇒ **34 → 35**

## A8 · 0 回归(G4)
- [ ] A8.1 `ctest --test-dir build -j4` **174/174**
- [ ] A8.2 **40** 个 `scripts/test/cep_*.sh` 逐个 rc=0
- [ ] A8.3 `format.sh --check` / `docs_sync_check.sh --strict` / `cep_doc_ref_check.sh --strict` / `agents_md_lint.py` 全 rc=0

## A9 · 归档 + KPI 回填(G5)
- [ ] A9.1 `openspec archive`(✅ 已完成归档语义)⇒ delta **合并**进 `openspec/specs/cep-instance-assembly/spec.md`
- [ ] A9.2 KPI 活口径 7 处 **1/3 → 0/3**
- [ ] A9.3 归档件 tasks 回填(逐项核对实际文本后再勾)

## ⛔ 明确不做
- ⛔ 改判据口径 · ⛔ 改声明/平面 JSON 内容 · ⛔ 新建 ADR · ⛔ 改 `B3b-2` · ⛔ 加「体量数字」断言 · ⛔ 修 `nvlink_4` 覆盖缺口
