#!/usr/bin/env python3
"""derive_expr.py — derive 表达式解析器"""

import re
from typing import Dict


class DeriveExprError(Exception):
    pass


class _ExprParser:
    def __init__(self, text: str):
        self.text = text
        self.pos = 0

    def parse(self):
        node = self._parse_expr()
        self._skip_ws()
        if self.pos < len(self.text):
            raise DeriveExprError(f"Unexpected token at position {self.pos}")
        return node

    def _skip_ws(self):
        while self.pos < len(self.text) and self.text[self.pos].isspace():
            self.pos += 1

    def _peek(self):
        self._skip_ws()
        return self.text[self.pos] if self.pos < len(self.text) else None

    def _consume(self):
        self._skip_ws()
        ch = self.text[self.pos] if self.pos < len(self.text) else None
        if ch:
            self.pos += 1
        return ch

    def _parse_expr(self):
        left = self._parse_term()
        while True:
            ch = self._peek()
            if ch in ('+', '-'):
                self._consume()
                right = self._parse_term()
                left = ('binop', ch, left, right)
            else:
                break
        return left

    def _parse_term(self):
        left = self._parse_factor()
        while True:
            ch = self._peek()
            if ch in ('*', '/'):
                self._consume()
                right = self._parse_factor()
                left = ('binop', ch, left, right)
            else:
                break
        return left

    def _parse_factor(self):
        ch = self._peek()
        if ch == '-':
            self._consume()
            return ('unary', '-', self._parse_factor())
        if ch == '(':
            self._consume()
            node = self._parse_expr()
            if self._peek() != ')':
                raise DeriveExprError("Expected ')'")
            self._consume()
            return node
        if ch and ch.isdigit():
            return self._parse_number()
        if ch and (ch.isalpha() or ch == '_'):
            return self._parse_identifier()
        raise DeriveExprError(f"Unexpected character: {ch}")

    def _parse_number(self):
        val = 0
        while True:
            ch = self._peek()
            if ch and ch.isdigit():
                self._consume()
                val = val * 10 + int(ch)
            else:
                break
        return ('number', val)

    def _parse_identifier(self):
        name = ''
        while True:
            ch = self._peek()
            if ch and (ch.isalnum() or ch == '_'):
                self._consume()
                name += ch
            else:
                break
        return ('ident', name)


def _eval_node(node, params: Dict[str, int]):
    if node[0] == 'number':
        return node[1]
    if node[0] == 'ident':
        name = node[1]
        if name not in params:
            raise DeriveExprError(f"Unknown parameter: {name}")
        return params[name]
    if node[0] == 'binop':
        op = node[1]
        left = _eval_node(node[2], params)
        right = _eval_node(node[3], params)
        if op == '+':
            return left + right
        if op == '-':
            return left - right
        if op == '*':
            return left * right
        if op == '/':
            if right == 0:
                raise DeriveExprError("Division by zero")
            return left // right
        raise DeriveExprError(f"Unknown operator: {op}")
    if node[0] == 'unary':
        val = _eval_node(node[2], params)
        return -val
    raise DeriveExprError(f"Unknown node type: {node[0]}")


def evaluate(expr: str, params: Dict[str, int]) -> int:
    parser = _ExprParser(expr)
    ast = parser.parse()
    return _eval_node(ast, params)


def evaluate_ternary(expr: str, params: Dict[str, int]) -> int:
    match = re.match(r"(\w+)\s*(>=|<=|>|<|==)\s*(\d+)\s*\?\s*(\d+)\s*:\s*(\d+)", expr.strip())
    if not match:
        raise DeriveExprError(f"Invalid ternary expression: {expr}")

    param_name = match.group(1)
    op = match.group(2)
    rhs = int(match.group(3))
    true_val = int(match.group(4))
    false_val = int(match.group(5))

    if param_name not in params:
        raise DeriveExprError(f"Unknown parameter: {param_name}")

    lhs = params[param_name]

    cond = False
    if op == '>=':
        cond = lhs >= rhs
    elif op == '<=':
        cond = lhs <= rhs
    elif op == '>':
        cond = lhs > rhs
    elif op == '<':
        cond = lhs < rhs
    elif op == '==':
        cond = lhs == rhs

    return true_val if cond else false_val


if __name__ == "__main__":
    print(evaluate("vc_count * 2 + 1", {"vc_count": 4}))
    print(evaluate_ternary("vc_count >= 4 ? 8 : 4", {"vc_count": 4}))