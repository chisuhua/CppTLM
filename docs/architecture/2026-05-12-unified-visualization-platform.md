# CppTLM 统一可视化平台设计文档

**版本**: v1.0
**日期**: 2026-05-12
**状态**: 待评审
**负责人**: CppTLM Team

---

## 1. 背景与目标

CppTLM 是一个 TLM 2.0 周期精确片上网络仿真框架。用户通过 Python 库构建拓扑配置（JSON），调用 C++ 仿真器运行，产生 JSONL 流式统计结果。

**当前痛点**：

| 问题 | 说明 |
|------|------|
| Dashboard 仅支持实时查看 | 仿真结束后无法加载历史结果 |
| 运行产物分散 | 每次运行的 JSONL/报告/拓扑无统一入口 |
| 无配置编辑能力 | 无法在 Web UI 中编辑 JSON 配置或设置仿真参数 |
| 多次运行无法导航 | 无法在多个运行目录之间切换查看 |

**本设计的目标**：

构建一个统一的 Web 可视化平台，同时支持：
- **仿真运行中**：实时轮询推送数据，图表自动更新
- **仿真结束后**：纯浏览模式，查看静态报告/拓扑/指标
- **重新运行**：覆盖当前目录，Dashboard 自动切换为实时模式
- **配置编辑**：Monaco Editor 编辑 JSON 配置 + 表单设置仿真参数
- **目录导航**：主页列出所有运行目录，点击切换

---

## 2. 架构设计

### 2.1 核心抽象：RunContext

引入 `RunContext` 抽象层（`cpptlm/visualization/run_context.py`），封装单个运行目录的所有操作：

```
runs/
  run_2026-05-12_143052/
    config.json           # 原始配置
    topology.dot         # DOT 文件
    topology.png         # 渲染后的拓扑图
    stats.jsonl           # 仿真原始数据流
    report.html           # 静态 HTML 报告
    metrics.json          # 解析后的指标摘要
    meta.json             # 运行元信息（时间、参数、版本等）
    pid                   # 仿真进程 PID（运行时存在）
```

**RunContext 接口**：

```python
class RunContext:
    """封装单个运行目录的只读视图."""

    run_id: str                          # 目录名
    root: Path                            # runs/run_xxx/
    is_active() -> bool                  # 仿真是否仍在运行
    config() -> Dict                     # 读取 config.json
    stats(seek_offset: int) -> Tuple[List[Dict], int]  # 增量读取 stats.jsonl
    metrics() -> Optional[Dict]           # 读取 metrics.json（若存在）
    report() -> Optional[str]             # 读取 report.html 路径（若存在）
    topology_png() -> Optional[str]       # 读取 topology.png 路径（若存在）
    meta() -> Dict                        # 读取 meta.json
    reload() -> None                      # 清空内存缓存，重新扫描目录内容
```

**is_active() 实现**：检查 `pid` 文件是否存在且进程仍在运行，或检测 `stats.jsonl` 的 mtime 是否在最近 5 秒内有更新。

**stats() 增量读取**：`seek()` 到上次偏移量，只读取新增行，避免每次请求全量解析。内存中维护聚合状态（`by_group`、实时指标），避免重复计算。

### 2.2 单进程 HTTP 服务器

使用 Python stdlib `http.server`（单端口，默认 8050），零外部依赖。

**目录结构**：

```
cpptlm/
  visualization/
    run_context.py        # NEW: RunContext 抽象层
    dashboard_server.py   # MODIFY: 重构为基于 RunContext
    dashboard_ui.py       # NEW: HTML 模板（从 dashboard_server.py 拆分）
    static/
      dashboard.css       # NEW: 样式（若有需要）
      dashboard.js        # NEW: 前端逻辑
    report.py             # EXISTING: 复用
    topology.py           # EXISTING: 复用
```

### 2.3 URL 路由设计

