#!/usr/bin/env bash
# CppTLM 测试入口 (根目录便捷路径).
#
# 设计动机: 用户从仓库根目录起步时, 期望 ./test.sh 可立即使用.
# 本脚本默认只跑测试 (假设 ./build.sh 已构建); 需先构建时显式 --build.
# 实际测试逻辑仍在 scripts/test/test_*.sh (OFF/PTX-EMU 分模式).
#
# 用法:
#   ./test.sh                          # 默认: 跑测试 (binary 须已构建, 否则报错)
#   ./test.sh --build                  # 先 build 后 test (旧默认行为, 一次性)
#   ./test.sh --build-only             # 只 build, 不跑测试
#   ./test.sh --verify-fix             # 跑 verify_test_seam_fix.sh (Oracle Option C + classify_naive fallback 验证)
#   ./test.sh --mode off|ptx-emu|both  选择测试模式 (默认 auto)
#   ./test.sh --quick / --python-only / --e2e / --ctest
#
# 配合 ./build.sh:
#   ./build.sh && ./test.sh             # 推荐流程: 先 build 再 test
#   ./build.sh --rebuild && ./test.sh  # 干净重建 + 跑测试

set -euo pipefail
set +m  # 关闭 job control 消息 (Killed 噪音抑制)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"
COMMON_LIB="$ROOT_DIR/scripts/test/_lib/common.sh"
# shellcheck source=/dev/null
source "$COMMON_LIB"

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_TESTS="${BUILD_TESTS:-ON}"
BUILD_EXAMPLES="${BUILD_EXAMPLES:-ON}"
BUILD_DIR_ENV_SET="${BUILD_DIR+x}"
BUILD_DIR="${BUILD_DIR:-build}"
MODE="auto"
# 默认行为改为 "test" (而非 "run"), 显式构建用 --build
ACTION="test"
BUILD_FIRST=0
# ⚠️ 必须区分「**默认** ACTION=test」与「**显式** --test-only」:
#    否则 --build 永远撞上 --test-only 的互斥检查(test.sh 的 --build 自 2026-10-24
#    CI 实测**完全不可用** —— 而 AGENTS.md 明写「./test.sh --build ≡ ./build.sh && ./test.sh」)。
EXPLICIT_BUILD_ONLY=0
EXPLICIT_TEST_ONLY=0
VERIFY_FIX=0
QUICK=0
PYTHON_ONLY=0
E2E=0
CTEST=0
EXTRA_ARGS=()

usage() {
    cat <<'EOF'
用法: ./test.sh [选项] [额外 CMake 参数]

选项:
  --build                     先构建后测试 (一次性, 旧默认行为; 显式触发)
  --build-only                仅配置并构建, 不跑测试
  --mode auto|off|ptx-emu|both  选择构建/测试模式 (默认 auto)
  --test-only                 仅测试; 缺少测试二进制时失败 (默认)
  --verify-fix                跑 verify_test_seam_fix.sh
                             ([dgpu_framebuffer] 修复 + 4 commit 回归验证)
  --quick                     运行迁移后的快速测试模式
  --python-only               仅运行 Python 测试
  --e2e                       运行端到端测试
  --ctest                     运行当前构建目录的 ctest (替代旧 scripts/test/test.sh)
  --help                      显示帮助

环境变量:
  BUILD_DIR BUILD_TYPE BUILD_TESTS BUILD_EXAMPLES
EOF
}

error() { echo "ERROR: $*" >&2; exit 2; }

cache_value() {
    local cache="$1" key="$2"
    [[ -f "$cache" ]] || return 1
    sed -n "s/^${key}:[^=]*=//p" "$cache" | tail -1
}

cache_mode() {
    local dir="$1" value
    value="$(cache_value "$dir/CMakeCache.txt" CPPTLM_WITH_PTX_EMU || true)"
    case "${value^^}" in
        ON|TRUE|YES|1) echo ptx-emu ;;
        OFF|FALSE|NO|0|"") echo off ;;
        *) error "无法识别 $dir/CMakeCache.txt 中的 CPPTLM_WITH_PTX_EMU=$value" ;;
    esac
}

