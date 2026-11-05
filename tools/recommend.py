#!/usr/bin/env python3
"""tools/recommend.py — CEP A/B 决策辅助

per ADR-CEP-06 §4.2 / §6 item 5:
  · `deadlock_violations == 0` 与 `memory_violations == 0` 是**硬性**条件;
  · `avg_latency` 退化/改善 **> 10%** 才标「显著」。

输出三种结论:`推荐 A` / `推荐 B` / `不显著`(目标端 A/B 对比语义见下)。

用法:
    python3 tools/recommend.py <baseline_dir> <target_dir> [--policy <json>]
    python3 tools/recommend.py <baseline_dir> <target_dir> --json

退出码:
    0 = 有明确推荐(target 或 baseline)
    1 = 输入错误
    3 = 不显著(差异未超阈值)
    4 = 两者均有硬性违例(均不合格)
"""
import argparse
import json
import sys
from pathlib import Path

DEFAULT_POLICY = {
    "deadlock_violations_max": 0,
    "memory_violations_max": 0,
    "avg_latency_significant_pct": 10.0,
}


def load_metrics(run_dir: Path):
    path = run_dir / "metrics.json"
    if not path.is_file():
        raise FileNotFoundError(f"缺 {path}")
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def load_policy(path):
    policy = dict(DEFAULT_POLICY)
    if path:
        with Path(path).open(encoding="utf-8") as f:
            policy.update(json.load(f))
    return policy


def _gate_state(metrics, policy):
    """单侧硬性闸 **3 态分解**(per ADR-CEP-06 SU#3 勘误:违例 > 不适用 > 通过)。

    返回 (violated: dict{metric: 值}, unmeasured: list[str])。

    ⚠️ **禁止把 `"not-collected"` 当数比**(此前 `hard_ok` 直接 `v <= max`
       ⇒ `TypeError: '<=' not supported between instances of 'str' and 'int'`
       —— 哨兵值缺陷;per SU#1 §⑤:三工具对哨兵必须有防护)。

    ⚠️ 未实测面 = **闸不适用**(per SU#3 勘误三态),**不构成**「该侧通过」。
       非法形态(缺 key / 非数值)同 ab_compare IC3 的「非法或缺失」,**按未实测面**
       处理(不构成肯定断言;数据层缺陷由 ab_compare/metric_type_problems 判红)。
    """
    violated, unmeasured = {}, []
    for k in ("deadlock_violations", "memory_violations"):
        v = metrics.get(k)
        if v == "not-collected":
            unmeasured.append(k)
        elif isinstance(v, bool) or not isinstance(v, (int, float)):
            unmeasured.append(k)  # 非法形态 ⇒ 不可构成依据,对齐 IC3「非法或缺失」
        elif v > policy[k + "_max"]:
            violated[k] = v
    return violated, unmeasured


