"""仿真进程管理模块.

功能：
- 命令构建（所有参数的组合方式）
- 拓扑文件生成（graphviz dot 输出）
- 报告生成（stats 分析）
- 进程生命周期管理

约束：
- 不直接读写 meta.json
- 不直接访问 RunContext/RunsIndex
"""

from __future__ import annotations

import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional


class SimulationRunner:
    """统一的仿真进程管理类.

    职责：
    - 命令构建（所有参数的组合方式）
    - 拓扑文件生成（graphviz dot 输出）
    - 报告生成（stats 分析）
    - 进程生命周期管理

    约束：
    - 不直接读写 meta.json
    - 不直接访问 RunContext/RunsIndex
    """

    def __init__(self, binary_path: Path, run_root: Path):
        """初始化仿真运行器.

        Args:
            binary_path: 仿真二进制文件路径
            run_root: 运行根目录
        """
        self.binary = binary_path
        self.root = run_root

    def build_command(
        self,
        config_path: Optional[Path] = None,
        cycles: int = 50000,
        seed: int = 0,
        interval: int = 1000,
        stream_path: Optional[Path] = None,
        extra_args: Optional[List[str]] = None,
    ) -> List[str]:
        """构建完整的仿真命令.

        Args:
            config_path: 配置文件路径
            cycles: 仿真周期数
            seed: 随机种子
            interval: 流统计间隔
            stream_path: 流统计输出路径
            extra_args: 额外命令行参数

        Returns:
            命令列表
        """
        cmd = [str(self.binary)]
        if config_path:
            cmd.append(str(config_path))
        if stream_path:
            cmd.extend([
                "--stream-stats",
                "--stream-interval", str(interval),
                "--stream-path", str(stream_path),
            ])
        cmd.extend(["--cycles", str(cycles)])
        if seed != 0:
            cmd.extend(["--seed", str(seed)])
        if extra_args:
            cmd.extend(extra_args)
        return cmd

    def launch(
        self,
        config_path: Optional[Path] = None,
        cycles: int = 50000,
        seed: int = 0,
        interval: int = 1000,
        stream_path: Optional[Path] = None,
        extra_args: Optional[List[str]] = None,
    ) -> subprocess.Popen:
        """启动仿真进程，返回 Popen 对象.

        Args:
            config_path: 配置文件路径
            cycles: 仿真周期数
            seed: 随机种子
            interval: 流统计间隔
            stream_path: 流统计输出路径
            extra_args: 额外命令行参数

        Returns:
            subprocess.Popen 对象
        """
        cmd = self.build_command(
            config_path, cycles, seed, interval, stream_path, extra_args
        )
        pid_file = self.root / "pid"
        proc = subprocess.Popen(cmd, cwd=str(self.root))
        pid_file.write_text(str(proc.pid), encoding="utf-8")
        return proc

    def generate_topology_dot(self, config_path: Path, output_path: Path) -> bool:
        """生成拓扑 DOT 文件.

        Args:
            config_path: 配置文件路径
            output_path: DOT 文件输出路径

        Returns:
            是否成功生成
        """
        cmd = [
            str(self.binary),
            str(config_path),
            "--emit-dot",
            str(output_path),
        ]
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=60
        )
        return result.returncode == 0

    def generate_report(self, stats_path: Path, output_path: Path) -> bool:
        """生成统计报告.

        Args:
            stats_path: 统计文件路径
            output_path: 报告输出路径

        Returns:
            是否成功生成
        """
        cmd = [
            str(self.binary),
            "--report",
            str(stats_path),
            "--output",
            str(output_path),
        ]
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=60
        )
        return result.returncode == 0

    @staticmethod
    def from_meta(run_root: Path, meta: Dict[str, Any]) -> "SimulationRunner":
        """从 run metadata 创建 SimulationRunner.

        Args:
            run_root: 运行根目录
            meta: 运行元数据字典

        Returns:
            SimulationRunner 实例
        """
        binary_path = Path(
            meta.get("params", {}).get("binary_path", "")
        )
        return SimulationRunner(binary_path, run_root)