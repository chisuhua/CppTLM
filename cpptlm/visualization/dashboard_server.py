"""
cpptlm/visualization/dashboard_server.py — 实时 Web Dashboard 服务器。

零外部依赖（仅 Python 标准库 + Plotly.js CDN）。
Serve 模式：启动轻量级 HTTP 服务器，浏览器访问 http://localhost:<port> 查看。
"""

from __future__ import annotations

import http.server
import json
import os
import threading
import time
from pathlib import Path
from typing import Any, Dict, List, Optional


# ── HTML 模板（嵌入 Plotly.js CDN）─────────────────────────────────────────

_DASHBOARD_HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CppTLM 实时 Dashboard</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 0; padding: 20px; background: #f8f9fa; color: #333; }
  h1 { font-size: 1.5em; border-bottom: 2px solid #333; padding-bottom: 8px; }
  .status-bar { display: flex; gap: 20px; margin: 10px 0; padding: 10px; background: #e9ecef; border-radius: 6px; font-size: 0.9em; }
  .status-bar .label { color: #666; }
  .chart-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  .chart-box { background: white; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); padding: 12px; }
  .chart-box h3 { margin: 0 0 8px 0; font-size: 1em; color: #555; }
  .chart-box .plotly-graph-div { width: 100%; height: 300px; }
  .error { color: #dc3545; padding: 10px; }
  .table-wrap { overflow-x: auto; }
  table { border-collapse: collapse; width: 100%; font-size: 0.85em; }
  th, td { border: 1px solid #dee2e6; padding: 6px 10px; text-align: left; }
  th { background: #f1f3f5; }
  td.num { text-align: right; font-variant-numeric: tabular-nums; }
  @media (max-width: 900px) { .chart-grid { grid-template-columns: 1fr; } }
</style>
</head>
<body>
<h1>CppTLM 仿真实时 Dashboard</h1>
<div class="status-bar" id="status">
  <span><span class="label">Cycle:</span> <span id="cycle">—</span></span>
  <span><span class="label">Records:</span> <span id="records">0</span></span>
  <span><span class="label">Groups:</span> <span id="groups">—</span></span>
  <span><span class="label">Last update:</span> <span id="update">—</span></span>
</div>
<div class="chart-grid" id="charts"></div>

<script>
const POLL_MS = 2000;
let lastData = null;

async function poll() {
  try {
    const resp = await fetch('/data');
    const data = await resp.json();
    lastData = data;
    render(data);
    document.getElementById('update').textContent = new Date().toLocaleTimeString();
  } catch(e) {
    console.warn('poll error:', e);
  }
  setTimeout(poll, POLL_MS);
}

function render(data) {
  // Status bar
  document.getElementById('cycle').textContent = data.simulation_cycle || '—';
  document.getElementById('records').textContent = data.record_count || 0;
  document.getElementById('groups').textContent = (data.groups || []).join(', ') || '—';

  const container = document.getElementById('charts');
  container.innerHTML = '';

  // Latency charts per group
  for (const group of (data.groups || [])) {
    const groupData = data.by_group[group];
    if (!groupData) continue;

    // Latency over time
    if (groupData.latency_cycle && groupData.latency_cycle.length > 0) {
      const box = createChartBox(group + ' 延迟 (周期)');
      const trace = {
        x: groupData.latency_cycle, y: groupData.latency_avg,
        type: 'scatter', mode: 'lines+markers',
        name: 'avg', line: { color: '#2196F3' },
      };
      Plotly.newPlot(box.querySelector('.plot'), [trace], {
        margin: { t: 10, r: 10, b: 30, l: 50 },
        xaxis: { title: 'Cycle' },
        yaxis: { title: 'Latency (cycle)' },
      });
      container.appendChild(box);
    }

    // Requests timeline
    if (groupData.req_cycle && groupData.req_cycle.length > 0) {
      const box = createChartBox(group + ' 请求统计');
      const traces = [];
      if (groupData.hits) traces.push({ x: groupData.req_cycle, y: groupData.hits, type: 'scatter', mode: 'lines+markers', name: 'hits', line: { color: '#4CAF50' } });
      if (groupData.misses) traces.push({ x: groupData.req_cycle, y: groupData.misses, type: 'scatter', mode: 'lines+markers', name: 'misses', line: { color: '#f44336' } });
      Plotly.newPlot(box.querySelector('.plot'), traces, {
        margin: { t: 10, r: 10, b: 30, l: 50 },
        xaxis: { title: 'Cycle' },
        yaxis: { title: 'Count' },
      });
      container.appendChild(box);
    }
  }

  // Metrics summary table
  if (data.metrics && data.metrics.length > 0) {
    const box = document.createElement('div');
    box.className = 'chart-box';
    box.innerHTML = '<h3>性能摘要</h3><div class="table-wrap"><table><tr><th>Group</th><th>Mean</th><th>P95</th><th>P99</th><th>Max</th><th>Count</th></tr>' +
      data.metrics.map(m => `<tr><td>${m.group}</td><td class="num">${m.mean.toFixed(2)}</td><td class="num">${m.p95.toFixed(2)}</td><td class="num">${m.p99.toFixed(2)}</td><td class="num">${m.max.toFixed(2)}</td><td class="num">${m.count}</td></tr>`).join('') +
      '</table></div>';
    container.appendChild(box);
  }

  // Bottlenecks
  if (data.bottlenecks && data.bottlenecks.length > 0) {
    const box = document.createElement('div');
    box.className = 'chart-box';
    box.innerHTML = '<h3>瓶颈检测</h3><div class="table-wrap"><table><tr><th>Group</th><th>Latency</th><th>Severity</th></tr>' +
      data.bottlenecks.map(b => `<tr><td>${b.group}</td><td class="num">${b.latency.toFixed(2)}</td><td style="color:${b.severity === 'high' ? '#dc3545' : '#ffc107'}">${b.severity}</td></tr>`).join('') +
      '</table></div>';
    container.appendChild(box);
  }
}

function createChartBox(title) {
  const box = document.createElement('div');
  box.className = 'chart-box';
  box.innerHTML = `<h3>${title}</h3><div class="plot"></div>`;
  return box;
}

poll();
</script>
</body>
</html>
"""


class DashboardServer:
    """零依赖实时 Web Dashboard 服务器。

    在后台线程中运行 HTTP 服务器，提供：
      /       →  Plotly.js 实时 Dashboard（自动轮询）
      /data   →  当前 JSONL 统计数据的 JSON 快照
      /metrics → MetricSummary + AnomalyDetector 计算结果

    用法::

        server = DashboardServer("output/stats_soc.jsonl", port=8080)
        server.start()       # 后台线程启动
        server.serve_forever()  # 主线程阻塞
        server.stop()
    """

    def __init__(
        self,
        jsonl_path: str,
        port: int = 8080,
    ):
        self.jsonl_path = Path(jsonl_path)
        self.port = port
        self._server: Optional[http.server.HTTPServer] = None
        self._thread: Optional[threading.Thread] = None
        self._prev_size: int = 0
        self._cached_data: Dict[str, Any] = {
            "simulation_cycle": 0,
            "record_count": 0,
            "groups": [],
            "by_group": {},
            "metrics": [],
            "bottlenecks": [],
        }

    def _collect_data(self) -> Dict[str, Any]:
        """读取 JSONL 并聚合为 Dashboard 数据。"""
        if not self.jsonl_path.exists():
            return self._cached_data

        # 增量读取
        records: List[Dict] = []
        try:
            with open(self.jsonl_path) as f:
                for line in f:
                    line = line.strip()
                    if line:
                        try:
                            records.append(json.loads(line))
                        except json.JSONDecodeError:
                            pass
        except (OSError, json.JSONDecodeError):
            return self._cached_data

        if not records:
            return self._cached_data

        # 按 group 聚合
        by_group: Dict[str, Dict[str, List]] = {}
        max_cycle = 0
        for r in records:
            g = r.get("group", "?")
            if g not in by_group:
                by_group[g] = {"latency_cycle": [], "latency_avg": [],
                               "req_cycle": [], "hits": [], "misses": []}
            cycle = r.get("simulation_cycle", 0)
            max_cycle = max(max_cycle, cycle)
            data = r.get("data", {})

            # 延迟（Distribution 取 avg）
            lat = data.get("latency", None)
            if isinstance(lat, dict) and "avg" in lat:
                by_group[g]["latency_cycle"].append(cycle)
                by_group[g]["latency_avg"].append(lat["avg"])

            # 请求统计
            for key in ("requests", "hits", "misses",
                        "flits_received", "flits_sent"):
                val = data.get(key, None)
                if isinstance(val, (int, float)):
                    if key == "requests" or key == "flits_received":
                        by_group[g]["req_cycle"].append(cycle)
                    target = key
                    if "req" in target or "flit" in target:
                        target = "hits"
                    by_group[g].setdefault(target, []).append(val)

        # 计算 MetricSummary
        from cpptlm.simulation.result import Result
        from cpptlm.analysis import MetricSummary, AnomalyDetector
        from cpptlm.analysis.adapters import adapt_result

        res = Result.from_jsonl(str(self.jsonl_path))
        adapted = adapt_result(res)
        metrics_list: List[Dict] = []
        for g in sorted(adapted.groups()):
            stats = MetricSummary(adapted).latency_statistics(group=g)
            if stats["count"] > 0:
                metrics_list.append({
                    "group": g,
                    "mean": stats["mean"],
                    "p95": stats["p95"],
                    "p99": stats["p99"],
                    "max": stats["max"],
                    "count": stats["count"],
                })

        bottlenecks = AnomalyDetector(adapted).identify_bottlenecks(
            threshold_percentile=80.0)
        bn_list: List[Dict] = [{
            "group": b["group"],
            "latency": b["mean_latency"],
            "severity": b["severity"],
        } for b in bottlenecks]

        self._cached_data = {
            "simulation_cycle": max_cycle,
            "record_count": len(records),
            "groups": sorted(adapted.groups()),
            "by_group": by_group,
            "metrics": metrics_list,
            "bottlenecks": bn_list,
        }
        return self._cached_data

    # ── HTTP 请求处理 ──

    class _Handler(http.server.BaseHTTPRequestHandler):

        def log_message(self, fmt, *args):
            pass  # 静默日志

        @property
        def _dashboard(self) -> "DashboardServer":
            """通过 HTTPServer 实例获取 DashboardServer 引用。"""
            return self.server._server_ref  # type: ignore

        def _send_json(self, data):
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(data).encode())

        def _send_html(self, html):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode())

        def do_GET(self):
            svr = self._dashboard
            if self.path == "/data":
                self._send_json(svr._collect_data())
            elif self.path == "/" or self.path == "":
                self._send_html(_DASHBOARD_HTML)
            else:
                self.send_response(404)
                self.end_headers()

        # 抑制 ConnectionError 日志
        def handle_one_request(self):
            try:
                super().handle_one_request()
            except (ConnectionError, BrokenPipeError, OSError):
                pass

    def start(self):
        """在后台线程中启动 HTTP 服务器。"""
        if self._server:
            return

        self._server = http.server.HTTPServer(
            ("0.0.0.0", self.port), self._Handler)
        self._server._server_ref = self  # type: ignore

        self._thread = threading.Thread(
            target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def serve_forever(self):
        """主线程阻塞（直接在当前线程启动服务器）。"""
        self._server = http.server.HTTPServer(
            ("0.0.0.0", self.port), self._Handler)
        self._server._server_ref = self  # type: ignore
        print(f"  [Dashboard] http://localhost:{self.port}")
        try:
            self._server.serve_forever()
        except KeyboardInterrupt:
            self.stop()

    def stop(self):
        if self._server:
            self._server.shutdown()
            self._server = None

    @property
    def url(self) -> str:
        return f"http://localhost:{self.port}"
