#!/usr/bin/env python3
"""test_validator.py — TopologyValidator 单元测试（合并自原 linter 测试）"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from cpptlm_config.validator import TopologyValidator


class TestValidator(unittest.TestCase):
    """迁移自原 test_linter.py — 测试 TopologyValidator 的检查功能"""

    def test_no_issues(self):
        config = {
            "modules": [
                {"name": "a", "type": "NICTLM"},
                {"name": "b", "type": "RouterTLM"},
            ],
            "connections": [
                {"src": "a", "dst": "b"},
            ]
        }
        validator = TopologyValidator(config)
        result = validator.validate_connectivity().result
        self.assertFalse(any(e.code == "E001" for e in result.errors))
        self.assertFalse(any(e.code == "E002" for e in result.errors))

    def test_missing_name_graceful(self):
        config = {
            "modules": [],
            "connections": []
        }
        validator = TopologyValidator(config)
        result = validator.validate_connectivity().result
        self.assertTrue(result.is_valid)

    def test_self_loop(self):
        config = {
            "modules": [
                {"name": "a", "type": "RouterTLM"},
            ],
            "connections": [
                {"src": "a.0", "dst": "a.1"},
            ]
        }
        validator = TopologyValidator(config)
        result = validator.validate_connectivity().result
        self.assertTrue(any(w.code == "W001" for w in result.warnings))

    def test_undefined_module_reference(self):
        config = {
            "modules": [
                {"name": "a", "type": "NICTLM"},
            ],
            "connections": [
                {"src": "a", "dst": "b"},
            ]
        }
        validator = TopologyValidator(config)
        result = validator.validate_connectivity().result
        self.assertTrue(any(e.code == "VALID-01" for e in result.errors))

    def test_isolated_module(self):
        config = {
            "modules": [
                {"name": "a", "type": "NICTLM"},
                {"name": "b", "type": "RouterTLM"},
            ],
            "connections": [
                {"src": "a", "dst": "a"},
            ]
        }
        validator = TopologyValidator(config)
        result = validator.validate_connectivity().result
        self.assertTrue(any(w.code == "W003" for w in result.warnings))

    def test_duplicate_connection(self):
        config = {
            "modules": [
                {"name": "a", "type": "NICTLM"},
                {"name": "b", "type": "RouterTLM"},
            ],
            "connections": [
                {"src": "a", "dst": "b"},
                {"src": "a", "dst": "b"},
            ]
        }
        validator = TopologyValidator(config)
        result = validator.validate_connectivity().result
        self.assertTrue(any(w.code == "W002" for w in result.warnings))


if __name__ == "__main__":
    unittest.main()
