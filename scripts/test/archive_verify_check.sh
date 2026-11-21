#!/usr/bin/env bash
# scripts/test/archive_verify_check.sh
#   —— 归档验收债的**存在性 + 形态**闸（仓级域 · 统一覆盖，不按 `cep` 前缀特判）
#
# per change `cep-d1-archive-verify-backfill`（登记册 §6.4 队列 2 的 **I-10**）
#   · 判据来源：`.rddf/improvements/cep-d1-archive-verify-backfill.md` §What Changes #3 + §Acceptance
#
# ── 闸的定位（**地板，不是交付标准**）────────────────────────────────────
#   本闸只证「**报告存在且形态成立**」；I-10 的 AC-1 要求每份报告另含
#   「失败项 / 已用轮次」等 4 段（**高于**本闸）⇒ ⛔ **不得**把闸绿读作
#   「报告达到 AC 标准」。
#
# ── 扫面口径（**封存边界**）────────────────────────────────────────────
#   扫面 `openspec/changes/archive/` 下**目录**中日期 **>= 2026-11-12** 者；
#   日期 **< 2026-11-12** = **甲层**（一次性历史封存，⛔ 不得按名字逐项豁免）。
#   ⚠️ 非目录项（如 `STATUS_AUDIT_2026-06-15.md`）**跳过**。
#
# ── join 规则（精确）───────────────────────────────────────────────────
#   report = ".rddf/reviews/" + strip_date_prefix(archive_dir) + "-verify.md"
#
# ── fail-closed（三条，⛔ 不得静默 rc=0）────────────────────────────────
#   ① **畸形目录名**：目录名不匹配 `^[0-9]{4}-[0-9]{2}-[0-9]{2}-` ⇒ 判红
#      （红因签名「归档目录名缺日期前缀」）—— ⛔ 不静默 strip。
#   ② **扫面不可见**：`openspec/changes/archive/` 被 `.gitignore` 排除 ⇒ git 只跟踪
#      一部分归档目录 ⇒ CI 的 fresh clone 上扫面是**残缺集合** ⇒ 若本地扫面目录数
#      **< git 跟踪的同口径目录数** ⇒ 判红（红因签名「归档目录不可见：CI 无法校验」）
#      —— ⛔ **不得**静默 rc=0（否则就是用新闸复刻 `S2-8` 的假绿）。
#      ⚠️ 基线**由 git 动态导出**（⛔ 不写死数字 —— 写死会随每次归档而腐化）。
#   ③ **环境缺失**：git 不可用 ⇒ 无法导出基线 ⇒ **exit 2**（⛔ 不计作判据失败）。
#
# ── 三检（每份报告）────────────────────────────────────────────────────
#   ① 报告存在 ② 含 `判据来源` ∧ (`判定` ∨ `逐条判定`) ∧ (`遗留` ∨ `遗留清扫`)
#      ③ 去空白后长度 >= 400 字符。
#   ＋ **provenance 一致性**：若报告自称「追溯补验」⇒ 必须同时含 `原归档未验`
#      与 `角色` 标记（⛔ 防「假声明」）；自称「原始验收」⇒ 合法（归档前已验）。
#
# 用法:
#   bash scripts/test/archive_verify_check.sh
#   ARCHIVE_VERIFY_ROOT=/path/to/shadow bash scripts/test/archive_verify_check.sh   # 影子副本
set -uo pipefail

ROOT="${ARCHIVE_VERIFY_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
ARCHIVE="$ROOT/openspec/changes/archive"
REVIEWS="$ROOT/.rddf/reviews"
BASELINE_DATE="2026-11-12"     # 封存边界：甲层 = 归档日期 < 此值

problems=0
checked=0
malformed_names=""

if [ ! -d "$ARCHIVE" ]; then
  # ⚠️ **不是环境错误** —— 这正是 fail-closed ② 的形态（扫面整体不可见 ⇒ CI 无法校验）
  #    ⇒ 判红（rc=1）+ 签名，⛔ **不得** exit 2（exit 2 = 「不计作判据失败」⇒ 会让变异不算数）
  echo "  ✗ 归档目录不可见：CI 无法校验（归档目录不存在: $ARCHIVE）" >&2
  echo "  ✗ 归档验收报告闸：1 项不通过（扫面 0 份）" >&2
  exit 1
fi

# ── fail-closed ③：git 可用性（基线来源）────────────────────────────────
if ! command -v git >/dev/null 2>&1 || ! git -C "$ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  echo "  ✗ 环境错误:git 不可用 —— 无法导出「git 跟踪的归档目录」基线（exit 2，⛔ 不计作判据失败）" >&2
  exit 2
fi

