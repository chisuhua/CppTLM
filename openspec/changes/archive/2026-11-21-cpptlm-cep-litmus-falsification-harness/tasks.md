# tasks — cpptlm-cep-litmus-falsification-harness

- [x] G1 `include/tlm/cep/litmus_falsifier.hh` —— 复合图构造 + 环搜索 + 判定结构（`reachable` / `unreachable_reason` / `compound_edges` / `classes_seen` / `cycles` / `falsified` / `coverage_note`；⛔ 无「已证明」一档）
- [x] G2 `test/cep_litmus_corpus.json` —— 语料（**当前空**）+ `_status` 写明「边类两仓均无定义 ⇒ 复合公理当前不可证伪」+ `_fill_when` 写明须由结构化声明项派生填充
- [x] G3 `test/test_cep_litmus_falsification.cc` —— 4 例：`[generic]`（语料无关的诚实性 + 判定产物写出）/ `[repo-corpus]`（仓内正控）/ `[fixture]`×2（跨域环证伪 + 端点未绑定不自证）
- [x] G4 `scripts/test/cep_litmus_falsification_check.sh` —— 闸（臂① 正控诚实性 + 臂② 反向 fixture 有鉴别力）+ `test/CMakeLists.txt` ctest 注册
- [x] G5 `specs/cep-litmus-falsification/spec.md` —— delta spec（新增 capability）

## 验收记录（2026-11-21）

| 判据 | 结果 |
|---|---|
| AC-1 | 工装可编译并给出**真结果**：`[cep][litmus]` **4 cases / 30 assertions 全绿**；仓内语料 ⇒ `reachable=false` + 具体原因 + `coverage_note` 非空 |
| AC-2 | 反向 fixture：跨域环语料 ⇒ `falsified=true` 且环文本**含两个网络** ⇒ 工装有牙 |
| AC-3 | 闸两臂全绿：`bash scripts/test/cep_litmus_falsification_check.sh` rc=0（正控诚实 + 反 fixture 翻转）；ctest `cep_litmus_falsification_check` Passed |
| AC-4 | 范围纪律：零生产代码改动（仅新增 header / 测试 / 闸 / 语料 / delta spec）；`channel_dependency_graph.hh` 零 diff；`protocol_instance.schema.json` 零 diff；任何已签批 ADR 零 diff |
| AC-5 | 0 回归：`ctest` 全绿 · 46 个 `cep_*.sh` 全 rc=0 · `rddf_stage_gate` 三路径 rc=0 |
