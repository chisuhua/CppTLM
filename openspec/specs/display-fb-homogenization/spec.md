# display-fb-homogenization Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-d3a1-display-fb-homogenization. Update Purpose after archive.
## Requirements
### Requirement: display_fb_storage_ owner 端 (DGpuBoard)

DGpuBoard **SHALL** 持有 `display_fb_storage_` (类比 `vram_storage_`), 作为 Board-level backing 真源。

#### Scenario: Board-owned 分配
- **WHEN** `DGpuBoard::init()` 调用
- **AND** `display_fb_size()` 派生值 > 0 (顶层 override or `pcie_ep.params.bar_sizes[1]`)
- **THEN** `display_fb_storage_` SHALL 分配 `display_fb_size()` 字节
- **AND** `display_fb_ptr()` SHALL 返回非 null 指针
- **AND** 沿用 `vram_storage_` 分配模式 (`std::unique_ptr<uint8_t[]>`, default-init, Linux lazy commit)

#### Scenario: 顶层 override 优先
- **WHEN** JSON 顶层 `display_fb_size_bytes` 字段存在
- **THEN** `display_fb_size()` SHALL 返回 `display_fb_size_bytes` 值
- **AND** 不再从 `pcie_ep.params.bar_sizes[1]` 派生

#### Scenario: 默认派生 (兼容现有 config)
- **WHEN** JSON 顶层 `display_fb_size_bytes` 字段**不**存在
- **AND** `pcie_ep.params.bar_sizes[1]` 存在
- **THEN** `display_fb_size()` SHALL 返回 `pcie_ep.params.bar_sizes[1]` 值
- **AND** 沿用 P0.5-landing 派生规则 (per `docs/pcie/driver-visible-minimal-soc.md:438`)

#### Scenario: 未配置跳过分配
- **WHEN** JSON 顶层 `display_fb_size_bytes` 字段**不**存在
- **AND** soc 缺 `pcie_ep` 实例
- **THEN** `display_fb_size()` SHALL 返回 0
- **AND** `bind_display_backings()` SHALL 跳过 backing 分配 (WARN 日志)

---

### Requirement: PcieDisplayDevice injected 端 (set_backing_store API)

PcieDisplayDevice **SHALL** 提供 `set_backing_store(ptr, size)` API (per ADR-DGPU-10 owner/injected 两级), 接收 Board-owned backing。

#### Scenario: set_backing_store 接收注入
- **WHEN** `PcieDisplayDevice::set_backing_store(ptr, size)` 被调用
- **AND** `ptr != nullptr && size > 0`
- **THEN** 内部 `backing_store_ = ptr; backing_size_ = size;`
- **AND** `framebuffer_size()` SHALL 返回 `backing_size_` (优先于 `attach_framebuffer_for_testing` 路径)

#### Scenario: 优先级 backing > framebuffer
- **WHEN** `set_backing_store()` **已**被调用 (D3a-1 注入)
- **AND** `attach_framebuffer_for_testing()` **也已**被调用 (测试用)
- **THEN** `framebuffer_size()` SHALL 返回 `backing_size_` (D3a-1 injected 优先)
- **AND** `framebuffer_data()` SHALL 返回 `backing_store_` (D3a-1 injected 优先)
- **注**: 测试 attach_framebuffer_for_testing 路径仅在 backing 未注入时生效 (兼容)

#### Scenario: 冻结面零修改
- **WHEN** PcieDisplayDevice 添加 `set_backing_store()` 方法
- **THEN** **SHALL NOT** 修改现有 API (attach_framebuffer_for_testing / framebuffer_size / mmio_write / 等)
- **AND** 现有 24 cases `[pcie][pcie-display]` SHALL 全绿 (零回归)

---

### Requirement: bind_display_backings() init 期调用

DGpuBoard **SHALL** 在 `init()` 期调用 `bind_display_backings()`, 类比 `bind_memory_backings()` (per D-AXI v1.4)。

#### Scenario: init 期注入顺序
- **WHEN** `DGpuBoard::init()` 调用
- **THEN** SHALL 调用顺序: `bind_memory_backings()` → `bind_display_backings()` → 后续 init
- **AND** `bind_display_backings()` SHALL 在 `display_fb_size()` > 0 时分配 backing
- **AND** SHALL 调用 `soc_->getInternalInstance("pcie_display")` 查找 PcieDisplayDevice
- **AND** 找到 SHALL 调 `set_backing_store(ptr, size)` 注入

#### Scenario: pcie_display 实例缺失处理
- **WHEN** soc 缺 `pcie_display` 实例
- **THEN** `bind_display_backings()` SHALL 输出 WARN 日志
- **AND** SHALL **NOT** throw / abort (per ADR-DGPU-02 软失败原则)
- **AND** `display_fb_storage_` SHALL 释放 (避免泄漏)

#### Scenario: backing 析构顺序
- **WHEN** `DGpuBoard` 析构
- **THEN** `display_fb_storage_` SHALL 在 PcieDisplayDevice 析构之后释放
- **AND** 沿用 `vram_storage_` 析构模式 (per ADR-DGPU-02 LifecycleProtocol 5 态状态机)

---

### Requirement: BAR1 routing 走 backing

BAR1 mmio_read/write **SHALL** 路由到 `PcieDisplayDevice::framebuffer_data()` 返回的 backing 指针 (D3a-1 injected 视图), 非 device-owned framebuffer_。

