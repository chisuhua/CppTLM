#!/bin/bash
# CppTLM CEP 文档同步检查脚本
# 用途：校验 CEP (Coherence Exploration Platform) 系列的**契约一致性**：
#       1. 21 份 CEP ADR 存在，且各自含「不变量」或「决策」节
#       2. `07 号 §4` 的 invariant 总表 ↔ 各 owner ADR 实际**定义**的条数一致
#       3. invariant 计数口径：区分「提及」与「定义」（跳空 ID 只被提及，从未定义）
#       4. `06 §15.4` 的 P 编号口径：CEP 文档不得引用 P6
#       5. ADR-CEP-00A 的 §4 映射行 ↔ `07 号 §4` 口径一致
#       6. Stage 0 umbrella change 4 件套 + 契约产物存在
#       7. M1 产物冻结校验(28 产物 SHA-256,冻结后不可改)
#       8. CEP ↔ ArchForge 接口对齐清单漂移检测(21 个 ArchForge 锚点)
#       9. ArchForge 协议实例声明 ↔ 本仓 schema/fabric 对齐检查(判据 C-4 的机器化)
#      10. 传输层 CATEGORY_* 分层纪律(6 类一致性词表**不**落传输层;A14-1 / A14-2 的机器化)
#
# 用法：
#   ./scripts/test/cep_docs_sync_check.sh            # 检查并打印报告
#   ./scripts/test/cep_docs_sync_check.sh --strict   # 发现问题返回非零退出码
#
# 设计依据：per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/design.md D5
#   —— 与既有 docs_sync_check.sh 分工不重叠（本脚本只管 CEP 专属）。

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

STRICT=false
if [ "$1" == "--strict" ]; then
    STRICT=true
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   CppTLM CEP 契约同步检查 v1.0         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
echo ""

