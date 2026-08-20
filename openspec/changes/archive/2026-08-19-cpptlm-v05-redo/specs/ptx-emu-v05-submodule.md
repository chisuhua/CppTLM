# Spec: PtxEmuSubmoduleV05 (Adapter pattern)

> **配套**: [`../design.md` §3.3](../design.md) · [`../proposal.md`](../proposal.md) · [`../tasks.md`](../tasks.md) · [`../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md`](../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md)
> **状态**: 📋 Spec — W2-W4 实施
> **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-X.16-cpptlm-v05-redo.md` §D4](../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md) — Adapter pattern

---

## 1. 范围 (Scope)

v0.5 新增 `PtxEmuSubmoduleV05` 是 PTX-EMU 静态链接的 adapter,提供**黑盒兼容 + 白盒 per-warp step** 双路径 API。

**关键约束**(per ADR-X.16 D4):
- `ptx_emu_submodule_v05.cc` 是**唯一** include PTX-EMU 头的 .cc(编译防火墙)
- 其他 CppTLM 代码只见前向声明(`namespace ptxsim { class SMContext; }`)

## 2. 公共 API

### 2.1 头文件

```cpp
// include/tlm/gpu/ptx_emu_submodule_v05.hh
#ifndef CPPTLM_PTX_EMU_SUBMODULE_V05_H
#define CPPTLM_PTX_EMU_SUBMODULE_V05_H

#include <cstdint>
#include <string>

// 前向声明(避免在头中 include PTX-EMU 头)
namespace ptxsim {
class SMContext;
}

namespace tlm::gpu {

class PtxEmuSubmoduleV05 {
public:
    using ImageHandle = uint64_t;

    void init(const std::string& ptx_emu_root);
    void shutdown();

    // ========== 黑盒路径(快速模式, v3.0 兼容) ==========
    ImageHandle image_load(const uint8_t* bytes, size_t size);
    int32_t image_execute(ImageHandle h,
                          uint32_t gx, uint32_t gy, uint32_t gz,
                          uint32_t bx, uint32_t by, uint32_t bz,
                          size_t shared_mem,
                          const void* args, size_t argc);
    int32_t image_unload(ImageHandle h);

    // ========== 白盒路径(per-warp precision, NEW v0.5) ==========
    // 步进一条 warp 指令,返回 PC + cycle + status
    // 返回 0 = 仍有指令未执行, 1 = kernel 完成, -1 = error
    int32_t stepOneWarpInstruction(uint32_t warp_id,
                                    uint64_t* out_pc,
                                    int32_t* out_status,
                                    uint64_t* out_cycle_count);

    // ========== 双路径一致性验证(per Oracle §F.4 + A2) ==========
    bool verify_dual_path_consistency(uint32_t max_warp_steps);

    // ========== 状态查询 ==========
    bool is_initialized() const;

private:
    // 实现细节(.cc 中持有 PTX-EMU 实例)
    ptxsim::SMContext* sm_context_ = nullptr;
    void* module_handle_ = nullptr;  // dlopen handle for backward compat (CPPTLMBRIDGE_VERSION=2)
    // ... 其他 PTX-EMU 实例
};

}  // namespace tlm::gpu

