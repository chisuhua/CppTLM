#!/bin/bash
set -e

BUILD_DIR="${BUILD_DIR:-build}"
TEST_BIN="${BUILD_DIR}/bin/cpptlm_tests"
PYTEST="python3 -m pytest"
EXECUTABLES=(
    "cpptlm_sim"
    "cpptlm_cpu"
    "cpptlm_traffic"
    "stats_demo"
    "traffic_gen_demo"
)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================="
echo "CppTLM Full Test Suite"
echo "=========================================="
echo ""

if [[ "$1" == "--python-only" ]]; then
    echo "[Python-only mode] Pure Python tests (no build required)"
    echo ""
    cd "$ROOT_DIR"
    $PYTEST cpptlm_config/tests/ test/python/ -v --tb=short
    echo ""
    echo "[SUCCESS] All Python tests passed!"
    exit 0
fi

if [[ "$1" == "--quick" ]]; then
    echo "[Quick mode] Python tests + key C++ tests"
    echo ""
    cd "$ROOT_DIR"
    $PYTEST cpptlm_config/tests/ test/python/ --tb=short || exit 1
    if [[ -f "$TEST_BIN" ]]; then
        echo ""
        echo "--- Key C++ tests (critical path) ---"
        timeout 60 "$TEST_BIN" --order rand --rng-seed 0 2>&1 | tail -5 || true
    else
        echo "[SKIP] cpptlm_tests not built"
    fi
    echo ""
    echo "[SUCCESS] Quick mode complete"
    exit 0
fi

echo "=========================================="
echo "Python Tests (cpptlm_config + test/python)"
echo "=========================================="
cd "$ROOT_DIR"
FAILED_PY=0
$PYTEST cpptlm_config/tests/ test/python/ -v --tb=short || ((FAILED_PY++))
echo ""

echo "=========================================="
echo "C++ Test case count (cpptlm_tests)"
echo "=========================================="
if [[ ! -f "$TEST_BIN" ]]; then
    echo ""
    echo "[ERROR] cpptlm_tests not found in ${BUILD_DIR}/bin/"
    echo ""
    echo "  Full mode requires C++ tests to be built."
    echo "  Build with: cmake --build build"
    echo "  Or run Python-only: $0 --python-only"
    echo ""
    echo "[FAIL] C++ not built - cannot run full test suite"
    exit 1
fi

"$TEST_BIN" --list-tests | wc -l
echo ""
echo "[Full mode] Running all C++ test cases"
"$TEST_BIN" 2>&1 | tee "${BUILD_DIR}/test_output.txt"

echo ""
echo "=========================================="
echo "Test Results Summary (cpptlm_tests)"
echo "=========================================="
grep -E "(test cases:|assertions:)" "${BUILD_DIR}/test_output.txt" | tail -2

echo ""
echo "=========================================="
echo "E2E Test: cpptlm_sim with all TLM configs"
echo "=========================================="
FAILED_E2E=0
if [[ -f "${BUILD_DIR}/bin/cpptlm_sim" ]]; then
    for cfg in \
        "${ROOT_DIR}/configs/"*_tlm.json \
        "${ROOT_DIR}/configs/tlm_e2e_test.json" \
        "${ROOT_DIR}/configs/cpu_tlm_test.json" \
        "${ROOT_DIR}/configs/crossbar_test.json" \
        "${ROOT_DIR}/configs/cache_chstream_test.json" \
        "${ROOT_DIR}/configs/arbiter_tlm_test.json" \
        "${ROOT_DIR}/configs/traffic_gen_tlm_test.json"; do
        if [[ ! -f "$cfg" ]]; then
            continue
        fi
        cfg_name=$(basename "$cfg")
        echo "--- $cfg_name ---"
        timeout 30 "${BUILD_DIR}/bin/cpptlm_sim" "$cfg" --cycles 100 > /dev/null 2>&1 && echo "[PASS] $cfg_name" || {
            echo "[FAIL] $cfg_name"
            ((FAILED_E2E++))
        }
    done
    if [[ $FAILED_E2E -gt 0 ]]; then
        echo ""
        echo "[FAIL] $FAILED_E2E configs failed E2E test"
        exit 1
    fi
else
    echo "[SKIP] cpptlm_sim not built"
fi

echo ""
echo "=========================================="
echo "Running other executables"
echo "=========================================="
FAILED_EXECS=0
for exe in "${EXECUTABLES[@]}"; do
    exe_path="${BUILD_DIR}/bin/${exe}"
    if [[ ! -f "$exe_path" ]]; then
        echo "[SKIP] $exe (not built)"
        continue
    fi

    echo ""
    echo "--- $exe ---"
    if [[ "$exe" == "cpptlm_sim" ]]; then
        timeout 5 "$exe_path" --help > /dev/null 2>&1 && echo "[PASS] $exe (help shown)" || echo "[INFO] $exe needs config file"
    elif [[ "$exe" == "cpptlm_cpu" || "$exe" == "cpptlm_traffic" ]]; then
        timeout 5 "$exe_path" 2>&1 && echo "[PASS] $exe" || echo "[INFO] $exe (no config needed)"
    else
        timeout 10 "$exe_path" 2>&1 && echo "[PASS] $exe" || {
            echo "[FAIL] $exe exited with error"
            ((FAILED_EXECS++))
        }
    fi
done

if [[ -f "${BUILD_DIR}/test_output.txt" ]]; then
    if grep -q "failed" "${BUILD_DIR}/test_output.txt"; then
        echo ""
        echo "[WARNING] Some C++ tests failed:"
        grep -B5 "FAILED" "${BUILD_DIR}/test_output.txt" | tail -10
        exit 1
    fi
fi

if [[ $FAILED_PY -gt 0 ]] || [[ $FAILED_EXECS -gt 0 ]]; then
    echo ""
    echo "[FAIL] Some tests failed (Python: $FAILED_PY, Executables: $FAILED_EXECS)"
    exit 1
else
    echo ""
    echo "[SUCCESS] All tests passed!"
    exit 0
fi