FAILED=0
pass() { echo -e "  ${GREEN}✓${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; FAILED=$((FAILED + 1)); }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }

echo -e "${BLUE}[1/11] ADR 清单与关键节${NC}"
ADR_LIST=(
  "ADR-CEP-00-platform-charter-and-governance.md"
  "ADR-CEP-00A-real-source-anchor-map.md"
  "ADR-CEP-01-fabric-collective-plane.md"
  "ADR-CEP-02-coherence-barrier-semantics-clarification.md"
  "ADR-CEP-03-fabric-collective-plane-internals.md"
  "ADR-CEP-04-sharer-tracker-abstraction-redesign.md"
  "ADR-CEP-05-stub-cache-core-interface.md"
  "ADR-CEP-06-experiment-protocol.md"
  "ADR-CEP-07-simulation-and-integration-semantics.md"
  "ADR-CEP-07A-06bis-artifact-disposition.md"
  "ADR-CEP-08-ordering-point-portability.md"
  "ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md"
  "ADR-CEP-11-protocol-version-locking.md"
  "ADR-CEP-12-protocol-engine-capability-map.md"
  "ADR-CEP-13-coherence-domain-ownership.md"
  "ADR-CEP-14-stub-contract-tests.md"
  "ADR-CEP-15-cachetlm-alignment.md"
  "ADR-CEP-16-06-evolution-route-mapping.md"
  "ADR-CEP-17-ruby-slicc-protocol-family.md"
  "ADR-CEP-18-protocol-generation-scope.md"
  "ADR-CEP-NUMBER-MAP.md"
)
MISSING=0
for f in "${ADR_LIST[@]}"; do
  if [ ! -f "docs/adr/$f" ]; then fail "缺 ADR: docs/adr/$f"; MISSING=$((MISSING + 1)); fi
done
[ "$MISSING" -eq 0 ] && pass "${#ADR_LIST[@]} 份 CEP ADR 全部存在"

NO_SECTION=0
for f in "${ADR_LIST[@]}"; do
  [ -f "docs/adr/$f" ] || continue
  if ! grep -qE "^##+ .*不变量" "docs/adr/$f" && ! grep -qE "^##+ .*决策" "docs/adr/$f"; then
    fail "$f 既无「不变量」节也无「决策」节"
    NO_SECTION=$((NO_SECTION + 1))
  fi
done
[ "$NO_SECTION" -eq 0 ] && pass "每份 ADR 均有「不变量」或「决策」节"

echo ""
echo -e "${BLUE}[2/11] invariant 计数与 07 号总表一致性${NC}"
python3 - <<'PYEOF'
import glob, re, os, sys

ROOT = os.getcwd()
ADR = os.path.join(ROOT, "docs", "adr")
TRUTH = os.path.join(ROOT, "docs", "architecture", "07-coherence-exploration-platform.md")

OWNERS = [
    ("ADR-CEP-02-coherence-barrier-semantics-clarification.md", ["CEN-Ib"]),
    ("ADR-CEP-05-stub-cache-core-interface.md", ["INV-SC", "INV-CEP-05-IC"]),
    ("ADR-CEP-04-sharer-tracker-abstraction-redesign.md", ["INV-CEP-04-IC"]),
    ("ADR-CEP-06-experiment-protocol.md", ["INV-CEP-06-IC"]),
    ("ADR-CEP-07-simulation-and-integration-semantics.md", ["INV-CEP-07-IC"]),
    ("ADR-CEP-NUMBER-MAP.md", ["INV-CEP-NUM-IC"]),
    ("ADR-CEP-08-ordering-point-portability.md", ["INV-CEP-08-IC"]),
    ("ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md", ["INV-CEP-09-IC"]),
    ("ADR-CEP-11-protocol-version-locking.md", ["INV-CEP-11-IC"]),
    ("ADR-CEP-12-protocol-engine-capability-map.md", ["INV-CEP-12-IC"]),
    ("ADR-CEP-13-coherence-domain-ownership.md", ["INV-CEP-13-IC"]),
    ("ADR-CEP-14-stub-contract-tests.md", ["INV-CEP-14-IC"]),
    ("ADR-CEP-15-cachetlm-alignment.md", ["INV-CEP-15-IC"]),
    ("ADR-CEP-16-06-evolution-route-mapping.md", ["INV-CEP-16-IC"]),
    ("ADR-CEP-07A-06bis-artifact-disposition.md", ["INV-CEP-07A-IC"]),
]

# 本就不定义 invariant 的 ADR(治理 / 真源映射 / 旁路内部 / 协议族 / 生成范围)
NO_INV = {
    "ADR-CEP-00-platform-charter-and-governance.md",     # 归口 / KPI / 边界
    "ADR-CEP-00A-real-source-anchor-map.md",             # 真源章节映射
    "ADR-CEP-01-fabric-collective-plane.md",             # 旁路平面
    "ADR-CEP-03-fabric-collective-plane-internals.md",   # 旁路内部接口
    "ADR-CEP-17-ruby-slicc-protocol-family.md",          # 协议族接入(引用他方 invariant)
    "ADR-CEP-18-protocol-generation-scope.md",           # 路线范围(纯 trade-off)
}

CAND = r"(?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+)"
FULL = r"(?:CEN-Ib|INV-SC|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC)"


def defined_ids(text):
    """只认**定义位**;跨 ADR 引用(反引号 / 普通文本)不算。

    五种定义形态(与 test/cep_adr_parser.hh 保持一致 —— 两处规则重复,
    修一处必须记得改另一处,否则守门出现盲区):
      P1 **ID**                        加粗
      P2 - ID: / - **ID**:             列表项
      P3 **标签**(ID):                 行内括号
      P4 **标签 ID** / **标签 `ID`**:  加粗 span 内含 ID
      P5 ### 决策 N:`ID` —— …          决策标题
    """
    out = set()
    for m in re.finditer(CAND, text):
        b, e = m.start(), m.end()
        if b >= 2 and text[b - 2:b] == "**" and text[e:e + 2] == "**":
            out.add(m.group(0))
    for m in re.finditer(r"^[\t ]*-\s+\*{0,2}(" + CAND + r")\*{0,2}\s*[:：]", text, re.M):
        out.add(m.group(1))
    for m in re.finditer(r"\*\*[^*\n]{1,40}\*\*\((" + CAND + r")\)\s*[:：]", text):
        out.add(m.group(1))
    for m in re.finditer(r"\*\*[^*\n]{0,60}?\b(" + CAND + r")\b[^*\n]{0,20}?\*\*\s*[:：]", text):
        out.add(m.group(1))
    for m in re.finditer(r"^#{2,6}\s*决策\s*\d+\s*[:：]\s*`?\b(" + CAND + r")\b", text, re.M):
        out.add(m.group(1))
    return out


def id_prefix(tok):
    return re.sub(r"\d+$", "", tok)


def expand_id_cell(cell):
    """展开 07 号 §4 第 3 列的 ID 区间表达 → ID 集合(与 C++ 侧同规则)。"""
    t = cell.replace("`", "")
    out = set()
    # A1:两端全名 "X ~ Y"
    rest = t
    cursor = 0
    pieces = []
    for m in re.finditer(r"(" + FULL + r")?(\d+)\s*~\s*(" + FULL + r")?(\d+)", rest):
        pieces.append((m.start(), m.end()))
    for m in re.finditer(r"(" + FULL + r")?(\d+)\s*~\s*(" + FULL + r")?(\d+)", rest):
        lp, ln, rp, rn = m.group(1), int(m.group(2)), m.group(3), int(m.group(4))
        if not rp:
            rp = lp
        if rp and 1 <= ln <= rn and rn - ln < 1000:
            for k in range(ln, rn + 1):
                out.add(rp + str(k))
    # A2:右端裸后缀 "X ~ ICn"
    for m in re.finditer(r"(" + FULL + r")?(\d+)\s*~\s*(?:IC)(\d+)", rest):
        lp, ln, rn = m.group(1), int(m.group(2)), int(m.group(3))
        if lp and 1 <= ln <= rn and rn - ln < 1000:
            for k in range(ln, rn + 1):
                out.add(lp + str(k))
    # B:斜杠紧凑式 "PREFIX-n/m/k"
    for m in re.finditer(r"(" + FULL + r")(\d+)((?:/\d+)+)", rest):
        prefix = m.group(1)
        out.add(prefix + m.group(2))
        for t2 in re.findall(r"/(\d+)", m.group(3)):
            out.add(prefix + t2)
    # C:独立全名 ID
    for m in re.finditer(r"(" + FULL + r")(\d+)", rest):
        out.add(m.group(1) + m.group(2))
    return out


group_defined = {}   # owner file -> set(ids)
owner_defined = {}   # id -> owner file
OWNERS_GAP = False
errors = []

# 完备性:目录里的 ADR 要么被 OWNERS 覆盖,要么在 NO_INV 白名单里
# (白名单 = 本就不定义 invariant 的 ADR:治理 / 真源映射 / 旁路内部 / 协议族 / 生成范围)
# 否则其 invariant **不被计数** —— 静默漏检,正是 Oracle N1/N2 的形态。
covered = {f for f, _ in OWNERS} | NO_INV
ondisk = {os.path.basename(p) for p in glob.glob(os.path.join(ADR, "ADR-CEP-*.md"))}
uncovered = sorted(ondisk - covered)
if uncovered:
    print(f"  \033[0;31m✗\033[0m 以下 ADR 既未纳入 OWNERS 也不在 NO_INV 白名单(其 invariant 不会被计数):")
    for u in uncovered:
        print(f"      {u}")
    OWNERS_GAP = True
else:
    print(f"  \033[0;32m✓\033[0m 目录内 {len(ondisk)} 份 ADR 全部纳入 OWNERS({len(OWNERS)}) 或 NO_INV 白名单({len(NO_INV)})")

# owner 前缀唯一性 + 两两不互为前缀
prefix_owner = {}
for fname, prefixes in OWNERS:
    for op in prefixes:
        if op in prefix_owner:
            errors.append(f"owner 前缀重复登记: {op}({prefix_owner[op]} / {fname})")
        prefix_owner[op] = fname
for a in prefix_owner:
    for b in prefix_owner:
        if a != b and b.startswith(a):
            errors.append(f"owner 前缀互为前缀(有歧义): {a} / {b}")

for fname, prefixes in OWNERS:
    p = os.path.join(ADR, fname)
    if not os.path.exists(p):
        continue
    ids = defined_ids(open(p, encoding="utf-8").read())
    group_defined[fname] = ids
    for i in ids:
        owner_defined.setdefault(i, fname)

# no-inv 反向检查:白名单里的 ADR 一旦**拥有** invariant 即违反契约
# (跨 ADR 引用不算 —— 例如 ADR-CEP-17 的引用表里 `**INV-CEP-04-IC7**` 是引用)
for fname in sorted(NO_INV):
    p = os.path.join(ADR, fname)
    if not os.path.exists(p):
        continue
    for i in defined_ids(open(p, encoding="utf-8").read()):
        owner = None
        for op, of in prefix_owner.items():
            if i.startswith(op):
                owner = of
                break
        if owner == fname:
            errors.append(f"NO_INV 白名单 ADR「{fname}」开始定义 invariant: {i}"
                          f"(须先迁进 OWNERS 并更新 07 号总表)")

# 孤儿检查:全仓扫描,前缀**无 owner** 的定义形态 ID = 未登记的 invariant
# (同时覆盖「在任一 ADR 新增 INV-CEP-NN-ICx」与「在 no-inv ADR 新增自己的 invariant」)
for fname in sorted(ondisk):
    p = os.path.join(ADR, fname)
    if not os.path.exists(p):
        continue
    for i in defined_ids(open(p, encoding="utf-8").read()):
        if not any(i.startswith(op) for op in prefix_owner):
            errors.append(f"孤儿 invariant(前缀无 owner): {i} —— 出现在 {fname}")

truth_text = open(TRUTH, encoding="utf-8").read()
m = re.search(r"^##\s*4\.\s*(.+)$", truth_text, re.M)
if not m:
    print("  \033[0;31m✗\033[0m 找不到 07 号 §4 节")
    sys.exit(1)
rest = truth_text[m.end():]
m2 = re.search(r"^##\s", rest, re.M)
sec = rest[:m2.start()] if m2 else rest

rows = []
for line in sec.splitlines():
    mm = re.match(r"^\|\s*\*\*([^*]+?)\*\*\s*\|\s*(\d+)\s*\|\s*([^|]*?)\s*\|", line)
    if mm:
        rows.append((mm.group(1), int(mm.group(2)), mm.group(3)))

# 逐组:第 3 列展开的 ID 集合 **必须** 与 owner 的实际定义集合相等
# (此前只比「条数」→ 区间写错 / 组间互换 owner 不可见)
union_truth = set()
total_refs = 0
for name, n, col3 in rows:
    if name == "合计":
        continue
    truth_ids = expand_id_cell(col3)
    total_refs += len(truth_ids)
    prefixes = set()
    for t2 in re.findall(r"(" + FULL + r")\d+", col3.replace("`", "")):
        prefixes.add(t2)
    owners = set()
    for fname, owner_prefixes in OWNERS:
        for op in owner_prefixes:
            if any(pfx == op or pfx.startswith(op) for pfx in prefixes):
                owners.add(fname)
    if not owners:
        errors.append(f"07 号 §4 组「{name}」的行内 ID 无法映射到 owner ADR")
        continue
    ids = set()
    for fname in owners:
        ids |= group_defined.get(fname, set())
    if len(ids) != n:
        errors.append(f"07 号 §4 组「{name}」写 {n} 条,owner ADR({', '.join(sorted(owners))})实际定义 {len(ids)} 条")
    missing = ids - truth_ids
    surplus = truth_ids - ids
    for i in sorted(missing):
        errors.append(f"07 号 §4 组「{name}」的 ID 区间**漏写**: {i}")
    for i in sorted(surplus):
        errors.append(f"07 号 §4 组「{name}」的 ID 区间**多写**: {i}")
    union_truth |= truth_ids

# 独立锚点:全部组引用的 ID 并集必须 == 实际定义总数,且每个 ID **恰好被引用一次**。
#
# ⚠️ 缺这条时,「组间互换 owner」**查不出来** —— 逐组校验里 owner 是从
#    真源单元格**反推**的(interpolated),互换后两边仍自洽。
#    实测:把 Ordering Point 行的 ID 区间改成 INV-CEP-09-IC1 ~ IC3 →
#    逐组集合相等仍成立,但 ADR-CEP-08 的 3 条 invariant **无人引用**。
#    本检查(并集 + 引用次数)提供与真源单元格**无关**的锚点。
all_defined = set(owner_defined)
if union_truth != all_defined:
    _miss = sorted(all_defined - union_truth)
    _sur = sorted(union_truth - all_defined)
    errors.append(f"07 号 §4 引用的 invariant 并集与实际定义不一致:缺 {_miss},多 {_sur}")
if total_refs != len(all_defined):
    errors.append(f"07 号 §4 引用的 invariant 总数 {total_refs} != 实际定义 {len(all_defined)}"
                  f"(存在重复引用 —— 组间互换 owner 的典型形态)")

if errors:
    for e in errors:
        print(f"  \033[0;31m✗\033[0m {e}")
else:
    print(f"  \033[0;32m✓\033[0m 07 号 §4 的 {len(rows)} 个分组条数与 owner ADR 实际定义数一致"
          f"(且第 3 列 ID 区间**集合相等**)")
    print(f"  \033[0;32m✓\033[0m 07 号 §4 引用的 invariant 总数 {total_refs} 条 == 实际定义总数")

actual = len(owner_defined)
skipped = 1
numbered = actual + skipped
print(f"  \033[0;32m✓\033[0m 实际定义 {actual} 条,编号位 {numbered} 条(含 1 条永久跳空)")

if "INV-CEP-04-IC4" in owner_defined:
    print("  \033[0;31m✗\033[0m INV-CEP-04-IC4 应为永久跳空,却出现在 owner ADR 的定义位")
    OWNERS_GAP = True
else:
    print("  \033[0;32m✓\033[0m INV-CEP-04-IC4 确为永久跳空(仅被提及,从未定义)")

mm = re.search(r"实际定义数\*\*:\*\*(\d+)", truth_text)
if mm and int(mm.group(1)) != actual:
    errors.append(f"07 号声明「实际定义数 = {mm.group(1)}」,实测 {actual}")
else:
    print(f"  \033[0;32m✓\033[0m 07 号声明的「实际定义数」与实测一致")

# ADR-CEP-00A §4 映射行口径(另一处声明总数的地方,必须同步)
_a00a_path = os.path.join(ADR, "ADR-CEP-00A-real-source-anchor-map.md")
_a00a_row = None
if os.path.exists(_a00a_path):
    for _line in open(_a00a_path, encoding="utf-8").read().splitlines():
        if _line.startswith("| §4 |") and ("编号位" in _line or "守门 invariant 总表" in _line):
            _a00a_row = _line
            break
if _a00a_row is None:
    errors.append("ADR-CEP-00A §4 找不到 invariant 总表映射行")
else:
    _nums = [int(x) for x in re.findall(r"\d+", _a00a_row)]
    if numbered not in _nums or actual not in _nums or 15 not in _nums:
        errors.append(f"ADR-CEP-00A §4 映射行含 {_nums},缺 编号位 {numbered} 或 实际 {actual} 或 组数 15")
    else:
        print(f"  \033[0;32m✓\033[0m ADR-CEP-00A §4 映射行口径与实测一致")

sys.exit(1 if (errors or OWNERS_GAP) else 0)
PYEOF
[ $? -ne 0 ] && FAILED=$((FAILED + 1))

echo ""
echo -e "${BLUE}[3/11] 06 §15.4 P 编号口径(禁 P6)${NC}"
python3 - <<'PYEOF'
import glob, os, re, sys

ROOT = os.getcwd()

# 只扫 **CEP 专属**文档 —— 06 原文 / 历史 archive / 调研附录里的 P6 是合法的
FILES = []
for pat in ("docs/adr/ADR-CEP-*.md", "docs/architecture/07-*.md",
            "docs/research/cep-*.md",
            # 注:docs/implementation/** **不**扫描 —— 实施笔记是「附属 2」层,
            #    其内容是关于 P 编号检查本身的**元讨论**(必然含 P6 字样),
            #    纳入会造假阳性。契约载体 = ADR + 07 号 + openspec + roadmap。
            "docs/designs/**/*.md",
            ".rddf/roadmap/*.md", ".rddf/roadmap/**/*.md",
            ".rddf/improvements/*.md", ".rddf/drafts/*.md",
            # 2026-10-23(Entry #30 修正):umbrella 已归档 → 必须扫归档区,否则其 4 件套
            # 脱离 P6 全禁规则的扫描范围(守门覆盖度静默下降 = 更危险)。
            # ⚠️ glob 写法:**归档目录名带日期前缀**(archive/<date>-<name>/)→ 必须用
            #    `*cpptlm-cep-umbrella` 而**不是** `*/cpptlm-cep-umbrella`(后者匹配 0)。
            #    下方 [11/11] 组有断言「归档 4 件套必须在扫描集内」兜底此类错误。
            # ⚠️ 只扫 **umbrella 自己的**归档目录(保持归档前的覆盖度):仓内另有大量
            #    历史归档 change,其文档合法地提及**项目优先级标记** `P0.5` 与旧
            #    `06 §15.4` 的 `P6` → 触发规则 A/B 大量假阳性;不写死条数(会漂移)。
            "openspec/changes/archive/*cpptlm-cep-umbrella/**/*.md",
            # 归档产出的**系统契约**(main spec)同样纳入 P6 全禁规则
            "openspec/specs/cep-core-contracts/*.md",
            "openspec/changes/*/specs/**/*.md",
            "openspec/changes/*/*.md"):
    import glob
    FILES += glob.glob(os.path.join(ROOT, pat), recursive=True)
FILES = sorted(set(FILES))

# ── 规则 B:P6 全禁 ───────────────────────────────────────────────
# 否定 / 排除语境标记(必须落在 P6 出现点前后 100 字符内)
#
# ⚠️ 「排除」单独作标记会漏放**反向语义**:「**不**排除 P6」命中「排除」→
#    被判为合规 → 假阴性。解法:**不**收紧 NEG(词序有「排除 P6」/「P6 排除」/
#    「排除** P6」等多种形态,收紧会造假阳性),而是加 REV(反向语义)词表 ——
#    命中即**判违规**,优先于 NEG。
NEG = ["不存在", "非 P", "排除", "不引入", "不得", "无 P6", "不含", "禁止",
       "冻结", "P3-P6", "禁 P6", "不引用", "不出现", "不在", "不一致", "= fail",
       "P6 口径", "非 P3-P6", "不得引用"]
# 反向语义:「不排除 / 未排除 / 不能排除 / 未必排除」= **仍可能引用 P6** → 违规
REV = ["不排除", "未排除", "不能排除", "未必排除", "非不排除"]

# ── 规则 A:06 §15.4 P 编号**白名单** ──────────────────────────────
# per spec「P 编号口径唯一(IC1)」:CEP 文档引用的 `06` P 编号
# **必须**落在 06 §15.4 口径内(P2.0 / P2.1 / P3 / P3.5 / P4 / P5)。
#
# ⚠️ **不能**对全仓所有 `P<数字>` 套白名单 —— `P0` / `P1` / `P2` 在本仓是
#    **优先级 / rdd-planner 阶段**用法(rdd-workflow 词汇),与 06 §15.4 无关:
#    实测 P0(12) / P1(24) / P2(23) 中,落在「§15.4 语境行」的只有 **1** 处。
#    故「06 §15.4 引用」判定为:
#      · 形如 P2.0 / P2.1 / P3.5 的**小数**形式(优先级永不为小数)→ 必是 §15.4 条目;
#      · 或**同一行**出现 `15.4` → 该行的整数 P 编号是 §15.4 条目。
ALLOWED_P = {"P2.0", "P2.1", "P3", "P3.5", "P4", "P5"}
# 明确声明「不引用该 P 编号」的排除语境(同一行内)
# 合并了规则 B 的 NEG 词表 + 元讨论标记(讨论「禁引/口径」本身的行)。
EXCL_P = ["无 ", "不引入", "禁 ", "排除", "不得", "不含", "不存在", "不在",
          "不引用", "禁止", "剔除", "口径", "非 ", "未 ", "P6 不",
          "不存在", "非 P", "无 P6", "冻结", "P3-P6", "禁 P6", "不出现",
          "不一致", "= fail", "非 P3-P6", "不得引用",
          "禁引", "白名单", "违规", "假阳性", "反向语义", "检查项",
          "为准", "不引用"]


def is_exclusion(line):
    return any(e in line for e in EXCL_P)


violations = []
occurrences = 0
ref_count = 0
for f in FILES:
    if not os.path.exists(f):
        continue
    text = open(f, encoding="utf-8").read()
    rel = os.path.relpath(f, ROOT)

    # ── 规则 B:P6 全禁 ─────────────────────────────────────────
    # 行级判定(与规则 A 同口径):P6 所在行必须含排除语境标记。
    # ⚠️ 原实现是「±100 字符窗口」—— 邻近的无关句可充当否定证据。
    for line in text.splitlines():
        if not re.search(r"\bP6\b", line):
            continue
        occurrences += 1
        # 反向语义(「不排除 P6」等)对**任何** P6 出现都成立 → 全局判违规
        if any(r in line for r in REV):
            violations.append((rel, f"[反向语义] {line.strip()[:110]}"))
            continue
        # ⚠️ 本仓 `P<数字>` 有**三种**互不相干的词汇:
        #    ① 06 §15.4 阶段编号(P2.0/P2.1/P3/P3.5/P4/P5)—— 契约管制的对象
        #    ② 优先级 / rdd-planner 阶段(P0/P1/P2)
        #    ③ 调研文档的**发现编号**(P1..P28)
        #    故「P6 是否违规」取决于它**是不是 06 编号引用** —— 以 06 引用标记
        #    (该行提到 06 的 §11.4 / §15.4 / inter-module-network)为准。
        is_06_ref = any(mk in line for mk in ("§11.4", "§15.4", "inter-module-network"))
        if not is_06_ref:
            continue
        if is_exclusion(line):
            continue
        violations.append((rel, f"[P6 违规] {line.strip()[:110]}"))

    # ── 规则 A:06 §15.4 P 编号白名单(行级) ───────────────────────
    for line in text.splitlines():
        in_154 = "15.4" in line
        for m in re.finditer(r"\bP(\d+(?:\.\d+)*)\b", line):
            num = m.group(1)
            tok = "P" + num
            is_decimal = "." in num
            if not (is_decimal or in_154):
                continue          # 非 06 §15.4 引用(如优先级 P0 / P1 / P2)
            # 区间简写(如 "P2-P5" / "P2.0-P5"):两端单独判,中间不判
            after = line[m.end():m.end() + 2]
            before = line[max(0, m.start() - 2):m.start()]
            if after.startswith("-P") or before.endswith("P-"):
                continue
            ref_count += 1
            if tok in ALLOWED_P:
                continue
            if is_exclusion(line):
                continue          # 明确声明不引用该编号(如「无 P6」)
            violations.append((rel, f"[口径外 P 编号 {tok}] {line.strip()[:100]}"))

if violations:
    print(f"  \033[0;31m✗\033[0m P 编号口径违规 {len(violations)} 处(扫 {len(FILES)} 个 CEP 文档):")
    for path, ctx in violations[:10]:
        print(f"      {path}: {ctx}")
    sys.exit(1)
print(f"  \033[0;32m✓\033[0m P6 共 {occurrences} 处(扫 {len(FILES)} 个 CEP 文档),全部处于排除/否定语境")
print(f"  \033[0;32m✓\033[0m 06 §15.4 P 编号引用 {ref_count} 处,全部落在白名单 {sorted(ALLOWED_P)} 内")

PYEOF
[ $? -ne 0 ] && FAILED=$((FAILED + 1))

echo ""
echo -e "${BLUE}[4/11] ADR 交叉引用可达性${NC}"
BROKEN=0
for f in "${ADR_LIST[@]}"; do
  [ -f "docs/adr/$f" ] || continue
  # 全量相对 md 链接(不限 ../adr/ —— 也覆盖指向 docs/architecture/ 等)
  for link in $(grep -oE '\]\((\.\./)?[A-Za-z0-9_./-]+\.md\)' "docs/adr/$f" | sed -E 's/.*\((.*)\)/\1/' | sort -u); do
    case "$link" in http*) continue ;; esac
    target="docs/adr/$link"
    if [ ! -f "$target" ]; then fail "$f → 断链 $link"; BROKEN=$((BROKEN + 1)); fi
  done
