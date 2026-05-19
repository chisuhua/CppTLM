# CppTLM Dashboard 实施计划

> **CppTLM Unified Visualization Platform — 渐进式增强实施指南**
> 版本：v2.0 | 日期：2026-05-13
>
> 本文档配合 `docs/architecture/13-dashboard-integration.md` 使用，提供每个任务的**具体代码、操作步骤、验证方法**。
>
> **架构原则**：禁止在代码中留下 TODO，禁止重复逻辑扩散，禁止 >500 行内联 HTML。

---

## 阶段 0：架构基础（预计 2-3 天）

**目的**：在开始功能开发前，消除会成倍放大未来工作的结构性问题。

### 任务 0.1：提取 SimulationRunner 类

**影响**：消除 cli.py 和 dashboard_server.py 之间重复的命令构建逻辑

**文件**：新建 `cpptlm/visualization/simulation_runner.py`

**步骤**：

1. 创建 `SimulationRunner` 类，统一所有子进程启动逻辑：

```python
import subprocess
import json
import os
from pathlib import Path
from typing import Optional, Dict, Any, List

class SimulationRunner:
    """统一的仿真进程管理类.

    职责：
    - 命令构建（所有参数的组合方式）
    - 拓扑文件生成（graphviz dot 输出）
    - 报告生成（stats 分析）
    - 进程生命周期管理
    """

    def __init__(self, binary_path: Path, run_root: Path):
        self.binary = binary_path
        self.root = run_root

    def build_command(
        self,
        config_path: Optional[Path] = None,
        cycles: int = 50000,
        seed: int = 0,
        interval: int = 1000,
        stream_path: Optional[Path] = None,
        extra_args: Optional[List[str]] = None,
    ) -> List[str]:
        """构建完整的仿真命令."""
        cmd = [str(self.binary)]

        if config_path:
            cmd.append(str(config_path))

        if stream_path:
            cmd.extend([
                "--stream-stats",
                "--stream-interval", str(interval),
                "--stream-path", str(stream_path),
            ])

        cmd.extend(["--cycles", str(cycles)])

        if seed != 0:
            cmd.extend(["--seed", str(seed)])

        if extra_args:
            cmd.extend(extra_args)

        return cmd

    def launch(
        self,
        config_path: Optional[Path] = None,
        cycles: int = 50000,
        seed: int = 0,
        interval: int = 1000,
        stream_path: Optional[Path] = None,
        extra_args: Optional[List[str]] = None,
    ) -> subprocess.Popen:
        """启动仿真进程，返回 Popen 对象."""
        cmd = self.build_command(config_path, cycles, seed, interval, stream_path, extra_args)

        pid_file = self.root / "pid"
        proc = subprocess.Popen(cmd, cwd=str(self.root))
        pid_file.write_text(str(proc.pid), encoding="utf-8")

        return proc

    def generate_topology_dot(self, config_path: Path, output_path: Path) -> bool:
        """生成拓扑 DOT 文件."""
        cmd = [str(self.binary), str(config_path), "--emit-dot", str(output_path)]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return result.returncode == 0

    def generate_report(self, stats_path: Path, output_path: Path) -> bool:
        """生成统计报告."""
        cmd = [str(self.binary), "--report", str(stats_path), "--output", str(output_path)]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return result.returncode == 0

    @staticmethod
    def from_meta(run_root: Path, meta: Dict[str, Any]) -> "SimulationRunner":
        """从 run metadata 创建 SimulationRunner."""
        binary_path = Path(meta.get("params", {}).get("binary_path", ""))
        return SimulationRunner(binary_path, run_root)
```

2. 更新 `dashboard_server.py` 使用 SimulationRunner：

```python
from .simulation_runner import SimulationRunner

# 在 _handle_post_runs 的 rerun 分支中：
runner = SimulationRunner(binary, run.root)
proc = runner.launch(
    config_path=run.root / "config.json",
    cycles=cycles,
    seed=seed,
    interval=interval,
    stream_path=stats_file,
)
```

3. 更新 `cli.py` 使用 SimulationRunner：

```python
from cpptlm.visualization.simulation_runner import SimulationRunner

# 在 run 命令中：
runner = SimulationRunner(binary_path, run_root)
proc = runner.launch(
    config_path=config_file,
    cycles=cycles,
    seed=seed,
    interval=interval,
    stream_path=stream_path,
)
```

**验证**：
- `python -c "from cpptlm.visualization.simulation_runner import SimulationRunner; print('OK')"`
- 运行现有测试，确保 cli.py 和 dashboard_server.py 行为不变

---

### 任务 0.2：移动内联 HTML 到静态文件

