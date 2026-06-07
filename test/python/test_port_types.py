#!/usr/bin/env python3
"""test_port_types.py — cpptlm_config port type tests"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

try:
    import pydantic as _pydantic
    HAS_DEPS = True
except ImportError as e:
    HAS_DEPS = False

if HAS_DEPS:
    from cpptlm_config import (
        RouterPort,
        NICPort,
        PortRole,
        BundleType,
        PortSpec,
        PortGroupMember,
        PortGroupBundleType,
        PortGroupSpec,
        ModulePortSpec,
        ConfigMetadata,
    )
    import unittest

    class TestRouterPort(unittest.TestCase):
        def test_router_port_enum_values(self):
            self.assertEqual(RouterPort.NORTH, 0)
            self.assertEqual(RouterPort.EAST, 1)
            self.assertEqual(RouterPort.SOUTH, 2)
            self.assertEqual(RouterPort.WEST, 3)
            self.assertEqual(RouterPort.LOCAL, 4)

        def test_router_port_int_enum(self):
            self.assertIsInstance(RouterPort.NORTH, int)
            self.assertEqual(int(RouterPort.NORTH), 0)

    class TestNICPort(unittest.TestCase):
        def test_nic_port_enum_values(self):
            self.assertEqual(NICPort.PE_REQ, 0)
            self.assertEqual(NICPort.PE_RESP, 1)
            self.assertEqual(NICPort.NET_REQ, 2)
            self.assertEqual(NICPort.NET_RESP, 3)

        def test_nic_port_int_enum(self):
            self.assertIsInstance(NICPort.NET_REQ, int)

    class TestPortRole(unittest.TestCase):
        def test_port_role_values(self):
            self.assertEqual(PortRole.INITIATOR.value, "initiator")
            self.assertEqual(PortRole.TARGET.value, "target")
            self.assertEqual(PortRole.BI_DIRECTIONAL.value, "bi_directional")
            self.assertEqual(PortRole.NETWORK.value, "network")
            self.assertEqual(PortRole.PE.value, "pe")

    class TestBundleType(unittest.TestCase):
        def test_bundle_type_values(self):
            self.assertEqual(BundleType.CACHE_REQ.value, "cache_req")
            self.assertEqual(BundleType.CACHE_RESP.value, "cache_resp")
            self.assertEqual(BundleType.NOC_FLIT.value, "noc_flit")
            self.assertEqual(BundleType.GENERIC.value, "generic")

    class TestPortSpec(unittest.TestCase):
        def test_port_spec_creation(self):
            spec = PortSpec(name="NORTH", role=PortRole.NETWORK, bundle=BundleType.NOC_FLIT)
            self.assertEqual(spec.name, "NORTH")
            self.assertEqual(spec.role, PortRole.NETWORK)
            self.assertEqual(spec.bundle, BundleType.NOC_FLIT)
            self.assertEqual(spec.width, 64)
            self.assertEqual(spec.is_multi, False)

        def test_port_spec_with_layout_hint(self):
            spec = PortSpec(
                name="test",
                role=PortRole.INITIATOR,
                bundle=BundleType.CACHE_REQ,
                layout_hint="top",
            )
            self.assertEqual(spec.layout_hint, "top")

        def test_port_spec_json(self):
            spec = PortSpec(name="test", role=PortRole.INITIATOR, bundle=BundleType.CACHE_REQ)
            data = spec.model_dump()
            self.assertEqual(data["name"], "test")
            self.assertEqual(data["role"], "initiator")

    class TestPortGroupMember(unittest.TestCase):
        def test_port_group_member(self):
            member = PortGroupMember(index=0, role=PortRole.TARGET, bundle=BundleType.CACHE_REQ)
            self.assertEqual(member.index, 0)
            self.assertEqual(member.role, PortRole.TARGET)

    class TestPortGroupBundleType(unittest.TestCase):
        def test_port_group_bundle_type_values(self):
            self.assertEqual(PortGroupBundleType.SINGLE.value, "SINGLE")
            self.assertEqual(PortGroupBundleType.BUNDLE_MASTER.value, "BUNDLE_MASTER")
            self.assertEqual(PortGroupBundleType.BUNDLE_SLAVE.value, "BUNDLE_SLAVE")

    class TestPortGroupSpec(unittest.TestCase):
        def test_port_group_spec(self):
            pg = PortGroupSpec(
                name="pe",
                bundle_type=PortGroupBundleType.SINGLE,
                ports=[
                    PortGroupMember(index=0, role=PortRole.TARGET, bundle=BundleType.CACHE_REQ),
                    PortGroupMember(index=1, role=PortRole.INITIATOR, bundle=BundleType.CACHE_RESP),
                ],
            )
            self.assertEqual(pg.name, "pe")
            self.assertEqual(len(pg.ports), 2)

    class TestModulePortSpec(unittest.TestCase):
        def test_module_port_spec(self):
            mod = ModulePortSpec(
                module_name="nic0",
                ports=[PortSpec(name="test", role=PortRole.INITIATOR, bundle=BundleType.CACHE_REQ)],
                port_groups=[
                    PortGroupSpec(
                        name="pe",
                        ports=[PortGroupMember(index=0, role=PortRole.TARGET, bundle=BundleType.CACHE_REQ)],
                    )
                ],
            )
            self.assertEqual(mod.module_name, "nic0")
            self.assertEqual(len(mod.ports), 1)
            self.assertEqual(len(mod.port_groups), 1)

    class TestConfigMetadata(unittest.TestCase):
        def test_config_metadata_default(self):
            meta = ConfigMetadata()
            self.assertEqual(meta.version, "1.0.0")
            self.assertEqual(meta.schema_version, "1.0")

        def test_bump_version_patch(self):
            meta = ConfigMetadata(version="1.2.3")
            result = meta.bump_version("patch")
            self.assertEqual(result, "1.2.4")
            self.assertEqual(meta.version, "1.2.4")

        def test_bump_version_minor(self):
            meta = ConfigMetadata(version="1.2.3")
            result = meta.bump_version("minor")
            self.assertEqual(result, "1.3.0")
            self.assertEqual(meta.version, "1.3.0")

        def test_bump_version_major(self):
            meta = ConfigMetadata(version="1.2.3")
            result = meta.bump_version("major")
            self.assertEqual(result, "2.0.0")
            self.assertEqual(meta.version, "2.0.0")

    def main():
        unittest.main()

else:
    import unittest

    class TestRouterPort(unittest.TestCase):
        pass

    class TestNICPort(unittest.TestCase):
        pass

    class TestPortRole(unittest.TestCase):
        pass

    class TestBundleType(unittest.TestCase):
        pass

    class TestPortSpec(unittest.TestCase):
        pass

    class TestPortGroupMember(unittest.TestCase):
        pass

    class TestPortGroupBundleType(unittest.TestCase):
        pass

    class TestPortGroupSpec(unittest.TestCase):
        pass

    class TestModulePortSpec(unittest.TestCase):
        pass

    class TestConfigMetadata(unittest.TestCase):
        pass

    def main():
        print("Skipping tests: pydantic not installed")
        sys.exit(0)

if __name__ == "__main__":
    main()
