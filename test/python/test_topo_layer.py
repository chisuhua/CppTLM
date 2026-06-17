#!/usr/bin/env python3
"""test_topo_layer.py — TopoLayer TDD 测试

测试 TopoLayer、ModuleSpec、ConnectionSpec、CoherenceDomainSpec 的完整功能。
"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo.layer import (
    TopoLayer, ModuleSpec, ConnectionSpec, CoherenceDomainSpec
)


class TestTopoLayer(unittest.TestCase):

    def setUp(self):
        pass

    def test_layer_creation(self):
        l = TopoLayer('test')
        self.assertEqual(l.name, 'test')
        self.assertEqual(len(l.modules), 0)
        self.assertEqual(len(l.connections), 0)
        self.assertEqual(len(l.sublayers), 0)
        self.assertEqual(len(l.coherence_domains), 0)

    def test_add_module(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        self.assertEqual(len(l.modules), 1)
        self.assertEqual(l.modules[0].name, 'cpu0')
        self.assertEqual(l.modules[0].type, 'CPUTLM')

    def test_add_modules(self):
        l = TopoLayer('test')
        l.add_modules(
            ModuleSpec(name='cpu0', type='CPUTLM'),
            ModuleSpec(name='mem', type='MemoryTLM')
        )
        self.assertEqual(len(l.modules), 2)

    def test_get_module(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        found = l.get_module('cpu0')
        self.assertIsNotNone(found)
        self.assertEqual(found.name, 'cpu0')

    def test_get_module_not_found(self):
        l = TopoLayer('test')
        found = l.get_module('nonexistent')
        self.assertIsNone(found)

    def test_remove_module(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        l.add_connection('cpu0', 'mem', latency=1)
        l.remove_module('cpu0')
        self.assertEqual(len(l.modules), 0)
        self.assertEqual(len(l.connections), 0)

    def test_add_connection(self):
        l = TopoLayer('test')
        l.add_connection('cpu0', 'mem', latency=10)
        self.assertEqual(len(l.connections), 1)
        self.assertEqual(l.connections[0].src, 'cpu0')
        self.assertEqual(l.connections[0].dst, 'mem')
        self.assertEqual(l.connections[0].latency, 10)

    def test_remove_connection(self):
        l = TopoLayer('test')
        l.add_connection('cpu0', 'mem', latency=1)
        l.remove_connection('cpu0', 'mem')
        self.assertEqual(len(l.connections), 0)

    def test_rewire_connection(self):
        l = TopoLayer('test')
        l.add_connection('cpu0', 'mem', latency=1)
        l.rewire('cpu0', 'mem', 'cpu1', 'l2_cache')
        self.assertEqual(l.connections[0].src, 'cpu1')
        self.assertEqual(l.connections[0].dst, 'l2_cache')

    def test_add_sublayer(self):
        sub = TopoLayer('cluster0')
        sub.add_module('cpu0', 'CPUTLM')
        l = TopoLayer('test')
        l.add_sublayer(sub)
        self.assertEqual(len(l.sublayers), 1)
        self.assertEqual(l.sublayers[0].name, 'cluster0')

    def test_get_sublayer(self):
        sub = TopoLayer('cluster0')
        l = TopoLayer('test')
        l.add_sublayer(sub)
        found = l.get_sublayer('cluster0')
        self.assertIsNotNone(found)
        self.assertEqual(found.name, 'cluster0')

    def test_add_coherence_domain(self):
        l = TopoLayer('test')
        l.add_coherence_domain('cpu_domain', 'MESI', ['cpu0', 'cpu1'])
        self.assertEqual(len(l.coherence_domains), 1)
        self.assertEqual(l.coherence_domains[0].name, 'cpu_domain')
        self.assertEqual(l.coherence_domains[0].protocol, 'MESI')
        self.assertEqual(len(l.coherence_domains[0].members), 2)

    def test_to_dict_basic(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        l.add_connection('cpu0', 'mem', latency=5)
        d = l.to_dict()
        self.assertEqual(d['name'], 'test')
        self.assertEqual(len(d['modules']), 1)
        self.assertEqual(len(d['connections']), 1)

    def test_from_dict(self):
        data = {
            'name': 'soc',
            'modules': [{'name': 'cpu0', 'type': 'CPUTLM'}],
            'connections': [{'src': 'cpu0', 'dst': 'mem', 'latency': 10}]
        }
        l = TopoLayer.from_dict(data)
        self.assertEqual(l.name, 'soc')
        self.assertEqual(len(l.modules), 1)
        self.assertEqual(len(l.connections), 1)

    def test_to_dict_roundtrip(self):
        l = TopoLayer('test')
        l.add_module('gpu', 'GPUTLM')
        l.add_connection('gpu', 'mem', latency=2)
        d1 = l.to_dict()
        l2 = TopoLayer.from_dict(d1)
        d2 = l2.to_dict()
        self.assertEqual(d1['name'], d2['name'])
        self.assertEqual(len(d1['modules']), len(d2['modules']))
        self.assertEqual(len(d1['connections']), len(d2['connections']))

    def test_flatten(self):
        sub = TopoLayer('cluster')
        sub.add_module('cpu0', 'CPUTLM')
        l = TopoLayer('parent')
        l.add_module('mem', 'MemoryTLM')
        l.add_sublayer(sub)
        flat = l.flatten()
        self.assertEqual(len(flat.modules), 2)
        self.assertEqual(len(flat.sublayers), 0)
        names = [m.name for m in flat.modules]
        self.assertIn('cpu0', names)
        self.assertIn('mem', names)

    def test_merge(self):
        l1 = TopoLayer('a')
        l1.add_module('cpu0', 'CPUTLM')
        l2 = TopoLayer('b')
        l2.add_module('gpu0', 'GPUTLM')
        merged = l1.merge(l2)
        self.assertEqual(len(merged.modules), 2)
        names = [m.name for m in merged.modules]
        self.assertIn('cpu0', names)
        self.assertIn('gpu0', names)

    def test_empty_layer(self):
        l = TopoLayer('empty')
        d = l.to_dict()
        self.assertEqual(d['name'], 'empty')
        self.assertEqual(len(d['modules']), 0)
        self.assertEqual(len(d['connections']), 0)

    def test_deep_nesting(self):
        l1 = TopoLayer('level1')
        l2 = TopoLayer('level2')
        l3 = TopoLayer('level3')
        l3.add_module('deep_cpu', 'CPUTLM')
        l2.add_sublayer(l3)
        l1.add_sublayer(l2)
        found = l1.get_module('deep_cpu')
        self.assertIsNotNone(found)
        self.assertEqual(found.name, 'deep_cpu')

    def test_tag_chainable(self):
        l = TopoLayer('test')
        result = l.tag('compute', 'shared')
        self.assertIs(result, l)
        self.assertEqual(l.tags, {'compute', 'shared'})

    def test_tag_accumulates(self):
        l = TopoLayer('test')
        l.tag('a').tag('a', 'b')
        self.assertEqual(l.tags, {'a', 'b'})

    def test_clone_without_prefix(self):
        l = TopoLayer('original')
        l.add_module('cpu0', 'CPUTLM')
        l.tag('compute')
        clone = l.clone()
        self.assertEqual(clone.name, 'original_clone')
        self.assertEqual(clone.modules[0].name, 'cpu0')
        clone.modules[0].params['new'] = 1
        self.assertNotIn('new', l.modules[0].params)
        clone.tags.add('modified')
        self.assertNotIn('modified', l.tags)

    def test_clone_with_prefix(self):
        l = TopoLayer('cluster0')
        l.add_module('cpu0', 'CPUTLM')
        l.add_module('l1', 'CacheTLM')
        l.add_connection('cpu0', 'l1', latency=1)
        clone = l.clone(prefix='cluster1')
        self.assertEqual(clone.name, 'cluster1')
        names = sorted(m.name for m in clone.modules)
        self.assertEqual(names, ['cluster1_cpu0', 'cluster1_l1'])
        for c in clone.connections:
            self.assertTrue(c.src.startswith('cluster1_'))
            self.assertTrue(c.dst.startswith('cluster1_'))

    def test_clone_preserves_sublayers(self):
        root = TopoLayer('root')
        sub = TopoLayer('sub')
        sub.add_module('cpu0', 'CPUTLM')
        root.add_sublayer(sub)
        clone = root.clone()
        self.assertEqual(len(clone.sublayers), 1)
        self.assertEqual(clone.sublayers[0].name, 'sub')
        self.assertEqual(len(clone.sublayers[0].modules), 1)
        clone.sublayers[0].modules[0].params['x'] = 1
        self.assertNotIn('x', sub.modules[0].params)

    def test_layout_grid_basic(self):
        l = TopoLayer('test')
        for i in range(4):
            l.add_module(f'cpu{i}', 'CPUTLM')
        l.layout_grid(dx=2, dy=2)
        coords = [m.metadata['layout'] for m in l.modules]
        self.assertEqual(coords[0], {'x': 0, 'y': 0})
        self.assertEqual(coords[1], {'x': 100, 'y': 0})
        self.assertEqual(coords[2], {'x': 0, 'y': 100})
        self.assertEqual(coords[3], {'x': 100, 'y': 100})

    def test_layout_grid_with_offsets(self):
        l = TopoLayer('test')
        l.add_module('a', 'CPUTLM')
        l.add_module('b', 'CPUTLM')
        l.layout_grid(dx=2, dy=1, x_offset=500, y_offset=300)
        coords = [m.metadata['layout'] for m in l.modules]
        self.assertEqual(coords[0], {'x': 500, 'y': 300})
        self.assertEqual(coords[1], {'x': 600, 'y': 300})

    def test_layout_grid_recursive(self):
        root = TopoLayer('root')
        sub_a = TopoLayer('sub_a')
        sub_a.add_module('a0', 'CPUTLM')
        sub_b = TopoLayer('sub_b')
        sub_b.add_module('b0', 'CPUTLM')
        root.add_sublayer(sub_a)
        root.add_sublayer(sub_b)
        root.layout_grid(dx=1, dy=1)
        self.assertIn('layout', sub_a.modules[0].metadata)
        self.assertIn('layout', sub_b.modules[0].metadata)


if __name__ == '__main__':
    unittest.main()