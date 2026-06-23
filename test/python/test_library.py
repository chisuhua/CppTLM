#!/usr/bin/env python3
"""test_library.py — cpptlm/library cluster factory + SoC orchestrator tests

覆盖: cluster 工厂结构 / SoC 多 cluster 组合 / connect_group 展开 / save 写盘 /
保留名检查.
"""
import unittest
import sys
import os
import json
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm.library import (
    cpu_l1_cluster,
    memory_cluster,
    crossbar_cluster,
    cpu_nested_cluster,
    memory_cluster_hierarchical,
    gpu_topology,
    mesh_cluster,
    ring_cluster,
    SoC,
)
from cpptlm.topo.emitter import CxxCompatibleEmitter, TopoEmitError


class TestStandardClusters(unittest.TestCase):

    def test_cpu_l1_cluster_default(self):
        l = cpu_l1_cluster(idx=0, n_cores=2)
        self.assertEqual(l.name, 'cluster0')
        names = sorted(m.name for m in l.modules)
        self.assertEqual(names, ['cluster0_cpu0', 'cluster0_cpu1', 'cluster0_l1'])
        self.assertEqual(len(l.connections), 2)
        for c in l.connections:
            self.assertEqual(c.dst, 'cluster0_l1')

    def test_cpu_l1_cluster_more_cores(self):
        l = cpu_l1_cluster(idx=1, n_cores=4)
        names = sorted(m.name for m in l.modules)
        self.assertEqual(len(names), 5)
        self.assertIn('cluster1_l1', names)
        self.assertEqual(len(l.connections), 4)

    def test_memory_cluster(self):
        l = memory_cluster(name='mem_bank', n_banks=2)
        self.assertEqual(l.name, 'mem_bank')
        names = sorted(m.name for m in l.modules)
        self.assertEqual(names, ['mem_bank_mem0', 'mem_bank_mem1'])

    def test_crossbar_cluster(self):
        l = crossbar_cluster(n_ports=8, name='xbar8')
        names = [m.name for m in l.modules]
        self.assertEqual(names, ['xbar8'])
        self.assertEqual(l.metadata['port_count'], 8)


class TestInterconnectClusters(unittest.TestCase):

    def test_mesh_cluster_basic(self):
        l = mesh_cluster(rows=2, cols=2, name_prefix='noc')
        names = sorted(m.name for m in l.modules)
        self.assertEqual(len(names), 4)
        self.assertIn('noc_router_0_0', names)
        self.assertIn('noc_router_1_1', names)
        self.assertEqual(len(l.connections), 4)

    def test_mesh_cluster_nonsquare(self):
        l = mesh_cluster(rows=3, cols=2, name_prefix='m')
        self.assertEqual(len(l.modules), 6)
        self.assertEqual(len(l.connections), 7)

    def test_ring_cluster(self):
        l = ring_cluster(n_nodes=4, name_prefix='r')
        self.assertEqual(len(l.modules), 4)
        self.assertEqual(len(l.connections), 4)
        self.assertIn('r_node_0', [m.name for m in l.modules])
        self.assertIn('r_node_3', [m.name for m in l.modules])

    def test_ring_cluster_minimum(self):
        with self.assertRaises(ValueError):
            ring_cluster(n_nodes=1)


class TestSoCOrchestrator(unittest.TestCase):

    def test_soc_single_cluster(self):
        soc = SoC('test_soc')
        soc.add_cluster(cpu_l1_cluster(0))
        self.assertEqual(len(soc._root.sublayers), 1)

    def test_soc_multi_cluster_with_tag(self):
        soc = SoC('test_soc')
        soc.add_cluster(cpu_l1_cluster(0)).tag('compute')
        soc.add_cluster(cpu_l1_cluster(1))
        out = CxxCompatibleEmitter().emit(soc._root)
        self.assertIn('compute', out['groups'])
        self.assertIn('cluster0_cpu0', out['groups']['compute'])
        self.assertNotIn('cluster1_cpu0', out['groups']['compute'])

    def test_soc_connect_group_emits_group_prefix(self):
        soc = SoC('test_soc')
        soc.add_cluster(cpu_l1_cluster(0)).tag('compute')
        soc.add_module('xbar', 'CrossbarTLM')
        soc.connect_group('compute', 'xbar.0', latency=5)
        self.assertEqual(len(soc._root.connections), 1)
        self.assertEqual(soc._root.connections[0].src, 'group:compute')
        self.assertEqual(soc._root.connections[0].dst, 'xbar.0')
        self.assertEqual(soc._root.connections[0].latency, 5)

    def test_soc_layout_grid_chains(self):
        soc = SoC('test')
        soc.add_cluster(cpu_l1_cluster(0))
        result = soc.layout_grid(dx=2, dy=2, x_offset=100, y_offset=100)
        self.assertIs(result, soc)
        for m in soc._root._all_modules():
            self.assertIn('layout', m.metadata)

    def test_soc_save_produces_valid_json(self):
        soc = SoC('test_soc', description='demo')
        soc.add_cluster(cpu_l1_cluster(0).layout_grid(dx=3, dy=1))
        soc.add_module('xbar', 'CrossbarTLM')
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'test_soc.json')
            soc.save(path)
            with open(path) as f:
                data = json.load(f)
            self.assertEqual(data['name'], 'test_soc')
            self.assertEqual(data['description'], 'demo')
            self.assertIn('hierarchy', data)
            self.assertEqual(data['hierarchy']['children'][0]['name'], 'cluster0')


