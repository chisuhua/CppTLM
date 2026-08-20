# ptx-emu-submodule-mvp 微架构文档

> **类别**: GPU > PTX-EMU Adapter · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **Header**: `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + **`.cc` 为唯一 PTX-EMU 头 include 位置(编译防火墙)**
> **位置**: DGpuBoardTLM 内部组件
> **蓝图来源**: PTX-EMU `cpptlm_module.h` 8 ABI + 3 multi-kernel API + ADR-X.16 D2/D4 + DP4=C 白盒永久禁用
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D3
> **关联模块**: [`cuda-core-adapter.md`](./cuda-core-adapter.md)
> **首版 commit**: 🔵 W1-2 实施 · **最近更新**: 2026-08-20
> **维护者**: CppTLM Team (Sisyphus)

> **关联调研**:
> - PTX-EMU submodule `external/PTX-EMU@87820951`(per DP1=B 决策,2026-08-13 audit commit):
>   - `include/cudart/cpptlm_module.h:18-52` — 8 ABI 真相源(`CPPTLM_MODULE_VERSION=2`)
>   - `include/ptxsim/sm_context.h:59` — `EXE_STATE exe_once()`
>   - `include/ptxsim/warp_context.h:62` — `execute_warp_instruction(StatementContext&, int)`
> - **🔴 白盒 API 状态**:`stepOneWarpInstruction` API **当前不存在**(2026-08-20 全仓 grep 0 命中)— per DP4=C 决策,MVP 永久仅黑盒

---

## 1. 设计目标

`PtxEmuSubmoduleMVP` 是 **PTX-EMU 集成的 adapter**(per ADR-X.16 D2/D4),负责:
1. **编译防火墙**:本模块 `.cc` 是 CppTLM 中**唯一** include PTX-EMU 头文件的位置,其他模块仅前向声明
2. **静态链接**(非 dlopen):`git submodule` 引入 `external/PTX-EMU`,`add_subdirectory` 静态链接到 `cpptlm_core`
3. **8 ABI 透传**:封装 PTX-EMU `cpptlm_module.h` 的 `ptxemu_image_load` / `ptxemu_image_execute` 等 8 个 C ABI
4. **白盒 API 封装**(可选):`stepOneWarpInstruction(warp_id, &pc, &status, &cycle_count)` 调 PTX-EMU `SMContext::stepOneWarpInstruction`(若 PTX-EMU 端新增)
5. **CudaCoreAdapter 接口**:提供 `image_execute` + `stepOneWarpInstruction` 给 CudaCoreAdapter

**MVP vs v0.5 完整版简化**:
- ✅ 保留:submodule 静态链接 + adapter 编译防火墙
- ✅ 保留:8 ABI 透传(黑盒路径)
- ❌ 裁剪:白盒 `stepOneWarpInstruction` API 推迟(需 PTX-EMU 维护者接受新 API)
- � 裁剪:ScoreboardTLM + PipelineTLM + TensorCoreTLM D1-Full Adapter 注入(MVP 不实施,per `ADR-NV-02 Status Update`)
- ❌ 裁剪:WarpScheduler Adapter(由 PTX-EMU 自带)

---

## 2. 架构概览

### 2.1 编译防火墙(关键约束)

```
CppTLM
├── include/tlm/gpu/
│   ├── ptx_emu_submodule_mvp.hh       ← 仅前向声明,不 include PTX-EMU 头
│   ├── cuda_core_adapter_mvp.hh       ← 不 include PTX-EMU 头
│   ├── command_processor_mvp.hh      ← 不 include PTX-EMU 头
│   └── ...
├── src/tlm/gpu/
│   ├── ptx_emu_submodule_mvp.cc       ← ⭐ 唯一 include PTX-EMU 头
│   │                                    #include <ptxsim/sm_context.h>
│   │                                    #include <ptxsim/warp_context.h>
│   │                                    #include <cudart/cpptlm_module.h>
│   ├── cuda_core_adapter_mvp.cc       ← 不 include PTX-EMU 头
│   └── ...
└── external/PTX-EMU/                  ← git submodule
    ├── include/cudart/cpptlm_module.h
    ├── include/ptxsim/sm_context.h
    └── include/ptxsim/warp_context.h
```

**验证命令**:
```bash
git grep "include.*ptxsim" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅命中 src/tlm/gpu/ptx_emu_submodule_mvp.cc
```

### 2.2 数据流

```
CudaCoreAdapter::dispatch_blackbox(image_handle, params)
    │
    ▼
PtxEmuSubmoduleMVP::image_execute(handle, grid, block, shared, args, argc)
    │
    ▼ (C ABI 透传)
