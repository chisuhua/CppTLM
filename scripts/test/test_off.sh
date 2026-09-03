#!/usr/bin/env bash
# CppTLM OFF-path regression suite: Python, Catch2, TLM CLI E2E, and executables.
#
# 迁移自 run_all_tests.sh + ci_e2e_test.sh。ci_e2e_test.sh 的全部有用行为
# （--help 冒烟 + 权威 TLM CLI E2E 配置 + PASS/FAIL 计数 + FAILED 列表）
# 已折叠进下方 run_e2e()，故 ci_e2e_test.sh 已被本脚本取代（见 run_all_tests.sh）。
#
# 用法:
#   BUILD_DIR=build ./scripts/test/test_off.sh             # 全量：Python + Catch2 + E2E + 可执行文件
#   BUILD_DIR=build ./scripts/test/test_off.sh --quick     # Python + 关键 Catch2
#   BUILD_DIR=build ./scripts/test/test_off.sh --python-only
#   BUILD_DIR=build ./scripts/test/test_off.sh --e2e       # 仅 TLM CLI E2E（ci_e2e_test.sh 等价物）
#   ./scripts/test/test_off.sh --build-dir build --quick   # 参数化 BUILD_DIR
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMMON_LIB="$SCRIPT_DIR/_lib/common.sh"
# shellcheck source=/dev/null
if [[ -f "$COMMON_LIB" ]]; then
    source "$COMMON_LIB"
else
    echo "WARNING: common.sh not found at $COMMON_LIB, using fallback" >&2
    TEST_PASS=0; TEST_FAIL=0; TEST_FAILURES=()
    pass_result(){ echo "[PASS] $1"; TEST_PASS=$((TEST_PASS+1)); }
    fail_result(){ echo "[FAIL] $1"; TEST_FAIL=$((TEST_FAIL+1)); TEST_FAILURES+=("$1"); }
    print_result_summary(){ echo "Results: ${TEST_PASS} passed, ${TEST_FAIL} failed"; if ((TEST_FAIL>0)); then printf 'FAILED TESTS:\n'; printf '  - %s\n' "${TEST_FAILURES[@]}"; return 1; fi; }
    run_with_timeout(){ local s="$1"; shift; if command -v timeout >/dev/null 2>&1; then timeout --preserve-status "$s" "$@"; elif command -v gtimeout >/dev/null 2>&1; then gtimeout --preserve-status "$s" "$@"; else "$@"; fi; }
fi

BUILD_DIR="${BUILD_DIR:-build}"
QUICK=0
PYTHON_ONLY=0
E2E=0
BUILD_DIR_SET=0
if [[ -n "${BUILD_DIR+x}" && "$BUILD_DIR" != "build" ]]; then BUILD_DIR_SET=1; fi

usage() {
    cat <<'EOF'
用法: BUILD_DIR=build ./scripts/test/test_off.sh [选项]

选项:
  --quick              Python + 关键 OFF Catch2 测试
  --python-only        仅运行 Python 测试（不要求构建产物）
  --e2e                运行权威 TLM CLI E2E 配置（ci_e2e_test.sh 等价物）
  --build-dir <dir>    构建目录（默认 build，支持相对/绝对路径）
  --help               显示帮助

环境变量: BUILD_DIR（默认 build）、CPPTLM_SIM（自定义 cpptlm_sim 路径）
EOF
}

while (($#)); do
    case "$1" in
        --quick) QUICK=1; shift ;;
        --python-only) PYTHON_ONLY=1; shift ;;
        --e2e) E2E=1; shift ;;
        --build-dir)
            [[ $# -ge 2 ]] || { echo "ERROR: --build-dir 需要参数" >&2; exit 2; }
            BUILD_DIR="$2"; BUILD_DIR_SET=1; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        --) shift; break ;;
        -*) echo "ERROR: 不支持的参数: $1" >&2; usage >&2; exit 2 ;;
        *)
            # 允许位置参数作为 BUILD_DIR（兼容旧调用）
            if [[ $BUILD_DIR_SET -eq 0 && -d "$1" ]]; then
                BUILD_DIR="$1"; BUILD_DIR_SET=1; shift
            else
                echo "ERROR: 不支持的位置参数: $1" >&2; usage >&2; exit 2
            fi
            ;;
    esac
done

if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$ROOT_DIR/$BUILD_DIR"
fi
TEST_BIN="$BUILD_DIR/bin/cpptlm_tests"
SIM_BIN="${CPPTLM_SIM:-$BUILD_DIR/bin/cpptlm_sim}"

cd "$ROOT_DIR"

run_python() {
    python3 -m pytest cpptlm_config/tests/ test/python/ --tb=short
}

if [[ "$PYTHON_ONLY" == 1 ]]; then
    if run_python; then pass_result "OFF-PYTHON"; else fail_result "OFF-PYTHON"; fi
    print_result_summary
    exit $?
fi

if [[ ! -x "$TEST_BIN" ]]; then
    echo "ERROR: OFF Catch2 测试二进制缺失/不可执行: $TEST_BIN" >&2
    echo "  构建: cmake -S . -B $(basename "$BUILD_DIR") && cmake --build $(basename "$BUILD_DIR")  或运行 $0 --python-only" >&2
    exit 1
