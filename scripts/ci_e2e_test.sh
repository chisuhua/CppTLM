#!/bin/bash
# ci_e2e_test.sh — cpptlm_sim CLI 端到端验证脚本
# 验证所有有效 TLM 配置文件可通过 cpptlm_sim 成功运行

set -euo pipefail

SIM_BIN="${CPPTLM_SIM:-./build/bin/cpptlm_sim}"
PROJECT_DIR="${CPPTLM_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$PROJECT_DIR"

if [ ! -x "$SIM_BIN" ]; then
    echo "ERROR: cpptlm_sim binary not found at $SIM_BIN"
    echo "Build with: cmake --build build -j\$(nproc)"
    exit 1
fi

echo "=========================================="
echo "cpptlm_sim CLI End-to-End Test Suite"
echo "Binary: $SIM_BIN"
echo "=========================================="

PASS=0
FAIL=0
FAILED_TESTS=()

# Cross-platform timeout function
# macOS doesn't have 'timeout' command by default
run_with_timeout() {
    local secs="$1"
    shift
    if command -v timeout >/dev/null 2>&1; then
        timeout "$secs" "$@"
    elif command -v gtimeout >/dev/null 2>&1; then
        gtimeout "$secs" "$@"
    else
        "$@"
    fi
}

run_sim_test() {
    local name="$1"
    local config="$2"
    local cycles="${3:-100}"

    echo -n "[TEST] $name ... "
    if run_with_timeout 60 "$SIM_BIN" "$config" --cycles "$cycles" > /dev/null 2>&1; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        FAIL=$((FAIL + 1))
        FAILED_TESTS+=("$name ($config)")
    fi
}

# 冒烟测试：--help 输出
echo -n "[TEST] --help 输出验证 ... "
if "$SIM_BIN" --help 2>&1 | grep -q "Usage:"; then
    echo "PASS"
    PASS=$((PASS + 1))
else
    echo "FAIL"
    FAIL=$((FAIL + 1))
    FAILED_TESTS+=("--help output verification")
fi

# 单端口 TLM 配置
run_sim_test "CacheTLM→MemoryTLM"          "configs/cache_chstream_test.json" 100
run_sim_test "CPUTLM→Cache→Memory"         "configs/cpu_tlm_test.json"        200

# 多端口 TLM 配置
run_sim_test "CrossbarTLM 4端口"           "configs/crossbar_test.json"       100
run_sim_test "ArbiterTLM2 双请求者"        "configs/arbiter_tlm_test.json"    100

# 发起者 TLM 配置
run_sim_test "TrafficGenTLM 双生成器"      "configs/traffic_gen_tlm_test.json" 200

# NoC 拓扑配置
run_sim_test "NIC→Router→NIC 最小拓扑"     "configs/test/nic_router_nic.json" 200
run_sim_test "mesh_2x2 拓扑"               "configs/mesh_2x2.json"            200
run_sim_test "mesh_4x4 大规模拓扑"         "configs/mesh_4x4.json"            100
run_sim_test "hierarchical_2x2 分层拓扑"   "configs/hierarchical_2x2.json"    100

echo ""
echo "=========================================="
echo "Results: $PASS passed, $FAIL failed"
echo "=========================================="

if [ "$FAIL" -gt 0 ]; then
    echo "FAILED TESTS:"
    for t in "${FAILED_TESTS[@]}"; do
        echo "  - $t"
    done
    exit 1
fi

echo "All E2E tests passed!"
exit 0
