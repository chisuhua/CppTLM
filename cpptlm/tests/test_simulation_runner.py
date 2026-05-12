#!/usr/bin/env python3
"""cpptlm/tests/test_simulation_runner.py — SimulationRunner unit tests."""

import unittest
import sys
import os
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))

from cpptlm.simulation.runner import SimulationRunner


class TestSimulationRunner(unittest.TestCase):
    def test_runner_creation_default(self):
        runner = SimulationRunner()
        self.assertEqual(runner.binary_path, "./build/bin/cpptlm_sim")
        self.assertIsNone(runner.config_path)

    def test_runner_creation_with_binary_path(self):
        runner = SimulationRunner(binary_path="/custom/path/sim")
        self.assertEqual(runner.binary_path, "/custom/path/sim")

    def test_runner_creation_with_config(self):
        runner = SimulationRunner(config_path="configs/mesh.json")
        self.assertEqual(runner.config_path, "configs/mesh.json")

    def test_add_arg_single(self):
        runner = SimulationRunner()
        runner.add_arg("--stream-stats", "output.jsonl")
        self.assertIn("--stream-stats", runner._args)
        self.assertIn("output.jsonl", runner._args)

    def test_add_arg_chainable(self):
        runner = SimulationRunner()
        result = runner.add_arg("--verbose", "1")
        self.assertIs(result, runner)

    def test_run_with_missing_binary(self):
        runner = SimulationRunner(binary_path="/nonexistent/binary")
        result = runner.run(timeout=5)
        self.assertNotEqual(result.returncode, 0)

    def test_build_command(self):
        runner = SimulationRunner(binary_path="/bin/echo", config_path="test.json")
        runner.add_arg("--flag", "value")
        result = runner.run()
        # echo prints args to stdout
        self.assertEqual(result.returncode, 0)
        self.assertIn("test.json", result.stdout)
        self.assertIn("--flag", result.stdout)


class TestSimulationRunnerWithConfig(unittest.TestCase):
    def test_run_with_temp_config(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write('{"modules":[],"connections":[]}')
            config_path = f.name

        try:
            runner = SimulationRunner(config_path=config_path)
            runner.add_arg("--help", "")
            result = runner.run(timeout=10)
            # Should not crash even with empty config
            self.assertIsNotNone(result)
        finally:
            os.unlink(config_path)


if __name__ == "__main__":
    unittest.main()