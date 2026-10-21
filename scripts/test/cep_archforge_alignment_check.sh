#!/usr/bin/env bash
# scripts/test/cep_archforge_alignment_check.sh
#   —— CEP ↔ ArchForge 接口对齐清单的**漂移检测**
#
# 背景(per docs/implementation/2026-10-21-cep-archforge-interface-alignment.md §7):
#   该清单引用的 ArchForge 接口名 / 字段名会随 ArchForge 仓演进而漂移。
#   本检查做两件事:
#     ① **引用完整性** —— 清单里每一处 ArchForge `file:line` 仍存在且仍含预期符号;
#     ② **契约侧锚点** —— 清单依赖的 CppTLM 侧契约符号仍在(防 CppTLM 改了契约、清单没跟)。
#
# ⚠️ **ArchForge 仓不在本仓环境时** → 检查**优雅跳过**(打印「跳过」,退出 0):
#    跨仓路径是外部条件,缺失不是本仓的缺陷。
#
# 用法:
#   bash scripts/test/cep_archforge_alignment_check.sh
#   ARCHFORGE_DIR=/path/to/ArchForge bash scripts/test/cep_archforge_alignment_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

CHECKLIST="docs/implementation/2026-10-21-cep-archforge-interface-alignment.md"
ARCHFORGE="${ARCHFORGE_DIR:-/workspace/main/ArchForge}"

if [ ! -f "$CHECKLIST" ]; then
  echo "  ✗ 找不到对齐清单: $CHECKLIST"
  exit 1
fi

if [ ! -d "$ARCHFORGE" ]; then
  echo "  · 跳过:ArchForge 仓不在本环境($ARCHFORGE)—— 跨仓引用完整性无法在本仓校验"
  echo "    (设置 ARCHFORGE_DIR=<路径> 可启用)"
  exit 0
fi

python3 - "$CHECKLIST" "$ROOT" "$ARCHFORGE" << 'PYEOF'
import json, os, re, sys

checklist, root, archforge = sys.argv[1], sys.argv[2], sys.argv[3]

text = open(os.path.join(root, checklist), encoding="utf-8").read()
m = re.search(r"<!-- CEP_ALIGNMENT_ANCHORS_BEGIN -->(.*?)<!-- CEP_ALIGNMENT_ANCHORS_END -->",
              text, re.S)
if not m:
    print("  \033[0;31m✗\033[0m 对齐清单缺 CEP_ALIGNMENT_ANCHORS 机器可读块")
    sys.exit(1)
block = re.search(r"```json\s*(\{.*?\})\s*```", m.group(1), re.S)
if not block:
    print("  \033[0;31m✗\033[0m CEP_ALIGNMENT_ANCHORS 块不是合法 JSON")
    sys.exit(1)
data = json.loads(block.group(1))

bad = []
checked = 0

# ① ArchForge 侧引用完整性
for a in data.get("archforge", []):
    path = os.path.join(archforge, a["file"])
    if not os.path.exists(path):
        bad.append(f"[ArchForge] 文件不存在: {a['file']}")
        continue
    lines = open(path, encoding="utf-8").read().splitlines()
    ln = a["line"]
    if ln < 1 or ln > len(lines):
        bad.append(f"[ArchForge] {a['file']}:{ln} 行号越界(共 {len(lines)} 行)")
        continue
    checked += 1
    if a["symbol"] not in lines[ln - 1]:
        bad.append(f"[ArchForge] {a['file']}:{ln} 不再含符号 '{a['symbol']}'"
                   f"(现为: {lines[ln-1].strip()[:70]})")

# ② CppTLM 侧契约锚点
for a in data.get("cpptlm", []):
    path = os.path.join(root, a["file"])
    if not os.path.exists(path):
        bad.append(f"[CppTLM] 文件不存在: {a['file']}")
        continue
    body = open(path, encoding="utf-8").read()
    checked += 1
    if a["symbol"] not in body:
        bad.append(f"[CppTLM] {a['file']} 不再含符号 '{a['symbol']}'")
    elif "line" in a:
        lines = body.splitlines()
        ln = a["line"]
        if ln < 1 or ln > len(lines):
            bad.append(f"[CppTLM] {a['file']}:{ln} 行号越界(共 {len(lines)} 行)")
        elif a["symbol"] not in lines[ln - 1]:
            bad.append(f"[CppTLM] {a['file']}:{ln} 不再含符号 '{a['symbol']}'"
                       f"(现为: {lines[ln-1].strip()[:70]})")

if bad:
    for b in bad:
        print(f"  \033[0;31m✗\033[0m {b}")
    print(f"  \033[0;31m✗\033[0m 对齐清单漂移检测失败:{len(bad)} 处")
    sys.exit(1)
print(f"  \033[0;32m✓\033[0m 对齐清单漂移检测通过:{checked} 个锚点全部有效"
      f"(ArchForge {len(data.get('archforge', []))} / CppTLM {len(data.get('cpptlm', []))})")
PYEOF
exit $?
