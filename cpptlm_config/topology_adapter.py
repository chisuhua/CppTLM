#!/usr/bin/env python3
"""cpptlm_config/topology_adapter.py — Connect topology_generator.py and ConfigBuilder

Ref: ADR-X.12 Decision 3 (encapsulation pattern)

DEPRECATED (2026-06-17): 此包已被 cpptlm.topo + cpptlm.library 替代.
scripts.topology_generator backend 已不存在, 此 adapter 仅保留向后兼容.
合并迁移在下一期 (参见 openspec/changes/unified-config-emitter/).
"""

import sys
import os
import warnings
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from cpptlm_config.builder import ConfigBuilder

_warned = False
def _warn_legacy():
    global _warned
    if not _warned:
        warnings.warn(
            "cpptlm_config is deprecated; use cpptlm.topo.TopOrchestrator "
            "+ cpptlm.topo.CxxCompatibleEmitter instead. See "
            "openspec/changes/unified-config-emitter/ for migration plan.",
            DeprecationWarning, stacklevel=3,
        )
        _warned = True

class TopologyAdapter:
    """Convert TopologyGenerator output to ConfigBuilder input"""

    @staticmethod
    def from_mesh(rows: int, cols: int, builder: "ConfigBuilder") -> "ConfigBuilder":
        """Generate mesh topology and add to ConfigBuilder"""
        _warn_legacy()
        try:
            from scripts.topology_generator import TopologyGenerator
        except ImportError:
            raise ImportError(
                "topology_generator.py not found. "
                "Ensure scripts/topology_generator.py is in the search path."
            )

        gen = TopologyGenerator(name=f"mesh_{rows}x{cols}")
        gen.generate_mesh(rows, cols)

        from cpptlm_config.models import ModuleSpec, ConnectionSpec
        from cpptlm_config.types import ModuleType

        for node, attrs in gen.graph.nodes(data=True):
            module_type_str = attrs.get("type", "")
            cpp_type = TopologyGenerator.CPPTLM_TYPE_MAP.get(module_type_str, module_type_str)
            try:
                mod_type = ModuleType(cpp_type)
            except ValueError:
                mod_type = ModuleType.ROUTER_TLM

            builder.add_module(ModuleSpec(
                name=node,
                type=mod_type,
                params=attrs.get("params", {})
            ))

        for src, dst, attrs in gen.graph.edges(data=True):
            builder.add_connection(ConnectionSpec(
                src=src,
                dst=dst,
                latency=attrs.get("latency", 0),
                bandwidth=attrs.get("bandwidth", None)
            ))

        return builder

    @staticmethod
    def from_ring(nodes: int, builder: "ConfigBuilder") -> "ConfigBuilder":
        """Generate ring topology"""
        try:
            from scripts.topology_generator import TopologyGenerator
        except ImportError:
            raise ImportError("topology_generator.py not found")

        gen = TopologyGenerator(name=f"ring_{nodes}")
        gen.generate_ring(nodes)

        from cpptlm_config.models import ModuleSpec, ConnectionSpec
        from cpptlm_config.types import ModuleType

        for node, attrs in gen.graph.nodes(data=True):
            module_type_str = attrs.get("type", "")
            cpp_type = TopologyGenerator.CPPTLM_TYPE_MAP.get(module_type_str, module_type_str)
            try:
                mod_type = ModuleType(cpp_type)
            except ValueError:
                mod_type = ModuleType.ROUTER_TLM

            builder.add_module(ModuleSpec(
                name=node,
                type=mod_type,
                params=attrs.get("params", {})
            ))

        for src, dst, attrs in gen.graph.edges(data=True):
            builder.add_connection(ConnectionSpec(
                src=src,
                dst=dst,
                latency=attrs.get("latency", 0),
                bandwidth=attrs.get("bandwidth", None)
            ))

        return builder