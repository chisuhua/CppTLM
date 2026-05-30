"""
TopoPatch - 局部修改机制

提供 PatchAction / Selector 体系，对 TopoLayer 进行局部修改：
- REMOVE: 移除匹配项
- ADD: 添加新内容
- REPLACE: 替换匹配项
- REWIRE: 修改连接端点

用法::

    # 移除所有到 xbar 的连接
    patch = TopoPatch(
        selector=ConnectionSelector(pattern="*.xbar.*"),
        action=PatchAction.REMOVE
    )

    # 重连 cpu0.cache → cpu0.new_cache
    patch = TopoPatch(
        selector=ConnectionSelector(pattern="cpu0.cache"),
        action=PatchAction.REWIRE,
        rewiring={"dst": "new_cache"}
    )

    # 添加新 NoC 连接
    patch = TopoPatch(
        selector=LayerSelector(name_pattern="noc"),
        action=PatchAction.ADD,
        template={"name": "new_noc", "modules": [], "connections": []}
    )
"""

import re
from enum import Enum
from typing import TYPE_CHECKING, Optional

if TYPE_CHECKING:
    from cpptlm.topo.layer import ConnectionSpec, ModuleSpec, TopoLayer

__all__ = [
    "PatchAction",
    "Selector",
    "ConnectionSelector",
    "ModuleSelector",
    "LayerSelector",
    "TopoPatch",
    "_glob_to_regex",
]


# ─────────────────────────────────────────────────────────────────
# TopoPatch Exceptions
# ─────────────────────────────────────────────────────────────────

class PatchError(Exception):
    """Patch 应用失败"""


# ─────────────────────────────────────────────────────────────────
# Glob → Regex 转换
# ─────────────────────────────────────────────────────────────────

def _glob_to_regex(pattern: str) -> re.Pattern:
    """
    将 glob 模式转换为正则表达式

    规则：
    - ``*``  → ``.*``  (任意字符序列)
    - ``?``  → ``.``   (单个字符)
    - ``.``  → ``\\.``  (转义字面点)
    - 其他正则特殊字符自动转义
    """
    _REGEX_META = frozenset(".+()[]{}|^$\"'|")
    result = []
    for ch in pattern:
        if ch == "*":
            result.append(".*")
        elif ch == "?":
            result.append(".")
        elif ch in _REGEX_META:
            result.append("\\" + ch)
        else:
            result.append(ch)
    return re.compile("".join(result))


# ─────────────────────────────────────────────────────────────────
# PatchAction 枚举
# ─────────────────────────────────────────────────────────────────

class PatchAction(Enum):
    """补丁操作类型"""
    ADD = "add"
    REMOVE = "remove"
    REPLACE = "replace"
    REWIRE = "rewire"


# ─────────────────────────────────────────────────────────────────
# Selector 抽象
# ─────────────────────────────────────────────────────────────────

class Selector:
    """选择器基类"""

    def match(self, item) -> bool:
        """通用匹配入口"""
        raise NotImplementedError


class ConnectionSelector(Selector):
    """
    连接选择器 - 通过 glob 模式匹配连接的 src 或 dst

    模式匹配规则：
    - ``*`` 匹配任意字符序列
    - ``?`` 匹配单个字符
    - 匹配 src 或 dst 任意一个即可
    """

    def __init__(self, pattern: str):
        self.pattern = pattern
        self._regex = _glob_to_regex(pattern)

    def match(self, item) -> bool:
        """如果 item 是 ConnectionSpec 且 src 或 dst 匹配则返回 True"""
        from cpptlm.topo.layer import ConnectionSpec
        if not isinstance(item, ConnectionSpec):
            return False
        return bool(self._regex.match(item.src) or self._regex.match(item.dst))


class ModuleSelector(Selector):
    """
    模块选择器 - 通过 glob 模式匹配模块名称
    """

    def __init__(self, pattern: str):
        self.pattern = pattern
        self._regex = _glob_to_regex(pattern)

    def match(self, item) -> bool:
        """如果 item 是 ModuleSpec 且 name 匹配则返回 True"""
        from cpptlm.topo.layer import ModuleSpec
        if not isinstance(item, ModuleSpec):
            return False
        return bool(self._regex.match(item.name))


