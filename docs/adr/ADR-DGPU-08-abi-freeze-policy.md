# ADR-DGPU-08: ABI 冻结策略 (23 ABI 签名级 + 二进制级 0 diff + 0 新增约束)

> **状态**: 📋 提案 (基于 ADR-088 §D5 + DGpuBoard v2.0.2 P0-3e DoD 仲裁)
> **日期**: 2026-09-26
> **关联架构文档**: [../designs/dgpu-board/architecture.md §5.1 ABI 兼容性](../designs/dgpu-board/architecture.md), [../designs/dgpu-driver/architecture.md §3 冻结面](../designs/dgpu-driver/architecture.md)
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前, 15 ABI + 0 新增)
> **配套 ADR**: [ADR-DGPU-09](ADR-DGPU-09-driver-visible-minimal-soc-scope.md) (Driver-Visible 范围定义)
> **仲裁引用**: Oracle v2.0.2 P0-3e DoD 仲裁 + ADR-088 §D5 + DGpuBoard 14 §5.1

---

## 1. 背景

### 1.1 问题

ABI（Application Binary Interface）是 UsrLinuxEmu 端 Linux driver 与 CppTLM 仿真器之间的**二进制契约**。ABI 变更导致：

| 后果 | 严重性 |
|------|--------|
| driver 须重新编译（源码兼容但 ABI 破坏 → driver 崩溃） | 🔴 H |
| driver 须重新链接（签名变更 → undefined reference） | 🔴 H |
| 既有 [abi][minimal_dgpu_soc] 测试套件失效 | 🟡 M |
| 跨仓（UsrLinuxEmu）同步成本激增 | 🟡 M |

### 1.2 历史背景

- **ADR-088 §D5**（v2026 早期）: 首次定义 ABI 冻结面
- **DGpuBoard v2.0.2 修订 (2027-02-09)**: Oracle P0-3e DoD 仲裁，明确**签名级 + 二进制级双冻结**策略
- **D-AXI v1.0 → v1.8**: 5 轮评审均要求 0 新增 ABI 函数（per spec Requirement "15 ABI Functions Unchanged"）
- **v2.0.1 P0-4**: ABI guard 引入 `-ENOSYS` / `-ESHUTDOWN` 后兼容性（per Oracle-4）

### 1.3 现状约束

- 23 ABI 函数当前位于 [include/abi/cpptlm_emulator.h](../../include/abi/cpptlm_emulator.h)
- 冻结面 4 个头文件零 diff：pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh / cpptlm_emulator.h
- driver 视角必须能通过这 15+8=23 ABI 闭环所有最小 SoC 能力
- 跨仓 UsrLinuxEmu driver 在 minimal → D5 演进过程中**零迁移**承诺

---

## 2. 决策

### 2.1 签名级冻结 (Signature-level Freeze)

**规则**：23 ABI 函数签名**字节级 0 diff**（per ADR-088 §D5）

```c
// include/abi/cpptlm_emulator.h (冻结签名示例)
// 签名一旦签发, 函数名/参数类型/返回类型/属性均不可修改

CPPTLM_API int cpptlm_emulator_create(int* out_handle);
CPPTLM_API int cpptlm_emulator_destroy(int handle);
CPPTLM_API int cpptlm_emulator_load_soc_config(int handle, const char* json_path);
CPPTLM_API int cpptlm_emulator_init(int handle);
// ... (其余 19 函数)
```

**测试**：

```bash
# 1. 签名级 0 diff
git diff include/abi/cpptlm_emulator.h | grep -v "^index\|^---\|^+++" | wc -l
# 期望: 0 (排除 hunk 头)

# 2. 排除 24 号 ABI 末尾追加位 (per Oracle-P0-3e 末尾追加允许)
git diff include/abi/cpptlm_emulator.h | grep "^[+-]" | grep -v "^---\|^+++" | grep -v "^[+-]//" | grep -v "^[+-]$"
# 期望: 空
```

### 2.2 二进制级冻结 (Binary-level Freeze)

**规则**：23 ABI 函数**符号 + 调用约定 + 类型布局**字节级 0 diff（更严格）

