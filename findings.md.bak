# Findings: 配置生成与性能可视化流水线

**创建日期**: 2026-04-22
**最后更新**: 2026-04-22

---

## 代码库探索发现

### 1. 现有 Stats 框架

**StatsManager** (`include/metrics/stats_manager.hh`):
- 单例模式，`instance()` 静态方法访问
- `register_group(StatGroup*, path)` 注册统计组
- `dump_all(ostream&, width)` 输出所有统计
- `groups()` 返回 `std::map<std::string, StatGroup*>`
- 线程安全（mutex 保护）

**StatGroup** (`include/metrics/stats.hh`):
- 层次化结构，支持 `.` 路径查找
- `dump(ostream&, path, width)` 递归输出
- 支持 Scalar, Average, Distribution, Formula 类型

**现有报告格式**:
- TextReporter: gem5 风格对齐文本
- JSONReporter: 嵌套 JSON
- MarkdownReporter: Markdown 表格

### 2. 主程序入口

**src/main.cpp**:
```cpp
int main(int argc, char* argv[]) {
    EventQueue eq;
    REGISTER_ALL
    REGISTER_MODULE
    json config = JsonIncluder::loadAndInclude(argv[1]);
    ModuleFactory factory(&eq);
    factory.instantiateAll(config);
    factory.startAllTicks();
    eq.run(10000);
}
```

**关键点**:
- 命令行只接受一个参数 (config.json)
- 需要添加 --stream-stats 等参数
- 仿真周期硬编码为 10000

### 3. CMake 结构

**输出目录**:
- `build/bin/` - 可执行文件
- `build/lib/` - 静态库

**核心库**:
- `cpptlm_core` - 静态库，包含所有 .cc 源文件
- `cpptlm_sim` - 可执行文件，仅 USE_SYSTEMC=ON 时构建

**头文件路径**:
- `include/` - 公共头文件
- `include/core/` - 兼容旧式 include
- `external/json/` - nlohmann/json

### 4. 项目目录结构

```
CppTLM/
├── include/
│   ├── metrics/
│   │   ├── stats.hh
│   │   ├── stats_manager.hh
│   │   ├── metrics_reporter.hh
│   │   └── histogram.hh
│   ├── core/
│   ├── tlm/
│   ├── bundles/
│   └── ...
├── src/
│   ├── main.cpp
│   ├── core/
│   └── utils/
├── scripts/
│   ├── build.sh
│   ├── format.sh
│   └── test.sh
├── configs/
├── test/
├── docs/
└── plans/
```

---

## 外部依赖分析

### Python 依赖

| 库 | 版本 | 用途 |
|----|------|------|
| networkx | >=3.0 | 图数据结构和布局算法 |
| pydot | >=1.4.2 | DOT 文件生成/解析 |
| graphviz | 系统包 | DOT 渲染 |
| watchdog | >=3.0 | 文件系统监控 |
| dash | >=2.14.0 | Web 仪表板框架 |
| plotly | >=5.18.0 | 图表库 |

### C++ 依赖

| 库 | 来源 | 用途 |
|----|------|------|
| nlohmann/json | external/json/ | JSON 序列化 |
| C++17 | 编译器 | 标准库 |

---

## 技术决策

### 决策 1: StreamingReporter 设计

**选项 A**: 集成到 StatsManager（修改现有类）
- 优点: 紧密耦合，访问方便
- 缺点: 污染现有代码

**选项 B**: 独立类，持有 StatsManager 引用（选中）
- 优点: 无侵入，易于移除
- 缺点: 需要显式引用

**结论**: 选择 B - `StreamingReporter` 独立存在，通过 `StatsManager::instance().groups()` 访问数据

### 决策 2: JSON Lines vs 完整 JSON

**选项 A**: 每次输出完整 JSON
- 优点: 简单，可直接用 jq 处理
- 缺点: 文件大，解析慢

**选项 B**: JSON Lines（每行一个 JSON 对象）（选中）
- 优点: 流式友好，追加写入，解析简单
- 缺点: 需要行解析器

**结论**: 选择 B - 适合流式场景

### 决策 3: Dashboard 框架

**选项 A**: Flask + 手动 HTML
- 优点: 轻量
- 缺点: 图表需要额外库

**选项 B**: Dash (Plotly)（选中）
- 优点: 完整仪表板方案，图表内置
- 缺点: 较重

**结论**: 选择 B - Dash 提供完整解决方案

---

## 参考资料

- [NetworkX Layout Algorithms](https://networkx.org/documentation/stable/reference/drawing.html)
- [Dash Gallery](https://plotly.com/dash/)
- [JSON Lines](https://jsonlines.org/)
- [Graphviz Documentation](https://graphviz.org/documentation/)