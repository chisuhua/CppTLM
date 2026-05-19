# CppTLM Dashboard 集成架构

> **CppTLM Unified Visualization Platform — 架构文档**
> 版本：v2.0 | 日期：2026-05-13 | 状态：已批准
>
> Oracle 评审后更新：增加架构原则、技术债务规则、FastAPI 迁移路径

---

## 一、架构原则

### 1.1 核心原则

| 原则 | 说明 | 阈值 |
|------|------|------|
| **无 TODO 残留** | 代码中的 TODO 即技术债务，必须在当前 PR 解决 | 零容忍 |
| **无重复逻辑** | 相同逻辑出现两次以上必须提取为共享组件 | 发现即提取 |
| **HTML 模块化** | 内联 HTML > 500 行必须提取为静态文件 | 500 行上限 |
| **路由复杂度控制** | HTTP 路由超过 ~10 个端点时迁移到 FastAPI | 10 端点 |
| **前端状态管理匹配** | 简单状态（<100 变量）用 Alpine.js，复杂交互用 React/Svelte | 100 变量 |

### 1.2 技术债务规则

**禁止项**：
- `TODO:` 注释 —— 表示工作 defer 到未来，当前即债务
- 跨文件重复逻辑 —— cli.py 和 dashboard_server.py 重复的命令构建
- 魔法数字 —— 如 `timeout=30` 无解释
- 内联 HTML > 500 行 —— 难以维护和测试

**债务处理**：
- 发现即解决，不进入后续阶段
- 每个 PR 必须消除至少一个债务项
- 债务累积必须体现在工作量估算中

### 1.3 组件边界

```
┌─────────────────────────────────────────────────────────────┐
│                        展示层                                │
│  静态 HTML/JS (cpptlm/visualization/static/)               │
│  - home.html, run_view.html, new_run.html, editor.html      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                        API 层 (FastAPI)                      │
│  app.py (Phase 2+)                                          │
│  - HTTP 路由                                                 │
│  - 请求验证 (Pydantic)                                       │
│  - API 端点                                                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      业务逻辑层                              │
│  SimulationRunner (shared)                                  │
│  - 命令构建                                                  │
│  - 子进程启动                                                │
│  - 拓扑生成                                                  │
│  - 报告生成                                                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      数据访问层                              │
│  RunContext / RunsIndex                                      │
│  - 运行目录管理                                              │
│  - meta.json 读写                                            │
│  - stats.jsonl 增量读取                                      │
└─────────────────────────────────────────────────────────────┘
```

**边界规则**：
- `SimulationRunner` 被 cli.py 和 dashboard_server.py 共享
- `RunContext`/`RunsIndex` 是纯数据访问，无业务逻辑
- 静态文件只做展示，不含业务逻辑
- API 层（FastAPI）只做路由和验证，不直接操作文件

---

## 二、架构全景

### 2.1 当前架构（Phase 0 完成后）

```
┌──────────────────────────────────────────────────────────────────┐
│                        Browser (Client)                           │
│  ┌────────────────┐  ┌──────────────┐  ┌─────────────────────┐  │
│  │  htmx 14KB     │  │ Alpine.js 4KB│  │  Cytoscape.js 200KB │  │
│  │  (AJAX/表单)   │  │ (状态管理)   │  │  (拓扑图，阶段3)    │  │
│  └────────┬───────┘  └──────┬───────┘  └──────────┬──────────┘  │
│           └─────────────────┼─────────────────────┘             │
│                             │ HTTP + JSON                       │
└─────────────────────────────┼───────────────────────────────────┘
                               │
┌─────────────────────────────┼───────────────────────────────────┐
│                    Python HTTP Server (8050)                     │
│  ┌──────────────────────┐  ┌──────────────────────────────────┐ │
│  │ dashboard_server.py  │  │ 新增 API 端点                     │ │
│  │ (已有)               │  │ - POST /api/runs (创建+启动)     │ │
│  │ - GET /              │  │ - GET  /api/templates            │ │
│  │ - GET /api/runs/*    │  │ - POST /api/validate             │ │
│  │ - POST /api/runs/*   │  │ - GET  /api/topology/dot         │ │
│  │ - GET /runs/*        │  │ - POST /api/topology/render      │ │
│  └──────────────────────┘  └──────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 共享组件                                                   │  │
│  │ - SimulationRunner (command, topology, report)            │  │
│  │ - RunContext / RunsIndex (data access)                    │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────────┬───────────────────────────────────┘
                               │ subprocess
┌─────────────────────────────┼───────────────────────────────────┐
│                    C++ Binary (cpptlm_sim)                       │
│  - 读取 JSON 配置                                                 │
│  - 流式输出 stats.jsonl                                          │
│  - 生成 topology.dot                                              │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 Phase 2 架构（FastAPI 迁移后）

```
┌──────────────────────────────────────────────────────────────────┐
│                        Browser (Client)                           │
│  ┌────────────────┐  ┌──────────────┐  ┌─────────────────────┐  │
│  │  htmx 14KB     │  │ Alpine.js 4KB│  │  Svelte/React       │  │
│  │  (AJAX/表单)   │  │ (简单状态)   │  │  (拓扑编辑器)       │  │
│  └────────┬───────┘  └──────┬───────┘  └──────────┬──────────┘  │
│           └─────────────────┼─────────────────────┘             │
│                             │ HTTP + JSON                       │
└─────────────────────────────┼───────────────────────────────────┘
                               │
