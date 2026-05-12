"""cpptlm/simulation/runner.py — SimulationRunner subprocess wrapper."""

from __future__ import annotations

import subprocess
import json
from pathlib import Path
from typing import Optional, List


class SimulationRunner:
    """Run CppTLM simulation as subprocess and parse results."""

    def __init__(
        self,
        binary_path: str = "./build/bin/cpptlm_sim",
        config_path: Optional[str] = None
    ):
        self.binary_path = binary_path
        self.config_path = config_path
        self._args: List[str] = []

    def add_arg(self, key: str, value: str) -> "SimulationRunner":
        self._args.extend([key, value])
        return self

    def run(self, timeout: int = 300) -> subprocess.CompletedProcess:
        if not Path(self.binary_path).exists():
            return subprocess.CompletedProcess(
                args=[self.binary_path],
                returncode=127,
                stdout="",
                stderr=f"Binary not found: {self.binary_path}"
            )

        cmd = [self.binary_path]
        if self.config_path:
            cmd.append(self.config_path)
        cmd.extend(self._args)

        try:
            return subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout
            )
        except FileNotFoundError:
            return subprocess.CompletedProcess(
                args=cmd,
                returncode=127,
                stdout="",
                stderr=f"Binary not found: {self.binary_path}"
            )
        except subprocess.TimeoutExpired:
            return subprocess.CompletedProcess(
                args=cmd,
                returncode=124,
                stdout="",
                stderr=f"Timeout after {timeout}s"
            )

    def run_with_stats(self, stats_output: str, interval: int = 10000) -> subprocess.CompletedProcess:
        self._args.append("--stream-stats")
        self._args.extend(["--stream-interval", str(interval)])
        self._args.extend(["--stream-path", stats_output])
        return self.run()