done
[ "$BROKEN" -eq 0 ] && pass "ADR 之间的相对链接无断裂(全量 .md 链接)"

echo ""
echo -e "${BLUE}[5/11] Stage 0 umbrella change 4 件套${NC}"
# 2026-10-23(Entry #29):umbrella 归档后路径带日期前缀 → 动态解析两态
CH="$(ls -d openspec/changes/cpptlm-cep-umbrella openspec/changes/archive/*cpptlm-cep-umbrella 2>/dev/null | head -1)"
[ -n "$CH" ] || { echo "  ✗ 找不到 umbrella change(active 或 archive 均未命中)"; exit 1; }
MISS=0
for f in "proposal.md" "design.md" "tasks.md" "specs/cep-core-contracts/spec.md"; do
  [ -f "$CH/$f" ] || { fail "缺 $CH/$f"; MISS=$((MISS + 1)); }
done
[ "$MISS" -eq 0 ] && pass "umbrella change 4 件套齐备"

echo ""
echo -e "${BLUE}[6/11] Stage 0 契约产物${NC}"
for f in "include/tlm/cep/protocol_instance.schema.json" "experimental/metrics.schema.json" "experimental/.gitkeep"; do
  [ -f "$f" ] && pass "$f 存在" || fail "$f 缺失"