class LayerSelector(Selector):
    """
    层选择器 - 按 name_pattern 和/或 type_pattern 匹配 TopoLayer

    :param name_pattern: glob 模式匹配层名称 (None 表示不检查名称)
    :param type_pattern: glob 模式匹配 metadata["type"] (None 表示不检查类型)
    """

    def __init__(self, name_pattern: Optional[str] = None,
                 type_pattern: Optional[str] = None):
        self.name_pattern = name_pattern
        self.type_pattern = type_pattern
        self._name_regex = _glob_to_regex(name_pattern) if name_pattern else None
        self._type_regex = _glob_to_regex(type_pattern) if type_pattern else None

    def match(self, item) -> bool:
        """如果 item 是 TopoLayer 且名称/类型匹配则返回 True"""
        from cpptlm.topo.layer import TopoLayer
        if not isinstance(item, TopoLayer):
            return False
        if self._name_regex and not self._name_regex.match(item.name):
            return False
        if self._type_regex:
            layer_type = item.metadata.get("type", "")
            if not self._type_regex.match(layer_type):
                return False
        return True


# ─────────────────────────────────────────────────────────────────
# TopoPatch
# ─────────────────────────────────────────────────────────────────

class TopoPatch:
    """
    拓扑补丁 - 局部修改指令

    :param selector: 选择器 (ConnectionSelector / ModuleSelector / LayerSelector)
    :param action: 操作类型 (PatchAction)
    :param template: ADD / REPLACE 时的内容 dict
    :param rewiring: REWIRE 时的端点修改 dict，键为 "src" 和/或 "dst"
    """

    def __init__(self, selector: Selector, action: PatchAction,
                 template: Optional[dict] = None,
                 rewiring: Optional[dict] = None):
        self.selector = selector
        self.action = action
        self.template = template
        self.rewiring = rewiring

    def apply_to(self, layer: "TopoLayer") -> "TopoLayer":
        """
        应用补丁到层，返回修改后的层（可能 in-place 修改）

        行为按 action 分：
        - REMOVE:  移除所有匹配 selector 的连接/模块/sublayer
        - ADD:     将 template 作为新连接/模块/sublayer 添加
        - REPLACE: 将匹配项替换为 template（目前仅连接支持）
        - REWIRE:  修改匹配的连接的 src/dst（rewiring 指定端点）
        """
        from cpptlm.topo.layer import ConnectionSpec, ModuleSpec, TopoLayer

        if isinstance(self.selector, ConnectionSelector):
            return self._apply_connection_patch(layer)
        elif isinstance(self.selector, ModuleSelector):
            return self._apply_module_patch(layer)
        elif isinstance(self.selector, LayerSelector):
            return self._apply_layer_patch(layer)
        return layer

    # ─────────────────────────────────────────────────────────────
    # 内部：连接补丁
    # ─────────────────────────────────────────────────────────────

    def _apply_connection_patch(self, layer: "TopoLayer"):
        """处理连接层面的 ADD / REMOVE / REPLACE / REWIRE"""
        from cpptlm.topo.layer import ConnectionSpec
        matched = [c for c in layer.connections if self.selector.match(c)]

        if self.action == PatchAction.REMOVE:
            for c in matched:
                layer.remove_connection(c.src, c.dst)

        elif self.action == PatchAction.REWIRE and self.rewiring:
            for c in matched:
                if "src" in self.rewiring:
                    c.src = self.rewiring["src"]
                if "dst" in self.rewiring:
                    c.dst = self.rewiring["dst"]

        elif self.action == PatchAction.REPLACE and self.template:
            for c in matched:
                layer.remove_connection(c.src, c.dst)
            layer.connections.append(ConnectionSpec(**self.template))

        elif self.action == PatchAction.ADD and self.template:
            layer.connections.append(ConnectionSpec(**self.template))

        return layer

    # ─────────────────────────────────────────────────────────────
    # 内部：模块补丁
    # ─────────────────────────────────────────────────────────────

    def _apply_module_patch(self, layer: "TopoLayer"):
        """处理模块层面的 ADD / REMOVE / REPLACE"""
        matched = [m for m in layer.modules if self.selector.match(m)]

        if self.action == PatchAction.REMOVE:
            for m in matched:
                layer.remove_module(m.name)

        elif self.action == PatchAction.REPLACE and self.template:
            for m in matched:
                layer.remove_module(m.name)
            layer.modules.append(ModuleSpec(**self.template))

        elif self.action == PatchAction.ADD and self.template:
            layer.modules.append(ModuleSpec(**self.template))

        return layer

    # ─────────────────────────────────────────────────────────────
    # 内部：层补丁
    # ─────────────────────────────────────────────────────────────

    def _apply_layer_patch(self, layer: "TopoLayer"):
        """处理层层面的 ADD / REMOVE"""
        from cpptlm.topo.layer import TopoLayer
        if self.action == PatchAction.ADD and self.template:
            layer.add_sublayer(TopoLayer.from_dict(self.template))

        elif self.action == PatchAction.REMOVE:
            layer.sublayers = [
                s for s in layer.sublayers if not self.selector.match(s)
            ]

        return layer