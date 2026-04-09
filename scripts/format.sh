#!/bin/bash
# CppTLM 代码格式化脚本
# 用法：./scripts/format.sh [--check]

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     CppTLM 代码格式化脚本 v2.0        ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""

# 检查 clang-format
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}❌ 错误：clang-format 未安装${NC}"
    echo "   安装：apt install clang-format"
    exit 1
fi

# 检查模式
CHECK_MODE=false
if [ "$1" == "--check" ]; then
    CHECK_MODE=true
    echo -e "${YELLOW}🔍 检查模式：仅检查格式，不修改文件${NC}"
fi

# 查找所有 C++ 源文件
FILES=$(find include src test examples -type f \( -name "*.hpp" -o -name "*.h" -o -name "*.cpp" -o -name "*.cc" \) 2>/dev/null | grep -v build | grep -v catch_amalgamated)

if [ -z "$FILES" ]; then
    echo -e "${YELLOW}⚠️  未找到 C++ 源文件${NC}"
    exit 0
fi

echo -e "${YELLOW}📝 格式化 ${#FILES} 个文件...${NC}"
echo ""

# 格式化文件
COUNT=0
for file in $FILES; do
    if [ "$CHECK_MODE" = true ]; then
        # 检查模式
        if ! clang-format -style=file -output-replacements-xml "$file" | grep -q "<replacement"; then
            echo -e "${GREEN}✓${NC} $file"
        else
            echo -e "${RED}✗${NC} $file (需要格式化)"
            COUNT=$((COUNT + 1))
        fi
    else
        # 格式化模式
        clang-format -style=file -i "$file"
        echo -e "${GREEN}✓${NC} $file"
        COUNT=$((COUNT + 1))
    fi
done

echo ""
if [ "$CHECK_MODE" = true ] && [ $COUNT -gt 0 ]; then
    echo -e "${RED}❌ $COUNT 个文件需要格式化${NC}"
    echo "   运行：./scripts/format.sh 自动格式化"
    exit 1
else
    echo -e "${GREEN}✅ 格式化完成！${NC}"
fi