#endif  // CPPTLM_PTX_EMU_SUBMODULE_V05_H
```

### 2.2 命名空间

- `namespace tlm::gpu` — 与 CppTLM 仓全局惯例一致(per `include/tlm/AGENTS.md`)

## 3. 行为契约

### 3.1 init(ptx_emu_root)

- **Precondition**: `is_initialized() == false`
- **Side effects**:
  1. 加载 PTX-EMU submodule(从 `ptx_emu_root` 路径)
  2. 解析 submodule 中的 v0.5 新 API (`stepOneWarpInstruction` 等)
  3. 解析 v0.5 兼容的 8 ABI(image_load 等)
  4. 创建 `ptxsim::SMContext` 实例
- **Postcondition**: `is_initialized() == true`
- **错误**: submodule 加载失败 → `throw std::runtime_error`
- **关键**: 编译防火墙检查通过(`git grep "include.*ptxsim"` 仅命中 .cc)

### 3.2 shutdown()

- **Precondition**: `is_initialized() == true`
- **Side effects**:
  1. 释放 `ptxsim::SMContext` 实例
  2. 关闭 submodule handle(若 v3.0 backward compat 路径使用)
- **Postcondition**: `is_initialized() == false`

### 3.3 image_load(bytes, size) (黑盒路径)

- **Precondition**: `bytes != nullptr && size > 0`
- **Returns**: ImageHandle(> 0 成功, 0 失败)
- **错误**: 失败 throw `std::runtime_error`
- **向后兼容**: 与 v3.0 PtxEmuSubmodule::image_load 行为一致

### 3.4 image_execute(h, gx, gy, gz, bx, by, bz, shared_mem, args, argc) (黑盒路径)

- **Precondition**: h > 0
- **Returns**: 0 = 成功, 非 0 = cudaError_t
- **行为**: 同步执行整个 kernel 至完成(v3.0 兼容)
- **向后兼容**: 与 v3.0 PtxEmuSubmodule::image_execute 行为一致

### 3.5 stepOneWarpInstruction(warp_id, out_pc, out_status, out_cycle_count) (白盒路径 NEW v0.5)

- **Precondition**: `is_initialized() && warp_id < MAX_WARPS && out_pc != nullptr && out_status != nullptr && out_cycle_count != nullptr`
- **Side effects**:
  1. 步进 warp_id 指定的 warp 一条指令
  2. 输出当前指令 PC → `*out_pc`
  3. 输出指令完成状态 → `*out_status`(0 = 成功, 非 0 = cudaError_t)
  4. 输出本条指令 cycle 数 → `*out_cycle_count`
- **Returns**:
  - `0` = 仍有指令未执行(继续调用以推进更多 warp)
  - `1` = 所有 warp 完成,kernel 已完成
  - `-1` = error(参数无效、kernel 未启动等)
- **关键特性**: per-warp precision, 与 nvprof / nsight 默认颗粒度一致

### 3.6 verify_dual_path_consistency(max_warp_steps)

- **Precondition**: `is_initialized() && max_warp_steps > 0`
- **Side effects**:
  1. 黑盒 `image_execute` 跑测试 kernel(默认 1 个)
  2. 白盒 `stepOneWarpInstruction` 循环跑完 kernel
  3. 比较寄存器终态、内存终态、cycle count 差
- **Returns**: `true` = byte-identical 终态 + cycle 差 ≤ 1, `false` = 不一致
- **用途**: 自动验证双路径等价(per A2 验证基础)

### 3.7 is_initialized()

- **Returns**: `true` if init 已调用且 shutdown 未调用, else `false`
- **线程安全**: 内部 `std::atomic<bool>`

## 4. 错误处理

| 错误源 | 处理 |
|---|---|
| init() submodule 路径不存在 | throw `std::runtime_error` |
| init() PTX-EMU 新 API 缺失 | throw `std::runtime_error`("v0.5 API not found in PTX-EMU submodule,需要升级") |
| image_load bytes=nullptr | throw `std::invalid_argument` |
| image_execute h=0 | return CUDA_ERROR_INVALID_HANDLE |
| stepOneWarpInstruction 参数无效 | return -1 |
| stepOneWarpInstruction kernel 未启动 | return -1 |
| verify_dual_path_consistency 不一致 | return false(log warn 详情)|

## 5. 性能预期

| 操作 | 时间复杂度 | 备注 |
|------|----------|------|
| init | O(1) | submodule 解析一次性 |
| image_load | O(1) | 单次 |
| image_execute | O(kernel_size) | 整 kernel |
| stepOneWarpInstruction | O(1) | **per-warp 精度** |
| verify_dual_path_consistency | O(max_warp_steps) | 测试用 |

**仿真速度损失**(per Oracle §D):per-warp step vs 黑盒 image_execute ≈ 10x-50x。

## 6. 测试要求

### 6.1 单元测试

| 测试 | Catch2 标签 |
|------|-----------|
| init 加载 submodule | `[ptx-emu-v05][init]` |
| image_load 加载 mock kernel | `[ptx-emu-v05][load]` |
| image_execute 整 kernel 跑完 | `[ptx-emu-v05][blackbox]` |
| stepOneWarpInstruction API 推进 | `[ptx-emu-v05][step]` |
| 编译防火墙验证(其他 .cc 不 include ptxsim/) | `[ptx-emu-v05][firewall]` |

### 6.2 双路径验证(per A2)

| 测试 | Catch2 标签 |
|------|-----------|
| 双路径 byte-identical 终态(5 kernels) | `[ptx-emu-v05][dual-path]` |
| cycle count 差 ≤ 1 cycle | `[ptx-emu-v05][dual-path][cycle]` |

## 7. 接口稳定性

### 7.1 冻结接口(P4' 前禁止变更)

- ✅ `init(shutdown)` / `image_load` / `image_execute`(v3.0 兼容)
- ✅ `stepOneWarpInstruction(warp_id, out_pc, out_status, out_cycle_count)` 签名(v0.5 新增)
- ✅ `verify_dual_path_consistency(max_warp_steps)` 签名(v0.5 新增)
- ✅ `is_initialized()` 签名

### 7.2 可演进接口(P1'-P3' 期间允许调整)

- 🟡 `MAX_WARPS` 常量(内部实现细节)
- 🟡 错误码扩展(新增 cudaError_t)

---

## 8. 跨文档参考

- [`../design.md` §3.3 PtxEmuSubmoduleV05](../design.md) — 详细设计
- [`../proposal.md` §"T-P1'-3 PtxEmuSubmoduleV05"](../proposal.md) — 任务验收
- [`../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md` §D4 Adapter pattern](../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md)
- Oracle review §F.1 (WarpScheduler 是抽象基类) + §F.4 (双路径等价)

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — W2-W4 实施
**下次更新**: W4 P1' 完成后(adapter 实施验证)
