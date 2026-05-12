# 统一可视化平台实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现统一的 Web 可视化平台，支持仿真运行中的实时监控、仿真结束后的历史浏览、多次运行的目录导航、JSON 配置编辑和仿真参数配置。

**设计文档**: `docs/architecture/2026-05-12-unified-visualization-platform.md`

---

## 阶段 0：环境准备

- [ ] 确认 `cpptlm_config` 包结构（`cpptlm_config/builder.py`、`cpptlm_config/models.py`）
- [ ] 确认现有 `cpptlm/visualization/` 目录结构
- [ ] 确认 `examples/demo_e2e_soc.py` 完整流程（配置生成→仿真→结果解析）
- [ ] 运行现有测试确保 baseline：`cd build && ctest --output-on-failure`

---

## 阶段 1：RunContext 抽象层

### 任务 1.1：定义目录结构规范

- [ ] 确认 `runs/` 目录不存在，创建 `runs/.gitkeep`
- [ ] 在 `cpptlm/visualization/` 下创建 `run_context.py`

### 任务 1.2：实现 RunContext 类

**文件**: `cpptlm/visualization/run_context.py`

```python
class RunContext:
    """封装单个运行目录的只读视图."""

    run_id: str
    root: Path

    def is_active(self) -> bool: ...
    def config(self) -> Dict: ...
    def stats(self, offset: int = 0) -> Tuple[List[Dict], int]: ...
    def metrics(self) -> Optional[Dict]: ...
    def report(self) -> Optional[str]: ...
    def topology_png(self) -> Optional[str]: ...
    def meta(self) -> Dict: ...
    def reload(self) -> None: ...
```

**实现要点**：
- `is_active()`: 检查 `pid` 文件存在且进程存活，或 `stats.jsonl` mtime 在最近 5s 内有更新
- `stats(offset)`: `seek()` 到偏移量，只读新增行；返回 `(new_records, new_offset)`
- 内存中惰性缓存 `config`、`meta`、`metrics`
- `reload()` 清空缓存重新扫描

### 任务 1.3：实现 RunsIndex 类

**文件**: `cpptlm/visualization/run_context.py`（同一文件）

```python
class RunsIndex:
    """管理 runs/ 目录，扫描所有 RunContext."""

    def __init__(self, runs_dir: Path = Path("runs")): ...
    def list_runs(self) -> List[RunContext]: ...
    def get_run(self, run_id: str) -> Optional[RunContext]: ...
    def create_run(self, config_json: str, params: Dict) -> RunContext: ...
```

**实现要点**：
- `list_runs()`: 扫描 `runs/` 下所有目录，过滤无效目录（无 `config.json`），按时间倒序
- `create_run()`: 创建新运行目录，写入 `config.json`、`meta.json`（包含时间戳、参数、binary_path）

### 任务 1.4：单元测试

**文件**: `cpptlm/tests/test_run_context.py`（新增）

- [ ] 测试 `RunContext.is_active()`（有 pid 文件 / 无 pid 文件 / 进程已退出）
- [ ] 测试 `RunContext.stats()` 增量读取（追加写入后seek到新偏移量）
- [ ] 测试 `RunsIndex.list_runs()`（空目录 / 多个目录 / 损坏目录跳过）
- [ ] 测试 `RunsIndex.create_run()`（创建目录 + 写入 config.json + meta.json）
- [ ] 运行测试：`python -m pytest cpptlm/tests/test_run_context.py -v`

---

## 阶段 2：CLI 入口

### 任务 2.1：创建 CLI 脚手架

**文件**: `cpptlm/cli.py`（新增）

```python
# cpptlm/cli.py
import argparse

def main():
    parser = argparse.ArgumentParser(prog="cpptlm")
    subparsers = parser.add_subparsers()

    # cpptlm dashboard
    dash_parser = subparsers.add_parser("dashboard")
    dash_parser.add_argument("--port", type=int, default=8050)
    dash_parser.add_argument("--runs-dir", default="runs")
    dash_parser.add_argument("--open")
    dash_parser.set_defaults(func=dashboard_cmd)

    # cpptlm run
    run_parser = subparsers.add_parser("run")
    run_parser.add_argument("--config")
    run_parser.add_argument("--cycles", type=int, default=50000)
    run_parser.add_argument("--interval", type=int, default=1000)
    run_parser.add_argument("--output-dir", default="runs")
    run_parser.add_argument("--dashboard", action="store_true")
    run_parser.add_argument("--generate-only", action="store_true")
    run_parser.set_defaults(func=run_cmd)

    args = parser.parse_args()
    args.func(args)
```

### 任务 2.2：实现 dashboard 命令

