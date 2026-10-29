#!/usr/bin/env bash
# scripts/test/generate_mesif_recognition_checklist.sh
#   —— 按规范性载体**重新生成**MESIF 逐格追认清单的三张表
#
# 背景(per critic 第 1 轮 SHOULD_FIX ③):清单此前宣称「由载体生成」,但仓内**无生成器**
#   ⇒ 实为人工产出 + MS-06 机器核对(措辞超出实际)⇒ 本脚本把该宣称**兑现**为可复现的。
#
# 行为:
#   ① 编译并运行 `scripts/generate_mesif_recognition_checklist.cc`(读载体 `kTable`);
#   ② 把三张表**就地替换**进清单文档的标记块
#      (`<!-- MESIF-TABLE:<name>:START -->` … `<!-- MESIF-TABLE:<name>:END -->`);
#   ③ **只改标记块内的内容** ⇒ 人工维护的散文段落**不动**。
#
# ⚠️ 生成后必须跑 `test_cep_mesif_state_spec.cc`(MS-06)确认清单与载体逐格一致。
#
# 用法:bash scripts/generate_mesif_recognition_checklist.sh [--check]
#   `--check`:只比对不写文件(有差异 ⇒ rc=1)—— 用于 CI / 提交前自检
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

GEN_SRC="scripts/test/generate_mesif_recognition_checklist.cc"
DOC="docs/implementation/2026-10-29-cep-mesif-state-spec-recognition-checklist.md"
CXX="${CXX:-g++}"
CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

if [ ! -f "$DOC" ]; then
    echo "✗ 清单文档不存在: $DOC"; exit 1
fi

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/mesif_gen.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

if ! "$CXX" -std=c++17 -I "$ROOT/include" -o "$TMP/gen" "$GEN_SRC" 2>"$TMP/build.log"; then
    echo "✗ 生成器构建失败:"; sed 's/^/   /' "$TMP/build.log" | head -20; exit 1
fi
"$TMP/gen" >"$TMP/tables.txt" || { echo "✗ 生成器运行失败"; exit 1; }

python3 - "$DOC" "$TMP/tables.txt" "$CHECK_ONLY" <<'PY'
import sys
doc_path, tables_path, check_only = sys.argv[1], sys.argv[2], sys.argv[3] == '1'

tables = {}
cur = None
for line in open(tables_path, encoding='utf-8').read().split('\n'):
    if line.startswith('===MESIF:'):
        cur = line[len('===MESIF:'):-len('===')]
        tables[cur] = []
    elif cur is not None and line.strip():
        tables[cur].append(line)

doc = open(doc_path, encoding='utf-8').read()
changed = []
for name, rows in tables.items():
    start = f'<!-- MESIF-TABLE:{name}:START -->'
    end = f'<!-- MESIF-TABLE:{name}:END -->'
    i = doc.find(start)
    j = doc.find(end)
    if i < 0 or j < 0:
        print(f'✗ 清单缺少标记块: {name}')
        sys.exit(2)
    block = start + '\n' + '\n'.join(rows) + '\n' + end
    new_doc = doc[:i] + block + doc[j + len(end):]
    if new_doc != doc:
        changed.append(name)
        doc = new_doc

if check_only:
    if changed:
        print(f'✗ 清单与载体**不一致**(需重新生成): {changed}')
        sys.exit(1)
    print('✓ 清单与载体一致(--check 通过)')
    sys.exit(0)

if changed:
    open(doc_path, 'w', encoding='utf-8').write(doc)
    print(f'✓ 已重新生成:{changed} ⇒ {doc_path}')
else:
    print('✓ 清单已是最新(无需改动)')
PY
rc=$?
[ $rc -eq 0 ] || exit $rc

if [ "$CHECK_ONLY" -eq 0 ]; then
    echo "⚠️ 请跑:./build/bin/cpptlm_tests \"[cep][mesif-state-spec]\" 确认 MS-06 通过"
fi