┌─────────────────────────────┼───────────────────────────────────┐
│                    FastAPI (8050)                                 │
│  ┌──────────────────────┐  ┌──────────────────────────────────┐ │
│  │ app.py (Router)     │  │ 静态文件                          │ │
│  │ - GET /, /new, /runs │  │ /static/* (HTML/JS/CSS)          │ │
│  │ - POST /api/runs     │  │                                   │ │
│  │ - GET /api/runs/*    │  │                                   │ │
│  └──────────────────────┘  └──────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 共享组件（不变）                                            │  │
│  │ - SimulationRunner                                          │  │
│  │ - RunContext / RunsIndex                                    │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────────┬───────────────────────────────────┘
                               │ subprocess
┌─────────────────────────────┼───────────────────────────────────┐
│                    C++ Binary (cpptlm_sim)                       │
└──────────────────────────────────────────────────────────────────┘
```

---

## 三、技术栈决策

### 3.1 前端框架决策树

```
需要前端框架？
    │
    ├─ 状态变量 < 100？
    │   └─ 是 → Alpine.js（ доста ）
    │
    ├─ 需要拖拽拓扑编辑器？
    │   └─ 是 → Svelte 或 React
    │
    └─ 需要复杂组件交互？
        └─ 是 → React 或 Svelte
```

**判断**：
- 阶段 1-2：Alpine.js 足够（简单表单、状态展示）
- 阶段 3：拓扑编辑器需要 React/Svelte（拖拽状态复杂）

### 3.2 后端框架决策

| 端点数量 | 推荐框架 | 原因 |
|---------|---------|------|
| < 10 | Python stdlib HTTP Server | 无外部依赖 |
| 10-50 | FastAPI | 自动 OpenAPI、内置验证、异步支持 |
| > 50 | 考虑 Django/FastAPI + 路由模块化 | 团队协作 |

**当前**：Phase 1 结束时约 8-10 个端点，Phase 2 会增加到 15+，建议在 Phase 2 开始时迁移 FastAPI。

### 3.3 静态文件 vs 内联模板

| 场景 | 推荐 |
|------|------|
| HTML < 100 行 | 内联或简单模板加载器 |
| HTML 100-500 行 | 静态文件，模板引擎加载 |
| HTML > 500 行 | 必须静态文件，考虑拆分 |

---

## 四、组件规格

### 4.1 SimulationRunner

**文件**：`cpptlm/visualization/simulation_runner.py`

**职责**：
- 命令构建（所有参数的组合方式）
- 子进程启动和生命周期管理
- 拓扑 DOT 文件生成
- 统计报告生成

**接口**：
```python
class SimulationRunner:
    def build_command(
        self,
        config_path: Optional[Path] = None,
        cycles: int = 50000,
        seed: int = 0,
        interval: int = 1000,
        stream_path: Optional[Path] = None,
        extra_args: Optional[List[str]] = None,
    ) -> List[str]

    def launch(...) -> subprocess.Popen

    def generate_topology_dot(self, config_path: Path, output_path: Path) -> bool

    def generate_report(self, stats_path: Path, output_path: Path) -> bool

    @staticmethod
    def from_meta(run_root: Path, meta: Dict[str, Any]) -> "SimulationRunner"
```

**约束**：
- 不直接读写 meta.json
- 不直接访问 RunContext/RunsIndex
- 纯业务逻辑，聚焦子进程和命令

### 4.2 RunContext / RunsIndex

**文件**：`cpptlm/visualization/dashboard_server.py`

**职责**：
- 运行目录结构管理
- meta.json 读写
- stats.jsonl 增量读取
- 运行状态（active/done）查询

**接口**：
```python
class RunContext:
    root: Path
    def meta() -> Dict[str, Any]
    def is_active() -> bool
    def stats(offset: int) -> Tuple[List[Dict], int]

class RunsIndex:
    def get_run(run_id: str) -> Optional[RunContext]
    def list_runs() -> List[RunContext]
    def create_run(...) -> RunContext
```

**约束**：
- 不构建命令
- 不启动进程
- 不做 HTTP 路由

### 4.3 静态文件

**目录**：`cpptlm/visualization/static/`

**文件**：
- `home.html` - 首页
- `run_view.html` - 运行详情页
- `new_run.html` - 新建运行向导
- `editor.html` - 拓扑编辑器（阶段 3+）

**约束**：
- 无 Python 代码
- 模板变量使用 `$VAR$` 格式
- 通过 fetch() 调用 API

---

## 五、FastAPI 迁移路径

### 5.1 迁移时机

**决策点**：Phase 2 开始时

**判断依据**：
- 端点数量接近 10 个阈值
- 路由 if/elif 链开始增长
- 需要请求验证和 OpenAPI 文档

### 5.2 迁移范围

| 组件 | 迁移 | 保持 |
|------|------|------|
| HTTP 路由 | → FastAPI Router | - |
| API 端点 | → @app.get/post | - |
| SimulationRunner | - | 不变 |
| RunContext/RunsIndex | - | 不变 |
| 静态 HTML | - | 移动到 /static/ |
| 业务逻辑 | - | SimulationRunner |

### 5.3 迁移步骤

1. **创建 app.py**：FastAPI 骨架，复制现有端点
2. **迁移 API 端点**：逐一迁移 /api/* 端点
3. **迁移页面端点**：/new, /runs/* 等
4. **挂载静态文件**：`app.mount("/static", StaticFiles(...))`
5. **验证**：所有现有功能通过 FastAPI 可用
6. **清理**：移除旧的 dashboard_server.py 中的已迁移端点

### 5.4 风险缓解

- **渐进迁移**：保留旧端点别名，切换期间两个都能用
- **测试覆盖**：每个端点迁移后运行集成测试
- **回滚计划**：如果 FastAPI 有问题，可以切回 stdlib HTTP Server

---

## 六、阶段规划

### 6.1 阶段 0：架构基础（2-3 天）

| 任务 | 输出 | 验收标准 |
|------|------|---------|
| 0.1 提取 SimulationRunner | simulation_runner.py | cli.py 和 dashboard_server.py 都用它 |
| 0.2 移动内联 HTML | static/*.html | dashboard_ui.py < 10KB |
| 0.3 规划 FastAPI | fastapi-migration-plan.md | 文档经过 Oracle 评审 |

### 6.2 阶段 1：Bug 修复（2-3 小时）

| 任务 | 验收标准 |
|------|---------|
| 1.1 修复 rerun | Re-run 收到完整参数 |
| 1.2 自动拓扑生成 | topology.dot 自动生成 |
| 1.3 自动报告生成 | report.json 自动生成 |
| 1.4 seed 参数 | seed 正确传递 |

### 6.3 阶段 2：FastAPI + 向导（3-4 天）

| 任务 | 验收标准 |
|------|---------|
| 2.1 FastAPI 骨架 | /docs 显示 API |
| 2.2 配置向导 | /new 显示表单 |

### 6.4 阶段 3：拓扑编辑器（5-7 天）

| 任务 | 验收标准 |
|------|---------|
| 3.1 前端选型 | Oracle 评审确定 |
| 3.2 拓扑编辑器 | 拖拽可用 |

### 6.5 阶段 4+：增强功能

- SSE 实时推送
- 多图表
- 多运行对比

---

## 七、技术债务登记

### 7.1 已识别债务

| ID | 债务项 | 来源 | 影响 | 解决阶段 |
|----|--------|------|------|---------|
| TD-001 | cli.py 和 dashboard_server.py 命令构建重复 | Phase 0 前 | 维护成本高 | Phase 0 |
| TD-002 | dashboard_ui.py > 20KB 内联 HTML | Phase 0 前 | 难以维护 | Phase 0 |
| TD-003 | HTTP 路由 if/elif 链 | Phase 2 前 | 扩展性差 | Phase 2 |
| TD-004 | Alpine.js 用于拓扑编辑器 | Phase 3 前 | 状态管理不足 | Phase 3 |

### 7.2 债务规则

1. **新增债务 = 拒绝合并**
2. **每个 PR 必须消除至少一个债务项**
3. **债务累积必须体现在工作量估算中**

---

## 八、参考资源

| 资源 | 链接 |
|---|---|
| FastAPI 文档 | https://fastapi.tiangolo.com/ |
| Pydantic 模型 | https://docs.pydantic.dev/ |
| Svelte 文档 | https://svelte.dev/docs |
| React 文档 | https://react.dev/ |
| htmx 文档 | https://htmx.org/docs/ |
| Alpine.js 文档 | https://alpinejs.dev/start-here |

---

*文档结束*