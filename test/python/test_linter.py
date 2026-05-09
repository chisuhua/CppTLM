#!/usr/bin/env python3
"""test_linter.py — linter.py 单元测试"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from linter import TopologyLinter


class TestLinter(unittest.TestCase):

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
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertEqual(len(errors), 0)
        self.assertFalse(linter.has_errors())

    def test_missing_name(self):
        config = {
            "modules": [
                {"type": "NICTLM"},
            ],
            "connections": []
        }
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertTrue(any(e.code == "E001" for e in errors))

    def test_missing_type(self):
        config = {
            "modules": [
                {"name": "a"},
            ],
            "connections": []
        }
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertTrue(any(e.code == "E002" for e in errors))

    def test_self_loop(self):
        config = {
            "modules": [
                {"name": "a", "type": "RouterTLM"},
            ],
            "connections": [
                {"src": "a.0", "dst": "a.1"},
            ]
        }
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertTrue(any(e.code == "W001" for e in errors))

    def test_missing_module_reference(self):
        config = {
            "modules": [
                {"name": "a", "type": "NICTLM"},
            ],
            "connections": [
                {"src": "a", "dst": "b"},
            ]
        }
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertTrue(any(e.code == "E005" for e in errors))

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
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertTrue(any(e.code == "W003" for e in errors))

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
        linter = TopologyLinter(config)
        errors = linter.lint()
        self.assertTrue(any(e.code == "W002" for e in errors))


if __name__ == "__main__":
    unittest.main()