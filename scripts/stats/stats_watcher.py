#!/usr/bin/env python3
"""
stats_watcher.py — CppTLM 统计流实时监控器

功能:
  - 监控 stats_stream.jsonl 文件变化
  - 增量解析并聚合数据
  - 支持控制台实时输出
  - 可选: Web 仪表板 (需要 dash)

用法:
    # 控制台模式
    python3 stats_watcher.py --stream output/stats_stream.jsonl --no-gui

    # Web 仪表板模式 (需要 dash)
    python3 stats_watcher.py --stream output/stats_stream.jsonl --port 8050

依赖:
    pip install watchdog dash plotly  # 可选: dash, plotly

@author CppTLM Development Team
@date 2026-04-22
"""

import argparse
import json
import sys
import time
import threading
from collections import defaultdict
from typing import Dict, List, Optional, Any

# 尝试导入可选依赖
try:
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler
    WATCHDOG_AVAILABLE = True
except ImportError:
    WATCHDOG_AVAILABLE = False
    print("Warning: watchdog not installed. File monitoring disabled.")
    print("  Install with: pip install watchdog")

try:
    import dash
    from dash import dcc, html
    from dash.dependencies import Input, Output
    DASH_AVAILABLE = True
except ImportError:
    DASH_AVAILABLE = False
    print("Warning: dash not installed. Web dashboard disabled.")
    print("  Install with: pip install dash plotly")


class StatsStreamHandler(FileSystemEventHandler if WATCHDOG_AVAILABLE else object):
    """监控 stats_stream.jsonl 文件变化"""

    def __init__(self, filepath: str, callback):
        self.filepath = filepath
        self.callback = callback
        self.last_position = 0
        self.lock = threading.Lock()

    def on_modified(self, event):
        if not WATCHDOG_AVAILABLE:
            return
        if event.src_path != self.filepath:
            return
        self._read_new_lines()

    def _read_new_lines(self):
        if not WATCHDOG_AVAILABLE:
            return
        with self.lock:
            try:
                with open(self.filepath, 'r') as f:
                    f.seek(self.last_position)
                    new_lines = f.readlines()
                    for line in new_lines:
                        if line.strip():
                            try:
                                record = json.loads(line)
                                self.callback(record)
                            except json.JSONDecodeError:
                                pass
                    self.last_position = f.tell()
            except FileNotFoundError:
                pass

    def read_all(self):
        """读取整个文件（初始化时使用）"""
        if not WATCHDOG_AVAILABLE:
            return
        with self.lock:
            try:
                with open(self.filepath, 'r') as f:
                    for line in f:
                        if line.strip():
                            try:
                                record = json.loads(line)
                                self.callback(record)
                            except json.JSONDecodeError:
                                pass
                    self.last_position = f.tell()
            except FileNotFoundError:
                pass


class StatsAggregator:
    """增量聚合统计流数据"""

    def __init__(self):
        self.history = defaultdict(list)
        self.latest = {}
        self.lock = threading.Lock()
        self.update_count = 0

    def add_snapshot(self, record: Dict):
        with self.lock:
            timestamp = record.get("timestamp_ns", 0) / 1e9
            cycle = record.get("simulation_cycle", 0)

            data = record.get("data", {})
            self._flatten_and_store("", data, timestamp, cycle)
            self.update_count += 1

    def _flatten_and_store(self, prefix: str, data: Dict, timestamp: float, cycle: int):
        for key, value in data.items():
            path = f"{prefix}.{key}" if prefix else key

            if isinstance(value, dict):
                self._flatten_and_store(path, value, timestamp, cycle)
            else:
                self.history[path].append({
                    "timestamp": timestamp,
                    "cycle": cycle,
                    "value": value
                })
                self.latest[path] = value

    def get_latest(self) -> Dict:
        with self.lock:
            return dict(self.latest)

    def get_history(self, metric: str, limit: int = 100) -> List:
        with self.lock:
            hist = self.history.get(metric, [])
            return hist[-limit:]

    def get_all_metrics(self) -> List[str]:
        with self.lock:
            return sorted(self.latest.keys())

    def get_update_count(self) -> int:
        return self.update_count


