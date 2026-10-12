#!/usr/bin/env bash
# CppTLM 独立构建入口 (根目录便捷路径)
#
# 设计动机: 用户从仓库根目录起步时, 期望 ./build.sh 可立即使用,
# 不必深入 scripts/build/ 子目录. 本脚本为薄包装, 实际构建逻辑
# 仍在 scripts/build/build.sh (OFF 路径) / build_ptx_emu.sh (PTX-EMU 路径).
#
# 用法:
#   ./build.sh                           # OFF 路径增量构建 (默认)
#   ./build.sh --rebuild                 # rm -rf build/ + 重新配置 + 构建
#   ./build.sh --ptx-emu                 # PTX-EMU 集成路径 (需 git submodule init)
#   ./build.sh --debug                   # Debug 构建
#   ./build.sh --no-tests                # 跳过测试 target
#   ./build.sh --no-examples             # 跳过 example target
#   ./build.sh --jobs N                  # 并行度 (默认 2, OOM-safe per AGENTS.md)
#   ./build.sh --clean                   # 同 --rebuild (alias)
#
# 环境变量:
#   BUILD_TYPE      Release/Debug/RelWithDebInfo (默认 Release)
#   BUILD_TESTS     ON/OFF (默认 ON)
#   BUILD_EXAMPLES  ON/OFF (默认 ON)
#   BUILD_DIR       自定义 build 目录 (默认 build)
#   CPPTLM_WITH_PTX_EMU  ON/OFF (默认 OFF; --ptx-emu 覆盖)

set -euo pipefail
set +m  # 关闭 job control 消息 (Killed 噪音抑制)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# 默认值 (与 scripts/build/build.sh 对齐)
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_TESTS="${BUILD_TESTS:-ON}"
BUILD_EXAMPLES="${BUILD_EXAMPLES:-ON}"
BUILD_DIR="${BUILD_DIR:-build}"
JOBS="${JOBS:-2}"  # OOM-safe 默认 (项目 AGENTS.md 警告: -j$(nproc) 在该机器 OOM)
REBUILD=0
PTX_EMU=0
EXTRA_ARGS=()

usage() {
    cat <<'EOF'
用法: ./build.sh [选项] [额外 CMake 参数]

选项:
  --rebuild               rm -rf build/ 后重新配置 + 构建
  --clean                 同 --rebuild (alias)
  --ptx-emu               启用 PTX-EMU 集成 (需 git submodule init)
  --debug                 Debug 构建 (覆盖 BUILD_TYPE)
  --no-tests              跳过测试 target (覆盖 BUILD_TESTS=OFF)
  --no-examples           跳过 example target (覆盖 BUILD_EXAMPLES=OFF)
  --jobs N                并行度 (默认 2, OOM-safe)
  --build-dir DIR         自定义 build 目录 (默认 build)
  --help                  显示帮助

环境变量:
  BUILD_TYPE BUILD_TESTS BUILD_EXAMPLES BUILD_DIR CPPTLM_WITH_PTX_EMU
EOF
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        --rebuild|--clean)    REBUILD=1; shift ;;
        --ptx-emu)            PTX_EMU=1; shift ;;
        --debug)              BUILD_TYPE=Debug; shift ;;
        --no-tests)           BUILD_TESTS=OFF; shift ;;
        --no-examples)        BUILD_EXAMPLES=OFF; shift ;;
        --jobs)               JOBS="${2:-2}"; shift 2 ;;
        --build-dir)          BUILD_DIR="${2:-build}"; shift 2 ;;
        --help|-h)            usage; exit 0 ;;
        --)                   shift; EXTRA_ARGS+=("$@"); break ;;
        *)                    EXTRA_ARGS+=("$1"); shift ;;
    esac
done

# 选择构建脚本
if [[ $PTX_EMU -eq 1 ]]; then
    BUILD_SCRIPT="$ROOT_DIR/scripts/build/build_ptx_emu.sh"
    BUILD_MODE_DESC="PTX-EMU 集成 (CPPTLM_WITH_PTX_EMU=ON)"
else
    BUILD_SCRIPT="$ROOT_DIR/scripts/build/build.sh"
    BUILD_MODE_DESC="OFF 路径 (CPPTLM_WITH_PTX_EMU=OFF)"
fi

if [[ ! -x "$BUILD_SCRIPT" ]]; then
    echo -e "${RED}ERROR: 构建脚本不存在或不可执行: $BUILD_SCRIPT${NC}" >&2
    exit 2
fi

# 干净构建
if [[ $REBUILD -eq 1 ]]; then
    echo -e "${YELLOW}🗑  干净构建: rm -rf $BUILD_DIR${NC}"
    rm -rf "$ROOT_DIR/$BUILD_DIR"
fi

# Banner
echo -e "${BOLD}${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║     CppTLM 构建入口 v1.0              ║${NC}"
echo -e "${BOLD}${BLUE}╚════════════════════════════════════════╝${NC}"
echo -e "  mode:      $BUILD_MODE_DESC"
echo -e "  type:      $BUILD_TYPE"
echo -e "  tests:     $BUILD_TESTS"
echo -e "  examples:  $BUILD_EXAMPLES"
echo -e "  build dir: $BUILD_DIR"
echo -e "  jobs:      $JOBS (OOM-safe 默认 2)"
echo ""

# 调用实际构建脚本
# 注意: scripts/build/build.sh 和 build_ptx_emu.sh 已内部包含 cmake configure + ninja 全套,
# 故此处不再额外 cmake --build (否则双重构建). 子脚本失败由 set -euo pipefail 捕获.
BUILD_TYPE="$BUILD_TYPE" \
BUILD_TESTS="$BUILD_TESTS" \
BUILD_EXAMPLES="$BUILD_EXAMPLES" \
BUILD_DIR="$BUILD_DIR" \
"$BUILD_SCRIPT" "${EXTRA_ARGS[@]}"

# 子脚本已做完整构建, 这里仅 print 输出指引 banner
echo ""
echo -e "${GREEN}✅ 构建完成 (由 scripts/build/$([ $PTX_EMU -eq 1 ] && echo "build_ptx_emu.sh" || echo "build.sh") 接管)${NC}"
echo ""
echo "📁 输出目录:"
echo "  可执行文件: $BUILD_DIR/bin/"
echo "  库文件:     $BUILD_DIR/lib/"
echo ""
if [[ -x "$ROOT_DIR/$BUILD_DIR/bin/cpptlm_tests" ]]; then
    echo -e "${GREEN}💡 提示: 运行测试用 ./test.sh 或 ./test.sh --build${NC}"
else
    echo -e "${YELLOW}⚠️  $BUILD_DIR/bin/cpptlm_tests 不存在 (BUILD_TESTS=OFF 或构建失败)${NC}"
fi
