"""cpptlm/config/topologies.py — High-level topology builders.

DEPRECATED (2026-06-17): scripts.topology_generator backend 已不存在, 此文件保留
向后兼容但所有 TopologyGenerator 调用现在会发出 ImportError + 警告.
新代码请用 cpptlm.library.{mesh_cluster, ring_cluster, crossbar_cluster}.
"""

from __future__ import annotations

from typing import Optional
import sys
import os
import warnings

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec
from cpptlm_config.types import ModuleType

_warned = False
def _warn_legacy():
    global _warned
    if not _warned:
        warnings.warn(
            "cpptlm.config.topologies is deprecated; use cpptlm.library "
            "(mesh_cluster, ring_cluster, crossbar_cluster) instead.",
            DeprecationWarning, stacklevel=3,
        )
        _warned = True


class MeshTopology:
    """2D Mesh topology builder."""

    def __init__(self, rows: int = 2, cols: int = 2):
        self.rows = rows
        self.cols = cols
        self._builder: Optional[ConfigBuilder] = None

    def build(self) -> ConfigBuilder:
        _warn_legacy()
        try:
            from scripts.topology_generator import TopologyGenerator
        except ImportError as e:
            raise ImportError(f"topology_generator.py not found: {e}")

        gen = TopologyGenerator(name=f"mesh_{self.rows}x{self.cols}")
        gen.generate_mesh(self.rows, self.cols)

        self._builder = ConfigBuilder(name=f"mesh_{self.rows}x{self.cols}")

        for node, attrs in gen.graph.nodes(data=True):
            self._builder.add_module(ModuleSpec(
                name=node,
                type=attrs.get("type", "RouterTLM"),
                params=attrs.get("params", {})
            ))

        for src, dst, attrs in gen.graph.edges(data=True):
            self._builder.add_connection(ConnectionSpec(
                src=src,
                dst=dst,
                latency=attrs.get("latency", 0)
            ))

        return self._builder


class RingTopology:
    """Ring topology builder."""

    def __init__(self, nodes: int = 4):
        self.nodes = nodes
        self._builder: Optional[ConfigBuilder] = None

    def build(self) -> ConfigBuilder:
        try:
            from scripts.topology_generator import TopologyGenerator
        except ImportError as e:
            raise ImportError(f"topology_generator.py not found: {e}")

        gen = TopologyGenerator(name=f"ring_{self.nodes}")
        gen.generate_ring(self.nodes)

        self._builder = ConfigBuilder(name=f"ring_{self.nodes}")

        for node, attrs in gen.graph.nodes(data=True):
            self._builder.add_module(ModuleSpec(
                name=node,
                type=attrs.get("type", "RouterTLM"),
                params=attrs.get("params", {})
            ))

        for src, dst, attrs in gen.graph.edges(data=True):
            self._builder.add_connection(ConnectionSpec(
                src=src,
                dst=dst,
                latency=attrs.get("latency", 0)
            ))

        return self._builder


class CrossbarTopology:
    """Crossbar topology builder."""

    def __init__(self, ports: int = 4):
        self.ports = ports
        self._builder: Optional[ConfigBuilder] = None

    def build(self) -> ConfigBuilder:
        self._builder = ConfigBuilder(name=f"crossbar_{self.ports}")

        for i in range(self.ports):
            self._builder.add_module(ModuleSpec(
                name=f"cpu_{i}",
                type="CPUTLM",
                params={}
            ))

        self._builder.add_module(ModuleSpec(
            name="xbar",
            type="CrossbarTLM",
            params={}
        ))

        for i in range(self.ports):
            self._builder.add_connection(ConnectionSpec(
                src=f"cpu_{i}",
                dst=f"xbar.{i}",
                latency=0
            ))

        self._builder.add_connection(ConnectionSpec(
            src="xbar.0",
            dst="mem",
            latency=1
        ))

        return self._builder