**文件**: `cpptlm/cli.py`

- [ ] `dashboard_cmd(args)`: 启动 `DashboardServer(args.port, args.runs_dir)`，可选 `args.open` 直接打开浏览器
- [ ] 实现 `find_cpptlm_sim_binary()` 辅助函数（在 `build/bin/` 下查找）
- [ ] `run_cmd(args)`: 调用 `run_simulation()` 并在后台启动 Dashboard（若 `--dashboard`）

### 任务 2.3：集成到 Python 包入口

**文件**: `cpptlm/__main__.py`（修改）

- [ ] 添加 `from cpptlm.cli import main; main()` 到 `__main__.py`
- [ ] 验证：`python -m cpptlm --help`

---

## 阶段 3：DashboardServer 重构

### 任务 3.1：拆分 HTML 模板

**文件**: `cpptlm/visualization/dashboard_ui.py`（新增，从 `dashboard_server.py` 拆分）

- [ ] 将 `_DASHBOARD_HTML` 模板从 `dashboard_server.py` 移入 `dashboard_ui.py`
- [ ] 将 `_HOME_HTML`（runs/ 目录列表页面）添加到此文件
- [ ] 将 `_RUN_VIEW_HTML`（单一运行视图）添加到此文件

### 任务 3.2：实现 URL 路由

**文件**: `cpptlm/visualization/dashboard_server.py`（重构）

- [ ] 实现 `DashboardRequestHandler` 继承 `http.server.BaseHTTPRequestHandler`
- [ ] 实现 `do_GET()` 路由：
  - `/` → 渲染 runs/ 目录列表页面
  - `/?run=<run_id>` → 渲染单一运行视图
  - `/api/runs` → JSON：所有运行目录元信息
  - `/api/runs/<run_id>` → JSON：单个 RunContext 信息
  - `/api/runs/<run_id>/stats?offset=<int>` → JSON：`stats()` 增量数据
  - `/api/runs/<run_id>/config` → JSON：config.json 内容
  - `/runs/<run_id>/<filename>` → 静态文件服务
- [ ] 实现 `do_POST()` 路由：
  - `/api/runs/<run_id>/config` → 保存编辑后的 config.json
  - `/api/runs/<run_id>/rerun` → 重新运行仿真

### 任务 3.3：模式自动切换

**文件**: `cpptlm/visualization/dashboard_server.py`

- [ ] `RunContext.is_active()` 嵌入到 API 响应（如 `/api/runs/<run_id>` 返回 `{"is_active": true/false}`）
- [ ] 前端 JavaScript 根据 `is_active` 决定：轮询（2s间隔）vs 静态显示
- [ ] 仿真结束后，前端自动切换为静态浏览模式

### 任务 3.4：重新运行功能

**文件**: `cpptlm/visualization/dashboard_server.py`

- [ ] 实现 `rerun_simulation(run_id, params)` 方法
  - 读取 `config.json` + 表单参数（cycles, interval, seed）
  - 调用 `subprocess.run(["cpptlm_sim", config.json, "--stream-stats", ...])`
  - 写入 `pid` 文件
  - 更新 `meta.json`（rerun_count++, last_run 时间戳）
- [ ] 防止并发：`is_active()` 为 true 时拒绝重新运行

---

## 阶段 4：前端 UI（HTML + JavaScript）

### 任务 4.1：主页（runs/ 目录列表）

**文件**: `cpptlm/visualization/dashboard_ui.py`

- [ ] 实现 `_HOME_HTML` 模板：列出所有运行目录（卡片形式）
- [ ] 每个卡片显示：run_id、创建时间、cycles、is_active 状态
- [ ] 卡片操作：[View] [Re-run] [Delete]

### 任务 4.2：单一运行视图

**文件**: `cpptlm/visualization/dashboard_ui.py`

- [ ] 实现 `_RUN_VIEW_HTML` 模板：Tab 导航 [Topology] [Metrics] [Config] [Report]
- [ ] **Topology Tab**: `<img src="/runs/<run_id>/topology.png">`
- [ ] **Metrics Tab**: Plotly.js 图表（Latency Over Time、P95/P99 柱状图）
- [ ] **Config Tab**: Monaco Editor 加载 config.json，支持编辑
- [ ] **Report Tab**: `<iframe src="/runs/<run_id>/report.html">`

### 任务 4.3：实时轮询逻辑

**文件**: `cpptlm/visualization/dashboard_ui.py`（JavaScript 部分）

- [ ] `pollStats()`: 每 2s 请求 `/api/runs/<id>/stats?offset=N`
- [ ] 收到数据后调用 `Plotly.react()` 更新图表
- [ ] `is_active` 变为 false 时停止轮询，切换为静态模式

