#!/usr/bin/env python3
"""cpptlm/tests/test_result.py — Result parser unit tests."""

import unittest
import sys
import os
import tempfile
import json

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.simulation.result import Result


class TestResult(unittest.TestCase):
    def test_from_jsonl_loads_records(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 5}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"requests": 10}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            self.assertEqual(len(result._records), 2)
        finally:
            os.unlink(path)

    def test_records_returns_all(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 5}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.memory", "data": {"reads": 3}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            records = result.records()
            self.assertEqual(len(records), 2)
        finally:
            os.unlink(path)

    def test_records_filter_by_group(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 5}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.memory", "data": {"reads": 3}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            cache_records = result.records(group="system.cache")
            self.assertEqual(len(cache_records), 1)
            self.assertEqual(cache_records[0]["group"], "system.cache")
        finally:
            os.unlink(path)

    def test_timestamps_returns_list(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            timestamps = result.timestamps()
            self.assertEqual(timestamps, [1000, 2000])
        finally:
            os.unlink(path)

    def test_groups_returns_unique(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.memory", "data": {}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "system.cache", "data": {}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            groups = result.groups()
            self.assertEqual(set(groups), {"system.cache", "system.memory"})
        finally:
            os.unlink(path)


class TestResultEmptyFile(unittest.TestCase):
    def test_empty_file_handled(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            path = f.name

        try:
            result = Result.from_jsonl(path)
            self.assertEqual(len(result._records), 0)
            self.assertEqual(result.timestamps(), [])
            self.assertEqual(result.groups(), [])
        finally:
            os.unlink(path)


if __name__ == "__main__":
    unittest.main()