done

echo ""
echo -e "${BLUE}[7/11] M1 产物冻结校验(冻结后不可改)${NC}"
# ⚠️ 不可裸调用:本脚本 `set -e`,裸调用的失败会**立即中止**整个脚本
#    → 后续组与汇总永不打印,且下面的 `if [ $? -ne 0 ]` 是死代码(2026-10-22 代码复审 S3)
if ! bash scripts/test/cep_freeze_check.sh; then FAILED=$((FAILED + 1)); fi

echo ""
echo -e "${BLUE}[8/11] CEP ↔ ArchForge 接口对齐清单漂移检测${NC}"
if ! bash scripts/test/cep_archforge_alignment_check.sh; then FAILED=$((FAILED + 1)); fi

echo ""
echo -e "${BLUE}[9/11] ArchForge 协议实例声明 ↔ 本仓 schema/fabric 对齐检查${NC}"
# ⚠️ 「跳过」与「跑过」必须可区分:脚本首行标 [RUN] / [SKIP],退出码都是 0。
#    跳过时**不算通过**(跨仓外部条件缺失不是「声明对齐已被验证」)。
if DECL_OUT="$(bash scripts/test/cep_decl_alignment_check.sh 2>&1)"; then
    DECL_RC=0
else
    DECL_RC=$?