class ConsoleReporter:
    """控制台实时输出"""

    def __init__(self, aggregator: StatsAggregator, interval: float = 1.0):
        self.aggregator = aggregator
        self.interval = interval
        self.running = False
        self.thread = None

    def start(self):
        self.running = True
        self.thread = threading.Thread(target=self._run)
        self.thread.daemon = True
        self.thread.start()

    def stop(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=2.0)

    def _run(self):
        last_count = 0
        while self.running:
            count = self.aggregator.get_update_count()
            if count != last_count:
                self._print_stats()
                last_count = count
            time.sleep(self.interval)

    def _print_stats(self):
        latest = self.aggregator.get_latest()
        if not latest:
            return

        print("\033[2J\033[H")  # 清屏
        print("=" * 60)
        print("CppTLM Stats Watcher - Real-time Statistics")
        print("=" * 60)

        # 显示关键指标
        print("\n[Key Metrics]")
        for key in sorted(latest.keys())[:20]:
            value = latest[key]
            if isinstance(value, (int, float)):
                if isinstance(value, float):
                    print(f"  {key}: {value:.4f}")
                else:
                    print(f"  {key}: {value}")
            else:
                print(f"  {key}: {value}")

        print(f"\n[Update Count: {self.aggregator.get_update_count()}]")
        print("[Press Ctrl+C to stop]")


class DashboardServer:
    """Dash Web 仪表板"""

    def __init__(self, aggregator: StatsAggregator, port: int = 8050):
        self.aggregator = aggregator
        self.port = port
        self.app = dash.Dash(__name__)
        self._setup_layout()

    def _setup_layout(self):
        self.app.layout = html.Div([
            html.H1("CppTLM Real-Time Performance Dashboard"),

            html.Div([
                html.Span("Updates: "),
                html.Span(id="update-count", children="0"),
                html.Span(" | Last refresh: "),
                html.Span(id="last-time", children="--"),
            ], style={"marginBottom": "20px"}),

            html.Div(id="metrics-container", children=[
                html.Div("Waiting for data...", id="metrics-placeholder")
            ]),

            html.H3("Latency Distribution"),
            dcc.Graph(id="latency-chart"),

            html.H3("Throughput Over Time"),
            dcc.Graph(id="throughput-chart"),

            dcc.Interval(
                id="refresh-interval",
                interval=1 * 1000,
                n_intervals=0
            )
        ])

        @self.app.callback(
            Output("metrics-container", "children"),
            Output("update-count", "children"),
            Input("refresh-interval", "n_intervals")
        )
        def update_metrics(n):
            latest = self.aggregator.get_latest()
            update_count = self.aggregator.get_update_count()
            timestamp = time.strftime("%H:%M:%S")

            if not latest:
                return [html.Div("No data yet...")], str(update_count)

            cards = []
            for key, value in sorted(latest.items())[:20]:
                if isinstance(value, (int, float)):
                    value_str = f"{value:.4f}" if isinstance(value, float) else str(value)
                else:
                    value_str = str(value)

                cards.append(html.Div([
                    html.Div(key.split('.')[-1], style={"fontSize": "12px", "color": "#666"}),
                    html.Div(value_str, style={"fontSize": "20px", "fontWeight": "bold"})
                ], style={
                    "display": "inline-block",
                    "margin": "5px",
                    "padding": "10px",
                    "border": "1px solid #ddd",
                    "borderRadius": "8px",
                    "minWidth": "120px"
                }))

            return cards, str(update_count)

        @self.app.callback(
            Output("latency-chart", "figure"),
            Input("refresh-interval", "n_intervals")
        )
        def update_latency_chart(n):
            latency_keys = [k for k in self.aggregator.get_all_metrics() if 'latency' in k.lower() or 'delay' in k.lower()]
            if not latency_keys:
                return {"data": [], "layout": {"title": "No latency data"}}

            figures = []
            for key in latency_keys[:5]:
                hist = self.aggregator.get_history(key, limit=50)
                if hist:
                    figures.append({
                        "x": [h["cycle"] for h in hist],
                        "y": [h["value"] for h in hist],
                        "type": "line",
                        "name": key.split('.')[-1]
                    })

            if not figures:
                return {"data": [], "layout": {"title": "No latency data"}}

            return {
                "data": figures,
                "layout": {
                    "title": "Latency Over Time",
                    "xaxis": {"title": "Simulation Cycle"},
                    "yaxis": {"title": "Latency"},
                }
            }

        @self.app.callback(
            Output("throughput-chart", "figure"),
            Input("refresh-interval", "n_intervals")
        )
        def update_throughput_chart(n):
            bw_keys = [k for k in self.aggregator.get_all_metrics() if 'throughput' in k.lower() or 'bw' in k.lower() or 'bandwidth' in k.lower()]
            if not bw_keys:
                return {"data": [], "layout": {"title": "No throughput data"}}

            figures = []
            for key in bw_keys[:5]:
                hist = self.aggregator.get_history(key, limit=50)
                if hist:
                    figures.append({
                        "x": [h["cycle"] for h in hist],
                        "y": [h["value"] for h in hist],
                        "type": "line",
                        "name": key.split('.')[-1]
                    })

            if not figures:
                return {"data": [], "layout": {"title": "No throughput data"}}

            return {
                "data": figures,
                "layout": {
                    "title": "Bandwidth Over Time",
                    "xaxis": {"title": "Simulation Cycle"},
                    "yaxis": {"title": "Bandwidth"},
                }
            }

    def run(self, debug: bool = False):
        print(f"Dashboard available at http://localhost:{self.port}")
        self.app.run_server(host="0.0.0.0", port=self.port, debug=debug)


