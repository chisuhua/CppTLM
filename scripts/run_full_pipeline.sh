#!/bin/bash
# run_full_pipeline.sh — 完整的 生成 → 仿真 → 可视化 流程
#
# 用法:
#   ./scripts/run_full_pipeline.sh [topology] [size] [output_dir]
#
# 示例:
#   ./scripts/run_full_pipeline.sh mesh 4x4
#   ./scripts/run_full_pipeline.sh ring 8
#   ./scripts/run_full_pipeline.sh hierarchical 3 2
#
# 依赖:
#   - C++ 仿真器已编译 (build/bin/cpptlm_sim)
#   - Python: networkx, pydot (可选), watchdog (可选), dash (可选), plotly (可选)
#
# 作者: CppTLM Development Team
# 日期: 2026-04-22

set -e

TOPOLOGY_TYPE=${1:-mesh}
SIZE=${2:-4x4}
OUTPUT_DIR=${3:-output}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

mkdir -p "$OUTPUT_DIR"

echo "=============================================="
echo "CppTLM Full Pipeline"
echo "=============================================="
echo "Topology: $TOPOLOGY_TYPE"
echo "Size: $SIZE"
echo "Output: $OUTPUT_DIR"
echo "=============================================="

# 1. Python 前置：生成配置和布局
echo "[1/5] Generating topology..."
python3 "$SCRIPT_DIR/topology_generator.py" \
    --type "$TOPOLOGY_TYPE" \
    --size "$SIZE" \
    --output "$OUTPUT_DIR/topology.json" \
    --layout "$OUTPUT_DIR/topology_layout.json" \
    --dot "$OUTPUT_DIR/topology.dot" \
    2>&1 || {
    echo "Warning: topology_generator.py failed or not available"
    echo "Creating minimal config for testing..."
    cat > "$OUTPUT_DIR/topology.json" << 'EOF'
{
    "name": "test_topology",
    "modules": [
        {"name": "cpu", "type": "CPUSim"},
        {"name": "cache", "type": "CacheSim"},
        {"name": "mem", "type": "MemorySim"}
    ],
    "connections": [
        {"src": "cpu", "dst": "cache", "latency": 1},
        {"src": "cache", "dst": "mem", "latency": 10}
    ]
}
EOF
}

# 2. 检查是否有可用的仿真器
if [ -x "$PROJECT_DIR/build/bin/cpptlm_sim" ]; then
    echo "[2/5] Running simulation..."
    SIM_CMD="$PROJECT_DIR/build/bin/cpptlm_sim"
    "$SIM_CMD" \
        "$OUTPUT_DIR/topology.json" \
        --stream-stats \
        --stream-interval 5000 \
        --stream-path "$OUTPUT_DIR/stats_stream.jsonl" \
        --cycles 10000 \
        2>&1 || {
        echo "Warning: Simulation failed or --stream-stats not supported"
        echo "Creating mock stats for visualization testing..."
        cat > "$OUTPUT_DIR/stats_stream.jsonl" << 'EOF'
{"timestamp_ns":1234567890000000000,"simulation_cycle":10000,"group":"system.cpu","data":{"cycles":10000,"instructions":5000}}
{"timestamp_ns":1234567891000000000,"simulation_cycle":20000,"group":"system.cpu","data":{"cycles":20000,"instructions":10000}}
{"timestamp_ns":1234567892000000000,"simulation_cycle":30000,"group":"system.cpu","data":{"cycles":30000,"instructions":15000}}
EOF
    }
else
    echo "[2/5] Skipping simulation (cpptlm_sim not found)"
    echo "Creating mock stats for visualization testing..."
    cat > "$OUTPUT_DIR/stats_stream.jsonl" << 'EOF'
{"timestamp_ns":1234567890000000000,"simulation_cycle":10000,"group":"system.cpu","data":{"cycles":10000,"instructions":5000}}
{"timestamp_ns":1234567891000000000,"simulation_cycle":20000,"group":"system.cpu","data":{"cycles":20000,"instructions":10000}}
{"timestamp_ns":1234567892000000000,"simulation_cycle":30000,"group":"system.cpu","data":{"cycles":30000,"instructions":15000}}
EOF
fi

# 3. Python 后置：生成报告
echo "[3/5] Generating reports..."
python3 "$SCRIPT_DIR/stats_annotator.py" \
    --config "$OUTPUT_DIR/topology.json" \
    --stats "$OUTPUT_DIR/stats_stream.jsonl" \
    --layout "$OUTPUT_DIR/topology_layout.json" \
    --output "$OUTPUT_DIR/report.html" \
    --dot "$OUTPUT_DIR/topology_annotated.dot" \
    2>&1 || echo "Warning: stats_annotator.py failed"

# 4. (可选) 启动 stats watcher
if [ "${ENABLE_WATCHER:-0}" = "1" ]; then
    echo "[4/5] Starting stats watcher (ENABLE_WATCHER=1)..."
    python3 "$SCRIPT_DIR/stats_watcher.py" \
        --stream "$OUTPUT_DIR/stats_stream.jsonl" \
        --port 8050 &
    WATCHER_PID=$!
    sleep 2
    echo "Dashboard: http://localhost:8050 (PID: $WATCHER_PID)"
    echo "Press Ctrl+C to stop..."
    wait $WATCHER_PID
else
    echo "[4/5] Skipping stats watcher (set ENABLE_WATCHER=1 to enable)"
fi

echo "[5/5] Pipeline complete!"
echo ""
echo "=============================================="
echo "Output files in $OUTPUT_DIR:"
echo "=============================================="
ls -la "$OUTPUT_DIR" 2>/dev/null || echo "(no files)"
echo ""
echo "=============================================="
echo "Done!"