fi
echo "$DECL_OUT"
if [ "$DECL_RC" -ne 0 ]; then
    fail "ArchForge 协议实例声明 ↔ 本仓 schema/fabric 对齐检查失败(见上)"
    FAILED=$((FAILED + 1))
elif echo "$DECL_OUT" | grep -q "\[SKIP\]"; then
    warn "ArchForge 仓缺席 → 本组跳过(跨仓外部条件缺失,非本仓缺陷;判据 C-4 的机器化未被本环境验证)"
else
    pass "ArchForge 协议实例声明 ↔ 本仓 schema/fabric 对齐检查通过"
fi

echo -e "${BLUE}[10/11] 传输层 CATEGORY_* 分层纪律(对应 A14-1 / A14-2)${NC}"
# 契约:6 类一致性消息词表落在 **Coherence Interface 层**(per ADR-CEP-12 §8 IC2 ③),
#       CATEGORY_* 保持**传输层 3 类** —— 不得为承载该词表而扩成 6 类(词表混层)。
# 机器强制 3 项:
#   ① CATEGORY_* 恰为 3 个声明,且值可**静态求值**且为 0/1/2 连续
#      → 保证 soc_internal_noc.hh 的「索引对齐」分桶不会静默折叠任何类别
#         (route_flit: `vc = (cat < VC_COUNT) ? cat : VC_SNP`,VC_COUNT=4)
#      ⚠️ **不可静态求值即判红**(不是跳过)—— 堵住「写成表达式/花括号初始化」绕过
#   ② flit 类别消费点引用的常量**必须真实存在**
#      → 封死 M-4 的「幽灵类别」(注释描述了从未实现的 4 类 CATEGORY_ 集)
#      ⚠️ 覆盖**注释与代码**(M-4 的幽灵类别正在注释里);误报时改写措辞即可
#   ③ VC_COUNT 与「4 独立 VC」物理模型声明一致
CATEGORY_FILE="include/bundles/noc_bundles_tlm.hh"

# ① 静态求值(C++ 语义:十进制 / 十六进制 / 引用先前常量)
if CAT_EVAL="$(python3 - "$CATEGORY_FILE" << 'PYEOF'
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
decl = re.compile(r"CATEGORY_([A-Z_]+)\s*(?:=|\{)\s*([^;}\n]+)")
vals, names, bad = {}, [], []
for m in decl.finditer(src):
    name, expr = m.group(1), m.group(2).strip()
    if name in vals:
        continue
    expr = expr.rstrip(",")
    try:
        v = int(expr, 0)
    except ValueError:
        if re.fullmatch(r"CATEGORY_[A-Z_]+", expr):
            ref = expr[len("CATEGORY_"):]
            if ref in vals:
                v = vals[ref]
            else:
                bad.append(f"{name} = {expr}(引用未定义)"); continue
        else:
            bad.append(f"{name} = {expr}(不可静态求值)"); continue
    vals[name] = v; names.append((name, v))
if bad:
    print("UNPARSEABLE:" + "; ".join(bad)); sys.exit(0)
print("N=%d" % len(names))
print("VALS=" + " ".join(str(v) for _, v in sorted(names, key=lambda kv: kv[1])))
print("NAMES=" + " ".join("CATEGORY_" + n for n, _ in names))
PYEOF
)"; then :; else CAT_EVAL="UNPARSEABLE:python 失败"; fi

if printf '%s' "$CAT_EVAL" | grep -q '^UNPARSEABLE:'; then
    fail "CATEGORY_* 声明不可静态求值 → **判红**(不跳过):$(printf '%s' "$CAT_EVAL" | sed 's/^UNPARSEABLE://')"
else
    CAT_N="$(printf '%s\n' "$CAT_EVAL" | sed -n 's/^N=//p')"
    CAT_VALS="$(printf '%s\n' "$CAT_EVAL" | sed -n 's/^VALS=//p')"
    CAT_NAMES="$(printf '%s\n' "$CAT_EVAL" | sed -n 's/^NAMES=//p')"
    if [ "$CAT_N" -eq 3 ] && [ "$CAT_VALS" = "0 1 2" ]; then
        pass "CATEGORY_* 恰为 3 类且值域连续 0..2($(printf '%s' "$CAT_NAMES" | sed 's/CATEGORY_//g');传输层类别,6 类一致性词表不落本层)"
    else
        fail "CATEGORY_* 应恰为 3 类且值域 0..2 —— 实测 $CAT_N 类,值域 [$CAT_VALS](A14-2:扩类会静默折叠越界类别)"
    fi
    CATEGORY_DEFS="$(printf '%s' "$CAT_NAMES" | tr ' ' '\n' | sed 's/$/ = 0/' | sort -u)"
fi

GHOST=0
# 作用域收窄:只检查**确实消费 flit 类别**的文件 —— `CATEGORY_*` 在本仓另有命名空间
# (如 include/core/error_category.hh 的 CATEGORY_HH),不可全仓按名字一刀切。
# 覆盖 `using namespace bundles;` + 裸类别名的写法(否则该路径 100% 漏检)。
CATEGORY_REF_FILES="$(grep -rlE 'flit_category|NoCFlitBundle|noc_bundles_tlm|using namespace bundles' include/ src/ --include='*.hh' --include='*.cc' 2>/dev/null || true)"
for ref_file in $CATEGORY_REF_FILES; do
    for tok in $(grep -oE 'CATEGORY_[A-Z_]{2,}' "$ref_file" 2>/dev/null | sort -u || true); do
        if ! printf '%s' "$CATEGORY_DEFS" | grep -q "^$tok "; then
            fail "幽灵类别: $ref_file 引用了不存在的 $tok(M-4:引用了未定义的 CATEGORY_*;注释提及亦计)"
            GHOST=$((GHOST + 1))
        fi
    done
done
[ "$GHOST" -eq 0 ] && pass "flit 类别消费点($(printf '%s' "$CATEGORY_REF_FILES" | wc -w | tr -d ' ') 个文件)无「幽灵类别」(M-4 防复发)"

VC_COUNT_DECL="$(grep -oE 'static constexpr unsigned VC_COUNT = [0-9]+' include/tlm/cluster/soc_internal_noc.hh 2>/dev/null | grep -oE '[0-9]+$' || true)"
if [ "$VC_COUNT_DECL" = "4" ] && grep -q '4 独立 VC' include/tlm/cluster/soc_internal_noc.hh 2>/dev/null; then
    pass "VC_COUNT = 4 与「4 独立 VC」物理模型声明一致"
else
    fail "VC_COUNT($VC_COUNT_DECL) 与 soc_internal_noc.hh 的「4 独立 VC」声明不一致"
