from pydantic import BaseModel, Field
from typing import Optional
from enum import Enum


class PortRole(str, Enum):
    INITIATOR = "initiator"
    TARGET = "target"
    BI_DIRECTIONAL = "bi_directional"
    NETWORK = "network"
    PE = "pe"


class BundleType(str, Enum):
    CACHE_REQ = "cache_req"
    CACHE_RESP = "cache_resp"
    NOC_FLIT = "noc_flit"
    GENERIC = "generic"


class PortSpec(BaseModel):
    name: str
    role: PortRole
    bundle: BundleType
    width: int = 64
    is_multi: bool = False
    port_count: int = 1
    layout_hint: Optional[str] = None


class PortGroupMember(BaseModel):
    index: int
    role: PortRole
    bundle: BundleType


class PortGroupBundleType(str, Enum):
    SINGLE = "SINGLE"
    BUNDLE_MASTER = "BUNDLE_MASTER"
    BUNDLE_SLAVE = "BUNDLE_SLAVE"


class PortGroupSpec(BaseModel):
    name: str
    bundle_type: PortGroupBundleType = PortGroupBundleType.SINGLE
    ports: list[PortGroupMember] = []


class ModulePortSpec(BaseModel):
    module_name: str
    ports: list[PortSpec] = []
    port_groups: list[PortGroupSpec] = []
    aliases: dict[str, str] = {}


class ConfigMetadata(BaseModel):
    version: str = "1.0.0"
    schema_version: str = "1.0"
    visualization: Optional[dict] = None

    def bump_version(self, part: str = "patch") -> str:
        v = self.version.split(".")
        major, minor, patch = int(v[0]), int(v[1]), int(v[2])
        if part == "major":
            major += 1
            minor = 0
            patch = 0
        elif part == "minor":
            minor += 1
            patch = 0
        else:
            patch += 1
        self.version = f"{major}.{minor}.{patch}"
        return self.version


class ConfigSchema(BaseModel):
    name: str
    description: str = ""
    metadata: ConfigMetadata = ConfigMetadata()
    modules: list = []
    connections: list = []
    module_groups: list = []
