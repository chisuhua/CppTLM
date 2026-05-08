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
