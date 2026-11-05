#!/usr/bin/env python3
"""cep_b4_observation_keys_check.py —— H3 v2 观测键的**文件层**校验

per `ADR-CEP-06` **SU#4** §② 约束 5(「同一事务流」判据的机器锚):
  · `metrics.json` 顶层须带 `observation_keys`(实例级 / 地址级);
  · 实例级键集须**两个都在**(memory_checked / blocking_subject),且交集覆盖
    **全部** memory_checked 实例(= 覆盖率 1.0)⇒ 退化装置(两段拼接)判红;
  · `trace.h5` 的 `run_summary/observation_keys` 组须存在(4 件套镜像 ⇒ 文件层可查)。

用法: python3 cep_b4_observation_keys_check.py <run_dir>
退出码: 0 = 通过;1 = 有问题;2 = 输入错误
"""
import json
import sys

import h5py


def main(argv):
    if len(argv) != 2:
        print("用法: cep_b4_observation_keys_check.py <run_dir>", file=sys.stderr)
        return 2
    run_dir = argv[1]
    problems = []

    try:
        with open(f"{run_dir}/metrics.json", encoding="utf-8") as f:
            metrics = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"✗ 读 metrics.json 失败: {exc}", file=sys.stderr)
        return 1

    obs = metrics.get("observation_keys") or {}
    if not obs:
        problems.append("metrics.json 缺 observation_keys(AC-5 机器锚缺失)")
    else:
        checked = set(obs.get("memory_checked_txn_ids") or [])
        subjects = set(obs.get("blocking_subject_txn_ids") or [])
        if not checked:
            problems.append("observation_keys 缺 memory_checked_txn_ids")
        if not subjects:
            problems.append("observation_keys 缺 blocking_subject_txn_ids")
        if checked and subjects:
            inter = len(checked & subjects)
            if inter != len(checked):
                problems.append(
                    f"同一事务流覆盖率 {inter}/{len(checked)} < 1.0"
                    f"(内存操作实例必须**同时**受阻塞支配 ⇒ 退化装置判红)")
        if "memory_checked_addrs" not in obs:
            problems.append("observation_keys 缺 memory_checked_addrs(地址级辅证)")

    try:
        with h5py.File(f"{run_dir}/trace.h5", "r") as f:
            if "observation_keys" not in f["run_summary"]:
                problems.append("trace.h5 缺 run_summary/observation_keys(4 件套未镜像观测键)")
    except (OSError, KeyError) as exc:
        problems.append(f"读 trace.h5 失败: {exc}")

    if problems:
        for p in problems:
            print(f"  ✗ {p}", file=sys.stderr)
        return 1
    print("  ✓ 观测键齐备(metrics.json + trace.h5)+ 同一事务流覆盖率 1.0")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
