#!/bin/bash
# CppTLM 统一测试编排（迁移后的向后兼容薄包装）
#
# 说明:
#   迁移后各模式测试已拆分到:
#     - scripts/test/test_off.sh      OFF 路径（Python + Catch2 + TLM E2E + 可执行文件）
#     - scripts/test/test_ptx_emu.sh  PTX-EMU 路径（H2D-only PTXIR image DMA 目录 PTX-EMU-001..012）
#   PTX-EMU 功能验证**不以 ctest -E cute 为凭**（ctest 只反映 CTest 注册的测试集，
#   不能证明 PTX-EMU 库/ABI/ptxir 往返的工作性），因此本包装不再将 --ctest 视为
#   PTX-EMU 验证入口；PTX-EMU 验证请直接运行 scripts/test/test_ptx_emu.sh。
#
# 向后兼容转发（受支持的 legacy 参数转发到根 test.sh）:
#   --quick        → root test.sh --quick
#   --python-only  → root test.sh --python-only
#   --e2e          → root test.sh --e2e
#   --ctest        → root test.sh --ctest
#   --build-dir D  → BUILD_DIR=D root test.sh（等效 --mode off --test-only）
#   --ptx-emu      → BUILD_DIR=build-on（默认）scripts/test/test_ptx_emu.sh
#   --off          → BUILD_DIR=build（默认）scripts/test/test_off.sh
#   其他参数/无参数 → root test.sh（默认模式回归），未知参数报错
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
ROOT_TEST="$ROOT_DIR/test.sh"

usage() {
    cat <<'EOF'
用法: ./scripts/test/run_all_tests.sh [选项]

选项（向后兼容转发）:
  --quick                快速模式（转发 root test.sh --quick）
  --python-only          仅 Python 测试
  --e2e                  仅 E2E
  --ctest                运行 ctest（root test.sh --ctest）
  --off                  直接运行 OFF 路径 test_off.sh（默认 build）
  --ptx-emu              直接运行 PTX-EMU 路径 test_ptx_emu.sh（默认 build-on）
  --build-dir <dir>      指定构建目录
  --help                 显示帮助

说明:
  legacy 脚本已迁移；PTX-EMU 功能验证请使用 scripts/test/test_ptx_emu.sh
  （ctest -E cute 不是 PTX-EMU 功能证明）。
EOF
}

MODE_ARG="run_all"
BUILD_DIR_ARG=""
MODE=""
while (($#)); do
    case "$1" in
        --quick) MODE_ARG=quick; shift ;;
        --python-only) MODE_ARG=python_only; shift ;;
        --e2e) MODE_ARG=e2e; shift ;;
        --ctest) MODE_ARG=ctest; shift ;;
        --off) MODE_ARG=off; shift ;;
        --ptx-emu) MODE_ARG=ptx_emu; shift ;;
        --build-dir)
            [[ $# -ge 2 ]] || { echo "ERROR: --build-dir 需要参数" >&2; exit 2; }
            BUILD_DIR_ARG="$2"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        --) shift; break ;;
        -*) echo "ERROR: 不支持的参数: $1" >&2; usage >&2; exit 2 ;;
        *) MODE_ARG=root; shift ;;
    esac
done

if [[ -n "$BUILD_DIR_ARG" ]]; then
    export BUILD_DIR="$BUILD_DIR_ARG"
fi

case "$MODE_ARG" in
    quick) exec "$ROOT_TEST" --quick ;;
    python_only) exec "$ROOT_TEST" --python-only ;;
    e2e) exec "$ROOT_TEST" --e2e ;;
    ctest) exec "$ROOT_TEST" --ctest ;;
    off) exec "$SCRIPT_DIR/test_off.sh" ;;
    ptx_emu) exec "$SCRIPT_DIR/test_ptx_emu.sh" ;;
    *) exec "$ROOT_TEST" ;;
esac