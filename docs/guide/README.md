# CppTLM 指南文档

> **版本**: 1.2
> **最后更新**: 2026-11-11 (新增 `cep-usage.md` 行 —— CEP 使用手册,per 台账 Entry #333)

---

## 指南文档

| 文档 | 说明 | 适用对象 |
|------|------|---------|
| [GETTING_STARTED.md](./GETTING_STARTED.md) | 快速开始（构建、运行第一个仿真） | 新用户 |
| [TOPOLOGY_USER_GUIDE.md](./TOPOLOGY_USER_GUIDE.md) | 拓扑配置与仿真流程 | 用户/开发者 |
| [PYTHON_TOOLING_GUIDE.md](./PYTHON_TOOLING_GUIDE.md) | Python 验证工具链使用 | 用户/开发者 |
| [CREDIT_FLOW_USER_GUIDE.md](./CREDIT_FLOW_USER_GUIDE.md) | Credit-based Flow Control | 用户/开发者 |
| [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md) | 开发者指南（模块开发、调试） | 开发者 |
| [UE_DRIVER_MIGRATION_GUIDE.md](./UE_DRIVER_MIGRATION_GUIDE.md) | UE driver 移植指南（framebuffer_size 单一真源约定） | UE driver 作者 |
| [cep-usage.md](./cep-usage.md) | **CEP 使用手册**（Stage 4+ 维护态交接：10 分钟上手 / 载体地图 / 新增协议族流程 / 实验 4 件套 / 闸清单 / 冻结面五步授权链 / DoD 读法 / 未结义务） | CEP 维护者 |

---

## 快速开始

### 1. 环境准备

```bash
# 必需
- CMake >= 3.16
- C++23 兼容编译器（GCC 13+, Clang 16+）
- Python 3.8+（用于验证工具链）
- pydantic >= 2.0
- ccache（推荐）
```

### 2. 构建项目

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### 3. 运行测试

```bash
# C++ 测试
ctest --output-on-failure

# Python 测试
python3 -m pytest test/python/ -v
```

---

## 核心内容

- **新用户**: 从 [GETTING_STARTED.md](./GETTING_STARTED.md) 开始
- **配置拓扑**: 参考 [TOPOLOGY_USER_GUIDE.md](./TOPOLOGY_USER_GUIDE.md)
- **Python 工具**: 参考 [PYTHON_TOOLING_GUIDE.md](./PYTHON_TOOLING_GUIDE.md)
- **开发模块**: 参考 [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md)

---

**维护**: CppTLM 开发团队