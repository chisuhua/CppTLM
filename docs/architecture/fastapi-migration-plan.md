# FastAPI 迁移规划

## 1. 迁移时机

- Phase 2 开始时（端点数量将超 10 个阈值）

当 API 端点数量接近或超过 10 个时，手动路由管理变得难以维护。此时引入 FastAPI 可以获得：
- 自动 OpenAPI 文档生成
- 内置请求验证
- 类型安全与 IDE 支持
- 高性能异步处理

## 2. 迁移范围

| 组件 | 迁移策略 | 备注 |
|------|---------|------|
| HTTP 路由层 | → FastAPI Router | 使用路径参数替代 if/elif |
| API 端点 | → @app.get/post | 自动 OpenAPI 文档 |
| 请求验证 | → Pydantic BaseModel | 替代手动解析 |
| SimulationRunner | 保持 | 不变 |
| RunContext/RunsIndex | 保持 | 不变 |
| 静态 HTML | 保持 | 移动到 /static/ 目录 |

### 详细说明

**HTTP 路由层**
```python
# 旧: stdlib HTTP Server
if path == "/api/runs":
    handle_runs(request)
elif path == "/api/stop":
    handle_stop(request)

# 新: FastAPI Router
router = APIRouter()
@router.get("/runs")
async def list_runs():
    ...
```

**API 端点迁移**
```python
# 旧: 手动解析
def handle_runs(request):
    body = json.loads(request.body)
    run_id = body.get("run_id")

# 新: Pydantic BaseModel
class RunRequest(BaseModel):
    run_id: str

@router.post("/runs")
async def list_runs(req: RunRequest):
    ...
```

**静态文件**
```python
# FastAPI 挂载静态目录
from fastapi.staticfiles import StaticFiles
app.mount("/static", StaticFiles(directory="static"), name="static")
```

## 3. 迁移步骤

### Phase 1: 骨架搭建

1. 创建 `app.py` FastAPI 骨架

```python
# app.py
from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
import uvicorn

app = FastAPI(title="CppTLM API", version="2.0")

# 挂载静态文件
app.mount("/static", StaticFiles(directory="static"), name="static")

# 导入路由
from api import runs, topology, metrics

app.include_router(runs.router, prefix="/api", tags=["runs"])
app.include_router(topology.router, prefix="/api", tags=["topology"])
app.include_router(metrics.router, prefix="/api", tags=["metrics"])

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
```

### Phase 2: 逐端点迁移

2. 逐一迁移 /api/* 端点

```python
# api/runs.py
from fastapi import APIRouter
from pydantic import BaseModel
from typing import List, Optional

router = APIRouter()

class RunRequest(BaseModel):
    run_id: Optional[str] = None
    limit: int = 100

class RunResponse(BaseModel):
    run_id: str
    status: str
    created_at: str

@router.get("/runs", response_model=List[RunResponse])
async def list_runs(limit: int = 100):
    runs_index = RunsIndex.get_instance()
    runs = runs_index.list_runs(limit=limit)
    return [RunResponse(run_id=r.run_id, status=r.status, created_at=r.created_at) for r in runs]

@router.post("/runs")
async def create_run(req: RunRequest):
    runner = SimulationRunner.get_instance()
    run_id = runner.start(req.run_id)
    return {"run_id": run_id}

@router.post("/stop")
async def stop_run(req: RunRequest):
    runner = SimulationRunner.get_instance()
    runner.stop(req.run_id)
    return {"status": "stopped"}
```

3. 迁移页面端点 /, /new, /runs/*

```python
# pages/runs.py
from fastapi import APIRouter
from fastapi.responses import HTMLResponse
import os

router = APIRouter()

@router.get("/", response_class=HTMLResponse)
async def index():
    path = os.path.join("static", "index.html")
    with open(path) as f:
        return f.read()

@router.get("/new", response_class=HTMLResponse)
async def new_run():
    path = os.path.join("static", "new.html")
    with open(path) as f:
        return f.read()

@router.get("/runs/{run_id}", response_class=HTMLResponse)
async def view_run(run_id: str):
    path = os.path.join("static", "run.html")
    with open(path) as f:
        return f.read()
```

### Phase 3: 集成与验证

4. 挂载静态文件 `app.mount("/static", ...)`

已在骨架中配置。

5. 验证所有功能

```bash
# 启动服务
uvicorn app:app --reload

# 访问 OpenAPI 文档
curl http://localhost:8000/docs

# 测试所有端点
curl http://localhost:8000/api/runs
curl -X POST http://localhost:8000/api/runs -d '{"run_id": "test"}'
```

6. 清理旧端点（保留别名 1 个 Sprint）

```python
# 别名支持（向后兼容）
@router.get("/runs/legacy")
async def runs_legacy():
    return await list_runs()
```

## 4. 风险缓解

### 渐进迁移策略

- **保留旧端点别名**: 迁移期间，旧端点保持可用
- **并行运行**: 新旧服务可同时运行进行对比测试
- **功能等价性验证**: 每个端点迁移后立即运行测试套件

### 回滚计划

| 场景 | 回滚操作 |
|------|---------|
| 新服务启动失败 | 保持 stdlib HTTP Server 运行 |
| 特定端点异常 | 禁用该端点，保留旧实现 |
| 整体迁移失败 | 删除 app.py，移除 FastAPI 依赖 |

```python
# 回滚配置
# run.sh
if [ "$USE_FASTAPI" = "true" ]; then
    uvicorn app:app --host 0.0.0.0 --port 8000
else
    python server.py  # stdlib HTTP Server
fi
```

### 测试策略

```bash
# 每个端点迁移后运行
pytest tests/api/ -v

# 端到端测试
pytest tests/e2e/ -v

# 性能基准对比
wrk -t4 -c100 http://localhost:8000/api/runs
```

## 5. 依赖清单

```
# requirements.txt
fastapi>=0.104.0
uvicorn>=0.24.0
pydantic>=2.0.0

# 可选：性能优化
httpx>=0.25.0  # 异步 HTTP 客户端
```

### 依赖安装

```bash
pip install fastapi uvicorn pydantic
```

## 6. 迁移检查清单

- [ ] 创建 `app.py` FastAPI 应用骨架
- [ ] 配置静态文件挂载 `/static/`
- [ ] 迁移 `/api/runs` 端点
- [ ] 迁移 `/api/stop` 端点
- [ ] 迁移 `/api/topology` 端点
- [ ] 迁移 `/api/metrics` 端点
- [ ] 迁移页面端点 `/`, `/new`, `/runs/{id}`
- [ ] 配置 Pydantic 请求/响应模型
- [ ] 验证 OpenAPI 文档生成
- [ ] 运行现有测试套件
- [ ] 执行端到端功能测试
- [ ] 性能基准对比（旧 vs 新）
- [ ] 清理旧端点实现
- [ ] 更新部署脚本

## 7. 时间估算

| 阶段 | 任务 | 预计工时 |
|------|------|---------|
| Phase 1 | 骨架搭建 | 2 小时 |
| Phase 2 | 端点迁移（每个） | 1-2 小时/端点 |
| Phase 3 | 集成验证 | 4 小时 |
| 缓冲 | 测试与修复 | 4 小时 |
| **总计** | | **1-2 天** |