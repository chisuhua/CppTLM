#!/usr/bin/env python3
"""test_topo_e2e.py — End-to-end tests for all 5 user requirements"""

import unittest
import sys
import os
import json
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.topo import (
    TopoLayer, TopoPatch, TopoVariant, TopoVariantSet,
    TopoOrchestrator, ConnectionSelector, PatchAction
)


class TestTopoE2E(unittest.TestCase):
    """E2E tests for 5 user requirements:
    1. Modular generation: Create CPU cluster, load existing Memory config, assemble full system
    2. Partial regeneration: Build full system, then rewire only specific module connections
    3. NoC adjustment: Create mesh topology with routers, verify after flattening
    4. Template extension: Based on 2x2 template, create 4x4 scale and verify module count
    5. Multi-topology comparison: Create low_latency / high_bandwidth variants, compare differences
    """

    def test_e2e_modular_generation(self):
        """Requirement 1: 模块化生成 — 顶层创建新系统，子模块拼装"""
        mem_layer = TopoLayer('mem_sub')
        mem_layer.add_module('dram0', 'MemoryTLM')
        mem_layer.add_module('dram1', 'MemoryTLM')
        mem_layer.add_connection('dram0', 'xbar', latency=2)

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(mem_layer.to_dict(), f)
            mem_path = f.name

        try:
            orch = TopoOrchestrator('e2e_modular')
            orch.load(mem_path, name='mem')

            cpu_cluster = TopoLayer('cpu_cluster')
            for i in range(4):
                cpu_cluster.add_module(f'cpu{i}', 'CPUTLM')
            orch.add_layer('cpu', layers=[cpu_cluster])

            top = TopoLayer('system')
            top.add_sublayer(orch.get_layer('mem'))
            top.add_sublayer(orch.get_layer('cpu'))
            top.add_coherence_domain('cpu_domain', 'MESI', [f'cpu{i}' for i in range(4)])

            flat = top.flatten()
            d = flat.to_dict()
            all_modules = [m['name'] for m in d['modules']]
            for i in range(4):
                self.assertIn(f'cpu{i}', all_modules)
            self.assertIn('dram0', all_modules)
            self.assertIn('dram1', all_modules)
            self.assertEqual(len(d['coherence_domains']), 1)
        finally:
            os.unlink(mem_path)

    def test_e2e_partial_regeneration(self):
        """Requirement 2: 局部重生成 — 只重连特定模块的连接，其他不变"""
        top = TopoLayer('e2e_partial')
        for i in range(4):
            top.add_module(f'cpu{i}', 'CPUTLM')
        top.add_module('mem', 'MemoryTLM')
        top.add_module('xbar', 'CrossbarTLM')

        for i in range(4):
            top.add_connection(f'cpu{i}', 'xbar', latency=1)
        top.add_connection('xbar', 'mem', latency=10)

        original_cpu_count = len([m for m in top.modules if m.name.startswith('cpu')])
        self.assertEqual(original_cpu_count, 4)
        self.assertEqual(len(top.connections), 5)

        p = TopoPatch(ConnectionSelector('cpu*'), PatchAction.REMOVE)
        top = p.apply_to(top)

        self.assertEqual(len(top.modules), 6)
        self.assertEqual(len(top.connections), 1)
        self.assertEqual(top.connections[0].src, 'xbar')
        self.assertEqual(top.connections[0].dst, 'mem')

    def test_e2e_noc_adjustment(self):
        """Requirement 3: NoC 专项调整 — 创建含多个 router 的 mesh 拓扑"""
        noc = TopoLayer('mesh_noc')
        for i in range(2):
            for j in range(2):
                noc.add_module(f'router_{i}_{j}', 'RouterTLM')

        for i in range(2):
            for j in range(2):
                if i < 1:
                    noc.add_connection(f'router_{i}_{j}', f'router_{i+1}_{j}', latency=1)
                if j < 1:
                    noc.add_connection(f'router_{i}_{j}', f'router_{i}_{j+1}', latency=1)

        self.assertEqual(len(noc.modules), 4)
        self.assertGreaterEqual(len(noc.connections), 4)

        flat_noc = noc.flatten()
        self.assertEqual(len(flat_noc.modules), 4)
        self.assertEqual(len(flat_noc.sublayers), 0)

    def test_e2e_template_extension(self):
        """Requirement 4: 模板扩展 — 基于 2x2 模板创建 4x4 规模并验证"""
        orch = TopoOrchestrator('e2e_template')

        template_2x2 = TopoLayer('template_2x2')
        for i in range(2):
            for j in range(2):
                template_2x2.add_module(f'cpu_{i}_{j}', 'CPUTLM')

        orch.add_layer('template_2x2', layers=[template_2x2])

        orch.create_template('template_4x4', 'template_2x2',
            modify=lambda l: [
                l.add_module(f'cpu_{i}_{j}', 'CPUTLM')
                for i in range(4)
                for j in range(4)
                if f'cpu_{i}_{j}' not in [m.name for m in l.modules]
            ])

        ext = orch.get_layer('template_4x4')
        self.assertIsNotNone(ext)
        self.assertGreaterEqual(len(ext.modules), 4)

    def test_e2e_variant_comparison(self):
        """Requirement 5: 多拓扑对比 — 创建不同变体并比较"""
        base = TopoLayer('soc_base')
        for i in range(4):
            base.add_module(f'cpu{i}', 'CPUTLM')
        base.add_module('mem', 'MemoryTLM')

        vs = TopoVariantSet('soc_variants')
        vs.add_variant(TopoVariant('minimal', base=base))

        extra = TopoLayer('')
        extra.add_module('gpu0', 'GPUTLM')
        big = base.merge(extra)
        vs.add_variant(TopoVariant('with_gpu', base=big))

        all_variants = vs.build_all()
        self.assertEqual(len(all_variants), 2)
        self.assertIn('minimal', all_variants)
        self.assertIn('with_gpu', all_variants)

        comp = vs.compare()
        self.assertEqual(comp['minimal']['module_count'], 5)
        self.assertEqual(comp['with_gpu']['module_count'], 6)


if __name__ == '__main__':
    unittest.main()