约束项：
- 函数符号名（`cpptlm_emulator_*`）
- 参数 ABI（cdecl 固定，不引入 stdcall/fastcall）
- 返回类型 ABI（int = 32-bit, pointer = 64-bit on x86_64）
- struct layout（`cpptlm_device_info_t` 字段顺序 + padding）

**测试**：

```bash
# nm 输出比对
nm -D build/lib/libcpptlm_emulator.so | grep "cpptlm_emulator_" > /tmp/before.txt
# rebuild
nm -D build/lib/libcpptlm_emulator.so | grep "cpptlm_emulator_" > /tmp/after.txt
diff /tmp/before.txt /tmp/after.txt
# 期望: 空
```

### 2.3 0 新增约束 (Zero-add Constraint)

**规则**：新功能必须经现有 15 ABI 闭环，**不允许新增 ABI 函数**

扩展机制（合法）：
- ✅ 现有 ABI 行为扩展（如 `mmio_read(bar_idx, ...)` 支持 BAR2）
- ✅ 通过 `PcieEndpointIP` 子类化扩展内部能力
- ✅ 通过 JSON 配置扩展 SoC 拓扑（新增模块 + chip-internal 连接）
- ❌ 新增 `cpptlm_emulator_*` 函数签名
- ❌ 修改现有 ABI 函数签名/返回类型

**测试**：

```bash
# grep 新 ABI 函数
git diff include/abi/cpptlm_emulator.h | grep "^[+]CPPTLM_API"
# 期望: 空（末尾追加允许，但需走 ADR-DGPU-09 范围扩展提案）
```

### 2.4 冻结面 4 个头文件零 diff

| 头文件 | 角色 | 修改门槛 |
|--------|------|---------|
| `include/tlm/gpu/pcie_endpoint_tlm.h` | PcieEndpointTLM [[deprecated]] 兼容 | 仅可追加 `[[deprecated]]` |
| `include/tlm/gpu/pcie_display_device.hh` | D1 Display IO | 仅可追加 `[[deprecated]]` |
| `include/tlm/gpu/pcie_bundles_tlm.hh` | PcieTlpBundle 定义 | 任何修改 = ADR 提案 |
| `include/abi/cpptlm_emulator.h` | 23 ABI C 头 | 任何修改 = ADR 提案 |

---

## 3. 关键不变性 (Invariants)

### Inv-1: 签名级 0 diff

**Where**: `include/abi/cpptlm_emulator.h` 23 函数签名

**测试**: `[abi][minimal_dgpu_soc]` 8 cases / 23 assertions（ABI 函数计数 + 签名 hash 一致性）

```cpp
// test/abi_smoke.cc
TEST_CASE("ABI signature freeze", "[abi][minimal_dgpu_soc]") {
    REQUIRE(cpptlm_emulator_count() == 15);  // D-AXI 阶段基线
    auto sig = abi_signature_hash("cpptlm_emulator");
    REQUIRE(sig == "a1b2c3d4...");  // 预录签名 hash
}
```

### Inv-2: 二进制级 0 diff

**Where**: `build/lib/libcpptlm_emulator.{so,a}` 符号表

**测试**: pre-commit hook + CI matrix 比对 nm 输出

### Inv-3: 冻结面零 diff

**Where**: 4 个冻结头文件

**测试**: `git diff HEAD include/abi/cpptlm_emulator.h` 必须为空（除 hunk 头与 24 号 ABI 行）

### Inv-4: 新功能经现有 ABI 闭环

**Where**: 所有 OpenSpec change 的 design.md 必须显式说明"0 新增 ABI 函数"约束

**测试**: spec.md Requirement "15 ABI Functions Unchanged" Scenario 验证

---

## 4. 实施步骤

