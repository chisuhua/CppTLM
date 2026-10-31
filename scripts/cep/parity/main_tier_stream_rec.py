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
    # ⚠️ **序列完整性硬断言**:gem5 侧实际访问数必须 == **名义**总条数。
    #    否则说明序列被**截断**(max tick / duration 口径不一致)⇒
    #    **该流的数据不可信** ⇒ 必须报错退出,**不得**把截断的数据带进结论。
    #    (实测踩过:R4 名义 512 条、实测 340 条 ⇒ 4 条流的数据全错,
    #     而当时的报告基于它 ⇒ 属「用错误数据出结论」。)
    seq = d.get("sequence") or {}
    nominal = int(seq.get("num_accesses", 0))
    actual = int(d.get("accesses", 0))
    if nominal and actual != nominal:
        raise SystemExit(
            f"✗ 流 {name} 的**序列被截断**:名义 {nominal} 条、实测 {actual} 条"
            f" ⇒ 数据不可信(检查 max tick / duration 口径)"
            f"⇒ 拒绝把截断数据带入结论")

    rec = {
        "name": name,
        "hit_rate": 1.0 - float(hr),
        "avg_latency_ns": d.get("avg_latency_ns"),
        "hits": d.get("hits"),
        "misses": d.get("misses"),
        "accesses": d.get("accesses"),
        "nominal_accesses": nominal,
        # ⚠️ **workload 特征** ⇒ 供分析器用 R-B 规则估 h_0(不依赖实测命中率)
        "working_set_bytes": seq.get("working_set_bytes"),
        "cache_bytes": seq.get("cache_bytes"),
    }
    out = pathlib.Path(out_dir) / f"stream_{name}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rec, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8")
    print(f"[stream_rec] {name}: h_m={rec['hit_rate']:.4f} "
          f"L_gem5={rec['avg_latency_ns']:.3f}ns")


if __name__ == "__main__":
    main()
