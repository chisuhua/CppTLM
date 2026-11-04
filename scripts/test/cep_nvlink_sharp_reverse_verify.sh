#!/usr/bin/env bash
# scripts/test/cep_nvlink_sharp_reverse_verify.sh
#   —— 队列 #13 **实现层** change `cpptlm-cep-nvlink-sharp` 的**反向验证**
#
# 行为(per delta spec NVA-01 ~ NVA-09 + tasks Phase 3.2 ~ 3.5):
#   ① **基线正控**:干净副本必须编译通过并通过探针(⇒ 探针本身有鉴别力);
#   ② 逐条**毒化** ⇒ 判据**必须失败**;
#   ③ 失败原因 SHALL 命中**期望的判据 id**(**精确相等**,⛔ 不用前缀匹配 ——
#      否则 `CP-03` 会命中 `CP-03-VC-PRECONDITION`)。
#
# ⚠️ **形态 = 「毒化 ⇒ 判据必红」**(鉴别力)⇒ 与队列 #11 修**误报**的那一组
#    (「毒化 ⇒ 判据仍绿」= 回归防护)**形态相反** ⇒ 两组都须登记,避免混用。
#
# 机制:每条 fixture 把**被毒化的副本**放进影子目录 ⇒ 编译探针时 `-I $SHADOW/include`
#      **优先于** `-I include` ⇒ 探针吃到的是**毒化副本**,而仓内文件**零改动**。
#      探针的**源扫描**与**JSON 读取**都以影子根目录(argv[1])为基准 ⇒ 毒化副本被扫到。
#
# 用法:bash scripts/test/cep_nvlink_sharp_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_nvlink_sharp_probe.cc"
CXX="${CXX:-g++}"

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }
[ -d "include/tlm/cep" ] || { echo "  ✗ 缺 include/tlm/cep"; exit 1; }

# ── fixture 定义 ────────────────────────────────────────────────────────────
# 格式:id:::expected-id:::edit1###edit2
#   edit = 相对路径@@old@@new
#   · old / new = 该文件内**必须恰好命中 1 次**的替换对
#   · `###` 分隔同一条 fixture 的多个编辑(⇒ 允许**跨文件同时毒化**)
# ⚠️ 分隔符用多字符(`:::` / `@@` / `###`)—— 单字符会被锚点里的含该字符文本截断
FIXTURES=(
  # F-1 —— adapter 注入**成员字面量** ⇒ 零副本判据红
  "f-01-member-literal:::NVA-01:::include/tlm/cep/nvlink_adapter.hh@@    static constexpr const char* kAdapterName = \"NvlinkAdapter\";@@    static constexpr const char* kAdapterName = \"NvlinkAdapter\";\n\n            // ⚠️ 毒化:成员字面量\n            static constexpr nvlink_sharp::IngressDecision kPoison = nvlink_sharp::IngressDecision::Pass;"

  # F-2 —— adapter 加 `transition_*` ⇒ 无转移表判据红
  "f-02-transition:::NVA-01:::include/tlm/cep/nvlink_adapter.hh@@            static constexpr bool kReduceCodeTieUnspecified =@@            static constexpr bool transition_x() noexcept { return false; }\n\n            static constexpr bool kReduceCodeTieUnspecified ="

  # F-3 —— tracker **硬编码** caps + traits 行改 CONSERVATIVE ⇒
  #        「可见 caps 必须跟随 traits」判据红(⚠️ 单一毒化**抓不到** ——
  #        `SharerTrackerCaps{}` 默认值与 traits 行同值 ⇒ 必须**同时**毒化两处)
  "f-03-caps-hardcoded:::NVA-02:::include/tlm/cep/broadcast_null_tracker.hh@@            caps = traits()->caps;@@            caps = SharerTrackerCaps{};###include/tlm/cep/sharer_tracker.hh@@    {TrackerImpl::BroadcastNull, \"BroadcastNull\",\n     {Scope::NONE, Completeness::EXACT, Authority::NONE, AggregationAt::NONE},@@    {TrackerImpl::BroadcastNull, \"BroadcastNull\",\n     {Scope::NONE, Completeness::CONSERVATIVE, Authority::NONE, AggregationAt::NONE},"

  # F-4 —— 实例声明删 1 键 ⇒ 13 键齐备判据红
  "f-04-instance-keys:::NVA-03:::include/tlm/cep/protocol_instances.json@@      \"id\": \"nvlink_4\",\n      \"family\": \"industrial\",@@      \"id\": \"nvlink_4\","

  # F-5 —— V3 两向**判成同一类** ⇒ barrier 分类判据红
  "f-05-barrier-class:::CP-BARRIER-CLASS:::include/tlm/cep/collective_plane.hh@@    return litmus_1_ordering ? BarrierKeyClass::Ordering : BarrierKeyClass::Collective;@@    return BarrierKeyClass::Ordering;"

  # F-6 —— `ReduceOp` **追加**第 7 个值 ⇒ 源文本计数判据红
  #        (⚠️ 「最大值 + 1」式计数**抓不到** ⇒ 必须用**枚举项计数**)
  "f-06-reduceop-7:::NVA-05:::include/tlm/cep/collective_plane.hh@@    BitXor = 5,\n};@@    BitXor = 5,\n    Prod = 6,\n};"

  # F-7 —— 移除 `group_destroy` 的 Fault 产出 ⇒ partial failure 判据红
  "f-07-fault-removed:::CP-PARTIAL-FAIL:::include/tlm/cep/collective_plane.hh@@                for (std::uint32_t m : members)\n                    events_.push_back(CollectiveEvt::make_fault(gid, m, Status::InvalidGroup));@@                for (std::uint32_t m : members)\n                    (void)m;"

  # F-8 —— 已注册 region 的 `mode` 改成 `normal` ⇒ R3 降级判据红
  "f-08-binding-mode:::NVA-07:::include/tlm/cep/collective_planes.json@@      \"mode\": \"nc_drop_cacheable\"@@      \"mode\": \"normal\""

  # F-9 —— 绑定一个**非本地模型**的 plane 名(`FabricCollectivePlane`,2026-11-04 起已登记)
  #      ⇒ V6-c 前提约束红(⛔ 不是「未注册」)
  #      ⚠️ **登记缺口(2026-11-04,critic 第 2 轮 R3)**:V6-c 的 `find_collective_plane() == nullptr`
  #      分支(「绑定**未注册**的 plane 名」)现**无**反向 fixture 触达 —— 本 fixture 在队列 #14 登记
  #      `FabricCollectivePlane` 后语义已漂移为「非本地模型」⇒ 该缺口记入台账 backlog(⛔ 不在本轮补)
  "f-09-v6c-unregistered:::CP-03-VC-PRECONDITION:::include/tlm/cep/collective_planes.json@@    \"nvlink_4\": \"LocalCollectivePlane\"@@    \"nvlink_4\": \"FabricCollectivePlane\""
)

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/nvsharp_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/external/json")

