#!/usr/bin/env bash
# scripts/test/cep_ualink_inc_reverse_verify.sh
#   —— 队列 #14 **实现层** change `cpptlm-cep-ualink-inc` 的**反向验证**
#
# 行为(per delta spec UIA-01 ~ UIA-07 + `design §4` 的归因 id 登记):
#   ① **归因 id 双向覆盖自检**:fixture 的期望 id 集合 ⟺ 本 change 登记的 id 集合(**11 ↔ 11**);
#   ② **基线正控**:干净副本必须编译通过并通过探针(⇒ 探针本身有鉴别力);
#   ③ 逐条**毒化** ⇒ 判据**必须失败**且失败原因**精确命中**期望归因 id。
#
# ⚠️ **归因 id 纪律**(per `ADR-CEP-01` E.3.1):匹配用**完整前缀精确相等**
#   (⛔ 不用子串前缀匹配 —— 否则 `CP-03` 会命中 `CP-03-VC-PRECONDITION`)。
#
# 机制(照队列 #13 / #14 规约层先例):每条 fixture 把**被毒化的副本**放进影子目录 ⇒
#   编译探针时 `-I $SHADOW/include` **优先于** `-I include` ⇒ 探针吃到的是**毒化副本**,
#   而仓内文件**零改动**;探针的**文本扫描**与 **JSON 读取**都以影子根目录为基准。
#
# 用法:bash scripts/test/cep_ualink_inc_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_ualink_inc_probe.cc"
CXX="${CXX:-g++}"
INCLUDES=(-I external/json)

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }
[ -d "$ROOT/include/tlm/cep" ] || { echo "  ✗ 缺 include/tlm/cep"; exit 1; }

# ── fixture 定义 ──────────────────────────────────────────────────────────
# 格式:id:::expected-id:::edit1###edit2
#   edit = 相对路径@@old@@new
#   · old / new = 该文件内**必须恰好命中 1 次**的替换对
#   · `###` 分隔同一条 fixture 的多个编辑(⇒ 允许**跨文件同时毒化**)
# ⚠️ 分隔符用多字符(`:::` / `@@` / `###`)—— 单字符会被锚点里的含该字符文本截断
FIXTURES=(
  "f-01-member-literal:::UIA-01:::include/tlm/cep/ualink_adapter.hh@@    static constexpr const char* kAdapterName = \"UalinkAdapter\";@@    static constexpr const char* kAdapterName = \"UalinkAdapter\";\n            static constexpr const char* kForbidden = \"Obligation::\";"
  "f-02-transition:::UIA-02:::include/tlm/cep/ualink_adapter.hh@@    static constexpr const char* kProtocolVersion = \"ualink-2.0\";@@    static constexpr const char* kProtocolVersion = \"ualink-2.0\";\n            static constexpr bool transition_read_reduce() noexcept { return false; }"
  "f-03-data-path-not-implemented:::UIA-03:::include/tlm/cep/fabric_collective_plane.hh@@                for (std::uint32_t m : round.participants) {\n                    (void)m;\n                    events_.push_back(CollectiveEvt::make_result(req.group, req.tag, result));\n                }\n                events_.push_back(CollectiveEvt::make_completion(req.group, req.tag));@@                (void)result;\n                last_status_ = Status::DataPathNotImplemented;\n                rounds_.erase(rit);\n                round_gid_.erase(key);\n                return;"
  "f-04-c3-check-removed:::UIA-04:::include/tlm/cep/fabric_collective_plane.hh@@                if (group_has_inflight(gid)) {\n                    last_status_ = Status::ControlPlaneConflict;\n                    return Status::ControlPlaneConflict;\n                }@@                // (fixture: \`group_register_region\` 的 in-flight 检查已移除)"
  "f-05-status-third-value:::UIA-05:::include/tlm/cep/collective_planes.json@@    \"status\": \"shared_no_isolation\",@@    \"status\": \"shared\","
  "f-06-certificate-scope-empty:::UIA-06:::include/tlm/cep/collective_planes.json@@    \"certificate_scope\": [\n      {\n        \"plane\": \"FabricCollectivePlane\",\n        \"network\": \"ualink_fabric\"\n      }\n    ]@@    \"certificate_scope\": []"
  "f-07-ualink-4-declared:::UIA-07:::include/tlm/cep/protocol_instances.json@@      \"id\": \"moesi_gpu\",@@      \"id\": \"ualink_4\","
  "f-08-c1-mapping-mismatch:::CP-03-VC-CERTIFICATE/C-1′:::include/tlm/cep/protocol_instances.json@@\"read\": \"RC-acquire\",\n        \"write\": \"RC-release\"\n      },\n      \"ordering_assumption\": {\n        \"compute_noc\": {\n          \"0\": \"unordered\",\n          \"1\": \"p2p_ordered\",\n          \"2\": \"unordered\",\n          \"3\": \"p2p_ordered\"\n        },\n        \"ualink_fabric\": {\n          \"0\": \"unordered\",\n          \"1\": \"unordered\",\n          \"2\": \"unordered\",\n          \"3\": \"p2p_ordered\"\n        }\n      },\n      \"vc_binding\": {\n        \"compute_noc\": {\n          \"request\": 0,\n          \"response\": 1,\n          \"snoop\": 2,\n          \"snoop_resp\": 3\n        },\n        \"ualink_fabric\": {\n          \"request\": 0,\n          \"atomic\": 1,\n          \"query\": 2,@@\"read\": \"RC-acquire\",\n        \"write\": \"RC-release\"\n      },\n      \"ordering_assumption\": {\n        \"compute_noc\": {\n          \"0\": \"unordered\",\n          \"1\": \"p2p_ordered\",\n          \"2\": \"unordered\",\n          \"3\": \"p2p_ordered\"\n        },\n        \"ualink_fabric\": {\n          \"0\": \"unordered\",\n          \"1\": \"unordered\",\n          \"2\": \"unordered\",\n          \"3\": \"p2p_ordered\"\n        }\n      },\n      \"vc_binding\": {\n        \"compute_noc\": {\n          \"request\": 0,\n          \"response\": 1,\n          \"snoop\": 2,\n          \"snoop_resp\": 3\n        },\n        \"ualink_fabric\": {\n          \"request\": 0,\n          \"atomic\": 1,\n          \"query\": 3,"
  "f-09-c2-vc-count:::CP-09-IC1:::include/tlm/cep/fabric.json@@    \"ualink_fabric\": { \"vc_count\": 4 }@@    \"ualink_fabric\": { \"vc_count\": 5 }"
  "f-10-c3-declaration:::CP-03-VC-CERTIFICATE/C-3:::include/tlm/cep/collective_plane.hh@@    {\"FabricCollectivePlane\", false, true},@@    {\"FabricCollectivePlane\", false, false},"
  "f-11-c4-untested-faked:::CP-03-VC-CERTIFICATE/C-4:::include/tlm/cep/collective_plane_contract.hh@@        reg.items.emplace_back(std::string(u.ref), std::string(u.statement));@@        reg.items.emplace_back(std::string(u.ref), std::string(u.statement) + \" —— 已验证\");"
)

