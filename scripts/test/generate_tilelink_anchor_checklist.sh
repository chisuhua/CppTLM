#!/usr/bin/env bash
# scripts/test/generate_tilelink_anchor_checklist.sh
#   —— 按**规范性载体**重新生成 TileLink-C 规约的 12 格逐格核对表 + 锚点统计块,
#      并对**锚点摘要**做**逐格交叉核对**(basis 唯一人工审计面)
#
# 行为(per delta spec `TSS-07` 子条件 ④ / ⑥ + tasks 3.3 / 3.4):
#   ① 编译并运行 `scripts/test/generate_tilelink_anchor_checklist.cc`(读载体 `kTransitions`);
#   ② **逐格交叉核对**:每格 `ref` SHALL 解析到锚点摘要 §2.3bis 的真实条目号;
#      每行的目标掩码与分桶 SHALL 与载体实算一致
#      ⇒ 手改任一侧(载体或摘要)⇒ **必须因正确原因红**(生成器 rc≠0);
#   ③ 把两个标记块**就地替换**:
#      · 核对表文档 `docs/implementation/2026-11-02-cep-tilelink-state-spec-anchor-checklist.md`
#        的 `<!-- TL-TABLE:MATRIX:START/END -->`;
#      · 锚点摘要 `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`
#        的 `<!-- TL-TABLE:STATS:START/END -->`。
#   ④ **只改标记块内的内容** ⇒ 人工维护的散文段落**不动**。
#
# ⚠️ **ID 文法不得照抄 CXL 版**(per 台账 Entry #141 / 二审 finding B-7):
#    本脚本以 `-I test` 引入 `test/cep_anchor_ids.hh`(容忍 Markdown 粗体 + 接受 `R-T<n>` 前缀)
#    ⇒ 合法 ID 集合**非空** ⇒ 逐格校验**不是**空转。
#
# 用法:bash scripts/test/generate_tilelink_anchor_checklist.sh [--check]
#   `--check`:只比对不写文件(有差异 ⇒ rc=1)—— 用于 CI / 提交前自检
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

GEN_SRC="scripts/test/generate_tilelink_anchor_checklist.cc"
DOC="docs/implementation/2026-11-02-cep-tilelink-state-spec-anchor-checklist.md"
SUMMARY="docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md"
CXX="${CXX:-g++}"
CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

for f in "$GEN_SRC" "$DOC" "$SUMMARY"; do
    if [ ! -f "$f" ]; then
        echo "✗ 文件不存在: $f"; exit 1
    fi
done

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/tl_gen.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

# ⚠️ `-I test`:复用 `test/cep_anchor_ids.hh` ⇒ 生成器与 ctest 判据**同源**
if ! "$CXX" -std=c++17 -I "$ROOT/include" -I "$ROOT/test" -o "$TMP/gen" "$GEN_SRC" 2>"$TMP/build.log"; then
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
    if line.startswith('===TL:'):
        cur = line[len('===TL:'):-len('===')]
        tables[cur] = []
    elif cur is not None and line.strip():
        tables[cur].append(line)

targets = {'MATRIX': doc_path, 'STATS': summary_path}
docs = {p: open(p, encoding='utf-8').read() for p in (doc_path, summary_path)}
changed = []
for name, path in targets.items():
    rows = tables.get(name)
    if rows is None:
        print(f'✗ 生成器未输出块 {name}'); sys.exit(1)
    new_block = '\n'.join(rows)
    start = f'<!-- TL-TABLE:{name}:START -->'
    end = f'<!-- TL-TABLE:{name}:END -->'
    text = docs[path]
    si = text.find(start)
    ei = text.find(end)
    if si < 0 or ei < 0:
        print(f'✗ {path} 缺标记块 {name}'); sys.exit(1)
    head = text[:si + len(start)]
    tail = text[ei:]
    new_text = head + '\n' + new_block + '\n' + tail
    docs[path] = new_text
    if new_text != text:
        changed.append(path)

if check_only:
    if changed:
        for p in changed:
            print(f'✗ 标记块内容与生成器输出不一致(未写文件): {p}')
        sys.exit(1)
    print('✓ 标记块与生成器输出一致(--check)')
    sys.exit(0)

for path, text in docs.items():
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
print(f'✓ 已更新 {len(changed)} 个文件的标记块')
PY
rc=$?
[ $rc -ne 0 ] && exit $rc

echo "✓ TileLink 锚点核对表生成器通过"