| URL | 行为 |
|-----|------|
| `GET /` | 扫描 `runs/` 目录，渲染运行列表主页 |
| `GET /?run=<run_id>` | 进入特定运行视图（根据 `is_active()` 自动选择模式） |
| `GET /api/runs` | 返回所有运行目录的元信息列表（JSON） |
| `GET /api/runs/<run_id>` | 返回单个 RunContext 的完整信息 |
| `GET /api/runs/<run_id>/stats?offset=<int>` | 增量获取 stats.jsonl（返回新行 + 新偏移量） |
| `GET /api/runs/<run_id>/config` | 返回 config.json 内容 |
| `POST /api/runs/<run_id>/config` | 保存编辑后的 config.json |
| `POST /api/runs/<run_id>/rerun` | 重新运行仿真（读取 config.json + 表单参数） |
| `GET /runs/<run_id>/<filename>` | 静态文件服务（report.html、topology.png 等） |

### 2.4 实时与静态模式自动切换

视图组件（图表、拓扑、配置编辑器）只依赖 `RunContext` 接口，不感知实时/历史模式：

```
RunContext.is_active()
  ├─ true  → Dashboard 页面启动轮询（2s 间隔 /api/runs/<id>/stats）
  └─ false → Dashboard 页面显示静态聚合指标和历史图表
```

前端 JavaScript 根据初始请求的 `is_active` 字段决定轮询策略。

---

## 3. 功能模块设计

### 3.1 主页（运行目录列表）

**UI 布局**：

```
┌─────────────────────────────────────────────────────┐
│ CppTLM Unified Dashboard            [8050] [? 帮助] │
├─────────────────────────────────────────────────────┤
│  Runs                                               │
│  ┌───────────────────────────────────────────────┐ │
│  │ run_2026-05-12_143052   2026-05-12 14:30:52  │ │
│  │ Cycles: 50000   Status: ● Completed          │ │
│  │ [View] [Re-run] [Delete]                       │ │
│  └───────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────┐ │
│  │ run_2026-05-12_160045   2026-05-12 16:00:45  │ │
│  │ Cycles: 50000   Status: ● Running (cycle=4231)│ │
│  │ [View]                                        │ │
│  └───────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────┤
│ [+ New Run]  ─── Configuration ───                 │
│                                                  │
│  [Select JSON config file or create new...]       │
└─────────────────────────────────────────────────────┘
```

**"New Run" 按钮**：上传已有 JSON 配置文件，或使用 Monaco Editor 从空白配置开始编写。

### 3.2 单一运行视图

**UI 布局**：

```
┌─────────────────────────────────────────────────────┐
│ ← Back to Runs     run_2026-05-12_143052   [● Done]│
├─────────────────────────────────────────────────────┤
│ [Topology] [Metrics] [Config] [Report]   [Re-run ▾] │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │           Topology Visualization             │   │
│  │        (topology.png rendered)               │   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────┐ ┌──────────────┐ ┌────────────┐ │
│  │ Latency Mean │ │    P95       │ │   P99      │ │
│  │   12.34 ns   │ │   45.67 ns   │ │  78.90 ns  │ │
│  └──────────────┘ └──────────────┘ └────────────┘ │
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │         Latency Over Time (Plotly)          │   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
└─────────────────────────────────────────────────────┘
```

### 3.3 Monaco Editor（JSON 配置编辑器）

**集成方式**：通过 CDN 加载 Monaco Editor（`https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js`）。

**降级方案**：CDN 加载失败时，自动切换到 `<textarea>` 纯文本模式，并显示警告提示。

**JSON Schema 校验**：为 `ConfigBuilder` 输出定义 JSON Schema，提供自动补全和错误提示。Schema 文件位于 `cpptlm_config/schema.json`。

**编辑流程**：

1. 用户在 Monaco Editor 中修改 JSON 配置
2. 点击"Save" → `POST /api/runs/<id>/config`
3. 点击"Re-run" → 保存配置 → 执行仿真 → 覆盖当前目录

### 3.4 表单参数配置

**预定义字段**（非 JSON 配置的仿真参数）：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `cycles` | int | 50000 | 仿真周期数 |
| `interval` | int | 1000 | 统计输出间隔（cycles） |
| `seed` | int | 0 | 随机种子 |
| `binary_path` | str | "" | 仿真器路径（空=自动查找） |

**表单参数不写入 `config.json`**，而是作为命令行参数传递给仿真器：