### 任务 4.4：表单参数 UI

**文件**: `cpptlm/visualization/dashboard_ui.py`

- [ ] Re-run 按钮展开的表单：cycles、interval、seed 输入框
- [ ] 点击 "Run" → `POST /api/runs/<id>/rerun` 发送表单数据
- [ ] 成功响应后跳转到实时视图

---

## 阶段 5：Monaco Editor 集成

### 任务 5.1：Monaco CDN + 降级

**文件**: `cpptlm/visualization/dashboard_ui.py`

- [ ] 在 Config Tab 中加载 Monaco CDN: `https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js`
- [ ] 初始化 Monaco Editor 实例，加载 config.json 内容
- [ ] CDN 加载失败时降级到 `<textarea>`，显示警告

### 任务 5.2：JSON Schema 校验（可选，降低优先级）

**文件**: `cpptlm_config/schema.json`（新增）

- [ ] 定义 `ConfigBuilder` 输出的 JSON Schema
- [ ] Monaco Editor 配置 `setDiagnosticsOptions({ validate: true, schemaUri: "..." })`
- [ ] Schema 来源：从现有 `ModuleSpec`、`ConnectionSpec` 模型推导

### 任务 5.3：保存功能

**文件**: `cpptlm/visualization/dashboard_ui.py`

- [ ] Monaco "Save" 按钮 → `POST /api/runs/<id>/config` 发送编辑后的 JSON
- [ ] 后端验证 JSON 格式，保存到 `config.json`
- [ ] 保存成功提示；格式错误显示 Monaco 内联错误

---

## 阶段 6：集成测试

### 任务 6.1：端到端手动测试

- [ ] 启动 Dashboard：`python -m cpptlm dashboard`
- [ ] 打开浏览器 `http://localhost:8050`，确认主页显示 "No runs yet"
- [ ] 运行仿真：`python -m cpptlm run --config configs/single_cluster.json --cycles 5000 --dashboard`
- [ ] 确认浏览器中实时看到 cycle 数据更新
- [ ] 仿真结束，确认静态报告和拓扑可见
- [ ] 点击 "Re-run"，修改 cycles=10000，确认新仿真覆盖旧数据

### 任务 6.2：回归测试

- [ ] 运行完整测试：`cd build && ctest --output-on-failure`
- [ ] 确保无新增失败测试
- [ ] `clang-format -i` 格式化所有修改的 Python 文件

---

## 阶段 7：文档与提交

### 任务 7.1：更新文档

- [ ] 更新 `docs/user-guide/python-usage.md`：新增 Dashboard 使用说明
- [ ] 更新 `docs/architecture/README.md`：添加本设计文档链接

### 任务 7.2：Git 提交

- [ ] 创建分支：`git checkout -b feature/unified-visualization-platform`
- [ ] `git add` 所有修改的文件
- [ ] 提交：`git commit -m "feat(visualization): unified dashboard with runs directory management"`
- [ ] 推送到远程：`git push -u origin HEAD`

---

## 实施顺序（建议）

```
阶段 1（RunContext）     → 阶段 2（CLI） → 阶段 3（DashboardServer 重构）
          ↓                                    ↓
      阶段 4（前端 UI）    ← ← ← ← ← ← ← ← ← ← ←
          ↓
阶段 5（Monaco Editor）
          ↓
阶段 6（集成测试）
          ↓
阶段 7（文档 + 提交）
```

**总工作量估计**: 2-3 天

---

## 关键文件清单

| 文件 | 操作 | 优先级 |
|------|------|--------|
| `cpptlm/visualization/run_context.py` | 新增 | P0 |
| `cpptlm/cli.py` | 新增 | P0 |
| `cpptlm/visualization/dashboard_server.py` | 重构 | P0 |
| `cpptlm/visualization/dashboard_ui.py` | 新增 | P0 |
| `cpptlm/__main__.py` | 修改 | P1 |
| `cpptlm/tests/test_run_context.py` | 新增 | P1 |
| `cpptlm_config/schema.json` | 新增 | P2（可选） |
| `docs/user-guide/python-usage.md` | 修改 | P2 |
| `runs/.gitkeep` | 新增 | P1 |

---

## 已知风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| Monaco CDN 访问受限 | 降级到 `<textarea>` 纯文本模式 |
| JSONL >100MB 加载慢 | 限制初始加载 10 万行，提供"加载更多"按钮 |
| 并发写入读到不完整行 | `JSONDecodeError` 时回退到上次成功偏移量 |
| 仿真器崩溃后 pid 文件残留 | `is_active()` 同时检查进程存活和 stats.jsonl mtime |
