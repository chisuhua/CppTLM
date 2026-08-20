# cpptlm-v05-redo: Tasks (P0'-P4' 12 周)

> **结构**: Phase 化任务清单 (P0' 启动 → P4' 收尾)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`../../docs/adr/ADR-X.16-cpptlm-v05-redo.md`](../../docs/adr/ADR-X.16-cpptlm-v05-redo.md)
> **Owner**: CppTLM Team (Sisyphus)
> **实施指南**: 待写 `/docs/05-advanced/cpptlm-v05-redo-action-plan.md`

---

## P0' 启动 (W1)

### T-P0'-1: 跨仓协议 + 治理 (HSK-7 源码契约)

**Acceptance**:
- [ ] PTX-EMU 仓发出 `2026-XX-XX-hsk-7-cpptlm-bridge-v3.md` 公告(USR-Linux-Emu Architecture Team + CppTLM cc)
- [ ] 公告内容必须列出 v0.5 新增的 C++ 公开 API(per `stepOneWarpInstruction` 草案):
  - `SMContext::stepOneWarpInstruction(uint32_t warp_id, uint64_t* out_pc, int32_t* out_status, uint64_t* out_cycle_count) → int32_t`
  - 任何 v0.5 需要的 PTX-EMU 公开类/方法清单(semver 承诺)
- [ ] CppTLM 仓发 HSK-7 ack 响应 commit
- [ ] 公告内容 **不是** C ABI v3 变更(per Oracle D7 — 保持 VERSION=2 冻结)

**验证命令**:
```bash
# PTX-EMU 仓确认公告存在
cd /workspace/project/PTX-EMU && git log --oneline | grep hsk-7

# CppTLM 仓确认 ack
cd /workspace/project/CppTLM && git log --oneline | grep hsk-7.*ack
```

**Commit** (CppTLM 仓 ack):
```bash
git commit -am "docs(hsk-7): CppTLM ack PTX-EMU v0.5 submodule API (stepOneWarpInstruction + state snapshots)"
```

### T-P0'-2: git submodule add external/PTX-EMU

**Acceptance**:
- [ ] `git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU`
- [ ] `.gitmodules` 添加入口(参考 `.gitmodules:1-4` CppHDL 格式)
- [ ] submodule pin commit(per Oracle §E.1 推荐:`87820951` 或当前 HSK 兼容点)
- [ ] `git submodule update --init` 验证 submodule 内容
- [ ] `external/PTX-EMU` 目录加入 `.gitignore` 的 `external/PTX-EMU/build/`(防止构建产物入库)

**验证命令**:
```bash
git submodule status  # 显示 PTX-EMU commit hash + path
ls external/PTX-EMU/ | head -10  # 验证 submodule 已检出
```

**Commit**:
```bash
git add .gitmodules external/PTX-EMU .gitignore
git commit -m "chore(submodule): add external/PTX-EMU@<commit_hash> (per Oracle §E.1)"
```

### T-P0'-3: CMakeLists.txt 集成 PTX-EMU