```bash
cpptlm_sim config.json \
  --stream-stats \
  --stream-interval 1000 \
  --stream-path runs/run_xxx/stats.jsonl \
  --cycles 50000 \
  --seed 42
```

**验证**：使用 Pydantic Schema 同时驱动 CLI 的 argparse 和 UI 表单验证，保证一致性。

### 3.5 重新运行仿真

**流程**：

```
1. 用户点击 [Re-run ▾] → 展开参数表单
2. 用户修改 cycles/interval 等参数（或保持默认）
3. 点击 [Run]
4. 后端执行:
   a. 保存当前 config.json（如有修改）
   b. 写入 meta.json（更新 rerun_count、时间戳）
   c. 清空或追加 stats.jsonl（旧数据可选保留或备份）
   d. fork subprocess 执行 cpptlm_sim
   e. 写入 pid 文件
5. 前端检测 is_active=true，切换为实时轮询模式
6. 仿真结束，前端自动切换为静态浏览模式
```

**PID 文件**：写入 `runs/<run_id>/pid`，内容为进程 PID。`is_active()` 检查进程是否存在。

**仿真器路径**：优先使用 `meta.json` 中记录的 binary_path，若为空则自动查找 `build/bin/cpptlm_sim`。

---

## 4. 数据流

### 4.1 正常运行（仿真前）

```
用户选择 JSON 配置
    ↓
POST /api/runs/<new_run_id>/config
    ↓
RunContext 保存 config.json
    ↓
POST /api/runs/<new_run_id>/rerun
    ↓
subprocess.run(["cpptlm_sim", config.json, ...])
    ↓
写入 pid 文件
    ↓
Dashboard 切换为实时轮询
```

### 4.2 实时监控模式

```
C++ 仿真器 → 追加写入 stats.jsonl
    ↓
DashboardServer.stats() 增量读取（seek 偏移量）
    ↓
聚合状态更新（内存）
    ↓
GET /api/runs/<id>/stats?offset=N
    ↓
前端 Plotly 图表更新
```

### 4.3 历史浏览模式

```
GET /?run=<run_id>
    ↓
RunContext 检测 is_active() == false
    ↓
读取 metrics.json（如存在，否则惰性计算）
    ↓
读取 topology.png、report.html
    ↓
渲染静态页面（无轮询）
```

---

## 5. 错误处理与边界情况

| 场景 | 处理方式 |
|------|----------|
| JSONL 文件 >100MB | 首次加载限制为最近 10 万行；提供"加载更多"按钮 |
| 并发写入不完整行 | `JSONDecodeError` 时回退到上次成功偏移量 |
| 损坏的运行目录 | 主页显示警告图标，跳过该目录继续扫描 |
| Monaco CDN 不可用 | 自动降级到 `<textarea>` 纯文本模式 |
| 仿真器崩溃 | `pid` 文件存在但进程消失 → `is_active()` 返回 false |
| 权限不足 | 显示权限错误提示，不影响其他功能 |

---

## 6. 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `cpptlm/visualization/run_context.py` | 新增 | RunContext 抽象层 |
| `cpptlm/visualization/dashboard_server.py` | 重构 | 基于 RunContext 重写，保留实时轮询能力 |
| `cpptlm/visualization/dashboard_ui.py` | 新增 | HTML 模板（从 dashboard_server.py 拆分） |
| `cpptlm/visualization/dashboard.py` | 保留 | PerformanceDashboard（可能被 RunContext 替代，待评估） |
| `cpptlm/visualization/report.py` | 保留 | 复用 |
| `cpptlm/visualization/topology.py` | 保留 | 复用 |
| `cpptlm_config/schema.json` | 新增 | ConfigBuilder JSON Schema（用于 Monaco 校验） |
| `cpptlm/cli.py` | 新增 | CLI 入口：`cpptlm dashboard`、`cpptlm run --dashboard` |

---

## 7. 依赖清单

| 依赖 | 用途 | 来源 |
|------|------|------|
| Python 3.9+ stdlib | HTTP 服务器、JSON 处理、subprocess | 内置 |
| Plotly.js | 图表渲染 | CDN (`cdn.plot.ly`) |
| Monaco Editor | JSON 编辑器 | CDN (`cdn.jsdelivr.net`) |

