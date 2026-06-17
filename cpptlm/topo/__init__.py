"""
cpptlm/topo - Hierarchical Topology Generator Package

Layer + Patch pattern for modular topology generation.
"""
from cpptlm.topo.layer import TopoLayer, ModuleSpec, ConnectionSpec, CoherenceDomainSpec
from cpptlm.topo.patch import TopoPatch, ConnectionSelector, ModuleSelector, LayerSelector, PatchAction
from cpptlm.topo.variant import TopoVariant, TopoVariantSet
from cpptlm.topo.orchestrator import TopoOrchestrator
from cpptlm.topo.emitter import CxxCompatibleEmitter, TopoEmitError

__all__ = [
    "TopoLayer",
    "ModuleSpec",
    "ConnectionSpec",
    "CoherenceDomainSpec",
    "TopoPatch",
    "ConnectionSelector",
    "ModuleSelector",
    "LayerSelector",
    "PatchAction",
    "TopoVariant",
    "TopoVariantSet",
    "TopoOrchestrator",
    "CxxCompatibleEmitter",
    "TopoEmitError",
]