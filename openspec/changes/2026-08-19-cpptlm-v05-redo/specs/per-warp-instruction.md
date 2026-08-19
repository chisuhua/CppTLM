# Spec: Per-Warp Instruction Precision API

> **配套**: [`../design.md` §3.3 + §3.4](../design.md) · [`../proposal.md`](../proposal.md) · [`../tasks.md`](../tasks.md) · [`ptx-emu-v05-submodule.md`](ptx-emu-v05-submodule.md)
> **状态**: 📋 Spec — W5-W7 实施
> **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-X.16-cpptlm-v05-redo.md` §D3](../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md) — per warp instruction

---

## 1. 范围

v0.5 per-warp instruction precision 是与 nvprof / nsight 默认颗粒度一致的精度级别。本 spec 定义 PTX-EMU 新增 + CppTLM adapter 调用契约。

## 2. 颗粒度定义(per ADR-X.16 §D3)

| 项 | 规格 |
|---|---|
| **精度颗粒度** | **per warp instruction**(不是 per PTX instruction) |
| **粒度选择理由** | nvprof / nsight 默认 + 不需重写 PTX-EMU 解释器循环 |
| **仿真速度损失** | 10x-50x(vs v3.0 黑盒 image_execute) |

## 3. PTX-EMU 新增 API(per ADR-X.16 §4.4)

```cpp
// PTX-EMU 仓新增(HSK-7 源码契约):
class SMContext {
public:
    // 既有 image_execute 不变(v3.0 黑盒兼容)
    int image_execute(ImageHandle, ...);
    
    // v0.5 新增(per-warp precision):
    int stepOneWarpInstruction(uint32_t warp_id,
                              uint64_t* out_pc,
                              int32_t* out_status,
                              uint64_t* out_cycle_count);
};
```

**返回值**:
- `0` = 仍有指令未执行(继续调用)
- `1` = 所有 warp 完成,kernel 完成
- `-1` = error(参数无效、kernel 未启动等)

## 4. CppTLM adapter 调用(per design.md §3.3)

```cpp
// include/tlm/gpu/ptx_emu_submodule_v05.hh
class PtxEmuSubmoduleV05 {
public:
    int32_t stepOneWarpInstruction(uint32_t warp_id,
                                    uint64_t* out_pc,
                                    int32_t* out_status,
                                    uint64_t* out_cycle_count);
};
```

**Adapter 实现**(`src/tlm/gpu/ptx_emu_submodule_v05.cc`):
```cpp
int32_t PtxEmuSubmoduleV05::stepOneWarpInstruction(
    uint32_t warp_id, uint64_t* out_pc,
    int32_t* out_status, uint64_t* out_cycle_count) {
    return sm_context_->stepOneWarpInstruction(
        warp_id, out_pc, out_status, out_cycle_count);
}
```

## 5. ComputeUnitTLM 调度使用

```cpp
// include/tlm/gpu/compute_unit_v05.hh
class ComputeUnitTLM : public ChStreamModuleBase {
public:
    int32_t dispatch_whitebox(uint32_t warp_count, uint64_t max_cycles);
    
private:
    int32_t dispatch_whitebox_impl(uint32_t warp_count, uint64_t max_cycles) {
        uint64_t total_cycles = 0;
        for (uint32_t w = 0; w < warp_count; ++w) {
            uint64_t pc, cycles;
            int32_t status;
            while (ptx_.stepOneWarpInstruction(w, &pc, &status, &cycles) == 0) {
                total_cycles += cycles;
                if (total_cycles >= max_cycles) return 0;  // 时间预算用尽
                scoreboard_.update(pc, status);
                pipeline_.issue(cycles);
            }
        }
        return 0;
    }
};
```

## 6. Warp 状态机

```
[warp_id 0] [warp_id 1] ... [warp_id N-1]
   ↓          ↓                ↓
 [PC=...]   [PC=...]         [PC=...]
 [state]    [state]          [state]
   ↓          ↓                ↓
 per-warp per-cycle accounting (parallel scheduler)
```

## 7. 与 v3.0 黑盒路径对比

| 维度 | v3.0 image_execute | v0.5 stepOneWarpInstruction |
|------|-------------------|-----------------------------|
| 颗粒度 | 整 kernel | **per warp, per instruction** |
| 状态可见性 | 仅 entry/exit status | **PC + cycle + status per step** |
| 仿真速度 | 1x | **10x-50x slowdown** |
| 使用场景 | 快速 functional 验证 | **指令级精度研究** |
| 可与 nvprof 比对 | � 否(black-box) | ✅ **是(per-warp state)** |

## 8. 测试要求

```cpp
TEST_CASE("Per-warp instruction: step one warp, observe PC + cycle",
          "[v0.5][per-warp][step]") {
    PtxEmuSubmoduleV05 ptx;
    ptx.init(MOCK_PTXEMU_V05_SO);

    uint8_t bytes[16] = {0xDE, 0xAD, 0xBE, 0xEF};
    auto handle = ptx.image_load(bytes, 16);
    
    // 启动 kernel
    ptx.image_execute(handle, 1, 1, 1, 1, 1, 1, 0, nullptr, 0);
    
    // per-warp 步进
    uint64_t pc = 0, cycles = 0, total = 0;
    int32_t status = 0;
    int rc = ptx.stepOneWarpInstruction(0, &pc, &status, &cycles);
    REQUIRE(rc == 0);  // 仍有指令
    REQUIRE(pc != 0);   // PC 实际变化
    REQUIRE(cycles > 0);
    total += cycles;
    
    // 继续直到完成
    int iterations = 0;
    while (rc == 0 && iterations < 10000) {
        rc = ptx.stepOneWarpInstruction(0, &pc, &status, &cycles);
        total += cycles;
        ++iterations;
    }
    REQUIRE(rc == 1);  // kernel 完成
}

TEST_CASE("Per-warp: cycle count == black-box image_execute total ±1",
          "[v0.5][per-warp][cycle-equivalence]") {
    PtxEmuSubmoduleV05 ptx;
    ptx.init(MOCK_PTXEMU_V05_SO);
    
    uint8_t bytes[16] = {0x12, 0x34};
    auto handle = ptx.image_load(bytes, 16);
    
    // 黑盒路径:获取 cycle_count(假设 mock 暴露)
    DispatchParams p{1, 1, 1, 1, 1, 1, 0};
    auto status_black = ptx.image_execute(handle, p, ...);
    
    // 白盒路径:per-warp 步进累积
    uint64_t pc, cycles, white_total = 0;
    int32_t status;
    int rc;
    do {
        rc = ptx.stepOneWarpInstruction(0, &pc, &status, &cycles);
        white_total += cycles;
    } while (rc == 0);
    
    // 关键断言:cycle 差 ≤ 1(per Oracle §F.4)
    REQUIRE(std::abs((int64_t)white_total - (int64_t)black_total) <= 1);
}
```

## 9. 性能预期(per Oracle §D)

| 项 | 数值 |
|---|---|
| stepOneWarpInstruction 调度开销 | ~每条指令一次虚调用 |
| per-warp step vs 黑盒 image_execute | **10x-50x slowdown** |
| 仿真周期精度 | **±1 cycle**(双路径等价) |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — W5-W7 实施