def decide(baseline, target, policy):
    """返回 (结论文本, 退出码, 明细 dict)。

    语义(per ADR-CEP-06 §4.2 + SU#3 勘误):
      · 实测硬性违例(violated)⇒ 该侧**不合格**;
      · 未实测面(unmeasured)⇒ **闸不适用** —— 不阻塞建议,但**建议不得**为该面
        作安全断言 ⇒ 文本须显式标注「未实测,不由建议背书」;
      · 延迟面只在两侧均实测时可比,否则不显著。
    """
    a_violated, a_unmeasured = _gate_state(baseline, policy)
    b_violated, b_unmeasured = _gate_state(target, policy)

    detail = {
        "baseline_avg_latency": baseline.get("avg_latency_cycles"),
        "target_avg_latency": target.get("avg_latency_cycles"),
        "baseline_hard_violated": a_violated,
        "target_hard_violated": b_violated,
        "baseline_hard_unmeasured": a_unmeasured,
        "target_hard_unmeasured": b_unmeasured,
        "baseline_hard_ok": not a_violated and not a_unmeasured,
        "target_hard_ok": not b_violated and not b_unmeasured,
        # ⚠️ 验证深度必须随结论输出(per ADR-CEP-06 §3):
        #    bounded_random / sampled 的结论**不构成**「已证明」,消费方须看到它。
        "baseline_verification_mode": baseline.get("verification_mode"),
        "baseline_verification_scope": baseline.get("verification_scope"),
        "target_verification_mode": target.get("verification_mode"),
        "target_verification_scope": target.get("verification_scope"),
    }

    def _unmeasured_note(extra_list):
        names = sorted(set(extra_list) | set(a_unmeasured) | set(b_unmeasured))
        if not names:
            return ""
        joined = ", ".join(names)
        return f" ⚠️ 硬性闸未实测({joined})—— 闸不适用,建议不含该面安全断言"

    if a_violated and b_violated:
        return "两者均有硬性违例 ⇒ 均不合格", 4, detail

    violated_names = lambda d: ", ".join(f"{k}={v}" for k, v in sorted(d.items()))
    if a_violated and not b_violated:
        return ("推荐 B(target)—— baseline 存在实测硬性违例(" + violated_names(a_violated) + ")"
                + _unmeasured_note([])), 0, detail
    if b_violated and not a_violated:
        return ("推荐 A(baseline)—— target 存在实测硬性违例(" + violated_names(b_violated) + ")"
                + _unmeasured_note([])), 0, detail

    # 无任何实测违例 ⇒ 闸只可能处于「通过」或「不适用」
    #  ⚠️ 延迟同必须防哨兵:`"not-collected"` / 非数值 ≠ 0 ⇒ 视为未实测(不构成比较依据)
    def _num(v):
        return v if isinstance(v, (int, float)) and not isinstance(v, bool) else None

    lat_unmeasured = []
    a_lat = _num(baseline.get("avg_latency_cycles"))
    b_lat = _num(target.get("avg_latency_cycles"))
    if a_lat is None or b_lat is None:
        lat_unmeasured.append("avg_latency_cycles")
    if float(a_lat or 0) == 0.0 and a_lat is not None:
        detail["delta_pct"] = None
        return ("不显著(baseline 延迟为 0,相对差不可定义)" + _unmeasured_note(lat_unmeasured)), 3, detail

    if a_lat is None or b_lat is None:
        return ("不显著(缺 avg_latency_cycles,无法比较)" + _unmeasured_note(lat_unmeasured)), 3, detail

    delta_pct = (b_lat - a_lat) / a_lat * 100.0
    detail["delta_pct"] = delta_pct
    threshold = policy["avg_latency_significant_pct"]

    if delta_pct > threshold:
        return (f"推荐 A(baseline)—— target 延迟退化 {delta_pct:.1f}% > {threshold}%"
                + _unmeasured_note([])), 0, detail
    if delta_pct < -threshold:
        return (f"推荐 B(target)—— 延迟改善 {abs(delta_pct):.1f}% > {threshold}%"
                + _unmeasured_note([])), 0, detail
    return (f"不显著(延迟差 {delta_pct:+.1f}%,未超 ±{threshold}%)" + _unmeasured_note([])), 3, detail


def main():
    ap = argparse.ArgumentParser(description="CEP A/B 决策辅助(硬性违例 + 10% 显著阈值)")
    ap.add_argument("baseline", help="baseline run 目录(含 metrics.json)")
    ap.add_argument("target", help="target run 目录(含 metrics.json)")
    ap.add_argument("--policy", help="policy_threshold.json 路径")
    ap.add_argument("--json", action="store_true", help="以 JSON 输出明细")
    args = ap.parse_args()

    try:
        baseline = load_metrics(Path(args.baseline))
        target = load_metrics(Path(args.target))
    except (FileNotFoundError, json.JSONDecodeError) as exc:
        print(f"✗ {exc}", file=sys.stderr)
        return 1

    policy = load_policy(args.policy)
    text, code, detail = decide(baseline, target, policy)

    if args.json:
        print(json.dumps({"conclusion": text, "exit_code": code, **detail},
                         ensure_ascii=False, indent=2))
    else:
        print(text)
    return code


if __name__ == "__main__":
    sys.exit(main())
