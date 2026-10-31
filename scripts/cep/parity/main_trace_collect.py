#!/usr/bin/env python3
"""scripts/cep/parity/main_trace_collect.py
   —— 主对拍档(文件同源):汇总各条流的**两侧**结果

每条流产出两个 JSON:
  · `mt_<name>.json`      —— gem5 侧(回放 `.pb`)
  · `mt_<name>_cpptlm.json` —— CppTLM 侧(读同一 `.csv`)
⇒ 汇总时**交叉核对地址序列指纹**(两侧都按同一 CSV 算)⇒ 不一致 ⇒ 报错退出。

用法:python3 main_trace_collect.py <stream_dir> <out.json> <out_dir>
"""
import json
import pathlib
import sys

# gem5 侧 cache 容量(per `parity_config.json` 的 `gem5.cache.size`)
CACHE_BYTES_DEFAULT = 16384


def main():
    if len(sys.argv) < 4:
        raise SystemExit(
            "用法:python3 main_trace_collect.py <stream_dir> <out.json> <out_dir>")
    stream_dir, out, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    od = pathlib.Path(out_dir)

    recs = []
    for p in sorted(pathlib.Path(stream_dir).glob("stream_*.json")):
        name = p.stem[len("stream_"):]
        g = json.loads((od / f"mt_{name}.json").read_text(encoding="utf-8"))
        c = json.loads((od / f"mt_{name}_cpptlm.json").read_text(encoding="utf-8"))
        # ── **交叉核对指纹**(文件同源的强证据)─────────────────────
        fp_g = g.get("addr_seq_fnv1a64")
        fp_c = (c.get("sequence") or {}).get("addr_seq_fnv1a64")
        if not fp_g or not fp_c:
            raise SystemExit(f"✗ 流 {name} 缺地址序列指纹(两侧都没算?)")
        if fp_g != fp_c:
            raise SystemExit(
                f"✗ 流 {name} 的**地址序列指纹不一致** ⇒ 两侧**没消费同一条 trace**"
                f" ⇒ 对拍无效\n    gem5   ={fp_g}\n    cpptlm={fp_c}")
        if int(g.get("accesses") or 0) != int(c.get("accesses") or 0):
            raise SystemExit(
                f"✗ 流 {name} 的访问数不一致:gem5={g.get('accesses')} "
                f"cpptlm={c.get('accesses')} ⇒ 对拍无效")

        meta = json.loads(
            (od / f"trace_{name}.meta.json").read_text(encoding="utf-8"))
        rec = {
            "name": name,
            "hit_rate": 1.0 - float(g["miss_rate"]),
            "avg_latency_ns": g.get("avg_latency_ns"),
            "hits": g.get("hits"),
            "misses": g.get("misses"),
            "accesses": g.get("accesses"),
            "nominal_accesses": meta.get("num_accesses"),
            # ⚠️ **workload 特征** ⇒ 供分析器用 R-B 规则估 h_0
            "working_set_bytes": meta.get("working_set_bytes"),
            "cache_bytes": CACHE_BYTES_DEFAULT,
            "profile": meta.get("profile"),
            "mode": g.get("mode"),
            # ⚠️ breaking 流可声明**期望误差下界**(per `14 号` §4.4)
            "expected_error_floor_pp": meta.get("expected_error_floor_pp"),
            "hot_ratio": meta.get("hot_ratio"),
            "addr_seq_fnv1a64": fp_g,
        }
        recs.append(rec)

    pathlib.Path(out).write_text(
        json.dumps(recs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[collect] 汇总 {len(recs)} 条流(文件同源,指纹逐条核对通过) → {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