# ── fail-closed ①：畸形目录名（报告后**跳过**该目录，不参与扫面）────────
for d in "$ARCHIVE"/*/; do
  [ -d "$d" ] || continue
  name="$(basename "$d")"
  if ! printf '%s' "$name" | grep -qE '^[0-9]{4}-[0-9]{2}-[0-9]{2}-'; then
    echo "  ✗ 归档目录名缺日期前缀: $name" >&2
    problems=$((problems + 1))
    malformed_names="$malformed_names $name"
  fi
done

# ── 扫面 + 三检 ─────────────────────────────────────────────────────────
for d in "$ARCHIVE"/*/; do
  [ -d "$d" ] || continue
  name="$(basename "$d")"
  # 畸形目录名已在 fail-closed ① 报过 ⇒ **跳过**（⛔ 不静默 strip 成一个假 change）
  case " $malformed_names " in *" $name "*) continue ;; esac
  date="${name:0:10}"
  # 甲层 = 归档日期 < 封存边界 ⇒ 跳过（⛔ 不是按名字豁免）
  [ "$date" \< "$BASELINE_DATE" ] && continue
  checked=$((checked + 1))
  change="${name:11}"                      # strip_date_prefix
  report="$REVIEWS/$change-verify.md"
  if [ ! -f "$report" ]; then
    echo "  ✗ 归档 change 缺独立验收报告: $change" >&2
    problems=$((problems + 1))
    continue
  fi
  if ! grep -qF '判据来源' "$report"; then
    echo "  ✗ 报告形态不成立(缺「判据来源」): $change" >&2
    problems=$((problems + 1))
  fi
  if ! grep -qF '判定' "$report"; then
    echo "  ✗ 报告形态不成立(缺「判定 / 逐条判定」): $change" >&2
    problems=$((problems + 1))
  fi
  if ! grep -qF '遗留' "$report"; then
    echo "  ✗ 报告形态不成立(缺「遗留 / 遗留清扫」): $change" >&2
    problems=$((problems + 1))
  fi
  len="$(tr -d '[:space:]' < "$report" | wc -c | tr -d ' ')"
  if [ "$len" -lt 400 ]; then
    echo "  ✗ 报告形态不成立(去空白长度 $len < 400): $change" >&2
    problems=$((problems + 1))
  fi
  # provenance 一致性（⛔ 防「假声明」；⛔ 不强制存量报告补标记）
  if grep -qF '**追溯补验' "$report"; then
    if ! grep -qF '**原归档未验**' "$report"; then
      echo "  ✗ 报告形态不成立(自称追溯补验但缺「原归档未验」): $change" >&2
      problems=$((problems + 1))
    fi
    if ! grep -qF '**角色**' "$report"; then
      echo "  ✗ 报告形态不成立(自称追溯补验但缺「角色」标记): $change" >&2
      problems=$((problems + 1))
    fi
  fi
done

# ── fail-closed ②：扫面不可见（**基线 = 已提交基线文件**）──────────────
#   ⚠️ 为什么基线是**已提交文件**而不是脚本里的常量：`openspec/changes/archive/`
#      被 `.gitignore` 排除 ⇒ git 只跟踪一部分归档目录 ⇒ CI 的 fresh clone 上
#      闸看到的是**残缺集合**。若基线取「git 当前跟踪数」，CI 上二者相等 ⇒
#      **空转绿**（= 用新闸复刻 `S2-8` 的假绿）⇒ 基线必须是一个**与当前
#      git 状态无关的已提交值**（`scripts/test/archive_verify_baseline.txt`）。
#      ⚠️ 每次新增归档 change ⇒ **同步更新基线 + force-add 该目录**。
BASELINE_FILE="$ROOT/scripts/test/archive_verify_baseline.txt"
baseline=0
if [ -f "$BASELINE_FILE" ]; then
  baseline="$(tr -d '[:space:]' < "$BASELINE_FILE" | head -1)"
  case "$baseline" in
    ''|*[!0-9]*)
      echo "  ✗ 基线文件内容不是整数: '$baseline'（$BASELINE_FILE）" >&2
      problems=$((problems + 1))
      baseline=0
      ;;
  esac
else
  echo "  ✗ 缺基线文件: $BASELINE_FILE（⛔ 无基线 ⇒ 无法判定扫面是否完整 ⇒ 按 fail-closed 判红）" >&2
  problems=$((problems + 1))
fi

tracked="$(git -C "$ROOT" ls-files "openspec/changes/archive/" 2>/dev/null \
  | awk -F/ '{print $4}' | sort -u \
  | grep -E '^[0-9]{4}-[0-9]{2}-[0-9]{2}-' \
  | awk -v b="$BASELINE_DATE" '{ if (substr($0,1,10) >= b) n++ } END { print n+0 }')"

if [ "$baseline" -gt 0 ] && [ "$checked" -lt "$baseline" ]; then
  echo "  ✗ 归档目录不可见：CI 无法校验（扫面 $checked < 基线 $baseline）" >&2
  problems=$((problems + 1))
fi
if [ "$tracked" -gt "$checked" ]; then
  echo "  ✗ 归档目录不可见：CI 无法校验（扫面 $checked < git 跟踪 $tracked）" >&2
  problems=$((problems + 1))
fi

if [ "$problems" -gt 0 ]; then
  echo "  ✗ 归档验收报告闸：$problems 项不通过（扫面 $checked 份 · 基线 $baseline · git 跟踪 $tracked）" >&2
  exit 1
fi

if [ "$checked" -gt "$baseline" ]; then
  echo "  ⚠️ 扫面 $checked > 基线 $baseline ⇒ 有新归档 change ⇒ **须**更新基线文件并 force-add 该目录（否则 CI 上扫不到）"
fi

echo "  ✅ 归档验收报告闸通过（扫面 $checked 份 · 基线 $baseline · git 跟踪 $tracked · 三检 + provenance 一致性）"
exit 0
