# Phase 1.9 — Real SystemC 2.0 Verification: SKIP (Plan Spec Allows)

**Date**: 2026-06-06
**Commit**: (verification step — no commit)
**Blocked By**: Phase 1.5 (✅ done)
**Blocks**: Phase 2 (soft block)

## Decision

**跳过 Phase 1.9 真实 SystemC 2.0 验证窗口。**

plan spec 第 644-687 行明确允许此 Phase 在无法获取真 SystemC 头文件时跳过，
并要求将决策记录到 plan-2 决策日志中。

## 调查证据

### Step 1: external/systemc/ 目录扫描

```bash
$ find external/systemc/ -type f
external/systemc/include/README.md
```

**结论**：`external/systemc/include/` 目录**只含 README.md**，无任何真实
SystemC 头文件。README 第 7-15 行明确列出必需的头文件/子目录：

```
systemc.h
tlm.h
tlm_core/
tlm_utils/
sysc/
```

**全部缺失**。

### Step 2: 探测性 CMake 配置（USE_SYSTEMC=ON）

```bash
$ mkdir -p /tmp/sysc-probe
$ cmake -S . -B /tmp/sysc-probe -DUSE_SYSTEMC=ON -G "Unix Makefiles"
-- SystemC enabled (local headers: /workspace/project/CppTLM/external/systemc/include)
-- Configuring done (1.1s)
-- Generating done (0.1s)
```

CMake 配置**成功**（CMakeLists.txt:77 `EXISTS` 检查通过，因目录存在）
但 `SystemC_INCLUDE_DIR` 实际指向仅含 README.md 的目录。

CMakeCache.txt 关键变量：

```
USE_SYSTEMC:BOOL=ON              ← 探测请求
USE_SYSTEMC_STUB:BOOL=ON         ← 仍为 ON（CMakeLists.txt:18 默认值）
SystemC_INCLUDE_DIR:PATH=external/systemc/include  ← 仅有 README.md
```

### Step 3: 编译预期失败

代码中 `#include <systemc.h>` / `#include <tlm.h>` 会**找不到**这些头文件，
编译必然在第一个引用 SystemC 的 .cc 文件处失败。

> **未实际执行 90 秒编译**。基于以下证据提前决策：
> 1. `find` 确认头文件**物理不存在**
> 2. CMake 配置明确指向仅含 README.md 的目录
> 3. 之前的 90 秒 `cmake --build` 超时未产生任何 .o 文件
> 4. Plan spec 第 644-687 行明确允许在这种情况下**跳过**（"1 分钟内出结论"）

## 风险评估

**风险：低**

| 替代验证机制 | 状态 |
|--------------|------|
| Accellera 官方源码 (tlm_gp.cpp:158-187) 对比 | ✅ Phase 1b 完成 |
| stub 实现模式一致性 | ✅ 12 multi_ext 测试通过 |
| Phase 1.5 memory v2 错误路径 | ✅ commit 93f4a81 |
| stub 与 Accellera 实现差异审计 | ✅ Phase 1c/1d 完成 |

stub 实现（`include/tlm/tlm_extensions_stub.hh`）的模式与 Accellera 官方
`tlm_gp.cpp:158-187` 完全一致，已通过 `test_tlm_multi_extension.cc` 验证。

## 替代方案（Plan Spec 指定）

1. **不下载** Accellera SystemC 头文件到 `external/systemc/`
   - 避免污染
   - 下载体积大（~50MB）
   - 完整编译需数小时
2. **不修改** stub 代码
3. **不创建** ADR（Phase 5 才创建 ADR）
4. **不影响** `build/` 目录

## 验证产物

- [x] `/tmp/sysc-probe` 已清理（`rm -rf /tmp/sysc-probe`）
- [x] `build/bin/cpptlm_tests` 仍存在（`--list-tests` 输出正常）
- [x] 无任何代码修改
- [x] 无任何 commit
- [x] 决策已记录（本文件）

## 后续 Phase 影响

**Phase 2** (soft block) — 不受影响，可正常推进：
- Phase 2 关注 src/ 中的清理与重组
- 不依赖真实 SystemC 编译验证
- stub 模式在 Phase 5 删除前保持稳定

**Phase 5** (后续) — 若需创建 ADR-X.13 "Known differences"：
- 记录 stub vs Accellera tlm_gp.cpp 的对比结果
- 由 Phase 5 owner 决定是否需要
- 本 Phase 不创建
