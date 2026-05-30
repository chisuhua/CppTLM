#!/usr/bin/env python3
"""test_topo_cli.py — CLI integration tests"""

import unittest
import sys
import os
import tempfile
import json
import subprocess

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo import TopoOrchestrator, TopoLayer


class TestTopoCLI(unittest.TestCase):

    def test_cli_gen_help(self):
        result = subprocess.run(
            [sys.executable, '-m', 'cpptlm.topo.cli', 'gen', '--help'],
            capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn('--name', result.stdout)

    def test_cli_gen_basic(self):
        with tempfile.NamedTemporaryFile(suffix='.json', delete=False) as f:
            out_path = f.name
        try:
            result = subprocess.run(
                [sys.executable, '-m', 'cpptlm.topo.cli', 'gen',
                 '--name', 'test_soc', '--output', out_path],
                capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 0)
            self.assertTrue(os.path.exists(out_path))
            with open(out_path) as f:
                data = json.load(f)
            self.assertIn('modules', data)
        finally:
            os.unlink(out_path)

    def test_cli_variant(self):
        base = TopoLayer('base')
        base.add_module('cpu0', 'CPUTLM')
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            base_path = f.name
            json.dump(base.to_dict(), f)

        with tempfile.TemporaryDirectory() as tmpdir:
            result = subprocess.run(
                [sys.executable, '-m', 'cpptlm.topo.cli', 'variant',
                 '--base', base_path, '--variants', 'v1', 'v2',
                 '--output-dir', tmpdir],
                capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 0)
            self.assertTrue(os.path.exists(os.path.join(tmpdir, 'v1.json')))
            self.assertTrue(os.path.exists(os.path.join(tmpdir, 'v2.json')))

        os.unlink(base_path)

    def test_cli_patch(self):
        layer = TopoLayer('test')
        layer.add_module('cpu0', 'CPUTLM')
        layer.add_connection('cpu0', 'mem', latency=1)

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            in_path = f.name
            json.dump(layer.to_dict(), f)

        with tempfile.NamedTemporaryFile(suffix='.json', delete=False) as f:
            out_path = f.name

        try:
            result = subprocess.run(
                [sys.executable, '-m', 'cpptlm.topo.cli', 'patch',
                 '--input', in_path, '--output', out_path,
                 '--selector', 'cpu*', '--action', 'remove'],
                capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 0)
            with open(out_path) as f:
                data = json.load(f)
            connections = [(c['src'], c['dst']) for c in data.get('connections', [])]
            has_cpu_conn = any(src == 'cpu0' or dst == 'cpu0' for src, dst in connections)
            self.assertFalse(has_cpu_conn)
        finally:
            os.unlink(in_path)
            if os.path.exists(out_path):
                os.unlink(out_path)

    def test_cli_compare(self):
        base = TopoLayer('base')
        base.add_module('cpu0', 'CPUTLM')

        var = TopoLayer('variant')
        var.add_module('gpu0', 'GPUTLM')

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            base_path = f.name
            json.dump(base.to_dict(), f)

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            var_path = f.name
            json.dump(var.to_dict(), f)

        try:
            result = subprocess.run(
                [sys.executable, '-m', 'cpptlm.topo.cli', 'compare',
                 '--base', base_path, '--variants', var_path],
                capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 0)
        finally:
            os.unlink(base_path)
            os.unlink(var_path)

    def test_cli_missing_args(self):
        result = subprocess.run(
            [sys.executable, '-m', 'cpptlm.topo.cli', 'variant'],
            capture_output=True, text=True
        )
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()