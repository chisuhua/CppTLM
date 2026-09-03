#!/usr/bin/env bash
# CppTLM 测试共享辅助函数：统一超时执行与 PASS/FAIL 统计。

set -euo pipefail

TEST_PASS=0
TEST_FAIL=0
TEST_FAILURES=()

run_with_timeout() {
    local seconds="$1"
    shift
    if [[ ! "$seconds" =~ ^[0-9]+([.][0-9]+)?$ ]] || [[ "$seconds" == 0 || "$seconds" == 0.* ]]; then
        echo "ERROR: timeout 必须是正数: $seconds" >&2
        return 2
    fi
    if command -v timeout >/dev/null 2>&1; then
        timeout --preserve-status "$seconds" "$@"
    elif command -v gtimeout >/dev/null 2>&1; then
        gtimeout --preserve-status "$seconds" "$@"
    else
        echo "WARNING: 未找到 timeout/gtimeout，直接执行: $*" >&2
        "$@"
    fi
}

pass_result() {
    local name="$1"
    echo "[PASS] $name"
    TEST_PASS=$((TEST_PASS + 1))
}

fail_result() {
    local name="$1"
    echo "[FAIL] $name"
    TEST_FAIL=$((TEST_FAIL + 1))
    TEST_FAILURES+=("$name")
}

run_checked() {
    local name="$1"
    shift
    if "$@"; then
        pass_result "$name"
        return 0
    fi
    fail_result "$name"
    return 1
}

print_result_summary() {
    echo "Results: ${TEST_PASS} passed, ${TEST_FAIL} failed"
    if (( TEST_FAIL > 0 )); then
        printf 'FAILED TESTS:\n'
        printf '  - %s\n' "${TEST_FAILURES[@]}"
        return 1
    fi
}
