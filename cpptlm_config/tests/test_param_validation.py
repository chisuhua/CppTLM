#!/usr/bin/env python3
"""cpptlm_config/tests/test_param_validation.py — TDD tests for PARAM-01/02

RED phase: All tests should FAIL until we implement:
1. validate_required_params() in TopologyValidator
2. validate_param_ranges() in TopologyValidator
3. load_param_rules() utility
"""

import sys
import os
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

try:
    import pydantic as _pydantic
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False

if HAS_DEPS:
    from cpptlm_config.validator import TopologyValidator, ValidationResult
    from cpptlm_config.types import ModuleType


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestParamValidationPARAM01(unittest.TestCase):
    """PARAM-01: Required parameters must be present"""

    def test_missing_required_param_reports_error(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM", "params": {"node_x": 0}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_required_params()
        self.assertFalse(validator.result.is_valid)
        self.assertTrue(any(e.code == "PARAM-01" for e in validator.result.errors))

    def test_all_required_params_present_no_error(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_required_params()
        self.assertTrue(validator.result.is_valid, [e.message for e in validator.result.errors])

    def test_router_missing_node_y(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0, "mesh_x": 2, "mesh_y": 2}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_required_params()
        self.assertFalse(validator.result.is_valid)
        missing = [e for e in validator.result.errors if e.code == "PARAM-01"]
        self.assertTrue(len(missing) > 0)

    def test_nic_missing_required_params(self):
        config = {
            "modules": [
                {"name": "n0", "type": "NICTLM", "params": {"node_id": 0}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_required_params()
        self.assertFalse(validator.result.is_valid)

    def test_unknown_module_type_skipped(self):
        config = {
            "modules": [
                {"name": "unknown0", "type": "UnknownModule", "params": {}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_required_params()
        self.assertTrue(validator.result.is_valid)


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestParamValidationPARAM02(unittest.TestCase):
    """PARAM-02: Parameter ranges must be respected"""

    def test_vc_count_above_max_reports_error(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2, "vc_count": 100}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_param_ranges()
        self.assertFalse(validator.result.is_valid)
        self.assertTrue(any(e.code == "PARAM-02" for e in validator.result.errors))

    def test_vc_count_within_range_no_error(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2, "vc_count": 4}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_param_ranges()
        self.assertTrue(validator.result.is_valid, [e.message for e in validator.result.errors])

    def test_vc_count_below_min_reports_error(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2, "vc_count": 0}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_param_ranges()
        self.assertFalse(validator.result.is_valid)
        self.assertTrue(any(e.code == "PARAM-02" for e in validator.result.errors))

    def test_missing_optional_param_not_error(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        validator.validate_param_ranges()
        self.assertTrue(validator.result.is_valid)


@unittest.skipUnless(HAS_DEPS, "pydantic not installed")
class TestParamValidationIntegration(unittest.TestCase):
    """Integration: PARAM-01 and PARAM-02 called from validate()"""

    def test_validate_calls_param_checks(self):
        config = {
            "modules": [
                {"name": "r0", "type": "RouterTLM",
                 "params": {"node_x": 0}}
            ],
            "connections": []
        }
        validator = TopologyValidator(config)
        result = validator.validate()
        self.assertFalse(result.is_valid)
        param_errors = [e for e in result.errors if e.code in ("PARAM-01", "PARAM-02")]
        self.assertTrue(len(param_errors) > 0)


if __name__ == "__main__":
    unittest.main()
