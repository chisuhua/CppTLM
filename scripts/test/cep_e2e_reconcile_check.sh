#!/usr/bin/env bash
# scripts/test/cep_e2e_reconcile_check.sh
# **双向闸**(per 台账 Entry #295 裁定 2 = retention **丙**;Entry #303)
#
# 目的:让「台账结论」首次**可对账** —— 台账条目 ↔ 仓内**已提交**的 run 产物。
#   ① **零 orphan**:仓内每份**已提交**的 `experimental/run-*/metrics.json`
#      ⇒ 必须**恰好**对应 `test/cep_e2e_runs.jsonl` 的**一条**条目;
#   ② **反向零 orphan**:jsonl 的每条条目 ⇒ 其 `metrics_path` 必须**存在且已被 git 跟踪**;
#   ③ **sha256 相等**:条目的 `metrics_sha256` == 仓内文件的 sha256;
#   ④ **7 字段齐备**:`run_id` / `config_sha256` / `metrics_sha256` / `metrics_13`
#      / `verification_mode` + `verification_scope` / `ic3_verdict` / `hypothesis_id`
#      / `topology_tag`(= 7 字段口径,`verification_mode` 与 `verification_scope` 合为一项);
#   ⑤ **verdict 三态词表**:✓ 通过 / ❌ 违例 fail / ⚪ 不适用(⛔ 不得二值「绿 / 红」)。
#
# ── 2026-11-13(change `cep-evidence-tier-declaration` / per `ADR-CEP-20` 决策 6(ii)):
#    **档位登记三判据**(T1~T3)+ `evidence_scope` 必填 —— 防「跨档并列以机器可读形式发生」
#      · `evidence_scope` **必填**(缺键 ⇒ 签名 `必填键缺失:evidence_scope`)
#      · **T1** 每条 jsonl 条目的 `run_id` **必须**在档位表(`test/cep_e2e_run_tiers.json`)里
#        ⇒ 否则 `档位未登记`
#      · **T2** **档 0** 条目的 `evidence_scope` **必须**含档位词(真源见脚本内 `TIER_WORDS`)
#        ⇒ 否则 `档位声明缺失`
#      · **T3** 档位表的每个 `run_id` **必须**存在于 jsonl(**反向零 orphan**,防僵尸登记)
#        ⇒ 否则 `僵尸档位登记`
#    ⚠️ **不改** `test/cep_e2e_runs.jsonl`(append-only 台账)⇒ 档位走**旁挂表**;
#    ⚠️ 档位词**真源单点** = 本脚本 `TIER_WORDS`(注释指向 `ADR-CEP-20` 决策 6 表格)。
#
# ⚠️ **豁免边界**(per Entry #295 ⑤):历史 127 run **grandfather** ⇒
#   本闸只覆盖**本 change 首批入库**的 run(清单 = `test/cep_e2e_runs.jsonl`);
#   历史 run **不入库** ⇒ 不产生 orphan。⛔ 若将来批量回填,须同步扩本闸的覆盖口径。
#
# 用法:
#   bash scripts/test/cep_e2e_reconcile_check.sh               # 基线正控(真实仓)
#   bash scripts/test/cep_e2e_reconcile_check.sh --mutations   # + 4 条决定性变异(影子副本)
#   CEP_RECONCILE_JSONL=... CEP_RECONCILE_TIERS=... bash ...   # 影子副本模式(变异用)
#
# ── 2026-11-14(change `cep-evidence-tier2-minimal-slice` / per `ADR-CEP-20` 决策 6):
#    **档 2 的非 run 载体登记面**判据(**T4** / **T5**)—— 补「跨档聚合禁令」在新载体上的机器面
#      · **T4** 档 2 条目字段齐备(含档位词 + 载体文件**实际存在** + `run_produced == false`)
#        ⇒ 否则 `档 2 登记字段缺失` / `档 2 载体文件不存在` / `档 2 产 run(与裁决 R3 冲突)`
#      · **T5a** 禁止**肯定式**「通过态」表述(否定语境豁免)⇒ 否则 `肯定式通过态表述`
#      · **T5b** 禁止夸大短语(`检测力已证` / `全族达成` / `所有族` / `强于档 1`)⇒ 否则 `夸大表述`
#      · **T5c** 档 2 条目**不得引用**档 0 / 档 1 的 run_id(跨档并列的机器形态)
#        ⇒ 否则 `跨档 run 引用`
#    ⚠️ 本组判据**独立于** run 闸的早退分支(档 2 ⭔ 不产 run ⇒ 不得依赖 run 存在)。
#
# ── 2026-11-14(普查落盘 · 台账 Entry #422):**档 2 可覆盖性地图**(**T7**)
#      补「将来把档 2 误推广到不适用的族」的机器面
#      · **T7a** `coverage_map.families` 的实例集合 == `protocol_instances.json` 的 `instances` 集合
#      · **T7b** 每实例的 `impl` 与 `protocol_instances.json` 声明一致
#      · **T7c** `completeness` / `broadcast_fallback` 与**代码真源** `kTrackerTraits` 逐字段一致
#      · **T7d** `tier2_applicable` 与**推导规则**一致
#        (`(completeness == EXACT) && (!broadcast_fallback) && (impl != 不参与)`)
#      ⇒ 否则 `覆盖地图实例集不符` / `覆盖地图 impl 不符` / `覆盖地图 caps 与代码真源不符` /
#        `覆盖地图推导规则不符`
#    ⚠️ **为什么必须有 T7**:`completeness == EXACT` **不足以**判定适用 ——
#      `nvlink_4`(BroadcastNull)是 EXACT 但 `broadcast_fallback = true` + `scope = NONE`
#      ⇒ sharer 视图恒为**全广播** ⇒ 「他人 sharer」恒非空 ⇒ **恒判违例**(⛔ 不是零违例)。
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

