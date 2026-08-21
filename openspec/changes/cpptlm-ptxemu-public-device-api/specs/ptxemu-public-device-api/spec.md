# Spec: ptxemu-public-device-api

> **Capability**: `ptxemu-public-device-api`
> **Version**: 1
> **Status**: 📋 Proposed — 2026-08-21
> **Handshake**: HSK-8（CppTLM 发起 + PTX-EMU ack, 锁定公共契约稳定性）

## ADDED Requirements

### Requirement: PTX-EMU 端提供 `ptxemu/device_api.h` 公共头

PTX-EMU 仓库 MUST 在 `include/ptxemu/device_api.h` 提供 `IPtxEmuDevice` 抽象接口 + `create_device/destroy_device` 工厂 + DTO（`DeviceConfig`/`WarpStatus`/`LaneStatus`/`ThreadState`）。该头文件 MUST 是 PTX-EMU 仓库内**唯一**对 CppTLM 公开的公共契约。`PTXEMU_API_VERSION` 宏 MUST 在该头中定义，初始值为 1。PTX-EMU 内部重构 MUST NOT 修改该头的公开签名（不修改 = 不删除/不修改/不改语义；新增可选接口需 bump `PTXEMU_API_VERSION`）。

#### Scenario: PTX-EMU 端维护公共契约
- **WHEN** PTX-EMU 内部 `EXE_STATE` 枚举值变更
- **THEN** `ThreadState` DTO MUST 同步调整，且 `static_assert` 在 `device_api_impl.cc` 内强制两枚举同构；不修改公共头签名时 MUST NOT bump `PTXEMU_API_VERSION`

#### Scenario: CppTLM 端仅 include 公共头
- **WHEN** CppTLM 端 facade.cc / adapter.cc 编译
- **THEN** MUST 仅 include `ptxemu/device_api.h`；include 任何 PTX-EMU 内部头（`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`）MUST 由 CMake include 防火墙 grep 门禁拦截并 FATAL_ERROR

### Requirement: `IPtxEmuDevice` 抽象接口契约

PTX-EMU MUST 在 `ptxemu/device_api.h` 实现以下抽象接口（17 个方法）：

```cpp
namespace ptxemu {
inline constexpr uint32_t kWarpSize = 32;

enum class ThreadState : uint32_t { kIdle=0, kRun=1, kExit=2, kBarSync=3, kUnknown=0xFF };

struct DeviceConfig {
    int num_sms = 1;  int max_warps_per_sm = 64;  int max_threads_per_sm = 2048;
    size_t shared_mem_size_per_sm = 64*1024;  int registers_per_sm = 65536;
    int max_blocks_per_sm = 32;  int warp_size = 32;
    size_t global_mem_size = 4ULL<<30;
};
struct LaneStatus { ThreadState state; bool exited; uint32_t pc; uint32_t blocked_cycles; };
struct WarpStatus { uint32_t active_mask; LaneStatus lanes[kWarpSize]; };

class IPtxEmuDevice {
public:
    virtual ~IPtxEmuDevice() = default;
    virtual uint32_t api_version() const = 0;                    // == PTXEMU_API_VERSION
    virtual bool load_ptx_source(const char* ptx_text) = 0;      // ★ 测试 fixture 关键路径
    virtual int64_t submit_kernel(const std::vector<Statement>& stmts) = 0;
    virtual bool reserve_resources(uint32_t sm, size_t shared_mem, uint32_t warps) = 0;
    virtual void attach_timing(uint32_t sm, IScoreboard*, IPipelineLatencyProvider*,
                               ITensorCoreTiming*) = 0;            // HSK-4 复用
    virtual void step_once(uint32_t sm = 0) = 0;
    virtual uint64_t cycle_count(uint32_t sm = 0) const = 0;
    virtual ThreadState execute_warp(uint32_t warp, const Statement& stmt, int target_pc) = 0;
    virtual WarpStatus warp_status(uint32_t warp) const = 0;
    virtual bool read_register_u32(uint32_t warp, int lane, const char* reg, uint32_t& out) const = 0;
    virtual bool write_register_u32(uint32_t warp, int lane, const char* reg, uint32_t v) = 0;
    virtual bool read_register_u64(uint32_t warp, int lane, const char* reg, uint64_t& out) const = 0;
    virtual bool write_register_u64(uint32_t warp, int lane, const char* reg, uint64_t v) = 0;
    virtual bool read_global_u32(uint64_t addr, uint32_t& out) const = 0;
    virtual bool write_global_u32(uint64_t addr, uint32_t v) = 0;
    virtual bool read_global_u64(uint64_t addr, uint64_t& out) const = 0;
    virtual bool write_global_u64(uint64_t addr, uint64_t v) = 0;
    virtual uint32_t read_thread_pc(uint32_t warp, int lane) const = 0;
    virtual void advance_thread_pc(uint32_t warp, int lane, uint32_t pc) = 0;
    virtual bool is_warp_finished(uint32_t warp) const = 0;
};

std::vector<Statement> decode_ptxir(const uint8_t* bytes, size_t len);
IPtxEmuDevice* create_device(const DeviceConfig&);
void destroy_device(IPtxEmuDevice*);
}
```

公共头 MUST 兼容 C++17 编译（不允许使用 C++20+ 特性，因 CppTLM 全局 C++17 标准）。

#### Scenario: 接口契约不变性
- **WHEN** PTX-EMU 内部 `SMContext::exe_once()` 行为变更（如增加 cycle 计数粒度）
- **THEN** `IPtxEmuDevice::step_once()` MUST 维持外部语义（驱动 warp 执行一步）；impl 内可调整实现细节，但公共签名 MUST NOT 变化

