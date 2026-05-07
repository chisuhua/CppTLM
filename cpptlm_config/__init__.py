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
    ]
    _HAS_DEPS = True
except ImportError:
    _HAS_DEPS = False
