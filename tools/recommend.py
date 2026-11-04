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


def hard_ok(metrics, policy):
    return (metrics.get("deadlock_violations", 0) <= policy["deadlock_violations_max"]
            and metrics.get("memory_violations", 0) <= policy["memory_violations_max"])


def decide(baseline, target, policy):
    """返回 (结论文本, 退出码, 明细 dict)。"""
    a_ok = hard_ok(baseline, policy)
    b_ok = hard_ok(target, policy)

    detail = {
        "baseline_avg_latency": baseline.get("avg_latency_cycles"),
        "target_avg_latency": target.get("avg_latency_cycles"),
        "baseline_hard_ok": a_ok,
        "target_hard_ok": b_ok,
        # ⚠️ 验证深度必须随结论输出(per ADR-CEP-06 §3):
        #    bounded_random / sampled 的结论**不构成**「已证明」,消费方须看到它。
        "baseline_verification_mode": baseline.get("verification_mode"),
        "baseline_verification_scope": baseline.get("verification_scope"),
        "target_verification_mode": target.get("verification_mode"),
        "target_verification_scope": target.get("verification_scope"),
    }

    if not a_ok and not b_ok:
        return "两者均有硬性违例 ⇒ 均不合格", 4, detail
    if a_ok and not b_ok:
        return "推荐 A(baseline)—— target 存在硬性违例", 0, detail
    if b_ok and not a_ok:
        return "推荐 B(target)—— baseline 存在硬性违例", 0, detail

    a_lat = baseline.get("avg_latency_cycles")
    b_lat = target.get("avg_latency_cycles")
    if a_lat is None or b_lat is None:
        return "不显著(缺 avg_latency_cycles,无法比较)", 3, detail

    if a_lat == 0:
        detail["delta_pct"] = None
        return "不显著(baseline 延迟为 0,相对差不可定义)", 3, detail

    delta_pct = (b_lat - a_lat) / a_lat * 100.0
    detail["delta_pct"] = delta_pct
    threshold = policy["avg_latency_significant_pct"]

    if delta_pct > threshold:
        return f"推荐 A(baseline)—— target 延迟退化 {delta_pct:.1f}% > {threshold}%", 0, detail
    if delta_pct < -threshold:
        return f"推荐 B(target)—— 延迟改善 {abs(delta_pct):.1f}% > {threshold}%", 0, detail
    return f"不显著(延迟差 {delta_pct:+.1f}%,未超 ±{threshold}%)", 3, detail


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
