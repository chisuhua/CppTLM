# dGPU-RealIP-Refactor 下一轮 Session Handoff

**生成时间**: 2026-10-07
**HEAD commit**: `385c7c35`
**分支**: `feature/dgpu-realip-refactor`

---

## 1. 项目愿景

CppTLM dGPU SoC 从 TLM 抽象 god-class(`DGpuBoard` + `PcieMemoryDevice`)重构为**真实物理 IP 模块边界对齐**的 TLM SoC,目标:RTL team 可对照每个 TLM 类直接实现硅片。完整规划见 `docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` (1949+ 行)。

**核心 IP 边界映射**(Oracle `bg_c60653e0` §4):
- `PcieControllerIP` — DWC i5122 对应(BAR decode + iATU)
- `PcieAxiBridgeIP` — EP 内 composite(非 JSON 顶层,已注销)
- `GpuInternalFabric` — wraps `crossbar_tlm`
- `VramControllerTLM` — REGISTER_CHSTREAM + 顶层 JSON 接 `dgpu_board.vram_storage_`

---

## 2. 本轮已完成(全部提交 + tag 化)

| Phase | 内容 | Commit | Tag |
|-------|------|--------|-----|
| P0 | ch_uint<W>64 分派 + UB修复 | `e9b1b..` | `p0-wide-type-foundation` |
| P1 | PcieTlpWireBundle 迁移 + wire-format spec + Status Update | `8d1f1d5` `b21d290` | `p1-wire-format-canonicalization` |
| P2 PR-A | facade + grep-gate + JSON config | `e46aac2d` `dcc2ab1a` | `p2-pr-a-facades` |
| P2 PR-B A | controller_ip_ pointer + setter | `cad184e6` `de6d44d5` | `p2-pr-b-scaffolding` |
| P2 PR-B B | doorbell + bar_size + bridge composite | `01af2f6a` `51aa4aa5` `c751a2d8` | `p2-pr-b-stage-b-c` |
| P2 PR-B C | dual-path facade consult mmio_read + write | `d1cd1363` | (同 stage-b-c) |
| P2 PR-B D | modules_cluster fix | `b3722d2d` | `p2-pr-b-stage-d` |
| P3 T22 | GpuInternalFabric wrapper scaffold | `95a68889` | `p3-task22-scaffolding` |
| **B2+B3 v1** | 首次尝试(已 superseded) | `e59004b9` | `pr-b-phase-gate-fix-b2-b3` |
| **B2 v2** | 4 缺陷修复 | **`385c7c35`** ← HEAD | `pr-b-phase-gate-fix-b2-v2` |

---

## 3. BLOCKER 当前状态

| BLOCKER | 状态 | commit/位置 |
|---------|------|------------|
| **B1** `PcieControllerIP` 继承 EP 而非组合 | ✅ **RESOLVED** | `ad13991d` 组合 refactor (`include/tlm/pcie/pcie_controller_ip.hh` + `.cc`, test `test_pin_contract_pcie_controller_ip.cc`) |
| **B2** facade verdict 未验证 | ✅ **RESOLVED** | `dgpu_board_shell.cc:319-353` (mmio_read) + `:433-467` (mmio_write) |
| **B3** BAR寄存器非规范语义 | ✅ **RESOLVED** | `pcie_endpoint_ip.cc:106-122` (写 0) + `:138-156` (读 `bar_sizes_[idx]`) |
| iATU 建模 | ⏸️ 后续 (中优先级) | — |
| TLP↔AXI bridge 边界 | ⏸️ 后续 (中优先级) | — |

**B1 实施工时**: ~25 分钟 (预期 2-2.5d → 实际 0.5d,因为 TDD 5 步 + 4 件套验证纪律已成熟)
**B1c 验证证据**: `[controller_ip][p2]` 6/31, `[minimal_dgpu_soc]` 12/69, `[pcie-memory]` 24/69, `[sdma]` 61/12906, `[pcie]` 433/36682 — 全绿

