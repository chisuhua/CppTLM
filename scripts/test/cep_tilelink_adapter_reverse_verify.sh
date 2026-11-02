#!/usr/bin/env bash
# scripts/test/cep_tilelink_adapter_reverse_verify.sh
#   —— 队列 #11 `cpptlm-cep-tilelink-tree-manager` 的**反向验证**
#      (毒化源文件 ⇒ 判据必红;干净载体 ⇒ 必绿)
#
# 行为(per delta spec TMA-01 ~ TMA-10 + tasks 3.3.4):
#   ① **基线正控**:干净载体必须编译并通过探针(⇒ 探针本身有鉴别力);
#   ② 逐条**毒化**载体 / 声明源 ⇒ 探针**必须失败**;
#   ③ 失败原因 SHALL 命中**期望的判据 id**(不给「反正红了」放行)。
#
# 机制:每条 fixture 把**被毒化的那一个文件**复制到影子目录
#      (`$TMP/shadow/<原路径>`)⇒ 编译探针时 `-I $TMP/shadow` **优先于** `-I include`
#      ⇒ 探针吃到的是**毒化副本**,而仓内文件**零改动**。
#
# 用法:bash scripts/test/cep_tilelink_adapter_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_tilelink_adapter_probe.cc"
CXX="${CXX:-g++}"

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }

# ── fixture 定义 ─────────────────────────────────────────────────────────
# 格式:id:::file:::old:::new:::expected-id
#   · file = 相对仓根的路径(空 ⇒ 不改文件,只跑「期望失败」的语义类 fixture)
#   · old / new = 该文件内**必须恰好命中 1 次**的替换对(空 old ⇒ 新建整份影子文件)
#   · expected-id = 探针失败时**必须命中**的判据 id 片段
FIXTURES=(
  "adapter-state-literal:::include/tlm/cep/tilelink_c_adapter.hh:::        using CellKind = TileLinkStateSpec::CellKind;:::        using CellKind = TileLinkStateSpec::CellKind;@NL@        static constexpr int kProbe = static_cast<int>(TileLinkStateSpec::State::Nothing);:::impl:TMA-01-ZERO-COPY"
  "adapter-forged-table:::include/tlm/cep/tilelink_c_adapter.hh:::    static constexpr std::size_t kMessageClassCount = 4;:::    static constexpr std::size_t kMessageClassCount = 4;@NL@    static constexpr int kTransitions[4][3] = {};:::impl:TMA-01-ZERO-COPY"
  "adapter-mapping-row-dropped:::include/tlm/cep/tilelink_c_adapter.hh:::            {\"response\", bundles::NoCFlitBundle::CATEGORY_RESPONSE},:::            {\"response_x\", bundles::NoCFlitBundle::CATEGORY_RESPONSE},:::impl:TMA-03-MAP6TO3"
  "adapter-identity-flipped:::include/tlm/cep/tilelink_c_adapter.hh:::        static constexpr bool kMappingIsIdentity = false;:::        static constexpr bool kMappingIsIdentity = true;:::impl:TMA-03-MAP6TO3"
  "adapter-protocol-msg-drift:::include/tlm/cep/tilelink_c_adapter.hh:::{\"GrantAck\", \"response\"},:::{\"GrantAck\", \"snoop_resp\"},:::impl:TMA-03-PROTOMSG"
  "adapter-protocol-msg-dropped:::include/tlm/cep/tilelink_c_adapter.hh:::{\"ReleaseAck\", \"response\"},:::{\"ReleaseAck_x\", \"response\"},:::impl:TMA-03-PROTOMSG"
  "tracker-caps-drift:::include/tlm/cep/tilelink_tree_manager.hh:::            caps = kTrackerTraits[3].caps;:::            caps = kTrackerTraits[0].caps;:::impl:TMA-04-CAPS"
  "tracker-escalate-downgraded:::include/tlm/cep/tilelink_tree_manager.hh:::            result = LookupResult::ESCALATE_UP;:::            result = LookupResult::ANSWER_HERE;:::impl:TMA-05-ESCALATE"
  "tracker-chain-downgraded:::include/tlm/cep/tilelink_tree_manager.hh:::                hazard = behavior.hazard_under_contention; // == CHAIN(取自 traits,非字面量):::                hazard = LookupHazard::STALL;:::impl:TMA-05-CHAIN"
  "join-remaining-not-extended:::include/tlm/cep/sharer_tracker.hh:::        fit->second.remaining |= added;:::        fit->second.remaining |= added & responder_set_t{0}; /* 毒化:不并入 remaining */:::impl:TMA-06-JOIN"
  "join-no-unmapped-reject:::include/tlm/cep/sharer_tracker.hh:::        if ((extra_responders & ~kMappedResponderMask) != 0):::        if (false && (extra_responders & ~kMappedResponderMask) != 0):::impl:TMA-06-JOIN"
  "decl-perm-unique-added:::include/tlm/cep/protocol_instances.json:::        \"NONE\",@NL@        \"READ\",@NL@        \"WRITEBACK\"@NL@      ],:::        \"NONE\",@NL@        \"READ\",@NL@        \"UNIQUE\",@NL@        \"WRITEBACK\"@NL@      ],:::decl:TMA-08-INSTANCE"
  "decl-vc-writeback:::include/tlm/cep/protocol_instances.json:::          \"request\": 0,@NL@          \"snoop\": 1,@NL@          \"snoop_resp\": 2,@NL@          \"response\": 3@NL@        }:::          \"request\": 0,@NL@          \"snoop\": 1,@NL@          \"snoop_resp\": 2,@NL@          \"response\": 3,@NL@          \"writeback\": 0@NL@        }:::decl:TMA-08-INSTANCE"
)

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/tl_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test" -I "$ROOT/external/json")

