"""RunContext - 封装单个运行目录的只读视图.

提供对仿真运行目录的统一访问接口，支持：
- 实时检测仿真进程状态
- 增量读取 stats.jsonl 流式数据
- 缓存 config.json 和 meta.json 内容
"""

from __future__ import annotations

import functools
import json
import os
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class RunContext:
    """封装单个运行目录的只读视图."""

    run_id: str
    root: Path

    def __init__(self, run_path: Path | str) -> None:
        """初始化 RunContext.

        Args:
            run_path: 运行目录路径（runs/run_xxx/）
        """
        self.root = Path(run_path)
        self.run_id = self.root.name

        self._config_cache: Optional[Dict] = None
        self._meta_cache: Optional[Dict] = None
        self._last_stats_offset: int = 0
        self._last_stats_offset_mtime: float = 0.0

    def is_active(self) -> bool:
        """检测仿真是否仍在运行.

        判断逻辑（按优先级）：
        1. 若存在 pid 文件且进程存活 → true
        2. 若 stats.jsonl 的 mtime 在最近 5 秒内有更新 → true
        3. 否则 → false
        """
        pid_file = self.root / "pid"
        if pid_file.exists():
            try:
                pid = int(pid_file.read_text().strip())
                os.kill(pid, 0)
                return True
            except (ValueError, OSError):
                pass

        stats_file = self.root / "stats.jsonl"
        if stats_file.exists():
            mtime = stats_file.stat().st_mtime
            if time.time() - mtime < 5:
                return True

        return False

    def config(self) -> Dict:
        """读取并缓存 config.json 内容."""
        if self._config_cache is None:
            config_file = self.root / "config.json"
            if config_file.exists():
                self._config_cache = json.loads(config_file.read_text(encoding="utf-8"))
            else:
                self._config_cache = {}
        return self._config_cache

    def stats(self, offset: int = 0) -> Tuple[List[Dict], int]:
        """增量读取 stats.jsonl.

        从指定字节偏移量 offset 读取新增行，
        返回 (new_records, new_offset)，其中 new_offset 是下次读取的起始位置.

        处理不完整行（JSONDecodeError）时回退到上次成功位置.

        Args:
            offset: 字节偏移量，0 表示从头读取

        Returns:
            (new_records, new_offset) 元组
        """
        stats_file = self.root / "stats.jsonl"
        if not stats_file.exists():
            return [], 0

        file_size = stats_file.stat().st_size
        if offset >= file_size:
            return [], offset

        try:
            with open(stats_file, "rb") as f:
                f.seek(offset)
                new_records: List[Dict] = []
                last_valid_pos = offset
                start_pos = offset

                while True:
                    pos = f.tell()
                    line_bytes = f.readline()
                    if not line_bytes:
                        break
                    line = line_bytes.decode("utf-8").rstrip("\n")
                    if not line:
                        continue
                    try:
                        record = json.loads(line)
                        new_records.append(record)
                        last_valid_pos = pos + len(line_bytes)
                    except json.JSONDecodeError:
                        break

                if new_records:
                    self._last_stats_offset = last_valid_pos
                    self._last_stats_offset_mtime = time.time()

                return new_records, last_valid_pos

        except OSError:
            return [], offset

    def metrics(self) -> Optional[Dict]:
        """读取 metrics.json（若存在），否则返回 None."""
        metrics_file = self.root / "metrics.json"
        if metrics_file.exists():
            return json.loads(metrics_file.read_text(encoding="utf-8"))
        return None

    def report(self) -> Optional[str]:
        """返回 report.html 文件路径（若存在），否则返回 None."""
        report_file = self.root / "report.html"
        if report_file.exists():
            return str(report_file.resolve())
        return None

    def topology_png(self) -> Optional[str]:
        """返回 topology.png 文件路径（若存在），否则返回 None."""
        png_file = self.root / "topology.png"
        if png_file.exists():
            return str(png_file.resolve())
        return None

    def meta(self) -> Dict:
        """读取并缓存 meta.json 内容（包含时间戳、参数、rerun_count 等）."""
        if self._meta_cache is None:
            meta_file = self.root / "meta.json"
            if meta_file.exists():
                self._meta_cache = json.loads(meta_file.read_text(encoding="utf-8"))
            else:
                self._meta_cache = {}
        return self._meta_cache

    def reload(self) -> None:
        """清空内存缓存，重新扫描目录内容."""
        self._config_cache = None
        self._meta_cache = None
        self._last_stats_offset = 0
        self._last_stats_offset_mtime = 0.0


