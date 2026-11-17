# tasks: cpptlm-router-buffer-size-param

## T0 — 立项与判据

- [x] T0.1 improvement 5 段草稿（`.rddf/improvements/cpptlm-router-buffer-size-param.md`）
- [x] T0.2 change 目录建立（`proposal.md` / `design.md` / `tasks.md` / `.openspec.yaml` + 1 份 delta spec）
- [x] T0.3 **默认值裁决写进 `design.md` D-1**（取 **8** = 让声明反映实际 ⇒ 零行为变更；含三条理由 + 被否选项 B 的后果）
- [ ] T0.4 architect 复核 D-1 裁决（可选 · 若认为「取 16」更优 ⇒ 需改 D-1 + AC-02 + AC-05 三处口径）

## T1 — 实现

- [x] T1.1 `include/tlm/router_tlm.hh`：新增运行时成员 `unsigned buffer_depth_`（初值 = `BUFFER_DEPTH`，常量语义改为「默认深度」）
- [x] T1.2 `src/tlm/router_tlm.cc` · `on_config_loaded`：读取 `buffer_size`（若存在）⇒ 校验类型 + 范围 1–64 ⇒ 写入 `buffer_depth_`；**非法 ⇒ 装配期硬错误**（⛔ 不得静默截断 / 静默回退）
- [x] T1.3 `src/tlm/router_tlm.cc` · **4 个使用点**改读 `buffer_depth_`：`:59`（credit 初值）· `:154`（输入缓冲容量判定）· `:485`（`receive_credit` 上限）· `:530`/`:535`（`credit_safety_reset`）
- [x] T1.4 **构造顺序**（per D-2「同源」）：先设 `buffer_depth_`，**再**按它初始化 `downstream_credits_` ⇒ 否则 credit 预算与缓冲深度不同源（AC-03 必红）
- [x] T1.5 `src/tlm/router_tlm.cc` · `get_param_rules`：`buffer_size` 默认值 **16 → 8**
- [x] T1.6 `configs/param_rules/router_tlm.json` **补** `buffer_size` 条目（`default_int: 8` / 1–64）—— 原「零 diff」判断已由 **D-7.2** 推翻
- [x] T1.7 ⛔ 确认零改动：`vc_count` 声明 · `NUM_VCS` / `NUM_PORTS` · `input_buffer_` / `downstream_credits_` 的**容器类型** · `RouterPort` 枚举 · `CohIfNICTLM` / `NICTLM` / `module_factory.cc`

## T2 — 测试

- [x] T2.1 **AC-01**：设 `buffer_size: N` ⇒ 输入缓冲**实际容量 == N**（注入 N 个 flit 全收；第 N+1 个被丢）
- [x] T2.2 **AC-02**：**未设** `buffer_size` ⇒ 深度 == 8 + `downstream_credits_` 初值 == 8 + 既有断言**零变化**
- [x] T2.3 **AC-03**：设 `buffer_size: N` ⇒ `downstream_credits_[p][v]` 初值 == N（credit 预算与深度**同源**）
- [x] T2.4 **AC-04**：`buffer_size` = 0 / 65 / 非整数 ⇒ **装配期 fail**（`instantiateAll` 返回 false；⛔ 不得静默截断、⛔ 不得回退默认值而通过）
- [x] T2.5 **AC-05**：`[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` 全绿；**既有 `REQUIRE` 常量零改动**（若出现数值变化 ⇒ per D-4 逐条重推导，⛔ 禁止就地改常量）
- [x] T2.6 **G-1 / G-2**：`vc_count` 声明与实际值**保持现状** + `NUM_VCS` / `NUM_PORTS` 仍为编译期常量

## T3 — spec

- [x] T3.1 delta spec · ADDED：`router-buffer-size-param`（3 个 Scenario：生效 / 非法值 fail / 默认值一致性）
- [ ] T3.2 归档时 `openspec archive` 合并 delta + 新建 main spec（⛔ **不用** `--skip-specs` —— 实现已完成）
- [ ] T3.3 归档时更新冻结清单（`cep_freeze_check --update --authorized-by "Entry #NNN"`）

## T4 — 验证

- [x] T4.1 `cmake --build build` 无 error
- [x] T4.2 新单测全绿 + `ctest -R cep` 全绿
- [x] T4.3 `cep_docs_sync_check` / `cep_doc_ref_check` 全通过
- [x] T4.4 遗留清扫：无 TODO / 临时诊断 / skip 测试 / 未跟踪残渣 / 注释掉的代码

## T5 — 收口

- [x] T5.1 台账 Entry（实施记录 + 改动面 + 实测）—— Entry **#469**
- [ ] T5.2 KPI 回填 **1/3 → 0/3**（归档时 · 6 处活口径）
- [ ] T5.3 归档（`openspec archive`，⛔ **不用** `--skip-specs`）
- [x] T5.4 独立验收（切 verifier）—— **PASS**，报告 `.rddf/reviews/cpptlm-router-buffer-size-param-verify.md`（AC-01~AC-05 + G-1/G-2 全通过 · 0 implementation_gap · 4 项观察）

## T6 — 待 architect（跨角色）

- [ ] T6.1 （可选）ADR 层面登记「**参数声明-实际一致性**」的通用纪律 —— 本 change 的根因是「`get_param_rules()` 声明了但 `on_config_loaded()` 不读」，而 `vc_count` 是**同族第二例** ⇒ 是否需要一条「声明即契约」的 ADR 决策（⛔ 非本 change 阻塞项）
