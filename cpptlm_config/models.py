from pydantic import BaseModel, Field
from typing import Optional
from enum import Enum

try:
    from cpptlm_config.types import ModuleType
except ImportError:
    ModuleType = None  # Will be resolved later


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


class ModuleSpec(BaseModel):
    name: str
    type: ModuleType | str = "RouterTLM"  # Accept both enum and string
    params: dict = {}
    port_spec: Optional[ModulePortSpec] = None
    ports: list = []
    port_groups: list = []


class ConnectionSpec(BaseModel):
    src: str
    dst: str
    latency: int = 0
    bandwidth: Optional[int] = None


class ModuleGroupSpec(BaseModel):
    name: str
    members: list[str]
    exclude: list[str] = []


class ConfigSchema(BaseModel):
    name: str
    description: str = ""
    metadata: ConfigMetadata = ConfigMetadata()
    modules: list = []
    connections: list = []
    module_groups: list[ModuleGroupSpec] = []
    include: Optional[str] = None
    extends: Optional[str] = None

    def save(self, path: str):
        """Serialize to JSON file (supports // comment-free JSON)"""
        data = self.to_json_dict()
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    def to_json_dict(self) -> dict:
        def _serialize(obj):
            if hasattr(obj, 'model_dump'):
                d = obj.model_dump()
                for k, v in d.items():
                    d[k] = _serialize(v)
                return d
            elif isinstance(obj, dict):
                return {k: _serialize(v) for k, v in obj.items()}
            elif isinstance(obj, list):
                return [_serialize(item) for item in obj]
            elif isinstance(obj, Enum):
                return obj.value
            return obj

        result = {
            "name": self.name,
            "description": self.description,
            "metadata": {
                "version": self.metadata.version,
                "schema_version": self.metadata.schema_version,
            },
            "modules": _serialize(self.modules),
            "connections": _serialize(self.connections),
        }
        if self.module_groups:
            result["module_groups"] = _serialize(self.module_groups)
        if self.include:
            result["include"] = self.include
        if self.extends:
            result["extends"] = self.extends
        return result


import json
