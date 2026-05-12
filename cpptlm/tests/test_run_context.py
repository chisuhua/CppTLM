#!/usr/bin/env python3
"""cpptlm/tests/test_run_context.py — RunContext and RunsIndex unit tests."""

import unittest
import sys
import os
import tempfile
import json
import time
from pathlib import Path
from datetime import datetime
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.visualization.run_context import RunContext, RunsIndex


class TestRunContext(unittest.TestCase):
    """RunContext unit tests."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.run_path = Path(self.temp_dir) / "run_test"
        self.run_path.mkdir()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_is_active_no_files(self):
        ctx = RunContext(self.run_path)
        self.assertFalse(ctx.is_active())

    def test_is_active_pid_exists(self):
        pid_file = self.run_path / "pid"
        pid_file.write_text("99999\n")
        ctx = RunContext(self.run_path)
        self.assertFalse(ctx.is_active())

    def test_is_active_stats_recent(self):
        stats_file = self.run_path / "stats.jsonl"
        stats_file.write_text('{"step": 1}\n')
        ctx = RunContext(self.run_path)
        with mock.patch('time.time', return_value=stats_file.stat().st_mtime + 2):
            self.assertTrue(ctx.is_active())

    def test_is_active_stats_stale(self):
        stats_file = self.run_path / "stats.jsonl"
        stats_file.write_text('{"step": 1}\n')
        ctx = RunContext(self.run_path)
        with mock.patch('time.time', return_value=stats_file.stat().st_mtime + 10):
            self.assertFalse(ctx.is_active())

    def test_config_returns_cached(self):
        config_file = self.run_path / "config.json"
        config_data = {"cycles": 1000, "seed": 42}
        config_file.write_text(json.dumps(config_data))
        ctx = RunContext(self.run_path)
        self.assertEqual(ctx.config(), config_data)
        config_file.write_text(json.dumps({"cycles": 9999}))
        self.assertEqual(ctx.config(), config_data)

    def test_config_not_exists(self):
        ctx = RunContext(self.run_path)
        self.assertEqual(ctx.config(), {})

    def test_stats_incremental_read(self):
        stats_file = self.run_path / "stats.jsonl"
        stats_file.write_text('{"step": 1}\n{"step": 2}\n')
        ctx = RunContext(self.run_path)
        records, offset = ctx.stats(0)
        self.assertEqual(len(records), 2)
        self.assertGreater(offset, 0)
        stats_file.write_text('{"step": 1}\n{"step": 2}\n{"step": 3}\n')
        records2, offset2 = ctx.stats(offset)
        self.assertEqual(len(records2), 1)
        self.assertEqual(records2[0]["step"], 3)

    def test_stats_handles_incomplete_line(self):
        stats_file = self.run_path / "stats.jsonl"
        stats_file.write_text('{"step": 1}\n{"step": ')
        ctx = RunContext(self.run_path)
        records, offset = ctx.stats(0)
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]["step"], 1)

    def test_stats_empty_file(self):
        stats_file = self.run_path / "stats.jsonl"
        stats_file.write_text("")
        ctx = RunContext(self.run_path)
        records, offset = ctx.stats(0)
        self.assertEqual(records, [])
        self.assertEqual(offset, 0)

    def test_stats_nonexistent(self):
        ctx = RunContext(self.run_path)
        records, offset = ctx.stats(0)
        self.assertEqual(records, [])
        self.assertEqual(offset, 0)

    def test_metrics_exists(self):
        metrics_file = self.run_path / "metrics.json"
        metrics_data = {"total_cycles": 50000, "throughput": 0.95}
        metrics_file.write_text(json.dumps(metrics_data))
        ctx = RunContext(self.run_path)
        self.assertEqual(ctx.metrics(), metrics_data)

    def test_metrics_not_exists(self):
        ctx = RunContext(self.run_path)
        self.assertIsNone(ctx.metrics())

    def test_report_exists(self):
        report_file = self.run_path / "report.html"
        report_file.write_text("<html></html>")
        ctx = RunContext(self.run_path)
        result = ctx.report()
        self.assertIsNotNone(result)
        self.assertTrue(Path(result).is_absolute())
        self.assertEqual(Path(result).name, "report.html")

    def test_report_not_exists(self):
        ctx = RunContext(self.run_path)
        self.assertIsNone(ctx.report())

    def test_topology_png_exists(self):
        png_file = self.run_path / "topology.png"
        png_file.write_text("\x89PNG\r\n\x1a\n")
        ctx = RunContext(self.run_path)
        result = ctx.topology_png()
        self.assertIsNotNone(result)
        self.assertTrue(Path(result).is_absolute())
        self.assertEqual(Path(result).name, "topology.png")

    def test_topology_png_not_exists(self):
        ctx = RunContext(self.run_path)
        self.assertIsNone(ctx.topology_png())

    def test_meta_returns_cached(self):
        meta_file = self.run_path / "meta.json"
        meta_data = {"created_at": "2024-01-01T00:00:00", "rerun_count": 1}
        meta_file.write_text(json.dumps(meta_data))
        ctx = RunContext(self.run_path)
        self.assertEqual(ctx.meta(), meta_data)
        meta_file.write_text(json.dumps({"created_at": "2025-01-01T00:00:00"}))
        self.assertEqual(ctx.meta(), meta_data)

    def test_meta_not_exists(self):
        ctx = RunContext(self.run_path)
        self.assertEqual(ctx.meta(), {})

    def test_reload_clears_cache(self):
        config_file = self.run_path / "config.json"
        config_file.write_text(json.dumps({"cycles": 1000}))
        meta_file = self.run_path / "meta.json"
        meta_file.write_text(json.dumps({"created_at": "2024-01-01T00:00:00"}))
        ctx = RunContext(self.run_path)
        self.assertEqual(ctx.config(), {"cycles": 1000})
        self.assertEqual(ctx.meta()["created_at"], "2024-01-01T00:00:00")
        config_file.write_text(json.dumps({"cycles": 9999}))
        meta_file.write_text(json.dumps({"created_at": "2025-01-01T00:00:00"}))
        self.assertEqual(ctx.config(), {"cycles": 1000})
        ctx.reload()
        self.assertEqual(ctx.config(), {"cycles": 9999})
        self.assertEqual(ctx.meta()["created_at"], "2025-01-01T00:00:00")


class TestRunsIndex(unittest.TestCase):
    """RunsIndex unit tests."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.runs_dir = Path(self.temp_dir) / "runs"
        self.runs_dir.mkdir()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_list_runs_empty(self):
        idx = RunsIndex(self.runs_dir)
        self.assertEqual(idx.list_runs(), [])

    def test_list_runs_skips_invalid(self):
        invalid_dir = self.runs_dir / "run_invalid"
        invalid_dir.mkdir()
        (invalid_dir / "README.txt").write_text("no config")
        with mock.patch('logging.warning') as mock_warning:
            idx = RunsIndex(self.runs_dir)
            result = idx.list_runs()
            self.assertEqual(result, [])
            mock_warning.assert_called_once()

    def test_list_runs_sorted(self):
        run1 = self.runs_dir / "run_2024-01-01_000000"
        run1.mkdir()
        (run1 / "config.json").write_text(json.dumps({}))
        (run1 / "meta.json").write_text(json.dumps({"created_at": "2024-01-01T00:00:00"}))
        run2 = self.runs_dir / "run_2024-01-02_000000"
        run2.mkdir()
        (run2 / "config.json").write_text(json.dumps({}))
        (run2 / "meta.json").write_text(json.dumps({"created_at": "2024-01-02T00:00:00"}))
        idx = RunsIndex(self.runs_dir)
        result = idx.list_runs()
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0].run_id, "run_2024-01-02_000000")
        self.assertEqual(result[1].run_id, "run_2024-01-01_000000")

    def test_list_runs_ignores_non_run_prefix(self):
        other_dir = self.runs_dir / "backup_2024-01-01"
        other_dir.mkdir()
        (other_dir / "config.json").write_text(json.dumps({}))
        idx = RunsIndex(self.runs_dir)
        self.assertEqual(idx.list_runs(), [])

    def test_get_run_exists(self):
        run_dir = self.runs_dir / "run_test"
        run_dir.mkdir()
        (run_dir / "config.json").write_text(json.dumps({"cycles": 5000}))
        idx = RunsIndex(self.runs_dir)
        ctx = idx.get_run("run_test")
        self.assertIsNotNone(ctx)
        self.assertEqual(ctx.config(), {"cycles": 5000})

    def test_get_run_not_exists(self):
        idx = RunsIndex(self.runs_dir)
        ctx = idx.get_run("nonexistent")
        self.assertIsNone(ctx)

    def test_create_run_basic(self):
        idx = RunsIndex(self.runs_dir)
        ctx = idx.create_run({"cycles": 1000, "seed": 42}, {"cycles": 50000})
        self.assertTrue(ctx.root.exists())
        self.assertTrue((ctx.root / "config.json").exists())
        self.assertTrue((ctx.root / "meta.json").exists())
        self.assertEqual(ctx.config(), {"cycles": 1000, "seed": 42})

    def test_create_run_timestamp_format(self):
        idx = RunsIndex(self.runs_dir)
        ctx = idx.create_run({}, {})
        self.assertRegex(ctx.run_id, r'^run_\d{4}-\d{2}-\d{2}_\d{6}$')

    def test_create_run_conflict(self):
        idx = RunsIndex(self.runs_dir)
        ctx1 = idx.create_run({}, {})
        ctx2 = idx.create_run({}, {})
        self.assertNotEqual(ctx1.run_id, ctx2.run_id)
        self.assertNotEqual(ctx1.root, ctx2.root)

    def test_create_run_json_string(self):
        idx = RunsIndex(self.runs_dir)
        ctx = idx.create_run('{"cycles": 2000}', {})
        self.assertEqual(ctx.config(), {"cycles": 2000})

    def test_create_run_meta_params(self):
        idx = RunsIndex(self.runs_dir)
        ctx = idx.create_run({}, {"cycles": 50000, "interval": 100, "seed": 123, "binary_path": "/usr/bin/sim"})
        meta = ctx.meta()
        self.assertEqual(meta["params"]["cycles"], 50000)
        self.assertEqual(meta["params"]["interval"], 100)
        self.assertEqual(meta["params"]["seed"], 123)
        self.assertEqual(meta["params"]["binary_path"], "/usr/bin/sim")


if __name__ == "__main__":
    unittest.main()