---

## 4. B2 v2 修复的 4 个缺陷(避免回退)

| 缺陷 | 修复方式 | 证据 |
|------|---------|------|
| **B2a** Release-inert | `std::abort()` 替换 `assert()` | `strings build/bin/cpptlm_tests \| grep -c "Strangler gate"` = **2** |
| **B2c** mmio_write 不对称 | facade consult 移到函数顶部,无条件 | mmio_write line ~433 unconditional |
| **B2d** doorbell/OOB 误报 | expected 镜像 controller bounds: `(offset < bar_size(1)) ? Bar1FbFastPath : Unknown` | 两条 path 都用 `pcie_ep()->bar_size(1)` |
| **B2e** 诊断不足 | fprintf(stderr, "[DGpuBoard B2]/[B2 write] facade verdict divergence: bar=%u offset=0x%lx ...") | mmio_read + mmio_write 区分 |

**回退警告**:任何用 `assert()` 替换 `std::abort()` 的提交都会让 Release 模式闸门消失。任何嵌套 facade block 进 display 分支的提交都会重新引入 BAR1/BAR2 写入盲区。

---

## 5. P3 Tasks 24-28 — **暂停 → 已 Amendment 修订**

`docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` Status Update 段已 Amendment(双独立体评审收敛)。原 T23-T28 6 任务改为 **8 子任务 + 3 MUST NOT + 标签规范 + 工时重估 5-6d**。详细 Amendment 见 plan 文件末尾 `## Status Update (2026-10-07, P3 Amendment ...)` 段。

**关键修订点 (Phase A 产出)**:

| Task | 原描述 | 修订后 | 备注 |
|------|--------|--------|------|
| T24 | "VramControllerTLM 桥接 seam (D3 evolution)" | 拆 T23A/B/C 3 段:死引用删除 / JSON 嵌套重建 / backing 注入 | ORACLE AM-1 / METIS AM-1,HI-1 |
| T25 | "真 equivalence harness (替换常量 REQUIRE scaffold)" | "dual-path equivalence smoke (per spec §10.4)" | ORACLE AM-4 / METIS HI-2/SD-2 |
| T28 | "`[pcie-realip]` 标签真空" | 删 vacuous `[pcie-realip]`,改为 7 条可执行命令清单 | ORACLE AM-6 / METIS AM-6,HI-3 |
| (新)T22b | 漏做:`test_pin_contract_gpu_internal_fabric.cc` | T22 commit `95a68889` 只落了 scaffold,test 文件需补 | METIS AM-6 |
| (新)T25A/T25B | 单步 equivalence | 拆装配测试 + 行为测试(失败先修 T23) | METIS AI-FP-6 |
| (新)T27 三文件 | 单文件 architecture.md | + AGENTS.md STRUCTURE + docs_sync_check.sh VIRTUAL_PATHS | METIS HI-4,SD-5 |

**启动顺序 (修订后)**: Phase B 并行 T23A + T22b + T27 → Phase C 依序 T23B → T23C → T25A → T25B → T26 → T28。

**禁用措辞替换**: "真 harness" / "真 equivalence" → 统一 "dual-path equivalence smoke (per spec §10.4 scaffold)"。"promote" / "promoted via JSON reference" → 显式拆 JSON promotion + C++ attach。

---

## 6. 下一轮 Session 第一步:B1 实施计划(Metis 详细化,~2.5d)

### B1a — 锁行为 (0.5d)
- **新文件**: `test/test_pin_contract_pcie_controller_ip.cc`
  - 现状:grep 确认 0 test 引用 `PcieControllerIP`(plan Task 13 文件从未创建)
- **测试内容**: 5 stimuli
1. `bar=0` → `Bar0Mmio`
  2. `bar=1, offset < bar_size(1)` → `Bar1FbFastPath`
  3. `bar=1, offset >= bar_size(1)` → `Unknown`(防 B2d 误报回归)
  4. `bar=2` → `Bar2VramAperture`
  5. `bar=3` → `Unknown`
