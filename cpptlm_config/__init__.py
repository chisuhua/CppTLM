try:
    from .types import RouterPort, NICPort
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
    )
    from .validator import TopologyValidator, ValidationResult, ValidationIssue
    from .topology_adapter import TopologyAdapter
    __all__ = [
        "RouterPort",
        "NICPort",
        "PortRole",
        "BundleType",
        "PortSpec",
        "PortGroupMember",
        "PortGroupBundleType",
        "PortGroupSpec",
        "ModulePortSpec",
        "ConfigMetadata",
        "ConfigSchema",
        "TopologyValidator",
        "ValidationResult",
        "ValidationIssue",
        "TopologyAdapter",
    ]
    _HAS_DEPS = True
except ImportError:
    _HAS_DEPS = False
