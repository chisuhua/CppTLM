#!/usr/bin/env bash
# scripts/test/cep_freeze_reverse_verify.sh
# —— CEP 冻结闸（派生文件 fabric.json）的反向验证
#
# ── 零污染形态（2026-11-24 重构 · per 台账 Entry #543 ⑨ 附带发现）──────────
#   ⛔ **旧形态**：直接改写真实仓内 include/tlm/cep/fabric.json 再靠 trap 还原 ⇒
#      ① **中断即污染**：被 SIGKILL(ctest timeout) 打断时 trap 不执行 ⇒ 冻结件被留成
#         变异态 ⇒ 之后冻结闸 + 多个 CEP 闸**假红**淹没真信号（2026-11-24 实测命中：
#         fabric.json 被清空 + protocol_instances.json 残留 -MUT 标记）；
#      ② 与其它改写同一文件的闸存在**还原竞态**。
#   ✅ **新形态**：把「冻结清单 + 全部冻结产物 + 冻结闸脚本」复制到**拷贝根**，在拷贝根里
#      做变异与验证 ⇒ **真实仓零写入、零污染**（与仓内既有正确形态一致：test/CMakeLists.txt
#      注释「只写自己的临时目录 ⇒ 可安全并行进 ctest」）⇒ 该闸的中断风险**归零**
#      （无需还原 ⇒ 无需 trap ⇒ 无残留可能）。
#
# 用法: bash scripts/test/cep_freeze_reverse_verify.sh
# 退出码: 0 = 反向验证闭环成立 · 1 = 未成立
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

FREEZE="scripts/test/cep_freeze_check.sh"
FABRIC="include/tlm/cep/fabric.json"
MANIFEST="docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'
FAILED=0

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_freeze_rv.XXXXXX")" || exit 2
trap 'rm -rf "$TMP"' EXIT
CP_ROOT="$TMP/root"

# ── 前置 1：依赖文件存在 ────────────────────────────────────────────────
for f in "$FREEZE" "$MANIFEST" "$FABRIC"; do
    if [ ! -f "$f" ]; then
        echo -e "${RED}✗ 缺少 $f —— 无法验证${NC}"
        exit 1
    fi
done

# ⚠️ 仓内 fabric.json 若有未提交改动 ⇒ 基线不可信（上次运行可能被打断留下污染）
if ! git diff --quiet -- "$FABRIC" 2>/dev/null; then
    echo -e "${RED}✗ $FABRIC 有未提交改动 —— 基线不可信(上次运行可能被 SIGKILL 打断)${NC}"
    echo "    · 先还原:git checkout -- $FABRIC"
    exit 1
fi

echo -e "\033[1mCEP 冻结闸 —— 派生文件(fabric.json)反向验证（拷贝根 · 真实仓零写入）\033[0m"

# ── 建拷贝根：冻结闸脚本 + 清单 + 全部冻结产物 ─────────────────────────
mkdir -p "$CP_ROOT/scripts/test" "$CP_ROOT/docs/implementation"
cp "$FREEZE" "$CP_ROOT/scripts/test/"
if ! python3 - "$MANIFEST" "$ROOT" "$CP_ROOT" <<'PYEOF'
import json, os, shutil, sys

man_path, repo, dst = sys.argv[1], sys.argv[2], sys.argv[3]
man = json.load(open(man_path, encoding="utf-8"))
arts = man.get("artifacts", {})
missing = []
for rel in arts:
    src = os.path.join(repo, rel)
    if not os.path.isfile(src):
        missing.append(rel)
        continue
    tgt = os.path.join(dst, rel)
    os.makedirs(os.path.dirname(tgt), exist_ok=True)
    shutil.copy2(src, tgt)
if missing:
    print("  · 缺失 " + str(len(missing)) + " 份冻结产物：" + ", ".join(missing[:5]))
    sys.exit(1)
# 拷贝根内无 git ⇒ provenance 子判定（比对 frozen_at_commit 的 git blob）无法评估
# ⇒ 去掉该键，避免拷贝根基线被 provenance 判红（该子判定与本闸要验的「hash 漂移检测」正交）
man.pop("frozen_at_commit", None)
out = os.path.join(dst, "docs/implementation", os.path.basename(man_path))
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", encoding="utf-8") as fh:
    json.dump(man, fh, ensure_ascii=False, indent=2)
print("  · 已复制冻结产物 " + str(len(arts)) + " 份到拷贝根")
PYEOF
then
    echo -e "  ${RED}✗${NC} 拷贝根构建失败"
    exit 1
fi

CP_FABRIC="$CP_ROOT/$FABRIC"
ORIG_SHA="$(sha256sum "$CP_FABRIC" | cut -d' ' -f1)"
echo "  基线 sha256: ${ORIG_SHA:0:16}…"

