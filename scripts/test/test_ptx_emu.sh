#!/usr/bin/env bash
# CppTLM PTX-EMU 集成模式测试编排（H2D-only PTXIR image DMA 范畴）
#
# 实现已批准的 H2D-only PTX-EMU 测试目录 PTX-EMU-001..012：
#   001   libptxemu_core.a 非空
#   002   libcpptlm_core.a 非空
#   003-006  四个 PTX-EMU .so 非空（libptxemu_device / libptxsim / libptx_parser / libcudart）
#   007   image-loading ABI 符号由构建库实际导出（nm 验证，非假设）
#   008   libcudart 不导出 cpptlm_bridge
#   009   ptxir embed/extract 往返（临时文件 + 清理）
#   010   ptxir pass-through（extract→re-embed→extract 字节一致）
#   011   现有 [sdma][h2d] Catch2 测试
#   012   未来 [sdma][h2d][ptxir] 测试（尚未落地 → 明确 SKIP，不静默跳过）
# 补充检查（不属于 001-012 核心目录，但仍执行）：
#   PTX-EMU-CUTE   CuTe 二进制仅 test -x + 依赖检查，绝不执行
#   PTX-EMU-DLOPEN emulator dlopen 二进制（若存在则运行）
#   PTX-EMU-ON     ON 模式完整 Catch2/TLM/SoC 回归（配置路径真实存在）
#
# 明确排除：kernel 执行（ptxemu_image_execute 不被调用）与 CuTe 执行。
# 用法:
#   BUILD_DIR=build-on ./scripts/test/test_ptx_emu.sh            # 完整目录
#   BUILD_DIR=build-on ./scripts/test/test_ptx_emu.sh --quick    # 快速子集
#   BUILD_DIR=build-on ./scripts/test/test_ptx_emu.sh --e2e      # 仅 ON 模式回归
#   BUILD_DIR=build-on ./scripts/test/test_ptx_emu.sh --python-only
#   ./scripts/test/test_ptx_emu.sh --build-dir build-on --quick
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

# PTX-EMU 默认构建目录为 build-on（与 scripts/build/build_ptx_emu.sh 一致）
BUILD_DIR="${BUILD_DIR:-build-on}"
QUICK=0
PYTHON_ONLY=0
E2E=0
BUILD_DIR_SET=0
if [[ -n "${BUILD_DIR+x}" && "$BUILD_DIR" != "build-on" ]]; then BUILD_DIR_SET=1; fi

