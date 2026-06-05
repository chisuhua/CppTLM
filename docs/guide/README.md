# CppTLM 指南文档

> **版本**: 1.1
> **最后更新**: 2026-05-09

---

## 指南文档

| 文档 | 说明 | 适用对象 |
|------|------|---------|
| [GETTING_STARTED.md](./GETTING_STARTED.md) | 快速开始（构建、运行第一个仿真） | 新用户 |
| [TOPOLOGY_USER_GUIDE.md](./TOPOLOGY_USER_GUIDE.md) | 拓扑配置与仿真流程 | 用户/开发者 |
| [PYTHON_TOOLING_GUIDE.md](./PYTHON_TOOLING_GUIDE.md) | Python 验证工具链使用 | 用户/开发者 |
| [CREDIT_FLOW_USER_GUIDE.md](./CREDIT_FLOW_USER_GUIDE.md) | Credit-based Flow Control | 用户/开发者 |
| [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md) | 开发者指南（模块开发、调试） | 开发者 |

---

## 快速开始

### 1. 环境准备

```bash
# 必需
- CMake >= 3.16
- C++17 兼容编译器
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