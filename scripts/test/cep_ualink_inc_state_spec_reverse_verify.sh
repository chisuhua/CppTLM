#!/usr/bin/env bash
# scripts/test/cep_ualink_inc_state_spec_reverse_verify.sh
#   —— 队列 #14 `cpptlm-cep-ualink-inc-state-spec` 的**反向验证**
#
# 行为(per delta spec UIS-01 ~ UIS-11 + tasks Phase 2.2 ~ 2.4 + design §4):
#   ① **基线正控**:干净载体必须编译通过**且**探针全绿(⇒ 探针本身有鉴别力);
#   ② **归因 id 双向覆盖自检**:本脚本的 fixture 期望 id 集合 ⟺ 载体的断言文案 id 集合;
#   ③ 逐条**毒化**载体 ⇒ 判据**必须失败**;
#   ④ 失败原因 SHALL 命中**期望的判据 id**(不给「反正红了」放行);
#   ⑤ **探针鉴别力自测**:验证运行期分支(探针)同样有牙。
#
# ⚠️ **本组形态 = 「毒化 ⇒ 判据必红」**(鉴别力)⇒ 与「毒化 ⇒ 判据仍绿」(回归防护)形态相反
#    ⇒ 两组都须登记,避免混用(per 队列 #13 的同款纪律)。
#
# ⚠️ **两类失败形态都支持**(per design §4):
#   ① **编译期红** ⇒ 载体的 `static_assert` 触发 ⇒ 归因 = 编译日志里的 `static assertion failed: <id>: `
#      (⚠️ **带尾冒号** —— `UIS-01` 是 `UIS-10` 的**前缀** ⇒ 裸子串会误命中);
#   ② **运行期红** ⇒ 探针的 `PROBE:FAIL <id> ` 输出(精确前缀)。
#   ⇒ 两分支都绿 ⇒ 判「**毒化后仍绿 ⇒ 无鉴别力**」⇒ 该组 fixture 失效。
#
# 机制:每条 fixture 把**被毒化的载体**放进影子目录 ⇒ 编译探针时 `-I $SHADOW/include` **优先于**
#      `-I include` ⇒ 探针吃到的是**毒化副本**,而仓内文件**零改动**。
#
# 用法:bash scripts/test/cep_ualink_inc_state_spec_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_ualink_inc_state_spec_probe.cc"
CARRIER="include/tlm/cep/ualink_inc_state_spec.hh"
CXX="${CXX:-g++}"

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }
[ -f "$CARRIER" ]   || { echo "  ✗ 载体不存在: $CARRIER"; exit 1; }