LEDGER="${CEP_RECONCILE_JSONL:-test/cep_e2e_runs.jsonl}"
TIERS="${CEP_RECONCILE_TIERS:-test/cep_e2e_run_tiers.json}"
EVIDENCE_TIERS="${CEP_RECONCILE_EVIDENCE_TIERS:-test/cep_evidence_tiers.json}"
# ── 文档产出物扫描面(per 验收 G-4)—— 影子副本模式用(变异 g)──
DOCS_ROOT="${CEP_RECONCILE_DOCS_ROOT:-$REPO_ROOT}"
# ⚠️ 指纹用**真实仓**路径(⛔ 不用 $DOCS_ROOT)⇒ 影子树模式下也能检出「变异污染真实仓」
DOC07="docs/architecture/07-coherence-exploration-platform.md"
GUIDE="docs/guide/cep-usage.md"
SPEC="openspec/specs/cep-evidence-tier2-observation/spec.md"
# ── T7 的两个**真源**(per 普查 2026-11-14):caps 真源 = 代码 traits 表;实例面 = JSON 声明 ──
CAPS_SRC="${CEP_RECONCILE_CAPS_SRC:-$REPO_ROOT/include/tlm/cep/sharer_tracker.hh}"
INSTANCES="${CEP_RECONCILE_INSTANCES:-$REPO_ROOT/include/tlm/cep/protocol_instances.json}"
MUTATIONS=0
[[ "${1:-}" == "--mutations" ]] && { MUTATIONS=1; shift; }
problems=0

fail() { echo "✗ $*" >&2; problems=$((problems + 1)); }

# ── 档 2 的**非 run 载体**登记面(T4 / T5)───────────────────────────────
# ⚠️ **独立于**下面的 run 闸 ⇒ 档 2 ⭔ 不产 run(per 裁决 R3)时本组判据**仍**执行。
python3 - "$EVIDENCE_TIERS" "$TIERS" "$DOCS_ROOT" "$CAPS_SRC" "$INSTANCES" <<'PYT'
import json, os, re, sys

ev_path = sys.argv[1]
run_tiers_path = sys.argv[2]
docs_root = sys.argv[3]
caps_src_path = sys.argv[4]
instances_path = sys.argv[5]

# 档位词真源单点 = 主判据块内的 TIER_WORDS(⛔ 本块**只引用**不复制定义)
TIER_WORDS_2 = ("档 2", "协议级")

# 禁止短语(肯定式)—— 否定语境豁免(见 `has_negation_context`)
FORBIDDEN = ("检测力已证", "全族达成", "所有族", "强于档 1", "通过态")

# 否定语境标记(命中短语**之前**的窗口内出现任一 ⇒ 豁免)
# ⚠️ 「⛔」(U+26D4)与「⭕」(U+2B50)是**不同字符** ⇒ 两者都要列
#    (踩坑记录:只列「⭕」时,正文用「⛔」的否定句会被误判为肯定式 ⇒ 假红)。
# ⚠️ `NOT` = RFC 2119 的规范否定标记(`SHALL NOT` / `MUST NOT`)⇒ main spec 是
#    SHALL 语法 ⇒ 不认英文否定会**系统性假红**(实测 2 处)。
#    ⚠️ 加它**不放宽**判据:变异 g 植入的肯定式夸大表述其前窗无 `NOT` ⇒ 仍必红。
NEG_MARKERS = ("不", "无", "非", "缺", "禁", "否", "⭕", "⛔", "≠", "NOT")


def has_negation_context(text, idx, window=16):
    head = text[max(0, idx - window):idx]
    return any(m in head for m in NEG_MARKERS)


