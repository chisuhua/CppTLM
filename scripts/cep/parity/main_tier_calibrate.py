#!/usr/bin/env python3
"""scripts/cep/parity/main_tier_calibrate.py
   —— **duration 校准**:使 gem5 实际访问条数 == 名义条数

⚠️ **为什么需要**:gem5 v24 的 `data_limit` 在 **Release 构建下不生效**
   ⇒ 实际终止条件是 `duration` ⇒ 条数 = `duration / 每条实际耗时`,
   而每条实际耗时**受背压影响**(实测 10ns ~ 165ns)⇒ **不能靠猜常数**
   ⇒ 按实测比例迭代校准。

用法:python3 main_tier_calibrate.py <config> <actual_accesses> <target_accesses>
   · 实际条数 == 目标 ⇒ 不动 config(退出码 0)
   · 否则按比例调 `duration_ticks` ⇒ 退出码 1(表示「需要重跑」)
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) < 4:
        raise SystemExit(
            "用法:python3 main_tier_calibrate.py <config> <actual> <target>")
    cfg_path = sys.argv[1]
    actual = int(sys.argv[2])
    target = int(sys.argv[3])

    p = pathlib.Path(cfg_path)
    c = json.loads(p.read_text(encoding="utf-8"))
    lin = c["gem5"]["linear"]
    dur = int(lin.get("duration_ticks") or 0)
    if dur <= 0:
        dur = lin.get("nominal_accesses", 100000) * lin.get(
            "tick_budget_per_access", 200000)

    # 允许 ±0.05% 的相对误差(条数是整数,不可能精确相等)
    if actual and abs(actual - target) <= max(1, target * 0.0005):
        print(f"  [calib] 条数已达标:实际 {actual} ≈ 目标 {target}")
        return 0

    new_dur = int(dur * (target / float(actual))) if actual else dur
    lin["duration_ticks"] = new_dur
    p.write_text(json.dumps(c, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"  [calib] 实际 {actual} ≠ 目标 {target} ⇒ duration {dur} → {new_dur}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
