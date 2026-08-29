# cpptlm-dgpu-board-soc-split: Tasks

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D6 + Status Update (Q3/Q5/Q6)
> **前置**: pcie-endpoint + sdma-engine 已 archive；s3 T-s3-1 + T-s3-2 + T-s3-3 三个 commit 全部落地 + `test_pm4_decoder_mvp` / `test_command_processor_mvp` / `test_tmu_dispatch_processor_mvp` 全 PASS + 头文件在 T-s3-3 后无新 commit（per design §6 Q5）

---

### T-bs-1: DGpuSoc SimModule 容器

- [x] 新建 `include/tlm/gpu/dgpu_soc.hh` + `src/tlm/gpu/dgpu_soc.cc`(SimModule 派生,`simulate_instantiate` 嵌套 JSON, W3 commit `5e47446` 已落地)
- [x] `include/modules_cluster.hh` 加 `REGISTER_MODULE(DGpuSoc)`
- [x] `test/test_dgpu_soc_from_config.cc`(BS-G1:完整实例化 + connections 解析 + outputs/inputs 暴露 + 断言内部 ChStream 组件 adapter 非空, per design §3 Q1 陷阱)
- [x] `src/tlm/gpu/dgpu_soc.cc` 加入 `src/CMakeLists.txt` 的 `CORE_SOURCES` 显式列表(项目约定禁 GLOB)
- [x] `test/test_dgpu_soc_from_config.cc` 加入 `test/CMakeLists.txt` 的 ctest 注册列表
- [x] **Commit**: `feat(dgpu-soc): SimModule container for JSON-built SOC topology` (5e47446)

### T-bs-2: s2 helper 组件化提升（CP/TMU/SQ/CQ）

> **拆分说明 (per Metis 审查 2026-08-28)**: 原 T-bs-2 含 8 项独立组件提升 + 1 项 bundle 新建,过大不利于 AI 实施。按组件拆 6 个 commit,每 commit 1-3 工具调用级别。

#### T-bs-2a: SubmitQueueTLM 提升

- [x] `submit_queue_mvp.*` → `SubmitQueueTLM`(ChStreamModuleBase + 端口化 per design §3.5 表:`cta_in/dispatch/done_in`,**删除** s3 `S3SubmitQueueHandler` handler 模式, W4 commit `5a254e03` 已落地)
- [x] 验证:`cmake --build build && ./build/bin/cptlm_tests "[dgpu]"` 无回归;`SubmitQueueTLM` from_config smoke PASS(978/978 全量)
- [x] `SubmitQueueTLM.cc` 加入 `src/CMakeLists.txt` 的 `CORE_SOURCES` 显式列表
- [x] **Commit**: `refactor(dgpu): promote SubmitQueue to SubmitQueueTLM` (5a254e03)

#### T-bs-2b: CompletionRingTLM 提升

- [x] `completion_ring_mvp.*` → `CompletionRingTLM`(4 端口:`done_in[0]/done_in[1]/done_out/irq_out`,CQ `num_ports() == 4`, W5 commit `49659f5` 已落地)
- [x] `CompletionRingTLM.cc` 加入 CORE_SOURCES
- [x] **Commit**: `refactor(dgpu): promote CompletionRing to CompletionRingTLM` (49659f5)

#### T-bs-2c: CommandProcessorTLM + TmuDispatchProcessorTLM 提升(s3 已填充)

- [x] `command_processor_mvp.*`(s3 已填充) → `CommandProcessorTLM`(rename + 注册 + 4 端口:`cmd_in/fetch_out/dma_req/dispatch`, W2c commit `7bc743e` 已落地)
- [ ] `tmu_dispatch_processor_mvp.*`(s3 已填充) → `TmuDispatchProcessorTLM`(3 端口:`dispatch_in/cta_out/done_in`, deferred — 与 T-bs-2d 时序对齐)
- [ ] 两组件 .cc 加入 CORE_SOURCES(CP 已加, TMU deferred)
- [ ] **Commit**: `refactor(dgpu): promote CP+TMU s3 to CommandProcessorTLM+TmuDispatchProcessorTLM` (待 T-bs-2d TMU 部分合并)

#### T-bs-2d: Doorbell + DGpuBar 迁移到 PcieEndpointTLM