# ── fixture 定义 ────────────────────────────────────────────────────────────
# 格式:id:::carrier-relative-path:::old:::new:::expected-id
#   · old / new = 该文件内**必须恰好命中 1 次**的替换对
#   · expected-id = 失败时**必须命中**的判据 id(编译期:static_assert 文案前缀;
#                  运行期:探针输出的 `PROBE:FAIL <id> `)
# ⚠️ 分隔符用 `:::`(多字符)—— 单字符 `|` 会被锚点里的含 `|` 文本截断
# ⚠️ `@NL@` = 换行(脚本替换,避免 bash 里硬换行把数组元素拆断)
#
# ⚠️ **每组的毒化锚点 SHALL 指向载体中确实存在的翻转点**(per design §4)
FIXTURES=(
  # F-1 —— INC 原语数 4 → 5 ⇒ `kIncOpCount == 4u` 编译红 ⇒ 归因 `UIS-01`
  "f-01-inc-op-count:::${CARRIER}:::inline constexpr std::size_t kIncOpCount = 4;:::inline constexpr std::size_t kIncOpCount = 5;:::UIS-01"
  # F-2 —— 载体自带算子族(声明锁翻转)⇒ 编译红 ⇒ 归因 `UIS-02`
  "f-02-operator-family-lock:::${CARRIER}:::inline constexpr bool kOperatorFamilyDefinedInCarrier = false;:::inline constexpr bool kOperatorFamilyDefinedInCarrier = true;:::UIS-02"
  # F-3 —— partial failure 静默(谓词函数体翻转)⇒ 编译红 ⇒ 归因 `UIS-03`
  "f-03-partial-failure-silent:::${CARRIER}:::return !silent;:::return silent;:::UIS-03"
  # F-4 —— `UIS-04` 的格由 pending 冒充已定稿 ⇒ 编译红 ⇒ 归因 `UIS-04`
  "f-04-uis04-pending-to-selfderived:::${CARRIER}:::{Obligation::Uis04, \"N-3\", BasisKind::Pending},:::{Obligation::Uis04, \"N-3\", BasisKind::SelfDerived},:::UIS-04"
  # F-5 —— `UIS-05` 同上 ⇒ 归因 `UIS-05`
  "f-05-uis05-pending-to-selfderived:::${CARRIER}:::{Obligation::Uis05, \"N-4\", BasisKind::Pending},:::{Obligation::Uis05, \"N-4\", BasisKind::SelfDerived},:::UIS-05"
  # F-6 —— `UIS-06` 同上 ⇒ 归因 `UIS-06`
  "f-06-uis06-pending-to-selfderived:::${CARRIER}:::{Obligation::Uis06, \"N-5\", BasisKind::Pending},:::{Obligation::Uis06, \"N-5\", BasisKind::SelfDerived},:::UIS-06"
  # F-7 —— 「不一致即禁用 M-2」的判定失效(谓词函数体翻转)⇒ 编译红 ⇒ 归因 `UIS-07`
  "f-07-vc-inheritance-always-true:::${CARRIER}:::return all_instances_agree;:::return true;:::UIS-07"
  # F-8 —— `UIS-08` 的格**由已定稿冒充 pending** ⇒ 编译红 ⇒ 归因 `UIS-08`
  #   ⚠️ **2026-11-06 方向翻转**(per 台账 Entry #238 + `ADR-CEP-04` SU 2026-11-06):
  #   `Uis08` / `Uis09` 已由 `Pending` 转 `SelfDerived`(ref = `C-10`)⇒ 原「pending→selfderived」
  #   的毒化**不再能命中锚点**(载体里已无 `N-8` / `N-9` 格)⇒ fixture 必须**跟随载体状态反向**,
  #   否则反向验证会因「锚点 0 命中」而**误报 fixture 失效**(本轮实测 ANCHOR-HIT:0)。
  "f-08-uis08-selfderived-to-pending:::${CARRIER}:::{Obligation::Uis08, \"C-10\", BasisKind::SelfDerived},:::{Obligation::Uis08, \"C-10\", BasisKind::Pending},:::UIS-08"
  # F-9 —— `UIS-09` 同上 ⇒ 归因 `UIS-09`
  "f-09-uis09-selfderived-to-pending:::${CARRIER}:::{Obligation::Uis09, \"C-10\", BasisKind::SelfDerived},:::{Obligation::Uis09, \"C-10\", BasisKind::Pending},:::UIS-09"
  # F-10 —— `UIS-10`:`SelfDerived` 的 `ref` 冒充成公开来源 ⇒ 遍历式断言红 ⇒ 归因 `UIS-10`
  "f-10-basis-ref-public-impersonate:::${CARRIER}:::{Obligation::Uis01, \"C-2 C-3\", BasisKind::SelfDerived},:::{Obligation::Uis01, \"C-2 A-3\", BasisKind::SelfDerived},:::UIS-10"
  # F-11a —— `UIS-11`:把「重跑 DAG」当作证书(声明锁翻转)⇒ 编译红 ⇒ 归因 `UIS-11`
  "f-11a-dag-rerun-as-certificate:::${CARRIER}:::inline constexpr bool kDagRerunIsCertificate = false;:::inline constexpr bool kDagRerunIsCertificate = true;:::UIS-11"
  # F-11b —— `UIS-11`:证书表**缺一条**(C-4 行删除 ⇒ 第 4 项值初始化 ⇒ 齐备性 / 顺序断言红)⇒ 归因 `UIS-11`
  "f-11b-certificate-table-missing:::${CARRIER}:::{CertificateId::C4, \"/C-4\", \"未检验项登记:U-1 / U-2\", \"C-1\", kDomainRepoOwned},::::::UIS-11"
)

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/uis_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

INCLUDES=(-I "$ROOT/include")

# 影子副本:毒化载体(仓内文件**零改动**)
poison() {   # $1 = 影子根 · $2 = old · $3 = new
    python3 - "$1/$CARRIER" "$2" "$3" <<'PY'
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
old = old.replace('@NL@', chr(10))
new = new.replace('@NL@', chr(10))
text = open(path, encoding='utf-8').read()
n = text.count(old)
if n != 1:
    print(f"ANCHOR-HIT:{n}")
    sys.exit(3)
open(path, 'w', encoding='utf-8').write(text.replace(old, new, 1))
PY
}

echo ""
echo "═══ 基线:干净载体必须编译并通过探针(正控)═══"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/base_build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/base_build.log" | head -20; exit 1
fi
if ! "$TMP/probe" "$CARRIER" "$ROOT" >"$TMP/base.out" 2>&1; then
    echo "  ✗ **干净载体未通过探针** ⇒ 探针本身有问题:"; sed 's/^/      /' "$TMP/base.out" | head -20; exit 1
fi
ok "基线:干净载体编译通过 + 探针全绿($(grep -c 'PROBE:OK' "$TMP/base.out") 处)"

echo ""
echo "═══ 归因 id 双向覆盖自检(per tasks 2.4)═══"
# ① fixture 期望 id 集合
fixture_ids="$(printf '%s\n' "${FIXTURES[@]}" | sed 's/.*:::\([^:]*\)$/\1/' | sort -u)"
# ② 载体的断言文案 id 集合(取 `static_assert` 文案里的 `"UIS-xx: ` 前缀)
carrier_ids="$(grep -o '"UIS-[0-9][0-9]: ' "$CARRIER" | tr -d '" :' | sort -u)"
if [ "$fixture_ids" = "$carrier_ids" ]; then
    ok "归因 id 双向覆盖:fixture 期望集合 ⟺ 载体断言文案集合($(printf '%s\n' "$fixture_ids" | wc -l) 个)"
