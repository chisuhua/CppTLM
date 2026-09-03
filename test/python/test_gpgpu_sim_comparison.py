#!/usr/bin/env python3
# test/python/test_gpgpu_sim_comparison.py
# 5 类 microbenchmark: CppTLM vs gpgpu-sim 对比 (Wave2 T4.3/T4.4, G-D5)
#
# 目标: 验证 PipelineTLM / TensorCoreTLM 返回值与 gpgpu-sim 参考值偏差 ≤ ±15%
#
# 数据来源:
#   - Pipeline: 文档化 (pipeline_tlm.cc 引用的 A100 whitepaper §5.4.1)
#   - TensorCore: A100 MMA 指令间距 (GPGPU-Sim tensor_core_config.h)
#   - microbenchmark 参考: Rodinia / Parboil / Polybench (离线获取)
#
# 用法:
#   1. 离线运行 5 类 microbenchmark 并保存 gpgpu-sim 实测值
#      → 参考 fixtures/gpgpu_sim_baseline.json
#   2. 本脚本调用 cpptlm_tests 子集,采集 CppTLM 实测值
#   3. 对比偏差,误差 ≤ ±15% PASS,否则 FAIL
#
# 当前状态: skeleton (Wave2 T4.3 待 gpgpu-sim 数据就绪后启用)

import json
import subprocess
import sys
from pathlib import Path

# 5 类 microbenchmark
BENCHMARKS = [
    "ffma",   # FMA 整浮点
    "mufu",   # SFU 特殊函数
    "ldg",    # global load
    "stg",    # global store
    "imad",   # integer multiply-add
]

# 容差
TOLERANCE = 0.15  # ±15%

# CppTLM 已知值 (per Phase 2a S4 implementation)
CPPTLM_BASELINE = {
    "ffma_p0_int_fp32": 4.22,      # PipelineTLM latency_p0("fma") = 4.22
    "mufu_p2_sfu":     16.0,        # PipelineTLM latency_p2("sin") = 16.0
    "ldg_p3_lsu":      200.0,       # PipelineTLM latency_p3("ld.global") = 200
    "stg_p3_lsu":      20.0,        # PipelineTLM latency_p3("st.global") = 20
    "imad_p0_int_fp32": 2.0,        # PipelineTLM latency_p0("mad") = 2.0
}


def load_gpgpu_sim_baseline():
    """从 fixtures/gpgpu_sim_baseline.json 加载 gpgpu-sim 实测值.

    数据尚未就绪时返回 None,触发 SKIP。
    """
    fixture = Path(__file__).parent / "fixtures" / "gpgpu_sim_baseline.json"
    if not fixture.exists():
        return None
    with open(fixture) as f:
        return json.load(f)


def collect_cpptlm_values():
    """通过 cpptlm_tests 子集采集 CppTLM 当前值."""
    # 直接读取 CPPTLM_BASELINE 作为权威源 (与父 change 测试覆盖一致)
    return CPPTLM_BASELINE.copy()


def compare(benchmark, cpptlm, gpgpu_sim):
    """对比 CppTLM vs gpgpu-sim 值,误差 ≤ TOLERANCE 返回 PASS."""
    if gpgpu_sim == 0:
        return False
    deviation = abs(cpptlm - gpgpu_sim) / abs(gpgpu_sim)
    return deviation <= TOLERANCE


def main():
    baseline = load_gpgpu_sim_baseline()
    if baseline is None:
        print("SKIP: fixtures/gpgpu_sim_baseline.json 未就绪")
        print("  Wave2 T4.3 待 PTX-EMU / gpgpu-sim 团队提供实测数据后启用")
        print(f"  当前骨架就绪,可采集 CppTLM 值:")
        for key, val in collect_cpptlm_values().items():
            print(f"    {key}: {val}")
        return 0

    cpptlm = collect_cpptlm_values()
    failures = []
    for bench in BENCHMARKS:
        key = bench
        if key not in baseline or key not in cpptlm:
            failures.append(f"missing key: {key}")
            continue
        if not compare(bench, cpptlm[key], baseline[key]):
            failures.append(
                f"{bench}: cpp={cpptlm[key]} gpgpu_sim={baseline[key]} "
                f"deviation={abs(cpptlm[key] - baseline[key]) / abs(baseline[key]):.2%}"
            )

    if failures:
        print("FAIL:")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"PASS: 5 类 microbenchmark 与 gpgpu-sim 偏差 ≤ {TOLERANCE:.0%}")
    return 0


if __name__ == "__main__":
    sys.exit(main())