usage() {
    cat <<'EOF'
用法: BUILD_DIR=build-on ./scripts/test/test_ptx_emu.sh [选项]

选项:
  --quick              PTX-EMU 快速子集（静态库/so/ABI/ptxir 往返 + [sdma][h2d]）
  --python-only        仅运行 Python 测试（不要求构建产物）
  --e2e                仅运行 ON 模式 Catch2/TLM/SoC 回归
  --build-dir <dir>    构建目录（默认 build-on，支持相对/绝对路径）
  --help               显示帮助

环境变量: BUILD_DIR（默认 build-on）、CPPTLM_SIM（自定义 cpptlm_sim 路径）

范围: H2D-only PTXIR image DMA；绝不执行 kernel / CuTe 程序。
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
LIB_DIR="$BUILD_DIR/lib"

# ptxir 往返输入 fixture（external/PTX-EMU 为只读 submodule，仅引用不修改）
PTXIR_CUBIN="$ROOT_DIR/external/PTX-EMU/tests/e2e/path_1B_ptxir_fatbinary/path_1B_standalone.cubin"
PTXIR_PTX="$ROOT_DIR/external/PTX-EMU/tests/e2e/path_1C_driver_api/vec_add.ptx"
PTXIR_KERNEL="vec_add"
PTXIR_EMBED="$BUILD_DIR/bin/ptxir_embed"
PTXIR_EXTRACT="$BUILD_DIR/bin/ptxir_extract"

cd "$ROOT_DIR"

if [[ "$PYTHON_ONLY" == 1 ]]; then
    if python3 -m pytest cpptlm_config/tests/ test/python/ --tb=short; then
        pass_result "PTX-EMU-PYTHON"
    else
        fail_result "PTX-EMU-PYTHON"
    fi
    print_result_summary
    exit $?
fi

# ============================================================
# PTX-EMU-001/002: 静态库非空（必产物，缺失即 FAIL）
# ============================================================
check_nonempty_lib() {
    local id="$1" path="$2"
    if [[ -f "$path" && -s "$path" ]]; then
        pass_result "$id ($(basename "$path"), $(stat -Lc%s "$path" 2>/dev/null || echo '?') bytes)"
    else
        fail_result "$id ($(basename "$path") 缺失或为空)"
    fi
}
check_nonempty_lib "PTX-EMU-001" "$LIB_DIR/libptxemu_core.a"
check_nonempty_lib "PTX-EMU-002" "$LIB_DIR/libcpptlm_core.a"

# ============================================================
# PTX-EMU-003..006: 四个 PTX-EMU .so 非空
# ============================================================
declare -a PTXEMU_SO_IDS=(
    "PTX-EMU-003|libptxemu_device.so"
    "PTX-EMU-004|libptxsim.so"
    "PTX-EMU-005|libptx_parser.so"
    "PTX-EMU-006|libcudart.so"
)
for entry in "${PTXEMU_SO_IDS[@]}"; do
    IFS='|' read -r so_id so <<< "$entry"
    path="$LIB_DIR/$so"
    if [[ -f "$path" && -s "$path" ]]; then
        pass_result "$so_id ($so, $(stat -Lc%s "$path" 2>/dev/null || echo '?') bytes)"
    else
        fail_result "$so_id ($so 缺失或为空)"
    fi
done

# ============================================================
# PTX-EMU-007: image-loading ABI 符号实际导出（nm 验证）
# ============================================================
# 已实测（本机 build-on/lib/libptxemu_device.so 动态符号表）：
#   ptxemu_image_load / unload / execute / execute_named
#   ptxemu_image_kernel_count / kernel_name / kernel_name_at / module_version
PTXEMU_ABI_SYMS=(
    ptxemu_image_load
    ptxemu_image_unload
    ptxemu_image_execute
    ptxemu_image_execute_named
    ptxemu_image_kernel_count
    ptxemu_image_kernel_name
    ptxemu_image_kernel_name_at
    ptxemu_module_version
)
check_abi_syms() {
    local lib="$1" missing=0
    shift
    for sym in "$@"; do
        # nm -D --defined-only 查动态导出表；|| true 防止 set -e 中断
        if ! nm -D --defined-only "$lib" 2>/dev/null | grep -qE "(^| )${sym}($|@)"; then
            echo "  [MISSING] $sym" >&2
            missing=1
        fi
    done
    return "$missing"
}
if [[ -f "$LIB_DIR/libptxemu_device.so" ]]; then
    if check_abi_syms "$LIB_DIR/libptxemu_device.so" "${PTXEMU_ABI_SYMS[@]}"; then
        pass_result "PTX-EMU-007 (image ABI: ${#PTXEMU_ABI_SYMS[@]} symbols @ libptxemu_device.so)"
    else
        fail_result "PTX-EMU-007 (image ABI 符号缺失，见上)"
    fi
else
    fail_result "PTX-EMU-007 (libptxemu_device.so 缺失，无法验证 ABI)"
fi

# ============================================================
# PTX-EMU-008: libcudart 不导出 cpptlm_bridge
# ============================================================
if [[ -f "$LIB_DIR/libcudart.so" ]]; then
    # grep -c 0 匹配仍返回 0 退出码；|| true 防止 pipefail/set -e 误杀
    bridge_count=$(nm -D --defined-only "$LIB_DIR/libcudart.so" 2>/dev/null | grep -c 'cpptlm_bridge' || true)
    if [[ "$bridge_count" == "0" ]]; then
        pass_result "PTX-EMU-008 (libcudart 无 cpptlm_bridge 泄漏)"
    else
        fail_result "PTX-EMU-008 (libcudart 导出 cpptlm_bridge x$bridge_count)"
    fi
else
    fail_result "PTX-EMU-008 (libcudart.so 缺失)"
fi

# ============================================================
# PTX-EMU-009/010: ptxir embed/extract 往返 + pass-through
# ============================================================
run_ptxir_roundtrip() {
    [[ -f "$PTXIR_CUBIN" && -f "$PTXIR_PTX" ]] || {
        echo "  [SKIP] PTX-EMU fixture 缺失 (cubin=$PTXIR_CUBIN, ptx=$PTXIR_PTX)" >&2
        return 2
    }
    [[ -x "$PTXIR_EMBED" && -x "$PTXIR_EXTRACT" ]] || {
        echo "  [SKIP] ptxir_embed/extract 工具未构建" >&2
        return 2
    }
    tmpdir="$(mktemp -d)"
    local rc=0
    if ! "$PTXIR_EMBED" --in-cubin "$PTXIR_CUBIN" --in-ptx "$PTXIR_PTX" \
            --kernel-name "$PTXIR_KERNEL" --out "$tmpdir/roundtrip.cubin" >/dev/null 2>&1; then
        rc=1
    fi
    if [[ $rc -eq 0 ]] && [[ ! -s "$tmpdir/roundtrip.cubin" ]]; then
        rc=1
    fi
    if [[ $rc -eq 0 ]] && ! "$PTXIR_EXTRACT" --in "$tmpdir/roundtrip.cubin" \
            --out-ptxir "$tmpdir/roundtrip.ptxir" >/dev/null 2>&1; then
        rc=1
    fi
    if [[ $rc -eq 0 ]] && [[ ! -s "$tmpdir/roundtrip.ptxir" ]]; then
        rc=1
    fi
    # pass-through: extract 出的 ptxir 再 embed → 再 extract，字节一致
    if [[ $rc -eq 0 ]] && ! "$PTXIR_EMBED" --in-cubin "$PTXIR_CUBIN" --in-ptxir "$tmpdir/roundtrip.ptxir" \
            --kernel-name "$PTXIR_KERNEL" --out "$tmpdir/pt2.cubin" >/dev/null 2>&1; then
        rc=1
    fi
    if [[ $rc -eq 0 ]] && ! "$PTXIR_EXTRACT" --in "$tmpdir/pt2.cubin" \
            --out-ptxir "$tmpdir/pt2.ptxir" >/dev/null 2>&1; then
        rc=1
    fi
    if [[ $rc -eq 0 ]] && ! cmp -s "$tmpdir/roundtrip.ptxir" "$tmpdir/pt2.ptxir"; then
        rc=1
    fi
    rm -rf "$tmpdir"
    return "$rc"
}
# 009 往返 / 010 pass-through 合并为一次临时文件往返（都覆盖，避免重复构建）
# set -euo pipefail 下禁止裸调用：非零返回会直接中止脚本。用 || rc=$? 守卫，
# 使函数真实退出码进入 case（FAIL=1 / SKIP=2），同时整体退出码为 0 不触发 set -e。
rc=0
run_ptxir_roundtrip || rc=$?
case "$rc" in
    0) pass_result "PTX-EMU-009 (ptxir embed→extract 往返)" && pass_result "PTX-EMU-010 (ptxir pass-through 字节一致)" ;;
    2) echo "[SKIP] PTX-EMU-009/010 (fixture 或工具缺失，跳过 ptxir 往返)" ;;
    *) fail_result "PTX-EMU-009/010 (ptxir 往返或 pass-through 失败)" ;;
