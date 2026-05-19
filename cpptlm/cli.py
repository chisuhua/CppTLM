#!/usr/bin/env python3
"""cpptlm/cli.py — 命令行入口."""

import argparse
import sys
from pathlib import Path

def find_cpptlm_sim_binary() -> str | None:
    """在 build/bin/ 目录下查找 cpptlm_sim 可执行文件."""
    import os
    search_paths = [
        Path("build/bin/cpptlm_sim"),
        Path(__file__).parent.parent / "build/bin/cpptlm_sim",
        Path.cwd() / "build/bin/cpptlm_sim",
    ]
    for p in search_paths:
        if p.exists() and os.access(p, os.X_OK):
            return str(p)
    return None

def run_simulation(config_path: str, cycles: int, interval: int,
                   seed: int, output_dir: str, dashboard: bool) -> bool:
    """运行 C++ 仿真器.

    1. 查找 cpptlm_sim binary
    2. 使用 RunsIndex.create_run() 创建运行目录
    3. 调用 subprocess.run() 执行仿真
    4. 写入 pid 文件

    返回 True 成功，False 失败。
    """
    import subprocess

    binary = find_cpptlm_sim_binary()
    if not binary:
        print("[ERROR] cpptlm_sim binary not found")
        return False

    from cpptlm.visualization.run_context import RunsIndex
    index = RunsIndex(output_dir)

    with open(config_path) as f:
        config_json = f.read()

    params = {
        "cycles": cycles,
        "interval": interval,
        "seed": seed,
        "binary_path": binary,
        "config_path": config_path,
    }
    run_ctx = index.create_run(config_json, params)

    print(f"[Simulation] cycles={cycles}, interval={interval}")
    print(f"[Simulation] Output: {run_ctx.root / 'stats.jsonl'}")

    from cpptlm.visualization.simulation_runner import SimulationRunner
    runner = SimulationRunner(Path(binary), run_ctx.root)
    stream_path = run_ctx.root / "stats.jsonl"
    proc = runner.launch(
        config_path=Path(config_path),
        cycles=cycles,
        seed=seed,
        interval=interval,
        stream_path=stream_path,
    )
    (run_ctx.root / "pid").write_text(str(proc.pid))

    pid_file = run_ctx.root / "pid"
    try:
        if dashboard:
            import uvicorn
            from cpptlm.visualization.app import app
            import threading
            server_port = 8001
            threading.Thread(target=lambda: uvicorn.run(app, host="0.0.0.0", port=server_port), daemon=True).start()
            import webbrowser
            webbrowser.open(f"http://localhost:{server_port}/?run={run_ctx.run_id}")

        proc.wait()
    finally:
        if pid_file.exists():
            pid_file.unlink()

    return proc.returncode == 0

def main():
    parser = argparse.ArgumentParser(
        prog="cpptlm",
        description="CppTLM Python 工具"
    )
    subparsers = parser.add_subparsers(dest="command")

    dash_parser = subparsers.add_parser("dashboard", help="启动统一可视化 Dashboard")
    dash_parser.add_argument("--port", type=int, default=8001, help="Dashboard 端口 (默认 8001)")
    dash_parser.add_argument("--runs-dir", default="runs", help="runs 目录路径")
    dash_parser.add_argument("--open", metavar="RUN_ID", help="直接打开指定运行目录")
    dash_parser.add_argument("--engine", choices=["fastapi", "stdlib"], default="fastapi",
                            help="HTTP 引擎: fastapi (默认，支持 SSE) 或 stdlib")

    run_parser = subparsers.add_parser("run", help="运行仿真")
    run_parser.add_argument("--config", help="JSON 配置文件路径")
    run_parser.add_argument("--cycles", type=int, default=50000, help="仿真周期数")
    run_parser.add_argument("--interval", type=int, default=1000, help="统计输出间隔")
    run_parser.add_argument("--seed", type=int, default=0, help="随机种子")
    run_parser.add_argument("--output-dir", default="runs", help="运行目录")
    run_parser.add_argument("--dashboard", action="store_true", help="启动后自动打开 Dashboard")
    run_parser.add_argument("--generate-only", action="store_true", help="仅生成配置，不运行仿真")

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        return

    if args.command == "dashboard":
        if args.engine == "fastapi":
            import uvicorn
            from cpptlm.visualization.app import app
            print(f"Starting FastAPI Dashboard on http://localhost:{args.port}")
            print(f"  API docs: http://localhost:{args.port}/docs")
            print(f"  SSE endpoint: /api/runs/{{id}}/stream")
            uvicorn.run(app, host="0.0.0.0", port=args.port)
        else:
            from cpptlm.visualization.dashboard_server import DashboardServer
            server = DashboardServer(port=args.port, runs_dir=args.runs_dir)
            if args.open:
                print(f"Opening run: {args.open}")
                server.set_open_run(args.open)
            print(f"Starting stdlib Dashboard on http://localhost:{args.port}")
            server.serve_forever()
    elif args.command == "run":
        if not args.config:
            print("[ERROR] --config is required for 'run' command")
            sys.exit(1)

        binary = find_cpptlm_sim_binary()
        if not binary:
            print("[ERROR] cpptlm_sim binary not found")
            sys.exit(1)

        from cpptlm.visualization.run_context import RunsIndex
        index = RunsIndex(args.output_dir)

        with open(args.config) as f:
            config_json = f.read()

        params = {
            "cycles": args.cycles,
            "interval": args.interval,
            "seed": args.seed,
            "binary_path": binary,
            "config_path": args.config,
        }
        run_ctx = index.create_run(config_json, params)

        if args.generate_only:
            print(f"[Generate] Output: {run_ctx.root}")
            print(f"[Generate] Config: {run_ctx.root / 'config.json'}")
            print(f"[Generate] Meta:   {run_ctx.root / 'meta.json'}")
            sys.exit(0)

        success = run_simulation(
            config_path=args.config,
            cycles=args.cycles,
            interval=args.interval,
            seed=args.seed,
            output_dir=args.output_dir,
            dashboard=args.dashboard,
        )
        sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