PTX-EMU libptxemu_device.so (静态链接)
    └─ ptxemu_image_execute(handle, grid, block, shared, args, argc)
        │
        ▼
PTX-EMU::GPUContext::submit_kernel_request(...)
    │
    ▼
PTX-EMU::SMContext::exe_once() × N cycles
    │
    ▼ (per cycle, PTX-EMU 内部)
PTX-EMU::WarpContext::execute_warp_instruction(StatementContext&, target_pc)
    ├─ Step A: Scoreboard hazard check
    ├─ Step B: Pipeline/TC latency query (内部)
    └─ Step C: Scoreboard release
    │
    ▼
Kernel 完成 → on_complete 回调 → CudaCoreAdapter → TMU → CQ
```

---

## 3. 接口(Public API)

```cpp
// include/tlm/gpu/ptx_emu_submodule_mvp.hh
// 仅前向声明,不 include PTX-EMU 头
namespace ptxsim { class SMContext; class WarpContext; }

class PtxEmuSubmoduleMVP {
public:
    /// Image handle 类型(per PTX-EMU 8 ABI)
    using ImageHandle = uint64_t;

    explicit PtxEmuSubmoduleMVP();
    ~PtxEmuSubmoduleMVP();

    /// 初始化 submodule 路径(从 DGpuBoardTLM JSON params 注入)
    /// @param ptx_emu_root PTX-EMU submodule 绝对路径
    void init(const std::string& ptx_emu_root);
    void shutdown();

    // === 黑盒路径(MVP 默认) ===

    /// ABI 1: ptxemu_image_load
    ImageHandle image_load(const uint8_t* image_bytes, size_t size);

    /// ABI 2: ptxemu_image_kernel_name(handle, buf, buf_size) → required length
    int32_t image_kernel_name(ImageHandle handle, char* buf, size_t buf_size);

    /// ABI 3: ptxemu_image_execute(handle, grid, block, shared, args, argc)
    int32_t image_execute(ImageHandle handle,
                          uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                          uint32_t block_x, uint32_t block_y, uint32_t block_z,
                          size_t shared_mem_bytes,
                          void** kernel_args, size_t args_count);

    /// ABI 4: ptxemu_image_unload(handle) → status
    int32_t image_unload(ImageHandle handle);

    /// ABI 5: ptxemu_module_version() → version
    int32_t module_version() const;

    /// ABI 6: ptxemu_image_kernel_count(handle) → count
    int32_t image_kernel_count(ImageHandle handle);

    /// ABI 7: ptxemu_image_kernel_name_at(handle, idx, buf, buf_size) → required length
    int32_t image_kernel_name_at(ImageHandle handle, uint32_t idx,
                                char* buf, size_t buf_size);

    /// ABI 8: ptxemu_image_execute_named(handle, name, ...) → status
    int32_t image_execute_named(ImageHandle handle, const char* kernel_name,
                               uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                               uint32_t block_x, uint32_t block_y, uint32_t block_z,
                               size_t shared_mem_bytes,
                               void** kernel_args, size_t args_count);

    // === 白盒精度路径(S3 启用,需 PTX-EMU 新 API) ===

    /// per-warp instruction step(可选)
    /// @return 0=执行中,>0=warp 完成,<0=错误
    /// @param warp_id 要执行的 warp(0 .. num_warps-1)
    /// @param out_pc 输出当前指令 PC
    /// @param out_status 输出指令完成状态(0=继续,非 0=完成/错误)
    /// @param out_cycle_count 输出本次 step 消耗的 cycle 数
    int32_t stepOneWarpInstruction(uint32_t warp_id,
                                   uint64_t* out_pc,
                                   int32_t* out_status,
                                   uint64_t* out_cycle_count);

    /// 双路径内部一致性验证(MVP 黑盒 + 白盒结果 byte-identical)
    bool verify_dual_path_consistency(uint32_t max_warp_steps);

    /// JSON params 注入
    void set_enable_whitebox(bool enabled) { enable_whitebox_ = enabled; }

    // === 测试/统计 ===
    bool is_initialized() const { return initialized_; }
    uint64_t image_load_count() const { return image_load_count_; }
    uint64_t image_execute_count() const { return image_execute_count_; }

private:
    // 持有 PTX-EMU 内部实例(前向声明,实现见 .cc)
    ptxsim::SMContext* sm_context_ = nullptr;  // 多 SM 时为 vector

    bool initialized_ = false;
    bool enable_whitebox_ = false;
    std::string ptx_emu_root_;