- [ ] s2 `Doorbell` strong-order 语义迁移到 `PcieEndpointTLM` 门铃 register block；删除 `doorbell_mvp.*` 独存类
- [ ] s2 `DGpuBar` 拆分退役：BAR0 寄存器表参数进 `PcieEndpointTLM` JSON；VRAM 由 MemoryTLM 承担；删除 `dgpu_bar.hh` 独存语义
- [ ] **【时序声明 per Metis 审查】**: 本子任务与 `change E T-prereq-4 (Doorbell 排队测试)` 操作同一 `doorbell_mvp.*`。执行顺序: ① E 先行 → 本子任务修订 E 用例; ② 本子任务先行 → E 测试跟随修订
- [ ] **Commit**: `refactor(dgpu): migrate Doorbell+DGpuBar to PcieEndpointTLM (sequenced vs change E)`

#### T-bs-2e: dgpu_bundles_tlm.hh 新建 + s2 旧类删除

- [ ] 新建 `include/bundles/dgpu_bundles_tlm.hh`（per design §3.5 端口表与陷阱 4）：定义 `Pm4DispatchBundle`（POD 化 `Pm4MethodDispatch`）、`CtaDescriptorBundle`（包 s2 `CtaDescriptor` 字段）两个 POD bundle。`CompletionBundle` 不在本文件定义——**复用** `cpptlm-dgpu-sdma-engine` change 交付的 `include/bundles/dma_bundles_tlm.hh::bundles::CompletionBundle`（sdma 是该类型唯一所有者；本 change 不得重复定义以避免 `bundles` 命名空间 ODR 冲突；如有字段扩展必须由 sdma 主导并通知本 change 跟进）。与 cache/noc/compute/pcie/dma bundles 同样按能力域分文件惯例
- [ ] 删除 s2 旧类（doorbell_mvp.cc/dgpu_bar.cc 等）从 CORE_SOURCES 的条目
- [ ] `test/CMakeLists.txt` 移除 s2 单体相关测试（如有）的 ctest 注册
- [ ] **Commit**: `chore(dgpu): add dgpu_bundles_tlm + retire s2 single-module deps`

### T-bs-3: DGpuBoard C++ shell（含 Q6 线程模型）

> **拆分说明 (per Metis 审查 2026-08-28)**: 原 T-bs-3 含 10 项并发设计,跨多文件大改动无中间 checkpoint,AI 失败风险高(死锁/悬垂/竞态)。按职责拆 5 个 commit,每 commit 1-3 工具调用级别。

#### T-bs-3a: 线程模型基础(eq_ + sim_thread_ + inject_q_)

- [x] 新建 `include/tlm/gpu/dgpu_board_shell.hh` + `src/tlm/gpu/dgpu_board_shell.cc` 骨架(per design §2:5 职责 + §2.5 执行模型, W6 commit `0928e12` 已落地)
- [x] 每卡 `eq_=make_unique<EventQueue>()` + `sim_thread_=std::thread` + `inject_mu_+inject_q_` + `stop_` atomic + quantum 默认 1000 cycles(JSON 可配)
- [x] `dgpu_board_shell.cc` 加入 CORE_SOURCES
- [x] **Commit**: `feat(dgpu-board): shell skeleton with per-card EventQueue + sim thread` (0928e12)

#### T-bs-3b: host→sim 注入 + 同步等待

- [x] **host→sim 注入**(`mmio_write` 等):push `PendingReq` → sim 线程 quantum 边界 drain → 构造 `PcieTlpBundle` 注入 `soc_->getInternalInputPort("pcie_ep.slave_in")`(占位 set_value(0),真实 PcieTlpBundle 构造 deferred T-bs-4, W6b commit `22203b4` 已落地)
- [x] **同步等待**:`std::promise/future` 配 1ms wall-clock 超时(防 sim 线程死锁);`PendingReq` 含 `trans_id`,future 经 `std::unordered_map<trans_id, promise>` 关联;resp 通道 `set_value` 时按 `trans_id` 查找并匹配回 host 线程
- [x] **Commit**: `feat(dgpu-board): host→sim injection queue + promise/future sync wait` (22203b4)

#### T-bs-3c: sim→host callback 非阻塞 + StatsManager 多卡前缀

- [x] **sim→host callback 非阻塞**:投递立即返回;callback 内**禁止**反向调 23 ABI(W6c commit `df0d24f` 已落地,`std::thread([cb, args](){...}).detach()`)
- [x] **StatsManager 多卡前缀**:`get_stats_path()` 返回 `"<device_id>.<module_name>"` 防单例冲突(W6 已有,W6c 验证)
- [x] **Commit**: `feat(dgpu-board): non-blocking callbacks + per-card StatsManager prefix` (df0d24f)

