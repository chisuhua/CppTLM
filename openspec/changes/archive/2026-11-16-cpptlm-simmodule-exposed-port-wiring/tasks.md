# tasks: cpptlm-simmodule-exposed-port-wiring

> **AC 映射**: proposal AC-01 ~ AC-08 · design D-1 ~ D-6 · 决策源 `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`

## Phase 1 · 基础设施

- [x] **T1.1** `include/core/simple_port.hh`：新增 `bool isBound() const { return pair != nullptr; }`（public 段）
- [x] **T1.2** `src/core/module_factory.cc`：**Step 7c** 骨架（位置：Step 7b 之后）—— 遍历 `final_config["connections"]`，按 design D-2.1 做端点分类；两端都非 exposed ⇒ `continue`

## Phase 2 · 接线（design D-2.2 ~ D-2.4）

- [x] **T2.1** 后缀识别 + 方向表：`req_in`↔`req_out` / `resp_in`↔`resp_out` / `req_out`↔`req_in` / `resp_out`↔`resp_in`
- [x] **T2.2** 对端解析：`ch_adapters` 存在性 + 纯数字索引 + 越界检查
- [x] **T2.3** `isBound()` 守卫 → 冲突时硬错误
- [x] **T2.4** 建 `PortPair(master, slave)` + `setDelay(latency)`（initiator 侧）

## Phase 3 · 失败语义（design D-2.5）

- [x] **T3.1** 五类硬错误各自报错并**终止装配**（①未声明标签（在声明过 exposed 的容器上）②`getInternal*Port` 空 ③索引越界/非数字 ④后缀非四类 ⑤两端都 exposed）
- [x] **T3.2** ⛔ 复核：**未声明** exposed 的 SimModule（如 `pcie_ep`）走普通端口分支 ⇒ **不触发**任何硬错误（AC-08 红线）

## Phase 4 · 判据与回归

- [x] **T4.1** 新增 `test/test_simmodule_exposed_port_wiring.cc`（AC-01 ~ AC-04）
- [x] **T4.2** AC-05 全量：`[cep]` / `[e2e]` / `[mesh]` / `[simmodule]` + `ctest -R cep`
- [x] **T4.3** AC-08：Phase 8 E2E（`examples/dgpu_soc_with_pcie_ip.json`）+ `configs/library_cpu_demo.json` 可装配
- [x] **T4.4** AC-06：`git diff src/core/module_factory.cc` 确认 Step 6 段零改动
- [x] **T4.5** AC-07：层次化 `test/test_cep_mesh_e2e.cc` 绿（依赖另一 change 的 config 改造）

## Phase 5 · 收口

- [ ] **T5.1** delta spec 合入 `openspec/specs/simmodule-nested/spec.md`（MODIFIED 修正过时条款 + ADDED Step 7c 契约）
- [ ] **T5.2** ADR-X.15 状态回填（实施完成 ⇒ Accepted）+ 关联 change 指针
- [ ] **T5.3** 台账 Entry + `include/core/AGENTS.md` 补 Step 7c 说明
- [ ] **T5.4** 归档五步链 + verifier 独立验收
