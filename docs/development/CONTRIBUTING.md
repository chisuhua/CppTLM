# CppTLM Contributing Guide

> **For**: CppTLM 框架贡献者 (新模块作者 / bug 修复 / 文档改进)
> **最后更新**: 2026-06-22

本指南汇总新贡献者需要遵循的工程实践，重点说明 pre-commit 钩子的安装与日常使用。

---

## 1. 开发环境

参照 [`docs/guide/DEVELOPER_GUIDE.md`](../guide/DEVELOPER_GUIDE.md) §1 快速开始：

- CMake ≥ 3.16
- C++17 兼容编译器 (GCC 9+, Clang 10+)
- ccache (可选，推荐)
- Ninja (加速构建)

---

## 2. Pre-commit 钩子 (clang-format + 文档同步)

`CppTLM` 使用 [pre-commit](https://pre-commit.com/) 在每次 commit 前自动校验：

| Hook ID | 作用 | 阻塞条件 |
|---------|------|----------|
| `docs-sync-check` | 验证 `AGENTS.md` / `docs/ONBOARDING.md` / `roadmap.md` / `scripts/README.md` 中提及的路径真实存在 | 路径缺失 |
| `clang-format` | 对 C++ 源文件运行 `scripts/build/format.sh --check` | 任何 `.cc`/`.hh`/`.h`/`.cpp`/`.hpp` 未格式化 |

### 2.1 安装

```bash
# 1. 安装 pre-commit (Python 工具)
pip install pre-commit

# 2. 安装 clang-format (系统包)
sudo apt install clang-format   # Ubuntu/Debian
brew install clang-format        # macOS

# 3. 在仓库根目录启用钩子
cd /path/to/CppTLM
pre-commit install
```

### 2.2 日常使用

启用后，每次 `git commit` 会自动运行钩子：

- ✅ **钩子通过** → commit 正常进行
- ❌ **钩子失败** → commit 被阻止，先修复再重试
  - `clang-format` 失败：运行 `./scripts/build/format.sh` 自动修复，然后重新 `git add` + `git commit`
  - `docs-sync-check` 失败：检查文档中提及的路径，删除或修正

### 2.3 紧急绕过

仅在以下情况使用 `--no-verify`：

- 钩子本身有 bug（请同时开 issue）
- 修复过程中需要先 commit 后修复

```bash
git commit --no-verify -m "fix: ..."
```

**禁止**日常绕过 — 钩子的存在是为了防止格式漂移和文档过期。

### 2.4 手动运行

```bash
# 对所有文件运行
pre-commit run --all-files

# 只对 clang-format 运行
pre-commit run clang-format --all-files

# 仅检查（不修改）
./scripts/build/format.sh --check
```

---

## 3. 代码规范

参照 `AGENTS.md` §CONVENTIONS：

- **缩进**: 4 空格 (`.clang-format: IndentWidth: 4`)
- **命名**: CamelCase 类 / camelCase 函数 / snake_case 变量 / SCREAMING_SNAKE_CASE 宏
- **注释**: 中文，文件头必含功能/作者/日期

clang-format 钩子会自动应用上述缩进/格式规则。

---

## 4. 测试规范

- 新模块必须配 C++ Catch2 测试 (位于 `test/test_<module>.cc`)
- 修改现有模块须跑全测试：`./build/bin/cpptlm_tests --reporter compact`
- 新功能优先 TDD：先写失败测试，再实现 (见 `.opencode/skills/cpptlm-debug/SKILL.md`)

---

## 5. 文档同步

修改任何被核心文档 (AGENTS.md / ONBOARDING.md / roadmap.md / scripts/README.md) 引用的路径时：

- 若路径仍存在：直接修改
- 若路径被删除/归档：在 `scripts/test/docs_sync_check.sh` 的 `VIRTUAL_PATHS` 数组添加条目 (而非删除文档段落)

`docs-sync-check` 钩子会阻止路径漂移。

---

## 6. 提交规范

参见 `AGENTS.md` §CONVENTIONS 提交风格 + `.opencode/skills/git-master/SKILL.md`。

---

## 7. 调试支持

测试失败或消息传递断链时，auto-load `.opencode/skills/cpptlm-debug/SKILL.md` 获取：

- 4 件套验证流程
- 6 步响应循环模板
- 文件级日志模式 (`static FILE* diag = fopen(...)`)

---

**维护**: CppTLM 开发团队  
**反馈**: 通过 GitHub Issues / PR