def iter_strings(node, path=""):
    """逐个产出 (json 路径, 文本值)"""
    if isinstance(node, dict):
        for k, v in node.items():
            yield from iter_strings(v, f"{path}.{k}" if path else str(k))
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from iter_strings(v, f"{path}[{i}]")
    elif isinstance(node, str):
        yield path, node


problems = 0
try:
    with open(ev_path, encoding="utf-8") as fh:
        doc = json.load(fh)
except FileNotFoundError:
    print(f"✗ 缺少档 2 登记面: {ev_path}", file=sys.stderr)
    sys.exit(1)
except json.JSONDecodeError as exc:
    print(f"✗ {ev_path} 不是合法 JSON: {exc}", file=sys.stderr)
    sys.exit(1)

entries = doc.get("evidence_tiers") if isinstance(doc, dict) else None
if not isinstance(entries, dict) or not entries:
    print(f"✗ {ev_path} 的 evidence_tiers 必须是非空 object", file=sys.stderr)
    sys.exit(1)

# 档 0 / 档 1 的 run_id 集合(供 T5c 跨档引用检查)
other_run_ids = set()
try:
    with open(run_tiers_path, encoding="utf-8") as fh:
        _rt = json.load(fh).get("run_tiers") or {}
    other_run_ids = {rid for rid, tier in _rt.items() if tier != 2}
except Exception:
    pass

REQUIRED = ("tier", "family", "carrier_kind", "version_caliber", "coverage_scope",
            "attribution", "evidence_scope", "run_produced", "has_verdict",
            "can_claim", "cannot_claim", "carrier_files")

for cid, e in entries.items():
    # ── T4:字段齐备 ──
    for key in REQUIRED:
        if key not in e or e[key] in (None, "", [], {}):
            print(f"✗ {ev_path} 档 2 登记字段缺失: carrier_id={cid!r} 缺 {key}", file=sys.stderr)
            problems += 1
    if e.get("tier") != 2:
        print(f"✗ {ev_path} 档 2 登记档位非法: carrier_id={cid!r} tier={e.get('tier')!r}(须 2)",
              file=sys.stderr)
        problems += 1
    scope = str(e.get("evidence_scope") or "")
    if scope and not any(w in scope for w in TIER_WORDS_2):
        print(f"✗ {ev_path} 档位声明缺失: carrier_id={cid!r} 的 evidence_scope 不含 {TIER_WORDS_2!r}",
              file=sys.stderr)
        problems += 1
    files = e.get("carrier_files")
    if isinstance(files, list):
        if not files:
            print(f"✗ {ev_path} 档 2 登记字段缺失: carrier_id={cid!r} carrier_files 为空",
                  file=sys.stderr)
            problems += 1
        for f in files:
            if not os.path.isfile(f):
                print(f"✗ {ev_path} 档 2 载体文件不存在: carrier_id={cid!r} {f!r}", file=sys.stderr)
                problems += 1
    if e.get("run_produced") is not False:
        print(f"✗ {ev_path} 档 2 产 run(与裁决 R3 冲突 ⭔ 不产 4 件套 run): carrier_id={cid!r}",
              file=sys.stderr)
        problems += 1
    if e.get("has_verdict") is not False:
        print(f"✗ {ev_path} 档 2 带 verdict(⭔ 无 verdict / 无通过态): carrier_id={cid!r}",
              file=sys.stderr)
        problems += 1

    # ── T5a / T5b:表述层(夸大 / 肯定式通过态)──
    for path, text in iter_strings(e, cid):
        for phrase in FORBIDDEN:
            start = 0
            while True:
                idx = text.find(phrase, start)
                if idx < 0:
                    break
                if not has_negation_context(text, idx):
                    label = "肯定式通过态表述" if phrase == "通过态" else "夸大表述"
                    print(f"✗ {ev_path} {label}: carrier_id={cid!r} 字段 {path} 命中 {phrase!r}",
                          file=sys.stderr)
                    problems += 1
                start = idx + len(phrase)

    # ── T5c:跨档 run 引用(跨档并列的机器形态)──
    for path, text in iter_strings(e, cid):
        for rid in other_run_ids:
            if rid and rid in text:
                print(f"✗ {ev_path} 跨档 run 引用: carrier_id={cid!r} 字段 {path} 引用了 "
                      f"档 0/1 的 run_id {rid!r}", file=sys.stderr)
                problems += 1

