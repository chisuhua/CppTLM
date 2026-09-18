# HSK-10: cpptlm-tlp-wire-datapath 跨仓契约镜像

> **镜像源** (Hub): `UsrLinuxEmu/ADR-088 §D5` (Status Update 待定, per T-P9-0-pre async)
> **本仓镜像**: `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/{proposal.md,specs/pcie-tlp-wire-datapath/spec.md,tasks.md}`
> **父 HSK**: HSK-9 (cpptlm-sm-rewrite, 2027-02-09) — 模板格式参考
> **创建日期**: 2027-02-10
> **维护**: CppTLM Team

## 1. Mirror Scope

本镜像覆盖 P9 TLP Wire Datapath 重构中涉及跨仓协调的 3 类实体:

1. **profile JSON 选路** — `pcie_path` 字段定义 (4 态路由) 及其语义对齐
2. **CplD 同步语义** — CompleterEngine → tx_tlp → host → mmio_read 的读泵环行为契约
3. **ABI 精简清单** — 从 `cpptlm_emulator.h` 22 函数精简至 18 函数 (删除 4 个 backdoor 辅助函数)

跨仓边界: Hub (`UsrLinuxEmu`, 关注 ADR-088 契约 + emulator API) ↔ Spoke (CppTLM 仓, 关注 profile 实现 + 测试覆盖).

## 2. Profile 选路表

`pcie_path` 字段 JSON schema (镜像 spec.md §profile-pcie-path-routing):

| 值 | 触发条件 | 路径分流点 | 行为对齐 |
|---|---|---|---|
| `"tlp"` | 非默认; profile 设 `"tlp"` 时启用 | PCIe EP 走完整 TLP 栈 (CompleterEngine/RequesterEngine + TLP codec + LinkLayer + PHY + Mux) | 最完整路径; 含 CSR 重定向 + 读泵环阻塞 |
| `"axi_bypass"` | `pcie_path = "axi_bypass"` | 跳过 TLP 编解码层, AXI 请求直接路由到 CompleterEngine → memory/cluster | 无 TLP 延迟, 无 DWORD 编解码; 保留 CSR 重定向 |
| `"mock"` | `pcie_path = "mock"` | 替换全部 PCIe 子模块为 PcieMockIP (无 TLP/LL/PHY/Mux/SR-IOV 依赖) | 极简响应; backdoor 行为通过 mock 路径实现 (驱动不可见) |
| `"legacy"` | 默认 (profile 无 `pcie_path` 或设 `"legacy"`) | 回退至 Phase 7.A 冻结的 `PcieEndpointTLM` (4 端口, `[[deprecated]]`) | 兼容遗留测试; 不参与 P9 新功能 |

Hub 端需理解: `"mock"` 路径是向 Hub 提供 backdoor 行为等价替换, 而非删除 backdoor — 驱动侧 ABI 签名不动.

## 3. CplD 同步语义

CompleterEngine → tx_tlp → host → mmio_read 的同步语义 (镜像 spec.md §读泵环线程安全与超时一致性):

```
Host (mmio_read) → AXI Read Req → CompleterEngine → 寻址 (BAR/CSR/Mem)
    ↓ (CSR 命中)            ↑ (Mem 命中)
  读泵环                     MemoryCluster
    ↓
  mutex lock → read_reg → mutex unlock
    ↓
  tx_tlp (CplD 回送) → Host mmio_read 返回
```

**关键同步保证**:

1. **读泵环互斥**: `pending_data_` 由 `std::mutex` 保护 (`abi_call_mutex_`); CplD 到达与 ABI `mmio_read` 读取均需 lock/unlock (同一 BAR 地址的并发 mmio_read 互斥)
2. **CSR 写入可见性**: `CompleterEngine::write_reg()` 使用 `std::atomic_store_explicit(..., std::memory_order_release)` — 保证写入在读泵环 `std::atomic_load_explicit(..., memory_order_acquire)` 之前可见
3. **超时降级**: 读泵环最大阻塞 **1000 虚拟周期**; 超时后 `pending_data_` 同步擦除 + **永久 fallback** 到 `mmio_regs_` 路径 + spin 循环不再 retry (防止 ABI 调用线程阻塞)
4. **CplD 回送保证**: CompleterEngine 保证每个 MRd 请求至多产生一个 CplD (或 Cpl, 当 Unsupported Request 时); 不会产生重复 CplD

Hub 端约束: ADR-088 §D5 需确认 `emulator_backdoor_read/write` 的 CplD 同步语义与之兼容 (包括**永久 fallback** 到 `mmio_regs_` 路径); 或确认 `pcie_path="mock"` 路径可替换.

## 4. ABI 精简清单 (22 → 18)

本镜像记录从 `cpptlm_emulator.h` 22 函数精简至 18 函数的契约.

**删除 4 函数** (完整签名, 逐行对应 `cpptlm_emulator.h` 现有声明):