fi

echo ""
echo -e "${BLUE}[11/11] 归档后状态一致性(7 类客观断言)${NC}"
# ⚠️ 2026-10-23(Entry #32)重新设计:前两版的「散文语义正则」被 critic 实测出
#    10/13 同义改述绕过 + 行尾贴白名单词即可绕过 → 属 overclaim。
#    本版只做**客观可判定**的断言(数字 / 同行关联 / glob 覆盖),不再声称「理解语义」。
python3 - "$PROJECT_ROOT" << 'PYEOF'
import json, os, re, subprocess, sys
root = sys.argv[1]
failed = []

LIVE = [
    ".rddf/roadmap/objectives/obj-cep-platform.md",
    ".rddf/roadmap/phases/phase-10.md",
    ".rddf/improvements/cpptlm-cep-umbrella.md",
    ".rddf/roadmap.md",
    "AGENTS.md",
    # ADR 是**状态入口之一**(§4.7.3「① 起步」要求实施者读 ADR)→ 纳入扫描集
    "docs/adr/ADR-CEP-00-platform-charter-and-governance.md",
]
# 最近一次「完成归档」日 —— Status Update 块的豁免按**块内日期**判定:
#   归档日之前的块(如 Status Update 2026-10-21)→ 豁免(历史);
#   归档日及之后的块(如 Status Update 2026-10-23)→ **不豁免**(必须写当前态)。
ARCHIVE_DAY = "2026-10-23"
# 区块级豁免:**标题**含下列词 → 该区块整体豁免(改标题会留痕;行尾贴词不能绕过)
EXEMPT_HEAD = re.compile(r"历史|沿革|误读|处置|单源纪律|自检|遗留|Status Update")
_DATE = re.compile(r"20\d{2}-\d{2}-\d{2}")


def own_date(head, h2):
    """块的**自身日期**(不取正文里的引用!):标题日期 → 否则所属 h2 的日期 → 否则 None。
    ⚠️ 不能用「正文里出现过的最大日期」:历史块正文里**引用**一个近期日期
    (如「后续见 2026-10-23 Entry #33」)会把整块误判为「最新」→ 对正确文档报红。"""
    d = _DATE.findall(head)
    if d:
        return max(d)
    d = _DATE.findall(h2)
    if d:
        return max(d)
    return None


def exempt_block(head, body, h2=""):
    """区块级豁免:标题(或所属 h2)命中即豁免;但 **Status Update** 按**块自身日期**判定 ——
    归档日之前的块豁免(历史),归档日及之后的块**不豁免**(必须写当前态);
    **无日期**的 Status Update 块视为最新(不豁免)—— 宁可多报,不可漏报。"""
    scope = head + "\n" + h2
    if not EXEMPT_HEAD.search(scope):
        return False
    if "Status Update" in scope:
        d = own_date(head, h2)
        if d is None or d >= ARCHIVE_DAY:
            return False
    return True
# 条件句不是状态断言(「若…则…」「当…时」)→ 跳过,避免对**正确文档**报红。
# ⚠️ **不得用裸字符子串**(2026-10-24 critic 实测):`则` 会命中 规则/原则/准则/否则/
#    法则/守则/细则 → 一个词就能让 ② 与 ⑤ 同时静默失守。
#    规则:条件标记必须出现在**分句开头**(切句后 ^ 锚定),或成对出现「若…则」。
# ⚠️ `当` **不得**裸用:「当前」「当时」「当然」「当务之急」都会命中 → 句首「当前 Stage 0
#    尚未归档」这类**最自然的陈旧投影句式**会被永久跳过。必须成对:`当…时`。
# ⚠️ **不得**含 `当` 分支:`当…时` 的中段可为空 ⇒ 「当时…」「当前时段…」被吞。
#    条件词只保留「若/假如/如若/一旦」+ 成对「若…则…」。
CONDITIONAL = re.compile(r"^\s*(若|假如|如若|一旦)")
# 当前态**强**限定词。该行任一句含它 → 按**当前态**处理并检查(整行匹配)。
# ⚠️ **不用行级历史标记白名单**:行尾贴一个「旧值」即可让 ① 静默失守
#    (这正是闸立项时否决过的设计)—— 故本实现**只**看「有没有当前态强限定词」。
CUR_STRONG = re.compile(r"当前|现行|真实|现在|目前")
# 该行是否在陈述**当前**状态:任一句含**当前态强限定词**即为当前。
#   「**历史**:截至 2026-10-20,并发 3/3」 → 无强限定词 → 历史 → 豁免
#   「历史遗留:当前 baseline = <数值>」    → 含「当前」 → 当前 → **不豁免**
#   「**当前 baseline = <数值>**(旧值)」   → 含「当前」 → 当前 → **不豁免**
def line_is_current(ln):
    for sent in re.split(r"[。；，、\n;!?！？,]", ln):
        if CUR_STRONG.search(sent):
            return True
    return False
# 实体(状态归属的主体)
ENTITY = re.compile(r"umbrella|cpptlm-cep-umbrella|Stage 0|\bR10\b")
# 旧状态词(表示「尚未归档 / 仍占用」)
# 旧状态词的**词形清单**(穷举可枚举的形态,不做语义解析):
#   归档类:未/尚未/待/不/无法/方可 + 归档|archive
#   回执类:真人回执 / 回执缺失 / 未收到真人 / 意见未回 / 需回执
#   计数类:尚余 / 未勾选 / 105 / 99 / 105 task
#   并发类:仍 active / 未释放 / pending / in-progress / 处于 Proposed / Proposed 状态 / 升为 Proposed
STALE_STATE = re.compile(
    r"(未|尚未|待|不|无法|需[^。;]{0,15}方可)\s*归档|"
    r"(未|尚未|待|不|无法|not)\s*archive|not archived|pending|"
    r"真人回执|回执缺失|未收到真人|意见未回|外部评审[^。;]{0,8}未回|需回执|"
    r"尚余|未勾选|105\s*/\s*99|99\s*/\s*105|105\s*task|"
    r"仍 active|未释放|in-progress|处于 Proposed|Proposed 状态|未闭合|"
    r"升为 \*{0,2}Proposed|仍为 \*{0,2}Proposed")
# 当前状态词(表示「已归档 / 已释放」)
CURRENT_STATE = re.compile(r"已归档|已完成归档|已释放|已闭合|归档前|曾占用|已被归档|已改写|曾 = |已计入历史|历史描述|措辞为历史|以本节为准|已失效|已登记|已作废")


def live_text(path):
    """取活文档区段:objective 截断到 §11 台账之前(台账是 append-only 历史)。"""
    try:
        t = open(os.path.join(root, path), encoding="utf-8").read()
    except OSError:
        return None
    if "obj-cep-platform.md" in path:
        i = t.find("## §11 决策台账")
        if i > 0:
            t = t[:i]
    return t


def blocks(text):
    """按标题切块:(标题行, 块内容, 所属 h2 标题)。无标题的散文归入块标题 ''。
    `###`/`####` 子节**继承**其所属 `##` 的上下文 —— 否则 `## Status Update` 下的
    `### 8.3` 子节会脱离豁免(ADR 的历史状态段会被误判)。"""
    out, cur, in_code, h2 = [], ["", [], ""], False, ""
    for line in text.splitlines():
        if line.strip().startswith("```"):
            in_code = not in_code
        if not in_code and re.match(r"^#{2,4} ", line):
            out.append((cur[0] if cur else "", cur[1], cur[2]))
            cur = [line, [], h2]
            if line.startswith("## "):
                cur[2] = line
                h2 = line          # 记住最近的 h2(供后续 ###/#### 子节继承)
        else:
            cur[1].append(line)
    out.append((cur[0] if cur else "", cur[1], cur[2]))
    return out


