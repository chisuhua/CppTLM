#!/usr/bin/env bash
# scripts/test/generate_cxl_anchor_checklist.sh
#   —— 按**规范性载体**重新生成 CXL.cache 规约的 68 格逐格核对表 + 摘要统计块,
#      并对**锚点摘要**做交叉核对(basis 唯一人工审计面)
#
# 行为(per delta spec `CSS-06` / 提案 `AC-3` / `AC-9`):
#   ① 编译并运行 `scripts/test/generate_cxl_anchor_checklist.cc`(读载体 `kTransitions`);
#   ② **交叉核对**:每格 `ref` SHALL 解析到锚点摘要 §8 的真实条目号;
#      每格 `note` 的 `R-n` SHALL 解析到摘要 §6 的真实编号
#      ⇒ 手改任一侧(载体或摘要)⇒ **必须因正确原因红**(生成器 rc≠0);
#   ③ 把两个标记块**就地替换**:
#      · 核对表文档 `docs/implementation/2026-11-01-cep-cxl-state-spec-anchor-checklist.md`
#        的 `<!-- CXL-TABLE:MATRIX:START/END -->`;
#      · 锚点摘要 `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`
#        的 `<!-- CXL-TABLE:STATS:START/END -->`。
#   ④ **只改标记块内的内容** ⇒ 人工维护的散文段落**不动**。
#
# 用法:bash scripts/test/generate_cxl_anchor_checklist.sh [--check]
#   `--check`:只比对不写文件(有差异 ⇒ rc=1)—— 用于 CI / 提交前自检
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

GEN_SRC="scripts/test/generate_cxl_anchor_checklist.cc"
DOC="docs/implementation/2026-11-01-cep-cxl-state-spec-anchor-checklist.md"
SUMMARY="docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md"
CXX="${CXX:-g++}"
CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

for f in "$GEN_SRC" "$DOC" "$SUMMARY"; do
    if [ ! -f "$f" ]; then
        echo "✗ 文件不存在: $f"; exit 1
    fi
done

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cxl_gen.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

if ! "$CXX" -std=c++17 -I "$ROOT/include" -o "$TMP/gen" "$GEN_SRC" 2>"$TMP/build.log"; then
    echo "✗ 生成器构建失败:"; sed 's/^/   /' "$TMP/build.log" | head -20; exit 1
fi

# ⚠️ 交叉核对失败 ⇒ **直接红**(不等标记块比对):这是「手改任一侧必红」的执行点
if ! "$TMP/gen" "$SUMMARY" >"$TMP/out.txt" 2>"$TMP/gen.err"; then
    echo "✗ 生成器交叉核对失败(载体 ↔ 摘要不一致):"
    grep -E "^CHECK:.*FAIL|^GEN:FAIL" "$TMP/out.txt" | sed 's/^/   /' | head -20
    sed 's/^/   /' "$TMP/gen.err" | head -10
    exit 1
fi

python3 - "$DOC" "$SUMMARY" "$TMP/out.txt" "$CHECK_ONLY" <<'PY'
import sys
doc_path, summary_path, out_path, check_only = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] == '1'

tables = {}
cur = None
for line in open(out_path, encoding='utf-8').read().split('\n'):
    if line.startswith('===CXL:'):
        cur = line[len('===CXL:'):-len('===')]
        tables[cur] = []
    elif cur is not None and line.strip():
        tables[cur].append(line)

targets = {'MATRIX': doc_path, 'STATS': summary_path}
docs = {p: open(p, encoding='utf-8').read() for p in (doc_path, summary_path)}
changed = []
for name, path in targets.items():
    rows = tables.get(name)
    if rows is None:
        print(f'✗ 生成器未产出表: {name}'); sys.exit(2)
    start = f'<!-- CXL-TABLE:{name}:START -->'
    end = f'<!-- CXL-TABLE:{name}:END -->'
    doc = docs[path]
    i = doc.find(start); j = doc.find(end)
    if i < 0 or j < 0:
        print(f'✗ 文档缺少标记块 {name}: {path}'); sys.exit(2)
    block = start + '\n' + '\n'.join(rows) + '\n' + end
    new_doc = doc[:i] + block + doc[j + len(end):]
    if new_doc != doc:
        changed.append(name)
        docs[path] = new_doc

if check_only:
    if changed:
        print(f'✗ 与载体**不一致**(需重新生成): {changed}'); sys.exit(1)
    print('✓ 核对表与摘要统计块均与载体一致(--check 通过)')
    sys.exit(0)

if changed:
    for path, text in docs.items():
        open(path, 'w', encoding='utf-8').write(text)
    print(f'✓ 已重新生成:{changed}')
    for name, path in targets.items():
        if name in changed:
            print(f'   · {name} ⇒ {path}')
else:
    print('✓ 已是最新(无需改动)')
PY
rc=$?
[ $rc -eq 0 ] || exit $rc

if [ "$CHECK_ONLY" -eq 0 ]; then
    echo '⚠️ 请跑:./build/bin/cpptlm_tests "[cep][cxl][state-spec]" 确认 CSS-06 通过'
fi
