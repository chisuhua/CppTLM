#!/usr/bin/env python3
"""scripts/cep/parity/main_tier_stream_rec.py
   —— 主对拍档:提取**单条流**的 gem5 侧结果 → 独立 JSON

用法:python3 main_tier_stream_rec.py <probe_json> <name> <out_dir>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) < 4:
        raise SystemExit("用法:python3 main_tier_stream_rec.py <probe_json> <name> <out_dir>")
    src, name, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    d = json.loads(pathlib.Path(src).read_text(encoding="utf-8"))
    hr = d.get("miss_rate")
    if hr is None:
        raise SystemExit(f"流 {name} 缺 miss_rate ⇒ stats 未读到")
    rec = {
        "name": name,
        "hit_rate": 1.0 - float(hr),
        "avg_latency_ns": d.get("avg_latency_ns"),
        "hits": d.get("hits"),
        "misses": d.get("misses"),
        "accesses": d.get("accesses"),
    }
    out = pathlib.Path(out_dir) / f"stream_{name}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rec, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8")
    print(f"[stream_rec] {name}: h_m={rec['hit_rate']:.4f} "
          f"L_gem5={rec['avg_latency_ns']:.3f}ns")


if __name__ == "__main__":
    main()
