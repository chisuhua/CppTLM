#!/usr/bin/env bash
# CppTLM 统一构建与测试入口。
# 支持 OFF、PTX-EMU、双构建及后续迁移中的模式专用测试脚本。

set -euo pipefail

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
ACTION="run"
QUICK=0
PYTHON_ONLY=0
E2E=0
CTEST=0
EXTRA_ARGS=()

usage() {
    cat <<'EOF'
用法: ./test.sh [选项] [额外 CMake 参数]

选项:
  --mode auto|off|ptx-emu|both  选择构建/测试模式（默认 auto）
  --build-only                 仅配置并构建
  --test-only                  仅测试；缺少测试二进制时失败
  --quick                      运行迁移后的快速测试模式
  --python-only                仅运行 Python 测试
  --e2e                        运行端到端测试
  --ctest                      运行当前构建目录的 ctest（替代旧 scripts/test/test.sh）
  --help                       显示帮助

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
            --build-only) ACTION=build; shift ;;
            --test-only) ACTION=test; shift ;;
            --quick) QUICK=1; shift ;;
            --python-only) PYTHON_ONLY=1; shift ;;
            --e2e) E2E=1; shift ;;
            --ctest) CTEST=1; shift ;;
            --help|-h) usage; exit 0 ;;
            --) shift; EXTRA_ARGS+=("$@"); break ;;
            *) EXTRA_ARGS+=("$1"); shift ;;
        esac
    done
    if (( CTEST )) && [[ "$ACTION" != run ]]; then
        error "--ctest 不能与 --build-only/--test-only 同时使用"
    fi
    if (( PYTHON_ONLY )) && [[ "$ACTION" != run ]]; then
        error "--python-only 不能与构建/测试专用模式同时使用"
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