validate_mode_dir() {
    local wanted="$1" dir="$2" actual
    [[ -f "$dir/CMakeCache.txt" ]] || return 0
    actual="$(cache_mode "$dir")"
    [[ "$actual" == "$wanted" ]] || error "显式模式 --mode $wanted 与构建目录 $dir 的 CMakeCache 模式 $actual 矛盾"
}

detect_mode() {
    if [[ "$MODE" != auto ]]; then
        [[ "$MODE" == both || "$MODE" == off || "$MODE" == ptx-emu ]] || error "无效模式: $MODE"
        return
    fi
    if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        MODE="$(cache_mode "$BUILD_DIR")"
    elif [[ "$BUILD_DIR" == build-on ]]; then
        MODE=ptx-emu
    else
        MODE=off
    fi
}

parse_args() {
    while (($#)); do
        case "$1" in
            --mode)
                (($# >= 2)) || error "--mode 需要参数"
                MODE="$2"; shift 2 ;;
            --build)       BUILD_FIRST=1; shift ;;
            --build-only)  ACTION=build; EXPLICIT_BUILD_ONLY=1; shift ;;
            --test-only)   ACTION=test; EXPLICIT_TEST_ONLY=1; shift ;;
            --verify-fix)  VERIFY_FIX=1; shift ;;
            --quick)       QUICK=1; shift ;;
            --python-only) PYTHON_ONLY=1; shift ;;
            --e2e)         E2E=1; shift ;;
            --ctest)       CTEST=1; shift ;;
            --help|-h)     usage; exit 0 ;;
            --) shift; EXTRA_ARGS+=("$@"); break ;;
            *) EXTRA_ARGS+=("$1"); shift ;;
        esac
    done
    # --build 隐式转 ACTION=run (build + test 一次完成)
    if (( BUILD_FIRST )); then
        if (( EXPLICIT_BUILD_ONLY )); then
            error "--build 与 --build-only 互斥 (--build 含 build+test; --build-only 仅 build)"
        fi
        if (( EXPLICIT_TEST_ONLY )); then
            error "--build 与 --test-only 互斥 (--build 含 build+test; --test-only 仅 test)"
        fi
        ACTION=run
    fi
    if (( CTEST )) && [[ "$ACTION" == "build" ]]; then
        error "--ctest 不能与 --build-only 同时使用 (ctest 跑测试, --build-only 仅构建)"
    fi
    if (( PYTHON_ONLY )) && [[ "$ACTION" == "build" ]]; then
        error "--python-only 不能与 --build-only 同时使用 (Python 跑测试, --build-only 仅构建)"
    fi
}

build_one() {
    local mode="$1" dir="$2"
    echo "=== 构建模式: $mode ($dir) ==="
    if [[ "$mode" == ptx-emu ]]; then
        if [[ "$dir" == build-on && -z "${BUILD_DIR_ENV_SET}" ]]; then
            (cd "$ROOT_DIR" && BUILD_TYPE="$BUILD_TYPE" BUILD_TESTS="$BUILD_TESTS" BUILD_EXAMPLES="$BUILD_EXAMPLES" \
                "$ROOT_DIR/scripts/build/build_ptx_emu.sh" "${EXTRA_ARGS[@]}")
        else
            cmake -S "$ROOT_DIR" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
                -DBUILD_TESTS="$BUILD_TESTS" -DBUILD_EXAMPLES="$BUILD_EXAMPLES" \
                -DCPPTLM_WITH_PTX_EMU=ON -DPTXEMU_BUILD_TESTING=OFF "${EXTRA_ARGS[@]}"
            cmake --build "$dir" -j"$(nproc)"
        fi
    elif [[ "$dir" == build && -z "${BUILD_DIR_ENV_SET}" ]]; then
        (cd "$ROOT_DIR" && BUILD_TYPE="$BUILD_TYPE" BUILD_TESTS="$BUILD_TESTS" BUILD_EXAMPLES="$BUILD_EXAMPLES" \
            "$ROOT_DIR/scripts/build/build.sh" "${EXTRA_ARGS[@]}")
    else
        cmake -S "$ROOT_DIR" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
            -DBUILD_TESTS="$BUILD_TESTS" -DBUILD_EXAMPLES="$BUILD_EXAMPLES" \
            -DCPPTLM_WITH_PTX_EMU=OFF "${EXTRA_ARGS[@]}"
        cmake --build "$dir" -j"$(nproc)"
    fi
}

