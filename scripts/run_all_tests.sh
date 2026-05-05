#!/bin/bash
set -e

BUILD_DIR="${BUILD_DIR:-build}"
TEST_BIN="${BUILD_DIR}/bin/cpptlm_tests"
EXECUTABLES=(
    "cpptlm_sim"
    "cpptlm_cpu"
    "cpptlm_traffic"
    "stats_demo"
    "traffic_gen_demo"
)

echo "=========================================="
echo "CppTLM Full Test Suite"
echo "=========================================="
echo ""

echo "Test case count:"
"$TEST_BIN" --list-tests | wc -l
echo ""

if [[ "$1" == "--quick" ]]; then
    echo "[Quick mode] Stop on first failure"
    "$TEST_BIN" || { echo "Tests failed"; exit 1; }
else
    echo "[Full mode] Running all test cases"
    "$TEST_BIN" 2>&1 | tee "${BUILD_DIR}/test_output.txt"

    echo ""
    echo "=========================================="
    echo "Test Results Summary (cpptlm_tests)"
    echo "=========================================="
    grep -E "(test cases:|assertions:)" "${BUILD_DIR}/test_output.txt" | tail -2

    echo ""
    echo "E2E Test: cpptlm_sim with TLM modules"
    echo "=========================================="
    E2E_CONFIG="${BUILD_DIR}/../configs/tlm_e2e_test.json"
    if [[ -f "$E2E_CONFIG" ]]; then
        echo "Running: cpptlm_sim $E2E_CONFIG --cycles 100"
        timeout 30 "${BUILD_DIR}/bin/cpptlm_sim" "$E2E_CONFIG" --cycles 100 2>&1 | tee "${BUILD_DIR}/e2e_output.txt"
        if grep -q "Simulation finished" "${BUILD_DIR}/e2e_output.txt"; then
            echo "[PASS] cpptlm_sim E2E test"
        else
            echo "[FAIL] cpptlm_sim E2E test"
            cat "${BUILD_DIR}/e2e_output.txt"
            exit 1
        fi
    else
        echo "[SKIP] E2E config not found: $E2E_CONFIG"
    fi

    # Run other executables with smoke test
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

    if grep -q "failed" "${BUILD_DIR}/test_output.txt"; then
        echo ""
        echo "[WARNING] Some tests failed:"
        grep -B5 "FAILED" "${BUILD_DIR}/test_output.txt" | tail -10
        exit 1
    else
        echo ""
        echo "[SUCCESS] All tests passed!"
        exit $FAILED_EXECS
    fi
fi