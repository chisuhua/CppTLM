# cpptlm-dgpu-sdma-engine: Tasks

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D3

---

### T-sd-1: DMA 描述符类型与 Bundle

- [x] 新建 `include/tlm/gpu/dma_descriptor_mvp.hh`（C++ API 类型 `DmaDescriptor`，per design §3）
- [x] 新建 `include/bundles/dma_bundles_tlm.hh`（POD bundle `DmaDescriptorBundle` + `CompletionBundle`，per design §2）——**不修改** `include/bundles/pcie_bundles_tlm.hh`（change A 交付），按能力域分文件，与 cache/noc/compute_bundles_tlm.hh 目录惯例一致
- [x] **Commit**: `feat(sdma): DMA descriptor C++ type + dma_bundles_tlm.hh (no touch on pcie_bundles_tlm.hh)`
- [x] 验证：`cmake --build build` PASS（新增 .hh 仅触发 cpptlm_tests 全量 rebuild，无失败）

### T-sd-2: SdmaEngineTLM 组件（5 端口）

- [x] 新建 `include/tlm/gpu/sdma_engine_tlm.hh` + `src/tlm/gpu/sdma_engine_tlm.cc`（ChStreamModuleBase，`num_ports()=5`）
- [x] H2D / D2H 处理流程（per design §3）+ `max_inflight` 反压 + `translate_latency` 参数
- [x] 错误路径：translate fault → CompleterAbort + error 通道（per design §5）
- [x] `include/chstream_register.hh` 注册 `REGISTER_CHSTREAM(SdmaEngineTLM)` + multi-port adapter
- [x] `src/tlm/gpu/sdma_engine_tlm.cc` 加入 `src/CMakeLists.txt` 的 `CORE_SOURCES` 显式列表（项目约定禁 GLOB）
- [x] **Commit**: `feat(sdma): SdmaEngineTLM PCIe master engine + registration`
- [x] 验证（推迟至 T-sd-3 一并执行）：`cmake --build build && ctest -R sdma_engine` PASS（5 端口 num_ports() 断言 + max_inflight 默认值断言）——T-sd-2 阶段尚无测试文件可跑，验证随 T-sd-3 测试创建后一并执行；T-sd-2 仅确认 `cmake --build build` 链接通过（产出 libcpptlm_core.a 含 sdma_engine_tlm.cc）

### T-sd-3: 单元测试（4 文件）

- [x] `test/test_sdma_engine_h2d.cc`（SD-G2）
- [x] `test/test_sdma_engine_d2h.cc`（SD-G3）
- [x] `test/test_sdma_engine_iommu_fault.cc`（SD-G4）
- [x] `test/test_sdma_engine_from_config.cc`（SD-G5）
- [x] 4 个测试文件（`test_sdma_engine_h2d.cc` / `_d2h.cc` / `_iommu_fault.cc` / `_from_config.cc`）加入 `test/CMakeLists.txt` 的 ctest 注册列表
- [x] `configs/test/sdma_engine_min.json` fixture：最小 `DGpuSoc` + `sdma` JSON（无 connections 仅声明类型），用于 `test_sdma_engine_from_config.cc`（SD-G5 验证依赖）
- [x] 反压满窗口测试（per design §8 R3 缓解）：当 `max_inflight=4` 全部占用时，第 5 个 desc_in 必须等到 done_out 释放后才能被处理；新增 mini test 或 fold 进 `test_sdma_engine_h2d.cc` 末段（不另开文件）
- [x] H2D/D2H 数据面 harness 说明：测试用例用 fake board stub（直接构造 fake `cpptlm_dma_translate_cb` 注入 + 直接内存地址读写模拟 IOMMU 翻译 + 直接 VRAM 内存指针）验证端到端数据搬运——**不依赖** `cpptlm-dgpu-abi-export` 的 backdoor ABI（按时间顺序 abi-export 在本 change 之后 archive），每个测试文件首部加注释说明 fake stub 来源
- [x] **Commit**: `test(sdma): H2D/D2H/fault/from-config coverage`

### T-sd-4: 集成验证

- [x] 全量 `build/bin/cpptlm_tests` 无回归
- [x] 本 change 所有 spec 场景满足 → 可 archive
- [x] **Commit**: `chore(sdma): integration validation`