esac

# ============================================================
# PTX-EMU-011: 现有 [sdma][h2d] Catch2 测试
# ============================================================
if [[ -x "$TEST_BIN" ]]; then
    if run_with_timeout 120 "$TEST_BIN" "[sdma][h2d]" -r compact >/tmp/ptxemu_h2d.log 2>&1; then
        pass_result "PTX-EMU-011 ([sdma][h2d] Catch2)"
    else
        fail_result "PTX-EMU-011 ([sdma][h2d] Catch2 失败)"
    fi
    rm -f /tmp/ptxemu_h2d.log
else
    fail_result "PTX-EMU-011 ($TEST_BIN 缺失)"
fi

# ============================================================
# PTX-EMU-012: [sdma][h2d][ptxir] Catch2 测试
# ============================================================
# 测试已存在 (test_sdma_engine_h2d.cc:234 "PTXIR-like 4 KiB image copies host→VRAM"),
# OFF 模式也可跑 (per test_sdma_engine_h2d.cc:243 注释 "不依赖 ON / 不涉及 SM 执行")。
# 仅当测试二进制未构建时跳过。
if [[ -x "$TEST_BIN" ]]; then
    if "$TEST_BIN" --list-tests 2>/dev/null | grep -q '\[sdma\]\[h2d\]\[ptxir\]'; then
        if run_with_timeout 120 "$TEST_BIN" "[sdma][h2d][ptxir]" -r compact >/tmp/ptxemu_ptxir.log 2>&1; then
            pass_result "PTX-EMU-012 ([sdma][h2d][ptxir] 通过)"
        else
            fail_result "PTX-EMU-012 ([sdma][h2d][ptxir] 失败)"
        fi
        rm -f /tmp/ptxemu_ptxir.log
    else
        # 测试用例未注册 → 明确 SKIP, 不静默通过也不误报失败
        echo "[SKIP] PTX-EMU-012 (test [sdma][h2d][ptxir] 未注册,跳过)"
    fi
else
    fail_result "PTX-EMU-012 ($TEST_BIN 缺失)"
fi

