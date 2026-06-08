#!/usr/bin/env python3
"""cpptlm_config/tests/test_config_builder.py — TDD tests for ConfigBuilder

RED phase: All tests should FAIL until we implement:
1. ModuleType enum in types.py
2. ModuleSpec + ConnectionSpec in models.py
3. ConfigBuilder in builder.py
4. ConfigSchema.save() in models.py
"""

import sys
import os
import unittest
import tempfile
import json

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

try:
    import pydantic as _pydantic
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False

if HAS_DEPS:
    from cpptlm_config.types import RouterPort, NICPort
    from cpptlm_config.models import (
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
    from cpptlm_config.validator import TopologyValidator, ValidationResult, ValidationIssue


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestModuleType(unittest.TestCase):
    """RED: ModuleType enum does not exist yet in types.py"""

    def test_module_type_enum_values(self):
        from cpptlm_config.types import ModuleType
        # Should have at least these module types
        self.assertEqual(ModuleType.ROUTER_TLM.value, "RouterTLM")
        self.assertEqual(ModuleType.NIC_TLM.value, "NICTLM")
        self.assertEqual(ModuleType.CACHE_TLM.value, "CacheTLM")
        self.assertEqual(ModuleType.CROSSBAR_TLM.value, "CrossbarTLM")
        self.assertEqual(ModuleType.MEMORY_TLM.value, "MemoryTLM")
        # CPU_SIM retained for backward compatibility; only valid in C++ build
        # when BUILD_LEGACY_MODULES=ON. See cpptlm_config/types.py for details.
        self.assertEqual(ModuleType.CPU_SIM.value, "CPUSim")

    def test_module_type_from_string(self):
        from cpptlm_config.types import ModuleType
        # Should parse from string
        self.assertEqual(ModuleType("RouterTLM"), ModuleType.ROUTER_TLM)
        self.assertEqual(ModuleType("NICTLM"), ModuleType.NIC_TLM)

    def test_module_type_is_string_enum(self):
        from cpptlm_config.types import ModuleType
        self.assertIsInstance(ModuleType.ROUTER_TLM.value, str)


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestModuleSpec(unittest.TestCase):
    """RED: ModuleSpec does not exist yet in models.py"""

    def test_module_spec_creation(self):
        from cpptlm_config.models import ModuleSpec
        from cpptlm_config.types import ModuleType

        spec = ModuleSpec(
            name="router_0_0",
            type=ModuleType.ROUTER_TLM,
            params={"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}
        )
        self.assertEqual(spec.name, "router_0_0")
        self.assertEqual(spec.type, ModuleType.ROUTER_TLM)
        self.assertEqual(spec.params["node_x"], 0)

    def test_module_spec_with_port_spec(self):
        from cpptlm_config.models import ModuleSpec, ModulePortSpec, PortSpec, PortRole, BundleType
        from cpptlm_config.types import ModuleType

        port_spec = ModulePortSpec(
            module_name="router_0_0",
            ports=[
                PortSpec(name="north", role=PortRole.NETWORK, bundle=BundleType.NOC_FLIT, width=64)
            ]
        )
        spec = ModuleSpec(
            name="router_0_0",
            type=ModuleType.ROUTER_TLM,
            params={"node_x": 0, "node_y": 0},
            port_spec=port_spec
        )
        self.assertEqual(spec.port_spec.module_name, "router_0_0")
        self.assertEqual(len(spec.port_spec.ports), 1)

    def test_module_spec_ports_and_groups_fields(self):
        from cpptlm_config.models import ModuleSpec
        from cpptlm_config.types import ModuleType

        # Per ADR-X.12 and plan P1.2, ModuleSpec should have ports and port_groups
        spec = ModuleSpec(
            name="router_0_0",
            type=ModuleType.ROUTER_TLM,
            params={},
            ports=[],       # New field per ADR-X.12
            port_groups=[]  # New field per ADR-X.12
        )
        self.assertEqual(spec.ports, [])
        self.assertEqual(spec.port_groups, [])


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestConnectionSpec(unittest.TestCase):
    """RED: ConnectionSpec does not exist yet in models.py"""

    def test_connection_spec_basic(self):
        from cpptlm_config.models import ConnectionSpec

        spec = ConnectionSpec(
            src="router_0_0.1",
            dst="router_0_1.3",
            latency=1
        )
        self.assertEqual(spec.src, "router_0_0.1")
        self.assertEqual(spec.dst, "router_0_1.3")
        self.assertEqual(spec.latency, 1)

    def test_connection_spec_with_bandwidth(self):
        from cpptlm_config.models import ConnectionSpec

        spec = ConnectionSpec(
            src="cache0.0",
            dst="xbar.0",
            latency=2,
            bandwidth=100
        )
        self.assertEqual(spec.bandwidth, 100)


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestConfigSchemaSave(unittest.TestCase):
    """RED: ConfigSchema.save() does not exist yet"""

    def setUp(self):
        from cpptlm_config.models import ConfigSchema, ConfigMetadata
        self.schema = ConfigSchema(
            name="test_mesh",
            description="Test topology",
            metadata=ConfigMetadata(version="1.0.0"),
            modules=[],
            connections=[]
        )

    def test_save_to_json_file(self):
        import tempfile
        import os
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            path = f.name
        try:
            self.schema.save(path)
            with open(path, 'r') as f:
                data = json.load(f)
            self.assertEqual(data["name"], "test_mesh")
            self.assertEqual(data["metadata"]["version"], "1.0.0")
        finally:
            os.unlink(path)

    def test_to_json_dict(self):
        d = self.schema.to_json_dict()
        self.assertEqual(d["name"], "test_mesh")
        self.assertIn("version", d["metadata"])


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestConfigBuilder(unittest.TestCase):
    """RED: ConfigBuilder does not exist yet"""

    def test_config_builder_init(self):
        from cpptlm_config.builder import ConfigBuilder
        builder = ConfigBuilder(name="mesh_2x2", description="2x2 mesh")
        self.assertEqual(builder.name, "mesh_2x2")
        self.assertEqual(builder.description, "2x2 mesh")

    def test_add_module(self):
        from cpptlm_config.builder import ConfigBuilder
        from cpptlm_config.models import ModuleSpec
        from cpptlm_config.types import ModuleType

        builder = ConfigBuilder(name="test", description="")
        builder.add_module(ModuleSpec(
            name="router_0_0",
            type=ModuleType.ROUTER_TLM,
            params={"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}
        ))
        schema = builder.build()
        self.assertEqual(len(schema.modules), 1)
        self.assertEqual(schema.modules[0].name, "router_0_0")

    def test_add_connection(self):
        from cpptlm_config.builder import ConfigBuilder
        from cpptlm_config.models import ModuleSpec, ConnectionSpec
        from cpptlm_config.types import ModuleType

        builder = ConfigBuilder(name="test", description="")
        builder.add_module(ModuleSpec(name="r0", type=ModuleType.ROUTER_TLM, params={}))
        builder.add_module(ModuleSpec(name="r1", type=ModuleType.ROUTER_TLM, params={}))
        builder.add_connection(ConnectionSpec(src="r0.1", dst="r1.3", latency=1))
        schema = builder.build()
        self.assertEqual(len(schema.connections), 1)
        self.assertEqual(schema.connections[0].src, "r0.1")

    def test_build_returns_config_schema(self):
        from cpptlm_config.builder import ConfigBuilder
        from cpptlm_config.models import ConfigSchema

        builder = ConfigBuilder(name="test", description="")
        result = builder.build()
        self.assertIsInstance(result, ConfigSchema)
        self.assertEqual(result.name, "test")

    def test_build_idempotent(self):
        from cpptlm_config.builder import ConfigBuilder
        from cpptlm_config.models import ModuleSpec
        from cpptlm_config.types import ModuleType

        builder = ConfigBuilder(name="test", description="")
        builder.add_module(ModuleSpec(name="r0", type=ModuleType.ROUTER_TLM, params={}))

        schema1 = builder.build()
        schema2 = builder.build()
        self.assertEqual(len(schema1.modules), 1)
        self.assertEqual(len(schema2.modules), 1)

    def test_config_builder_full_mesh_workflow(self):
        from cpptlm_config.builder import ConfigBuilder
        from cpptlm_config.models import ModuleSpec, ConnectionSpec
        from cpptlm_config.types import ModuleType

        builder = ConfigBuilder(name="mesh_2x2", description="2x2 mesh NoC")

        for r in range(2):
            for c in range(2):
                builder.add_module(ModuleSpec(
                    name=f"router_{r}_{c}",
                    type=ModuleType.ROUTER_TLM,
                    params={"node_x": r, "node_y": c, "mesh_x": 2, "mesh_y": 2}
                ))

        for r in range(2):
            for c in range(1):
                builder.add_connection(ConnectionSpec(
                    src=f"router_{r}_{c}.1",
                    dst=f"router_{r}_{c+1}.3",
                    latency=1
                ))

        for r in range(1):
            for c in range(2):
                builder.add_connection(ConnectionSpec(
                    src=f"router_{r}_{c}.2",
                    dst=f"router_{r+1}_{c}.0",
                    latency=1
                ))

        schema = builder.build()
        self.assertEqual(len(schema.modules), 4)
        self.assertEqual(len(schema.connections), 4)

        import tempfile, os
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            path = f.name
        try:
            schema.save(path)
            with open(path) as f:
                data = json.load(f)
            self.assertEqual(data["name"], "mesh_2x2")
            self.assertEqual(len(data["modules"]), 4)
        finally:
            os.unlink(path)


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestConfigBuilderImports(unittest.TestCase):
    """Verify ConfigBuilder can be imported from cpptlm_config package"""

    def test_import_config_builder(self):
        from cpptlm_config import ConfigBuilder
        self.assertTrue(ConfigBuilder is not None)

    def test_import_module_spec(self):
        from cpptlm_config import ModuleSpec
        self.assertTrue(ModuleSpec is not None)

    def test_import_connection_spec(self):
        from cpptlm_config import ConnectionSpec
        self.assertTrue(ConnectionSpec is not None)

    def test_import_module_type(self):
        from cpptlm_config import ModuleType
        self.assertTrue(ModuleType is not None)


if __name__ == "__main__":
    unittest.main()
