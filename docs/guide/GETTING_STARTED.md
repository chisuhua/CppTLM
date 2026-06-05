# CppTLM 快速开始指南

> **版本**: 1.0
> **更新日期**: 2026-05-09
> **状态**: ✅ 完整

---

## 1. 概述

CppTLM 是 C++ TLM 2.0 周期精确片上网络仿真框架，支持 JSON 驱动拓扑、ModuleFactory 动态注入、ChStream 内部通信协议。

**核心特性**:
- JSON 驱动拓扑配置
- Python 验证工具链（validator.py, topology_adapter.py）
- 分层融合架构 v2.1（CacheTLM/CrossbarTLM/MemoryTLM）
- 87 个 Python 测试 + 528 个 C++ 测试

---

## 2. 环境准备

### 2.1 必需依赖

```bash
- CMake >= 3.16
- C++17 兼容编译器（GCC 9+, Clang 10+）
- Python 3.8+（用于验证工具链）
- pydantic >= 2.0（pip install pydantic）
```

### 2.2 可选依赖

```bash
- Ninja（加速构建）
- ccache（编译缓存，推荐）
- SystemC（混合仿真）
```

---

## 3. 构建项目

### 3.1 标准构建

```bash
# 1. 克隆项目
git clone https://github.com/chisuhua/CppTLM.git
cd CppTLM

# 2. 配置
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. 构建（使用所有 CPU 核心）
cmake --build . -j$(nproc)

# 4. 测试
ctest --output-on-failure
```

### 3.2 快速验证构建

```bash
# 仅构建核心库（跳过测试）
cmake --build . --target cpptlm_core -j$(nproc)

# 仅构建测试
cmake --build . --target cpptlm_tests -j$(nproc)
```

### 3.3 构建选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `-DCMAKE_BUILD_TYPE` | `Debug` / `Release` | `Release` |
| `-DBUILD_TESTS` | 构建测试 | `ON` |
| `-DBUILD_EXAMPLES` | 构建示例 | `ON` |

---

## 4. 运行第一个仿真

### 4.1 使用预定义配置

```bash
# 使用内置的 2x2 Mesh 配置
./build/bin/cpptlm_sim configs/mesh_2x2_tlm.json --cycles 10000
```

### 4.2 生成自定义拓扑

```bash
# 1. 生成拓扑配置
python3 cpptlm_config/examples/mesh_2x2.py > my_mesh.json

# 2. 验证配置
python3 scripts/topology_validator.py my_mesh.json -v

# 3. 运行仿真
./build/bin/cpptlm_sim my_mesh.json --cycles 10000
```

### 4.3 使用 Python API

```python
from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.validator import TopologyValidator

# 1. 构建配置
builder = ConfigBuilder()
builder.add_router("router_0_0", node_x=0, node_y=0, mesh_x=2, mesh_y=2)
builder.add_nic("ni0", node_id=0, mesh_x=2, mesh_y=2)
builder.add_connection("router_0_0.4", "ni0.1")

config = builder.build()

# 2. 验证配置
v = TopologyValidator(config)
result = v.validate()
if result.is_valid:
    print("配置有效！")
else:
    for e in result.errors:
        print(f"错误 [{e.code}]: {e.message}")

# 3. 导出 JSON
builder.export_json("my_config.json")
```

---

## 5. 运行测试

### 5.1 C++ 测试

```bash
# 全部测试
./build/bin/cpptlm_tests

# 按标签过滤
./build/bin/cpptlm_tests "[phase6]"      # Phase 6 集成测试
./build/bin/cpptlm_tests "[chstream]"    # ChStream 相关
./build/bin/cpptlm_tests "[crossbar]"    # Crossbar 相关
```

### 5.2 Python 测试

```bash
# 全部 Python 测试
python3 -m pytest test/python/ -v

# 特定测试
python3 -m pytest test/python/test_validator.py -v
```

### 5.3 完整验证

```bash
# 本地完整验证（构建 + 测试 + 格式检查）
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure
cd .. && python3 -m pytest test/python/ -v
```

---

## 6. 项目结构速览

```
CppTLM/
├── include/          # 头文件（10 子目录）
│   ├── core/         # SimObject/ModuleFactory/Port
│   ├── tlm/          # TLM 模块（CacheTLM, CrossbarTLM, MemoryTLM）
│   ├── framework/    # StreamAdapter 适配器
│   └── bundles/      # Bundle 定义
├── src/              # 源实现
│   ├── core/         # ModuleFactory, ConnectionResolver
│   └── tlm/          # RouterTLM, NICTLM, LinkTLM
├── test/             # C++ 测试（Catch2）
├── test/python/      # Python 测试（pytest）
├── configs/          # JSON 拓扑配置
├── cpptlm_config/    # Python 配置包（validator, builder）
├── scripts/          # 工具脚本（topology_validator.py 等）
└── docs/             # 文档
```

---

## 7. 下一步

| 文档 | 说明 |
|------|------|
| [DEVELOPER_GUIDE.md](./DEVELOPER_GUIDE.md) | 开发者指南（模块开发、调试技巧） |
| [TOPOLOGY_USER_GUIDE.md](./TOPOLOGY_USER_GUIDE.md) | 拓扑配置与仿真流程 |
| [PYTHON_TOOLING_GUIDE.md](./PYTHON_TOOLING_GUIDE.md) | Python 验证工具链使用 |
| [CREDIT_FLOW_USER_GUIDE.md](./CREDIT_FLOW_USER_GUIDE.md) | Credit-based Flow Control |

---

## 8. 常见问题

### Q: 构建失败，提示找不到 SystemC

**A**: CppTLM 默认使用内置 TLM stub（无需任何 SystemC 编译选项）。

### Q: Python 测试失败，提示找不到 pydantic

**A**: `pip install "pydantic>=2.0"`

### Q: 仿真器提示模块类型未注册

**A**: 检查 JSON 中 `type` 字段是否匹配已注册类型。运行 `./build/bin/cpptlm_sim --help` 查看支持的类型。

---

**维护**: CppTLM 开发团队
**版本**: 1.0 | **最后更新**: 2026-05-09