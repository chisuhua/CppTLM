from __future__ import annotations

import os
from pathlib import Path
from typing import Optional


class ReportGenerator:
    def __init__(self, result_path: str, topology_image: Optional[str] = None):
        self.result_path = result_path
        self.topology_image = topology_image

    def generate(self, output_path: str = "cpptlm_report.html"):
        from cpptlm.simulation.result import Result
        from cpptlm.analysis import MetricSummary, AnomalyDetector
        from cpptlm.analysis.adapters import adapt_result

        data = adapt_result(Result.from_jsonl(self.result_path))
        metrics = MetricSummary(data)
        detector = AnomalyDetector(data)
        bottlenecks = detector.identify_bottlenecks()

        groups_html = "<table><tr><th>Group</th><th>Mean Latency</th><th>P95</th><th>P99</th></tr>"
        for group in data.groups():
            stats = metrics.latency_statistics(group=group)
            groups_html += f"<tr><td>{group}</td><td>{stats['mean']:.2f}</td><td>{stats['p95']:.2f}</td><td>{stats['p99']:.2f}</td></tr>"
        groups_html += "</table>"

        bottleneck_html = "<table><tr><th>Group</th><th>Mean Latency</th><th>Severity</th></tr>"
        for b in bottlenecks:
            bottleneck_html += f"<tr><td>{b['group']}</td><td>{b['mean_latency']:.2f}</td><td>{b['severity']}</td></tr>"
        bottleneck_html += "</table>"

        # ── Topology image ──
        topology_html = ""
        if self.topology_image and Path(self.topology_image).exists():
            img_rel = os.path.relpath(self.topology_image, os.path.dirname(output_path))
            topology_html = f"""
<h2>Topology</h2>
<img src="{img_rel}" alt="SoC Topology" style="max-width:100%;height:auto;border:1px solid #ccc;"/>
"""

        html = f"""<!DOCTYPE html>
<html>
<head><title>CppTLM Report</title>
<style>
body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 2em; }}
table {{ border-collapse: collapse; margin: 1em 0; }}
th, td {{ border: 1px solid #ccc; padding: 0.5em 1em; text-align: left; }}
th {{ background: #f5f5f5; }}
h1 {{ border-bottom: 2px solid #333; padding-bottom: 0.3em; }}
img {{ margin: 1em 0; }}
</style>
</head>
<body>
<h1>CppTLM Simulation Report</h1>
{topology_html}
<h2>Groups</h2>
<p>{", ".join(data.groups())}</p>
<h2>Latency Statistics</h2>
{groups_html}
<h2>Bottlenecks</h2>
{bottleneck_html}
</body>
</html>"""

        Path(output_path).write_text(html)
        return output_path