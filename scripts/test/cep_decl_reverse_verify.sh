#!/usr/bin/env bash
# scripts/test/cep_decl_reverse_verify.sh
#   —— 证明 `cep_decl_alignment_check.sh` **有牙**的反向验证
#
# 背景(per 复核回执 §8.3 M-2):「声称有自动化、实则没有」是本仓已发生过的真实失败模式
#   —— 判据 C-4 长期标「✅ 可自动化」,而该检查器从未实现,中间态因此静默存在数周。
#   故本脚本做**破坏 → 必须报红 → 恢复 → 必须报绿**的闭环,并把结果作为台账 entry 的证据。
#
# 破坏对象分两类:
#   · 本仓 `include/tlm/cep/fabric.json`  → 临时改 + 还原(备份保护,字节级复验)
#   · ArchForge 的声明文档                 → **改副本**(ARCHFORGE_DIR 指向临时副本),**真仓零改动**
#
# 用法:
#   bash scripts/test/cep_decl_reverse_verify.sh
#   ARCHFORGE_DIR=/path/to/ArchForge bash scripts/test/cep_decl_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

CHECK="scripts/test/cep_decl_alignment_check.sh"
FABRIC="include/tlm/cep/fabric.json"
# ⚠️ **刻意不注册进 ctest**:本脚本会**临时改写**本仓 `include/tlm/cep/fabric.json`,
#    在 `ctest -j4` 并行下会与其它测试读到中间态 → 必须**串行手动跑**。
TMP="${TMPDIR:-/tmp/opencode}/cep_decl_reverse_verify.$$"
REAL_AF="${ARCHFORGE_DIR:-/workspace/main/ArchForge}"

cleanup() {
    [ -f "$TMP/fabric.json.bak" ] && cp "$TMP/fabric.json.bak" "$FABRIC"
    rm -rf "$TMP"
}
trap cleanup EXIT

rm -rf "$TMP"; mkdir -p "$TMP/docs/architecture"
cp "$REAL_AF/docs/architecture/32-cep-based-coherence-architecture.md" "$TMP/docs/architecture/"
cp "$REAL_AF/docs/architecture/22-nsa-fabric-address-spec.md" "$TMP/docs/architecture/"
DECL="$TMP/docs/architecture/32-cep-based-coherence-architecture.md"
cp "$FABRIC" "$TMP/fabric.json.bak"

pass=0; fail=0
run() {  # run <label> <expect: RED|GREEN> <archforge_dir>
    local out rc color
    out="$(ARCHFORGE_DIR="$3" bash "$CHECK" 2>&1)"; rc=$?
    if [ $rc -ne 0 ]; then color="RED"; else color="GREEN"; fi
    if { [ "$2" = "RED" ] && [ "$color" = "RED" ]; } ||
       { [ "$2" = "GREEN" ] && [ "$color" = "GREEN" ]; }; then
        echo "  ✓ $1 → $color(期望 $2)"; pass=$((pass+1))
    else
        echo "  ✗ $1 → $color(期望 $2) —— **反向验证失败**"; fail=$((fail+1))
        echo "$out" | sed 's/^/      /' | head -14
    fi
}
edit_decl() {  # edit_decl <python 表达式片段>(text / re 已在作用域内)
    python3 - "$DECL" "$1" << 'PYEOF'
import re, sys
path, expr = sys.argv[1], sys.argv[2]
text = open(path, encoding="utf-8").read()
text = eval(expr, {"text": text, "re": re})
open(path, "w", encoding="utf-8").write(text)
PYEOF
}
restore_decl() { cp "$REAL_AF/docs/architecture/32-cep-based-coherence-architecture.md" "$DECL"; }
restore_fabric() { cp "$TMP/fabric.json.bak" "$FABRIC"; }

echo "═══ 反向验证基线 ═══"
run "基线(未破坏)" GREEN "$REAL_AF"

echo ""
echo "═══ 破坏 1:fabric.json 的 compute_noc.vc_count 4→2(槽位上界)═══"
python3 -c "
import json
p='$FABRIC'; d=json.load(open(p))
d['networks']['compute_noc']['vc_count']=2
json.dump(d,open(p,'w'),ensure_ascii=False,indent=2)"
run "fabric vc_count 4→2" RED "$REAL_AF"
restore_fabric

echo ""
echo "═══ 破坏 2:fabric.json 增 ArchForge 清单外的网络 nsa_switch(反向比对)═══"
python3 -c "
import json
p='$FABRIC'; d=json.load(open(p))
d['networks']['nsa_switch']={'vc_count':4}
json.dump(d,open(p,'w'),ensure_ascii=False,indent=2)"
run "fabric 增未锁定网络" RED "$REAL_AF"
restore_fabric

echo ""
echo "═══ 破坏 3:声明的 vc_binding vc_id 3→9(越界,schema maximum 3)═══"
edit_decl "text.replace('\"response\": 3, \"snoop_resp\": 3', '\"response\": 9, \"snoop_resp\": 3')"
run "vc_id 3→9" RED "$TMP"
restore_decl

echo ""
echo "═══ 破坏 4:声明的 ordering_assumption 删掉 ualink_fabric(键对称性)═══"
edit_decl "re.sub(r'\n\s*\"ualink_fabric\": \{\n\s*\"0\": \"unordered\",\n\s*\"1\": \"unordered\",\n\s*\"2\": \"unordered\",\n\s*\"3\": \"p2p_ordered\"\n\s*\}', '', text, count=1)"
run "ordering_assumption 少一个网络" RED "$TMP"
restore_decl

echo ""
echo "═══ 破坏 5:声明的 vc_binding 消息类写成 forward_data(6 类词表)═══"
edit_decl "text.replace('\"query\": 2,', '\"forward_data\": 2,')"
run "消息类 forward_data" RED "$TMP"
restore_decl

echo ""
echo "═══ 破坏 6:声明的 ordering_assumption 内层 key 非 vc_id ═══"
edit_decl "text.replace('\"0\": \"unordered\",', '\"high_pri\": \"unordered\",', 1)"
run "内层 key = high_pri" RED "$TMP"
restore_decl

echo ""
echo "═══ 破坏 7:声明的 family 改成非法值(13 个 required 属性的枚举校验)═══"
edit_decl "text.replace('\"family\": \"industrial\"', '\"family\": \"TOTALLY-BOGUS\"', 1)"
run "family 非法枚举值" RED "$TMP"
restore_decl

echo ""
echo "═══ 破坏 8:声明的 JSON 非法(解析失败必须报红,不得静默丢弃)═══"
edit_decl "text.replace('\"id\": \"moesi_gpu\",', '\"id\": \"moesi_gpu\",,', 1)"
run "声明 JSON 非法" RED "$TMP"
restore_decl

echo ""
echo "═══ 破坏 9:ArchForge 权威网络清单文件缺失(反向守卫不得静默关闭)═══"
cp "$TMP/docs/architecture/22-nsa-fabric-address-spec.md" "$TMP/22-nsa.bak"
rm -f "$TMP/docs/architecture/22-nsa-fabric-address-spec.md"
run "22-nsa 缺失" RED "$TMP"
cp "$TMP/22-nsa.bak" "$TMP/docs/architecture/22-nsa-fabric-address-spec.md"

echo ""
echo "═══ 恢复后复验 ═══"
run "本仓 + 真仓 全恢复" GREEN "$REAL_AF"
run "副本 全恢复" GREEN "$TMP"

echo ""
echo "══════════════════════════════════════"
echo "反向验证:${pass} 条通过 / ${fail} 条失败"
[ "$fail" -eq 0 ] || exit 1
