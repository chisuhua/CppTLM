#!/usr/bin/env bash
# scripts/test/cep_spec_strict_check.sh
#   —— `cep-*` 契约面 `openspec validate --strict` 的**零容忍**守门闸
#
# 为什么需要它（change `cpptlm-cep-spec-strict-compliance` · 台账 Entry #434）：
#   `openspec validate <change> --strict` **不检查 `MODIFIED` 块的长度**
#   （工具源码注释原文：`MODIFIED is left alone: its text is the existing requirement,
#     which the specs instruction says to keep whole.`），而 main spec 会被检查。
#   ⇒ 超长只能在**归档合并之后**才成立，而此前**仓内没有任何闸**跑逐份 `--strict`
#   ⇒ 契约面腐化是**静默**的（实测已发生过一次：两个 Requirement 由 89/348 涨到 604/811）。
#
# 判据（三条，全部机器可判）：
#   ① **零红**：`openspec validate --specs --strict --json` 过滤 `cep-` 前缀的项 ⇒ invalid 集合 == ∅
#   ② **覆盖对账**（防静默漏检）：`items` 数 == `openspec/specs` 的目录数；`cep-` 项数 == `cep-*` 目录数
#   ③ **反向 fixture**（本脚本 `--fixture`，默认开）：影子根毒化一份**基线已绿**的 spec
#      ⇒ `invalid_ids(毒化后) == invalid_ids(基线) ∪ {毒化 id}` 且**恰好 +1**
#
# ⚠️ **⛔ 无基线豁免**：不登记「已知红」集合 —— 登记红值等于把判据抹掉。
# ⚠️ **判据 owner 在仓外**：500 / 50 两个阈值由 `@fission-ai/openspec` 持有 ⇒
#    本闸是**变更检测器**，**不是**判据权威。故输出必打印工具版本；版本漂移**仅记录**，
#    ⛔ 本脚本不自作判断（改判据须走独立裁决，残余 R-6）。
# ⚠️ **环境缺失 ⇒ exit 2**（⛔ 不是 pass）：`openspec` / `python3` 不在 PATH 时无法取证。
# ⚠️ **覆盖边界**：判据只覆盖目录名以 `cep-` 开头的 spec ⇒ 非 `cep-*` 的腐化**不在**本闸范围。
# ⚠️ **影子树**：经共享 helper `scripts/test/cep_test_shadow.sh` 承载
#    （冻结契约 `openspec/specs/cep-invariant-gate/spec.md:13`/`:115`：⛔ 任一闸 SHALL NOT 自带复制实现）。
#
# 用法:
#   bash scripts/test/cep_spec_strict_check.sh                 # 主判据 + 反向 fixture
#   bash scripts/test/cep_spec_strict_check.sh --main-only     # 只跑主判据（快）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

MAIN_ONLY=0
[ "${1:-}" = "--main-only" ] && MAIN_ONLY=1

# ── 环境取证:工具缺失 ⇒ exit 2（⛔ 不得当 pass）──────────────────────────────
if ! command -v openspec >/dev/null 2>&1; then
    echo "  ✗ 环境错误:找不到 'openspec' —— 无法取证（exit 2，⛔ 不计作判据失败）" >&2
    exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "  ✗ 环境错误:找不到 'python3' —— 无法解析 openspec 的 --json 输出（exit 2，⛔ 不计作判据失败）" >&2
    exit 2
fi
OS_VER="$(openspec --version 2>/dev/null | head -1)"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

echo "═══ 判据版本（owner 在仓外 ⇒ 仅记录，⛔ 不自行改判据）═══"
echo "  openspec: ${OS_VER:-<unknown>}"
echo "  阈值: Requirement 正文 > 500 字符 / Purpose < 50 字符 / Purpose 占位"
echo "  ⚠️ 工具 major 漂移会改变本闸语义 ⇒ 须走独立裁决（残余 R-6）"

SPEC_DIRS="$(find openspec/specs -mindepth 1 -maxdepth 1 -type d | wc -l)"
CEP_DIRS="$(find openspec/specs -mindepth 1 -maxdepth 1 -type d -name 'cep-*' | wc -l)"