# ── 断言 ①:KPI baseline 数字 == 实测 ────────────────────────────────
try:
    raw = subprocess.run(["openspec", "list", "--json"], cwd=root, capture_output=True,
                         text=True, timeout=120).stdout
    data = json.loads(raw) if raw.strip() else []
    changes = data if isinstance(data, list) else data.get("changes", [])
    active = len(changes)
except Exception:
    active = None
if active is None:
    failed.append("无法实测 openspec list --json(KPI baseline 断言无法执行)")
else:
    expect = f"{active}/3"
    # 只抓「当前/现行/真实 … N/3」的**当前态**表述(历史叙述"0/3 → 1/3"不含这些限定词)
    CUR = re.compile(r"(当前|现行|真实|现在|目前|截至)[^|\n]{0,80}?([0-9])/3")
    vals = {}
    # ⚠️ 断言① 与断言② **同一块级豁免规则**:ADR 的**归档日之前的块**跳过
    #    (正文不可变,历史数值是合法叙述),但**归档日及之后的 Status Update 块**
    #    必须写当前态 → **不跳过**(整文件排除会让新追加的段失守)。
    for p in LIVE:
        t = live_text(p)
        if not t:
            continue
        is_adr = p.startswith("docs/adr/")
        for head, body, h2 in blocks(t):
            if is_adr and "Status Update" not in (head + h2):
                continue   # ADR 的**正文块**(不可变)里的历史基线叙述不参与本断言;
                           # 只有 Status Update 块(可追加)里的当前态数值被检查
            if exempt_block(head, body, h2):
                # 豁免块:逐行看 —— 带**历史标记**的行放行(合法历史叙述),
                # 不带历史标记的行仍查(否则 obj 豁免块里的当前态数值会失守)
                for ln in body:
                    if not line_is_current(ln):
                        continue          # 合法历史叙述 → 放行
                    # ⚠️ **每行只取第一个**匹配:第一个限定词后的第一个 N/3 才是该行的
                    #    当前态数值。取全部会把同行后续的历史引用也算进来
                    #    (如「当前并发 0/3(截至 2026-10-20 曾为 3/3)」)→ 对**正确**文档误报红。
                    m = CUR.search(ln)
                    if m:
                        vals.setdefault(p, set()).add(f"{m.group(2)}/3")
                continue
            for ln in body:
                if not line_is_current(ln):
                    continue              # 合法历史叙述 → 放行
                # ⚠️ 匹配在**整行**上做:限定词与 N/3 常被标点隔开
                #    (如「现行基线 (…): **0 个 active** → **0/3**」)——
                #    切句后限定词与数值分居两句 → **D1 头号目标行会掉出覆盖**。
                m = CUR.search(ln)      # ⚠️ 每行只取第一个匹配(见上)
                if m:
                    vals.setdefault(p, set()).add(f"{m.group(2)}/3")
    bad = {p: sorted(v) for p, v in vals.items() if v != {expect}}
    if bad:
        failed.append(f"KPI baseline 与实测({expect})不一致:{bad}")
    else:
        print(f"  \033[0;32m✓\033[0m KPI baseline 数字一致(实测 {expect} = 所有活文档的当前态表述)")

# ── 断言 ②:归档状态互斥(实体 + 旧状态词 同行,区块级豁免)────────────
hits = []
for p in LIVE:
    t = live_text(p)
    if not t:
        continue
    for head, body, h2 in blocks(t):
        if exempt_block(head, body, h2):
            continue          # 区块级豁免
        for ln in body:
            # 句级判定:同一**句**内同时出现「实体」与「旧状态词」才算冲突
            # (行级判定会误报:条件句/协议说明句常在同一行内先后出现两者)
            for sent in re.split(r"[。；，、\n;!?！？,]", ln):   # 全角+半角+换行+逗号顿号(中文主力是全角标点)
                if CONDITIONAL.search(sent):
                    continue      # 条件句不是状态断言
                if ENTITY.search(sent) and STALE_STATE.search(sent) and not CURRENT_STATE.search(sent):
                    hits.append(f"{p}: {sent.strip()[:110]}")
                    break
if hits:
    failed.append("活文档仍有「实体 + 旧状态词」同行出现(共 %d 处):" % len(hits))
    for h in hits[:5]:
        failed.append("      · " + h)
else:
    print("  \033[0;32m✓\033[0m 无「实体 + 旧状态词」同行出现(区块级豁免)")

# ── 断言 ③:冻结成员数字集合 == {30} ───────────────────────────────
FZ = [".rddf/roadmap/objectives/obj-cep-platform.md", ".rddf/roadmap/phases/phase-10.md",
      ".rddf/improvements/cpptlm-cep-umbrella.md", ".rddf/roadmap.md",
      "docs/ONBOARDING.md", "scripts/README.md", "docs/adr/ADR-CEP-NUMBER-MAP.md"]
try:
    n = json.load(open(os.path.join(root, "docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json"),
                       encoding="utf-8"))["artifact_count"]
except Exception:
    n = None
NUM = re.compile(r"冻结面[^0-9]{0,12}([0-9]{2})[^0-9]{0,4}成员|([0-9]{2})[^0-9]{0,4}冻结成员|冻结成员[^0-9]{0,6}([0-9]{2})|冻结面成员数[^0-9]{0,6}([0-9]{2})")
setn = set()
for p in FZ:
    t = live_text(p)
    if not t:
        continue
    for m in NUM.finditer(t):
        gs = [g for g in m.groups() if g]
        setn.add(gs[0] if gs else "")
if n is None:
    failed.append("无法读取冻结清单 artifact_count")
elif setn != {str(n)}:
    failed.append(f"冻结成员数字集合 {sorted(setn)} != {{{n}}}")
else:
    print(f"  \033[0;32m✓\033[0m 冻结成员数字集合一致 {{{n}}}")

# ── 断言 ④:旧任务计数(客观数字形态,不依赖实体锚点)───────────────
OLDCOUNT = re.compile(r"\b105\s*[/／:：]\s*99\b|\b99\s*[/／:：]\s*105\b|105\s*task|6\s*项未勾|尚余[^。;]{0,6}项未勾")
c_hits = []
for p in LIVE:
    t = live_text(p)
    if not t:
        continue
    for head, body, h2 in blocks(t):
        if exempt_block(head, body, h2):
            continue
        for ln in body:
            if OLDCOUNT.search(ln):
                c_hits.append(f"{p}: {ln.strip()[:100]}")
if c_hits:
    failed.append("活文档仍有旧任务计数形态(共 %d 处):" % len(c_hits))
    for h in c_hits[:4]:
        failed.append("      · " + h)
else:
    print("  \033[0;32m✓\033[0m 无旧任务计数形态(105 / 99 等)")