run_mode() {
    local mode="$1" dir="$2" script
    # mode 名称中的连字符归一化（下划线命名）
    local mode_slug="${mode//-/_}"
    script="$ROOT_DIR/scripts/test/test_${mode_slug}.sh"
    [[ -x "$script" ]] || error "模式脚本不存在或不可执行: $script（迁移完成后应提供该脚本）"
    local args=()
    (( QUICK )) && args+=(--quick)
    (( PYTHON_ONLY )) && args+=(--python-only)
    (( E2E )) && args+=(--e2e)
    BUILD_DIR="$dir" "$script" "${args[@]}"
}

parse_args "$@"

# --verify-fix 短路: 转 verify_test_seam_fix.sh 后退出 (替代 export 形式, 避免 main 顶层污染)
dispatch_verify_fix() {
    local verify_script="$ROOT_DIR/scripts/test/verify_test_seam_fix.sh"
    local verify_args=()
    local arg
    if [[ ! -x "$verify_script" ]]; then
        error "verify 脚本不存在或不可执行: $verify_script"
    fi
    for arg in "$@"; do
        [[ "$arg" == "--verify-fix" ]] && continue
        verify_args+=("$arg")
    done
    exec "$verify_script" "${verify_args[@]}"
}

if (( VERIFY_FIX )); then
    dispatch_verify_fix "$@"
fi

# 显式 PTX-EMU 模式未指定 BUILD_DIR 时，采用其专用默认目录。
if [[ "$MODE" == ptx-emu && -z "$BUILD_DIR_ENV_SET" && "$BUILD_DIR" == build ]]; then
    BUILD_DIR=build-on
fi
if [[ "$MODE" == off && "$BUILD_DIR" == build-on ]]; then
    error "显式模式 --mode off 不能使用命名为 build-on 的 BUILD_DIR"
fi
if [[ "$MODE" == ptx-emu && "$BUILD_DIR" == build ]]; then
    error "显式模式 --mode ptx-emu 不能使用命名为 build 的 BUILD_DIR"
fi
detect_mode
if [[ "$MODE" == off ]]; then validate_mode_dir off "$BUILD_DIR"; fi
if [[ "$MODE" == ptx-emu ]]; then validate_mode_dir ptx-emu "$BUILD_DIR"; fi

if (( CTEST )); then
    [[ -f "$BUILD_DIR/CMakeCache.txt" ]] || error "构建目录不存在或未配置: $BUILD_DIR"
    ctest --test-dir "$BUILD_DIR" --output-on-failure -E cute "${EXTRA_ARGS[@]}"
    exit 0
fi

if [[ "$MODE" == both ]]; then
    [[ -n "${BUILD_DIR_ENV_SET}" ]] && error "--mode both 不支持单一 BUILD_DIR；请清除 BUILD_DIR 以使用 build/build-on"
    modes=(off ptx-emu)
else
    modes=("$MODE")
fi

for mode in "${modes[@]}"; do
    dir="$BUILD_DIR"
    [[ "$MODE" == both ]] && dir="$([[ "$mode" == ptx-emu ]] && echo build-on || echo build)"
    if [[ "$ACTION" != test ]]; then build_one "$mode" "$dir"; fi
    if [[ "$ACTION" == build ]]; then continue; fi
    if (( PYTHON_ONLY == 0 )); then
        [[ -x "$dir/bin/cpptlm_tests" ]] || error "--test-only/测试需要 $dir/bin/cpptlm_tests，但该文件不存在"
    fi
    run_mode "$mode" "$dir"
done
