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
    # ── **序列完整性断言:按 driver 分档**(per `14 号` §4.4「条数契约(P-1)按 driver 分档」)
    # ⚠️ **trace 回放路径**(`mode == "same_source_trace"`):1 条 = 1 个包
    #    ⇒ 名义条数**就是**条数契约 ⇒ 实测必须**精确相等**。
    #    否则说明序列被**截断**(max tick / duration 口径不一致)⇒
    #    **该流的数据不可信** ⇒ 必须报错退出,**不得**把截断的数据带进结论。
    #    (实测踩过:R4 名义 512 条、实测 340 条 ⇒ 4 条流的数据全错,
    #     而当时的报告基于它 ⇒ 属「用错误数据出结论」。)
    # ⚠️ **random driver 路径**(`mode == "parameter_same_source"`):条数由 **duration**
    #    决定 ⇒ 实测可达名义的 **17~20 倍**(实测 1.75e6~2.0e6 vs 名义 1e5)
    #    ⇒ **这是该路径的设计行为,不是失控** ⇒ `nominal_accesses` 在该路径
    #    **不是条数契约** ⇒ **禁止**在此施加条数断言(否则该路径每条流恒红
    #    ⇒ 参数同源对照档不可复现)。
    seq = d.get("sequence") or {}
    mode = d.get("mode") or ""
    # ⚠️ **D-5 修复**:键名对齐 —— 探针写的是 `nominal_accesses`
    #    (旧代码读 `num_accesses` ⇒ 恒读不到 ⇒ 断言被**静默跳过**)。
    nominal = int(seq.get("nominal_accesses", 0) or 0)
    actual = int(d.get("accesses", 0))
    if mode == "same_source_trace":
        if nominal and actual != nominal:
            raise SystemExit(
                f"✗ 流 {name} 的**序列被截断**:名义 {nominal} 条、实测 {actual} 条"
                f" ⇒ 数据不可信(检查 max tick / duration 口径)"
                f"⇒ 拒绝把截断数据带入结论")
    elif mode == "parameter_same_source":
        # ⚠️ 该路径**不做**条数断言(见上方注释);仅登记实际/名义比例供追溯。
        pass
    else:
        raise SystemExit(
            f"✗ 流 {name} 的 mode 未知({mode!r})⇒ 无法判定条数契约"
            f" ⇒ 拒绝出结论(避免因未知口径而静默放行)")

    rec = {
        "name": name,
        "mode": mode,
        "profile": seq.get("profile"),
        "hit_rate": 1.0 - float(hr),
        "avg_latency_ns": d.get("avg_latency_ns"),
        "hits": d.get("hits"),
        "misses": d.get("misses"),
        "accesses": d.get("accesses"),
        "nominal_accesses": nominal,
        # ⚠️ **workload 特征** ⇒ 供分析器用 R-B 规则估 h_0(不依赖实测命中率)
        "working_set_bytes": seq.get("working_set_bytes"),
        "cache_bytes": seq.get("cache_bytes"),
        # ⚠️ breaking 流可声明**期望误差下界**(per §4.4)
        "expected_error_floor_pp": seq.get("expected_error_floor_pp"),
    }
    out = pathlib.Path(out_dir) / f"stream_{name}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rec, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8")
    print(f"[stream_rec] {name}: h_m={rec['hit_rate']:.4f} "
          f"L_gem5={rec['avg_latency_ns']:.3f}ns")


if __name__ == "__main__":
    main()
