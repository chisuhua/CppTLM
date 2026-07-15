# CppTLM 端 HSK 响应（HSK-1/2 OK + HSK-3 偏好）

> **用途**: PTX-EMU 端 3 个 HSK 消息（hsk-1/2/3.md）的 CppTLM 端正式响应
> **关联**:
> - PTX-EMU [`openspec/changes/cpptlm-d1-full/hsk-1.md`](../../../../PTX-EMU/openspec/changes/cpptlm-d1-full/hsk-1.md)（已发出）
> - PTX-EMU [`openspec/changes/cpptlm-d1-full/hsk-2.md`](../../../../PTX-EMU/openspec/changes/cpptlm-d1-full/hsk-2.md)（已发出）
> - PTX-EMU [`openspec/changes/cpptlm-d1-full/hsk-3.md`](../../../../PTX-EMU/openspec/changes/cpptlm-d1-full/hsk-3.md)（候选已确定，待评审）
> **CppTLM 端 commit**: `7ed12c2`（综合计划 §2.0 前置条件表同步）+ `97a5d65`（C1 P0 openspec tasks.md 重构）+ `0941183`（Task #5 注释回填 HSK-1 hash）

---

## ✅ [HSK-1 OK] CppTLMBridge 头文件确认

**收到的内容**:
- Commit hash: `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`（PTX-EMU commit `9be56f8f` 锁定 hsk-1.md）
- ABI path: `include/cudart/cpptlm_bridge.h`
- `CPPTLMBRIDGE_VERSION`: 1
- 5 个虚方法签名（含 12 参数 `submit_kernel`）字节级与 CppTLM 综合计划 §2.1 Task #1 一致

**CppTLM 端验证**:
- [x] ABI 字节级对比 `include/cudart/cpptlm_bridge.h` ↔ 综合计划 §2.1 Task #1：✅ 匹配
- [x] 12 端点（PipelineId 6 + TcPrecision 6）`static_assert` 编译期拦截设计：✅ 已记录于综合计划 §3.2 Task #C3
- [x] `static_assert(sizeof(cudaStream_t) <= sizeof(uint64_t))`：✅ 保留
- [x] `extern CppTLMBridge* g_cptlm_bridge`：✅ 保留
- [x] include 依赖（`<cstddef>` + `<cstdint>` + `<cuda_runtime.h>`）：✅ 零 CppTLM 依赖

**CppTLM 端行动**:
- CppTLM 端 `MemoryBridge` 实现（#C1）将引用 PTX-EMU `8dc000ec` commit hash（通过 `ExternalProject_Add` 或 HSK-3 选定方式）
- CppTLM `MemoryBridge::version()` 实现返回 `CPPTLMBRIDGE_VERSION = 1`
- CppTLM CI 加入 12 端点双重 `static_assert`（CppTLM 端 + PTX-EMU 端同步）

**状态**: ✅ **CppTLM 端已确认 HSK-1**——可立即消费

---

## ✅ [HSK-2 OK] ANTLR4 版本 4.13.2 满足契约

**收到的内容**:
- ANTLR4 vendored 目录: `antlr4/antlr4-cpp-runtime-4.13.2-source/`（4 源之一）
- `antlr-4.13.2-complete.jar`
- `AGENTS.md` §已知限制: "ANTLR 版本：4.13.2（antlr-4.13.2-complete.jar）"
- 根 `README.md`: "ANTLR 版本：4.13.2 完全 vendored"
- `.github/copilot-instructions.md`: "ANTLR 运行时来自 antlr4/antlr4-cpp-runtime-**4.13.2**-source"（已从 4.13.1 笔误修正为 4.13.2）

**CppTLM 端验证**:
- [x] 4 权威源全为 4.13.2（vendored + AGENTS.md + README.md + copilot-instructions.md）：✅ 一致
- [x] 满足综合计划 §2.1 Task #5 下限 `>= 4.13.2`：✅ **正好匹配**
- [x] CppTLM CI 不会被牵连（PTX-EMU ANTLR4 完全 vendored，无 apt/yum install 依赖）：✅
- [x] 升级流程（半年 review + fork branch + 全量回归 + 通知 CppTLM 同步）已记录于 PTX-EMU ADR-0021 D-PTX-4

**CppTLM 端行动**:
- 综合计划 §2.1 Task #5 注释已更新（commit `0941183`）：`>= 4.13.2` → `4.13.2` HSK-2 已满足
- CppTLM CI 无需调整：链接 `libcpptlm_cudart.so` 时 ANTLR4 不进入 CppTLM CI
- 监控点：PTX-EMU 未来 ANTLR4 升级时重新发出 HSK-2 通知 CppTLM 同步

**状态**: ✅ **CppTLM 端已确认 HSK-2**——Task #5 ANTLR4 契约完全满足

---

## 🟡 [HSK-3 待评审] CMake 集成方式偏好

**收到的内容**（PTX-EMU 候选 3 选项）:

| 选项 | 方式 | 优点 | 缺点 |
|:---:|------|------|------|
| **1** | `ExternalProject_Add` pin commit | 版本强制、build 隔离、零 ABI 漂移 | 首次 build 需访问 CppTLM 仓库；CI/CD 需 credentials |
| **2** | `find_library` + 环境变量 | build 完全解耦；适合容器化部署 | 手动管理安装路径；多版本易混淆 |
| **3** | `pkg-config` | 标准 Linux 工具集成 | Windows/macOS 需替代方案；PTX-EMU 主要 Linux x86_64 |

