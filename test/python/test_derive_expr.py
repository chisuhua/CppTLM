#!/usr/bin/env python3
"""test_derive_expr.py — derive_expr.py 单元测试"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from derive_expr import evaluate, evaluate_ternary, DeriveExprError


class TestDeriveExpr(unittest.TestCase):

    def test_simple_number(self):
        self.assertEqual(evaluate("42", {}), 42)

    def test_addition(self):
        self.assertEqual(evaluate("1 + 2", {}), 3)

    def test_multiplication_precedence(self):
        self.assertEqual(evaluate("2 + 3 * 4", {}), 14)

    def test_parentheses(self):
        self.assertEqual(evaluate("(2 + 3) * 4", {}), 20)

    def test_with_params(self):
        self.assertEqual(evaluate("vc_count * 2 + 1", {"vc_count": 4}), 9)

    def test_division(self):
        self.assertEqual(evaluate("16 / 4", {}), 4)

    def test_division_by_zero(self):
        with self.assertRaises(DeriveExprError):
            evaluate("1 / 0", {})

    def test_unknown_param(self):
        with self.assertRaises(DeriveExprError):
            evaluate("unknown_param", {})

    def test_unary_minus(self):
        self.assertEqual(evaluate("-5", {}), -5)

    def test_unary_minus_expression(self):
        self.assertEqual(evaluate("-(3 + 2)", {}), -5)

    def test_ternary_true(self):
        self.assertEqual(evaluate_ternary("vc_count >= 4 ? 8 : 4", {"vc_count": 4}), 8)

    def test_ternary_false(self):
        self.assertEqual(evaluate_ternary("vc_count >= 4 ? 8 : 4", {"vc_count": 2}), 4)

    def test_complex_expression(self):
        self.assertEqual(evaluate("(buffer_size + 1) * vc_count / 2", {
            "buffer_size": 7,
            "vc_count": 4
        }), 16)


if __name__ == "__main__":
    unittest.main()