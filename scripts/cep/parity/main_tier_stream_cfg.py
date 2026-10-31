#!/usr/bin/env python3
"""scripts/cep/parity/main_tier_stream_cfg.py
   —— 主对拍档:生成**单条流**的 gem5 侧段配置(写回 `parity_config.json`)

用法:python3 main_tier_stream_cfg.py <config> <base> <hot_lines> <cold_lines> <repeats>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) < 6:
        raise SystemExit(
            "用法:python3 main_tier_stream_cfg.py <config> <base> <hot_lines> "
            "<cold_lines> <repeats>")
    cfg_path = sys.argv[1]
    base = int(sys.argv[2], 0)
    hot_lines = int(sys.argv[3], 0)
    cold_lines = int(sys.argv[4], 0)
    repeats = int(sys.argv[5], 0)

    p = pathlib.Path(cfg_path)
    c = json.loads(p.read_text(encoding="utf-8"))
    line = int(c["gem5"]["linear"]["blocksize"])

    hot_start = base
    hot_end = base + hot_lines * line
    cold_start = base + 0x400000
    cold_end = cold_start + cold_lines * line

    # ⚠️ `data_limit` 是**每遍**的字节上限 ⇒ 每遍条数 = data_limit / blocksize
    c["gem5"]["linear"]["segments"] = [
        {"start": hot_start, "end": hot_end, "repeats": repeats,
         "data_limit": hot_lines * line},
        {"start": cold_start, "end": cold_end, "repeats": 1,
         "data_limit": cold_lines * line},
    ]
    total = hot_lines * repeats + cold_lines
    c["gem5"]["linear"]["duration_ticks"] = int(max(
        total * (int(c["gem5"]["linear"]["min_period"]) + 100000) * 4,
        int(c["gem5"]["linear"]["duration_ticks"])))
    p.write_text(json.dumps(c, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[stream_cfg] 段数=2 总条数={total} hot={hot_lines}行×{repeats}遍 "
          f"cold={cold_lines}行×1遍")


if __name__ == "__main__":
    main()