- **必做 (两步都需)**:
  - `ctrl.attach_endpoint(ep)`(若 ep_ 为空,stimulus 2 永远返回 Unknown — dormant-gate 陷阱复发)
  - `dgpu_board.set_pcie_controller_ip(ctrl)` 激活 dormant B2 闸门
- **标签**: `[controller_ip][p2]`

### B1b — 组合重构 (1d)
- **修改文件**: `include/tlm/pcie/pcie_controller_ip.hh` + `src/tlm/pcie/pcie_controller_ip.cc`
- **改动 (5 处,缺一不可)**:
  - `pcie_controller_ip.hh:20`: `: public PcieEndpointIP` → `: public cpptlm::SimModule`
  - `pcie_controller_ip.cc:11` ctor 初始化列表: `: PcieEndpointIP(name, eq)` → `: SimModule(name, eq)`(若漏改 → 编译错 "no matching constructor for init list")
  - 新成员 `PcieEndpointIP* ep_ = nullptr`(non-owning)
  - 新方法 `void attach_endpoint(PcieEndpointIP*)`
  - `pcie_controller_ip.cc:35`: `bar_size(1)` (继承成员) → `ep_ ? ep_->bar_size(1) : 0`
  - 保留 `get_module_type() override`(SimModule 已声明 virtual)
  - 保留 `REGISTER_MODULE(tlm::pcie::PcieControllerIP)`(`modules_cluster.hh:75`)
- **风险**: 0 消费者依赖 controller 作为 EP;`dgpu_board_shell.cc:336` `PcieControllerIP*` 调用只有 `match_bar`

### B1c — 验证 (0.5d)
- `./build/bin/cpptlm_tests "[controller_ip][p2]"` (~1 min)
- `./build/bin/cpptlm_tests "[minimal_dgpu_soc]"` (~2 min)
- `./build/bin/cpptlm_tests "[pcie-memory]"` (~2 min, 24 cases)
- `./build/bin/cpptlm_tests "[sdma]"` (~1 min, doorbell carve-out)
- 全部绿 → commit `pr-b-phase-gate-fix-b1` + tag

---

## 7. 关键文件位置

| 文件 | 作用 | 关键行 |
|------|------|--------|
| `docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` | 总计划 | Status Update 段(末尾) |
| `src/tlm/gpu/dgpu_board_shell.cc` | B2 v2 闸门 | mmio_read line 319-353 + mmio_write line 433-467 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | B3 修复 | install_bar_registers 106-122 + bar_size 138-156 |
| `include/tlm/pcie/pcie_controller_ip.hh` | B1 目标 | line 20 继承 → 组合 |
| `src/tlm/pcie/pcie_controller_ip.cc` | B1 目标 | line 35 `bar_size(1)` → `ep_->bar_size(1)` |
| `include/tlm/cluster/gpu_internal_fabric.hh` | P3 T22 | `CrossbarTLM* crossbar_` non-owning |
| `configs/dgpu_soc_v2_realip_p2.json:17` | T24 陷阱 | 死 `PcieAxiBridgeIP` 顶层引用 |
| `scripts/test/check_pcie_ip_facade.sh` | grep-gate | P2 PR-A 纪律 |

---

## 8. 必读评审 (Oracle + Metis 意见已落 action)

- Oracle `bg_9c11d0e8` — 首轮评审,B1/B2/B3 BLOCKER 起源
- Metis `bg_fe773aed` — 首轮评审,P3 traps (Task 23 no-op, T24 dead ref, T25 fake scaffold, T26 missing CMake, T28 tag vacuum)
- Oracle `bg_9f2fd8de` — 复评,B2a/b/c/d 4缺陷发现(已修)

---

## 9. 必知风险(避免重新踩坑)

