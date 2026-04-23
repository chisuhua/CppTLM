#!/usr/bin/env python3
"""
stats_annotator.py — CppTLM 性能数据标注工具

功能:
  - 从 JSON/JSONL 文件解析性能统计
  - 生成带性能标注的 HTML 报告
  - 生成带性能标注的 DOT 文件

用法:
    python3 stats_annotator.py \
        --config configs/mesh.json \
        --stats output/stats_stream.jsonl \
        --layout configs/mesh_layout.json \
        --output docs/report.html

@author CppTLM Development Team
@date 2026-04-22
"""

import argparse
import json
import sys
from typing import Dict, List, Optional, Any
from collections import defaultdict


class StatsAnnotator:
    """性能数据标注器"""

    def __init__(self, config_path: str, stats_path: str, layout_path: str = None):
        self.config = self._load_json(config_path)
        self.stats = self._load_stats(stats_path)
        self.layout = self._load_json(layout_path) if layout_path else {}

    def _load_json(self, path: str) -> Dict:
        """加载 JSON 文件"""
        try:
            with open(path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"Warning: File not found: {path}")
            return {}
        except json.JSONDecodeError as e:
            print(f"Warning: Invalid JSON in {path}: {e}")
            return {}

    def _load_stats(self, path: str) -> Dict:
        """
        加载统计数据
        支持 JSON（单次仿真）或 JSON Lines（流式仿真）
        """
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read().strip()
                if not content:
                    return {}

                # 尝试 JSON Lines 格式
                if '\n' in content:
                    # JSON Lines: 取最后一个快照
                    lines = content.split('\n')
                    last_line = None
                    for line in lines:
                        line = line.strip()
                        if line:
                            last_line = line
                    if last_line:
                        record = json.loads(last_line)
                        return self._extract_final_stats(record)
                else:
                    # 标准 JSON
                    data = json.loads(content)
                    return self._extract_final_stats(data)

        except FileNotFoundError:
            print(f"Warning: Stats file not found: {path}")
            return {}
        except json.JSONDecodeError as e:
            print(f"Warning: Invalid JSON in {path}: {e}")
            return {}

        return {}

    def _extract_final_stats(self, data: Dict) -> Dict:
        """从记录中提取最终统计"""
        if "data" in data:
            return data["data"]
        return data

    def _get_module_stats(self, module_name: str) -> Dict:
        """获取模块的统计"""
        # 尝试多种路径格式
        possible_keys = [
            module_name,
            f"system.{module_name}",
            f"stats.{module_name}",
        ]

        for key in possible_keys:
            if key in self.stats:
                return self.stats[key]
            # 模糊匹配
            for k, v in self.stats.items():
                if module_name in k:
                    return v

        return {}

    def generate_html_report(self) -> str:
        """生成 HTML 报告"""
        modules = self.config.get("modules", [])
        connections = self.config.get("connections", [])

        # 收集所有指标
        all_metrics = []
        for module in modules:
            stats = self._get_module_stats(module["name"])
            for key, value in stats.items():
                all_metrics.append({
                    "module": module["name"],
                    "type": module.get("type", "Unknown"),
                    "metric": key,
                    "value": value
                })

        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CppTLM Performance Report</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body {{
            font-family: Arial, sans-serif;
            margin: 20px;
            background-color: #f5f5f5;
        }}
        h1 {{
            color: #333;
            border-bottom: 2px solid #2196F3;
            padding-bottom: 10px;
        }}
        h2 {{
            color: #555;
            margin-top: 30px;
        }}
        .summary {{
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }}
        table {{
            border-collapse: collapse;
            width: 100%;
            background: white;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            border-radius: 8px;
            overflow: hidden;
        }}
        th {{
            background-color: #2196F3;
            color: white;
            padding: 12px;
            text-align: left;
        }}
        td {{
            padding: 10px 12px;
            border-bottom: 1px solid #eee;
        }}
        tr:hover {{
            background-color: #f5f5f5;
        }}
        .metric-card {{
            display: inline-block;
            background: white;
            padding: 15px;
            margin: 5px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            min-width: 120px;
        }}
        .metric-value {{
            font-size: 24px;
            font-weight: bold;
            color: #2196F3;
        }}
        .metric-label {{
            font-size: 12px;
            color: #666;
        }}
        .module-section {{
            background: white;
            padding: 20px;
            margin: 20px 0;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        .module-header {{
            font-size: 18px;
            font-weight: bold;
            color: #333;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid #eee;
        }}
    </style>
</head>
<body>
    <h1>CppTLM Performance Report</h1>

    <div class="summary">
        <h2>Simulation Summary</h2>
        <p><strong>Topology:</strong> {self.config.get("name", "Unknown")}</p>
        <p><strong>Modules:</strong> {len(modules)}</p>
        <p><strong>Connections:</strong> {len(connections)}</p>
    </div>

    <h2>Module Performance</h2>
"""

        # 按模块分组显示
        for module in modules:
            module_name = module["name"]
            module_type = module.get("type", "Unknown")
            stats = self._get_module_stats(module_name)

            if stats:
                html += f"""
    <div class="module-section">
        <div class="module-header">{module_name} ({module_type})</div>
"""

                # 数值型指标显示为卡片
                numeric_stats = {k: v for k, v in stats.items()
                               if isinstance(v, (int, float))}
                if numeric_stats:
                    html += '        <div style="display:flex; flex-wrap:wrap;">\n'
                    for metric, value in sorted(numeric_stats.items())[:8]:
                        value_str = f"{value:.4f}" if isinstance(value, float) else str(value)
                        html += f'''
            <div class="metric-card">
                <div class="metric-value">{value_str}</div>
                <div class="metric-label">{metric}</div>
            </div>
'''
                    html += '        </div>\n'

                # 非数值型显示为表格
                non_numeric = {k: v for k, v in stats.items()
                              if not isinstance(v, (int, float))}
                if non_numeric:
                    html += f'''
        <table>
            <tr><th>Metric</th><th>Value</th></tr>
'''
                    for metric, value in sorted(non_numeric.items()):
                        value_str = json.dumps(value) if isinstance(value, dict) else str(value)
                        html += f'''
            <tr><td>{metric}</td><td>{value_str}</td></tr>
'''
                    html += '        </table>\n'

                html += '    </div>\n'

        # 连接统计
        html += """
    <h2>Connection Latency</h2>
    <table>
        <tr><th>Source</th><th>Destination</th><th>Latency</th><th>Bandwidth</th></tr>
"""

        for conn in connections:
            src = conn.get("src", "")
            dst = conn.get("dst", "")
            latency = conn.get("latency", "-")
            bandwidth = conn.get("bandwidth", "-")
            html += f"""
        <tr><td>{src}</td><td>{dst}</td><td>{latency}</td><td>{bandwidth}</td></tr>
"""

        html += """
    </table>

</body>
</html>
"""
        return html

    def generate_annotated_dot(self) -> str:
        """生成带性能标注的 DOT 文件"""
        modules = self.config.get("modules", [])
        connections = self.config.get("connections", [])

        lines = [
            'digraph simulation {',
            '    rankdir=LR;',
            '    node [shape=box, style="rounded,filled"];',
            '    edge [color="#333333"];',
            ''
        ]

        # 添加节点
        for module in modules:
            name = module["name"]
            module_type = module.get("type", "Unknown")
            stats = self._get_module_stats(name)

            attrs = [f'label="{name}\\n({module_type})"']

            # 添加性能标注
            if stats:
                if "throughput" in stats:
                    attrs.append(f'xlabel="BW: {stats["throughput"]} GB/s"')
                if "latency" in stats:
                    latency_val = stats["latency"]
                    if isinstance(latency_val, dict):
                        if "avg" in latency_val:
                            attrs.append(f'tooltip="lat: {latency_val["avg"]} cy"')
                    elif isinstance(latency_val, (int, float)):
                        attrs.append(f'tooltip="lat: {latency_val} cy"')

            # 获取布局位置
            if name in self.layout.get("nodes", {}):
                pos = self.layout["nodes"][name]
                attrs.append(f'pos="{pos["x"]},{pos["y"]}"')

            node_attrs = ", ".join(attrs)
            lines.append(f'    {name} [{node_attrs}];')

        lines.append('')

        # 添加边
        for conn in connections:
            src = conn.get("src", "")
            dst = conn.get("dst", "")
            latency = conn.get("latency", 1)
            bandwidth = conn.get("bandwidth", 100)

            lines.append(f'    {src} -> {dst} [label="{latency}cy\\n{bandwidth}GB/s"];')

        lines.append('}')
        return '\n'.join(lines)

    def export_html(self, output_path: str):
        """导出 HTML 报告"""
        html = self.generate_html_report()
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html)
        print(f"[OK] HTML report saved to {output_path}")

    def export_dot(self, output_path: str):
        """导出带标注的 DOT 文件"""
        dot = self.generate_annotated_dot()
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(dot)
        print(f"[OK] Annotated DOT saved to {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="CppTLM Stats Annotator - 生成带性能标注的报告"
    )
    parser.add_argument("--config", "-c", required=True,
                       help="仿真配置文件 (JSON)")
    parser.add_argument("--stats", "-s", required=True,
                       help="统计输出文件 (JSON or JSONL)")
    parser.add_argument("--layout", "-l", default=None,
                       help="布局文件 (JSON, 可选)")
    parser.add_argument("--output", "-o", required=True,
                       help="输出 HTML 报告")
    parser.add_argument("--dot", "-d", default=None,
                       help="输出 DOT 文件 (可选)")

    args = parser.parse_args()

    # 创建标注器
    annotator = StatsAnnotator(args.config, args.stats, args.layout)

    # 生成报告
    annotator.export_html(args.output)

    if args.dot:
        annotator.export_dot(args.dot)


if __name__ == "__main__":
    main()