# ── T6:防夸大 · **文档产出物**扫描面(per 验收 G-4 / AC-9 原文)────────────
#
# ⚠️ **为什么是「段级」而不是「文件级」**(实测裁定):
#   `FORBIDDEN` 是**档位相关**的词表 —— 「通过态」在**档 0** 合法(档 0 产 run
#   ⇒ 有 verdict)、在**档 2** 非法 ⇒ 用它扫**不分档位**的范围必然假红
#   (实测整文件扫 ⇒ 7 处假红:`07号` 的档 0 SU 标题 ×3 + 后置否定 ×2
#    + main spec 的英文 `SHALL NOT` ×2)。
#   ⇒ AC-9 原文是「**新增产出物内** grep 禁表述」⇒ **段级**才是字面读法
#   (文件级反而**超出**契约)。
#
# ⚠️ **只读**:本块**不改**任何文档 ⇒ 冻结面 hash 不变 ⇒ 无需五步链。
DOC_UNITS = [
    # (相对 docs_root 的路径, 段标题前缀(None=整份), 单元标签)
    ("docs/architecture/07-coherence-exploration-platform.md",
     "## Status Update 2026-11-14", "07号 · 档 2 SU 段"),
    ("docs/guide/cep-usage.md", "## 12.10", "手册 · §12.10 段"),
    ("openspec/specs/cep-evidence-tier2-observation/spec.md",
     None, "main spec(整份 · 整体即档 2 产出物)"),
]


def extract_section(text, heading_prefix):
    """取从 `heading_prefix` 开头的标题到**下一个 `## ` 之前**的段(⛔ 不写死行号)"""
    lines = text.split("\n")
    start = None
    for i, line in enumerate(lines):
        if line.startswith(heading_prefix):
            start = i
            break
    if start is None:
        return None
    end = len(lines)
    for i in range(start + 1, len(lines)):
        if lines[i].startswith("## "):
            end = i
            break
    return "\n".join(lines[start:end])


doc_units = 0
for rel, heading, label in DOC_UNITS:
    full = os.path.join(docs_root, rel)
    try:
        with open(full, encoding="utf-8") as fh:
            text = fh.read()
    except FileNotFoundError:
        print(f"✗ 缺少档 2 文档产出物: {full}", file=sys.stderr)
        problems += 1
        continue
    if heading is not None:
        section = extract_section(text, heading)
        if section is None:
            print(f"✗ 档 2 文档产出物**找不到目标段**: {full} 标题前缀={heading!r}",
                  file=sys.stderr)
            problems += 1
            continue
    else:
        section = text
    doc_units += 1
    for phrase in FORBIDDEN:
        s = 0
        while True:
            idx = section.find(phrase, s)
            if idx < 0:
                break
            if not has_negation_context(section, idx):
                label_txt = "文档肯定式通过态表述" if phrase == "通过态" else "文档夸大表述"
                print(f"✗ {full} {label_txt}: {label} 命中 {phrase!r}", file=sys.stderr)
                problems += 1
            s = idx + len(phrase)

# ── T7:档 2 可覆盖性地图(per 普查落盘 2026-11-14 · 台账 Entry #422)─────────
#
# ⚠️ **三个真源**:① `evidence_tiers.json` 的 `coverage_map` ② `protocol_instances.json`
#    (实例面声明)③ **代码** `kTrackerTraits`(caps 真源 —— JSON 是派生副本)。
# ⚠️ **T7c 必须读代码**:若只比 JSON 与 JSON,两者一起错时闸抓不到。
def parse_traits(path):
    txt = open(path, encoding="utf-8").read()
    i = txt.index("kTrackerTraits{{")
    seg = txt[i:txt.index("\n}};", i)]
    out = {}
    for blk in re.split(r"\n    \{TrackerImpl::", seg):
        if not blk.strip():
            continue
        impl = blk.split(",")[0].strip()
        caps = re.search(
            r"\{Scope::(\w+),\s*Completeness::(\w+),\s*Authority::(\w+),\s*AggregationAt::(\w+)\}",
            blk, re.S)
        beh = re.search(r"\{LookupHazard::(\w+),\s*(true|false)\}", blk)
        if caps and beh:
            out[impl] = {"scope": caps.group(1), "completeness": caps.group(2),
                         "broadcast_fallback": beh.group(2) == "true"}
    return out


cov = doc.get("coverage_map") if isinstance(doc, dict) else None
if not isinstance(cov, dict) or not isinstance(cov.get("families"), dict):
    print(f"✗ {ev_path} 缺 coverage_map.families(档 2 可覆盖性地图)", file=sys.stderr)
    problems += 1
