# CppTLM 开发指南

> **版本**: 1.0  
> **最后更新**: 2026-04-10

---

## 指南文档

| 文档 | 说明 | 适用对象 |
|------|------|---------|
| [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md) | 开发者指南 | 所有开发者 |

---

## 快速开始

### 1. 环境准备

```bash
# 必需
- CMake >= 3.16
- C++17 兼容编译器
- ccache (推荐)
```

### 2. 构建项目

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j4
```

### 3. 运行测试

```bash
ctest --output-on-failure
```

---

## 核心内容

详见 [开发者指南](./DEVELOPER_GUIDE.md)

---

**维护**: CppTLM 开发团队
