#!/usr/bin/env python3
"""
demo_pcie_full_e2e.py — PCIe 全链路 E2E Demo (PcieEndpointIP <-> Host)

运行编译好的 C++ demo (demo_pcie_full_e2e)，验证 HostBypassTLM +
PcieRootComplexTLM 与 PcieEndpointIP 之间的端到端桥接：
  1. HostBypassTLM: 软件 bring-up（配置空间写/读回、BAR 访问）
  2. PcieRootComplexTLM: PCIe 枚举 + 配置访问 + BAR 分配/路由
  3. Per-VF 配置空间隔离

用法:
  python3 examples/demo_pcie_full_e2e.py
  python3 examples/demo_pcie_full_e2e.py --binary build/bin/demo_pcie_full_e2e

输出:
  验证每一步结果，最终报告 ALL PASSED / FAILED（非零退出码表示失败）
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path


def find_binary() -> Path:
    """定位编译好的 demo_pcie_full_e2e 二进制。"""
    candidates = [
        Path("build/bin/demo_pcie_full_e2e"),
        Path("build/bin/examples/demo_pcie_full_e2e"),
        Path("../build/bin/demo_pcie_full_e2e"),
        Path("../build/bin/examples/demo_pcie_full_e2e"),
    ]
    for p in candidates:
        if p.is_file() and os.access(p, os.X_OK):
            return p.resolve()
    return Path()


def main() -> int:
    parser = argparse.ArgumentParser(description="CppTLM PCIe Full E2E Demo")
    parser.add_argument("--binary", type=str, default=None,
                        help="demo_pcie_full_e2e 二进制路径（默认自动查找）")
    args = parser.parse_args()

    binary = Path(args.binary) if args.binary else find_binary()
    if not binary.is_file():
        print("[ERROR] demo_pcie_full_e2e binary not found.")
        print("        Build it first:")
        print("          cmake -S . -B build -DCMAKE_BUILD_TYPE=Release")
        print("          cmake --build build -j$(nproc)")
        print("        (produces build/bin/demo_pcie_full_e2e)")
        return 1

    print(f"[Run] {binary}")
    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120)
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)

    if result.returncode != 0:
        print("[FAIL] demo_pcie_full_e2e exited non-zero "
              f"(code={result.returncode})")
        return 1

    stdout = result.stdout
    expected = [
        "HostBypassTLM: Software Bring-up Demo",
        "PcieRootComplexTLM: Enumeration + Configuration Demo",
        "E2E Demo Summary: ALL PASSED",
        "CppTLM Phase 7 PCIe Host Bypass + Root Complex: COMPLETE",
    ]
    missing = [m for m in expected if m not in stdout]
    if missing:
        print("[FAIL] Expected markers missing from demo output:")
        for m in missing:
            print(f"        - {m}")
        return 1

    print("[OK] All expected markers present — E2E demo verification passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