**PTX-EMU 倾向**: 选项 1（`ExternalProject_Add`），理由：CI/CD 简单、版本 pin 强制、build 隔离、single source of truth

**CppTLM 端评估**:

| 维度 | 选项 1 | 选项 2 | 选项 3 |
|------|:------:|:------:|:------:|
| **CppTLM 改动量** | 极小（无需 install 基础设施） | 需加 `make install` + `cpptlm-config.cmake` | 需加 `.pc` 文件生成 |
| **版本可追溯性** | git commit（精确） | 用户管理（手动） | 需 pkg-config 仓库约定 |
| **跨平台** | 全平台 | 全平台 | Linux 优先 |
| **CI/CD 集成** | 需 `credentials`（HTTPS token） | 需预安装 | 需预安装 |
| **对现有 CppTLM 构建系统影响** | 零侵入 | 需新增 install 规则 | 需新增 .pc 生成规则 |
| **长期可维护性** | 高 | 中 | 中 |

**CppTLM 决策**: ✅ **选项 1（ExternalProject_Add）**

**理由**:
1. **零侵入**: CppTLM 端无需新增 install 基础设施，PTX-EMU 直接 `git fetch` + `git checkout <commit>` + `cmake --build` 自包含构建
2. **精确版本控制**: `GIT_TAG <commit_hash>` 锁定 PTX-EMU 端所用 CppTLM commit，零 ABI 漂移风险
3. **单一真值源**: cpptlm_bridge.h + cpptlm_core + libcpptlm_cudart.so 来自同一 commit
4. **CppTLM 端零阻塞**: 综合计划 §2.1 Task #5 已有 cmake 草案，HSK-3 确认后 CppTLM 端无需再调整
5. **升级简单**: CppTLM 端发出新 commit hash，PTX-EMU 端 bump `GIT_TAG` 即可

**CppTLM 端行动**:
- CppTLM 端 git 仓库保持 public 可访问（`https://github.com/chisuhua/CppTLM.git`）
- CppTLM CI 构建产物（`libcpptlm_core.a` + 头文件）随 `git archive` + `cmake --build` 自动生成
- 文档：CppTLM 端 README 需补充"如何被 PTX-EMU ExternalProject_Add 引用"章节（待办）

**状态**: ✅ **CppTLM 端确认 HSK-3 选项 1（ExternalProject_Add）**——PTX-EMU 端按选项 1 实施即可

---

## 下一步行动清单

### CppTLM 端立即执行

- [x] HSK-1 OK 确认（ABI 字节级验证 + 12 端点 static_assert 设计）
- [x] HSK-2 OK 确认（ANTLR4 4.13.2 满足 Task #5 下限）
- [x] HSK-3 偏好确认（选项 1 ExternalProject_Add）
- [ ] CppTLM README 补充"PTX-EMU 端 ExternalProject_Add 引用"章节（待办）
- [ ] Phase 0.5 baseline worktree 建立：`git worktree add .worktrees/feature/d1-full -b feature/d1-full-impl`
- [ ] openspec C1 P0 修复（已完成 commit `97a5d65`，已 push）

### PTX-EMU 端立即执行

- [ ] HSK-3 实施 ExternalProject_Add（按 CppTLM 端选定的选项 1）
- [ ] Phase 0.5 baseline worktree 建立
- [ ] Phase 1: cpptlm_bridge.h 头文件 commit（已 commit `8dc000ec`）
- [ ] Phase 2-7: 实施 6 模块 + Adapter + CMake 集成

### 同步点

- [ ] D1: 双端开始 P0 实施（同步 commit hash）
- [ ] D5 EOD: G-F0 vector_add 烟雾测试跑通（双端联合验证）
- [ ] D8: P1 阶段完成（4 Adapter 就绪）
- [ ] D14: P3 阶段完成（5 类 microbenchmark + 全量回归 + 文档同步）

---

## 综合状态总览

| HSK | PTX-EMU 端 | CppTLM 端 | 整体 |
|-----|-----------|-----------|------|
| **HSK-1** ABI 头文件 | ✅ 已发出（commit `8dc000ec`） | ✅ **已确认** | ✅ 完成 |
| **HSK-2** ANTLR4 4.13.2 | ✅ 已满足（4 源一致） | ✅ **已确认**（满足 Task #5 下限）| ✅ 完成 |
| **HSK-3** CMake 集成 | ⏳ 候选 ExternalProject_Add | ✅ **已确认**选项 1 | ✅ 完成 |
| **C1 P0** openspec tasks.md 重构 | n/a | ✅ commit `97a5d65`（已 push） | ✅ 完成 |
| **§2.0 前置条件表** | n/a | ✅ commit `7ed12c2`（已 push） | ✅ 完成 |

**整体评级**: 🟢 **可开工**

---

**最后更新**: 2026-07-15
**CppTLM commit 链路**:
- `7ed12c2` §2.0 前置条件表同步
- `97a5d65` C1 P0 openspec tasks.md 重构
- `0941183` Task #5 注释回填 HSK-1 hash
- `9e9f0d4` PTX-EMU-README §10

**PTX-EMU 端引用**:
- commit `8dc000ec` ABI 真值源
- commit `9be56f8f` hsk-1.md 锁定
