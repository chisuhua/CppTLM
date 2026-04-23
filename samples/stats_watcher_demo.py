#!/usr/bin/env python3
"""
stats_watcher_demo.py — stats_watcher 使用示例

演示如何使用 stats_watcher.py CLI 工具：
1. 生成模拟统计流文件
2. 使用 stats_watcher 控制台模式监控
"""

import json
import os
import sys
import subprocess
import tempfile
import time


def generate_sample_stats(output_path, num_records=10):
    modules = ['system.cpu', 'system.cache.l1', 'system.memory']

    with open(output_path, 'w') as f:
        for i in range(num_records):
            cycle = i * 1000
            timestamp_ns = int(time.time() * 1e9)

            for module in modules:
                if 'cpu' in module:
                    data = {'cycles': cycle, 'instructions': cycle * 2, 'ipc': 1.5}
                elif 'l1' in module:
                    data = {'accesses': cycle * 3, 'hits': int(cycle * 3 * 0.85)}
                else:
                    data = {'reads': int(cycle * 0.2), 'latency_avg': 100.0}

                record = {'timestamp_ns': timestamp_ns, 'simulation_cycle': cycle, 'group': module, 'data': data}
                f.write(json.dumps(record) + '\n')

            f.flush()
            time.sleep(0.1)

    return output_path


def demo_stats_watcher():
    print("=" * 60)
    print("Demo: stats_watcher Console Mode")
    print("=" * 60)

    with tempfile.TemporaryDirectory() as tmpdir:
        stats_path = os.path.join(tmpdir, 'stats_stream.jsonl')
        config_path = os.path.join(tmpdir, 'config.json')

        config = {'name': 'demo', 'modules': [{'name': 'cpu', 'type': 'Processor'}], 'connections': []}
        with open(config_path, 'w') as f:
            json.dump(config, f)

        print(f"  Generating sample stats to: {stats_path}")
        generate_sample_stats(stats_path, num_records=5)

        print(f"\n  Running stats_watcher in console mode...")
        print(f"  Command: python3 scripts/stats_watcher.py --stream {stats_path} --no-gui --interval 0.5")
        print()

        result = subprocess.run(
            [sys.executable, 'scripts/stats_watcher.py', '--stream', stats_path, '--no-gui', '--interval', '0.5'],
            cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            capture_output=True,
            text=True,
            timeout=5
        )

        if result.returncode == 0:
            print(result.stdout)
        else:
            print("  Note: stats_watcher requires watchdog/dash for full functionality")
            print(f"  Install with: pip install watchdog dash plotly")

    print()


def main():
    print("\nCppTLM Stats Watcher Demo\n")

    demo_stats_watcher()

    print("Usage:")
    print("  # Console mode (no GUI):")
    print("  python3 scripts/stats_watcher.py --stream output/stats.jsonl --no-gui")
    print()
    print("  # Web dashboard mode:")
    print("  python3 scripts/stats_watcher.py --stream output/stats.jsonl --port 8050")
    print()
    print("  # Combined with streaming simulation:")
    print("  ./build/bin/streaming_demo --output output/stats.jsonl &")
    print("  python3 scripts/stats_watcher.py --stream output/stats.jsonl --no-gui")


if __name__ == '__main__':
    main()
