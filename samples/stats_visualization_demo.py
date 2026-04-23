#!/usr/bin/env python3
"""
stats_visualization_demo.py — 统计可视化工具使用示例

演示如何使用 stats_annotator.py：
1. 生成模拟的统计 JSON Lines 数据
2. 使用 stats_annotator 生成 HTML 报告
"""

import json
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))

try:
    from stats_annotator import StatsAnnotator
except ImportError as e:
    print(f"Error: Could not import stats_annotator: {e}")
    print("Make sure networkx and pydot are installed: pip install networkx pydot")
    sys.exit(1)


def generate_sample_stats(output_path, num_cycles=5):
    """生成模拟的流式统计 JSON Lines 数据"""
    modules = ['system.cpu', 'system.cache.l1', 'system.cache.l2', 'system.memory']

    with open(output_path, 'w') as f:
        for cycle in range(0, num_cycles * 1000, 1000):
            timestamp_ns = int(time.time() * 1e9)

            for module in modules:
                if 'cpu' in module:
                    data = {'cycles': cycle, 'instructions': cycle * 2, 'ipc': 1.5}
                elif 'l1' in module:
                    data = {'accesses': cycle * 3, 'hits': int(cycle * 3 * 0.85), 'misses': int(cycle * 3 * 0.15)}
                elif 'l2' in module:
                    data = {'accesses': int(cycle * 0.45), 'hits': int(cycle * 0.45 * 0.7), 'misses': int(cycle * 0.45 * 0.3)}
                else:
                    data = {'reads': int(cycle * 0.2), 'writes': int(cycle * 0.1), 'latency_avg': 100.0}

                record = {'timestamp_ns': timestamp_ns + cycle, 'simulation_cycle': cycle, 'group': module, 'data': data}
                f.write(json.dumps(record) + '\n')

    print(f"Generated {output_path}")


def demo_html_report():
    print("=" * 60)
    print("Demo: Generate HTML Report")
    print("=" * 60)

    with tempfile.TemporaryDirectory() as tmpdir:
        stats_path = os.path.join(tmpdir, 'stats.jsonl')
        generate_sample_stats(stats_path, num_cycles=5)

        config = {'name': 'demo_topology', 'modules': [{'name': 'cpu', 'type': 'Processor'}, {'name': 'l1_cache', 'type': 'Cache'}], 'connections': []}
        config_path = os.path.join(tmpdir, 'config.json')
        with open(config_path, 'w') as f:
            json.dump(config, f)

        annotator = StatsAnnotator(config_path, stats_path)
        report_path = os.path.join(tmpdir, 'report.html')
        # generate_html_report() returns HTML string, write to file manually
        html_content = annotator.generate_html_report()
        with open(report_path, 'w') as f:
            f.write(html_content)

        print(f"  Generated: {report_path}")
        print(f"  Size: {os.path.getsize(report_path)} bytes")

    print()


def main():
    print("\nCppTLM Stats Visualization Demo\n")

    demo_html_report()

    print("Usage:")
    print("  # Run simulation with streaming stats:")
    print("  ./build/bin/streaming_demo --output output/stats.jsonl --interval 1000")
    print()
    print("  # Generate HTML report:")
    print("  python3 scripts/stats_annotator.py --config config.json --stats output/stats.jsonl --output report.html")


if __name__ == '__main__':
    main()
