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
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

LEDGER="${CEP_RECONCILE_JSONL:-test/cep_e2e_runs.jsonl}"
TIERS="${CEP_RECONCILE_TIERS:-test/cep_e2e_run_tiers.json}"
EVIDENCE_TIERS="${CEP_RECONCILE_EVIDENCE_TIERS:-test/cep_evidence_tiers.json}"
MUTATIONS=0
[[ "${1:-}" == "--mutations" ]] && { MUTATIONS=1; shift; }
problems=0

fail() { echo "✗ $*" >&2; problems=$((problems + 1)); }

# ── 档 2 的**非 run 载体**登记面(T4 / T5)───────────────────────────────
# ⚠️ **独立于**下面的 run 闸 ⇒ 档 2 ⭔ 不产 run(per 裁决 R3)时本组判据**仍**执行。
python3 - "$EVIDENCE_TIERS" "$TIERS" <<'PYT'
import json, os, sys

ev_path = sys.argv[1]
run_tiers_path = sys.argv[2]

# 档位词真源单点 = 主判据块内的 TIER_WORDS(⛔ 本块**只引用**不复制定义)
TIER_WORDS_2 = ("档 2", "协议级")

# 禁止短语(肯定式)—— 否定语境豁免(见 `has_negation_context`)
FORBIDDEN = ("检测力已证", "全族达成", "所有族", "强于档 1", "通过态")

# 否定语境标记(命中短语**之前**的窗口内出现任一 ⇒ 豁免)
# ⚠️ 「⛔」(U+26D4)与「⭕」(U+2B50)是**不同字符** ⇒ 两者都要列
#    (踩坑记录:只列「⭕」时,正文用「⛔」的否定句会被误判为肯定式 ⇒ 假红)。
NEG_MARKERS = ("不", "无", "非", "缺", "禁", "否", "⭕", "⛔", "≠")


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

print(f"[evidence-tiers] 档 2 非 run 载体登记 {len(entries)} 条 · 问题 {problems}")
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

# ── 4 条决定性变异(⛔ 只在影子副本上做;每条须**按签名红**)──────────────
if [[ $MUTATIONS -eq 1 ]]; then
  echo "── 决定性变异(影子副本)──────────────────────────────"
  FP_BEFORE="$(sha256sum "$LEDGER" "$TIERS" "$EVIDENCE_TIERS")"
  MF=0
  mutate() {  # $1=签名 $2=编号 $3=说明
    local sig="$1" no="$2" desc="$3" out rc
    local SHD; SHD="$(mktemp -d)"
    cp "$LEDGER" "$SHD/ledger.jsonl"; cp "$TIERS" "$SHD/tiers.json"
    cp "$EVIDENCE_TIERS" "$SHD/evidence_tiers.json"
    python3 - "$SHD" "$no" <<'PYM'
import json, sys
from pathlib import Path
shd, no = Path(sys.argv[1]), sys.argv[2]
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
led.write_text("\n".join(json.dumps(r, ensure_ascii=False) for r in rows) + "\n", encoding="utf-8")
tir.write_text(json.dumps(tiers, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PYM
    out="$(CEP_RECONCILE_JSONL="$SHD/ledger.jsonl" CEP_RECONCILE_TIERS="$SHD/tiers.json" \
           CEP_RECONCILE_EVIDENCE_TIERS="$SHD/evidence_tiers.json" \
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

  FP_AFTER="$(sha256sum "$LEDGER" "$TIERS" "$EVIDENCE_TIERS")"
  if [[ "$FP_BEFORE" != "$FP_AFTER" ]]; then
    echo "✗ 真实仓字节指纹前后**不等** ⇒ 变异污染了真实仓"; exit 1
  fi
  echo "  ✓ 真实仓字节指纹前后相等"
  [[ $MF -ne 0 ]] && { echo "✗ $MF 条变异未按签名红"; exit 1; }
  echo "✓ 6 条决定性变异全部按签名红 + 还原绿"
fi
exit 0
