# Design: cpptlm-stage-1-1-pcie-ep-fixes

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **关联**: [proposal.md](../proposal.md) + [tasks.md](../tasks.md) + [specs/cpptlm-stage-1-1-fixes/spec.md](../specs/cpptlm-stage-1-1-fixes/spec.md)

## 设计概述

阶段 1.1 4 bug 修复聚焦 change。设计原则：
1. **TDD 5 步**：每个修复一个 commit，先写失败测试，再实施
2. **顺序实施**：#3 → #6 → #5 → #7 文档（探索报告建议）
3. **范围最小**：只修 4 个 bug，不引入新功能
4. **架构约束**：不改 ABI 边界（23 ABI 冻结）+ 5 ports wire-format 冻结

## 修复 #3 设计: pcie_config_read/write 转发

### 当前代码（stub）
```cpp
// dgpu_board_shell.cc:151-164
int DGpuBoard::pcie_config_read(uint16_t offset, uint8_t width, uint32_t* val) {
    (void)offset; (void)width; (void)val;
    return -ENOSYS;  // ← 纯 stub
}
```

### 目标代码（转发）
```cpp
int DGpuBoard::pcie_config_read(uint16_t offset, uint8_t width, uint32_t* val) {
    if (val == nullptr) return -EINVAL;
    auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
    if (ep == nullptr || ep->cfg_space_ == nullptr) return -ENOSYS;
    return ep->cfg_space_->read(offset, width, val);  // 转发到 cfg_space_
}
```

### 依赖关系
- `soc_->getInternalInstance("pcie_ep")` 返回 `PcieEndpointTLM*`
- `PcieEndpointTLM::cfg_space_` 是 `PcieConfigSpace*` 实例（已实现 `read/write`）
- 无需修改 `cfg_space_` 本身（已实现）

### 风险点
- **SOC 初始化时序**：如果 SOC 延迟初始化，`pcie_ep` 可能未注册 → null → 返 `-ENOSYS`
- **解引用 null**：必须 null check

### 测试用例
```cpp
TEST_CASE("pcie_config_read returns Vendor ID 0x10DE", "[dgpu][pcie][config]") {
    DGpuBoard board;
    uint32_t val = 0;
    REQUIRE(board.pcie_config_read(0x00, 4, &val) == 0);
    REQUIRE(val == 0x10DE);  // NVIDIA 或 AMD 0x1002
}
```

## 修复 #6 设计: backdoor_read miss 返 -ENOENT

### 当前代码（语义错位）
```cpp
// dgpu_board_shell.cc:168-190
int DGpuBoard::backdoor_read(uint64_t vram_offset, void* buf, size_t len) {
    int rc = static_cast<int>(len);  // ← miss 返 len 伪装成功
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        auto it = vram_segments_.find(vram_offset);
        if (it != vram_segments_.end() && it->second.size() == len) {
            std::memcpy(buf, it->second.data(), len);
            rc = 0;  // hit 返 0
        }
    }
    return rc;  // miss: 返 len (伪装成功); hit: 返 0
}
```

### 目标代码
```cpp
int DGpuBoard::backdoor_read(uint64_t vram_offset, void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;
    std::lock_guard<std::mutex> lock(inject_mu_);
    auto it = vram_segments_.find(vram_offset);
    if (it == vram_segments_.end()) return -ENOENT;  // ← miss 返 -ENOENT
    if (it->second.size() != len) return -EINVAL;    // size 错配
    std::memcpy(buf, it->second.data(), len);
    return 0;
}
```

### 关键变更
- miss 返 `-ENOENT`（-38）而非 `len`
- 加 `nullptr` 和 `size mismatch` 边界检查
- 返值语义：`-ENOENT` = 未找到 / `-EINVAL` = 参数错 / `0` = 成功

### 测试用例
```cpp
TEST_CASE("backdoor_read miss returns -ENOENT", "[dgpu][backdoor]") {
    DGpuBoard board;
    std::vector<uint8_t> buf(64);
    REQUIRE(board.backdoor_read(0xDEADBEEF, buf.data(), 64) == -ENOENT);
}
```

## 修复 #5 设计: mmio_read 数据拷贝

