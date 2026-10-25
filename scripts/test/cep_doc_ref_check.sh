#!/bin/bash
# CEP 对外文档引用校验
#
# 背景（2026-10-22）:
#   `ADR-CEP-05 §10` 被写作「α 失效判据 2 的所在节」,但判据 2 实际在 **§7**
#   (§10 是「关联 ADR / OpenSpec」)。该笔误从初始 commit(`7195f7f7`)起扩散到 **24 处**、
#   跨越 3 个 commit 无人发现 —— 且出现在**要发给 ArchForge 的对外文档**里。
#   根因:本仓有**锚点机制**管 `file:line`(33 锚点全绿),但**节号引用无任何机器校验**。
#
# 两层校验（分工明确,互补）:
#   [3] 自动检测 —— 「<别名> §N」的**目标节必须存在**。
#       来源 = 文档自身派生,**不存在漏登记**问题。
#       抓得住:节号错别字、别名解析到错文件、目标文件删节。
#       ⚠️ **抓不住**:`ADR-CEP-05 §10` 这种「节存在、但语义指错」(§10 确实存在)。
#   [4] 语义断言注册表 —— 目标节**标题必须含期望关键词** + 文档侧字面量仍在。
#       来源 = 人工断言(见 cep_doc_ref_registry.tsv)。
#       抓得住:上面那个「节存在但语义指错」的原始 bug 类。
#       ⚠️ **依赖人工登记**,未登记 = 零覆盖(覆盖度见 [4b])。
#
# 为什么用白名单而非纯自动解析:
#   实测用 glob `NN-*.md` 自动解析会把 `07 号` 错解成 ArchForge `07-memory-system.md`
#   (而文档指的是本仓 `07-coherence-exploration-platform.md`,含 §5.1.1 工业硬件协议族)。
#   **错解比不解更危险** —— 会给出虚假的通过。故别名到文件的映射必须显式登记
#   (见 cep_doc_ref_aliases.tsv)。
#
# 用法:
#   bash scripts/test/cep_doc_ref_check.sh            # 检查并打印报告
#   bash scripts/test/cep_doc_ref_check.sh --strict   # 有问题返回非零
#   ARCHFORGE_DIR=/path/to/ArchForge bash scripts/test/cep_doc_ref_check.sh
#
# 设计依据:台账 `obj-cep-platform §11` Entry #12 ④ 的机制缺口登记
#   —— 「对外文档的节号引用必须与行号引用同等校验」。

set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
STRICT=false
[ "${1:-}" = "--strict" ] && STRICT=true

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$ROOT"
ARCHFORGE="${ARCHFORGE_DIR:-/workspace/main/ArchForge}"

echo -e "${BLUE}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   CEP 对外文档引用校验 v2.0                          ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════╝${NC}"
echo ""

