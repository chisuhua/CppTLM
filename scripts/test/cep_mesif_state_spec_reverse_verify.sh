#!/usr/bin/env bash
# scripts/test/cep_mesif_state_spec_reverse_verify.sh
#   —— 证明 MESIF 规约判据(change `cpptlm-cep-mesif-state-spec` 的 MS-01 ~ MS-05)**有牙**
#      的反向验证
#
# 背景(per 判据纪律):「纯负向断言必须有正控」+「编译期锁不得只以『编译红』充当 ctest 判据」
#   ⇒ 每条判据都要有一个**可执行的反向 fixture**:破坏它 ⇒ 对应守卫**必须报红**。
#   本脚本做**毒化 → 必须失败 → 干净版必须通过**的闭环。
#
# ⚠️ **口径边界**(per change 的 design `D-ζ`):本脚本验证的是**一致性类**判据
#   (完备性 / 依据自洽 / 写规则 / 秩口径 / 偏离清单 / 统计)⇒ **语义值**本身的改动
#   (例如把 `M + SnpShared` 的目标态改掉)**不**由 ctest 兜住 —— 那是 **ArchForge 逐格追认**
#   的职责(per delta spec 的「判据口径边界」段)。⇒ 本脚本**刻意不**覆盖语义值漂移。
#
# ⚠️ 本脚本**只写自己的临时目录**,**不改**仓内任何文件 ⇒ 可安全进 ctest 并行运行。
#
# 用法:bash scripts/test/cep_mesif_state_spec_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

HDR="include/tlm/cep/mesif_state_spec.hh"
PROBE_SRC="scripts/test/cep_mesif_state_spec_probe.cc"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_mesif_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test"
          -I "$ROOT/external/json")
pass=0; fail=0

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); }

if [ ! -f "$HDR" ]; then
    echo "  ✗ 规约载体不存在: $HDR"; exit 1
fi

# ── 毒化定义:每条 = (kind, 旧串, 新串)──────────────────────────────
# ⚠️ 旧串**必须唯一命中** —— 否则毒化是空操作 ⇒ 判据假绿(脚本会显式报错)。
MUTATIONS=(
  "basis_ref_empty|BasisKind::Structural, true, \"ArchForge 09 号 §3.3:150\",|BasisKind::Structural, true, \"\","
  "primary_marked_pending|BasisKind::Primary, false, \"ArchForge 25-nsa:553; ArchForge 32 号:529\",|BasisKind::Primary, true, \"ArchForge 25-nsa:553; ArchForge 32 号:529\","
  "derived_not_pending|BasisKind::Structural, true, \"ArchForge 09 号 §3.3:151\",|BasisKind::Structural, false, \"ArchForge 09 号 §3.3:151\","
  "primary_ref_without_anchor|BasisKind::Primary, false, \"ArchForge 25-nsa:553; ArchForge 32 号:529\",|BasisKind::Primary, false, \"参见设计文档\","
  "shared_write_mutates|{1, 2, 1, 2, 1, 0}, // S :|{1, 2, 2, 2, 1, 0}, // S :"
  "local_rank_drops|{2, 2, 3, 2, 4, 0}, // E :|{2, 0, 3, 2, 4, 0}, // E :"
  "transition_hole|{1, 2, 0, 0, 0, 0}, // I :|{1, 2, 0, 0, 0xFF, 0}, // I :"
  "deviation_not_real|{State::F, Event::SnpUnique, \"—\",|{State::F, Event::SnpUnique, \"I\","
)

echo ""
echo "═══ 基线:干净载体必须编译并通过探针(正控)═══"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/build.log" | head -20; exit 1
fi
ok "探针构建成功(干净载体)"

if "$TMP/probe" >"$TMP/base_run.log" 2>&1; then
    ok "基线:干净载体全部判据通过($(head -1 "$TMP/base_run.log"))"
else
    bad "基线:干净载体判据红 —— $(head -3 "$TMP/base_run.log" | tr '\n' ' ')"
fi

# ── 逐条毒化 → 必须失败 ────────────────────────────────────────────
run_mutation_expect_fail() {  # run_mutation_expect_fail <kind> <old> <new>
    local kind="$1" old="$2" new="$3"
    local shadow="$TMP/shadow/$kind/tlm/cep"
    mkdir -p "$shadow"
    cp "$HDR" "$shadow/mesif_state_spec.hh" || { bad "$kind 影子副本生成失败"; return; }

    python3 - "$shadow/mesif_state_spec.hh" "$old" "$new" <<'PY' 2>"$TMP/mut_$kind.log"
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(path, encoding='utf-8').read()
n = s.count(old)
if n != 1:
    print(f"旧串命中 {n} 次(必须恰好 1 次)—— 毒化是空操作或定位不唯一")
    sys.exit(3)
open(path, 'w', encoding='utf-8').write(s.replace(old, new, 1))
PY
    local mrc=$?
    if [ $mrc -ne 0 ]; then
        bad "$kind 毒化失败 —— $(cat "$TMP/mut_$kind.log")"
        return
    fi

    # 编译期锁报红 ⇒ 编译失败;运行期谓词为假 ⇒ 探针退出非 0 ⇒ 两者都算「被抓到」
    local log="$TMP/run_$kind.log" rc
    if ! "$CXX" -std=c++17 -I "$TMP/shadow/$kind" "${INCLUDES[@]:1}" \
            -o "$TMP/probe_$kind" "$PROBE_SRC" >"$log" 2>&1; then
        rc=1   # 编译失败(编译期锁红)
    else
        "$TMP/probe_$kind" >>"$log" 2>&1 || rc=1
        rc=${rc:-0}
    fi
    if [ "${rc:-0}" -eq 1 ]; then
        ok "$kind ⇒ 被抓到(编译期锁或运行期谓词报红)"
    else
        bad "$kind ⇒ **未被抓到** —— 判据无牙"
    fi
}

for m in "${MUTATIONS[@]}"; do
    IFS='|' read -r kind old new <<<"$m"
    echo ""
    echo "═══ 毒化:${kind} ═══"
    run_mutation_expect_fail "$kind" "$old" "$new"
done

echo ""
echo "══════════════════════════════════════"
echo "反向验证:${pass} 条通过 / ${fail} 条失败"
[ "$fail" -eq 0 ] || exit 1