#### Scenario: HSK-4 接口注入复用
- **WHEN** CppTLM 端注入 IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 到 PTX-EMU
- **THEN** `IPtxEmuDevice::attach_timing(sm, sb, pl, tc)` MUST 接收三个 HSK-4 接口指针；PTX-EMU 内部 MUST 在 impl 层将这三个接口替换 HSK-5 已废止的 3-step 注入

### Requirement: PTX-EMU 端 `add_library(ptxemu_core STATIC ...)` 库目标

PTX-EMU 仓库 MUST 在 `CMakeLists.txt` 新增 `add_library(ptxemu_core STATIC ...)` 目标，显式列源文件（禁止 GLOB）。该目标 MUST 设置 `target_include_directories(ptxemu_core PUBLIC include/ptxemu)` + `PRIVATE ${PTXEMU_SRC}`，让 CppTLM 编译时只能看到 `include/ptxemu/` 下的公共头，看不到 PTX-EMU 内部实现头。PTX-EMU 端 MUST 在 `if(PROJECT_IS_TOP_LEVEL)` 块内放置自身 tests/tools（避免 add_subdirectory 调用时污染 CppTLM 构建）。PTX-EMU 端 MUST 提供 `option(PTXEMU_BUILD_TESTING OFF)` 选项（默认 OFF）。

#### Scenario: CppTLM add_subdirectory 调用
- **WHEN** CppTLM 根 `CMakeLists.txt` 执行 `add_subdirectory(external/PTX-EMU)`
- **THEN** MUST 暴露 `ptxemu_core` 静态库 target 给 CppTLM；CppTLM 可执行 `target_link_libraries(... ptxemu_core)` 链接

#### Scenario: PTX-EMU 端 tests 隔离
- **WHEN** CppTLM 构建时（`PTXEMU_BUILD_TESTING=OFF`）
- **THEN** PTX-EMU 端 `tests/` 目录 MUST NOT 被构建；PTX-EMU 自身 tests/tools MUST NOT 加入 CppTLM 构建目标

### Requirement: PTX-EMU 端 `device_api_impl.cc` 适配层

PTX-EMU 仓库 MUST 在 `src/ptxemu/device_api_impl.cc` 实现 `IPtxEmuDevice` 接口，封装 `GPUContext`/`SMContext`/`WarpContext`/`ThreadContext`/`HardwareMemoryManager`/`RegisterBankManager` 等内部类的访问。impl MUST 做 DTO 映射（`EXE_STATE` ↔ `ThreadState`、`WarpState::threads[i].blocked_cycles_remaining` ↔ `LaneStatus::blocked_cycles`）。impl MUST 在 internal namespace 内使用 `static_assert` 强制公共 `ThreadState` 与内部 `EXE_STATE` 同构（abi_guards 模式内化）。

#### Scenario: DTO 映射正确性
- **WHEN** 调用 `device->warp_status(warp_id)` 返回 `WarpStatus`
- **THEN** `WarpStatus::lanes[i]::state` MUST 与内部 `ThreadContext::get_state()` 一致；`blocked_cycles` MUST 与 `WarpState::threads[i].blocked_cycles_remaining` 一致

#### Scenario: `load_ptx_source` 端到端路径
- **WHEN** 调用 `device->load_ptx_source(PTX 源码字符串)`
- **THEN** MUST 经 `InstructionFactory::initialize()` + `StatementFactory` 完成 PTX 解析；返回 bool 表示成功/失败；后续 `execute_warp(warp_id, stmt, target_pc)` MUST 在解析成功的 IR 上工作

### Requirement: PTX-EMU 端契约测试

PTX-EMU 仓库 MUST 在 `tests/build_cpptlm_consume/` 新增 (a) `consumer_smoke.cc`：仅 include `ptxemu/device_api.h` + 调用 `create_device/destroy_device` 的最小 consumer 可执行文件；(b) `drift_check.cmake`：cmake 脚本比较 `ptxemu_core` target 源文件 vs `git ls-files` 候选集，新增未登记的 .cc 即 CI 失败。PTX-EMU 自身 CI MUST 运行这两个测试。

#### Scenario: 消费契约 smoke 测试
- **WHEN** PTX-EMU CI 运行 `consumer_smoke`
- **THEN** MUST 编译链接成功（无未定义符号）+ 运行成功（创建 device + destroy 无泄漏）

#### Scenario: drift 门禁
- **WHEN** 有人添加 `src/ptxsim/instructions/new_op.cpp` 但未在 `ptxemu_core` 显式源清单中登记
- **THEN** `drift_check.cmake` MUST FATAL_ERROR 报错（新增未收录的源文件）

### Requirement: `Statement` 公共 IR 头

PTX-EMU 仓库 MUST 在 `include/ptxemu/ir/statement.h` 提供 `ptxemu::Statement`（晋升自内部 `StatementContext`）。该头 MUST 是纯数据 IR 类型，不包含 PTX-EMU 内部实现依赖。MUST 兼容 C++17 编译。

#### Scenario: StatementContext 传递闭包验证
- **WHEN** `StatementContext` 头文件的传递 include 闭包检查
- **THEN** MUST NOT 引入 PTX-EMU 内部实现头（`ptxsim/*.h`/`memory/*.h`/`register/*.h`/`cudart/*.h`）；若闭包含实现头 MUST 降级到 `StatementHandle` 不透明句柄方案