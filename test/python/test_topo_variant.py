#!/usr/bin/env python3
"""test_topo_variant.py — TopoVariant TDD 测试"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo.layer import TopoLayer
from cpptlm.topo.variant import TopoVariant, TopoVariantSet
from cpptlm.topo.patch import TopoPatch, ConnectionSelector, ModuleSelector, PatchAction


class TestTopoVariant(unittest.TestCase):

    def setUp(self):
        self.base = TopoLayer('base')
        self.base.add_module('cpu0', 'CPUTLM')
        self.base.add_connection('cpu0', 'mem', latency=1)
        self.base.add_module('mem', 'MemoryTLM')

    def test_variant_construction(self):
        v = TopoVariant('test', base=self.base)
        self.assertEqual(v.name, 'test')
        self.assertEqual(v.base.name, 'base')

    def test_variant_build_creates_copy(self):
        v = TopoVariant('v1', base=self.base)
        result = v.build()
        self.assertEqual(result.name, 'base_v1')
        self.assertEqual(len(result.modules), len(self.base.modules))

    def test_variant_build_applies_patches(self):
        v = TopoVariant('low_latency', base=self.base,
                        patches=[TopoPatch(ConnectionSelector('cpu0'),
                                          PatchAction.REWIRE,
                                          rewiring={'dst': 'l2_cache'})])
        result = v.build()
        self.assertEqual(result.connections[0].dst, 'l2_cache')

    def test_variant_build_preserves_unaffected(self):
        v = TopoVariant('v1', base=self.base)
        result = v.build()
        self.assertEqual(len(result.modules), len(self.base.modules))
        self.assertEqual(len(result.connections), len(self.base.connections))

    def test_variant_name_suffix_format(self):
        v = TopoVariant('variant_a', base=self.base)
        result = v.build()
        self.assertEqual(result.name, 'base_variant_a')

    def test_variant_set_construction(self):
        vs = TopoVariantSet('test_set')
        self.assertEqual(vs.name, 'test_set')
        self.assertEqual(len(vs.variants), 0)

    def test_variant_set_add_variant(self):
        vs = TopoVariantSet('test_set')
        vs.add_variant(TopoVariant('v1', base=self.base))
        self.assertEqual(len(vs.variants), 1)

    def test_variant_set_build_all(self):
        vs = TopoVariantSet('test')
        vs.add_variant(TopoVariant('v1', base=self.base))
        vs.add_variant(TopoVariant('v2', base=self.base))
        all_variants = vs.build_all()
        self.assertEqual(len(all_variants), 2)
        self.assertIn('v1', all_variants)
        self.assertIn('v2', all_variants)

    def test_variant_set_compare(self):
        vs = TopoVariantSet('test')
        vs.add_variant(TopoVariant('minimal', base=self.base))
        comp = vs.compare()
        self.assertEqual(comp['minimal']['module_count'], 2)

    def test_variant_empty_patches(self):
        v = TopoVariant('same', base=self.base, patches=[])
        result = v.build()
        self.assertEqual(len(result.modules), len(self.base.modules))
        self.assertEqual(len(result.connections), len(self.base.connections))

    def test_variant_multiple_variants_unique_names(self):
        vs = TopoVariantSet('multi')
        vs.add_variant(TopoVariant('a', base=self.base))
        vs.add_variant(TopoVariant('b', base=self.base))
        all_v = vs.build_all()
        self.assertEqual(len(all_v), 2)


if __name__ == '__main__':
    unittest.main()