else:
    fam = cov["families"]
    try:
        inst_doc = json.load(open(instances_path, encoding="utf-8"))
        inst_impl = {}
        for v in inst_doc["instances"]:
            inst_impl[v["id"]] = (v.get("sharer_tracker") or {}).get("impl")
    except Exception as exc:
        print(f"✗ 缺实例声明真源 {instances_path}: {exc}", file=sys.stderr)
        problems += 1
        inst_impl = None
    try:
        traits = parse_traits(caps_src_path)
        if len(traits) != 6:
            raise ValueError(f"kTrackerTraits 解析出 {len(traits)} 行(须 6)")
    except Exception as exc:
        print(f"✗ 缺 caps 代码真源 {caps_src_path}: {exc}", file=sys.stderr)
        problems += 1
        traits = None

    if inst_impl is not None:
        # T7a 实例集合相等
        if set(fam) != set(inst_impl):
            print(f"✗ {ev_path} 覆盖地图实例集不符: 多={sorted(set(fam) - set(inst_impl))} "
                  f"缺={sorted(set(inst_impl) - set(fam))}", file=sys.stderr)
            problems += 1
        for iid, rec in fam.items():
            # T7b impl 与声明一致
            if iid in inst_impl and rec.get("impl") != inst_impl[iid]:
                print(f"✗ {ev_path} 覆盖地图 impl 不符: {iid} 表={rec.get('impl')!r} "
                      f"声明={inst_impl[iid]!r}", file=sys.stderr)
                problems += 1
            # T7d 推导规则
            impl = rec.get("impl")
            if impl == "不参与":
                want = False
            elif traits is not None and impl in traits:
                want = (traits[impl]["completeness"] == "EXACT") and \
                       (not traits[impl]["broadcast_fallback"])
            else:
                want = None
            if want is not None and rec.get("tier2_applicable") is not want:
                print(f"✗ {ev_path} 覆盖地图推导规则不符: {iid} "
                      f"tier2_applicable={rec.get('tier2_applicable')!r} 应为 {want!r}", file=sys.stderr)
                problems += 1
            # T7c caps 与代码真源逐字段
            if traits is not None and impl in traits:
                for k in ("completeness", "broadcast_fallback"):
                    if rec.get(k) != traits[impl][k]:
                        print(f"✗ {ev_path} 覆盖地图 caps 与代码真源不符: {iid}.{k} "
                              f"表={rec.get(k)!r} 代码={traits[impl][k]!r}", file=sys.stderr)
                        problems += 1

print(f"[evidence-tiers] 档 2 非 run 载体登记 {len(entries)} 条 · "
      f"文档产出物 {doc_units} 个 · "
      f"覆盖地图 {len(cov['families']) if isinstance(cov, dict) and isinstance(cov.get('families'), dict) else 0} 族 · "
      f"问题 {problems}")
sys.exit(1 if problems else 0)
PYT
_ev_rc=$?
if [[ $_ev_rc -ne 0 ]]; then
  fail "档 2 登记面判据未通过(见上)"
  exit 1
fi

if [[ ! -f "$LEDGER" ]]; then
  echo "✗ 缺少台账清单: $LEDGER" >&2
  exit 2
fi

# ── ① 仓内已提交的 run 产物(只认 **git 跟踪** 的)──
mapfile -t tracked_metrics < <(git ls-files 'experimental/run-*/metrics.json')
mapfile -t tracked_configs < <(git ls-files 'experimental/run-*/simulation_config.json')

