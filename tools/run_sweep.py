#!/usr/bin/env python3
"""tools/run_sweep.py — CEP sweep 驱动

per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/design.md D4:纯标准库,零项目内依赖。
输入是一个 sweep 规格(JSON),对每个参数组合调用一个**外部** runner 命令,
把每次运行的 4 件套收进输出目录,并产出 sweep_summary.csv。

runner 契约(产物输出目录经**两个通道**告知):
  - 环境变量 `CEP_EXP_DIR`(本实验的输出目录)+ `CEP_EXP_ID`(组合 id)
  - 命令行追加 `--exp-dir=<输出目录>`

用法:
    python3 tools/run_sweep.py <sweep_spec.json> -o <out_dir> [--render-report]
    python3 tools/run_sweep.py <sweep_spec.json> --dry-run
"""
import argparse
import csv
import itertools
import json
import os
import shutil
import subprocess
import sys

REQUIRED_METRICS = [
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
]

# 4 件套契约(per ADR-CEP-06 §2),**按产出方向**区分:
#   runner 产出输入 3 件套 -> report.html 由后置的 render_report 产出。
# ⚠️ 早期版本要求 runner 产出全部 4 件套,链路不自洽(runner 产不出 report.html)。
INPUT_ARTIFACTS = ("simulation_config.json", "trace.h5", "metrics.json")
OUTPUT_ARTIFACT = "report.html"


def load_spec(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def expand(dimensions):
    """dimensions: {name: [v1, v2, ...]} → 笛卡尔积的 kwargs 列表。"""
    if not dimensions:
        return [{}]
    keys = list(dimensions.keys())
    values = [dimensions[k] for k in keys]
    out = []
    for combo in itertools.product(*values):
        out.append(dict(zip(keys, combo)))
    return out


def combo_id(params):
    parts = []
    for k in sorted(params):
        v = str(params[k]).replace("/", "_").replace(" ", "_")
        parts.append(f"{k}={v}")
    return "__".join(parts) if parts else "base"


def consistency_violations(metrics):
    """一致性闸门(per ADR-CEP-06 §3 IC3):deadlock_violations == 0 且
    memory_violations == 0,否则 = **一致性违例,实验 fail**。

    ⚠️ 此前本闸门只存在于文档里 —— 三个工具**只查 metric 键是否存在**、
       从不查**值**。实测 deadlock_violations=42 / memory_violations=17 的
       实验目录:render_report --check 退出码 0、run_sweep 计入成功、
       ab_compare 输出「⚠️ B 更差」的正常 A/B 结论(2026-10-21 代码复审 B2)。

    返回**违例描述列表**;空列表 = 合规。
    """
    problems = []
    dv = metrics.get("deadlock_violations")
    mv = metrics.get("memory_violations")
    if dv == "not-collected":
        # 违例类未采集 ⇒ IC3「不适用」(per ADR-CEP-06 SU#1 §④ ⑤)—— 不是违例
        return problems
    if not isinstance(dv, int) or isinstance(dv, bool) or dv < 0:
        problems.append(f"deadlock_violations 非法或缺失: {dv!r}")
    elif dv > 0:
        problems.append(f"死锁违例 {dv} 次(deadlock_violations > 0)")

    if not isinstance(mv, int) or isinstance(mv, bool) or mv < 0:
        problems.append(f"memory_violations 非法或缺失: {mv!r}")
    elif mv > 0:
        problems.append(f"一致性违例 {mv} 次(memory_violations > 0)")

    # 自洽性:memory_violations 必须 == kinds.swmr + kinds.data_value
    kinds = metrics.get("memory_violation_kinds")
    if isinstance(kinds, dict):
        try:
            subtotal = int(kinds.get("swmr", 0)) + int(kinds.get("data_value", 0))
        except (TypeError, ValueError):
            subtotal = None
        if subtotal is not None and isinstance(mv, int) and not isinstance(mv, bool):
            if subtotal != mv:
                problems.append(
                    f"memory_violations({mv}) 与 memory_violation_kinds 合计({subtotal}) 不自洽")
    return problems


def metric_type_problems(metrics):
    """13 必纳 metric 的**类型 + 非负**校验(per experimental/metrics.schema.json)。

    ⚠️ 此前 schema 从未用于校验任何真实文件 —— "total_cycles": "abc" 也能过所有闸门。

    ⚠️ 2026-11-05(Entry #218 §⑤-2;per ADR-CEP-06 SU#1 §② 载体 A):13 必纳 metric 的取值
    类型放宽为「数值 **或** `"not-collected"`」——「实测 0」与「未测量」必须在**数据层**可区分
    (此前两者同形 ⇒ 结构零冒充实测)。配套:**标了 `"not-collected"` 的 metric 必须在
    `_not_collected` 表中有该 metric 的 per-metric 原因** ⇒ reason 缺失即判红
    (per SU#1 §②「reason 缺失 ⇒ 判红」)。
    """
    problems = []
    reasons = metrics.get("_not_collected")
    if reasons is not None and not isinstance(reasons, dict):
        problems.append(f"_not_collected 必须是 object(per-metric 原因表): {reasons!r}")
        reasons = {}
    for k in REQUIRED_METRICS:
        v = metrics.get(k)
        if v == "not-collected":
            # 必须在 _not_collected 表中有该 metric 的原因
            if not reasons or k not in reasons:
                problems.append(f"metric {k} 标 not-collected 但 _not_collected 缺其原因")
            continue
        if not isinstance(v, (int, float)) or isinstance(v, bool):
            problems.append(f"metric {k} 非数值: {v!r}")
        elif v < 0:
            problems.append(f"metric {k} 为负: {v!r}")
    # 反向:_not_collected 表里的 key 必须是真实的 13 必纳 metric
    for k in (reasons or {}):
        if k not in REQUIRED_METRICS:
            problems.append(f"_not_collected 含非 13 必纳 metric 的 key: {k!r}")
    return problems


# ── 预登记闸(per ADR-CEP-06 SU#1 §④ 判据④ / SU#2 ① 载体裁决)────────
#   run 的 `not-collected` 集合必须 ⊆ 该 run 所服务假设的**预登记** out-of-scope 集。
#   ⇒ 预登记真源 = `objective §9.1` ② 表;机器投影 = `experimental/preregistration.json`
#   (入 M1 冻结集;drift 校验守「② 表 ↔ JSON」两份一致)。
#   ⚠️ **不由 run 自报 out_of_scope** —— run 自报 = 被试者自证面(SU#2 ① 已判死);
#      本闸只读**外部锁定的投影**。
PREREG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                           "experimental", "preregistration.json")


