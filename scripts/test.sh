#!/bin/bash
# CppTLM 测试脚本
# 用法：./scripts/test.sh [选项]

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     CppTLM 测试脚本 v2.0              ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""

# 检查构建目录
if [ ! -d "build" ]; then
    echo -e "${RED}❌ 错误：构建目录不存在，请先运行 ./scripts/build.sh${NC}"
    exit 1
fi

cd build

echo -e "${YELLOW}🧪 运行测试...${NC}"
echo ""

# 运行测试
ctest --output-on-failure "$@"

echo ""
echo -e "${GREEN}✅ 测试完成！${NC}"
echo ""

# 显示测试摘要
echo "📊 测试摘要:"
ctest -N | tail -5
