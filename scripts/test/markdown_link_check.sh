#!/usr/bin/env bash
# scripts/test/markdown_link_check.sh
#
# Markdown 链接检查脚本 (per docs/architecture/ 治理 critic B6 BLOCKER)
#
# 检查 docs/ + AGENTS.md 中所有 markdown 相对链接是否有效.
#
# 与 docs_sync_check.sh 互补:
# - docs_sync_check.sh: 扫 4 核心文档中**代码路径** (.hh/.cc/.json/.py 等), 跳过 .md
# - markdown_link_check.sh: 扫 .md 中的**相对路径**引用, 验证路径有效
#
# 用法:
#   ./scripts/test/markdown_link_check.sh           # 检查 + 列出失效
#   ./scripts/test/markdown_link_check.sh --strict  # 不通过时 exit 1
#
# 维护: CppTLM build agent (per critic B6 BLOCKER, 2026-10-19)

set -e

STRICT=false
[ "$1" = "--strict" ] && STRICT=true

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

# 已知虚拟路径 (per docs_sync_check.sh VIRTUAL_PATHS + critic B6 扩展)
# 这些路径**预期**不存在 (已迁 ArchForge / 已归档 / 计划中 / 调研附属), 文档中保留作 reference
VIRTUAL_PATH_PATTERNS=(
    "src/noc/"
    "include/noc/"
    "cpptlm/visualization/editor/node_modules"
    "configs/example/apu_se.py"
    "configs/example/gpufs/Disjoint_VIPER.py"
    "src/dev/amdgpu/amdgpu_device.py"
    "docs/soc_arch/architecture/"
    "docs/soc_arch/adr/"
    "docs/soc_arch/modules/"
    "docs/soc_arch/specs/"
    "docs/soc_arch/roadmap/"
    "openspec/changes/cpptlm-dgpu-soc-internal-noc-e1/"
    "openspec/changes/cpptlm-pcie-host-test-flr-rebar/"
    "docs/architecture/01-hybrid-architecture-v2.md"
    "docs/architecture/05-packet-semantic-network.md"
    "docs/architecture/10-tgms-specifications.md"
    "docs/architecture/2026-05-12-unified-visualization-platform.md"
    "docs/architecture/fastapi-migration-plan.md"
    "docs/designs/dgpu-soc/pnn-protocol-neutral-network.md"
    "docs/architecture/14-pcie-tlp-wire-format.md"
    "docs/architecture/多层次混合仿真.md"
    "cpptlm_emulator_flr.h"
    "cpptlm_emulator_resizable_bar.h"
    "file:///"
    # 2026-10-27 硬件微架构调研迁 ArchForge 仓；归档文档（docs-archived / openspec archive）保留历史相对路径
    "research/SM/"
    "research/TMU/"
    "research/CP/"
    "research/WDU/"
    "research/WDUtoSM/"
    "research/PCIe/"
    "14-sm-operand-supply-design-target.md"
    "docs/research/"
    "docs/superpowers/plans/"
    "docs/pcie/"
)

# 收集所有 markdown 文件
MD_FILES=()
SCAN_PATHS=(
    "AGENTS.md"
    "docs/README.md"
    "docs/ONBOARDING.md"
    "docs/architecture/"
    "docs/designs/"
    "docs/adr/"
    "docs/archive/architecture/"
    "docs/archive/designs/"
)
for scan_path in "${SCAN_PATHS[@]}"; do
    if [ -d "$scan_path" ]; then
        while IFS= read -r f; do
            MD_FILES+=("$f")
        done < <(find "$scan_path" -maxdepth 5 -name "*.md" -type f 2>/dev/null | sort)
    elif [ -f "$scan_path" ]; then
        MD_FILES+=("$scan_path")
    fi
done

echo "📋 扫描 ${#MD_FILES[@]} 个 markdown 文件..."

# 用 Python 做链接提取 (避免 bash state machine bug)
BROKEN_LINKS=()
TOTAL_LINKS=0

for md_file in "${MD_FILES[@]}"; do
    # 调用 Python 脚本, 输出 "TOTAL BROKEN" 行 (供 bash 解析)
    output=$(python3 - "$md_file" << 'PYEOF'
import sys, re, os

md_file = sys.argv[1]
md_dir = os.path.dirname(md_file)

with open(md_file, 'r', encoding='utf-8') as f:
    content = f.read()

# 移除代码块 (```...```) 和 行内代码 (`...`)
# 简单状态机
in_codeblock = False
cleaned_lines = []
for line in content.split('\n'):
    if line.strip().startswith('```'):
        in_codeblock = not in_codeblock
        cleaned_lines.append('')
        continue
    if in_codeblock:
        cleaned_lines.append('')
    else:
        # 移除行内代码 `...`
        cleaned = re.sub(r'`[^`\n]*`', '', line)
        cleaned_lines.append(cleaned)

cleaned_content = '\n'.join(cleaned_lines)

# 提取 markdown 链接: [text](path) 或 [text](path "title")
pattern = re.compile(r'\[([^\]]+)\]\(([^\)]+)\)')
total = 0
broken = []
for match in pattern.finditer(cleaned_content):
    text = match.group(1)
    link_raw = match.group(2).strip()
    # 去掉 title (格式: path "title")
    link_path = re.sub(r'\s+["\'].*$', '', link_raw).strip()

    # 跳过网络链接 / 锚点 / 空
    if not link_path or link_path.startswith(('http://', 'https://', 'mailto:', '#', 'file://')):
        continue

    # 解析相对路径
    resolved = os.path.normpath(os.path.join(md_dir, link_path))
    abs_resolved = os.path.abspath(resolved)

    # 检查文件存在
    if os.path.exists(abs_resolved):
        continue
    # 尝试 .md 自动补全
    if os.path.exists(abs_resolved + '.md'):
        continue
    if os.path.exists(abs_resolved.rstrip('/') + '.md'):
        continue

    total += 1
    broken.append(f'{link_path} → {abs_resolved}')

print(f'{total}|{"||".join(broken)}')
PYEOF
    )

    # 解析输出
    total_count=$(echo "$output" | head -1 | cut -d'|' -f1)
    broken_section=$(echo "$output" | head -1 | cut -d'|' -f2-)

    TOTAL_LINKS=$((TOTAL_LINKS + total_count))

    if [ -n "$broken_section" ]; then
        IFS='||' read -ra links <<< "$broken_section"
        for link in "${links[@]}"; do
            BROKEN_LINKS+=("$md_file: $link")
        done
    fi
done

echo ""
echo "📊 总失效链接数: ${#BROKEN_LINKS[@]}"
echo "📊 总扫描链接数: $TOTAL_LINKS"
echo ""

if [ ${#BROKEN_LINKS[@]} -gt 0 ]; then
    echo "失效链接列表 (按文件分组):"
    for bl in "${BROKEN_LINKS[@]}"; do
        echo "  $bl"
    done
    echo ""
    if [ "$STRICT" = true ]; then
        echo "❌ markdown_link_check.sh --strict 失败 (exit 1)"
        exit 1
    else
        echo "⚠️  失效链接已列出 (默认模式不退出, 跑 --strict 强制 exit 1)"
    fi
else
    echo "✅ 所有 markdown 链接有效"
fi
