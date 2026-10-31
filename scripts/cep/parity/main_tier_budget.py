#!/usr/bin/env python3
"""scripts/cep/parity/main_tier_budget.py
   —— 主对拍档:把 `tick_budget_per_access` 加倍(序列被截断时重跑用)

⚠️ 为什么需要:背压下实际每条访问耗时可达 **~165ns**(实测)⇒ 预算不足 ⇒
   **序列被截断**(名义 512 条、实测 340 条)⇒ **数据不可信**。
   ⇒ 不用「猜一个足够大的常数」—— 猜大又会撞 gem5 v24 的 generator 耗尽 null 崩。

用法:python3 main_tier_budget.py <config>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) < 2:
        raise SystemExit("用法:python3 main_tier_budget.py <config>")
    p = pathlib.Path(sys.argv[1])
    c = json.loads(p.read_text(encoding="utf-8"))
    b = int(c["gem5"]["linear"].get("tick_budget_per_access", 200000))
    c["gem5"]["linear"]["tick_budget_per_access"] = b * 2
    p.write_text(json.dumps(c, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"  [budget] {b} → {b * 2}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
