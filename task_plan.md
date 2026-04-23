# Task Plan: 配置生成与性能可视化流水线

**项目**: CppTLM 配置生成 + 流式统计 + Python 可视化
**版本**: 1.0
**创建日期**: 2026-04-22
**最后更新**: 2026-04-22

---

## 目标

建立完整的**配置生成 → 仿真 → 性能可视化**流水线。

---

## Phase 状态总览

| Phase | 任务 | 状态 | 产出 |
|-------|------|:----:|------|
| A | Python 拓扑生成器 | 🔴 pending | `scripts/topology_generator.py` |
| B | C++ 流式统计导出 | 🔴 pending | `include/metrics/streaming_reporter.hh` |
| C | Python 实时监控 + Dashboard | 🔴 pending | `scripts/stats_watcher.py` |
| D | Python 性能标注工具 | 🔴 pending | `scripts/stats_annotator.py` |
| E | 集成测试 + 文档更新 | 🔴 pending | 完整流水线验证 |

---

## Phase A: Python 拓扑生成器

### 任务清单

- [ ] A.1 创建 `scripts/topology_generator.py`
  - [ ] A.1.1 TopologyGenerator 类框架
  - [ ] A.1.2 generate_mesh() 方法
  - [ ] A.1.3 generate_ring() 方法
  - [ ] A.1.4 generate_bus() 方法
  - [ ] A.1.5 generate_hierarchical() 方法
  - [ ] A.1.6 export_json_config() 方法
  - [ ] A.1.7 export_layout() 方法
  - [ ] A.1.8 export_dot() 方法
  - [ ] A.1.9 apply_networkx_layout() 方法
  - [ ] A.1.10 命令行参数解析

- [ ] A.2 创建 `scripts/layout_manager.py`
  - [ ] A.2.1 LayoutManager 类
  - [ ] A.2.2 布局导入/导出 JSON
  - [ ] A.2.3 DOT 位置注入

- [ ] A.3 创建 `scripts/layout_to_dot.py`
  - [ ] A.3.1 布局转 DOT 转换器

- [ ] A.4 验收测试
  - [ ] A.4.1 mesh 4x4 生成验证
  - [ ] A.4.2 ring 8 生成验证
  - [ ] A.4.3 layout JSON 导入导出验证

### 产出文件

```
scripts/
├── topology_generator.py   # 拓扑生成器
├── layout_manager.py       # 布局管理器
└── layout_to_dot.py       # 布局转 DOT
```

---

## Phase B: C++ 流式统计导出

### 任务清单

- [ ] B.1 创建 `include/metrics/streaming_reporter.hh`
  - [ ] B.1.1 StreamingReporter 类定义
  - [ ] B.1.2 后台输出线程
  - [ ] B.1.3 JSON Lines 序列化
  - [ ] B.1.4 非阻塞入队机制

- [ ] B.2 修改 `src/main.cpp`
  - [ ] B.2.1 添加命令行参数解析
  - [ ] B.2.2 集成 StreamingReporter

- [ ] B.3 验收测试
  - [ ] B.3.1 --stream-stats 参数工作
  - [ ] B.3.2 JSON Lines 文件正确生成
  - [ ] B.3.3 仿真性能开销 <5%

### 产出文件

```
include/metrics/
└── streaming_reporter.hh   # 流式导出器

src/
└── main.cpp                # 修改: 添加流式统计参数
```

---

## Phase C: Python 实时监控 + Dashboard

### 任务清单

- [ ] C.1 创建 `scripts/stats_watcher.py`
  - [ ] C.1.1 StatsStreamHandler (watchdog)
  - [ ] C.1.2 StatsAggregator 聚合器
  - [ ] C.1.3 命令行参数

- [ ] C.2 创建 `scripts/dashboard_server.py`
  - [ ] C.2.1 DashboardServer 类
  - [ ] C.2.2 指标卡片组件
  - [ ] C.2.3 Plotly 图表

- [ ] C.3 验收测试
  - [ ] C.3.1 Dashboard 启动验证
  - [ ] C.3.2 实时更新验证
  - [ ] C.3.3 --no-gui 控制台模式

### 产出文件

```
scripts/
├── stats_watcher.py        # 统计监控器
└── dashboard_server.py     # Web 仪表板 (可选模块)
```

---

## Phase D: Python 性能标注工具

### 任务清单

- [ ] D.1 创建 `scripts/stats_annotator.py`
  - [ ] D.1.1 StatsAnnotator 类
  - [ ] D.1.2 HTML 报告生成
  - [ ] D.1.3 DOT 性能标注

- [ ] D.2 创建 `scripts/run_full_pipeline.sh`
  - [ ] D.2.1 完整流水线脚本

- [ ] D.3 验收测试
  - [ ] D.3.1 HTML 报告生成
  - [ ] D.3.2 DOT 标注验证

### 产出文件

```
scripts/
├── stats_annotator.py      # 性能标注工具
└── run_full_pipeline.sh    # 集成脚本
```

---

## Phase E: 集成测试 + 文档

### 任务清单

- [ ] E.1 创建 `requirements-visualization.txt`
- [ ] E.2 更新 `docs/README.md`
- [ ] E.3 完整流水线端到端测试

### 产出文件

```
requirements-visualization.txt   # Python 依赖
docs/                            # 文档更新
```

---

## 错误记录

| 错误 | Phase | 尝试 | 解决 |
|------|-------|:----:|------|
| (无) | - | - | - |

---

## 决策记录

| 决策 | 日期 | 理由 |
|------|------|------|
| Phase A 先于 B 实现 | 2026-04-22 | Python 独立于 C++，可并行开发 |
| StreamingReporter header-only | 2026-04-22 | 无需编译，易于集成 |
| JSON Lines 格式 | 2026-04-22 | 流式解析优于完整 JSON |

---

## 下一步行动

**Phase A.1**: 创建 `scripts/topology_generator.py`