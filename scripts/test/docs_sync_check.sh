#!/bin/bash
# CppTLM 文档同步检查脚本
# 用途：扫描核心文档 (AGENTS.md, ONBOARDING.md, roadmap.md, scripts/README.md)
#       中提及的文件/目录路径，验证它们在仓库中真实存在。
#
# 用法：
#   ./scripts/test/docs_sync_check.sh            # 检查并打印报告
#   ./scripts/test/docs_sync_check.sh --strict   # 发现问题返回非零退出码
#
# 作为 pre-commit hook 运行（.pre-commit-config.yaml）时使用 --strict 模式。

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

STRICT=false
if [ "$1" == "--strict" ]; then
    STRICT=true
fi

# 项目根目录（脚本所在位置的祖父目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   CppTLM 文档同步检查 v1.0            ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
echo ""

# 核心文档列表
DOCS=(
    "AGENTS.md"
    "docs/ONBOARDING.md"
    "roadmap.md"
    "scripts/README.md"
)

# 路径提取模式：
#   - 形如 `path/to/file.hh` 的反引号引用（Markdown）
#   - 形如 `include/xxx.hh`、`scripts/yyy.sh` 的相对路径
#   - 排除：
#     * 纯 URL（含 :// 或 www.）
#     * Markdown 锚点（#xxx）
#     * 以 .md 结尾的链接（其他文档）
#     * 单字符路径（占位符如 "/"）
#     * 含通配符的路径（*.py 等）
#     * 已知文档内提及的"虚拟"路径（src/noc/ 等已通过注释说明）
PATH_REGEX='`(([a-zA-Z0-9_./-]+/)?[a-zA-Z0-9_.-]+\.(hh|cc|hpp|cpp|h|sh|py|json|yaml|yml|toml))`'

# 虚拟/设计示意路径（不应被检查）
# 包括：设计示意 + 已删除/已归档文件（在文档中说明删除原因时引用）+ 规划中文件
VIRTUAL_PATHS=(
    "src/noc/"        # 设计示意，实际在 src/tlm/ 或 src/rtl/
    "src/noc/routing"
    "src/noc/*.cc"
    "src/noc/*.hh"
    "include/noc/"
    "cpptlm/visualization/editor/node_modules"  # gitignored
    # v2.1 已删除（CHANGELOG.md 2026-06-08），文档中说明删除原因时引用
    "cpu_main.cpp"
    "traffic_main.cpp"
    "sc_main.cpp"
    "src/cpu_main.cpp"
    "src/traffic_main.cpp"
    "src/sc_main.cpp"
    "cpu_cluster.cc"  # samples-orphaned 中；说明 DEPRECATED 时引用
    "cpu_cluster.hh"  # v2.2 删除 (include/modules/legacy/)；docs/ONBOARDING.md §2.6 历史说明引用
    "cpu_sim.hh"      # v2.2 删除 (include/modules/legacy/cpu_sim.hh)；docs/ONBOARDING.md §2.6 历史说明引用
    # 已归档到 docs-archived/
    "ext/packet_to_payload.hh"
    "ext/payload_to_packet.hh"
    "modules_v2.hh"
    # Phase 7.B-F (GPU APU Fused SoC) 规划中文件（roadmap.md 2026-06-11；7.A 已落地 2026-06-11）
    "compute_unit_tlm.hh"
    "tcc_tlm.hh"
    "kernel_launch_tlm.hh"
    "pcie_bridge_tlm.hh"
    "apu_demo_v1.json"
    "apu_demo_v2.json"
    "apu_demo_v3.json"
    "apu_demo_v4.json"
    "apu_full_soc.json"
    "test_apu_soc.py"
    # gem5 参考路径（roadmap.md 中引用，但非本仓库文件）
    "configs/example/apu_se.py"
    "configs/example/gpufs/Disjoint_VIPER.py"
    "src/dev/amdgpu/amdgpu_device.py"
)

# CMake 添加的 include dir（无前缀写法也能找到文件）
# 见 CMakeLists.txt: include/, include/core/, external/json/
# 以及 scripts/ 的 5 子目录结构（build/, test/, pipeline/, topology/, stats/）
# 与 cpptlm/ 的子目录结构（config/, simulation/, topo/, visualization/, analysis/）
INCLUDE_PATH_PREFIXES=(
    ""
    "include/"
    "include/core/"
    "include/core/ext/"
    "include/ext/"
    "include/tlm/"
    "include/framework/"
    "include/bundles/"
    "include/rtl/"
    "include/utils/"
    "include/sc_core/"
    "include/metrics/"
    "include/modules/"
    "include/modules/legacy/"
    "src/"
    "src/core/"
    "src/tlm/"
    "src/rtl/"
    "src/utils/"
    "scripts/"
    "scripts/build/"
    "scripts/test/"
    "scripts/pipeline/"
    "scripts/topology/"
    "scripts/stats/"
    "cpptlm/"
    "cpptlm/analysis/"
    "cpptlm/config/"
    "cpptlm/simulation/"
    "cpptlm/tests/"
    "cpptlm/topo/"
    "cpptlm/visualization/"
    "cpptlm_config/"
    "cpptlm_config/tests/"
    "cpptlm_config/examples/"
    "configs/"
    "test/"
    "test/python/"
    "examples/"
    "samples/"
)