**影响**：`dashboard_ui.py` 当前约 20KB，内联 HTML 难以维护

**文件**：新建目录 `cpptlm/visualization/static/`

**步骤**：

1. 创建目录结构：
```
cpptlm/visualization/static/
├── home.html
├── run_view.html
├── new_run.html
└── dashboard.html
```

2. 从 `dashboard_ui.py` 提取 `_HOME_HTML` 到 `static/home.html`，保留 jinja2 风格占位符：
```html
<!-- $TITLE$ 等占位符由模板引擎替换 -->
<div class="runs-list">
  $RUNS_LIST$
</div>
```

3. 创建简单的模板加载器（无外部依赖）：
```python
from pathlib import Path
from typing import Dict

def load_template(name: str, variables: Dict[str, str]) -> str:
    """加载静态 HTML 模板并替换变量."""
    path = Path(__file__).parent / "static" / f"{name}.html"
    content = path.read_text(encoding="utf-8")
    for key, value in variables.items():
        content = content.replace(f"${key}$", value)
    return content
```

4. 更新 `dashboard_server.py` 使用 `load_template()` 而非 `make_xxx_html()` 函数

**验证**：
- Dashboard 页面可正常加载
- 所有链接和资源路径正确

---

### 任务 0.3：规划 FastAPI 迁移

**输出**：编写 `docs/architecture/fastapi-migration-plan.md`

**内容**：

1. **迁移时机**：Phase 2 开始时
2. **迁移范围**：
   - HTTP 路由层 → FastAPI Router
   - API 端点 → @app.get/post 装饰器
   - 保持 RunContext/RunsIndex 不变
   - 保持 SimulationRunner 不变
3. **预期收益**：
   - 路由复杂度从 O(n) 降为 O(1)（PathConverter）
   - 自动 OpenAPI 文档
   - 内置请求验证（Pydantic）
4. **风险缓解**：
   - 分阶段迁移，先迁 API 层，再迁页面
   - 保留旧端点别名以支持渐进切换

**验证**：
- 文档经过 Oracle 审查
- 任务分解到 Phase 2 任务列表

---

## 阶段 1：修复关键 Bug（预计 2-3 小时）

**前提**：Phase 0 完成，SimulationRunner 已提取

### 任务 1.1：修复 rerun 命令缺失 config 路径

**影响**：用户在 Dashboard 点击 Re-run 后，C++ 二进制因缺少 config 参数而失败

**文件**：`cpptlm/visualization/dashboard_server.py`

**步骤**：

```python
# 约第 280 行，rerun 分支，使用 SimulationRunner
runner = SimulationRunner(binary, run.root)
proc = runner.launch(
    config_path=run.root / "config.json",
    cycles=cycles,
    seed=params.get("seed", 0),
    interval=params.get("interval", 1000),
    stream_path=run.root / "stats.jsonl",
)

# 清空旧 stats 文件
run.root.joinpath("stats.jsonl").write_bytes(b"")

# 更新 meta
meta["rerun_count"] = meta.get("rerun_count", 0) + 1
meta["last_run"] = datetime.now().isoformat()
run.root.joinpath("meta.json").write_text(json.dumps(meta, indent=2))
```

**关键**：命令构建逻辑在 SimulationRunner 中，rerun 分支只做委托，无重复代码

**验证**：
- 点击 Dashboard Re-run，二进制收到完整参数
- stats.jsonl 正确写入

---

### 任务 1.2：自动拓扑图生成

**文件**：`cpptlm/visualization/dashboard_server.py`

**步骤**：

```python
# 在仿真启动后调用
runner = SimulationRunner(binary, run.root)
runner.generate_topology_dot(
    config_path=run.root / "config.json",
    output_path=run.root / "topology.dot",
)
```

**验证**：run_root 目录下生成 topology.dot

---

### 任务 1.3：自动报告生成

**文件**：`cpptlm/visualization/dashboard_server.py`

**步骤**：

```python
# 在仿真完成后调用（或按需调用）
runner.generate_report(
    stats_path=run.root / "stats.jsonl",
    output_path=run.root / "report.json",
)
```

**验证**：run_root 目录下生成 report.json

---

### 任务 1.4：检查 --seed 参数传递

**文件**：`cpptlm/visualization/dashboard_server.py` 和 `cpptlm/cli.py`

**步骤**：

使用 SimulationRunner 后，seed 参数通过 `runner.launch(seed=seed)` 统一传递，无需单独处理

**验证**：
- meta.json 中 seed 值正确传递
- 相同 seed 产生相同仿真结果

---

## 阶段 2：FastAPI 迁移 + 配置向导（预计 3-4 天）

