#!/usr/bin/env python3
"""scripts/cep/parity/main_tier_stream_cfg.py
   —— 主对拍档:生成**单条流**的 gem5 侧配置(random driver + 该流的工作集)

用法:python3 main_tier_stream_cfg.py <config> <base> <working_set_bytes> <cache_bytes>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) < 5:
        raise SystemExit(
            "用法:python3 main_tier_stream_cfg.py <config> <base> "
            "<working_set_bytes> <cache_bytes>")
    cfg_path = sys.argv[1]
    base = int(sys.argv[2], 0)
    ws = int(sys.argv[3], 0)
    cache_bytes = int(sys.argv[4], 0)

    p = pathlib.Path(cfg_path)
    c = json.loads(p.read_text(encoding="utf-8"))
    lin = c["gem5"]["linear"]

    # ⚠️ **random driver**:均匀随机访问 ⇒ 命中率 ≈ 1 − C/W ⇒ 扫 W 得连续分散的 h_m。
    lin["driver"] = "random"
    lin["random_start_addr"] = base
    lin["random_end_addr"] = base + ws
    # 记录该流的**工作集/容量** ⇒ 分析器据此用 R-B 规则估 h_0
    lin["working_set_bytes"] = ws
    lin["cache_bytes"] = cache_bytes
    # 名义条数 ≥ 1e5(per `14 号` §4.1);random driver 的条数由 duration 决定
    lin["nominal_accesses"] = int(lin.get("nominal_accesses", 100000))
    # duration 由探针按「名义条数 × tick 预算」算 ⇒ 这里只需给预算
    lin.setdefault("tick_budget_per_access", 200000)

    p.write_text(json.dumps(c, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[stream_cfg] driver=random W={ws}B cache={cache_bytes}B "
          f"名义条数={lin['nominal_accesses']}")


if __name__ == "__main__":
    main()