# ── 断言 ⑤:ADR 的「必须追加 Status Update」回归锁 ──────────────────
# 规则:若某 ADR 的**非豁免块**仍写着旧状态(实体 + 旧状态词),则该 ADR **必须**
#       存在一个「自身日期 ≥ 归档日」的 Status Update 块登记其失效。
#       —— 这条把「B1 的修复路径 = 追加 Status Update」变成**可回归验证**的,
#       而不只是「追加后的内容被检查」。
# ⚠️ 作用域 = LIVE 里的 ADR(**硬编码**):新增「状态入口」ADR 时必须同步加入 LIVE,
#    否则 ⑤ 静默不覆盖该 ADR(与本仓其它硬编码列表同类的脆弱点)。
adrs = [x for x in LIVE if x.startswith("docs/adr/")]
a_fail = []
for p in adrs:
    t = live_text(p)
    if not t:
        continue
    has_new_su = False
    stale_in_adr = False
    for head, body, h2 in blocks(t):
        d = own_date(head, h2)
        if "Status Update" in (head + h2) and d is not None and d >= ARCHIVE_DAY:
            has_new_su = True
        # ⚠️ 这里**必须扫全部块(含豁免块)**:B1 的陈旧句位于 `### 8.3`
        #    (继承 2026-10-21 的 h2 → 豁免)→ 只扫非豁免块时 stale_in_adr 恒 False,
        #    ⑥ 永不触发(2026-10-24 实测:删掉 SU 段闸仍全绿)。
        # ⚠️ ⑤ **不使用**条件句跳过:条件式表述的旧结论**仍算**旧结论
        #    (宁可要求追加 SU,也不让「若…则…」把 ⑤ 静默失守)
        for sent in re.split(r"[。；，、\n;!?！？,]", "\n".join(body)):
            if ENTITY.search(sent) and STALE_STATE.search(sent) and not CURRENT_STATE.search(sent):
                stale_in_adr = True
    if stale_in_adr and not has_new_su:
        a_fail.append(f"{p}: ADR 内仍写旧状态(任意块,含历史块),但**没有** ≥ {ARCHIVE_DAY} 的 Status Update 块登记其失效")
if a_fail:
    failed.append("ADR 的「必须追加 Status Update」回归锁未满足:")
    for x in a_fail:
        failed.append("      · " + x)
else:
    print("  \033[0;32m✓\033[0m ADR 的「必须追加 Status Update」回归锁满足")

# ── 断言 ⑦:移交链的可达性与 owner 活性 ────────────────────────────
# 背景:Entry #22 S8 —— 「6 项移交后**无任何机器闸**保证队列 #2/#4/#6 承接」→
#       移交链**只有人工可见性**。2026-10-24 planner 审查(Entry #46)正是在这类
#       漂移上栽了跟头(移交基于**陈旧快照**:已完成的 S1-1 仍写作「待消费」)。
ARCH_TASKS = "openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/tasks.md"
OBJ_REL = ".rddf/roadmap/objectives/obj-cep-platform.md"
PHASE_REL = ".rddf/roadmap/phases/phase-10.md"


def _mk_set(text):
    """抽出文本里出现的 S1-N 标记集合;**区间写法**(S1-1 ~ S1-5)须展开。"""
    out = set()
    for m in re.finditer(r"S1-(\d+)\s*(?:~\s*S1-(\d+))?", text):
        a = int(m.group(1))
        b = int(m.group(2)) if m.group(2) else a
        for k in range(min(a, b), max(a, b) + 1):
            out.add(f"S1-{k}")
    return out


def _section(text, head_prefix):
    """取 obj 中以 head_prefix 开头的 ### 小节正文(到下一个 ### 为止)。"""
    i = text.find(head_prefix)
    if i < 0:
        return ""
    j = text.find("\n### ", i + 1)
    return text[i:j] if j > 0 else text[i:]


def _rows(table):
    """取表格行(以 | 开头且非表头/分隔行)。"""
    out = []
    for ln in table.splitlines():
        s = ln.strip()
        if not s.startswith("|"):
            continue
        if set(s) <= set("|-: "):      # 分隔行
            continue
        if "---" in s:                  # 表头分隔
            continue
        out.append(s)
    return out


try:
    _arch_txt = open(os.path.join(root, ARCH_TASKS), encoding="utf-8").read()
    _obj_txt = open(os.path.join(root, OBJ_REL), encoding="utf-8").read()
    _phase_txt = open(os.path.join(root, PHASE_REL), encoding="utf-8").read()
except Exception as _e:
    failed.append(f"断言 ⑦ 无法读取承接载体({ARCH_TASKS} / {OBJ_REL} / {PHASE_REL}): {_e}")
    _arch_txt = _obj_txt = _phase_txt = ""

_handed = set()          # 归档件里「移交」的标记
_arch_all = _mk_set(_arch_txt)   # 归档件里存在过的全部标记
for _ln in _arch_txt.splitlines():
    if "⏭(移交)" in _ln:
        _handed |= _mk_set(_ln)

_s40 = _section(_obj_txt, "### 4.0 ")
_s42 = _section(_obj_txt, "### 4.2 ")
_m40 = _mk_set(_s40)
_m42 = _mk_set(_s42)
_mph = _mk_set(_phase_txt)

_fail7 = []
# ① 可达 + ② 复述一致
for _mk in sorted(_handed):
    if _mk not in _m40:
        _fail7.append(f"{_mk}: 归档件标为「移交」,但 obj §4.0 排队表**未提及**(移交项在路线图上消失)")
    if _mk not in _m42:
        _fail7.append(f"{_mk}: obj §4.2(§4.0 的同义复述)**未提及** → 两处已漂移")
# ③ owner 活性:凡提及移交标记的行,其 change 不得是已归档 change
import glob as _g7
_archived = set()
for _p in _g7.glob(os.path.join(root, "openspec/changes/archive/*")):
    _nm = os.path.basename(_p.rstrip("/"))
    for _m2 in re.finditer(r"cpptlm-cep-[a-z0-9-]+", _nm):
        _archived.add(_m2.group(0))
for _tbl, _tag in ((_s40, "§4.0"), (_s42, "§4.2")):
    for _row in _rows(_tbl):
        if not (_mk_set(_row) & _handed):
            continue
        for _m3 in re.finditer(r"`(cpptlm-cep-[a-z0-9-]+)`", _row):
            if _m3.group(1) in _archived:
                _fail7.append(
                    f"{_tag}: 行内提及移交标记,但其 change `{_m3.group(1)}` **已归档** → 无主债"
                    f"(应重指向活队列行)")
# ④ 反向:不凭空(活文档出现的标记必须在归档件里存在)
for _mk in sorted((_m40 | _m42 | _mph) - _arch_all):
    _fail7.append(f"{_mk}: 活文档出现该标记,但归档件中**不存在** → 凭空造的移交项")

if _fail7:
    failed.append("移交链断言 ⑦ 未满足:")
    for _x in _fail7:
        failed.append("      · " + _x)
else:
    print(f"  \033[0;32m✓\033[0m 移交链断言 ⑦ 满足(移交 {len(_handed)} 项 · 可达 + 复述一致 + owner 活性)")

# ── 断言 ⑥:glob 覆盖(归档件在 P6 扫描集内)─────────────────────────
import glob as _g
n_hit = len(_g.glob(os.path.join(root, "openspec/changes/archive/*cpptlm-cep-umbrella/**/*.md"), recursive=True))
if n_hit < 4:
    failed.append(f"归档区 P6 扫描 glob 只命中 {n_hit} 个文件(应 >= 4)")
else:
    print(f"  \033[0;32m✓\033[0m 归档区 P6 扫描 glob 命中 {n_hit} 个文件")

if failed:
    for f in failed:
        print("  \033[0;31m✗\033[0m " + f)
    sys.exit(1)
PYEOF
if [ $? -ne 0 ]; then FAILED=$((FAILED + 1)); fi

echo ""
echo "════════════════════════════════════════"
if [ "$FAILED" -eq 0 ]; then
  echo -e "${GREEN}✅ CEP 契约同步检查全部通过${NC}"
  [ "$STRICT" = true ] && exit 0
  exit 0
else
  echo -e "${RED}❌ CEP 契约同步检查发现 $FAILED 处问题${NC}"
  [ "$STRICT" = true ] && exit 1
  exit 0
fi