class TestReservedNames(unittest.TestCase):

    def test_explicit_reserved_name_in_factory_raises(self):
        from cpptlm.library.standard import _check_reserved
        with self.assertRaises(ValueError):
            _check_reserved(['cpu0', 'groups'])
        with self.assertRaises(ValueError):
            _check_reserved(['modules'])
        with self.assertRaises(ValueError):
            _check_reserved(['hierarchy'])

    def test_soc_reserved_module_name_via_emitter(self):
        from cpptlm.topo.emitter import TopoEmitError
        soc = SoC('test')
        soc.add_module('groups', 'CPUTLM')
        with self.assertRaises(TopoEmitError):
            CxxCompatibleEmitter().emit(soc._root)


class TestCpuNestedCluster(unittest.TestCase):

    def test_default_2core(self):
        l = cpu_nested_cluster()
        self.assertEqual(l.name, 'cpu_nested_cpu')
        module_names = [m.name for m in l.modules]
        self.assertEqual(module_names, ['cpu'])
        cpu_mod = l.modules[0]
        self.assertEqual(cpu_mod.type, 'CpuCluster')
        self.assertEqual(cpu_mod.params['num_cpus'], 2)

    def test_4core_with_l1_l2(self):
        l = cpu_nested_cluster(num_cores=4, l1_count=2, l1_size="32KB", l2_size="1MB")
        cpu_mod = l.modules[0]
        self.assertEqual(cpu_mod.params['num_cpus'], 4)
        self.assertGreater(len(l.sublayers), 0)
        inner = l.sublayers[0]
        inner_modules = [m.name for m in inner.modules]
        self.assertEqual(len(inner_modules), 4)
        self.assertIn('cpu0_cpu0', inner_modules)
        self.assertIn('cpu0_cpu3', inner_modules)
        self.assertEqual(len(inner.sublayers), 1)
        l2cache = inner.sublayers[0]
        self.assertIn('l2cache', l2cache.name)
        self.assertEqual(l2cache.modules[0].params['l1_count'], 2)
        self.assertEqual(l2cache.modules[0].params['l1_size'], '32KB')
        self.assertEqual(l2cache.modules[0].params['l2_size'], '1MB')

    def test_inner_connections_route_to_l1(self):
        l = cpu_nested_cluster(num_cores=2, l1_count=2)
        inner = l.sublayers[0]
        conns = inner.connections
        self.assertEqual(len(conns), 2)
        dsts = sorted([c.dst for c in conns])
        self.assertEqual(dsts, ['cpu0_l2cache.l1_0', 'cpu0_l2cache.l1_1'])


class TestMemoryClusterHierarchical(unittest.TestCase):

    def test_4_channels(self):
        l = memory_cluster_hierarchical(channels=4)
        self.assertEqual(l.name, 'memory_hier_mem')
        self.assertEqual(len(l.modules), 1)
        mem_mod = l.modules[0]
        self.assertEqual(mem_mod.type, 'MemoryCluster')
        self.assertEqual(mem_mod.params['channel_count'], 4)

    def test_inner_channels_and_arbiter(self):
        l = memory_cluster_hierarchical(channels=4, channel_size="2GB", memory_type="DDR4")
        inner = l.sublayers[0]
        inner_names = sorted([m.name for m in inner.modules])
        # 4 channels + 1 arbiter
        self.assertEqual(len(inner_names), 5)
        self.assertIn('mem_channel0', inner_names)
        self.assertIn('mem_channel3', inner_names)
        self.assertIn('mem_arbiter', inner_names)
        for m in inner.modules:
            if 'channel' in m.name:
                self.assertEqual(m.params['size'], '2GB')


class TestGpuTopology(unittest.TestCase):

    def test_2gpc_2tpc_2cu_default(self):
        l = gpu_topology()
        gpu_mod = l.modules[0]
        self.assertEqual(gpu_mod.type, 'GpuCluster')
        self.assertEqual(gpu_mod.params['gpc_count'], 2)
        self.assertEqual(gpu_mod.params['tpc_per_gpc'], 2)
        self.assertEqual(gpu_mod.params['cu_per_tpc'], 2)
        # 4-level: gpu → 2×gpc → 2×tpc per gpc → compute_grp
        self.assertEqual(len(l.sublayers), 1)
        inner = l.sublayers[0]
        self.assertEqual(len(inner.sublayers), 2)
        gpc0 = inner.sublayers[0]
        self.assertIn('gpc0', gpc0.name)
        self.assertEqual(len(gpc0.sublayers), 2)

    def test_cu_template_passthrough(self):
        l = gpu_topology(cu_template="configs/templates/custom_cu.json")
        gpu_mod = l.modules[0]
        self.assertEqual(gpu_mod.params['cu_template'],
                         'configs/templates/custom_cu.json')
        inner = l.sublayers[0]
        gpc0 = inner.sublayers[0]
        self.assertEqual(gpc0.modules[0].params['cu_template'],
                         'configs/templates/custom_cu.json')

    def test_4level_cu_count_8(self):
        # 2 gpc × 2 tpc/gpc × 2 cu/tpc = 8 CUs
        l = gpu_topology(gpc_count=2, tpc_per_gpc=2, cu_per_tpc=2)
        total_cus = 0
        for gpc in l.sublayers[0].sublayers:
            for tpc in gpc.sublayers:
                for compute_grp in tpc.sublayers:
                    total_cus += compute_grp.modules[0].params['cu_count']
        self.assertEqual(total_cus, 8)


if __name__ == '__main__':
    unittest.main()