| # | 函数名 | 理由 |
|---|--------|------|
| 1 | `cpptlm_emulator_backdoor_read` | 不再需要: backdoor 读通过 `pcie_path="mock"` 路径实现, 且 mock 路径无单独 backdoor API |
| 2 | `cpptlm_emulator_backdoor_write` | 同上 |
| 3 | `cpptlm_emulator_register_backdoor_cb` | 不再需要: backdoor callback 注册移至 `PcieMockIP` 内部配置 |
| 4 | `cpptlm_emulator_lookup_register` | 不再需要: register lookup 通过 `config_space` 映射表实现 |

**保留 18 驱动核心函数** (按 `cpptlm_emulator.h` 行 64-115 实测):

```
cpptlm_emulator_get_version           cpptlm_emulator_get_device_count
cpptlm_emulator_get_device_info       cpptlm_emulator_create
cpptlm_emulator_create_by_id          cpptlm_emulator_destroy
cpptlm_emulator_mmio_write            cpptlm_emulator_mmio_read
cpptlm_emulator_pcie_config_write     cpptlm_emulator_pcie_config_read
cpptlm_emulator_msix_init             cpptlm_emulator_msix_update_pending
cpptlm_emulator_msix_clear_pending    cpptlm_emulator_register_callbacks
cpptlm_emulator_register_dma_translate_cb  cpptlm_emulator_open
cpptlm_emulator_close                 cpptlm_emulator_get_adapter_info
```

**4 callback typedef 不动**:

- `cpptlm_intr_deliver_cb_t`
- `cpptlm_error_cb_t`
- `cpptlm_reset_complete_cb_t`
- `cpptlm_power_cb_t`

backdoor 行为通过 `profile.pcie_path = "mock"` 切换 (驱动不可见 — 驱动仍调 `cpptlm_emulator_read_mmio` / `cpptlm_emulator_write_mmio`; 内部路由到 PcieMockIP 的 mock 响应).

**实施 gate**: Hub ADR-088 Status Update ack (T-P9-0). 未 ack 前, 4 个 backdoor 函数保留但加 `[[deprecated]]` 属性.

## 5. 跨仓协调约定

### 5.1 Hub 侧 PR / Issue 提交方法

(待 Hub 文档明确 — 当前通过 `UsrLinuxEmu` repo 的 ADR-088 Status Update 流程协调.)

- 提交方法: Hub 侧 PR 或 Issue (格式待定)
- 标记: 标题前缀 `[HSK-10]`
- 责任人: CppTLM Team → 提交, Hub 侧 maintainer → review

### 5.2 本仓等待 Hub ack 超时

- 最大等待: 10 工作日 (自 HSK-10 镜像创建日: 2027-02-10 → 截止 2027-02-24)
- 超时后 T-P9-0 切出为 follow-up change: `cpptlm-abi-slimming` (backdoor 删除独立化, 不阻塞 P9 主线)

### 5.3 Hub 拒绝回退策略

若 Hub 拒绝删除 4 个 backdoor 函数, 按优先级评估以下策略 (待 HSK-10 与 Hub 协同确定, per Metis AF5 标记):

1. **保留部分 backdoor**: 保留 `cpptlm_emulator_backdoor_read/write`, 删除 `cpptlm_emulator_register_backdoor_cb` / `cpptlm_emulator_lookup_register`
2. **全部不删**: 4 个 backdoor 函数全部保留, 加 `[[deprecated]]` 属性 + 内部路由到 mock 路径
3. **其他**: 通过 `pcie_path` profile 做 ABI versioning (如 `ABI_VERSION=2` 宏)

**确定后更新本段**.

## Status Update

> **2026-09-17 — Hub ack 超时 (10 工作日窗口已过), 触发 tasks.md T-P9-0-pre step 4 拆分 fallback:**
> 
> - **超时原因**: UsrLinuxEmu ADR-088 §D5 Status Update 自 Day 0 (2026-09-17) 提交后 10 工作日窗口内无响应
> - **本 HSK §4 ABI 清单 (22 → 18 函数精简) 拆分出独立 follow-up change**: `openspec/changes/cpptlm-abi-slimming/`
> - **本仓主线 archive 状态**: 22 ABI 函数状态 archive (Hub ack 异步, follow-up `cpptlm-abi-slimming` 独立推进)
> - **新跨仓镜像**: 详见 `docs/cross_repo/HSK-11-cpptlm-abi-slimming.md` (取代本 HSK §4 ABI 清单)
> - **架构决策**: 详见 `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md`
> - **驱动侧影响**: 0 (实测真实驱动仅需 ~12 核心 ABI, 远超 18 个)
> - **后续跟踪**: Hub ack 到达后, 按 HSK-11 §4 表格对应策略执行 (ack/部分 ack/拒绝/无响应)

---
**关联 ADR / 文档**:
- `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` §P9-TLP-Wire-Datapath
- `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md`
- `docs/adr/ADR-088-pcie-emulator-abi.md` §D5 (Hub 侧, 外部引用)
- `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md` (父 HSK 范本)