if [[ ${#tracked_metrics[@]} -eq 0 ]]; then
  echo "✓ 无已提交的 run 产物 ⇒ 双向闸空真通过(首批入库前状态)"
  exit 0
fi

# ── ② 台账条目解析 + 逐项核对 ──
python3 - "$LEDGER" "$TIERS" <<'PY'
import hashlib, json, os, subprocess, sys

ledger_path = sys.argv[1]
rows = []
with open(ledger_path, encoding='utf-8') as fh:
    for lineno, line in enumerate(fh, 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        try:
            rows.append((lineno, json.loads(line)))
        except json.JSONDecodeError as exc:
            print(f"✗ {ledger_path}:{lineno} 不是合法 JSON: {exc}", file=sys.stderr)
            sys.exit(1)

problems = 0
required = ["run_id", "config_sha256", "metrics_sha256", "metrics_13",
            "verification_mode", "verification_scope", "ic3_verdict",
            "hypothesis_id", "topology_tag", "evidence_scope"]
verdicts = {"✓ 通过", "❌ 违例 fail", "⚪ 不适用"}

# ⚠️ **档位词真源单点**(change `cep-evidence-tier-declaration`);
#    真源定义 = `docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md` 决策 6 的能力阶梯表。
#    ⛔ 本表**不得**在别处复制 —— 文档侧引用同一词,但无机器一致性保证(已登记为残余)。
TIER_WORDS = {0: ("档 0", "拓扑级"), 1: ("档 1", "模型级"), 2: ("档 2", "协议级")}

# ── 档位登记表加载(T3 在循环后判)──────────────────────────────────────────
tiers_path = sys.argv[2] if len(sys.argv) > 2 else "test/cep_e2e_run_tiers.json"
run_tiers = {}
try:
    with open(tiers_path, encoding="utf-8") as fh:
        _tiers_doc = json.load(fh)
    run_tiers = _tiers_doc.get("run_tiers") if isinstance(_tiers_doc, dict) else None
    if not isinstance(run_tiers, dict):
        print(f"✗ {tiers_path} 的 run_tiers 必须是 object", file=sys.stderr)
        problems += 1
        run_tiers = {}
    _decl_required = set(_tiers_doc.get("declaration_required") or [])
    _grandfathered = dict(_tiers_doc.get("grandfathered") or {})
    # T2b:每个**档 0** run 必须落在 declaration_required 或 grandfathered 之一
    #      (⛔ 不得静默跳过;grandfathered 须逐条给非空理由)
    for _rid, _t in run_tiers.items():
        if _t != 0:
            continue
        in_decl = _rid in _decl_required
        in_gran = _rid in _grandfathered
        if in_decl and in_gran:
            print(f"✗ {tiers_path} 档 0 run 同时登记于两表(须恰 1): {_rid!r}", file=sys.stderr)
            problems += 1
        elif not in_decl and not in_gran:
            print(f"✗ {tiers_path} 档 0 run 未登记声明义务(⛔ 不得静默跳过): {_rid!r}",
                  file=sys.stderr)
            problems += 1
        elif in_gran and not str(_grandfathered[_rid]).strip():
            print(f"✗ {tiers_path} grandfathered 须给非空理由: {_rid!r}", file=sys.stderr)
            problems += 1
except FileNotFoundError:
    print(f"✗ 缺少档位登记表: {tiers_path}", file=sys.stderr)
    problems += 1
except json.JSONDecodeError as exc:
    print(f"✗ {tiers_path} 不是合法 JSON: {exc}", file=sys.stderr)
    problems += 1

by_metrics_path = {}
for lineno, row in rows:
    for key in required:
        if key not in row or row[key] in (None, ""):
            print(f"✗ {ledger_path}:{lineno} 必填键缺失:{key}", file=sys.stderr)
            problems += 1
    # ── 档位判据(T1/T2 · per `ADR-CEP-20` 决策 6(ii))──
    _rid = row.get("run_id")
    _tier = run_tiers.get(_rid) if isinstance(run_tiers, dict) else None
    if _rid and _tier is None:
        print(f"✗ {ledger_path}:{lineno} 档位未登记: run_id={_rid!r}", file=sys.stderr)
        problems += 1
    elif _tier == 0 and _rid in _decl_required:
        _scope = str(row.get("evidence_scope") or "")
        _words = TIER_WORDS[0]
        if not any(w in _scope for w in _words):
            print(f"✗ {ledger_path}:{lineno} 档位声明缺失: 档 0 条目的 evidence_scope 不含 "
                  f"{_words!r} ⇒ 跨档并列会以机器可读形式发生", file=sys.stderr)
            problems += 1
    if _tier is not None and _tier not in TIER_WORDS:
        print(f"✗ 非法档位值: run_id={_rid!r} tier={_tier!r}(仅 0/1/2)", file=sys.stderr)
        problems += 1
    if row.get("ic3_verdict") not in verdicts:
        print(f"✗ {ledger_path}:{lineno} ic3_verdict 非三态词表: {row.get('ic3_verdict')!r}",
              file=sys.stderr)
        problems += 1
    m13 = row.get("metrics_13")
    if not isinstance(m13, dict) or not m13:
        print(f"✗ {ledger_path}:{lineno} metrics_13 必须是非空对象(逐键结论)", file=sys.stderr)
        problems += 1
    else:
        for key, val in m13.items():
            if val == "not-collected" and not row.get("metrics_13_reasons", {}).get(key):
                print(f"✗ {ledger_path}:{lineno} metrics_13[{key}]=not-collected 但无 reason",
                      file=sys.stderr)
                problems += 1
    path = row.get("metrics_path", "")
    if not path:
        print(f"✗ {ledger_path}:{lineno} 缺 metrics_path", file=sys.stderr)
        problems += 1
        continue
    if path in by_metrics_path:
        print(f"✗ {ledger_path}:{lineno} metrics_path 重复: {path}", file=sys.stderr)
        problems += 1
    by_metrics_path[path] = (lineno, row)

# T3:反向零 orphan —— 档位表不得登记 jsonl 里不存在的 run(防僵尸登记)
_jsonl_run_ids = {r.get("run_id") for _, r in rows if r.get("run_id")}
for _rid in list(run_tiers):
    if _rid not in _jsonl_run_ids:
        print(f"✗ {tiers_path} 僵尸档位登记: run_id={_rid!r} 不在 {ledger_path} 内",
              file=sys.stderr)
        problems += 1

tracked = subprocess.run(["git", "ls-files", "experimental/run-*/metrics.json"],
                         capture_output=True, text=True).stdout.split()
for path in tracked:
    if path not in by_metrics_path:
        print(f"✗ orphan: 仓内已提交 {path} 但台账无对应条目", file=sys.stderr)
        problems += 1

for path, (lineno, row) in by_metrics_path.items():
    if not os.path.isfile(path):
        print(f"✗ {ledger_path}:{lineno} metrics_path 不存在: {path}", file=sys.stderr)
        problems += 1
        continue
    tracked_hit = subprocess.run(["git", "ls-files", "--error-unmatch", path],
                                 capture_output=True, text=True)
    if tracked_hit.returncode != 0:
        print(f"✗ {ledger_path}:{lineno} metrics_path 未被 git 跟踪: {path}", file=sys.stderr)
        problems += 1
        continue
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
    if digest != row.get("metrics_sha256"):
        print(f"✗ {ledger_path}:{lineno} metrics_sha256 不等: 期望 {row.get('metrics_sha256')} 实得 {digest}",
              file=sys.stderr)
        problems += 1

print(f"[reconcile] 台账条目 {len(rows)} · 仓内已提交 run {len(tracked)} · 问题 {problems}")
sys.exit(1 if problems else 0)
PY
rc=$?
if [[ $rc -ne 0 ]]; then
  fail "双向闸未通过(见上)"
  exit 1
fi

echo "✓ CEP E2E 双向闸通过(台账 ↔ 仓内 run 产物)"

# ── 9 条决定性变异(⛔ 只在影子副本上做;每条须**按签名红**)──────────────
if [[ $MUTATIONS -eq 1 ]]; then
  echo "── 决定性变异(影子副本)──────────────────────────────"
  FP_BEFORE="$(sha256sum "$LEDGER" "$TIERS" "$EVIDENCE_TIERS" "$DOC07" "$GUIDE" "$SPEC")"
  MF=0
  mutate() {  # $1=签名 $2=编号 $3=说明
    local sig="$1" no="$2" desc="$3" out rc
    local SHD; SHD="$(mktemp -d)"
    cp "$LEDGER" "$SHD/ledger.jsonl"; cp "$TIERS" "$SHD/tiers.json"
    cp "$EVIDENCE_TIERS" "$SHD/evidence_tiers.json"
    # ── 文档影子树(per 验收 G-4 / 变异 g)—— T6 的扫描面指向它 ──
    mkdir -p "$SHD/docs/$(dirname "$DOC07")" "$SHD/docs/$(dirname "$GUIDE")" \
             "$SHD/docs/$(dirname "$SPEC")"
    cp "$DOC07" "$SHD/docs/$DOC07"
    cp "$GUIDE" "$SHD/docs/$GUIDE"
    cp "$SPEC" "$SHD/docs/$SPEC"
    python3 - "$SHD" "$no" "$DOC07" <<'PYM'
import json, sys
from pathlib import Path
shd, no = Path(sys.argv[1]), sys.argv[2]
DOC07_REL = sys.argv[3]
led = shd / "ledger.jsonl"; tir = shd / "tiers.json"; evt = shd / "evidence_tiers.json"
rows = [json.loads(l) for l in led.read_text(encoding="utf-8").splitlines() if l.strip()]
tiers = json.loads(tir.read_text(encoding="utf-8"))
TARGET = "cep-full-green-2026-11-13"
if no == "a":      # 删档 0 条目的档位词
    for r in rows:
        if r.get("run_id") == TARGET:
            r["evidence_scope"] = r["evidence_scope"].replace("拓扑级", "").replace("档 0", "")
elif no == "b":    # 删 tiers 表某 run_id
    tiers["run_tiers"].pop(TARGET, None)
    tiers["declaration_required"] = [x for x in tiers["declaration_required"] if x != TARGET]
elif no == "c":    # 僵尸档位登记
    tiers["run_tiers"]["cep-nonexistent-run"] = 1
elif no == "d":    # 删 evidence_scope 键
    for r in rows:
        if r.get("run_id") == TARGET:
            r.pop("evidence_scope", None)
elif no == "e":    # 档 2 条目写成**肯定式**「通过态」
    ev = json.loads(evt.read_text(encoding="utf-8"))
    _e = ev["evidence_tiers"]["mesif-observation"]
    # ⚠️ 整段替换为**纯肯定式**文本 —— 若只替换局部,前文的否定语境
    #    (如「⭕ 无 run / 无 verdict / 无通过态」)会把「通过态」豁免掉 ⇒ 变异不会红。
    _e["evidence_scope"] = "档 2(协议级)观测面 —— 通过态已达成"
    evt.write_text(json.dumps(ev, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
elif no == "f":    # 档 2 条目**引用**档 0 的 run_id(跨档并列)
    ev = json.loads(evt.read_text(encoding="utf-8"))
    ev["evidence_tiers"]["mesif-observation"]["attribution"] += \
        " (对照档 0 的 run cep-full-green-2026-11-13)"
    evt.write_text(json.dumps(ev, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
elif no == "i":    # 覆盖地图的 caps **与代码真源不符**(T7c)· ⚠️ 推导**保持自洽**
    # ⇒ 只触发 T7c,不触发 T7d ⇒ 这是 T7c 的**专属**变异(否则 T7c 只被 T7d 间接覆盖)
    ev = json.loads(evt.read_text(encoding="utf-8"))
    fam = ev["coverage_map"]["families"]
    fam["mesif"]["completeness"] = "CONSERVATIVE"
    fam["mesif"]["tier2_applicable"] = False
    evt.write_text(json.dumps(ev, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
elif no == "h":    # 覆盖地图的 tier2_applicable **反转**(T7d 推导规则)
    ev = json.loads(evt.read_text(encoding="utf-8"))
    fam = ev["coverage_map"]["families"]
    fam["tilelink_c"]["tier2_applicable"] = not fam["tilelink_c"]["tier2_applicable"]
    evt.write_text(json.dumps(ev, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
elif no == "g":    # 档 2 **文档产出物**被植入**肯定式**夸大表述(T6)
    # ⚠️ 必须是**纯肯定式**文本 —— 若沿用原文的否定语境(⛔ / 无 / ≠),
    #    `has_negation_context` 会把它豁免 ⇒ 变异不会红。
    p = shd / "docs" / DOC07_REL
    lines = p.read_text(encoding="utf-8").split("\n")
    start = next(i for i, l in enumerate(lines)
                 if l.startswith("## Status Update 2026-11-14"))
    end = next((i for i in range(start + 1, len(lines))
                if lines[i].startswith("## ")), len(lines))
    lines.insert(end, "> 档 2 的检测力已证,所有族均达成通过态。")
    p.write_text("\n".join(lines), encoding="utf-8")
led.write_text("\n".join(json.dumps(r, ensure_ascii=False) for r in rows) + "\n", encoding="utf-8")
tir.write_text(json.dumps(tiers, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PYM
    out="$(CEP_RECONCILE_JSONL="$SHD/ledger.jsonl" CEP_RECONCILE_TIERS="$SHD/tiers.json" \
           CEP_RECONCILE_EVIDENCE_TIERS="$SHD/evidence_tiers.json" \
           CEP_RECONCILE_DOCS_ROOT="$SHD/docs" \
           CEP_RECONCILE_CAPS_SRC="$CAPS_SRC" CEP_RECONCILE_INSTANCES="$INSTANCES" \
           bash "$REPO_ROOT/scripts/test/cep_e2e_reconcile_check.sh" 2>&1)" && rc=0 || rc=$?
    if [[ $rc -eq 0 ]]; then
      printf '✗ 变异 %s 未变红(基线仍绿)\n' "$no"; MF=$((MF + 1))
    elif ! grep -qF "$sig" <<<"$out"; then
      printf '✗ 变异 %s 未按签名红(期望 %s)\n%s\n' "$no" "$sig" "$out"; MF=$((MF + 1))
    else
      printf '  ✓ 变异 %s 按签名红:%s(%s)\n' "$no" "$sig" "$desc"
    fi
    rm -rf "$SHD"
  }
  mutate "档位声明缺失"               a "删档 0 条目的 evidence_scope 档位词"
  mutate "档位未登记"                 b "删档位表里的 run_id"
  mutate "僵尸档位登记"               c "档位表加 jsonl 不存在的 run_id"
  mutate "必填键缺失:evidence_scope"  d "删 evidence_scope 键"
  mutate "肯定式通过态表述"             e "档 2 条目写成肯定式「通过态已达成」"
  mutate "跨档 run 引用"                f "档 2 条目引用档 0 的 run_id"
  mutate "文档夸大表述"                  g "档 2 文档产出物被植入肯定式夸大表述"
  mutate "覆盖地图推导规则不符"            h "覆盖地图的 tier2_applicable 反转(tilelink_c)"
  mutate "覆盖地图 caps 与代码真源不符"      i "覆盖地图的 mesif.completeness 改 CONSERVATIVE(推导保持自洽)"

  FP_AFTER="$(sha256sum "$LEDGER" "$TIERS" "$EVIDENCE_TIERS" "$DOC07" "$GUIDE" "$SPEC")"
  if [[ "$FP_BEFORE" != "$FP_AFTER" ]]; then
    echo "✗ 真实仓字节指纹前后**不等** ⇒ 变异污染了真实仓"; exit 1
  fi
  echo "  ✓ 真实仓字节指纹前后相等"
  [[ $MF -ne 0 ]] && { echo "✗ $MF 条变异未按签名红"; exit 1; }
  echo "✓ 9 条决定性变异全部按签名红 + 还原绿"
fi
exit 0