def _load_prereg():
    if not os.path.isfile(PREREG_PATH):
        return None
    try:
        with open(PREREG_PATH, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return None


def preregistration_problems(metrics, hypothesis_id=None):
    """判据④:`not-collected` ⊆ 预登记 out-of-scope;违例类 not-collected ⇒ IC3 不适用。

    返回 (problems, ic3_not_applicable)。
    """
    problems = []
    pre = _load_prereg()
    if pre is None:
        return ([], False)
    hyps = pre.get("hypotheses") or {}

    # ① run ↔ hypothesis 绑定:simulation_config.json 的 hypothesis_id(纯标签)
    if hypothesis_id is None:
        hypothesis_id = metrics.get("hypothesis_id")
    if hypothesis_id is None:
        problems.append("缺 hypothesis_id(无法绑定预登记 —— run 自报 out_of_scope 被禁止)")
        return (problems, False)
    hid = str(hypothesis_id).upper()
    entry = hyps.get(hid)
    if entry is None:
        problems.append(f"hypothesis_id {hypothesis_id!r} 在 preregistration.json **无登记行**")
        return (problems, False)
    if entry.get("status") == "pending":
        problems.append(f"hypothesis_id {hypothesis_id!r} 的预登记状态 = pending ⇒ 禁产出 run")
        return (problems, False)

    allowed = set(entry.get("out_of_scope") or [])
    actual = {k for k in REQUIRED_METRICS if metrics.get(k) == "not-collected"}
    over = sorted(actual - allowed)
    if over:
        problems.append(
            f"not-collected 集合越界(超出预登记 out-of-scope):{over} —— "
            f"{hid} 允许 {sorted(allowed) or '(空)'}")
    allowed = set(entry.get("out_of_scope") or [])
    actual = {k for k in REQUIRED_METRICS if metrics.get(k) == "not-collected"}
    over = sorted(actual - allowed)
    if over:
        problems.append(
            f"not-collected 集合越界(超出预登记 out-of-scope):{over} —— "
            f"{hid} 允许 {sorted(allowed) or '(空)'}")

    # ③ **三分支判定**(per SU#1 §④ 判据③ 的可执行化):
    #    「实测 0」与「未测量」在**数据层同形**(都是 0)⇒ 机器闸无法从数值区分
    #    ⇒ 由**产出侧**显式声明「我测了哪些」(`_measured` 集,实测侧事实)。
    #    · 报 measured      ⇒ 必须有数值(**可以是 0** —— 那是实测零)
    #    · 报 not-collected ⇒ 必须 ⊆ 预登记 out-of-scope(上面的越界检查覆盖)
    #    · 两者都没报       ⇒ **判红**(漏声明)
    #    ⚠️ run 自报「我测了哪些」**不违反** SU#2 ① 的「不自报 out-of-scope」——
    #    前者是**实测侧事实**(与 metrics 一致),后者是「预登记授权」(仍由外部锁定表授予)。
    measured = set(metrics.get("_measured") or [])
    nc_map = metrics.get("_not_collected") or {}
    for k in REQUIRED_METRICS:
        v = metrics.get(k)
        if k in measured:
            if v == "not-collected":
                problems.append(f"metric {k} 声明为实测却写成 not-collected(自相矛盾)")
        elif k in nc_map:
            if v != "not-collected":
                problems.append(
                    f"metric {k} 声明为未采集却写成 {v!r}(须写 \"not-collected\")")
        else:
            problems.append(
                f"metric {k} **未声明**实测/未采集 —— 产出侧必须显式声明"
                f"(per SU#1 §④ 判据③:实测 0 与未测量同形,机器闸无法从数值区分)")
    # ② 违例类 not-collected ⇒ IC3 判定「不适用」(per SU#1 §④ ⑤)
    ic3_na = bool({"deadlock_violations", "memory_violations"} & actual)
    return (problems, ic3_na)


def validate_exp(exp_dir, require_complete=False):
    """校验实验目录。默认只校验**输入 3 件套**;require_complete=True 时
    追加校验 report.html(完整 4 件套,由后置 render_report 产出)。"""
    problems = []
    names = list(INPUT_ARTIFACTS) + ([OUTPUT_ARTIFACT] if require_complete else [])
    for name in names:
        if not os.path.exists(os.path.join(exp_dir, name)):
            problems.append(f"缺件套成员: {name}")
    mpath = os.path.join(exp_dir, "metrics.json")
    if os.path.exists(mpath):
        with open(mpath, encoding="utf-8") as f:
            m = json.load(f)
        for k in REQUIRED_METRICS:
            if k not in m:
                problems.append(f"13 必纳 metric 缺: {k}")
        problems.extend(metric_type_problems(m))
        # ⚠️ 预登记闸 + IC3「不适用」分支(per ADR-CEP-06 SU#1 §④ ⑤)
        cfg_path = os.path.join(exp_dir, "simulation_config.json")
        cfg = {}
        if os.path.exists(cfg_path):
            try:
                with open(cfg_path, encoding="utf-8") as f:
                    cfg = json.load(f)
            except (json.JSONDecodeError, OSError):
                cfg = {}
        _pre, _na = preregistration_problems(m, cfg.get("hypothesis_id"))
        problems.extend(_pre)
        # 一致性闸门(ADR-CEP-06 IC3)+ verification 声明(IC2)
        if not _na:
            problems.extend(consistency_violations(m))
    else:
        problems.append("缺 metrics.json")
    # verification 声明:每个实验**必须**声明(IC2)—— 不带 --render-report 的
    # sweep 此前完全不校验,能产出无声明的实验。
    cfg_path = os.path.join(exp_dir, "simulation_config.json")
    if os.path.exists(cfg_path):
        with open(cfg_path, encoding="utf-8") as f:
            cfg = json.load(f)
        for k in ("verification_mode", "verification_scope"):
            if k not in cfg:
                problems.append(f"simulation_config.json 缺 {k}")
    else:
        problems.append("缺 simulation_config.json")
    return problems


def main():
    ap = argparse.ArgumentParser(description="CEP sweep 驱动")
    ap.add_argument("spec", help="sweep 规格 JSON")
    ap.add_argument("-o", "--out", required=True, help="输出目录")
    ap.add_argument("--dry-run", action="store_true", help="只列出组合,不执行")
    ap.add_argument("--require-complete", action="store_true",
                    help="额外要求每个实验目录含完整 4 件套(含 report.html)")
    ap.add_argument("--render-report", action="store_true",
                    help="每次运行后调用 tools/render_report.py 产出 report.html(补齐第 4 件套)")
    args = ap.parse_args()

    spec = load_spec(args.spec)
    runner = spec.get("runner")
    dimensions = spec.get("dimensions", {})
    base = spec.get("base_args", [])
    verification = spec.get("verification", {})

    combos = expand(dimensions)
    print(f"[run_sweep] {len(combos)} 个组合")
    if not runner:
        print("[run_sweep] 规格缺 runner 命令,仅列出组合")
        for c in combos:
            print("  " + combo_id(c))
        return 0

    if args.dry_run:
        for c in combos:
            print("  " + combo_id(c))
        return 0

    os.makedirs(args.out, exist_ok=True)
    rows = []
    failures = 0
    # runner 契约:产物输出目录经 **两个通道**告知 —— 环境变量(给任意 runner)
    # 与 `--exp-dir=` 参数(给显式约定的 runner);两者同值,便于脚本化接入。
    render_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "render_report.py")
    for c in combos:
        cid = combo_id(c)
        exp_dir = os.path.join(os.path.abspath(args.out), cid)
        os.makedirs(exp_dir, exist_ok=True)
        cmd = list(runner) + [f"--{k}={v}" for k, v in sorted(c.items())] +               [f"--exp-dir={exp_dir}"] + list(base)
        env = dict(os.environ)
        env["CEP_EXP_DIR"] = exp_dir
        env["CEP_EXP_ID"] = cid
        print(f"[run_sweep] → {cid}")
        try:
            rc = subprocess.call(cmd, env=env)
        except OSError as e:
            print(f"  ✗ runner 无法执行: {e}")
            failures += 1
            continue
        if rc != 0:
            print(f"  ✗ runner 退出码 {rc}")
            failures += 1
            continue
        # 补齐第 4 件套(render_report 的产物)
        if args.render_report:
            if not os.path.exists(render_script):
                print(f"  ! 找不到 render_report.py: {render_script}")
                failures += 1
                continue
            # 用 sys.executable(而非硬编码 "python3"):环境只有 python 时会
            # FileNotFoundError 崩出(此前未捕获);且**必须**检查返回码 ——
            # 渲染失败此前被静默吞掉,sweep 仍报成功。
            try:
                rrc = subprocess.call([sys.executable, render_script, exp_dir],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except OSError as e:
                print(f"  ✗ render_report 无法执行: {e}")
                failures += 1
                continue
            if rrc != 0:
                print(f"  ✗ render_report 退出码 {rrc}(含一致性违例 → 该组合 fail)")
                failures += 1
                continue
        problems = validate_exp(exp_dir, require_complete=args.require_complete)
        if problems:
            failures += 1
            for p in problems:
                print(f"  ✗ {p}")
            continue
        with open(os.path.join(exp_dir, "metrics.json"), encoding="utf-8") as f:
            m = json.load(f)
        row = dict(c)
        for k in REQUIRED_METRICS:
            row[k] = m.get(k, "")
        row["verification_mode"] = verification.get("mode", "")
        row["verification_scope"] = verification.get("scope", "")
        rows.append(row)

    summary = os.path.join(args.out, "sweep_summary.csv")
    if rows:
        keys = list(rows[0].keys())
        with open(summary, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=keys)
            w.writeheader()
            w.writerows(rows)
        print(f"[run_sweep] 已写 {summary}({len(rows)} 行)")
    print(f"[run_sweep] 完成:{len(rows)} 成功 / {failures} 失败")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