    // 统计
    uint64_t image_load_count_ = 0;
    uint64_t image_execute_count_ = 0;
};
```

---

## 4. 行为流程

### 4.1 init()

```cpp
void PtxEmuSubmoduleMVP::init(const std::string& ptx_emu_root) {
    ptx_emu_root_ = ptx_emu_root;

    // 1. 验证 submodule 路径有效(per Phase A 修复 S5)
    //    检查 ptx_emu_root 包含 expected include path(<ptxsim/sm_context.h>)
    //    失败时:抛 std::runtime_error,阻止 ModuleFactory 继续 instantiateAll
    validate_ptx_emu_root(ptx_emu_root);

    // 2. 验证 vendored 副本就绪(per UsrLinuxEmu ADR-090 v2 §D5)
    //    cpptlm_attach_bridge() 确认 PTX-EMU HSK-1 真相源与 vendored 副本版本一致
    //    失败时:log fatal + abort(防止 submodule 漂移)
    if (cpptlm_attach_bridge() != 0) {
        throw std::runtime_error("cpptlm_attach_bridge failed: PTX-EMU submodule version mismatch");
    }

    // 3. **per DP4=C 决策**:强制 enable_whitebox_ = false
    //    MVP 永久仅黑盒路径,不需要 stepOneWarpInstruction API
    //    即使外部 JSON params 设置 enable_whitebox_path=true,init() 也强制改为 false
    enable_whitebox_ = false;

    initialized_ = true;

    // 注:PTX-EMU 子模块静态链接到 cpptlm_core,无需显式 dlopen
    //     PTX-EMU::GPUContext 由 main.cpp 单例管理
    //     PtxEmuSubmoduleMVP 通过 cpptlm_set_driver() 或全局指针访问
    //
    // 白盒路径代码占位(per DP4=C):
    //   - CudaCoreAdapter::dispatch_whitebox() + PtxEmuSubmoduleMVP::stepOneWarpInstruction() 接口保留
    //   - 但 MVP 范围内 enable_whitebox=false 强制不调用
    //   - per-warp 精度推到 v0.5 完整版(per ADR-X.16 P0'-P4')
}
```

### 4.2 image_execute()(黑盒 MVP 默认路径)

```cpp
int32_t PtxEmuSubmoduleMVP::image_execute(ImageHandle handle,
                                           uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                           uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                           size_t shared_mem_bytes,
                                           void** kernel_args, size_t args_count) {
    if (!initialized_) {
        return -1;  // 错误:未初始化
    }

    image_execute_count_++;

    // 8 ABI 透传(per PTX-EMU cpptlm_module.h:22-26)
    return ptxemu_image_execute(handle,
                                grid_x, grid_y, grid_z,
                                block_x, block_y, block_z,
                                shared_mem_bytes,
                                kernel_args, args_count);
}
```

### 4.3 stepOneWarpInstruction()(白盒路径,S3 启用)

```cpp
int32_t PtxEmuSubmoduleMVP::stepOneWarpInstruction(uint32_t warp_id,
                                                   uint64_t* out_pc,
                                                   int32_t* out_status,
                                                   uint64_t* out_cycle_count) {
    if (!initialized_ || !enable_whitebox_) {
        return -1;  // 错误:未启用
    }
    if (!sm_context_) {
        return -2;  // 错误:无 SM context
    }

    // 调 PTX-EMU SMContext::stepOneWarpInstruction(待 PTX-EMU 端新增)
    return sm_context_->stepOneWarpInstruction(warp_id, out_pc, out_status, out_cycle_count);
}
```

---

## 5. 关键设计取舍

### 5.1 静态链接(非 dlopen)(per ADR-X.16 D2)

- **dlopen 模式**(v0.4 黑盒):通过 `libptxemu_device.so` dlsym 8 个函数
  - 缺点:ABI 漂移风险、运行时错误、C ABI 限制
- **submodule + 静态链接**(v0.5/MVP):git submodule + `add_subdirectory` + 静态链接
  - 优点:编译期验证、C++ 源码契约、零运行时错误

### 5.2 编译防火墙(per ADR-X.16 D4)

**唯一性约束**:`ptx_emu_submodule_mvp.cc` 是 CppTLM 中**唯一** include PTX-EMU 头的 `.cc`。
- 其他 `.cc/.hh` 仅前向声明 `namespace ptxsim { class SMContext; class WarpContext; }`
- 测试: `git grep "include.*ptxsim"` 仅命中 `ptx_emu_submodule_mvp.cc`

### 5.3 黑盒 MVP 永久优先(per ADR-X.17 D2 + DP4=C 决策)

- **MVP 永久 `enable_whitebox=false`**(per DP4=C 决策 2026-08-20):黑盒路径是唯一启用路径
- **白盒路径代码占位**:接口保留(`stepOneWarpInstruction` 框架存在)但 `init()` 强制 `enable_whitebox_ = false`
- **per-warp 精度推到 v0.5 完整版**(per ADR-X.16 P0'-P4'):不损失最终能力,仅调整交付时机
- **简化协作**:不需要 HSK-7 公告,不需要 PTX-EMU 端新增 API,跨仓风险降低

### 5.4 不实施 D1-Full Adapter 注入(per `ADR-NV-02 Status Update`)

per ADR-NV-02 Status Update (2026-08-18):
- D1-Full 4 Adapter(WarpScheduler/Scoreboard/Pipeline/TensorCore)**不实施**
- 12 SM 模块保留 Legacy 标签(`[legacy]`)
- MVP 不注入 ScoreboardTLM/PipelineTLM(由 PTX-EMU 自带)
- CppTLM 端只承接 PCIe 设备语义 + VRAM backing + 8 ABI 透传

### 5.5 ANTLR4 不在 CppTLM scope(per HSK-6 §3.3)

- PTX-EMU submodule 自带 ANTLR4(4.13.2)运行时
- 编译期 PTX-EMU 构建依赖 ANTLR4 4.13.2(自动)
- CppTLM 端不直接依赖 ANTLR4

---

## 6. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_ptx_emu_submodule_mvp.cc` | `[ptx-emu-v05][mvp]` | submodule init + 8 ABI 单测 + 编译防火墙 |
| `test_ptx_emu_submodule_mvp_whitebox.cc` | `[ptx-emu-v05][mvp][whitebox]` | 白盒 stepOneWarpInstruction(S3) |

