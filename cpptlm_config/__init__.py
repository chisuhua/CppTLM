"""cpptlm_config — Legacy Pydantic-based topology configuration (DEPRECATED 2026-06-17)

本包已被 `cpptlm.topo` (Layer/Patch/Variant/Orchestrator) +
`cpptlm.topo.CxxCompatibleEmitter` (Python → C++ JSON emit) +
`cpptlm.library` (cluster 工厂) 替代.

合并迁移在下一期 (参见 openspec/changes/unified-config-emitter/).
本包仍可用, 每次 import 会触发 DeprecationWarning.

Migration::

    # Old (cpptlm_config Pydantic)
    from cpptlm_config.builder import ConfigBuilder
    b = ConfigBuilder("my_soc")
    b.add_module(ModuleSpec(name="cpu0", type="CPUTLM"))
    b.add_group("compute", ["cpu0"])
    schema = b.build()
    schema.save("configs/my_soc.json")  # outputs module_groups (dead field)

    # New (cpptlm.topo + cpptlm.library)
    from cpptlm.topo import TopoLayer
    from cpptlm.library import cpu_l1_cluster, SoC
    from cpptlm.topo.emitter import CxxCompatibleEmitter
    soc = SoC("my_soc")
    soc.add_cluster(cpu_l1_cluster(0)).tag("compute")
    soc.save("configs/my_soc.json")  # emits groups dict (C++ compatible)
"""
import warnings as _warnings
_warnings.warn(
    "cpptlm_config is deprecated (2026-06-17); use cpptlm.topo + "
    "cpptlm.topo.CxxCompatibleEmitter + cpptlm.library instead. See "
    "openspec/changes/unified-config-emitter/ for migration plan.",
    DeprecationWarning, stacklevel=2,
)

try:
    from .types import RouterPort, NICPort, ModuleType
    from .models import (
        PortRole,
        BundleType,
        PortSpec,
        PortGroupMember,
        PortGroupBundleType,
        PortGroupSpec,
        ModulePortSpec,
        ConfigMetadata,
        ConfigSchema,
        ModuleSpec,
        ConnectionSpec,
    )
    from .validator import TopologyValidator, ValidationResult, ValidationIssue
    from .topology_adapter import TopologyAdapter
    from .builder import ConfigBuilder
    __all__ = [
        "RouterPort",
        "NICPort",
        "ModuleType",
        "PortRole",
        "BundleType",
        "PortSpec",
        "PortGroupMember",
        "PortGroupBundleType",
        "PortGroupSpec",
        "ModulePortSpec",
        "ConfigMetadata",
        "ConfigSchema",
        "ModuleSpec",
        "ConnectionSpec",
        "TopologyValidator",
        "ValidationResult",
        "ValidationIssue",
        "TopologyAdapter",
        "ConfigBuilder",
    ]
    _HAS_DEPS = True
except ImportError:
    _HAS_DEPS = False
