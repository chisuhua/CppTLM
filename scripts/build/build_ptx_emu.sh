#!/bin/bash
# CppTLM 构建脚本（PTX-EMU 集成模式）
# 用法：./scripts/build/build_ptx_emu.sh [选项]
#
# 适用场景：dGPU/APU SoC 长期开发任务。
#   - 默认启用 -DCPPTLM_WITH_PTX_EMU=ON（PTX-EMU submodule 集成）
#   - 编译 ptxemu_core 静态库 + libptxemu_device.so + libptxsim.so + libptx_parser.so + libcudart.so
#   - PTX kernel 指令通过 PTXEMU_API_VERSION=1 公共接口执行
#
# 何时使用本脚本 vs scripts/build/build.sh：
#   - dGPU/APU SoC / GPU 集成开发者 → 本脚本（强制 ON）
#   - 纯 TLM/NoC 研究者 → scripts/build/build.sh（默认 OFF，构建更快）
#   - CI（Release/Debug × ASan 矩阵）→ .github/workflows/ci.yml

set -e

# 默认配置
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_TESTS=${BUILD_TESTS:-ON}
BUILD_EXAMPLES=${BUILD_EXAMPLES:-ON}
BUILD_DIR=${BUILD_DIR:-build-on}

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}╔════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  CppTLM 构建脚本（PTX-EMU 集成模式）v1.0      ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════╝${NC}"
echo ""

# 1. 验证 submodule 已初始化
if [ ! -f "external/PTX-EMU/CMakeLists.txt" ]; then
    echo -e "${RED}❌ 错误: external/PTX-EMU submodule 未初始化${NC}"
    echo -e "${YELLOW}  修复: git submodule update --init --recursive external/PTX-EMU${NC}"
    exit 1
fi

# 2. PTX-EMU 端 ANTLR4 路径硬编码 workaround
#    PTX-EMU CMakeLists.txt 用 ${CMAKE_SOURCE_DIR}/antlr4/... 假设 PTX-EMU 是顶层项目。
#    CppTLM 作为父项目消费时需创建 symlink 指向 PTX-EMU 内的 antlr4/。
ANTLR4_LINK="./antlr4"
ANTLR4_TARGET="external/PTX-EMU/antlr4"
if [ ! -e "$ANTLR4_LINK" ]; then
    echo -e "${YELLOW}🔗 创建 ANTLR4 symlink workaround ($ANTLR4_LINK → $ANTLR4_TARGET)${NC}"
    ln -sfn "$ANTLR4_TARGET" "$ANTLR4_LINK"
    ANTLR4_CREATED=1
else
    echo -e "${BLUE}ℹ  ANTLR4 symlink 已存在${NC}"
    ANTLR4_CREATED=0
fi

# 3. 确保脚本退出时清理 symlink（如本脚本创建）
cleanup() {
    if [ "$ANTLR4_CREATED" = "1" ] && [ -L "$ANTLR4_LINK" ]; then
        rm -f "$ANTLR4_LINK"
        echo -e "${BLUE}🧹 已清理 ANTLR4 symlink${NC}"
    fi
}
trap cleanup EXIT

# 4. 创建构建目录
echo -e "${YELLOW}📦 配置构建（PTX-EMU ON, $BUILD_TYPE, Tests: $BUILD_TESTS, Examples: $BUILD_EXAMPLES）...${NC}"
mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS="$BUILD_TESTS" \
    -DBUILD_EXAMPLES="$BUILD_EXAMPLES" \
    -DCPPTLM_WITH_PTX_EMU=ON \
    -DPTXEMU_BUILD_TESTING=OFF \
    "$@"

# 5. 开始构建
echo ""
echo -e "${YELLOW}🔨 开始构建（PTX-EMU 集成模式）...${NC}"
echo -e "${BLUE}ℹ  预期产出:${NC}"
echo -e "${BLUE}   - libcpptlm_core.a (含 dGPU/APU SoC 集成代码)${NC}"
echo -e "${BLUE}   - libptxemu_core.a (PTX-EMU 公共 device API 实现)${NC}"
echo -e "${BLUE}   - libptxemu_device.so + libptxsim.so + libptx_parser.so${NC}"
echo -e "${BLUE}   - libcudart.so (fake CUDA runtime)${NC}"
echo ""
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo ""
echo -e "${GREEN}✅ PTX-EMU 集成构建成功！${NC}"
echo ""
echo "📊 构建统计:"
echo "  构建类型：  $BUILD_TYPE"
echo "  CPPTLM_WITH_PTX_EMU: ON"
echo "  PTXEMU_BUILD_TESTING: OFF (CppTLM ctest 隔离 PTX-EMU tests)"
echo "  测试：      $BUILD_TESTS"
echo "  示例：      $BUILD_EXAMPLES"
echo ""
echo "📁 输出目录:"
echo "  可执行文件：  $BUILD_DIR/bin/"
echo "  静态库：      $BUILD_DIR/lib/lib{cpptlm_core,ptxemu_core}.a"
echo "  PTX-EMU .so: $BUILD_DIR/lib/lib{ptxemu_device,ptxsim,ptx_parser}.so"
echo "  fake cudart: $BUILD_DIR/lib/libcudart.so"
echo ""

# 6. 验证关键产出
if [ -f "$BUILD_DIR/lib/libptxemu_core.a" ] && [ -f "$BUILD_DIR/lib/libcpptlm_core.a" ]; then
    echo -e "${GREEN}✓ libptxemu_core.a + libcpptlm_core.a 均已生成${NC}"
    echo ""
    echo "🚀 快速验证:"
    echo "  ctest --test-dir $BUILD_DIR --output-on-failure -j4"
    echo "  ./$BUILD_DIR/bin/cpptlm_tests  # 全部 817 用例"
else
    echo -e "${RED}⚠️  警告: 关键产物缺失,构建可能未完成${NC}"
    exit 1
fi

if command -v ccache &> /dev/null; then
    echo ""
    echo "📊 ccache 统计:"
    ccache -s | head -5
fi