**无新增 pip 依赖**。遵循 CppTLM 零外部依赖哲学。

---

## 8. 长期演进路径

| 阶段 | 扩展方向 | 说明 |
|------|----------|------|
| 当前 | 方案 1 实现 | 单进程 stdlib，满足单机使用 |
| 未来 | FastAPI 替换 | 如需多用户/远程访问，局部替换 http.server 为 FastAPI，不影响 RunContext 和视图组件 |
| 未来 | 运行标签/搜索 | 在 RunContext 之上引入纯内存索引（dict 或 JSON manifest），仍不需要数据库 |
| 未来 | 多仿真器支持 | RunContext 接口支持不同仿真器后端（当前仅 cpptlm_sim） |

---

## 9. 附录：CLI 命令设计

```bash
# 启动统一 Dashboard（扫描 runs/ 目录）
cpptlm dashboard
cpptlm dashboard --port 8050
cpptlm dashboard --runs-dir ./my_runs

# 运行仿真并自动打开 Dashboard
cpptlm run examples/demo_e2e_soc.py --dashboard
cpptlm run --config configs/my_soc.json --cycles 50000 --dashboard

# 仅生成运行目录（不启动仿真）
cpptlm run --config configs/my_soc.json --generate-only --output-dir runs/

# 启动已完成的运行（纯浏览模式）
cpptlm dashboard --open runs/run_2026-05-12_143052
```

---

## 10. 增强设计：Dashboard 内 Python 拓扑构建器

### 10.1 背景与目标

当前 Dashboard 的 JSON 编辑器适合已有 JSON 配置的用户，但以下场景需要增强：

| 场景 | 问题 |
|------|------|
| 用户不熟悉 JSON 结构 | 直接编辑 JSON 容易出错 |
| 需要动态调整模块数量 | 每次改都要手动编辑 JSON |
| 需要复用已有拓扑模板 | 无法从 Python 模板继承 |
| 需要参数化生成 | JSON 本身不支持变量/循环 |

**目标**：在 Dashboard 中提供 **Python 拓扑构建器**（后端 Python 函数执行 + 前端交互），作为 Monaco JSON 编辑器的**互补能力**。

### 10.2 设计方案

#### 方案 A：Python 代码执行器（推荐）

**架构**：

```
Dashboard (Browser)
    │
    ├── Monaco Editor (JSON 模式)
    │       └─ 直接编辑 config.json
    │
    └── Python Builder (表单 + 代码预览)
            │
            ├── 前端表单：模块类型、数量、连接方式
            ├─→ 生成 Python 代码（ConfigBuilder DSL）
            │
            └─→ POST /api/build  →  subprocess.run(code)  →  返回 JSON
                    │
                    └─→ 预览生成的 JSON → 确认后合并到编辑器
```

**前端 UI**：