run_freeze() {  # <logfile> —— 在拷贝根内跑冻结闸
    ( cd "$CP_ROOT" && bash scripts/test/cep_freeze_check.sh ) > "$1" 2>&1
}

# ── 基线:未破坏 → freeze check 必须报绿 ──────────────────────────────
echo ""
echo "═══ 基线:未破坏 → freeze check 必须 GREEN ═══"
if run_freeze "$TMP/freeze_base.log"; then
    echo -e "  ${GREEN}✓${NC} 基线绿(freeze check rc=0)"
else
    echo -e "  ${RED}✗${NC} 基线就红 —— 拷贝根与冻结清单已不一致"
    tail -5 "$TMP/freeze_base.log" | sed 's/^/      /'
    FAILED=1
fi

# ── 破坏:改 fabric.json 一个字节 → freeze check 必须报红 ───────────────
echo ""
echo "═══ 破坏:改 fabric.json 一字节 → freeze check 必须 RED ═══"
python3 - "$CP_FABRIC" <<'PYEOF'
import sys

p = sys.argv[1]
raw = open(p, "rb").read()
# 在 networks 段后插入一个空格(语义不变 ⇒ decl check 不会红,但 hash 变)
if b'"networks"' in raw:
    raw = raw.replace(b'"networks"', b'"networks" ', 1)
else:
    raw = raw + b" "
open(p, "wb").write(raw)
print("  · 已改写(拷贝根)" + p)
PYEOF

NEW_SHA="$(sha256sum "$CP_FABRIC" | cut -d' ' -f1)"
if [ "$NEW_SHA" = "$ORIG_SHA" ]; then
    echo -e "  ${RED}✗${NC} 破坏无效(hash 未变)—— 无法验证"
    FAILED=1
else
    echo "  · 破坏后 sha256: ${NEW_SHA:0:16}… (与基线不同 ✓)"
fi

if run_freeze "$TMP/freeze_broken.log"; then
    echo -e "  ${RED}✗${NC} 破坏后 freeze check 仍绿 —— **冻结闸对派生文件无牙**"
    FAILED=1
else
    if grep -q "fabric.json" "$TMP/freeze_broken.log"; then
        echo -e "  ${GREEN}✓${NC} 破坏后 freeze check 报红,且点名 fabric.json"
        grep "fabric.json" "$TMP/freeze_broken.log" | head -2 | sed 's/^/      /'
    else
        echo -e "  ${YELLOW}!${NC} 破坏后 freeze check 报红,但**未点名** fabric.json(红因可能是别的)"
        tail -5 "$TMP/freeze_broken.log" | sed 's/^/      /'
        FAILED=1
    fi
fi

# ── 还原:必须字节级还原 → freeze check 必须回绿 ──────────────────────
echo ""
echo "═══ 还原:字节级还原 → freeze check 必须 GREEN ═══"
cp "$FABRIC" "$CP_FABRIC"
RESTORED_SHA="$(sha256sum "$CP_FABRIC" | cut -d' ' -f1)"
if [ "$RESTORED_SHA" != "$ORIG_SHA" ]; then
    echo -e "  ${RED}✗${NC} 还原后 sha256 与基线不同 —— 还原失败"
    FAILED=1
else
    echo -e "  ${GREEN}✓${NC} 字节级还原成功(sha256 一致)"
fi

if run_freeze "$TMP/freeze_restored.log"; then
    echo -e "  ${GREEN}✓${NC} 还原后 freeze check 回绿"
else
    echo -e "  ${RED}✗${NC} 还原后 freeze check 仍红 —— 还原不彻底"
    tail -5 "$TMP/freeze_restored.log" | sed 's/^/      /'
    FAILED=1
fi

# ── 收尾：确认真实仓零写入 ────────────────────────────────────────────
echo ""
if git diff --quiet -- "$FABRIC" 2>/dev/null; then
    echo -e "  ${GREEN}✓${NC} 真实仓零写入（$FABRIC 无改动）"
else
    echo -e "  ${RED}✗${NC} 真实仓 $FABRIC 被改动 —— 本闸不应写仓内文件"
    FAILED=1
fi

echo ""
if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}✅ 冻结闸对派生文件(fabric.json)的反向验证闭环成立${NC}"
    echo "  · 基线绿 → 破坏红(点名 fabric.json)→ 还原绿"
    echo "  · 真实仓零写入（拷贝根内验证）"
    exit 0
fi
echo -e "${RED}❌ 反向验证未成立 —— 冻结闸对 fabric.json 可能无牙${NC}"
exit 1