class RunsIndex:
    """管理 runs/ 目录，扫描所有 RunContext."""

    def __init__(self, runs_dir: str | Path = Path("runs")) -> None:
        """初始化 RunsIndex.

        Args:
            runs_dir: runs 目录路径
        """
        self.runs_dir = Path(runs_dir)

    def list_runs(self) -> List[RunContext]:
        """扫描 runs/ 下所有有效运行目录，返回按时间倒序的 RunContext 列表.

        跳过无效目录（无 config.json）。
        跳过损坏目录时记录日志但不抛出异常。
        """
        import logging

        if not self.runs_dir.exists():
            return []

        runs: List[RunContext] = []
        for entry in self.runs_dir.iterdir():
            if not entry.is_dir():
                continue
            if not entry.name.startswith("run_"):
                continue

            try:
                config_file = entry / "config.json"
                if not config_file.exists():
                    logging.warning("Skipping invalid run directory (no config.json): %s", entry.name)
                    continue
                runs.append(RunContext(entry))
            except Exception as e:
                logging.warning("Skipping corrupted run directory: %s (%s)", entry.name, e)

        return sorted(
            runs,
            key=lambda r: r.meta().get("created_at", ""),
            reverse=True
        )

    def get_run(self, run_id: str) -> Optional[RunContext]:
        """根据 run_id 获取对应的 RunContext，不存在则返回 None."""
        run_path = self.runs_dir / run_id
        if not run_path.exists():
            return None
        config_file = run_path / "config.json"
        if not config_file.exists():
            return None
        return RunContext(run_path)

    def create_run(self, config_json: str | Dict, params: Dict) -> RunContext:
        """创建新的运行目录.

        Args:
            config_json: JSON 配置字符串或 dict
            params: 仿真参数字典（cycles, interval, seed, binary_path 等）
                    这些参数会写入 meta.json

        Returns:
            新创建的 RunContext

        行为：
        1. 生成目录名: run_{timestamp}（格式 run_YYYY-MM-DD_HHMMSS）
        2. 创建目录
        3. 写入 config.json
        4. 写入 meta.json（包含 timestamp, params, rerun_count=0）
        5. 返回 RunContext
        """
        import random
        import string

        from datetime import datetime

        timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
        base_name = f"run_{timestamp}"

        run_path = self.runs_dir / base_name

        if run_path.exists():
            suffix = ''.join(random.choices(string.ascii_lowercase + string.digits, k=6))
            run_path = self.runs_dir / f"{base_name}_{suffix}"

        run_path.mkdir(parents=True, exist_ok=True)

        if isinstance(config_json, str):
            config_data = json.loads(config_json)
        else:
            config_data = config_json

        config_file = run_path / "config.json"
        config_file.write_text(json.dumps(config_data, indent=2, ensure_ascii=False), encoding="utf-8")

        meta_data = {
            "created_at": datetime.now().isoformat(),
            "last_run": None,
            "rerun_count": 0,
            "params": {
                "cycles": params.get("cycles", 50000),
                "interval": params.get("interval", 1000),
                "seed": params.get("seed", 0),
                "binary_path": params.get("binary_path", ""),
            }
        }

        meta_file = run_path / "meta.json"
        meta_file.write_text(json.dumps(meta_data, indent=2, ensure_ascii=False), encoding="utf-8")

        return RunContext(run_path)
