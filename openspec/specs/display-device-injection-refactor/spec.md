# display-device-injection-refactor Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-d3a1-display-injection-refactor. Update Purpose after archive.
## Requirements
### Requirement: PcieDisplayDevice non-owning + injected (per ADR-DGPU-10)

PcieDisplayDevice **SHALL** 不持有任何 backing storage,改通过 `set_backing_view(ptr, size)` 注入 raw pointer 视图。

#### Scenario: 删除自有 framebuffer_ 字段
- **WHEN** PcieDisplayDevice 头文件 `pcie_display_device.hh` 检查
- **THEN** **SHALL NOT** 包含 `std::vector<uint8_t> framebuffer_` 字段
- **AND** **SHALL NOT** 包含 `kFbSize` 常量(32MB 假设)
- **AND** **SHALL NOT** 包含 `ensure_framebuffer_allocated()` 方法
- **AND** backing storage 唯一真源在 DGpuBoard

#### Scenario: 新增 set_backing_view 注入 API
- **WHEN** `PcieDisplayDevice::set_backing_view(ptr, size)` 被调用
- **AND** `ptr != nullptr && size > 0`
- **THEN** 内部 `backing_view_ = ptr; backing_view_size_ = size;`
- **AND** `has_backing_view()` SHALL 返回 `true`
- **AND** `backing_size()` SHALL 返回 `size` *(API 名: `backing_size()`, 镜像 PcieMemoryDevice::backing_size(), 私有字段名为 `backing_view_size_`)*

#### Scenario: 新增 has_backing_view 查询 API
- **WHEN** PcieDisplayDevice 构造但未注入
- **THEN** `has_backing_view()` SHALL 返回 `false`
- **AND** `backing_size()` SHALL 返回 0

#### Scenario: framebuffer_size_bytes 保留(兼容现有测试)
- **WHEN** 现有测试调 `dev.framebuffer_size_bytes()`
- **THEN** **SHALL** 返回 `backing_view_size_` (与原有 `framebuffer_.size()` 语义等价)
- **AND** 现有 `[pcie][display][basic]` 13 cases SHALL 全绿(零回归) *(v0.1 写 7 cases 错, 实际数 13)*

#### Scenario: backdoor_read/write view-only
- **WHEN** host `dev.backdoor_read(offset, buf, len)` 调用
- **AND** `has_backing_view() == false`
- **THEN** SHALL 返回 `-ENODEV` (无 silent lazy alloc 兜底)
- **WHEN** `has_backing_view() == true && offset+len <= backing_view_size_`
- **THEN** SHALL 从 `backing_view_ + offset` memcpy `len` 字节到 `buf`
- **AND** `backdoor_write` SHALL 对称 view-only 操作

---

### Requirement: PcieEndpointIP 持有模式对称化

PcieEndpointIP `display_device_` **SHALL** 由 `unique_ptr` 改为 raw ptr + 新增 `set_display_device()` 注入 API,镜像 `memory_device_` (per D2 v1.1)。

#### Scenario: display_device_ 由 unique_ptr 改为 raw ptr
- **WHEN** `pcie_endpoint_ip.hh:299` 检查
- **THEN** `display_device_` SHALL 为 `tlm::gpu::PcieDisplayDevice* = nullptr` (raw ptr,非 owning)
- **AND** 与 `memory_device_` (line 302) 类型/语义对称

#### Scenario: 构造期不自动 make_unique
- **WHEN** `PcieEndpointIP` 构造
- **THEN** **SHALL NOT** 自动 `make_unique<PcieDisplayDevice>()`
- **AND** `display_device_` SHALL 初始化为 `nullptr`
- **AND** `has_display_device()` SHALL 返回 `false` (未注入)

#### Scenario: 新增 set_display_device 注入 API
- **WHEN** `ep.set_display_device(dev)` 被调用
- **AND** `dev != nullptr`
- **THEN** 内部 `display_device_ = dev;`
- **AND** `has_display_device()` SHALL 返回 `true`
- **AND** `display_device()` SHALL 返回 `*dev`