# ── 主判据 ────────────────────────────────────────────────────────────────
# 一次 `--specs --json` 取结构化结果（实测 ~1.3s 全 129 份）⇒ ⛔ 不逐份循环（双真源）
JSON="$(openspec validate --specs --strict --json 2>/dev/null)"
if [ -z "$JSON" ]; then
    echo "  ✗ 环境错误:'openspec validate --specs --json' 无输出（exit 2）" >&2
    exit 2
fi

VERDICT="$(printf '%s' "$JSON" | python3 -c '
import json, sys
d = json.load(sys.stdin)
items = d.get("items", [])
spec_items = [i for i in items if i.get("type") == "spec"]
cep = [i for i in spec_items if str(i.get("id", "")).startswith("cep-")]
bad = [i for i in cep if not i.get("valid", False)]
print("ITEMS=%d" % len(spec_items))
print("CEP=%d" % len(cep))
print("BAD=%d" % len(bad))
for i in bad:
    print("BADID=%s" % i["id"])
    for iss in i.get("issues", []):
        if iss.get("level") != "WARNING":
            continue
        print("  ISSUE=%s: %s" % (iss.get("path"), " ".join(str(iss.get("message", "")).split())[:150]))
')"

ITEMS=$(printf '%s\n' "$VERDICT" | sed -n 's/^ITEMS=//p')
CEP=$(printf '%s\n' "$VERDICT" | sed -n 's/^CEP=//p')
BAD=$(printf '%s\n' "$VERDICT" | sed -n 's/^BAD=//p')

echo ""
echo "═══ ① 零红:cep-* 逐份 --strict ═══"
if [ "${BAD:-x}" = "0" ]; then
    ok "cep-* 全部 valid（$CEP 份）"
else
    ng "cep-* 有 $BAD 份 rc=1"
    printf '%s\n' "$VERDICT" | sed -n '/^BADID=/p;/^  ISSUE=/p' | sed 's/^/      /'
fi

echo ""
echo "═══ ② 覆盖对账（防静默漏检）═══"
if [ "${ITEMS:-x}" = "$SPEC_DIRS" ]; then
    ok "items 数 == openspec/specs 目录数（$SPEC_DIRS）"
else
    ng "items 数($ITEMS) != 目录数($SPEC_DIRS) —— 有 spec 未被校验(或反之)"
fi
if [ "${CEP:-x}" = "$CEP_DIRS" ]; then
    ok "cep- 项数 == cep-* 目录数（$CEP_DIRS）"
else
    ng "cep- 项数($CEP) != cep-* 目录数($CEP_DIRS) —— 有 cep spec 逃逸校验"
fi

# ── ③ 反向 fixture ────────────────────────────────────────────────────────
if [ "$MAIN_ONLY" -eq 1 ]; then
    echo ""
    echo "（--main-only：跳过反向 fixture）"
