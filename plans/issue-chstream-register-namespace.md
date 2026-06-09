# Issue: `chstream_register.hh` namespace resolution failure (P2)

**发现时间**: 2026-06-07
**发现人**: Sisyphus (代码审查期间)
**严重度**: P2（不阻塞 RTL feature 自身功能，仅阻塞测试 build）
**关联审查**: `plans/commit-review-26b2d6c4-ae78969f.md`

---

## 一、问题描述

`include/chstream_register.hh` 第 61-63 行引用 `HybridCacheWrapper`（无命名空间），但该类在 `include/rtl/hybrid_cache_wrapper.hh` 中已移至 `namespace cpptlm { namespace rtl {` 命名空间内。

```cpp
// include/chstream_register.hh:61-63
ModuleFactory::registerObject<HybridCacheWrapper>("HybridCacheWrapper"); \
ChStreamAdapterFactory::get().registerAdapter<HybridCacheWrapper, \
    bundles::CacheReqBundle, bundles::CacheRespBundle>("HybridCacheWrapper");
```

编译器建议：

```
'HybridCacheWrapper' was not declared in this scope; did you mean 'cpptlm::rtl::HybridCacheWrapper'?
```

---

## 二、根因分析

| 时间 | 事件 | 影响 |
|------|------|------|
| `26b2d6c4` (2026-06-06) | 添加 `HybridCacheWrapper` PIMPL header（**全局命名空间**） | — |
| `7448447` (后续) | 重构 Wrapper + FragmentMapper 为 `ch_stream<BundleT>`，将 `HybridCacheWrapper` 移至 `namespace cpptlm::rtl` | 与兄弟文件 `fragment_mapper.hh`/`hybrid_cache_component.hh` 一致 |
| `0a06329` | 添加 `HybridCacheWrapper` 到 `REGISTER_CHSTREAM` 宏 | **使用旧的全限定名 `HybridCacheWrapper`（无命名空间）** → build 失败 |

`include/chstream_register.hh:14` 的 `#include "rtl/hybrid_cache_wrapper.hh"` 引入了该类，但因为它在 `cpptlm::rtl::` 命名空间内，宏展开时直接写 `HybridCacheWrapper` 无法解析。

---

## 三、影响范围

### 受影响的编译单元
- `test/test_config_inheritance.cc:32:5`（`REGISTER_CHSTREAM;` 宏展开）— 编译失败

### 受影响的目标
- `cpptlm_tests`（`test/CMakeFiles/cpptlm_tests.dir/test_config_inheritance.cc.o`）— 编译失败
- 进一步阻塞 `cpptlm_tests` 整体 link

### 不受影响
- `cpptlm_core`、`cpptlm_sim`、`cpptlm_cpu`、`cpptlm_traffic` 等其他目标（不直接 include `chstream_register.hh`）
- 任何不包括 `REGISTER_CHSTREAM` 宏的测试文件（包括新加的 `test/test_fragment_mapper.cc`）

### 完整错误信息（g++ 13.3.0）

```
/workspace/project/CppTLM/src/../include/chstream_register.hh:61:35: error: 'HybridCacheWrapper' was not declared in this scope; did you mean 'cpptlm::rtl::HybridCacheWrapper'?
/workspace/project/CppTLM/src/../include/chstream_register.hh:61:54: error: no matching function for call to 'ModuleFactory::registerObject<<expression error> >(const char [19])'
/workspace/project/CppTLM/src/../include/chstream_register.hh:61:54: error: template argument 1 is invalid
/workspace/project/CppTLM/src/../include/chstream_register.hh:63:59: error: no matching function for call to 'ChStreamAdapterFactory::get().registerAdapter<HybridCacheWrapper, bundles::CacheReqBundle, bundles::CacheRespBundle>(const char [19])'
```

---

## 四、建议修复方案

### 方案 A：使用全限定名（推荐，最小改动）

修改 `include/chstream_register.hh:61-63`：

```cpp
// 原代码（错误）
ModuleFactory::registerObject<HybridCacheWrapper>("HybridCacheWrapper");
ChStreamAdapterFactory::get().registerAdapter<HybridCacheWrapper, \
    bundles::CacheReqBundle, bundles::CacheRespBundle>("HybridCacheWrapper");

// 修复后
ModuleFactory::registerObject<cpptlm::rtl::HybridCacheWrapper>("HybridCacheWrapper");
ChStreamAdapterFactory::get().registerAdapter<cpptlm::rtl::HybridCacheWrapper, \
    bundles::CacheReqBundle, bundles::CacheRespBundle>("HybridCacheWrapper");
```

**优点**：
- 改动最小（4 处替换）
- 不影响其他模块
- 修复后整个测试集可正常 link

**风险**：低（仅影响类型名解析，运行时行为不变）

### 方案 B：在 `chstream_register.hh` 顶部添加 `using namespace`

```cpp
// 在 chstream_register.hh 顶部
namespace cpptlm { namespace rtl {
class HybridCacheWrapper;  // 仅前向声明
}}
using cpptlm::rtl::HybridCacheWrapper;
```

**优点**：宏定义更简洁

**风险**：`using` 在头文件全局污染符号表，可能影响其他宏。

**不推荐**。

### 方案 C：将 HybridCacheWrapper 移回全局命名空间

**不推荐**：违反 `7448447` 重构的设计意图（与兄弟文件保持 `cpptlm::rtl` 命名空间一致）。

---

## 五、修复工作量估算

| 任务 | 工作量 |
|------|--------|
| 修改 `chstream_register.hh` 4 处 | < 5 分钟 |
| 验证 build（`make cpptlm_tests`） | 5-10 分钟 |
| 验证测试（`ctest --test-dir build --output-on-failure`） | 1-2 分钟 |
| 提交（`fix(register): qualify HybridCacheWrapper with cpptlm::rtl namespace`） | 2 分钟 |
| **总计** | **~20 分钟** |

---

## 六、回归风险评估

| 风险项 | 评估 | 说明 |
|--------|------|------|
| 引入新的 include 依赖 | 无 | 仅修改类型名引用，不改 include |
| 运行时行为变化 | 无 | 模板参数类型不变（仍是 `cpptlm::rtl::HybridCacheWrapper`） |
| 影响其他模块 | 无 | 其他 TLM 模块（`CacheTLM`、`MemoryTLM` 等）仍在全局命名空间 |
| 与 `chstream_register.hh` 已注册的其他模块冲突 | 无 | `RouterTLM`/`NICTLM` 等已使用 `tlm::` 前缀，模式一致 |

---

## 七、建议提交信息模板

```
fix(register): qualify HybridCacheWrapper with cpptlm::rtl namespace

- 修复 chstream_register.hh 编译失败:
  HybridCacheWrapper 在 7448447 重构后移至 cpptlm::rtl 命名空间,
  但 chstream_register.hh:61-63 仍使用旧的无命名空间全限定名
- 阻塞症状: test_config_inheritance.cc:32 (REGISTER_CHSTREAM 宏展开) 失败
- 修复方案: 改用 cpptlm::rtl::HybridCacheWrapper 全限定名
- 验证: make cpptlm_tests link 成功 + 现有测试集无回归
```

---

## 八、关联

- 关联审查报告: `plans/commit-review-26b2d6c4-ae78969f.md`（本 issue 在该审查期间发现）
- 关联 commit: `0a06329 feat(register): add HybridCacheWrapper to REGISTER_CHSTREAM macro`（引入问题）
- 关联 commit: `7448447 refactor(rtl): update Wrapper + FragmentMapper for ch_stream<BundleT>`（类移至命名空间）