**Acceptance**:
- [ ] `CMakeLists.txt` 添加 `add_subdirectory(external/PTX-EMU)`
- [ ] 设置 `PTX_EMU_BUILD_TESTS=OFF`(不构建 PTX-EMU 自家测试)
- [ ] 设置 `PTX_EMU_BUILD_SHARED=OFF`(强制静态库)
- [ ] 设置 `-fvisibility=hidden`(per Oracle §E.1 风险 R5)
- [ ] cpptlm_core 静态链接 PTX-EMU(避免 ABI 二进制耦合)
- [ ] `tests/test_*` 添加 `mock_ptxemu_v05` 测试目标(per T-P3'-1)
- [ ] 现有 850 测试仍通过(per Oracle §F.4 双路径兼容性)

**验证命令**:
```bash
cmake --build build -j$(nproc)
build/bin/cpptlm_tests --reporter compact
# 预期: All tests passed (≥850 assertions in ≥850 test cases)
```

**Commit**:
```bash
git add CMakeLists.txt
git commit -m "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
```

---

## P1' Adapter pattern 落地 (W2-4)

### T-P1'-1: Pm4Decoder (Mesa-style TYPE3)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/pm4_decoder_v05.hh` + `src/tlm/gpu/pm4_decoder_v05.cc`
- [ ] `Pm4Type3Header` 结构体(Mesa convention,per design.md §3.2):
  ```cpp
  struct Pm4Type3Header {
      uint32_t IT : 1;             // bit 0 (Increment Type, NOT predicate!)
      uint32_t predicate : 1;     // bit 1
      uint32_t opcode : 8;        // bits 2-9 (256 opcodes)
      uint32_t reserved : 6;      // bits 10-15
      uint32_t count : 14;        // bits 16-29
      uint32_t type : 2;          // bits 30-31 = 0b11
  };
  ```
- [ ] `Pm4Decoder::parse_type3(uint32_t header, const uint32_t* payload, size_t max_dwords) → Pm4Packet`
- [ ] 4 MVP opcodes 支持:DISPATCH_DIRECT(0x15) / EVENT_WRITE(0x46) / RELEASE_MEM(0x49) / ACQUIRE_MEM(0x58)
- [ ] `tests/test_pm4_decoder.cc` 测试 4 opcode 路径 + bit field 解析

**验证命令**:
```bash
ctest -R "test_pm4_decoder" --output-on-failure
# 预期: 4 MVP opcodes + bit field round-trip PASS
```

**Commit**:
```bash
git add include/tlm/gpu/pm4_decoder_v05.hh src/tlm/gpu/pm4_decoder_v05.cc tests/test_pm4_decoder.cc CMakeLists.txt
git commit -m "feat(pm4-decoder): Mesa-style TYPE3 header parsing (4 MVP opcodes)"
```

### T-P1'-2: CommandProcessor (5-state FSM)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/command_processor_v05.hh` + `src/tlm/gpu/command_processor_v05.cc`
- [ ] `CommandProcessor` 类,5-state FSM:`IDLE → FETCH → DECODE → DISPATCH → COMPLETE`
- [ ] `submit_kernel(image_bytes, size, grid_dim, block_dim, stream_id, out_id)` API(host entry)
- [ ] `tick()` 由 EventQueue 调度
- [ ] 内部调用 `Pm4Decoder::parse_type3` 解析 PM4
- [ ] 5-state FSM 状态转换正确(测试覆盖所有 transition)
- [ ] `tests/test_command_processor.cc` 5 个 transition 测试

**验证命令**:
```bash
ctest -R "test_command_processor" --output-on-failure
# 预期: 5 transition tests PASS
```

**Commit**:
```bash
git add include/tlm/gpu/command_processor_v05.hh src/tlm/gpu/command_processor_v05.cc tests/test_command_processor.cc CMakeLists.txt
git commit -m "feat(command-processor): 5-state FSM with Pm4Decoder integration"
```

### T-P1'-3: PtxEmuSubmoduleV05 (Adapter pattern)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/ptx_emu_submodule_v05.hh` + `src/tlm/gpu/ptx_emu_submodule_v05.cc`
- [ ] **关键约束**:`ptx_emu_submodule_v05.cc` 是**唯一** include PTX-EMU 头的 .cc(编译防火墙)
- [ ] 其他 CppTLM 代码只见前向声明(`namespace ptxsim { class SMContext; }`)
- [ ] 黑盒路径:`image_load` / `image_execute`(v3.0 兼容,走 `image_execute`)
- [ ] 白盒路径:`stepOneWarpInstruction(warp_id, out_pc, out_status, out_cycle_count) → int32_t`
- [ ] `init(ptx_emu_root)` + `shutdown()`(RAII 模式)
- [ ] `verify_dual_path_consistency(max_warp_steps)`(per Oracle §F.4)
- [ ] `tests/test_ptx_emu_submodule_v05.cc`:
  - `[ptx-emu-v05][step]` stepOneWarpInstruction API
  - `[ptx-emu-v05][init]` submodule 加载

**验证命令**:
```bash
# 编译防火墙检查 — v0.5 头应只被 submodule .cc include
git grep "include.*ptxsim" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅 src/tlm/gpu/ptx_emu_submodule_v05.cc

# 测试 PASS
ctest -R "test_ptx_emu_submodule_v05" --output-on-failure
```

**Commit**:
```bash
git add include/tlm/gpu/ptx_emu_submodule_v05.hh src/tlm/gpu/ptx_emu_submodule_v05.cc tests/test_ptx_emu_submodule_v05.cc CMakeLists.txt
git commit -m "feat(ptx-emu-v05): adapter pattern with stepOneWarpInstruction (per-warp precision)"
```

### T-P1'-4: Mock libptxemu_v05.so (双路径测试 fixture)

**Acceptance**:
- [ ] 新建 `test/mock/mock_libptxemu_v05.cc` + `test/mock/CMakeLists.txt`
- [ ] SHARED target `mock_libptxemu_v05`,link `${CMAKE_DL_LIBS}`
- [ ] 提供 8 黑盒 ABI(向后兼容)+ 1 新 API `stepOneWarpInstruction`
- [ ] Mock `stepOneWarpInstruction` 维护内部 warp state:
  ```cpp
  static std::unordered_map<uint32_t, WarpState> warps;
  static uint64_t total_cycle_count;
  WarpState{ pc, cycle, status } 逐次步进
  ```
- [ ] 黑盒 image_execute 与白盒 stepOneWarpInstruction **逐字节一致**(per Oracle §F.4)

**验证命令**:
```bash
# Mock 加载
nm -D build/lib/mock_libptxemu_v05.so | grep -E "image_load|image_execute|stepOneWarpInstruction"
# 预期: 9 个符号 (8 ABI + 1 new)
```

**Commit**:
```bash
git add test/mock/ CMakeLists.txt
git commit -m "test(mock): mock_libptxemu_v05.so — 8 ABI + stepOneWarpInstruction"
```

---

## P2' ComputeUnit v2 + SM 升级 (W5-7)

### T-P2'-1: ScoreboardTLM 升级 production

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/scoreboard_tlm_v05.hh`(继承现有 `scoreboard_tlm.hh`)
- [ ] 新增 `WarpState { pc, cycle_count, register_deps }` 数据结构
- [ ] `tick()` 增加 per-warp tracking
- [ ] 与 `PtxEmuSubmoduleV05::stepOneWarpInstruction` cycle_count 同步
- [ ] `tests/test_scoreboard_v05.cc` per-warp cycle 单元测试

**验证命令**:
```bash
ctest -R "test_scoreboard_v05" --output-on-failure
```

**Commit**:
```bash
git add include/tlm/gpu/scoreboard_tlm_v05.hh tests/test_scoreboard_v05.cc
git commit -m "feat(scoreboard-v05): per-warp cycle tracking (upgrade from Legacy)"
```

### T-P2'-2: PipelineTLM 升级 production

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/pipeline_tlm_v05.hh`(继承现有 `pipeline_tlm.hh`)
- [ ] 保留 5+V latency 表(`A100 latency table`)
- [ ] 新增 `issue(latency)` API,响应 PTX-EMU::Pipeline::step_b_set_blocked_cycles
- [ ] `tests/test_pipeline_v05.cc` latency issue 测试

**验证命令**:
```bash
ctest -R "test_pipeline_v05" --output-on-failure
```

**Commit**:
```bash
git add include/tlm/gpu/pipeline_tlm_v05.hh tests/test_pipeline_v05.cc
git commit -m "feat(pipeline-v05): latency issue API (upgrade from Legacy)"
```

### T-P2'-3: ComputeUnitTLM v2 (Adapter + dual-path dispatch)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/compute_unit_v05.hh` + `src/tlm/gpu/compute_unit_v05.cc`
- [ ] `ComputeUnitTLM : public ChStreamModuleBase`
- [ ] 持有 `PtxEmuSubmoduleV05&`(adapter)
- [ ] 持有 `ScoreboardViewTlm` + `PipelineViewTlm`(升级 2 个)
- [ ] 双路径 dispatch:
  - `dispatch_blackbox(DispatchParams)` → `PtxEmuSubmoduleV05::image_execute`
  - `dispatch_whitebox(warp_count, max_cycles)` → 循环 `stepOneWarpInstruction`
- [ ] `tests/test_compute_unit_v05.cc` 双路径调度测试

**验证命令**:
```bash
ctest -R "test_compute_unit_v05" --output-on-failure
```

**Commit**:
```bash
git add include/tlm/gpu/compute_unit_v05.hh src/tlm/gpu/compute_unit_v05.cc tests/test_compute_unit_v05.cc CMakeLists.txt
git commit -m "feat(compute-unit-v2): ChStreamModuleBase with dual-path dispatch (blackbox + whitebox)"
```

---

## P3' 验证 + docs (W8-10)

### T-P3'-1: 双路径内部一致性测试(per Oracle §F.4 + A2)

**Acceptance**:
- [ ] 新建 `tests/test_v05_dual_path.cc`
- [ ] `TEST_CASE("Dual-path: image_execute vs stepOneWarpInstruction byte-identical", ...)`:
  1. Mock PTX-EMU v0.5 加载测试 kernel
  2. 黑盒路径跑整 kernel,记录终态
  3. 白盒路径循环 `stepOneWarpInstruction` 直到 kernel 完成
  4. **逐字节 diff**:寄存器值、内存值、completion 状态、cycle count
- [ ] 5 类 microbenchmark kernels(GEMM/vector_add/FlashAttention/stencil/SpMV)
- [ ] cycle count 差 ≤ 1 cycle(per-warp step vs 黑盒整 kernel)

**验证命令**:
```bash
ctest -R "test_v05_dual_path" --output-on-failure
# 预期: 5 kernels × 2 paths byte-identical + cycle diff ≤ 1
```

**Commit**:
```bash
git add tests/test_v05_dual_path.cc
git commit -m "test(v0.5): dual-path byte-identical consistency (5 kernels)"
```

### T-P3'-2: docs/research/ 入仓跟踪

**Acceptance**:
- [ ] `git add docs/research/CP/ docs/research/WDU/ docs/research/PCIe/`
- [ ] 新建 `docs/research/AGENTS.md`(research 索引)
- [ ] 更新 `docs/adr/README.md` 加入 ADR-X.16 条目
- [ ] `scripts/test/docs_sync_check.sh --strict` 通过

**验证命令**:
```bash
scripts/test/docs_sync_check.sh --strict
# 预期: 路径同步 PASS
```

**Commit**:
```bash
git add docs/research/ docs/research/AGENTS.md docs/adr/README.md
git commit -m "docs(research): track CP + WDU + PCIe patents + research index"
```

### T-P3'-3: include/AGENTS.md 同步 v0.5 模块

**Acceptance**:
- [ ] `include/AGENTS.md` 加入 v0.5 模块注册宏说明:
  - `CommandProcessor` 注册(REGISTER_OBJECT)
  - `PtxEmuSubmoduleV05` 注册(REGISTER_MODULE)
  - `ComputeUnitTLM v2` 注册(REGISTER_CHSTREAM)
- [ ] `include/tlm/AGENTS.md` 更新 cluster + GPU 章节
- [ ] `docs/README.md` 顶部版本号 + "current focus" 更新

**Commit**:
```bash
git add include/AGENTS.md include/tlm/AGENTS.md docs/README.md
git commit -m "docs(agents): sync v0.5 modules (CP + PtxEmuSubmoduleV05 + ComputeUnitTLM v2)"
```

### T-P3'-4: ADR-X.15 Status Update 追加

**Acceptance**:
- [ ] `docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md` 末尾追加 `## Status Update (2026-XX-XX): v0.5 redo 反转`
- [ ] 引用 ADR-X.16
- [ ] 显式列出 5 项反转决策 + Net-new 路径
- [ ] 不修改 ADR-X.15 既有内容(per AGENTS.md "ADR 不可变"原则)

**Commit**:
```bash
git add docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md
git commit -m "docs(adr): Status Update on ADR-X.15 (v0.5 redo reversal)"
```

---

## P4' 收尾 (W11-12)

### T-P4'-1: 全量 baseline 验证(≥850 + 新增 ≥50)

**Acceptance**:
- [ ] `build/bin/cpptlm_tests --reporter compact`
- [ ] 预期 ≥900 test cases PASS(v0.4.1 baseline 850 + v0.5 新增 ≥50)
- [ ] assertions ≥19000
- [ ] 无 regression(v0.4.1 测试 100% 保留绿)

**验证命令**:
```bash
build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3
# 预期: All tests passed (≥19000 assertions in ≥900 test cases)
```

**Commit**:
```bash
git tag -a v0.5.0-rc1 -m "v0.5.0-rc1: submodule + per-warp precision + dual-path validation"
git push origin v0.5.0-rc1
```

### T-P4'-2: CHANGELOG + 0.5.0 release tag

**Acceptance**:
- [ ] `CHANGELOG.md` 记录 v0.5.0 release:
  - 新增: submodule 集成 + 5 项 v0.5 决策
  - 升级: ScoreboardTLM + PipelineTLM
  - 撤销: v3.0-extract 的 11 项删除清单
- [ ] Tag `v0.5.0` 与 commit `git tag -s v0.5.0 -m "v0.5.0: per-warp instruction precision dGPU"`
- [ ] `docs/05-advanced/cpptlm-v05-redo-action-plan.md` 创建(实施指南)

**Commit**:
```bash
git add CHANGELOG.md docs/05-advanced/cpptlm-v05-redo-action-plan.md
git commit -m "docs(changelog): record v0.5.0 release (submodule + per-warp precision)"
git tag -a v0.5.0 -m "v0.5.0: per-warp instruction precision dGPU"
```

---

## 风险登记(per ADR-X.16 §5.3)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | pin commit + 月度 bump PR |
| R2 | PTX-EMU maintainer 拒收新 API | 中 | 高 | 上游 PR 先行,本地 fork 兜底 |
| R3 | fork 长期 merge 冲突 | 中 | 中 | 监控 diff 量,>1000 LOC 评估独立 release |
| R4 | UsrLinuxEmu 需同步调整 | 中 | 中 | HSK-7 cc UsrLinuxEmu |
| R5 | submodule 编译依赖扩散 | 中 | 低 | 符号可见性 -fvisibility=hidden |
| R6 | 验证无独立 golden | 已确认 | 中 | 文档化 "内部一致性,无独立参考" |
| R7 | 12 周时间线偏紧 | 中 | 中 | MVP 切片(本表)+ P1 推迟其余 |
| R8 | PtxEmuSubmoduleV05 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 |
| R9 | Mesa-style PM4 bit field 与 KFD 不同 | 中 | 中 | 同时验证两种 convention |

---

## 验收检查表

最终 v0.5.0 tag 前:

- [ ] T-P0'-1 ~ T-P0'-3 完成
- [ ] T-P1'-1 ~ T-P1'-4 完成
- [ ] T-P2'-1 ~ T-P2'-3 完成
- [ ] T-P3'-1 ~ T-P3'-4 完成
- [ ] T-P4'-1 ~ T-P4'-2 完成
- [ ] 全部 ≥900 测试 PASS
- [ ] 编译防火墙验证:`git grep "include.*ptxsim"` 仅命中 ptx_emu_submodule_v05.cc
- [ ] 双路径验证:5 kernels byte-identical + cycle diff ≤ 1
- [ ] docs 同步:`scripts/test/docs_sync_check.sh --strict` PASS
- [ ] 跨仓协调:PTX-EMU 新 API PR 已 merge + submodule 已 bump

---

**Cc**: CppTLM Team · PTX-EMU Architecture Team · UsrLinuxEmu Architecture Team

**Refs**:
- [`proposal.md`](proposal.md)
- [`design.md`](design.md)
- [`../../docs/adr/ADR-X.16-cpptlm-v05-redo.md`](../../docs/adr/ADR-X.16-cpptlm-v05-redo.md)
- Oracle review `ses_fe691c532ffeF4dACcAd1dNwwY`
- Metis review `ses_fe6915688ffelUG9G8HB1msoaY`
- [`docs/research/CP/`](../../docs/research/CP/) · [`docs/research/WDU/`](../../docs/research/WDU/)

---

**起草**: Sisyphus (2026-08-19)
**Owner**: CppTLM Team
**状态**: 📐 Tasks — 等 W1 启动后开始实施