# ── 归因 id 登记(**唯一**登记处 = 本 change 的 `design §4`)─────────────────
#   本层**新建** 7 个(`UIA-01` ~ `UIA-07`)+ **4** 个 `ADR-CEP-01` E.3.1 **既有** id 的消费
REGISTERED_IDS=(
  "UIA-01"
  "UIA-02"
  "UIA-03"
  "UIA-04"
  "UIA-05"
  "UIA-06"
  "UIA-07"
  "CP-03-VC-CERTIFICATE/C-1′"
  "CP-09-IC1"
  "CP-03-VC-CERTIFICATE/C-3"
  "CP-03-VC-CERTIFICATE/C-4"
)

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/ualink_rv.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

echo ""
echo "═══ ① 归因 id 双向覆盖自检(fixture 期望集合 ⟺ 登记集合)═══"
fx_ids="$(for fx in "${FIXTURES[@]}"; do _r="${fx#*:::}"; echo "${_r%%:::*}"; done | sort -u)"
reg_ids="$(printf '%s\n' "${REGISTERED_IDS[@]}" | sort -u)"
if [ "$fx_ids" = "$reg_ids" ]; then
    n_fx=$(printf '%s\n' "${REGISTERED_IDS[@]}" | wc -l)
    ok "归因 id 双向覆盖:fixture 集合 == 登记集合(**$n_fx ↔ $n_fx**)"
else
    bad "归因 id 双向覆盖失配 —— 仅在 fixture:$(comm -23 <(echo "$fx_ids") <(echo "$reg_ids") | tr '\n' ' ')|仅在登记:$(comm -13 <(echo "$fx_ids") <(echo "$reg_ids") | tr '\n' ' ')"
fi

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
echo "═══ 逐条毒化 ⇒ 判据必须失败(且失败原因命中期望归因)═══"
for fx in "${FIXTURES[@]}"; do
    id="${fx%%:::*}"; _r="${fx#*:::}"
    expected="${_r%%:::*}"; edits="${_r#*:::}"

    SHADOW="$TMP/shadow_$id"
    rm -rf "$SHADOW"
    mkdir -p "$SHADOW"
    cp -r "$ROOT/include" "$SHADOW/include"

    # 逐个编辑做毒化(仓内文件**零改动**)
    # ⚠️ **编辑串的切分必须在 python 里做** —— bash 的多字符 IFS 会**逐字符**切分
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
        # ① **编译期红**:归因 = 编译日志里的断言文案(**精确相等**)
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