# ============================================================
# PTX-EMU-CUTE: CuTe 二进制 test -x + 依赖检查（绝不执行）
# ============================================================
if [[ "$QUICK" == 0 && "$E2E" == 0 ]]; then
    CUTE_BINS=(
        cute_hello_col_major
        cute_hello_tensor
        cute_hello_tiled_copy
        cute_rmsnorm
        cute_rmsnorm_debug
    )
    for bin in "${CUTE_BINS[@]}"; do
        path="$BUILD_DIR/bin/$bin"
        if [[ -x "$path" ]]; then
            # 仅检查可执行 + 依赖 PTX-EMU 库，绝不执行（kernel/CuTe 程序）
            deps=$(ldd "$path" 2>/dev/null | grep -oE 'lib(cudart|ptxsim|ptx_parser|ptx_ir)\.so' | sort -u || true)
            if [[ -n "$deps" ]]; then
                pass_result "PTXEMU-CUTE ($bin: 可执行 + 依赖 $(echo "$deps" | tr '\n' ' '))"
            else
                pass_result "PTXEMU-CUTE ($bin: 可执行，无 PTX-EMU 动态依赖)"
            fi
        else
            echo "[SKIP] PTXEMU-CUTE ($bin 未构建)"
        fi
    done
fi

# ============================================================
# PTX-EMU-DLOPEN: emulator dlopen 二进制（若存在）
# ============================================================
if [[ "$QUICK" == 0 && "$E2E" == 0 ]]; then
    DLOPEN_BIN="$BUILD_DIR/bin/test_cpptlm_emulator_dlopen"
    if [[ -x "$DLOPEN_BIN" ]]; then
        if run_with_timeout 60 "$DLOPEN_BIN" >/tmp/ptxemu_dlopen.log 2>&1; then
            pass_result "PTXEMU-DLOPEN ($(basename "$DLOPEN_BIN"))"
        else
            fail_result "PTXEMU-DLOPEN ($(basename "$DLOPEN_BIN") 失败)"
        fi
        rm -f /tmp/ptxemu_dlopen.log
    else
        echo "[SKIP] PTXEMU-DLOPEN (test_cpptlm_emulator_dlopen 未构建)"
    fi
fi

# ============================================================
# PTX-EMU-ON: ON 模式完整 Catch2/TLM/SoC 回归（配置路径真实存在）
# ============================================================
run_on_regression() {
    local ok=0
    # 完整 Catch2（ON 模式，含 PTX-EMU 链接）
    if [[ -x "$TEST_BIN" ]]; then
        if run_with_timeout 900 "$TEST_BIN" -r compact >/tmp/ptxemu_on_catch2.log 2>&1; then
            pass_result "PTXEMU-ON (Catch2 全量)"
        else
            fail_result "PTXEMU-ON (Catch2 全量失败)"
            ok=1
        fi
        rm -f /tmp/ptxemu_on_catch2.log
    else
        fail_result "PTXEMU-ON ($TEST_BIN 缺失)"
        ok=1
    fi
    # TLM/SoC 回归（配置路径全部真实存在，缺失即 FAIL）
    if [[ -x "$SIM_BIN" ]]; then
        local -a cfg_cases=(
            "cache_chstream_test.json|100"
            "cpu_tlm_test.json|200"
            "crossbar_test.json|100"
            "arbiter_tlm_test.json|100"
            "traffic_gen_tlm_test.json|200"
            "test/nic_router_nic.json|200"
            "mesh_2x2_tlm.json|200"
            "mesh_4x4_tlm.json|100"
            "hierarchical_2x2_tlm.json|100"
            "link_tlm_chain.json|100"
            "tlm_e2e_test.json|100"
            "gpu_2gpc_2tpc_2cu.json|100"
            "apu_soc_v1.json|100"
            "dgpu_board_v1.json|100"
        )
        local cfg cycles
        for entry in "${cfg_cases[@]}"; do
            IFS='|' read -r cfg cycles <<< "$entry"
            local full="$ROOT_DIR/configs/$cfg"
            if [[ ! -f "$full" ]]; then
                echo "ERROR: ON 回归配置缺失: $full" >&2
                fail_result "PTXEMU-ON (config $cfg 缺失)"
                ok=1
                continue
            fi
            if run_with_timeout 90 "$SIM_BIN" "$full" --cycles "$cycles" >/dev/null 2>&1; then
                pass_result "PTXEMU-ON (TLM $cfg)"
            else
                fail_result "PTXEMU-ON (TLM $cfg 失败)"
                ok=1
            fi
        done
    else
        fail_result "PTXEMU-ON (cpptlm_sim 缺失)"
        ok=1
    fi
    return "$ok"
}

if [[ "$E2E" == 1 || "$QUICK" == 0 ]]; then
    run_on_regression || true
fi

print_result_summary
