# PTX-EMU `libcpptlm_cudart.so` 构建方案审查反馈

> **来源**: CppTLM 端 Oracle 审查  
> **日期**: 2026-07-23  
> **审查 Commit**: `adb12d77` (HEAD), 关键变更 `84212a9d` (统一 CppTLM 构建)  
> **审查范围**: CMakeLists.txt / cudart_sim.cpp / cpptlm_bridge.h / PtxEmuDriverShim

---

## 一、总体结论

**实现质量良好，核心机制正确，可以合并推进。** 发现 3 个必须修复的注释/可见性问题（均不影响功能）和 3 个建议改进项。

## 二、架构变更认可

PTX-EMU 将方案从"独立 `libcpptlm_cudart.so` + dlopen"变更为"`cpptlm_core.a` 通过 `--whole-archive` 直接链接进 `cudart.so`"——**这个变更合理且优于原方案**：

| 维度 | 原方案 | 实际实现 |
|------|--------|---------|
| 复杂度 | 独立 .so + dlopen/LD_PRELOAD | 单 .so，链接时强定义覆盖 |
| 运行时控制 | `option(BUILD_LIB_CPPTLM_CUDART)` | `EMU_COSIM=1` 环境变量 |
| 兼容性 | 需管理 .so 路径/加载时机 | 编译即就绪，零运行时配置 |

## 三、验证通过项 ✅

| 检查项 | 状态 | 证据 |
|--------|:--:|------|
| PIC 已启用 | ✅ | `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` 在 `add_subdirectory()` 之前 |
| 强定义覆盖 | ✅ | `nm -D libcudart.so.12.0 \| grep cpptlm_set_driver` → `T` (strong text) |
| 双向 ABI 打通 | ✅ | `cpptlm_set_driver` (强) + `cpptlm_attach_bridge` (弱→PTX-EMU 实现) |
| 运行时门控 | ✅ | `EMU_COSIM=1` → StubBridge auto-attach + auto-advance |
| G-D4 ABI 断言 | ✅ | 12 端点 + 4 签名 static_assert 已同步到 PTX-EMU 侧 |
| advance 修复 | ✅ | `has_pending_tasks()` 加入 EXIT 条件 |
| 双 enqueue | ✅ | cudaLaunchKernel 同时走 bridge submit + GPUContext enqueue |
| mark_complete 链 | ✅ | `on_complete` callback 链入 `g_ptx_emu_driver_shim->mark_complete()` |
| ceiling 保护 | ✅ | `PTX_EMU_MAX_ADVANCE_CYCLES` 默认 10M |

## 四、必须修复项 ⚠️

### P1: `cpptlm_set_driver` 声明缺少可见性宏

**文件**: `include/cudart/cpptlm_bridge.h:213`

**当前代码**:
```cpp
extern "C" void cpptlm_set_driver(void* shim, PtxEmuDriverApi api);
```

**问题**: 缺少 `PTXEMU_BRIDGE_API`。当前 `cudart.so` 无 `-fvisibility=hidden` 所以能工作，但若未来加可见性控制，此符号会被隐藏，导致弱符号覆盖失效。

**修复**:
```cpp
extern "C" PTXEMU_BRIDGE_API void cpptlm_set_driver(void* shim, PtxEmuDriverApi api);
```

> 与 `cpptlm_attach_bridge`/`cpptlm_detach_bridge` 保持一致的可见性标注。

---

### P2: 3 处注释与实现机制不一致

| # | 文件:行 | 当前注释 | 应改为 |
|---|---------|---------|--------|
| 1 | `include/cudart/cpptlm_bridge.h:178` | "CppTLM 的 libcpptlm_cudart.so 提供强定义" | "CppTLM 的 cpptlm_core (通过 --whole-archive 链接到 libcudart.so) 提供强定义" |
| 2 | `src/cudart/cudart_sim.cpp:314` | "CppTLM 的 libcpptlm_cudart.so 加载后其强定义接管" | "CppTLM 的 cpptlm_set_driver 强定义 (通过 --whole-archive 在 libcudart.so 中覆盖弱符号)" |
| 3 | `src/cudart/cpptlm_bridge/PtxEmuDriverShim.h:10` | "This file is only compiled when BUILD_LIB_CPPTLM_CUDART=ON" | "This file is always compiled (CppTLM is always linked since commit 84212a9d)" |

---

### P3: 构建目录残留文件

```bash
-rwxr-xr-x 41M build/lib/libcpptlm_cudart.so   # 旧方案残留
```

`src/cudart/cpptlm_bridge/cpptlm_cudart_lib.cpp` 已删除，但 `.so` 和 `.o` 仍留在 build 目录。

**修复**: `rm build/lib/libcpptlm_cudart.so && cmake --build build --target clean` 后重编。

---

## 五、建议改进项 💡

### S1: advance ceiling 耗尽时的错误处理可更友好

**位置**: `src/cudart/cudart_sim.cpp:1011`

当前 ceiling 耗尽时直接清空所有状态并返回 `cudaError_t(999)`。如果调用方不知晓此语义，会导致静默数据丢失。建议在错误日志中提示用户调高 `PTX_EMU_MAX_ADVANCE_CYCLES`：

```cpp
PTX_ERROR_EMU("cudaDeviceSynchronize: advance ceiling exhausted "
              "(max_cycles=%u, actual=%u). "
              "Set PTX_EMU_MAX_ADVANCE_CYCLES to increase limit.",
              max_cycles, actual);
```

### S2: 未来考虑拆分 cpptlm_core 链接粒度

当前 `--whole-archive` 将全部 CppTLM 模块（RouterTLM 805 行、ModuleFactory 604 行、topology_parser 等）链接进 `cudart.so`。功能无影响，但如果未来 CppTLM 拆分出 `cpptlm_core_minimal`（仅含 bridge + IPtxEmuDriver + MemoryBridge），可减小 .so 体积。**非阻塞，属于远期优化**。

### S3: 建议增加弱/强符号覆盖的自动化测试

当前符号覆盖正确性仅通过 `nm -D` 手动验证。建议在 CI 中增加一个脚本测试（例如 `nm -D libcudart.so | grep 'T cpptlm_set_driver'` 断言非空），防止未来误改链接配置导致退化。

---

## 六、行动清单

| 优先级 | 行动 | 仓库 | 预估 |
|:--:|------|:--:|------|
| 🔴 P1 | `cpptlm_bridge.h:213` 加 `PTXEMU_BRIDGE_API` | PTX-EMU | 1 行 |
| 🔴 P2 | 更新 3 处注释 | PTX-EMU | 3 行 |
| 🔴 P3 | 清理 build 残留 | PTX-EMU | 1 命令 |
| 🟡 S1 | advance ceiling 错误提示改进 | PTX-EMU | 2 行 |
| 🟢 S2 | cpptlm_core 拆分 | CppTLM | 远期 |
| 🟢 S3 | 符号覆盖 CI 测试 | PTX-EMU | 5 行脚本 |

**前三项修复后可立即推进 E2E 验证**，其余为优化项不阻塞进度。

---

*CppTLM 团队 · 2026-07-23*