fi

# 1. Python 测试（cpptlm_config + test/python；--e2e 模式跳过）
if [[ "$E2E" == 0 ]]; then
    if run_python; then pass_result "OFF-PYTHON"; else fail_result "OFF-PYTHON"; fi
fi

# 2. Catch2（quick = 关键路径，否则全量；--e2e 模式跳过）
if [[ "$E2E" == 0 ]]; then
    if [[ "$QUICK" == 1 ]]; then
    if run_with_timeout 120 "$TEST_BIN" "[phase6]" -r compact; then
        pass_result "OFF-CATCH2-QUICK"
    else
        fail_result "OFF-CATCH2-QUICK"
    fi
else
    # tee 同时保留日志，pipefail 保证失败能被捕获
    if run_with_timeout 600 "$TEST_BIN" -r compact 2>&1 | tee "$BUILD_DIR/test_output.txt"; then
        # 额外检查：即使 tee 成功，也要确认没有隐藏的失败（grep -q failed 需安全处理）
        if [[ -f "$BUILD_DIR/test_output.txt" ]] && grep -q "failed" "$BUILD_DIR/test_output.txt" 2>/dev/null; then
            fail_result "OFF-CATCH2-FULL"
        else
            pass_result "OFF-CATCH2-FULL"
        fi
    else
        fail_result "OFF-CATCH2-FULL"
    fi
    fi
fi

# 3. TLM CLI E2E（ci_e2e_test.sh 权威配置 + apu_soc_v1 SoC 配置）
#    配置路径全部相对于仓库根，缺失即报 FAIL（避免静默跳过真实回归）。
run_e2e() {
    if [[ ! -x "$SIM_BIN" ]]; then
        if [[ "$E2E" == 1 ]]; then
            fail_result "OFF-E2E-BINARY (cpptlm_sim 缺失/不可执行)"
        else
            echo "[SKIP] OFF-E2E（cpptlm_sim 未构建: $SIM_BIN）"
        fi
        return 1
    fi
    local name config cycles
    local -a cases=(
        "CacheTLM-MemoryTLM|configs/cache_chstream_test.json|100"
        "CPUTLM-Cache-Memory|configs/cpu_tlm_test.json|200"
        "CrossbarTLM-4port|configs/crossbar_test.json|100"
        "ArbiterTLM2-dual|configs/arbiter_tlm_test.json|100"
        "ArbiterTLM4|configs/arbiter_tlm4_test.json|100"
        "TrafficGenTLM-dual|configs/traffic_gen_tlm_test.json|200"
        "NIC-Router-NIC|configs/test/nic_router_nic.json|200"
        "mesh-2x2|configs/mesh_2x2_tlm.json|200"
        "mesh-4x4|configs/mesh_4x4.json|100"
        "mesh-4x4-tlm|configs/mesh_4x4_tlm.json|100"
        "hierarchical-2x2|configs/hierarchical_2x2_tlm.json|100"
        "ring-8-tlm|configs/ring_8_tlm.json|100"
        "link-tlm-chain|configs/link_tlm_chain.json|100"
        "tlm-e2e|configs/tlm_e2e_test.json|100"
        "gpu-2gpc-2tpc-2cu|configs/gpu_2gpc_2tpc_2cu.json|100"
        "apu-soc-v1|configs/apu_soc_v1.json|100"
    )
    # --help 冒烟（沿用 ci_e2e_test.sh）
    if "$SIM_BIN" --help 2>&1 | grep -q 'Usage:'; then
        pass_result "OFF-E2E-HELP"
    else
        fail_result "OFF-E2E-HELP"
    fi
    local ok=0
    for entry in "${cases[@]}"; do
        IFS='|' read -r name config cycles <<< "$entry"
        if [[ ! -f "$ROOT_DIR/$config" ]]; then
            echo "ERROR: E2E 配置缺失（不应发生）: $ROOT_DIR/$config" >&2
            fail_result "OFF-E2E-$name"
            ok=1
            continue
        fi
        if run_with_timeout 60 "$SIM_BIN" "$ROOT_DIR/$config" --cycles "$cycles" >/dev/null 2>&1; then
            pass_result "OFF-E2E-$name"
        else
            fail_result "OFF-E2E-$name"
            ok=1
        fi
    done
    return "$ok"
}

# 默认全量跑 E2E；--quick 跳过 E2E；--e2e 仅跑 E2E。
if [[ "$E2E" == 1 || "$QUICK" == 0 ]]; then
    run_e2e || true
fi

# 4. 可执行文件（可选产物：缺失仅 SKIP，不视为失败；--e2e 模式下跳过）
if [[ "$E2E" == 0 ]]; then
    for exe in stats_demo traffic_gen_demo; do
        path="$BUILD_DIR/bin/$exe"
        if [[ -x "$path" ]]; then
            if run_with_timeout 30 "$path" >/dev/null 2>&1; then
                pass_result "OFF-EXE-$exe"
            else
                fail_result "OFF-EXE-$exe"
            fi
        else
            echo "[SKIP] OFF-EXE-$exe（可选产物未构建，跳过）"
        fi
    done
fi

print_result_summary