#### Scenario: BAR1 mmio_write 走 backing
- **WHEN** host `mmio_write(1, offset, data, len)` 调用
- **AND** offset ∈ [0, `display_fb_size()`)
- **AND** D3a-1 `set_backing_store()` 已注入
- **THEN** write SHALL 写入 `backing_store_` (非 `framebuffer_`)
- **AND** `board.display_fb_ptr()` SHALL 反映最新值 (同一指针)

#### Scenario: BAR1 mmio_read 走 backing
- **WHEN** host `mmio_read(1, offset, out, len)` 调用
- **AND** offset ∈ [0, `display_fb_size()`)
- **AND** D3a-1 `set_backing_store()` 已注入
- **THEN** read SHALL 从 `backing_store_` 读取
- **AND** 与 `board.backdoor_read(offset, out, len)` SHALL 数据一致 (per R-WIRE-2: SoC↔Board backing 共享)

#### Scenario: doorbell offset 路径不受影响
- **WHEN** host `mmio_write(1, 0x10010000, wptr, 4)` (doorbell)
- **THEN** SHALL 走 doorbell 路径 (per D-AXI v1.4 §6 行 2 决策, 不依赖 BAR1 实窗大小)
- **AND** D3a-1 不改变此行为 (仅扩展 backing 模式)

---

### Requirement: 23 ABI 冻结 + 0 新增

D3a-1 **SHALL NOT** 修改 23 ABI 签名 (per ADR-DGPU-08 §D5)。

#### Scenario: cpptlm_emulator.h 零 diff
- **WHEN** D3a-1 实施完成
- **THEN** `include/abi/cpptlm_emulator.h` SHALL 零修改 (签名级 + 二进制级)
- **AND** `include/tlm/gpu/pcie_endpoint_ip.h` SHALL 零修改 (per ADR-DGPU-08 冻结面)
- **AND** PcieDisplayDevice 新增 `set_backing_store()` **不**是 ABI 函数 (per ADR-088 §D5 例外条款: 内部 C++ API)

#### Scenario: driver 视角闭环不破
- **WHEN** UE driver 通过 23 ABI 调用 (`cpptlm_emulator_create` + `cpptlm_emulator_mmio_write/read` 等)
- **AND** 加载 `configs/dgpu_soc_minimal_v1.json`
- **THEN** 8 ABI 步骤 SHALL 全部返回 0 / 接受 deferred (per minimal-dgpu-soc-test-coverage spec)
- **AND** `[abi][minimal_dgpu_soc]` 10 cases SHALL 全绿

---

### Requirement: timing-mode 同步实施

D3a-1 **SHALL** 与 timing-mode 同步实施, 因为 timing_v1 沿用 minimal_v1 backing 模式 (per ADR-DGPU-11 H2 0 new connections)。

#### Scenario: timing_v1 config 兼容性
- **WHEN** `dgpu_soc_timing_v1.json` 加载
- **THEN** D3a-1 backing 模式 SHALL 自动生效 (沿用 minimal_v1 派生)
- **AND** `[dgpu_soc_timing]` 37 cases SHALL 全绿 (零回归)
- **AND** timing-mode 不引入新连接 (per H2)

#### Scenario: VramControllerTLM 与 D3a-1 互不干扰
- **WHEN** timing-mode 启用 `vram_ctrl` (VramControllerTLM)
- **AND** D3a-1 backing 模式启用
- **THEN** 两 backing 模式 SHALL 互不干扰 (display_fb 走 PcieDisplayDevice backing, vram 走 VramControllerTLM)
- **AND** 沿用 D-AXI v1.4 §7 seam 模式 (handle_slave_port ↔ backing_ptr_)

---

### Requirement: 测试覆盖 (display_fb_owner_is_board_)

D3a-1 **SHALL** 新增 `display_fb_owner_is_board_` 测试家族, 覆盖 owner / injected / 兼容性 / 派生规则。

#### Scenario: 测试家族规模
- **WHEN** D3a-1 实施完成
- **THEN** 新增测试 ≥ 5 cases, 标签 `[pcie][pcie-display][d3a1]`
- **AND** 覆盖: Board-owned 分配 / PcieDisplayDevice injected / BAR1 routing / 顶层 override / 兼容路径

#### Scenario: 现有 24 cases 零回归
- **WHEN** D3a-1 实施完成
- **THEN** `[pcie][pcie-display]` 24 cases SHALL 全绿 (per R-WIRE-4 冻结面保护)
- **AND** `[pcie-memory]` 24 cases SHALL 全绿 (D3a-1 不影响 PcieMemoryDevice backing)

#### Scenario: 全量回归
- **WHEN** D3a-1 实施完成
- **THEN** 79046 assertions / 1634 cases SHALL 全绿 (基线不变)
- **AND** `[dgpu_soc_timing]` 12024 assertions SHALL 全绿 (37 cases)
- **AND** `[minimal_dgpu_soc]` 69 assertions SHALL 全绿 (12 cases)

---

### Requirement: docs_sync_check + openspec validate

D3a-1 实施完成后, **SHALL** 通过 docs_sync_check.sh --strict + openspec validate --strict。

#### Scenario: docs_sync_check --strict
- **WHEN** D3a-1 实施完成 + 文档同步
- **THEN** `./scripts/test/docs_sync_check.sh --strict` SHALL 返回 0
- **AND** 0 missing paths (370 paths valid)

#### Scenario: openspec validate --strict
- **WHEN** D3a-1 实施完成
- **THEN** `openspec validate --changes --strict` SHALL 返回 0
- **AND** D3a-1 change SHALL 通过验证 (7 Requirements + ~14 scenarios)

---