is_virtual_path() {
    local path="$1"
    for vp in "${VIRTUAL_PATHS[@]}"; do
        if [[ "$path" == *"$vp"* ]]; then
            return 0
        fi
    done
    return 1
}

# 检查路径是否存在（尝试多种前缀）
path_exists() {
    local path="$1"
    # 绝对路径直接检查
    if [ -e "$path" ]; then
        return 0
    fi
    # 尝试 CMake include path 前缀补全
    local prefix_list=("${INCLUDE_PATH_PREFIXES[@]}")
    # 路径已含已知根目录前缀时，避免嵌套重复（scripts/credit_flow.py 不应再补 scripts/topology/）
    # 但仍允许"父目录下其他子目录"的尝试：scripts/credit_flow.py → scripts/topology/credit_flow.py
    if [[ "$path" == scripts/* ]]; then
        prefix_list=("" "${path%%/*}/" "scripts/build/" "scripts/test/" "scripts/pipeline/" "scripts/topology/" "scripts/stats/")
    elif [[ "$path" == cpptlm/* ]]; then
        prefix_list=("" "cpptlm/analysis/" "cpptlm/config/" "cpptlm/simulation/" "cpptlm/tests/" "cpptlm/topo/" "cpptlm/visualization/")
    elif [[ "$path" == cpptlm_config/* ]]; then
        prefix_list=("" "cpptlm_config/tests/" "cpptlm_config/examples/")
    fi
    for prefix in "${prefix_list[@]}"; do
        if [ -e "${prefix}${path}" ]; then
            return 0
        fi
    done
    return 1
}

MISSING_COUNT=0
TOTAL_COUNT=0
declare -a MISSING_PATHS

for doc in "${DOCS[@]}"; do
    if [ ! -f "$doc" ]; then
        echo -e "${YELLOW}⚠️  文档不存在：$doc${NC}"
        continue
    fi

    while IFS= read -r match; do
        # 去除反引号
        path="${match//\`/}"

        # 跳过虚拟路径
        if is_virtual_path "$path"; then
            continue
        fi

        # 跳过 .md 文件（指向其他文档，已由 markdown 链接检查覆盖）
        if [[ "$path" == *.md ]]; then
            continue
        fi

        TOTAL_COUNT=$((TOTAL_COUNT + 1))

        # 检查路径是否存在（文件或目录），尝试 CMake include path 前缀补全
        if ! path_exists "$path"; then
            MISSING_COUNT=$((MISSING_COUNT + 1))
            MISSING_PATHS+=("$doc: \`$path\`")
        fi
    done < <(grep -oP "$PATH_REGEX" "$doc" 2>/dev/null || true)
done

echo -e "${BLUE}扫描完成：${NC}"
echo -e "  文档数：${#DOCS[@]}"
echo -e "  路径引用总数：$TOTAL_COUNT"
echo -e "  缺失路径数：$MISSING_COUNT"
echo ""

if [ $MISSING_COUNT -gt 0 ]; then
    echo -e "${RED}❌ 发现缺失路径：${NC}"
    for entry in "${MISSING_PATHS[@]}"; do
        echo -e "  ${RED}•${NC} $entry"
    done
    echo ""
    echo -e "${YELLOW}修复建议：${NC}"
    echo "  1. 更新文档中的过时路径引用"
    echo "  2. 如路径属设计示意，添加 VIRTUAL_PATHS 数组"
    echo "  3. 如需忽略某些文档，编辑 DOCS 数组"
    echo ""

    if [ "$STRICT" == "true" ]; then
        exit 1
    fi
else
    echo -e "${GREEN}✅ 所有路径引用都有效${NC}"
fi

# 同时检查 scripts/ 子目录中的脚本是否在 CMakeLists.txt 中注册
echo ""
echo -e "${BLUE}额外检查：scripts/ 子目录一致性${NC}"
SCRIPTS_CMAKE="scripts/CMakeLists.txt"
if [ -f "$SCRIPTS_CMAKE" ]; then
    echo "  scripts/CMakeLists.txt 存在"
    # 检查 5 个子目录是否都有 .gitkeep
    for sub in build test pipeline topology stats; do
        if [ -d "scripts/$sub" ] && [ ! -f "scripts/$sub/.gitkeep" ]; then
            echo -e "  ${YELLOW}⚠️  scripts/$sub/ 缺少 .gitkeep${NC}"
        fi
    done
else
    echo -e "  ${YELLOW}⚠️  scripts/CMakeLists.txt 不存在${NC}"
fi

exit 0