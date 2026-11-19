# docs/user-guide/ — Python 用户手册

> **定位**: **用户手册** —— 面向「用 Python 驱动 CppTLM」的端到端使用场景。
> **与 [`../guide/`](../guide/) 的分工**（两者主题相邻但内容不重叠）：

| | [`../guide/`](../guide/) | 本目录 |
|---|---|---|
| **回答的问题** | 「这个功能**怎么用**？」 | 「我**要做一件事**，怎么把它跑起来？」 |
| **典型文档** | 快速开始 / 开发者指南 / 拓扑配置指南 / Python 工具链指南 / CEP 使用手册 | E2E 性能建模模板 / 层次化 SoC 配置 / ConfigBuilder API / 可视化 Dashboard |
| **读者** | 开发者 | 应用侧工程师 / 性能建模者 |

---

## 目录

| 文档 | 内容 |
|------|------|
| [`python-usage.md`](./python-usage.md) | Python 用户指南 —— 环境准备 · E2E SoC 性能建模模板（推荐起手）· 层次化 SoC 配置（§3，含 §3.5 `groups` / `module_groups` 边界）· ConfigBuilder API 参考 · 统一可视化 Dashboard |

> ⚠️ **路径稳定性**：`python-usage.md` 被不可变 ADR [`../adr/ADR-X.14-coherence-domains-stub.md`](../adr/ADR-X.14-coherence-domains-stub.md) 按路径引用（§3.5）⇒ **本文件路径与节号不得变动**。

---

## 相关

- [`../guide/PYTHON_TOOLING_GUIDE.md`](../guide/PYTHON_TOOLING_GUIDE.md) —— 校验器（`validator.py`）与配置生成器工具链
- [`../guide/GETTING_STARTED.md`](../guide/GETTING_STARTED.md) —— 首次构建与运行
- [`../superpowers/specs/HSK-9-baseline-tracker.md`](../superpowers/specs/HSK-9-baseline-tracker.md) —— SM 重构实测基线

---

**维护**: builder