else
    bad "归因 id 双向覆盖不一致 —— fixture:$(echo "$fixture_ids" | tr '\n' ' ') | 载体:$(echo "$carrier_ids" | tr '\n' ' ')"
fi

echo ""
echo "═══ 逐条毒化 ⇒ 判据必须失败(且失败原因命中期望判据)═══"
for fx in "${FIXTURES[@]}"; do
    id="${fx%%:::*}"; _r="${fx#*:::}"
    file="${_r%%:::*}"; _r="${_r#*:::}"
    old="${_r%%:::*}"; _r="${_r#*:::}"
    new="${_r%%:::*}"; expected="${_r#*:::}"

    SHADOW="$TMP/shadow_$id"
    rm -rf "$SHADOW"
    # ⚠️ **整目录**复制 `include/tlm/cep/` ⇒ 影子里的其它头齐备
    mkdir -p "$SHADOW/include/tlm"
    cp -r "$ROOT/include/tlm/cep" "$SHADOW/include/tlm/cep"

    poison "$SHADOW" "$old" "$new"
    if [ $? -eq 3 ]; then
        bad "$id —— 毒化锚点**未恰好命中 1 次** ⇒ fixture 本身失效"
        continue
    fi

    SHADOW_CARRIER="$SHADOW/$file"
    # 编译探针(影子优先)
    if ! "$CXX" -std=c++17 -I "$SHADOW/include" "${INCLUDES[@]}" -o "$TMP/p" "$PROBE_SRC" \
            2>"$TMP/b.log"; then
        # ① **编译期红**:归因 = 编译日志里的 `static assertion failed: <id>: `
        #    ⚠️ **带尾冒号** ⇒ `UIS-01` 不会被 `UIS-10` 误命中
        if grep -q "static assertion failed: ${expected}: " "$TMP/b.log"; then
            ok "$id ⇒ **编译期红**且归因命中($expected)"
        else
            bad "$id —— 编译失败但**归因未命中**期望判据($expected)"
            grep -E "error:|static assertion" "$TMP/b.log" | sed 's/^/      /' | head -4
        fi
        continue
    fi

    # ② **运行期红**(探针)
    if "$TMP/p" "$SHADOW_CARRIER" "$ROOT" >"$TMP/o.txt" 2>&1; then
        bad "$id —— 毒化后探针**仍绿** ⇒ 判据无鉴别力"
        continue
    fi
    if ! grep -q "PROBE:FAIL ${expected} " "$TMP/o.txt"; then
        bad "$id —— 探针红了但**归因未命中**期望判据($expected)"
        sed 's/^/      /' "$TMP/o.txt" | head -4
        continue
    fi
    ok "$id ⇒ 探针红且归因命中($expected)"
done

echo ""
echo "═══ 探针鉴别力自测(运行期分支 ⇒ 探针必须红)═══"
# ⚠️ 载体**加一个项目内头 include** ⇒ 编译**仍绿**(不触发任何 static_assert)
#    ⇒ 只有**探针的禁项扫描**能观测到 ⇒ 证明运行期分支有牙。
#    ⚠️ **不计入** §4 的 11 组 fixture(那是「编译期红」组)⇒ 避免破坏「11 组 ⟺ 11 个归因 id」的 1:1。
SHADOW="$TMP/shadow_probe_selftest"
rm -rf "$SHADOW"; mkdir -p "$SHADOW/include/tlm"
cp -r "$ROOT/include/tlm/cep" "$SHADOW/include/tlm/cep"
poison "$SHADOW" '#include <cstdint>' '#include <cstdint>@NL@#include "tlm/cep/collective_plane.hh"'
if [ $? -eq 3 ]; then
    bad "probe-selftest —— 毒化锚点未恰好命中 1 次 ⇒ 自测本身失效"
else
    if ! "$CXX" -std=c++17 -I "$SHADOW/include" "${INCLUDES[@]}" -o "$TMP/p2" "$PROBE_SRC" \
            2>"$TMP/b2.log"; then
        bad "probe-selftest —— 毒化后**编译期就红了** ⇒ 该毒化点不属运行期分支"
    elif "$TMP/p2" "$SHADOW/$CARRIER" "$ROOT" >"$TMP/o2.txt" 2>&1; then
        bad "probe-selftest —— 毒化后探针**仍绿** ⇒ 探针无鉴别力"
    elif grep -q "PROBE:FAIL UIS-10 " "$TMP/o2.txt"; then
        ok "probe-selftest ⇒ 编译绿 + 探针红且归因命中(UIS-10)⇒ 运行期分支有牙"
    else
        bad "probe-selftest —— 探针红了但归因未命中(UIS-10)"
        sed 's/^/      /' "$TMP/o2.txt" | head -4
    fi
fi

echo ""
if [ "$fail" -gt 0 ]; then
    echo "✗ 反向验证失败:$fail 条(共 $((pass + fail)) 条)"
    for m in "${fail_msgs[@]}"; do echo "    · $m"; done
    exit 1
fi
echo "✓ 反向验证全部通过:$pass 条"