#### T-bs-3d: destroy + 异常跨线程

- [ ] **destroy 顺序**：`stop_=true` → inject_q 推 poison pill → `sim_thread_.join()` → 析构 SOC → 析构 EventQueue
- [ ] **异常跨线程**：`sim_loop` 顶层 catch → `std::exception_ptr` → 下一 ABI 调用时 rethrow
- [x] **Commit**: `feat(dgpu-board): deterministic destroy + cross-thread exception capture`

#### T-bs-3e: backdoor 路径走 inject_q + 测试 + 注册

- [x] **backdoor 路径走 inject_q**(per §2.5 第 5 项 + ADR-SOC-07 Q3 裁决):`backdoor_read/write` 经 sim 线程 quantum 边界服务,保证 VRAM 不与 timed 路径竞争(W6e commit `f9bc65c` 已落地,10 test cases)
- [x] `test/test_dgpu_board_shell_abi.cc`(BS-G2:5 职责 + 多线程注入 + 异常传播,10 测试桩)
- [x] `test_dgpu_board_shell_abi.cc` 加入 `test/CMakeLists.txt` 的 ctest 注册列表
- [x] **Commit**: `feat(dgpu-board): backdoor via inject_q + ctest registration` (f9bc65c)

### T-bs-4: board JSON + PCIe 视角测试适配

- [x] `configs/dgpu_board_v1.json`（per design §4：connections 含 `cq.done_in[0]/[1]` 多源汇聚 + `cq.done_out→tmu.done_in` + `cp.fetch_out→vram.0` + `gpu.done→sq.done_in[0]` + `outputs/inputs` 暴露 + `params.quantum_cycles`）
- [x] `test/test_dgpu_pcie_device_perspective.cc` 适配：shell→SOC 端口注入路径，6 测试语义逐条保留（BS-G3）
- [x] `UsrLinuxEmuIoctlStub` 从 board JSON 移出，保留为测试内直接构造工具
- [x] `test/test_dgpu_pcie_device_perspective.cc` 适配更新后，加入 `test/CMakeLists.txt` 的 ctest 注册列表（确保仍可 ctest -R 找到）
- [ ] **Commit**: `test(dgpu): adapt PCIe device-perspective tests to shell+SOC path`

### T-bs-5: s2 单体物理删除 + 全量验证

- [ ] 删除 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`
- [ ] `chstream_register.hh` 移除 `DGpuBoardTLM` 注册
- [ ] 删除 `configs/dgpu_board_v1_mvp.json.in`
- [ ] `grep -rl "DGpuBoardTLM" include/ src/ test/ configs/` → 0 命中（per 项目约定 grep 路径覆盖所有运行时引用源，configs/ 不能漏）
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS（确保新路径在仓库快照中）
- [ ] `build/bin/cpptlm_tests` 全量 PASS（BS-G4）+ `cmake --build build --target validate_topology` PASS
- [ ] 23 ABI 语义不变验证（BS-G5：shell 层契约对比）
- [ ] **Commit**: `chore(dgpu): retire s2 DGpuBoardTLM monolith after SOC migration`

### T-bs-6: 23 ABI C 头文件冻结（per ADR-SOC-07 Q2 裁决）

- [x] 新建 `include/abi/cpptlm_emulator.h`（**纯 C 声明**，`#ifdef __cplusplus extern "C"`，0 实现）
- [x] 声明 ADR-088 §D5 的 23 ABI：`cpptlm_emulator_get_version` / `create` / `mmio_read` / `mmio_write` / `pcie_config_read` / `pcie_config_write` / `backdoor_read` / `backdoor_write` / `msix_init` / `msix_update_pending` / `msix_clear_pending` / `get_device_count` / `get_device_info` / `create_by_id` / `register_callbacks` / `register_backdoor_cb` / `register_dma_translate_cb` / `lookup_register` / `destroy` / 及 4 个 callback typedef
- [x] **Commit**: `feat(abi): 23 C ABI header freeze for cpptlm_emulator (no implementation)`
- [x] 函数体实现移交后续独立 change `cpptlm-dgpu-abi-export`（SHARED 库 + `cpptlm_core` PIC + 全局设备注册表 mutex，per Oracle Q2 落地建议）
- [x] 新建 `include/abi/` 目录（如不存在）；后续 ARCHIVE 时同步根 `AGENTS.md` STRUCTURE 节增加 `include/abi/` 行（per DOC HYGIENE 硬性规则：结构调整 PR 必含 AGENTS.md STRUCTURE 同步）