### 当前代码（数据缺口）
```cpp
// dgpu_board_shell.cc:106-133
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    // ... setup req, push to inject_q_, wait 1ms ...
    auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(1));
    if (status != std::future_status::ready) return -110;  // ETIMEDOUT
    int32_t rc = pending_resp_[req.trans_id].get();
    // TODO T-bs-3c: copy resp data to buf
    pending_resp_.erase(req.trans_id);
    return rc;  // ← rc 是状态码, buf 未填充
}
```

### 目标代码
```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;
    PendingReq req;
    req.bar = bar; req.offset = offset;
    req.data.resize(len);  // 预分配
    req.trans_id = next_trans_id_++;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        inject_q_.push_back(std::move(req));
    }
    auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(WAIT_TIMEOUT_MS));
    if (status != std::future_status::ready) {
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_.erase(req.trans_id);
        return -110;  // ETIMEDOUT
    }
    int32_t rc = pending_resp_[req.trans_id].get();
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        if (rc == 0) {
            const auto& data = pending_data_[req.trans_id];  // ← 新数据结构
            std::memcpy(buf, data.data(), std::min(len, data.size()));
            rc = static_cast<int32_t>(std::min(len, data.size()));  // ← 返 byte count
        }
        pending_resp_.erase(req.trans_id);
        pending_data_.erase(req.trans_id);
    }
    return rc;
}
```

### 关键变更
- 新数据结构 `pending_data_` 存储 sim_loop 响应数据
- sim_loop drain 真实化（per design.md §2.5 同步等待，注入器不再 `set_value(0)`）
- 返值改为 byte count（成功）或负 errno（失败）
- `WAIT_TIMEOUT_MS` 改为可配置（per design.md 建议）

### 依赖关系
- 修改 `drain_injection_queue()` 实现：注入器完成后 set_value + set data
- 新数据结构 `pending_data_: std::unordered_map<uint64_t, std::vector<uint8_t>>`

### 测试用例
```cpp
TEST_CASE("mmio_read returns real data", "[dgpu][mmio]") {
    DGpuBoard board;
    std::vector<uint8_t> buf(4);
    REQUIRE(board.mmio_read(0, 0, buf.data(), 4) >= 0);  // byte count
    REQUIRE(buf[0] != 0);  // 不是 garbage (TODO T-bs-3c 修复后)
}
```

## 修复 #7 设计: 文档矛盾澄清

### 当前文档矛盾
- `design.md §3.1` 步骤 1.1 描述："mmio_write blocks synchronously until sim_loop drain completes"
- `architecture/18-pcie-endpoint-entry.md §2.1`："MMIO Write 是异步（sim_loop tick 内 drain）"
- 实际代码（`dgpu_board_shell.cc:148`）：`return 0; // async, no wait`

### 裁决
**保持 async 行为**（当前代码正确）：
- async 性能更好（host driver 不用阻塞等待）
- architecture §2.1 与代码一致
- design.md §3.1 措辞错误

### 修改清单
- `openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md` §3.1 步骤 1.1：删除 "blocks synchronously" 措辞
- 添加明确："mmio_write returns 0 immediately (async); data is drained by sim_loop on next tick"

### 不需改代码
- 代码已正确（async）

### 测试
- N/A（仅文档修订）

## 实施顺序

```
修复 #3 (pcie_config) → 修复 #6 (backdoor_read miss) → 修复 #5 (mmio_read data) → 修复 #7 (文档)
```

**理由**（per 探索报告建议）：
1. #3 最简单，建立 cfg_space 基础设施
2. #6 仅返值修正，不影响数据路径
3. #5 数据流修复，依赖 #3 cfg_space 工作
4. #7 文档修订，无代码变更

每修复一个独立 commit + 单元测试 + Oracle 复审。

## 风险评估

| 修复 | 风险 | 缓解 |
|------|------|------|
| #3 | SOC 初始化时序（pcie_ep 未注册） | null check + 返 -ENOSYS |
| #6 | 多线程访问 vram_segments_ | 保持 inject_mu_ lock |
| #5 | sim_loop drain 调度 race | WAIT_TIMEOUT_MS + 数据结构 race check |
| #7 | 文档理解差异 | 仅澄清，不动代码 |

## 不在设计范围

- 阶段 1.2 MSI-X 修复 #4（独立 change）
- 阶段 1.3 SDMA 4 子阶段（父 change §4）
- 阶段 1.4 电源管理（父 change §5）
- 23 ABI 扩展（已 ship `bab64dd5`，保持冻结）
- 5 ports wire-format（保持冻结）