FAILED=0
pass() { echo -e "  ${GREEN}✓${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; FAILED=$((FAILED + 1)); }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }

# 受检的文档（新增时在此登记）
# ① 对外文档（发给 ArchForge 的）—— 引用错误会**直接**误导对方
# ② 契约面（ADR / 架构 / tasks / 台账）—— 状态不一致会**内部矛盾**
#    ② 的加入源于 2026-10-22 复审:AC-9 要求「无『已触发 / 未触发』矛盾」,
#    但当时的门禁只覆盖 ① → 契约面的矛盾**无人能自动发现**。
DOCS=(
  "docs/implementation/2026-10-22-cep-archforge-response-checklist.md"
  "docs/implementation/2026-10-21-cep-archforge-response-receipt.md"
  "docs/implementation/2026-10-22-cep-stage0-external-review-summary.md"
  "docs/implementation/2026-10-22-cep-poc-spike-spec.md"
  "docs/adr/ADR-CEP-05-stub-cache-core-interface.md"
  "docs/adr/ADR-CEP-11-protocol-version-locking.md"
  "docs/adr/ADR-CEP-12-protocol-engine-capability-map.md"
  "docs/adr/ADR-CEP-13-coherence-domain-ownership.md"
  "docs/architecture/07-coherence-exploration-platform.md"
  "openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/tasks.md"
  ".rddf/roadmap/objectives/obj-cep-platform.md"
)

# grep -P 支持性探测(用于 CJK 前边界;BSD grep 无 -P → 降级 + warn)
GREP_P_OK=0
if echo "x" | grep -qP "x" 2>/dev/null; then GREP_P_OK=1; fi

ALIASES="$SCRIPT_DIR/cep_doc_ref_aliases.tsv"
REGISTRY="$SCRIPT_DIR/cep_doc_ref_registry.tsv"
for f in "$ALIASES" "$REGISTRY"; do
  [ -f "$f" ] || { fail "数据文件缺失: $f"; exit 1; }
done

# ── VIRTUAL_PATHS（per `docs_sync_check.sh` 先例）──
# 这些路径**有意**不在本仓，或不是「可判存在的单一路径」:
#   · 跨仓（ArchForge 侧）—— 由「[附] 跨仓引用」段单独校验
#   · 设计示意（尚未创建的目标产物，如 Stage 1 才建的 `coherence_protocol.hh`）
#   · 花括号展开 / 多路径合写 / 目录列表 —— 无法作为单一路径判存在
# ⚠️ 新增条目必须写清理由 —— 这个白名单是「有意例外」的登记处，不是消音器。
VIRTUAL_PATHS=(
  "docs/roadmap/cep-interface-alignment"            # 跨仓:ArchForge 侧文档
  "docs/architecture/32-cep-based-coherence"        # 跨仓:ArchForge 侧文档
  "docs/adr/ADR-SOC-25"                             # 跨仓:ArchForge 侧 ADR
  "include/core/coherence_protocol.hh"              # 设计示意:Stage 1 才新建(本仓 0 命中,per 回执 §5)
  "include/ src/"                                   # 多路径合写(非单一路径)
  "include/ src/ test/"                             # 同上
  # —— 通配 / 省略号 / 目录列表:不是「可判存在的单一路径」——
  "docs/adr/ADR-CEP-*.md"                           # 通配(族名)
  "docs/adr/ADR-DGPU-06/08/09/11/12"                # 多编号合写
  "docs/adr/ADR-CEP-05-...md"                       # 省略号(散文引用)
  "docs/adr/ADR-CEP-11-...md"                       # 同上
  "docs/adr/ADR-CEP-12-...md"                       # 同上
  "docs/adr/ADR-CEP-13-...md"                       # 同上
  "docs/adr/ADR-CEP-00-...md"                       # 同上
  "openspec/.../tasks.md"                          # 省略号(散文引用,per obj §4.7.3 载体分工表的通用写法)
  "docs/architecture/07-*.md"                        # 通配
  "include/tlm/cep/.gitkeep"                        # 占位文件
  "include/tlm/pcie/pcie_endpoint_tlm.h"            # 冻结/待迁路径
  "tools/aggregate.py"                              # 设计示意
  "tools/recommend.py"                              # 设计示意
  "docs/architecture/07-...md"                      # 省略号(散文引用)
  "openspec/changes/<name>/"                        # 占位写法
  "openspec/changes/archive/2026-10-20-*/"          # 通配
  "openspec/changes/cpptlm-cep-umbrella"            # 已归档 change 的**原路径**(2026-10-23 归档)—— 历史快照指向归档前位置
  "openspec/changes/cpptlm-cep-poc-spike"           # 同上(2026-10-25 归档 → 2026-10-25-cpptlm-cep-poc-spike);历史叙述(规格文档 §/台账 Entry)指向归档前位置
  "openspec/changes/cpptlm-cep-stub-cache-core"        # 同上(2026-10-25 归档 → 2026-10-25-cpptlm-cep-stub-cache-core);历史叙述(台账 Entry #58/#60 的产出物清单)指向归档前位置
  "openspec/changes/cpptlm-cep-stub-cache-core/specs/cep-vc-mapping-contract/spec.md"  # 同上(该 delta spec 已合并进 openspec/specs/cep-vc-mapping-contract/spec.md)
  "openspec/changes/cpptlm-cep-stub-cache-core/specs/cep-stub-cache-core/spec.md"  # 同上(该 delta spec 已合并进 openspec/specs/cep-stub-cache-core/spec.md)
  "openspec/changes/cpptlm-cep-poc-spike/specs/cep-poc-spike-verification/spec.md"  # 同上(该 delta spec 已合并进 openspec/specs/cep-poc-spike-verification/spec.md);现址见 openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella
  # —— 通配 / 族名(2026-10-23 Entry #29 引入):不是「可判存在的单一路径」——
  "docs/implementation/*"                           # 历史快照族通配(台账 Entry #29 ② 引述)
  "openspec/changes/**/specs/cep-core-contracts/spec.md"  # 冻结清单 glob(兼容 active/archived 两态)
  "openspec/changes/archive/**/*.md"                # 归档区通配(台账 Entry #29 ③ 引述)
  "openspec/changes/archive/*cpptlm-cep-umbrella/**/*.md"  # 归档区定向通配(同上)
  "openspec/changes/archive/*/cpptlm-cep-umbrella/**/*.md" # 同上(**旧写法**:归档目录名带日期前缀,此写法匹配 0 —— 已在 Entry #30 修正)
  "openspec/changes/archive/*cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md"  # 冻结 glob(同上)
  "openspec/specs/cep-core-contracts/*.md"          # main spec 通配(同上)
  "tools/*.py"                                      # 工具族通配(台账 Entry #29 ④ 引述)
)

is_virtual_path() {
  local p="$1"
  for vp in "${VIRTUAL_PATHS[@]}"; do
    [[ "$p" == *"$vp"* ]] && return 0
  done
  return 1
}

# 提取「像单一路径」的引用：
#   · 排除花括号展开 `a.{b,c}.json`
#   · 排除含空格的多路径合写
#   · 排除行号区间 `:60-62`（只取单一行号 `:N`）
extract_path_refs() {
  grep -oE '`(docs|include|scripts|openspec|tools|experimental)/[^`]*`' "$1" 2>/dev/null \
    | tr -d '`' \
    | grep -vE '[{}]' \
    | grep -vE '[[:space:]]' \
    | grep -vE ':\*' \
    | grep -vE ':[0-9]+-' \
    | sort -u
}

# 在目标文件里找 §<sec> 的标题行；找不到则输出空
section_line() {
  local file="$1" sec="$2" esc
  esc="${sec//./\\.}"
  grep -nE "^#+ §?${esc}[ .:]" "$file" 2>/dev/null | head -1 || true
}

echo -e "${BLUE}[1/4] 对外文档存在性${NC}"
for d in "${DOCS[@]}"; do
  [ -f "$d" ] && pass "$d" || fail "对外文档缺失: $d"
done

echo ""
echo -e "${BLUE}[2/4] 本仓 path / path:line 引用可达性${NC}"
BAD_PATH=0; VIRT_SKIPPED=0
for d in "${DOCS[@]}"; do
  [ -f "$d" ] || continue
  while IFS= read -r ref; do
    [ -z "$ref" ] && continue
    if is_virtual_path "$ref"; then VIRT_SKIPPED=$((VIRT_SKIPPED + 1)); continue; fi
    f="${ref%%:*}"; ln="${ref#*:}"
    [ "$ln" = "$f" ] && ln=""
    if [ ! -e "$f" ]; then
      fail "$(basename "$d"): 引用不存在的文件 $f"; BAD_PATH=$((BAD_PATH + 1)); continue
    fi
    if [ -n "$ln" ]; then
      total=$(wc -l < "$f")
      if [ "$ln" -gt "$total" ]; then
        fail "$(basename "$d"): $f:$ln 越界（共 $total 行）"; BAD_PATH=$((BAD_PATH + 1))
      fi
    fi
  done < <(extract_path_refs "$d")
done
[ "$BAD_PATH" -eq 0 ] && pass "全部 path / path:line 引用可达（VIRTUAL_PATHS 跳过 $VIRT_SKIPPED 处:跨仓/示意/合写）"

echo ""
echo -e "${BLUE}[3/4] 自动检测:「<别名> §N」的目标节必须存在（文档自身派生,无漏登记）${NC}"
if [ "$GREP_P_OK" != "1" ]; then
  warn "[DEGRADED] 本环境 grep 不支持 -P → 已降级为「无 CJK 前边界」匹配（可能对「答复回执」产生假阳性）"
fi
# 别名表:root<TAB>file<TAB>alias。别名按长度降序,避免短名吃掉长名（如 `22 号` 吃掉 `22-nsa`）。
DETECTED=()          # 元素: root|file|sec|doc|alias|lineno
BAD_DETECT=0
while IFS=$'\t' read -r a_root a_file a_alias; do
  case "$a_root" in \#*|"") continue ;; esac
  a_root="$(echo "$a_root" | tr -d ' \r')"
  a_file="$(echo "$a_file" | tr -d '\r')"
  a_alias="$(echo "$a_alias" | tr -d '\r')"
  [ -z "$a_alias" ] && continue
  # 目标文件缺席（如 ArchForge 仓不在本环境）→ 该别名整体跳过
  case "$a_root" in archforge) a_base="$ARCHFORGE" ;; *) a_base="$ROOT" ;; esac
  [ -e "$a_base/$a_file" ] || continue
  # 别名内的正则特殊字符转义（`.` 常见于 `remaining-batches.md`）
  # ⚠️ 匹配要求别名后为**非字母数字**边界 —— 否则 `ADR-CEP-00` 会吃掉 `ADR-CEP-00A`
  #    （截断匹配 → 解析到**错文件** → 假阴性/假阳性）
  a_esc="$(printf '%s' "$a_alias" | sed 's/[.[\*^$()+?{}|]/\\&/g')"
  for d in "${DOCS[@]}"; do
    [ -f "$d" ] || continue
    # 边界规则:
    #   后边界 → `ADR-CEP-00` 不会吃掉 `ADR-CEP-00A`
    #   前边界 → `回执` 不会匹配到「**答复**回执」的尾部(那是对方文件)
    # 前边界需排除**汉字** → 用 Perl 字符类 \p{Han}(仅 GNU grep -P 支持)
    if [ "$GREP_P_OK" = "1" ]; then
      _m="$(grep -oP "(?<![\p{Han}A-Za-z0-9])${a_esc}(?![A-Za-z0-9])(?:[^§\`|]{0,3})§[0-9]+(?:\.[0-9]+)*" "$d" 2>/dev/null || true)"
    else
      # 降级:无 CJK 前边界 → 「答复回执」会被当成本仓「回执」(可能假阳性)
      _m="$(grep -oE "${a_esc}([^A-Za-z0-9§\`|][^§\`|]{0,3})§[0-9]+(\.[0-9]+)*" "$d" 2>/dev/null || true)"
    fi
    while IFS= read -r m; do
      [ -z "$m" ] && continue
      sec="$(printf '%s' "$m" | grep -oE '§[0-9]+(\.[0-9]+)*$' | tr -d '§')"
      [ -z "$sec" ] && continue
      DETECTED+=("$a_root|$a_file|$sec|$d|$a_alias|")
    done <<< "$_m"
  done
done < "$ALIASES"

for entry in "${DETECTED[@]}"; do
  IFS='|' read -r e_root e_file e_sec e_doc _ _ <<< "$entry"
  # 自引用（文档引用自己的节）不算外部引用
  [ "$e_root" = "local" ] && [ "$e_file" = "$e_doc" ] && continue
  case "$e_root" in archforge) e_base="$ARCHFORGE" ;; *) e_base="$ROOT" ;; esac
  if [ -z "$(section_line "$e_base/$e_file" "$e_sec")" ]; then
    fail "$(basename "$e_doc") 引用的 $(basename "$e_file") §$e_sec **不存在**"
    BAD_DETECT=$((BAD_DETECT + 1))
  fi
done
[ "$BAD_DETECT" -eq 0 ] && pass "自动检测 ${#DETECTED[@]} 处「<别名> §N」引用 —— 目标节全部存在"

echo ""
echo -e "${BLUE}[4/4] 语义断言:目标节标题含期望关键词 + 文档侧字面量仍在（抓「节存在但语义指错」）${NC}"
# 格式见 cep_doc_ref_registry.tsv（6 列制表符分隔）
# ⚠️ 已知边界（勿高估）:
#   ① 字面量校验是**存在性**而非**计数性** —— 同一节号在文档出现 N 次,只改 M<N 次
#      （其余仍在）时**不会**红。但本机制仍能抓住它要抓的那类 bug（当初 §10 是**统一**写错的
#      → 修正后的 `§7` 在文档里**一处都不存在** → 会红）。
#   ② **登记时必须以目标文件的实际标题为准,不得以文档字面量为准** ——
#      若照抄笔误文档登记（§10/关联 ADR）,则 (a)(b) 双双全绿 = 精确复刻原始事故。
REG_TOTAL=$(grep -vcE '^[[:space:]]*(#|$)' "$REGISTRY" || true)
# 列数预检:注册表是**手工维护**的 TSV,少打一个 tab 会让 `lit` 读成空
# → `[ -n "$lit" ]` 为假 → 该行的防漂移校验**静默消失**（无任何报错）。
# 故在循环前对**原始行**做一次列数校验（在循环内用 read 拆完再数是数不出来的）。
BADCOL="$(awk -F'\t' '!/^[[:space:]]*#/ && NF>0 && (NF<6 || NF>7) {printf "  行 %d: %d 列\n", NR, NF}' "$REGISTRY" || true)"
if [ -n "$BADCOL" ]; then
  fail "注册表存在列数非 6/7 的行（应为 root/file/sec/kw/doc/lit[/forbid]）:"
  printf '%s\n' "$BADCOL"
  BAD_SEC_ABORT=1
fi
BAD_SEC=0; DRIFT=0; N_DRIFT_REG=0; N_FORBID_REG=0; SKIPPED_X=0
while IFS=$'\t' read -r root file sec kw doc lit forbid; do
  case "$root" in \#*|"") continue ;; esac
  # 归一化:仅压缩空白,不用 xargs（xargs 遇未配对引号会 rc!=0,在 set -e 下直接崩）
  norm() { printf '%s' "$1" | tr -s '[:space:]' ' ' | sed 's/^ //; s/ $//'; }
  root="$(norm "$root")"; file="$(norm "$file")"; sec="$(norm "$sec")"
  kw="$(norm "$kw")"; doc="$(norm "$doc")"; lit="$(norm "$lit")"
  forbid="$(norm "$forbid")"
  [ "$doc" = "-" ] && doc=""; [ "$lit" = "-" ] && lit=""; [ "$forbid" = "-" ] && forbid=""

  case "$root" in archforge) base="$ARCHFORGE" ;; *) base="$ROOT" ;; esac
  full="$base/$file"
  if [ ! -f "$full" ]; then
    if [ "$root" = "archforge" ]; then warn "[SKIP] 跨仓断言目标缺席: $file"; SKIPPED_X=$((SKIPPED_X + 1)); continue; fi
    fail "断言目标文件不存在: $file"; BAD_SEC=$((BAD_SEC + 1)); continue
  fi
  # (a) 目标文件确有 §sec,且标题含 kw（用 grep -F 避免 kw 内的 glob 字符被当模式）
  line="$(section_line "$full" "$sec")"
  if [ -z "$line" ]; then
    fail "$(basename "$file") 无 §$sec"; BAD_SEC=$((BAD_SEC + 1)); continue
  fi
  title="${line#*:}"
  if ! printf '%s' "$title" | grep -qF -- "$kw"; then
    fail "$(basename "$file") §$sec 实为「$(printf '%s' "${title:0:50}")」—— 期望标题含「$kw」"
    BAD_SEC=$((BAD_SEC + 1))
  fi
  # (b) 文档侧字面量仍在（防文档漂移）
  if [ -n "$doc" ] && [ -n "$lit" ]; then
    N_DRIFT_REG=$((N_DRIFT_REG + 1))
    if [ ! -f "$doc" ]; then
      fail "漂移校验的文档不存在: $doc"; DRIFT=$((DRIFT + 1))
    elif ! grep -qF -- "$lit" "$doc"; then
      fail "$(basename "$doc") 找不到字面量「$lit」—— 文档可能已漂移（注册表指 $file §$sec）"
      DRIFT=$((DRIFT + 1))
    fi
  fi
  # (c) 负向断言:该文档中**不得**出现的过时字面量（防**状态回退**）
  #    ⚠️ 只对**明确过时**的字面量用负向断言 —— 含「曾 / 已失效 / 已补齐」等限定语的
  #       **历史留痕**不适用（它们是合法的）。
  if [ -n "$forbid" ] && [ -n "$doc" ]; then
    N_FORBID_REG=$((N_FORBID_REG + 1))
    if [ ! -f "$doc" ]; then
      fail "负向断言的文档不存在: $doc"; DRIFT=$((DRIFT + 1))
    elif grep -qF -- "$forbid" "$doc"; then
      fail "$(basename "$doc") 仍含过时字面量「$forbid」—— 状态可能已回退"
      DRIFT=$((DRIFT + 1))
    fi
  fi
done < "$REGISTRY"
[ "$BAD_SEC" -eq 0 ] && pass "全部语义断言成立（$((REG_TOTAL - SKIPPED_X)) 条生效 / ${REG_TOTAL} 条登记）"
[ "$DRIFT" -eq 0 ] && pass "全部登记项在文档侧的字面量仍存在（防漂移 $N_DRIFT_REG 条）"
[ "$DRIFT" -eq 0 ] && pass "负向断言全部成立（$N_FORBID_REG 条:过时字面量未复现）"

echo ""
echo -e "${BLUE}[4b] 覆盖度自检 —— 自动检测到的引用中,哪些尚无语义断言${NC}"
# 语义断言是**人工白名单**:未登记 = 只有「存在性」保障,没有「语义正确」保障。
# 本段如实报出这个差额（advisory,不判红）—— 补断言是**增量工作**,不该阻断 CI。
# ⚠️ 刻意**不**自动生成断言:断言的 `kw` 是**人工判断**（「该节应当讲什么」）,
#    自动生成只能照抄标题 = 恒真 = 零信息（且照抄笔误文档会精确复刻原始事故）。
# 匹配用 **file + sec 精确** —— 登记 `§5` **不**覆盖 `§5.1`(子节标题是另一回事)。
UNASSERTED=(); N_HAVE_ASSERT=0
for entry in "${DETECTED[@]}"; do
  IFS='|' read -r e_root e_file e_sec e_doc _ _ <<< "$entry"
  [ "$e_root" = "local" ] && [ "$e_file" = "$e_doc" ] && continue
  hit=0
  while IFS=$'\t' read -r r_root r_file r_sec _ _ _; do
    case "$r_root" in \#*|"") continue ;; esac
    r_file="$(printf '%s' "$r_file" | tr -s '[:space:]' ' ' | sed 's/^ //; s/ $//')"
    r_sec="$(printf '%s' "$r_sec" | tr -s '[:space:]' ' ' | sed 's/^ //; s/ $//')"
    [ "$r_file" = "$e_file" ] && [ "$r_sec" = "$e_sec" ] && { hit=1; break; }
  done < "$REGISTRY"
  if [ "$hit" -eq 1 ]; then N_HAVE_ASSERT=$((N_HAVE_ASSERT + 1))
  else UNASSERTED+=("$(basename "$e_file") §$e_sec"); fi
done
N_DISTINCT=$(printf '%s\n' "${UNASSERTED[@]:-}" | sort -u | grep -c . || true)
echo "  外部引用 $(( ${#DETECTED[@]} )) 处 → 有「语义正确」保障 $N_HAVE_ASSERT 处 / 仅「存在性」保障 ${#UNASSERTED[@]} 处"
if [ "${#UNASSERTED[@]}" -eq 0 ]; then
  pass "全部自动检测到的引用都有语义断言（最强保障）"
else
  warn "出现 ${#UNASSERTED[@]} 处未断言引用（涉及 $N_DISTINCT 个不同的 文件§节）—— 前 6 个:"
  printf '      %s\n' "${UNASSERTED[@]}" | sort -u | head -6
  [ "$N_DISTINCT" -gt 6 ] && echo "      …（其余 $((N_DISTINCT - 6)) 个组合略；补断言见 cep_doc_ref_registry.tsv）"
fi

echo ""
echo -e "${BLUE}[附] 跨仓 path 引用（ArchForge:）—— 对方仓缺席则 SKIP,不判红${NC}"
if [ -d "$ARCHFORGE" ]; then
  BAD_X=0; EMPTY_X=0
  for d in "${DOCS[@]}"; do
    [ -f "$d" ] || continue
    while IFS= read -r ref; do
      [ -z "$ref" ] && continue
      p="${ref#ArchForge:}"
      f="${p%%:*}"; ln="${p#*:}"
      [ "$ln" = "$f" ] && ln=""
      # 空前缀（如 `` `ArchForge:` 同上 **§6.0.1** ``）= 承接上文,非可判引用 → 只提示
      if [ -z "$f" ]; then EMPTY_X=$((EMPTY_X + 1)); continue; fi
      if [ ! -e "$ARCHFORGE/$f" ]; then
        fail "$(basename "$d"): ArchForge 侧文件不存在 $f"; BAD_X=$((BAD_X + 1)); continue
      fi
      if [ -n "$ln" ]; then
        total=$(wc -l < "$ARCHFORGE/$f")
        [ "$ln" -gt "$total" ] && { fail "$(basename "$d"): ArchForge:$f:$ln 越界（共 $total 行）"; BAD_X=$((BAD_X + 1)); }
      fi
    done < <(grep -oE '`ArchForge:[^`]*`' "$d" 2>/dev/null | tr -d '`' | sort -u)
  done
  [ "$BAD_X" -eq 0 ] && pass "跨仓引用全部可达（ArchForge 仓在位: $ARCHFORGE）"
  [ "$EMPTY_X" -gt 0 ] && warn "$EMPTY_X 处「ArchForge:」为承接上文的空前缀（非可判引用,已跳过）"
else
  warn "[SKIP] ArchForge 仓不在本环境（$ARCHFORGE）—— 跨仓引用与跨仓语义断言无法校验（非本仓缺陷）"
fi

echo ""
echo "════════════════════════════════════════════════════════"
if [ "$FAILED" -eq 0 ]; then
  echo -e "${GREEN}✅ CEP 对外文档引用校验全部通过${NC}"
  exit 0
else
  echo -e "${RED}❌ CEP 对外文档引用校验发现 $FAILED 处问题${NC}"
  [ "$STRICT" = true ] && exit 1
  exit 0
fi
