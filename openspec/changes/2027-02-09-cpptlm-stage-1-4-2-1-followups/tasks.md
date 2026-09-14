# Tasks: cpptlm-stage-1-4-2-1-followups — 6 子任务 TDD 实施

> **工期**: 1.5-2.0 周 | TDD 5 步结构 | 7 commit (6 子任务 + 1 复评)
> **Oracle**: 实施完成 → 复评 → archive

---

## §0 边界与交叉引用

- 与 predecessor `2026-09-10-cpptlm-stage-1-4-2-1` 强耦合: 不变量继承 (INV-A/B/C/D) + 设计/spec 协同
- 详细技术方案: 见 `design.md`
- 边界用例: 见 `spec.md` §MODIFIED Requirements

---

## §1 任务 1.1：INV-A gate 收窄到 BAR only（🔴 严重 PCIe spec 违反）

- [ ] **Write test**: `test/test_pcie_power_state_cfg_access.cc`
  - D3hot 下 cfg 写 PMCSR (offset 0x44) → 触发状态机回 D0 (INV-E)
  - D3hot 下 cfg 读 Vendor ID → 仍成功 (PCIe spec)
  - D3hot 下 BAR 写 → 返回 DECERR bresp (不静默)
  - D3hot 下 BAR 读 → 返回 DECERR rresp
  - D0 下 cfg + BAR 读写 → 全正常 (回归)
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` tick() 入口
  - 移除早返回 `if (mmio_gated_) return;` (line 233-235)
  - 在 cfg/BAR 判别后, 仅 `mmio_gated && !is_cfg` 时返回 DECERR 响应
  - 注意 cfg 路径需在 D3hot 下保留 (INV-E)
- [ ] **Verify fail → implement → Verify pass**: 5 cases 全 PASS
- [ ] **Commit (CppTLM)**: `fix(pcie): INV-A gate narrowed to BAR only + DECERR response (Stage 1.4 §1)`

## §2 任务 1.2：PMCSR PWS=1/2 mask

- [ ] **Write test**: **EP 级测试**（勘误: mask 在 EP 构造器 lambda `pcie_endpoint_ip.cc:28-30`，`test_pm_capability.cc` 测独立 PcieConfigSpace 够不到）— 扩展 `test_power_state_transition.cc` 或新 `test_pcie_power_state_cfg_access.cc`:
  - SECTION "writing PWS=1 (D1) keeps D0": `power_state_` 不变 (D0)
  - SECTION "writing PWS=2 (D2) keeps D0": 同上
  - SECTION "writing PWS=3 (D3hot) transitions": 现有逻辑保持
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc:28`
  - `set_pmcsr_write_cb` lambda 加 mask: `if (new_pws != 0 && new_pws != 3) return;`
- [ ] **Verify**: 既有 [pm] 测试全 PASS + 新 SECTION 通过
- [ ] **Commit (CppTLM)**: `fix(pcie): PMCSR PWS=1/2 reserved values ignored (PCI PM spec)`

## §3 任务 1.3：PM Cap 控制域 JSON-driven

- [ ] **Write test**: `test/test_pm_cap_control_json.cc`
  - 默认无 JSON 键: control=0x0013 (current behavior)
  - JSON `pm_cap_control: 5251`: 写入 cap dword control field
  - JSON `pm_cap_control: 0`: 写入 0 (边界)
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc:28`
  - `pm_cap_control` JSON-driven, 默认 0x0013
- [ ] **Verify**: 3 cases 全 PASS, 既有 [pm] 测试无回归
- [ ] **Commit (CppTLM)**: `feat(pcie): PM Cap control word JSON-driven`

## §4 任务 2.1：ACS Extended Cap 完整实现

- [ ] **Write test**: `test/test_acs_extended_cap.cc`
  - 默认安装: read offset=0xE0 → 0x000D0001 (id=0x000D, version=1)
  - ACS Cap Reg 读: 0x00000000 (no caps)
  - enable V bit (bit 0): 读 Control → 0x0001
- [ ] **New file**: `src/tlm/pcie/pcie_acs_extended_cap.cc`
  - `install_acs_extended_cap(PcieConfigSpace& cfg, uint16_t offset = 0xE0)`
  - 写 header + Cap + Control regs
  - `set_acs_bit_enabled(cfg, bit_idx, enable)` helper
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` 构造期
  - 在 PM Cap 安装后调用 `install_acs_extended_cap(cfg_pf)`
