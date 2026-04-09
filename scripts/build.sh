#!/bin/bash
# CppTLM 构建脚本
# 用法：./scripts/build.sh [选项]

set -e

# 默认配置
BUILD_TYPE=${BUILD_TYPE:-Release}
USE_SYSTEMC=${USE_SYSTEMC:-OFF}
BUILD_TESTS=${BUILD_TESTS:-ON}
BUILD_EXAMPLES=${BUILD_EXAMPLES:-ON}

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     CppTLM 构建脚本 v2.0              ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""

# 创建构建目录
mkdir -p build
cd build

echo -e "${YELLOW}📦 配置构建...${NC}"
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DUSE_SYSTEMC=$USE_SYSTEMC \
    -DBUILD_TESTS=$BUILD_TESTS \
    -DBUILD_EXAMPLES=$BUILD_EXAMPLES \
    "$@"

echo ""
echo -e "${YELLOW}🔨 开始构建...${NC}"
ninja

echo ""
echo -e "${GREEN}✅ 构建成功！${NC}"
echo ""
echo "📊 构建统计:"
echo "  构建类型：$BUILD_TYPE"
echo "  SystemC:  $USE_SYSTEMC"
echo "  测试：    $BUILD_TESTS"
echo "  示例：    $BUILD_EXAMPLES"
echo ""
echo "📁 输出目录:"
echo "  可执行文件：build/bin/"
echo "  库文件：    build/lib/"
echo ""

if command -v ccache &> /dev/null; then
    echo "📊 ccache 统计:"
    ccache -s | head -5
    echo ""
fi
