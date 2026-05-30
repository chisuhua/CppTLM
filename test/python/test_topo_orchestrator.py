#!/usr/bin/env python3
"""test_topo_orchestrator.py — TopoOrchestrator TDD 测试"""

import unittest
import sys
import os
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo.orchestrator import TopoOrchestrator
from cpptlm.topo.layer import TopoLayer


class TestTopoOrchestrator(unittest.TestCase):

    def setUp(self):
        self.orch = TopoOrchestrator('test_orch')

    def test_construction(self):
        self.assertEqual(self.orch.name, 'test_orch')
        self.assertEqual(len(self.orch._layers), 0)

    def test_layer_factory(self):
        l = TopoOrchestrator.layer('cluster')
        self.assertEqual(l.name, 'cluster')

    def test_add_layer(self):
        l = TopoLayer('mem')
        self.orch.add_layer('memory', layers=[l])
        self.assertIsNotNone(self.orch.get_layer('memory'))

    def test_get_layer(self):
        self.orch.add_layer('cpu')
        found = self.orch.get_layer('cpu')
        self.assertIsNotNone(found)
        self.assertEqual(found.name, 'cpu')

    def test_remove_layer(self):
        self.orch.add_layer('cpu')
        self.orch.remove_layer('cpu')
        self.assertIsNone(self.orch.get_layer('cpu'))

    def test_save_load(self):
        l = TopoLayer('clust')
        l.add_module('cpu0', 'CPUTLM')
        l.add_connection('cpu0', 'cache', latency=1)
        self.orch.add_layer('cluster0', layers=[l])

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            path = f.name
        try:
            self.orch.save(path)
            o2 = TopoOrchestrator('test2')
            o2.load(path)
            self.assertIsNotNone(o2.get_layer('cluster0'))
        finally:
            os.unlink(path)

    def test_add_variant(self):
        l = TopoLayer('base')
        l.add_module('cpu0', 'CPUTLM')
        self.orch.add_layer('base', layers=[l])
        self.orch.add_variant('v1', 'base')
        result = self.orch.build_variant('v1')
        self.assertIsNotNone(result)

    def test_build_all_variants(self):
        l = TopoLayer('base')
        l.add_module('cpu0', 'CPUTLM')
        self.orch.add_layer('base', layers=[l])
        self.orch.add_variant('v1', 'base')
        self.orch.add_variant('v2', 'base')
        all_v = self.orch.build_all_variants()
        self.assertEqual(len(all_v), 2)

    def test_create_template(self):
        base = TopoLayer('base')
        base.add_module('cpu0', 'CPUTLM').add_module('cache', 'CacheTLM')
        self.orch.add_layer('base', layers=[base])
        self.orch.create_template('extended', 'base',
            modify=lambda l: l.add_module('cpu1', 'CPUTLM'))
        ext = self.orch.get_layer('extended')
        self.assertIsNotNone(ext)
        self.assertEqual(len(ext.modules), 3)

    def test_rewire_noc(self):
        noc = TopoLayer('mesh_noc')
        noc.add_module('router_0_0', 'RouterTLM')
        noc.add_module('router_0_1', 'RouterTLM')
        noc.add_connection('router_0_0', 'router_0_1', latency=1)
        self.orch.add_layer('mesh_noc', layers=[noc])
        self.orch.rewire_noc('mesh_noc', 'mesh')
        mesh_layer = self.orch.get_layer('mesh')
        self.assertIsNotNone(mesh_layer)

    def test_load_missing_file(self):
        with self.assertRaises(FileNotFoundError):
            self.orch.load('/nonexistent/path.json')

    def test_build_missing_variant(self):
        result = self.orch.build_variant('nonexistent')
        self.assertIsNone(result)


if __name__ == '__main__':
    unittest.main()