- [ ] **Verify**: 3 cases 全 PASS, [pcie] 全绿
- [ ] **Commit (CppTLM)**: `feat(pcie): ACS Extended Cap (id=0x000D) 完整实现`

## §5 任务 3.1：ResizableBar → PcieEndpointIP 集成

- [ ] **Write test**: `test/test_resizable_bar_integration.cc`
  - `resizable_bar(0).reprogram_size(0x100000)` 后 BAR 写 key=0x80000 成功
  - enable + resize down → 越界 key 被警告清除 (INV-G)
  - 6 个 BAR slot 独立
- [ ] **Modify**: `include/tlm/pcie/pcie_endpoint_ip.hh`
  - 新增 `ResizableBar& resizable_bar(unsigned)` accessor
  - 私有 `std::array<ResizableBar, 6> resizable_bars_;`
  - 私有 `void on_bar_resize(unsigned)` hook
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - 实现 `on_bar_resize`: 校验 bar_store_ keys < new_size, 越界 push warning + erase
- [ ] **Verify**: 3 cases 全 PASS
- [ ] **Commit (CppTLM)**: `feat(pcie): ResizableBar 集成 PcieEndpointIP + INV-G 越界校验`

## §6 任务 4.1：P2P DMA SUCCESS 路径 + AcsPolicy

- [ ] **Write test**: `test/test_p2p_dma_success.cc`
  - 默认 AcsPolicy: 跨 BDF BLOCKED_BY_ACS (向后兼容)
  - `grant(src, dst)` 后: SUCCESS
  - 不同 BDF pair grant 不影响其他
  - 1000-iter fuzz + 多 pair grant
- [ ] **Modify**: `include/tlm/pcie/pcie_bypass_mux_p2p.hh`
  - 新增 `AcsPolicy` 类 (grant 集合)
  - `p2p_dma_route` 增可选 `const AcsPolicy*` 参数 (default nullptr)
  - policy 非空且 !allow → BLOCKED_BY_ACS; 否则 SUCCESS
- [ ] **Verify**: 4 cases / 2000+ assertions PASS, 既有 [p2p] 测试无回归
- [ ] **Commit (CppTLM)**: `feat(pcie): P2P DMA SUCCESS path via AcsPolicy (Stage 2.1 §2.1 完整实现)`

## §7 §5 总计

| 指标 | 值 |
|------|-----|
| **CppTLM commits** | 6 (1 per 子任务) |
| **工期** | 1.5d + 0.5d 复评 = 2.0d |
| **新增测试 cases** | 19 (5+2+2+3+3+4) |
| **新增测试 assertions** | ~2071 (含 P2P fuzz) |
| **Oracle** | 1 次精简复评 |
| **下游** | UE `ue-stage-1-4-2-1-extensions` (待启动) |

## §8 Oracle 复评证据清单（提交 Oracle 时附）

1. `git diff --stat` 限于 6 修改 + 6 新增 + spec.md
2. 全 [pcie] 回归 320+ cases / 19000+ assertions 全绿
3. `strings build/bin/cpptlm_tests | grep -c "ACS Extended"` ≥ 1
4. `grep -rn "static FILE\* diag"` 输出空
5. `openspec validate cpptlm-stage-1-4-2-1-followups --strict` PASS
6. `python3 examples/demo_pcie_full_e2e.py` PASS
7. 全 4 不变量 (INV-A 收窄版 / INV-B / INV-C / INV-D + INV-E/F/G) 测试覆盖
