#!/usr/bin/env python3
"""test_topo_patch.py — TopoPatch TDD 测试

测试 TopoPatch、ConnectionSelector、ModuleSelector、LayerSelector 的完整功能。
"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo.layer import TopoLayer, ConnectionSpec, ModuleSpec
from cpptlm.topo.patch import (
    TopoPatch, ConnectionSelector, ModuleSelector, LayerSelector, PatchAction
)


class TestTopoPatch(unittest.TestCase):

    def setUp(self):
        self.layer = TopoLayer('test')
        self.layer.add_module('cpu0', 'CPUTLM')
        self.layer.add_module('cpu1', 'CPUTLM')
        self.layer.add_module('cache', 'CacheTLM')
        self.layer.add_module('mem', 'MemoryTLM')
        self.layer.add_connection('cpu0', 'cache', latency=1)
        self.layer.add_connection('cpu1', 'cache', latency=1)
        self.layer.add_connection('cache', 'mem', latency=10)

    def test_connection_selector_match_src(self):
        sel = ConnectionSelector('cpu*')
        conn = ConnectionSpec(src='cpu0', dst='cache')
        self.assertTrue(sel.match(conn))

    def test_connection_selector_match_dst(self):
        sel = ConnectionSelector('cache')
        conn = ConnectionSpec(src='cpu0', dst='cache')
        self.assertTrue(sel.match(conn))

    def test_connection_selector_glob_pattern(self):
        sel = ConnectionSelector('cpu*.cache')
        conn1 = ConnectionSpec(src='cpu0.cache', dst='xbar')
        conn2 = ConnectionSpec(src='gpu0.cache', dst='xbar')
        self.assertTrue(sel.match(conn1))
        self.assertFalse(sel.match(conn2))

    def test_connection_selector_no_match(self):
        sel = ConnectionSelector('xyz*')
        conn = ConnectionSpec(src='cpu0', dst='cache')
        self.assertFalse(sel.match(conn))

    def test_module_selector_match(self):
        sel = ModuleSelector('cpu*')
        mod = ModuleSpec(name='cpu0', type='CPUTLM')
        self.assertTrue(sel.match(mod))

    def test_module_selector_no_match(self):
        sel = ModuleSelector('gpu*')
        mod = ModuleSpec(name='cpu0', type='CPUTLM')
        self.assertFalse(sel.match(mod))

    def test_layer_selector_by_name(self):
        sel = LayerSelector(name_pattern='test')
        layer = TopoLayer('test')
        self.assertTrue(sel.match(layer))

    def test_topopatch_remove_connection(self):
        patch = TopoPatch(ConnectionSelector('cpu*'), PatchAction.REMOVE)
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(len(self.layer.connections), 1)
        self.assertEqual(self.layer.connections[0].dst, 'mem')

    def test_topopatch_remove_module(self):
        patch = TopoPatch(ModuleSelector('cpu*'), PatchAction.REMOVE)
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(len(self.layer.modules), 2)
        self.assertFalse(any(m.name.startswith('cpu') for m in self.layer.modules))

    def test_topopatch_add_connection(self):
        patch = TopoPatch(ConnectionSelector('*'), PatchAction.ADD,
                         template={'src': 'gpu0', 'dst': 'cache', 'latency': 2})
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(len(self.layer.connections), 4)

    def test_topopatch_replace_connection(self):
        patch = TopoPatch(ConnectionSelector('cache.mem'),
                         PatchAction.REPLACE,
                         template={'src': 'cache', 'dst': 'l2_cache', 'latency': 5})
        self.layer = patch.apply_to(self.layer)
        cache_to_mem = [c for c in self.layer.connections
                       if c.src == 'cache' and c.dst == 'l2_cache']
        self.assertEqual(len(cache_to_mem), 1)
        self.assertEqual(cache_to_mem[0].latency, 5)

    def test_topopatch_rewire_single_end(self):
        patch = TopoPatch(ConnectionSelector('cpu0'), PatchAction.REWIRE,
                         rewiring={'dst': 'l2_cache'})
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(self.layer.connections[0].dst, 'l2_cache')

    def test_topopatch_rewire_both_ends(self):
        patch = TopoPatch(ConnectionSelector('cpu0'), PatchAction.REWIRE,
                         rewiring={'src': 'cpu_new', 'dst': 'l2_new'})
        self.layer = patch.apply_to(self.layer)
        conn = self.layer.connections[0]
        self.assertEqual(conn.src, 'cpu_new')
        self.assertEqual(conn.dst, 'l2_new')

    def test_topopatch_add_to_sublayer(self):
        sub = TopoLayer('cluster')
        sub.add_connection('a', 'b', latency=1)
        self.layer.add_sublayer(sub)
        patch = TopoPatch(LayerSelector(name_pattern='cluster'), PatchAction.ADD,
                         template={'name': 'added', 'modules': [], 'connections': []})
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(len(self.layer.sublayers), 2)

    def test_topopatch_remove_from_sublayer(self):
        sub = TopoLayer('cluster')
        sub.add_module('cpu', 'CPUTLM')
        sub.add_module('gpu', 'GPUTLM')
        self.layer.add_sublayer(sub)
        patch = TopoPatch(LayerSelector(name_pattern='cluster'), PatchAction.REMOVE)
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(len(self.layer.sublayers), 0)

    def test_topopatch_no_match_harmless(self):
        patch = TopoPatch(ConnectionSelector('nonexistent*'), PatchAction.REMOVE)
        original_conn_count = len(self.layer.connections)
        self.layer = patch.apply_to(self.layer)
        self.assertEqual(len(self.layer.connections), original_conn_count)


if __name__ == '__main__':
    unittest.main()