#### Scenario: set_memory_device 与 set_display_device 对称
- **WHEN** PcieEndpointIP API 检查
- **THEN** **SHALL** 同时提供:
  - `set_memory_device(PcieMemoryDevice*)` (D2 已有)
  - `set_display_device(PcieDisplayDevice*)` (D3a-2 新增)
- **AND** 两 API 签名/语义/调用时机镜像

---

### Requirement: DGpuBoard bind_display_backings 镜像 bind_memory_backings

DGpuBoard **SHALL** 新增 `display_fb_storage_` 字段 + `bind_display_backings()` 方法,镜像 `vram_storage_` + `bind_memory_backings()` (per ADR-DGPU-10 owner 端)。

#### Scenario: display_fb_storage_ owner 端
- **WHEN** DGpuBoard 头文件检查
- **THEN** **SHALL** 包含 `std::unique_ptr<uint8_t[]> display_fb_storage_` 字段
- **AND** 派生 size 来源:`display_fb_size_override_` > 0 优先,否则从 `framebuffer_size_` 派生(沿用 v1.4 P0.5-landing)

#### Scenario: bind_display_backings init 期调用
- **WHEN** `DGpuBoard::init()` 调用
- **THEN** SHALL 调用顺序: `bind_memory_backings()` → `bind_display_backings()` → 后续 init
- **AND** `bind_display_backings()` SHALL 在 `display_fb_size() > 0` 时分配 backing
- **AND** SHALL 经 `soc_->display_device()` accessor 获取 PcieDisplayDevice value 对象, per dgpu_soc.hh:27-30
- **AND** SHALL 调 `display_dev.set_backing_view(display_fb_storage_.get(), derived_size)` 注入
- **AND** SHALL 调 `ep->set_display_device(&display_dev)` (经 `pcie_ep()` 获取 EP 实例)
- **AND** **NOT** 通过 `soc_->getInternalInstance("pcie_display")` 查找 *(PcieDisplayDevice 是纯类, 非 SimObject, 该 API 不可查)*

#### Scenario: bind_display_backings 与 bind_memory_backings 对称
- **WHEN** `bind_display_backings()` 与 `bind_memory_backings()` 检查
- **THEN** **SHALL** 共享以下模式:
  - owner 端 `std::make_unique<uint8_t[]>(size)` 分配
  - `set_backing_view(ptr, size)` 注入调用
  - `set_X_device(dev)` EP 注册调用

#### Scenario: soc 缺省状态(无 PcieDisplayDevice)
- **WHEN** `soc_` 未实例化(`!soc_`)
- **THEN** `bind_display_backings()` SHALL 早期 return (skip)
- **AND** **NOT** throw / abort
- **AND** `display_fb_storage_` 保持 `nullptr`
- **注**: PcieDisplayDevice 是 DGpuSoc 的 value 成员, 只要 soc_ 存在则 display_device 必在, 无 "pcie_display 实例缺失" 路径

#### Scenario: 重入 guard(精确语义)
- **WHEN** `DGpuBoard::init()` 二次调用(shutdown→init cycle)
- **THEN** `bind_display_backings()` SHALL: `display_fb_storage_` 的 make_unique **加** guard(`if (!display_fb_storage_) { allocate }`, cc:693), 但 `set_backing_view()` + `set_display_device()` **每次 init 都执行** *(因 shutdown→init 后 soc_ 重建, value 对象的 backing_view_ 字段回到 nullptr, 需重新注入; cc:679-681 P1.1 修复注释)*
- **NOT** 用整函数级 `if (display_fb_storage_) return;` 提前退出 *(会导致 soc_ 重建后留下 dangling 未注入 device)*

---

### Requirement: 23 ABI 冻结 + 0 新增

D3a-2 **SHALL NOT** 修改 23 ABI 签名 (per ADR-DGPU-08 §D5)。

#### Scenario: cpptlm_emulator.h 零 diff
- **WHEN** D3a-2 实施完成
- **THEN** `include/abi/cpptlm_emulator.h` SHALL 零修改
- **AND** `include/tlm/gpu/pcie_endpoint_tlm.h` SHALL 零修改 (PcieEndpointTLM 4 端口冻结)