echo ""
echo "═══ 基线:干净载体必须编译并通过探针(正控)═══"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/base_build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/base_build.log" | head -20; exit 1
fi
if ! "$TMP/probe" "$ROOT" >"$TMP/base.out" 2>&1; then
    echo "  ✗ **干净载体未通过探针** ⇒ 探针本身有问题:"; sed 's/^/      /' "$TMP/base.out" | head -20; exit 1
fi
ok "基线:干净载体编译通过 + 探针全绿"

echo ""
echo "═══ 逐条毒化 ⇒ 探针必须失败(且失败原因命中期望判据)═══"
for fx in "${FIXTURES[@]}"; do
    # ⚠️ 分隔符用 `:::`(多字符)—— 单字符 `|` 会被锚点里的 `|=` 截断
    id="${fx%%:::*}"; _r="${fx#*:::}"; file="${_r%%:::*}"; _r="${_r#*:::}"
    old="${_r%%:::*}"; _r="${_r#*:::}"; new="${_r%%:::*}"; expected="${_r#*:::}"
    SHADOW="$TMP/shadow_$id"
    rm -rf "$SHADOW"
    # ⚠️ **整目录**复制 `include/tlm/cep/` ⇒ 探针既能吃到**被毒化**的头,
    #    也能读到 JSON 声明源(未被毒化的文件照常解析)⇒ 数据面 fixture 也可判红
    mkdir -p "$SHADOW/include/tlm"
    cp -r "$ROOT/include/tlm/cep" "$SHADOW/include/tlm/cep"
    if [ -n "$old" ]; then
        python3 - "$ROOT/$file" "$SHADOW/$file" "$old" "$new" <<'PY'
import sys
src, dst, old, new = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
text = open(src, encoding='utf-8').read()
old = old.replace('@NL@', chr(10))
new = new.replace('@NL@', chr(10))
n = text.count(old)
if n != 1:
    print(f"ANCHOR-HIT:{n}")
    sys.exit(3)
open(dst, 'w', encoding='utf-8').write(text.replace(old, new, 1))
PY
        if [ $? -eq 3 ]; then
            bad "$id —— 毒化锚点**未恰好命中 1 次** ⇒ fixture 本身失效"
            continue
        fi
    else
        cp "$ROOT/$file" "$SHADOW/$file"
    fi
    if ! "$CXX" -std=c++17 -I "$SHADOW/include" "${INCLUDES[@]}" -o "$TMP/p" "$PROBE_SRC" 2>"$TMP/b.log"; then
        # 编译期红也算「被抓到」—— 但必须命中期望的 static_assert 文案 ⇒ 本探针无编译期锁 ⇒ 记失败
        bad "$id —— 毒化后**编译失败**(期望是运行期判据红)"
        continue
    fi
    if "$TMP/p" "$SHADOW" >"$TMP/o.txt" 2>&1; then
        bad "$id —— 毒化后探针**仍绿** ⇒ 判据无鉴别力"
        continue
    fi
    if ! grep -q "$expected" "$TMP/o.txt"; then
        bad "$id —— 探针红了但**失败原因未命中**期望判据($expected)"
        sed 's/^/      /' "$TMP/o.txt" | head -5
        continue
    fi
    ok "$id ⇒ 探针红且归因命中($expected)"
done

echo ""
if [ "$fail" -gt 0 ]; then
    echo "✗ 反向验证失败:$fail 条(共 $((pass + fail)) 条)"
    for m in "${fail_msgs[@]}"; do echo "    · $m"; done
    exit 1
fi
echo "✓ 反向验证全部通过:$pass 条"
