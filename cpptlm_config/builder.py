#!/usr/bin/env python3
"""cpptlm_config/builder.py — ConfigBuilder API for constructing topologies"""

from __future__ import annotations

from typing import Optional
from cpptlm_config.models import ConfigSchema, ConfigMetadata, ModuleSpec, ConnectionSpec


class ConfigBuilder:
    """
    构造 CppTLM JSON 配置的 Python API。

    用法::

        builder = ConfigBuilder(name="mesh_2x2", description="2x2 mesh")
        builder.add_module(ModuleSpec(name="r0", type=ModuleType.ROUTER_TLM, params={...}))
        builder.add_connection(ConnectionSpec(src="r0.1", dst="r1.3", latency=1))
        schema = builder.build()
        schema.save("configs/mesh_2x2.json")
    """

    def __init__(self, name: str, description: str = ""):
        self.name = name
        self.description = description
        self._modules: list[ModuleSpec] = []
        self._connections: list[ConnectionSpec] = []
        self._metadata = ConfigMetadata(version="1.0.0", schema_version="1.0")

    def add_module(self, module: ModuleSpec) -> ConfigBuilder:
        self._modules.append(module)
        return self

    def add_connection(self, connection: ConnectionSpec) -> ConfigBuilder:
        self._connections.append(connection)
        return self

    def build(self) -> ConfigSchema:
        return ConfigSchema(
            name=self.name,
            description=self.description,
            metadata=self._metadata,
            modules=self._modules,
            connections=self._connections,
            module_groups=[]
        )