```
┌─────────────────────────────────────────────────────┐
│ [JSON Editor] [Python Builder] [Form Mode]          │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─ Builder ──────────────────────────────────────┐ │
│  │                                                  │ │
│  │  Topology Name: [single_cluster_soc      ]     │ │
│  │                                                  │ │
│  │  Modules:                                       │ │
│  │  ┌──────────────────────────────────────────┐  │ │
│  │  │ [+ Add TrafficGen] [+ Add Cache] [+ Add Memory] │ │
│  │  │                                            │  │ │
│  │  │ cpu0: TrafficGen  pattern=[SEQUENTIAL ▼]  │  │ │
│  │  │ cpu1: TrafficGen  pattern=[RANDOM    ▼]  │  │ │
│  │  │ l1_0: Cache       size=[4096     ]       │  │ │
│  │  │ xbar: Crossbar    ports=[8        ]       │  │ │
│  │  │ mem0: Memory      latency=[100   ]       │  │ │
│  │  └──────────────────────────────────────────┘  │ │
│  │                                                  │ │
│  │  Connections:                                   │ │
│  │  [cpu0 → l1_0] [cpu1 → l1_1] [l1_0 → xbar]     │ │
│  │                                                  │ │
│  │  [Preview Python Code]  [Apply to Editor]       │ │
│  └─────────────────────────────────────────────────┘ │
│                                                     │
│  ┌─ Generated Code ───────────────────────────────┐ │
│  │ from cpptlm_config.builder import ConfigBuilder │ │
│  │                                                  │ │
│  │ b = ConfigBuilder("single_cluster_soc", ...)    │ │
│  │ b.add_module(...)                               │ │
│  │ ...                                             │ │
│  └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

**后端 API**：

| API | 方法 | 说明 |
|-----|------|------|
| `/api/build/preview` | POST | 接收表单数据，返回预览的 Python 代码（不执行） |
| `/api/build/generate` | POST | 执行生成的 Python 代码，返回 JSON 配置 |
| `/api/build/templates` | GET | 返回可用拓扑模板列表（对应 `examples/` 下的脚本） |

**安全考虑**：

- subprocess 执行隔离在单独目录
- 限制执行时间和内存
- 禁止访问文件系统（仅允许 cpptlm_config builder API）
- 执行结果不包含任何系统命令输出

#### 方案 B：纯前端表单生成

**架构**：前端根据 JSON Schema 动态生成表单，用户填表单 → 直接生成 JSON（不执行 Python）。

**优点**：无后端执行风险，前端即可完成。

**缺点**：无法复用 Python `ConfigBuilder` 的参数验证和模块连接逻辑（如 `b.add_connection()` 自动推导反向连接）。

### 10.3 推荐方案

**采用方案 A（Python 代码执行器）**，理由：

| 理由 | 说明 |
|------|------|
| 代码复用 | 直接复用 `cpptlm_config.builder.ConfigBuilder` API，无需重复实现逻辑 |
| 表达能力 | Python 可以实现变量、循环、继承等高级拓扑生成能力 |
| 模板复用 | `examples/` 下的 `.py` 脚本可以直接作为模板加载 |
| 用户友好 | 表单驱动降低入门门槛，高级用户可直接编辑 Python 代码 |

### 10.4 模板系统

**目录结构**：

```
examples/
  templates/
    single_cluster_soc.py     # 单集群模板
    dual_cluster_soc.py       # 双集群模板
    hierarchical_noc.py        # 分层 NoC 模板
```

**模板接口**：

```python
# examples/templates/single_cluster_soc.py

def build(num_cpus: int = 4,
          cache_size: int = 4096,
          mem_latency: int = 100) -> str:
    """返回 JSON 配置字符串."""
    from cpptlm_config.builder import ConfigBuilder

    b = ConfigBuilder("single_cluster_soc", "Single cluster SoC")
    # ... 构建逻辑 ...
    return b.to_json()

# 或返回 ConfigBuilder 实例本身
def build_builder(num_cpus: int = 4, ...) -> ConfigBuilder:
    ...
```

**Dashboard 中加载模板**：

```
1. GET /api/build/templates → 返回模板列表
2. 用户选择模板（如 "dual_cluster_soc.py"）
3. 前端加载模板代码 + 展示参数表单
4. 用户调整参数（num_cpus=8, cache_size=8192）
5. POST /api/build/generate → 执行 build(num_cpus=8, cache_size=8192)
6. 返回 JSON 配置
```

### 10.5 文件变更（补充）

| 文件 | 操作 | 说明 |
|------|------|------|
| `cpptlm/visualization/build_api.py` | 新增 | Python Builder 后端 API |
| `cpptlm_config/templates/` | 新增 | 模板脚本目录 |
| `cpptlm_config/schema.json` | 新增 | ConfigBuilder JSON Schema（用于 Monaco 校验 + 表单生成） |

### 10.6 与 JSON Editor 的关系

```
用户操作流程（两种模式可切换）：

[空白配置]
    │
    ├── "Python Builder" 标签页
    │       └─ 选择模板 → 填参数 → 生成 JSON → 合并到编辑器
    │
    └── "JSON Editor" 标签页
            └─ 直接编辑 / 微调配置
```

两种模式**互补而非互斥**，用户可根据熟悉程度选择：
- 新手：先用 Builder 生成基础拓扑，再用 Editor 微调
- 高级用户：直接 Editor 编辑完整 JSON
- 模板用户：加载模板 → Editor 微调