**验收标准**(per ADR-X.17 G-MVP-1, G-MVP-6):
- submodule init PASS
- 8 ABI 全部 PASS(image_load/image_kernel_name/image_execute/image_unload/module_version/image_kernel_count/image_kernel_name_at/image_execute_named)
- 编译防火墙 PASS(`git grep` 仅命中 `ptx_emu_submodule_mvp.cc`)
- 白盒路径(S3):per-warp step + cycle 跟踪 PASS

---

## 7. 实施路径

### 7.1 S1 MVP-Cut(W1-2)

1. `git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU`
2. `.gitmodules` 加入 PTX-EMU 入口
3. `CMakeLists.txt` 加 `add_subdirectory(external/PTX-EMU)`
4. 设置 `PTX_EMU_BUILD_TESTS=OFF` + `PTX_EMU_BUILD_SHARED=OFF`
5. 设置 `-fvisibility=hidden`(per Oracle §E.1 风险 R5)
6. 新建 `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `src/tlm/gpu/ptx_emu_submodule_mvp.cc`(~200 LOC)
7. 8 ABI 透传实现
8. 新建 `test/test_ptx_emu_submodule_mvp.cc`(8 ABI + 编译防火墙)

### 7.2 S3 Warp-Precision(W5-6,per DP4=C **白盒路径代码占位,不启用**)

1. **白盒路径代码占位**:保留 `stepOneWarpInstruction` 框架(代码 + 接口 + 测试桩)
   - 但 init() 强制 `enable_whitebox_ = false`
   - dispatch_whitebox() 调用前检查 `enable_whitebox_`,未启用时立即返回 -1
2. **不发起 HSK-7 公告**:per DP4=C 决策,MVP 永久仅黑盒
3. **测试覆盖**:`test/test_ptx_emu_submodule_mvp_whitebox.cc` 测试白盒 API stub 行为(返回 -1,不实际调用 PTX-EMU)
4. **v0.5 完整版**:per ADR-X.16 P0'-P4',届时再评估 HSK-N 公告

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | 编译防火墙破裂 | 低 | 高 | 严格 `git grep` + CI 拦截 |
| R3 | PTX-EMU 构建依赖扩散(ANTLR4 4.13.2) | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R4 | ~~PTX-EMU 维护者拒收 `stepOneWarpInstruction` API~~ | — | — | **per DP4=C 决策消除该风险**:MVP 永久仅黑盒路径,不依赖该 API |
| R5 | 8 ABI 透传 ABI 漂移 | 中 | 中 | 真相源锁定 `cpptlm_module.h:18-52` commit hash |
| R6 | submodule 包含 PTX-EMU 整个仓库(~大) | 中 | 中 | 仅构建 PTX-EMU `libptxemu_device.a`(无需 cudart_sim) |

---

## 9. 修订历史

- **2026-08-19**: 初版 — per ADR-X.17 D3 切片(MVP 4 阶段 S1+S3)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
