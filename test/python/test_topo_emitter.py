#!/usr/bin/env python3
"""test_topo_emitter.py — CxxCompatibleEmitter TDD 测试

验证 CxxCompatibleEmitter 正确将 TopoLayer 树展开为 C++ ModuleFactory 可加载的 JSON.
覆盖: 白名单/保留名/tag 展开/嵌套 hierarchy/layout 注入/placement/hierarchy 绑定.
"""
import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo.layer import TopoLayer, ModuleSpec, ConnectionSpec
from cpptlm.topo.emitter import CxxCompatibleEmitter, TopoEmitError


class TestEmitterBasic(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_emit_minimal_layer(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        out = self.emitter.emit(l)
        self.assertEqual(out['name'], 'test')
        self.assertEqual(len(out['modules']), 1)
        self.assertEqual(out['modules'][0]['name'], 'cpu0')
        self.assertEqual(out['modules'][0]['type'], 'CPUTLM')
        self.assertEqual(out['connections'], [])

    def test_emit_with_params(self):
        l = TopoLayer('test')
        l.add_module('l1', 'CacheTLM', size='32KB', associativity=4)
        out = self.emitter.emit(l)
        self.assertEqual(out['modules'][0]['params'], {'size': '32KB', 'associativity': 4})

    def test_emit_with_connection(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        l.add_module('l1', 'CacheTLM')
        l.add_connection('cpu0', 'l1', latency=2)
        out = self.emitter.emit(l)
        self.assertEqual(len(out['connections']), 1)
        self.assertEqual(out['connections'][0]['src'], 'cpu0')
        self.assertEqual(out['connections'][0]['dst'], 'l1')
        self.assertEqual(out['connections'][0]['latency'], 2)


class TestEmitterReservedNames(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_reserved_module_name_raises(self):
        l = TopoLayer('test')
        l.add_module('groups', 'CPUTLM')
        with self.assertRaises(TopoEmitError) as ctx:
            self.emitter.emit(l)
        self.assertIn("'groups'", str(ctx.exception))
        self.assertIn('reserved', str(ctx.exception))

    def test_reserved_name_modules_raises(self):
        l = TopoLayer('test')
        l.add_module('modules', 'CPUTLM')
        with self.assertRaises(TopoEmitError):
            self.emitter.emit(l)

    def test_reserved_name_hierarchy_raises(self):
        l = TopoLayer('test')
        l.add_module('hierarchy', 'CPUTLM')
        with self.assertRaises(TopoEmitError):
            self.emitter.emit(l)


class TestEmitterGroups(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_tag_expands_to_groups(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        l.add_module('cpu1', 'CPUTLM')
        l.tag('compute')
        out = self.emitter.emit(l)
        self.assertIn('compute', out['groups'])
        self.assertEqual(sorted(out['groups']['compute']), ['cpu0', 'cpu1'])

    def test_multiple_tags(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        l.add_module('l1', 'CacheTLM')
        l.tag('compute', 'shared')
        out = self.emitter.emit(l)
        self.assertIn('compute', out['groups'])
        self.assertIn('shared', out['groups'])

    def test_tags_across_sublayers_merged(self):
        root = TopoLayer('root')
        sub = TopoLayer('sub')
        sub.add_module('cpu0', 'CPUTLM')
        root.add_sublayer(sub)
        root.tag('all')
        sub.tag('all')
        out = self.emitter.emit(root)
        self.assertIn('cpu0', out['groups']['all'])


class TestEmitterHierarchy(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_no_sublayers_no_hierarchy(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        out = self.emitter.emit(l)
        self.assertNotIn('hierarchy', out)

    def test_flat_hierarchy(self):
        l = TopoLayer('root')
        l.add_module('cpu0', 'CPUTLM')
        sub = TopoLayer('cluster0')
        sub.add_module('cpu1', 'CPUTLM')
        l.add_sublayer(sub)
        out = self.emitter.emit(l)
        self.assertIn('hierarchy', out)
        self.assertEqual(out['hierarchy']['name'], 'root')
        self.assertEqual(len(out['hierarchy']['children']), 1)
        self.assertEqual(out['hierarchy']['children'][0]['name'], 'cluster0')
        self.assertEqual(out['hierarchy']['children'][0]['children'], [])

    def test_three_level_hierarchy(self):
        root = TopoLayer('root')
        mid = TopoLayer('mid')
        leaf = TopoLayer('leaf')
        leaf.add_module('cpu0', 'CPUTLM')
        mid.add_sublayer(leaf)
        root.add_sublayer(mid)
        out = self.emitter.emit(root)
        h = out['hierarchy']
        self.assertEqual(h['name'], 'root')
        self.assertEqual(h['children'][0]['name'], 'mid')
        self.assertEqual(h['children'][0]['children'][0]['name'], 'leaf')


class TestEmitterLayout(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_layout_in_metadata_propagates(self):
        l = TopoLayer('test')
        m = ModuleSpec(name='cpu0', type='CPUTLM')
        m.metadata = {'layout': {'x': 100, 'y': 200}}
        l.add_modules(m)
        out = self.emitter.emit(l)
        self.assertEqual(out['modules'][0]['layout'], {'x': 100, 'y': 200})

    def test_no_layout_no_field(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        out = self.emitter.emit(l)
        self.assertNotIn('layout', out['modules'][0])


class TestEmitterValidation(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_hierarchy_binding_valid_with_submodule(self):
        root = TopoLayer('root')
        sub = TopoLayer('cluster0')
        sub.add_module('cpu0', 'CPUTLM')
        root.add_sublayer(sub)
        out = self.emitter.emit(root)
        self.assertEqual(out['hierarchy']['name'], 'root')
        self.assertEqual(out['hierarchy']['children'][0]['name'], 'cluster0')

    def test_hierarchy_binding_valid_with_empty_sublayer(self):
        root = TopoLayer('root')
        sub = TopoLayer('phantom')
        root.add_sublayer(sub)
        out = self.emitter.emit(root)
        self.assertEqual(out['hierarchy']['children'][0]['name'], 'phantom')


class TestEmitterCoherence(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_coherence_passthrough(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        l.add_coherence_domain('d0', 'MESI', ['cpu0'])
        out = self.emitter.emit(l)
        self.assertIn('coherence_domains', out)
        self.assertEqual(out['coherence_domains'][0]['name'], 'd0')
        self.assertEqual(out['coherence_domains'][0]['protocol'], 'MESI')
        self.assertEqual(out['coherence_domains'][0]['members'], ['cpu0'])

    def test_no_coherence_no_field(self):
        l = TopoLayer('test')
        l.add_module('cpu0', 'CPUTLM')
        out = self.emitter.emit(l)
        self.assertNotIn('coherence_domains', out)


class TestEmitterConnectionFeatures(unittest.TestCase):

    def setUp(self):
        self.emitter = CxxCompatibleEmitter()

    def test_connection_with_bandwidth(self):
        l = TopoLayer('test')
        l.add_module('a', 'CPUTLM')
        l.add_module('b', 'CPUTLM')
        c = ConnectionSpec(src='a', dst='b', latency=1, bandwidth=100)
        l.connections.append(c)
        out = self.emitter.emit(l)
        self.assertEqual(out['connections'][0]['bandwidth'], 100)

    def test_connection_with_vc_priorities(self):
        l = TopoLayer('test')
        l.add_module('a', 'CPUTLM')
        l.add_module('b', 'CPUTLM')
        c = ConnectionSpec(src='a', dst='b', latency=1, vc_priorities=[0, 1])
        l.connections.append(c)
        out = self.emitter.emit(l)
        self.assertEqual(out['connections'][0]['vc_priorities'], [0, 1])


if __name__ == '__main__':
    unittest.main()
