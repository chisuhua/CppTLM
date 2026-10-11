# 实施笔记 — cpptlm-lwc-bus-hub-mvp (Phase 0 Change B)

> **Change**: `openspec/changes/cpptlm-lwc-bus-hub-mvp/`
> **目标 tag**: `cpptlm-lwc-bus-hub-mvp-completion`
> **业务定位**: `docs/designs/dgpu-soc/architecture.md` §0.1 原则 3 + §0.1.1 LWC 总线原则 + §0.2 Phase 0 Change B (P1, 7-10d)
> **修复 Gap**: H (CR-Bus 寄存器访问) + J (Doorbell 直调) + K (MMIO Read write-mirror)
> **作者**: CppTLM Team · **日期**: 2026-10-09

---

## T0 — Pre-flight 验证 (setup, per tasks.md Phase 0)

**验证目标**: LWC/CrBundle/DoorbellBundle 基础设施不存在, Gap H/J/K 调用点确认, baseline 测试计数锁定.

**证据** (verbatim grep):
```bash
$ ls include/tlm/lwc/                    → No such file or directory
$ ls include/bundles/cr_bundles_tlm.hh   → No such file or directory
$ ls include/bundles/doorbell_bundles_tlm.hh → No such file or directory
$ ls include/bundles/msix_bundles_tlm.hh → No such file or directory

$ grep -n "set_pt_base\|mmio_write\|mmio_regs_\|inject_q_" src/tlm/gpu/dgpu_board_shell.cc
  467: gmmu_->set_pt_base_lo(val32);          # Gap H 直调 (0x00)
  469: gmmu_->set_pt_base_hi(val32);          # Gap H 直调 (0x04)
  471: gmmu_->set_enabled((val32 & 1u) != 0);  # Gap H 直调 (0x08)
  500: mmio_regs_[std::make_pair(bar, offset)] = payload;  # Gap K write-mirror
  526: auto it = mmio_regs_.find(std::make_pair(bar, offset));  # Gap K roundtrip
  537: sdma_engine_->mmio_write(/*bar=*/1, ... kBar1DoorbellOffset ...);  # Gap J 直调

$ ./build/bin/cpptlm_tests "[minimal_dgpu_soc]" → All tests passed (125 assertions in 18 test cases)
$ ./build/bin/cpptlm_tests "[pcie-memory]"      → All tests passed (48 assertions in 17 test cases)
$ ./build/bin/cpptlm_tests "[dgpu_soc_timing]"  → All tests passed (12024 assertions in 37 test cases)
$ ./build/bin/cpptlm_tests "[driver_visible]"   → All tests passed (40 assertions in 4 test cases)
$ ./build/bin/cpptlm_tests "[realip]"           → All tests passed (366 assertions in 51 test cases)
```

**dirty baseline** (out of scope, 不动): `CMakeLists.txt` / `configs/dgpu_board_v1.json` / `configs/dgpu_soc_timing_v1.json` / `docs/designs/dgpu-soc/soc-internal-noc-design.md` / `docs/pcie/driver-visible-minimal-soc.md` 修改; `configs/dgpu_soc_v1.json.in` + `openspec/changes/dgpu-realip-pin-contract/*` 删除; `cpptlm-{gmmu-pte,pcie-tlp}` 4件套 untracked.

**设计决策锁定** (v1.0.1 B-H1/H2 修订 + 本实施落点):
- `LwcBusHub` **header-only** (dirty `CMakeLists.txt` 禁改 → 不能加 .cc 源文件); `tick()` no-op + host 线程同步 handler
- `cr_write` / `cr_read` 同步 handler 路由 (`cr_targets_` + `db_targets_` 双 map, 无 pending queue — 测试期望 mmio_write 后 GMMU/SDMA 同步生效)
- `mmio_regs_` 保留作 **无 hub / 非 GMMU reg** fallback (无 SOC `[pcie]` characterization 测试 + `[dgpu-board]` doorbell 测试依赖); Gap K 修复 = mmio_read 优先走 CR-Bus
- 4 frozen headers + 23 ABI 零改动; `pcie_endpoint_ip.hh` quasi-frozen 0 修改

---