| 步骤 | 任务 | 位置 |
|------|------|------|
| 1 | ABI 签名 hash 预录 | `test/abi_smoke.cc` |
| 2 | pre-commit hook 添加 ABI 头 diff 检查 | `.pre-commit-config.yaml` |
| 3 | CI matrix 添加 nm 输出比对 | `.github/workflows/abi-freeze.yml` |
| 4 | spec.md Requirement "15 ABI Functions Unchanged" 落地 | OpenSpec spec.md |
| 5 | ABI guard 引入 -ENOSYS/-ESHUTDOWN (per Oracle-4 v2.0.1) | dgpu_board_shell.cc |
| 6 | LifecycleProtocol ABI 调用 guard 表 | dgpu_board_shell.cc |

**预计工时**: 1-2 工作日 (per t1.md Phase 0)

---

## 5. 测试策略

### 5.1 单元测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_abi_signature_freeze | 23 ABI 签名 hash 一致 | `[abi][freeze]` |
| test_abi_count_15 | ABI 函数数 = 15 (D-AXI 基线) | `[abi][count]` |
| test_freeze_surface_no_diff | 4 冻结头文件零 diff | `[abi][surface]` |

### 5.2 集成测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_minimal_dgpu_soc_abi_smoke | driver 经 15 ABI 闭环 | `[abi][minimal_dgpu_soc]` |
| test_abi_guard_returns_enosys | 未 init 调 mmio_read 返 -ENOSYS | `[abi][guard]` |
| test_abi_guard_returns_eshutdown | shutdown 后调 mmio_read 返 -ESHUTDOWN | `[abi][guard]` |

### 5.3 回归测试

- `[abi]` baseline 28 assertions 全绿
- `[minimal_dgpu_soc]` 41 assertions 全绿
- Phase 8 baseline 66951 assertions 全绿保持

---

## 6. 兼容性

### 6.1 ABI 兼容性（硬约束）

- 签名级 0 diff（per ADR-088 §D5）
- 二进制级 0 diff（per Oracle v2.0.2 P0-3e DoD 仲裁）
- 末尾追加 24 号 ABI 允许（需走 ADR-DGPU-09 范围扩展提案）
- `-ENOSYS` / `-ESHUTDOWN` guard 引入需保留现有 ABI 行为（per Oracle-4 v2.0.1）

### 6.2 JSON 配置兼容性

- minimal_v1 → D5 演进过程 JSON schema 兼容
- `bar_sizes` 数组扩展支持更多 BAR（BAR0/1/2 → BAR0/1/2/3+）

### 6.3 测试兼容性

- `[abi]` 套件 baseline 全绿
- 跨仓 UsrLinuxEmu driver binary 在 minimal → D5 演进过程零迁移

---

## 7. 风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| 误改冻结头文件 | 🔴 H | pre-commit hook 阻断 + CI matrix 验证 |
| 误增 ABI 函数 | 🔴 H | grep 检查 `+CPPTLM_API` 零匹配 |
| ABI guard 行为变更引入兼容性破坏 | 🟡 M | Oracle-4 v2.0.1 已审；新行为需 ABI 测试套件覆盖 |
| 跨仓同步遗漏 | 🟡 M | per DGpuBoard 14 §10 UsrLinuxEmu 端 hand-off 清单 |

### Oracle/Metis 评审重点

- **Oracle v2.0.2 P0-3e**: DoD 仲裁（签名级 + 二进制级双冻结 + 末尾追加允许）
- **Oracle-4 v2.0.1**: ABI guard 引入 -ENOSYS/-ESHUTDOWN 后兼容性
- **D-AXI v1.4 P0-2.3**: spec 数字一致化（H6 修订）

---

## 8. 参考

- ADR-088 §D5: ABI 冻结原始定义
- DGpuBoard 14 §5.1: 签名级 + 二进制级 0 diff 仲裁详细规范
- D-AXI spec.md Requirement "15 ABI Functions Unchanged" + "Freeze Surface Untouched"
- D-AXI driver-visible §11 关联文档（[../../pcie/driver-visible-minimal-soc.md](../../pcie/driver-visible-minimal-soc.md)）
- [cpptlm_emulator.h](../../include/abi/cpptlm_emulator.h)
- 配套 ADR: ADR-DGPU-09 (Driver-Visible 范围定义)

---

## Status Update

_(本节将在 D-AXI v1.4 实施通过 Oracle 评审后追加，记录签发时间与实施 commit hashes)_