def main():
    parser = argparse.ArgumentParser(
        description="CppTLM Stats Stream Watcher"
    )
    parser.add_argument("--stream", "-s", default="output/stats_stream.jsonl",
                       help="Path to stats stream JSONL file")
    parser.add_argument("--port", "-p", type=int, default=8050,
                       help="Dashboard port (default: 8050)")
    parser.add_argument("--no-gui", action="store_true",
                       help="Console output only (no web dashboard)")
    parser.add_argument("--interval", "-i", type=float, default=1.0,
                       help="Console update interval in seconds (default: 1.0)")

    args = parser.parse_args()

    aggregator = StatsAggregator()

    # 读取已有数据
    print(f"Reading existing data from {args.stream}...")
    handler = StatsStreamHandler(args.stream, aggregator.add_snapshot)
    handler.read_all()

    if args.no_gui or not DASH_AVAILABLE:
        # 控制台模式
        console = ConsoleReporter(aggregator, interval=args.interval)

        if WATCHDOG_AVAILABLE:
            observer = Observer()
            observer.schedule(handler, args.stream, recursive=False)
            observer.start()
            print(f"Watching {args.stream} for updates...")
            print("Press Ctrl+C to stop")

            try:
                console.start()
                while True:
                    time.sleep(1)
            except KeyboardInterrupt:
                print("\nStopping watcher...")
                console.stop()
                observer.stop()
                observer.join()
        else:
            # 无 watchdog，只能显示已有数据
            print("File monitoring not available. Showing current data:")
            console._print_stats()

    else:
        # Web 仪表板模式
        server = DashboardServer(aggregator, args.port)

        if WATCHDOG_AVAILABLE:
            observer = Observer()
            observer.schedule(handler, args.stream, recursive=False)
            observer.start()

            try:
                server.run()
            except KeyboardInterrupt:
                print("\nStopping watcher...")
                observer.stop()
                observer.join()
        else:
            print("File monitoring not available. Starting dashboard anyway...")
            server.run()


if __name__ == "__main__":
    main()