else
    echo ""
    echo "═══ ③ 反向 fixture:影子根毒化一份本已绿的 spec ═══"

    # 基线前置:主判据必须全绿,否则毒化后仍红 ⇒ 零信息量
    if [ "${BAD:-x}" != "0" ]; then
        echo "  ✗ 前置不满足:基线已有 $BAD 份红 ⇒ 毒化不可归因" >&2
        echo "    （这是**环境/前置错误**，⛔ 不是「判据失败」⇒ exit 2）" >&2
        exit 2
    fi

    # 毒化目标:取**基线已绿**的第一份 cep- spec（动态选取，防新债污染）
    TARGET="$(find openspec/specs -mindepth 1 -maxdepth 1 -type d -name 'cep-*' | sort | head -1)"
    TARGET_ID="$(basename "$TARGET")"

    # 仓内零改动自证:全量 sha256 manifest（⛔ 不用 `git diff --quiet` —— 脏工作区会假红）
    MANIFEST_BEFORE="$(find openspec docs/adr -type f | LC_ALL=C sort | xargs sha256sum | sha256sum)"

    TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_spec_strict.XXXXXX")"
    trap 'rm -rf "$TMP"' EXIT

    # 影子树:⚠️ 经共享 helper（冻结契约硬约束：⛔ 禁止自带复制实现）
    # shellcheck source=scripts/test/cep_test_shadow.sh
    source "$ROOT/scripts/test/cep_test_shadow.sh"

    # run_shadow_case <case-name> <poison-python-heredoc-path>
    #   ⇒ 建全新影子根 → 毒化 → 影子根 validate → 断言 invalid_ids == {TARGET_ID}（恰好 +1）
    run_shadow_case() {
        local label="$1" poisoner="$2"
        cep_shadow_reset "$TMP/shadow" openspec || { echo "  ✗ 影子树构建失败（exit 2）" >&2; exit 2; }
        python3 "$poisoner" "$TMP/shadow/openspec/specs/$TARGET_ID/spec.md" || {
            echo "  ✗ [$label] 毒化失败（exit 2）" >&2; exit 2; }
        local sj
        sj="$(cd "$TMP/shadow" && openspec validate --specs --strict --json 2>/dev/null)"
        if [ -z "$sj" ]; then
            echo "  ✗ [$label] 影子根 validate 无输出（exit 2）" >&2; exit 2
        fi
        local setok
        setok="$(printf '%s' "$sj" | python3 -c '
import json, sys
target = sys.argv[1]
d = json.load(sys.stdin)
# ⚠️ 与主判据同口径:只看 cep- 前缀（非 cep 的 75 份本就红,不在本闸范围）
bad = sorted(i["id"] for i in d.get("items", [])
             if i.get("type") == "spec"
             and str(i.get("id", "")).startswith("cep-")
             and not i.get("valid", False))
# 基线 cep- = ∅（已由前置断言）⇒ 期望恰好 {target}
print("OK" if bad == [target] else "NO:%s" % ",".join(bad))
' "$TARGET_ID")"
        if [ "$setok" = "OK" ]; then
            ok "[$label] 毒化 $TARGET_ID ⇒ invalid_ids == {毒化 id}（恰好 +1，归因精确）"
        else
            ng "[$label] 毒化 $TARGET_ID 后 invalid_ids 不等于期望：${setok#NO:}"
        fi
    }

    # ── 毒化 A:Purpose 占位（<50 / 占位 判据）─────────────────────────────
    cat > "$TMP/poison_purpose.py" <<'PYEOF'
import re, sys
p = sys.argv[1]
s = open(p).read()
m = re.search(r'(?m)^## Purpose\n', s)
assert m, "找不到 ## Purpose"
end = re.search(r'(?m)^## ', s[m.end():])
body = s[m.end(): m.end() + (end.start() if end else 0)]
s2 = s[:m.end()] + "TBD - poisoned by reverse fixture.\n\n" + s[m.end() + len(body):]
assert s2 != s
open(p, 'w').write(s2)
PYEOF
    run_shadow_case "毒化A:Purpose 占位" "$TMP/poison_purpose.py"

    # ── 毒化 B:Requirement 正文 >500 字符 ────────────────────────────────
    #    ⚠️ 必须覆盖 —— 这正是本 change 要防的历史腐化模式（>500 判据）。
    #    只验 Purpose 占位会让「工具静默移除长度检查」时的复发逃逸。
    cat > "$TMP/poison_longreq.py" <<'PYEOF'
import re, sys
p = sys.argv[1]
s = open(p).read()
m = re.search(r'(?m)^### Requirement:\s*.*$', s)
assert m, "找不到 ### Requirement"
tail = s[m.end():]
# 在 Requirement 标题之后、首个 header 之前插入 600 字符正文
filler = ("本段为反向 fixture 注入的超长正文，用于验证 >500 字符判据确实生效；"
          "它不含任何规范语义，仅由本脚本在 mktemp 影子根内写入。") * 12
assert len(filler) > 500
s2 = s[:m.end()] + "\n\n" + filler + "\n" + tail
assert s2 != s
open(p, 'w').write(s2)
PYEOF
    run_shadow_case "毒化B:Requirement >500 字符" "$TMP/poison_longreq.py"

    MANIFEST_AFTER="$(find openspec docs/adr -type f | LC_ALL=C sort | xargs sha256sum | sha256sum)"
    if [ "$MANIFEST_BEFORE" = "$MANIFEST_AFTER" ]; then
        ok "仓内零改动（openspec/ + docs/adr/ 全量 sha256 manifest 前后相等）"
    else
        ng "仓内文件被改动 —— 本脚本必须只写 mktemp 目录"
    fi
fi

echo ""
if [ "$fail" -eq 0 ]; then
    echo "✅ cep-* 契约面 --strict 守门通过（$pass 项）"
    exit 0
fi
echo "❌ cep-* 契约面 --strict 守门失败：$fail 项红 / $pass 项绿"
exit 1