**顺序**：先迁移 API 层，再添加向导页面

### 任务 2.1：FastAPI 骨架搭建

**文件**：新建 `cpptlm/visualization/app.py`

**步骤**：

```python
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import Optional, List
import json
from pathlib import Path

app = FastAPI(title="CppTLM Dashboard API", version="2.0")

# 复用现有 RunContext/RunsIndex
from .dashboard_server import RunsIndex, RunContext

_index = RunsIndex()

# Pydantic 模型
class RunCreateRequest(BaseModel):
    binary_path: str
    config_path: str
    cycles: int = 50000
    seed: int = 0
    interval: int = 1000

class RunResponse(BaseModel):
    id: str
    root: str
    status: str

# API 端点
@app.post("/api/runs", response_model=RunResponse)
async def create_run(req: RunCreateRequest):
    # 实现创建逻辑
    pass

@app.get("/api/runs/{run_id}")
async def get_run(run_id: str):
    run = _index.get_run(run_id)
    if not run:
        raise HTTPException(404, "Run not found")
    return {"id": run_id, "root": str(run.root), "status": "active" if run.is_active() else "done"}

@app.post("/api/runs/{run_id}/rerun")
async def rerun_run(run_id: str, cycles: int = 50000):
    run = _index.get_run(run_id)
    runner = SimulationRunner.from_meta(run.root, run.meta())
    proc = runner.launch(cycles=cycles)
    return {"pid": proc.pid}
```

**验证**：
- FastAPI 启动成功
- `/docs` 显示 OpenAPI 文档
- 现有功能通过 API 仍可用

---

### 任务 2.2：配置向导页面

**文件**：`cpptlm/visualization/static/new_run.html`

**步骤**：

1. 创建静态 HTML 文件（不使用 Python 字符串）：
```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>新建仿真 - CppTLM</title>
<script src="https://unpkg.com/htmx.org@2.0.4/dist/htmx.min.js"></script>
<script src="https://unpkg.com/alpinejs@3.x.x/dist/cdn.min.js" defer></script>
</head>
<body>
<div x-data="wizard()">
  <!-- Wizard 表单 -->
</div>
<script>
function wizard() {
  return {
    step: 1,
    config: { cycles: 50000, seed: 0 },
    async submit() {
      const resp = await fetch('/api/runs', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(this.config)
      });
      const data = await resp.json();
      window.location.href = `/runs/${data.id}`;
    }
  };
}
</script>
</body>
</html>
```

2. 在 FastAPI 中挂载静态文件：
```python
from fastapi.staticfiles import StaticFiles
app.mount("/static", StaticFiles(directory="cpptlm/visualization/static"), name="static")
```

**验证**：访问 `/new` 显示向导表单

---

## 阶段 3：拓扑编辑器（预计 5-7 天）

**重要**：`Alpine.js` 状态管理能力不足，无法支持拖拽拓扑编辑器的实时状态同步。

### 任务 3.1：前端框架选型

**推荐**：Svelte 或 React

**判断依据**：
- 拖拽拓扑编辑器需要 >100 个状态变量
- 组件间状态同步要求高
- Alpine.js 在此场景下会形成意大利面代码

**决策**：在 Phase 3 开始前，由 Oracle 评审确定最终选择

---

### 任务 3.2：拓扑编辑器实现

**文件**：`cpptlm/visualization/static/editor.html`（或独立前端项目）

**功能**：
- 拖拽添加模块
- 连接线绑定
- 导出 JSON 配置
- 实时预览

**验证**：
- 编辑器加载 < 2 秒
- 拖拽操作流畅
- 导出配置格式正确

---

## 阶段 4+：实时监控与增强功能

### 4.1 SSE 实时推送

**前提**：FastAPI 已迁移（SSE 需要异步支持）

### 4.2 多图表支持

**图表**：吞吐量、缓存命中率、队列深度

### 4.3 多运行对比

**功能**：选择多个运行，比较性能指标

---

## 验证命令

```bash
# Phase 0 验证
python -c "from cpptlm.visualization.simulation_runner import SimulationRunner; print('SimulationRunner OK')"
./build/bin/cpptlm_tests "[dashboard]"  # 现有测试通过

# Phase 1 验证
curl -X POST http://localhost:8050/api/runs/<id>/rerun -d '{"cycles": 50000}'
# 检查 stats.jsonl 正确写入

# Phase 2 验证
curl http://localhost:8000/docs  # FastAPI 文档
curl http://localhost:8000/new   # 向导页面

# Phase 3 验证
curl http://localhost:8000/editor  # 拓扑编辑器
```

---

*文档结束*