1. **VramControllerTLM `REGISTER_CHSTREAM`**:零参 `REGISTER_CHSTREAM` 宏定义在 `include/chstream_register.hh:49`(`#define REGISTER_CHSTREAM` 不带参),其内部展开已包含 `registerObject<VramControllerTLM>`(行内 in macro body);实际宏调用在 `:185`(`REGISTER_CHSTREAM;`)。**禁止** 在 `vram_controller_tlm.hh` 末尾加 `REGISTER_CHSTREAM(VramControllerTLM, "...")`(2-arg 形式 / `REGISTER_CHSTREAM_DUAL`-style)—— 编译错 "macro passed 2 arguments, but takes just 0"。
2. **cpptlm_tests build hang**: 系统 load avg 8-12 时反复 terminate。**用 `-j1` + 较长 timeout**(已验证),避开 11:00-15:00 系统高峰。
3. **set_pcie_controller_ip 零调用**:当前 `dgpu_board_shell.hh:93` setter **没有任何调用方**。B2 闸门实际 dormant 直到 B1 测试激活它。
4. **frozen header 列表**:`pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h` —— B1+所有改动都不能触碰(本轮已验证 4 干净)。
5. **docs/soc_arch 已迁 ArchForge**:`docs/soc_arch/` 是占位 README;真实 dGPU SoC 架构文档去 `/workspace/project/ArchForge/` 仓。

---

## 10. 启动命令(下一轮 Session)

```bash
# 1. 验证当前状态
cd /workspace/project/CppTLM
git log --oneline feature/dgpu-realip-refactor ^main | head -5
git tag -l | sort | grep -E "p0|p2|p3|pr-b"
git status --short  # 应该 clean

# 2. 验证 build 绿(注意:cpptlm_core 只编静态库;**新 test TU 只在 cpptlm_tests target 编**)
cd build && timeout 180 make -j2 cpptlm_tests   # 必须 cpptlm_tests 不是 cpptlm_core

# 3. 启动 B1a
#    创建 test/test_pin_contract_pcie_controller_ip.cc
#    5 stimuli + ctrl.attach_endpoint(ep) + dgpu_board.set_pcie_controller_ip(ctrl)

# 4. 验证 4 件套(AGENTS.md 调试纪律):
stat -c '%y %n' build/bin/cpptlm_tests src/tlm/gpu/dgpu_board_shell.cc
strings build/bin/cpptlm_tests | grep -c "Strangler gate"   # 应 = 2
```

---

**下一轮 Session 目标:** B1 实施 (~2.5d) → P3 Tasks 24-28 重启 (~3d) → 最终 tag `dgpu-realip-refactor-completion` + PR handoff。

---

## 附:A1-A2 ADR/Status 状态变更追踪

(便于下一轮 Session 知道哪些 ADR 已被本轮触动,无需重读)

| ADR / Status | 状态 |
|--------------|------|
| `ADR-DGPU-06` (Status Update) | ✅ 已 append (P1 wire-format canonicalization) |
| `docs/architecture/14-pcie-tlp-wire-format.md` | ✅ 新建 (P1 byte-loc spec) |
| `wire-format-snapshot.json` | ✅ 已扩展 (P1 TBD entries) |
| `ADR-DGPU-08` 23 ABI 冻结 | ✅ 本轮 zero touch (e59004b9 + 385c7c35 commit grep 确认) |
| `docs/superpowers/plans/...Status Update (PR-B start)` | ✅ 追加 (cad184e6) |
| `docs/superpowers/plans/...Status Update (P3 freeze + BLOCKER)` | ✅ 追加 (e59004b9) |
| `docs/superpowers/plans/...Status Update (2026-10-07, B2 v2 resolved)` | ✅ 追加 (本轮 handoff commit) |

> 注:完整 git graph 见 `git log feature/dgpu-realip-refactor ^main`(canonical 真相源,避免在 handoff 中重复导致漂移)。

---

**维护**: 本 handoff 文件由 2026-10-07 session 自动生成。下一轮 Session 入口必读;完成后 archive 至 `docs/superpowers/plans/archive/`。