#### Scenario: driver 视角闭环不破
- **WHEN** UE driver 通过 23 ABI 调用 + 加载 `dgpu_soc_minimal_v1.json`
- **THEN** `[abi][minimal_dgpu_soc]` 12 cases SHALL 全绿
- **AND** `[minimal_dgpu_soc][driver_visible]` E2E SHALL 全绿
- **AND** driver 通过 ABI 可访问 display framebuffer(经注入路径)

---

### Requirement: 现有测试零回归 (但需同步更新)

D3a-2 实施完成后,现有 `[pcie][pcie-display]` 测试 **SHALL** 全绿,但需要同步更新测试代码(因 lazy alloc 路径删除)。

#### Scenario: test_pcie_display_device_basic.cc 13 cases 同步
- **WHEN** 现有 13 cases 检查 (实际文件, `[pcie][display][basic]` 标签)
- **THEN** 测试代码 SHALL 改用 `dev.set_backing_view(buf, size)` 注入模式
- **AND** 13 cases SHALL 全绿(零回归)
- **AND** 语义不变(backdoor_read/write 行为一致)
- **注**: v0.1 写 "7 cases" 错, 实际 `test_pcie_display_device_basic.cc` 含 13 TEST_CASE

#### Scenario: test_pcie_display_device_e2e.cc 8 cases 同步
- **WHEN** 现有 8 cases 检查 (实际文件, `[pcie][display][e2e]` 标签)
- **THEN** 测试代码 SHALL 改用注入模式
- **AND** 8 cases SHALL 全绿
- **注**: v0.1 写 "4 cases" 错

#### Scenario: test_pcie_display_device_vblank.cc 7 cases 不受影响
- **WHEN** VBLANK timer 测试(7 cases, 仅依赖 tick(MsiXTable&))
- **THEN** **NOT** 受 backing 注入语义变化影响
- **AND** 7 cases SHALL 维持全绿

---

### Requirement: 新增 display_device_injection_ 测试家族

D3a-2 **SHALL** 新增 `display_device_injection_` 测试家族,覆盖 owner/injected/symmetry/safety/init-order。

#### Scenario: 测试家族规模
- **WHEN** D3a-2 实施完成
- **THEN** 新增测试 7 cases, 标签 `[pcie][pcie-display][d3a2]` 或 `[pcie][d3a2]` 或 `[dgpu][d3a2]` (实际 test_display_device_injection.cc)
- **AND** 覆盖: backing_view 注入 / 无注入返 -ENODEV / API 镜像 PcieMemoryDevice / set_display_device / DGpuBoard 完整路径 / 重入 / EP 构造
- **注**: v0.1 写 "≥5 cases" 偏保守, 实际 7 cases

#### Scenario: 静态对称性断言(修正 API 名)
- **WHEN** D3a-2 实施完成
- **THEN** `static_assert` 验证 PcieDisplayDevice 与 PcieMemoryDevice 的 **三 API 签名镜像**:
  - `set_backing_view(uint8_t*, uint64_t) noexcept`
  - `has_backing_view() const noexcept → bool`
  - `backing_size() const noexcept → uint64_t` *(API 名是 `backing_size()`, 不是 `backing_view_size()` — 后者是私有字段名)*
- **AND** 编译期保证对称性(per ADR-DGPU-10 §2.2 + §3 Inv-2)

---

### Requirement: docs_sync_check + openspec validate

D3a-2 实施完成后, **SHALL** 通过 docs_sync_check.sh --strict + openspec validate --strict。

#### Scenario: docs_sync_check --strict
- **WHEN** D3a-2 实施完成 + 文档同步
- **THEN** `./scripts/test/docs_sync_check.sh --strict` SHALL 返回 0

#### Scenario: openspec validate --strict
- **WHEN** D3a-2 实施完成
- **THEN** `openspec validate --changes --strict` SHALL 返回 0
- **AND** D3a-2 change SHALL 通过验证

#### Scenario: D3a-1 supersede 标记
- **WHEN** D3a-2 archive 完成
- **THEN** D3a-1 (`cpptlm-dgpu-d3a1-display-fb-homogenization`) SHALL 标注 "superseded by D3a-2"
- **AND** D3a-1 不阻塞 KPI(归档后离开 openspec/changes/)

---

