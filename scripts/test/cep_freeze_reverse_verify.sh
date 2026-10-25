#!/usr/bin/env bash
# scripts/test/cep_freeze_reverse_verify.sh
#   —— 证明 `cep_freeze_check.sh` 对**派生文件**(fabric.json)**有牙**的反向验证
#
# 为什么需要(per 台账 Entry #56 B1' 条款 ④):
#   `include/tlm/cep/fabric.json` 于 2026-10-25 纳入 M1 冻结集(31 成员)。
#   但它是**派生副本**(`derived_from` = ArchForge `22-nsa §0.5.4.1` + `32 号 §6.0.2`),
#   且 `scripts/test/cep_decl_reverse_verify.sh` 的破坏对象虽含它,**走的却是 decl check 路径**
#   —— **两条路径的牙必须分别验**:decl check 验的是「声明 ↔ fabric 语义一致」,
#   freeze check 验的是「fabric 的 hash 未被静默改写」。前者红不代表后者红。
#
# 闭环(与 decl_reverse_verify 同构):
#   破坏 → freeze check 必须报红 → 还原 → freeze check 必须报绿
#
# ⚠️ **刻意不注册进 ctest**:本脚本会**临时改写**本仓 `include/tlm/cep/fabric.json`,
#    在 `ctest -j4` 并行下会与其它测试读到中间态 → 必须**串行手动跑**。
#
# 用法:
#   bash scripts/test/cep_freeze_reverse_verify.sh
#
# 退出码:0 = 闭环成立(红→绿);1 = 有一步未达成(判据无牙)

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

FREEZE="scripts/test/cep_freeze_check.sh"
FABRIC="include/tlm/cep/fabric.json"
BACKUP="$(mktemp -t fabric_backup.XXXXXX.json)"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

FAILED=0

cleanup() {
    # 还原(fabric.json 是冻结产物 —— 必须字节级还原,否则污染工作区)
    if [ -f "$BACKUP" ]; then
        cp "$BACKUP" "$FABRIC"
        rm -f "$BACKUP"
    fi
}
trap cleanup EXIT

if [ ! -f "$FABRIC" ]; then
    echo -e "${RED}✗ 找不到 $FABRIC —— 无法验证${NC}"
    exit 1
fi

cp "$FABRIC" "$BACKUP"
ORIG_SHA="$(sha256sum "$FABRIC" | cut -d' ' -f1)"

echo -e "\033[1mCEP 冻结闸 —— 派生文件(fabric.json)反向验证\033[0m"
echo "  基线 sha256: ${ORIG_SHA:0:16}…"

# ── 基线:未破坏 → freeze check 必须报绿 ──────────────────────────────
echo ""
echo "═══ 基线:未破坏 → freeze check 必须 GREEN ═══"
if bash "$FREEZE" > /tmp/cep_freeze_rv_base.log 2>&1; then
    echo -e "  ${GREEN}✓${NC} 基线绿(freeze check rc=0)"
else
    echo -e "  ${RED}✗${NC} 基线就红 —— freeze check 与工作区已不一致(先修基线)"
    tail -5 /tmp/cep_freeze_rv_base.log | sed 's/^/      /'
    FAILED=1
fi

# ── 破坏:改 fabric.json 一个字节 → freeze check 必须报红 ───────────────
echo ""
echo "═══ 破坏:改 fabric.json 一字节 → freeze check 必须 RED ═══"
# 用 python 做**字节级**改动(避免 sed 的换行/编码副作用)
python3 - "$FABRIC" <<'PYEOF'
import sys
p = sys.argv[1]
raw = open(p, "rb").read()
# 在 networks 段后插入一个空格(语义不变 → decl check 不会红,但 hash 变)
if b'"networks"' in raw:
    raw = raw.replace(b'"networks"', b'"networks" ', 1)
else:
    raw = raw + b" "
open(p, "wb").write(raw)
print(f"  · 已改写 {p}")
PYEOF

NEW_SHA="$(sha256sum "$FABRIC" | cut -d' ' -f1)"
if [ "$NEW_SHA" = "$ORIG_SHA" ]; then
    echo -e "  ${RED}✗${NC} 破坏无效(hash 未变)—— 无法验证"
    FAILED=1
else
    echo "  · 破坏后 sha256: ${NEW_SHA:0:16}… (与基线不同 ✓)"
fi

if bash "$FREEZE" > /tmp/cep_freeze_rv_broken.log 2>&1; then
    echo -e "  ${RED}✗${NC} 破坏后 freeze check 仍绿 —— **冻结闸对派生文件无牙**"
    FAILED=1
else
    if grep -q "fabric.json" /tmp/cep_freeze_rv_broken.log; then
        echo -e "  ${GREEN}✓${NC} 破坏后 freeze check 报红,且点名 fabric.json"
        grep "fabric.json" /tmp/cep_freeze_rv_broken.log | head -2 | sed 's/^/      /'
    else
        echo -e "  ${YELLOW}!${NC} 破坏后 freeze check 报红,但**未点名** fabric.json(红因可能是别的)"
        tail -5 /tmp/cep_freeze_rv_broken.log | sed 's/^/      /'
        FAILED=1
    fi
fi

# ── 还原:必须字节级还原 → freeze check 必须回绿 ──────────────────────
echo ""
echo "═══ 还原:字节级还原 → freeze check 必须 GREEN ═══"
cp "$BACKUP" "$FABRIC"
RESTORED_SHA="$(sha256sum "$FABRIC" | cut -d' ' -f1)"
if [ "$RESTORED_SHA" != "$ORIG_SHA" ]; then
    echo -e "  ${RED}✗${NC} 还原后 sha256 与基线不同 —— 还原失败"
    FAILED=1
else
    echo -e "  ${GREEN}✓${NC} 字节级还原成功(sha256 一致)"
fi

if bash "$FREEZE" > /tmp/cep_freeze_rv_restored.log 2>&1; then
    echo -e "  ${GREEN}✓${NC} 还原后 freeze check 回绿"
else
    echo -e "  ${RED}✗${NC} 还原后 freeze check 仍红 —— 还原不彻底"
    tail -5 /tmp/cep_freeze_rv_restored.log | sed 's/^/      /'
    FAILED=1
fi

echo ""
if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}✅ 冻结闸对派生文件(fabric.json)的反向验证闭环成立${NC}"
    echo "  · 基线绿 → 破坏红(点名 fabric.json)→ 还原绿"
    exit 0
else
    echo -e "${RED}❌ 反向验证未成立 —— 冻结闸对 fabric.json 可能无牙${NC}"
    exit 1
fi