echo ""
echo "═══ 基线:干净副本必须编译并通过探针(正控)═══"
mkdir -p "$TMP/shadow"
cp -r "$ROOT/include" "$TMP/shadow/include"
if ! "$CXX" -std=c++17 -I "$TMP/shadow/include" "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" \
        2>"$TMP/base_build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/base_build.log" | head -20; exit 1
fi
if ! "$TMP/probe" "$TMP/shadow" >"$TMP/base.out" 2>&1; then
    echo "  ✗ **干净副本未通过探针** ⇒ 探针本身有问题:"; sed 's/^/      /' "$TMP/base.out" | head -20; exit 1
fi
ok "基线:干净副本编译通过 + 探针全绿"

echo ""
echo "═══ 逐条毒化 ⇒ 判据必须失败(且失败原因命中期望判据)═══"
for fx in "${FIXTURES[@]}"; do
    id="${fx%%:::*}"; _r="${fx#*:::}"
    expected="${_r%%:::*}"; edits="${_r#*:::}"

    SHADOW="$TMP/shadow_$id"
    rm -rf "$SHADOW"
    mkdir -p "$SHADOW"
    cp -r "$ROOT/include" "$SHADOW/include"

    # 逐个编辑做毒化(仓内文件**零改动**)
    # ⚠️ **编辑串的切分必须在 python 里做** —— bash 的多字符 IFS 会**逐字符**切分
    #    (`IFS='###'` ⇒ 空元素 ⇒ 毒化到目录上 ⇒ 崩溃),故此处一次性交给 python。
    python3 - "$SHADOW" "$edits" <<'PYEOF'
import sys
shadow, edits = sys.argv[1], sys.argv[2]
for edit in edits.split('###'):
    if not edit:
        continue
    rel, _rest = edit.split('@@', 1)
    old, new = _rest.split('@@', 1)
    old = old.replace(chr(92) + 'n', chr(10))
    new = new.replace(chr(92) + 'n', chr(10))
    path = shadow + '/' + rel
    text = open(path, encoding='utf-8').read()
    n = text.count(old)
    if n != 1:
        print(f"ANCHOR-HIT:{n}:{rel}")
        sys.exit(3)
    open(path, 'w', encoding='utf-8').write(text.replace(old, new, 1))
PYEOF
    if [ $? -eq 3 ]; then
        bad "$id —— 毒化锚点**未恰好命中 1 次** ⇒ fixture 本身失效"
        continue
    fi

    # 编译探针(影子优先)
    if ! "$CXX" -std=c++17 -I "$SHADOW/include" "${INCLUDES[@]}" -o "$TMP/p" "$PROBE_SRC" \
            2>"$TMP/b.log"; then
        # ① **编译期红**:归因 = 编译日志里的 `static_assert` 文案(**精确相等**)
        if grep -q "$expected" "$TMP/b.log"; then
            ok "$id ⇒ **编译期红**且归因命中($expected)"
        else
            bad "$id —— 编译失败但**归因未命中**期望判据($expected)"
            grep -E "error:|static assertion" "$TMP/b.log" | sed 's/^/      /' | head -4
        fi
        continue
    fi

    # ② **运行期红**
    if "$TMP/p" "$SHADOW" >"$TMP/o.txt" 2>&1; then
        bad "$id —— 毒化后探针**仍绿** ⇒ 判据无鉴别力"
        continue
    fi
    # ⚠️ **精确相等**:不得用子串前缀(否则 `CP-03` 会命中 `CP-03-VC-PRECONDITION`)
    if ! grep -qx "PROBE:FAIL $expected .*" "$TMP/o.txt"; then
        bad "$id —— 探针红了但**归因未命中**期望判据($expected)"
        sed 's/^/      /' "$TMP/o.txt" | head -4
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
