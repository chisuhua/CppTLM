"""
cpptlm/topo/layer.py - TopoLayer and Specification Dataclasses

Layer-based hierarchical topology representation with chainable API.
Implements TopoLayer, ModuleSpec, ConnectionSpec, CoherenceDomainSpec dataclasses.

Author: CppTLM Team
Created: 2026-05-29
"""

from dataclasses import dataclass, field
from typing import Optional, Union
from enum import Enum
import copy


class ConnectionAction(Enum):
    """连接操作类型"""
    ADD = "add"
    REMOVE = "remove"
    REPLACE = "replace"


@dataclass
class ModuleSpec:
    """模块规格"""
    name: str
    type: str
    params: dict = field(default_factory=dict)
    metadata: dict = field(default_factory=dict)


@dataclass
class ConnectionSpec:
    """连接规格"""
    src: str  # e.g., "cpu0" or "cpu0.0"
    dst: str
    latency: int = 1
    bandwidth: Optional[int] = None
    vc_priorities: list = field(default_factory=list)


@dataclass
class CoherenceDomainSpec:
    """一致性域规格"""
    name: str
    protocol: str = "MESI"  # MESI or MOESI
    members: list = field(default_factory=list)
    bridges: dict = field(default_factory=dict)  # target_domain -> bridge_name


@dataclass
class TopoLayer:
    """
    拓扑层 - 可递归嵌套的基本构件

    用法::

        layer = TopoLayer(name="cluster0")
        layer.add_module(ModuleSpec(name="cpu0", type="CPUTLM"))
        layer.add_connection(ConnectionSpec(src="cpu0", dst="cache", latency=1))
        layer.add_coherence_domain(CoherenceDomainSpec(name="cpu_domain", members=["cpu0"]))
    """

    name: str
    modules: list = field(default_factory=list)
    connections: list = field(default_factory=list)
    coherence_domains: list = field(default_factory=list)
    sublayers: list = field(default_factory=list)
    metadata: dict = field(default_factory=dict)
    tags: set = field(default_factory=set)

    # ─────────────────────────────────────────────────────────────────
    # 模块操作
    # ─────────────────────────────────────────────────────────────────

    def add_module(self, name: str, type: str, **params) -> "TopoLayer":
        """添加模块 (链式调用)"""
        self.modules.append(ModuleSpec(name=name, type=type, params=params))
        return self

    def add_modules(self, *modules: ModuleSpec) -> "TopoLayer":
        """批量添加模块"""
        self.modules.extend(modules)
        return self

    def get_module(self, name: str) -> Optional[ModuleSpec]:
        """按名称查找模块"""
        for m in self.modules:
            if m.name == name:
                return m
        for sub in self.sublayers:
            found = sub.get_module(name)
            if found:
                return found
        return None

    def remove_module(self, name: str) -> "TopoLayer":
        """移除模块及其连接"""
        self.modules = [m for m in self.modules if m.name != name]
        self.connections = [c for c in self.connections
                           if c.src != name and c.dst != name]
        return self

    def _all_modules(self) -> list:
        """递归收集当前层及所有子层中的所有模块（含 ModuleSpec 引用）。"""
        result = list(self.modules)
        for sub in self.sublayers:
            result.extend(sub._all_modules())
        return result

    def _all_sublayers(self) -> list:
        """递归收集所有子层（不包含 self）。"""
        result = []
        for sub in self.sublayers:
            result.append(sub)
            result.extend(sub._all_sublayers())
        return result

    def tag(self, *tags: str) -> "TopoLayer":
        """为当前层添加 Python 端标签（链式调用）。

        标签在 emit 时会展开为顶层 `groups` 字典（C++ `group:` 前缀消费）。
        标签仅在 Python 端维护，不会出现在 JSON 中。
        """
        self.tags.update(tags)
        return self

    def clone(self, prefix: str = None) -> "TopoLayer":
        """深拷贝当前层（递归所有子层）。

        若提供 `prefix`，所有模块名和连接 src/dst 都会加上 `<prefix>_` 前缀；
        新层 `name` 设为 `prefix`。否则 `name` 保持 `f"{self.name}_clone"`。
        副本与原对象**无共享引用**（含 params / metadata / sublayers）。
        """
        import copy
        new = copy.deepcopy(self)
        if prefix is not None:
            new.name = prefix
            for layer in [new] + new._all_sublayers():
                for m in layer.modules:
                    m.name = f"{prefix}_{m.name}"
                for c in layer.connections:
                    c.src = f"{prefix}_{c.src}"
                    c.dst = f"{prefix}_{c.dst}"
        else:
            new.name = f"{self.name}_clone"
        return new

    def layout_grid(self, dx: int, dy: int,
                    x_offset: int = 0, y_offset: int = 0) -> "TopoLayer":
        """自动为所有模块计算 layout.x/y 坐标（递归子层，链式调用）。

        坐标公式: col = i % dx, row = i // dx; layout = (x_offset + col*100, y_offset + row*100)。
        写入 `module.metadata["layout"]`，由 CxxCompatibleEmitter 提取到 JSON 的 `layout` 字段。
        `dx` 控制列数；`dy` 是行数提示（不强制截断，超出会自动扩展行）。
        """
        all_modules = self._all_modules()
        for i, m in enumerate(all_modules):
            col = i % dx
            row = i // dx
            layout = {"x": x_offset + col * 100, "y": y_offset + row * 100}
            m.metadata = {**m.metadata, "layout": layout}
        return self

    # ─────────────────────────────────────────────────────────────────
    # 连接操作
    # ─────────────────────────────────────────────────────────────────

    def add_connection(self, src: str, dst: str, latency: int = 1,
                       bandwidth: Optional[int] = None) -> "TopoLayer":
        """添加连接 (链式调用)"""
        self.connections.append(ConnectionSpec(src=src, dst=dst, latency=latency,
                                               bandwidth=bandwidth))
        return self

    def remove_connection(self, src: str, dst: str) -> "TopoLayer":
        """移除连接"""
        self.connections = [c for c in self.connections
                           if not (c.src == src and c.dst == dst)]
        return self

    def rewire(self, old_src: str, old_dst: str, new_src: str, new_dst: str) -> "TopoLayer":
        """重连 (修改连接端点)"""
        for conn in self.connections:
            if conn.src == old_src and conn.dst == old_dst:
                conn.src = new_src
                conn.dst = new_dst
        return self

    # ─────────────────────────────────────────────────────────────────
    # 子层操作
    # ─────────────────────────────────────────────────────────────────

    def add_sublayer(self, layer: "TopoLayer") -> "TopoLayer":
        """嵌套子层"""
        self.sublayers.append(layer)
        return self

    def get_sublayer(self, name: str) -> Optional["TopoLayer"]:
        """按名称查找子层"""
        for sub in self.sublayers:
            if sub.name == name:
                return sub
        return None

    # ─────────────────────────────────────────────────────────────────
    # 一致性域操作
    # ─────────────────────────────────────────────────────────────────

    def add_coherence_domain(self, name: str, protocol: str = "MESI",
                            members: list = None) -> "TopoLayer":
        """添加一致性域"""
        self.coherence_domains.append(CoherenceDomainSpec(
            name=name, protocol=protocol, members=members or []
        ))
        return self

    # ─────────────────────────────────────────────────────────────────
    # 序列化
    # ─────────────────────────────────────────────────────────────────

    def to_dict(self) -> dict:
        """导出为字典 (用于 JSON)"""
        result = {
            "name": self.name,
            "modules": [
                {"name": m.name, "type": m.type, **({"params": m.params} if m.params else {})}
                for m in self.modules
            ],
            "connections": [
                {"src": c.src, "dst": c.dst, "latency": c.latency,
                 **({"bandwidth": c.bandwidth} if c.bandwidth else {})}
                for c in self.connections
            ]
        }
        if self.coherence_domains:
            result["coherence_domains"] = [
                {"name": d.name, "protocol": d.protocol, "members": d.members}
                for d in self.coherence_domains
            ]
        if self.sublayers:
            result["sublayers"] = [sub.to_dict() for sub in self.sublayers]
        return result

    @classmethod
    def from_dict(cls, data: dict) -> "TopoLayer":
        """从字典创建"""
        layer = cls(name=data["name"])
        for m in data.get("modules", []):
            params = m.pop("params", {})
            layer.add_module(name=m["name"], type=m["type"], **params)
        for c in data.get("connections", []):
            layer.add_connection(c["src"], c["dst"], latency=c.get("latency", 1),
                                bandwidth=c.get("bandwidth"))
        for d in data.get("coherence_domains", []):
            layer.add_coherence_domain(d["name"], protocol=d.get("protocol", "MESI"),
                                      members=d.get("members", []))
        for sub_data in data.get("sublayers", []):
            layer.add_sublayer(cls.from_dict(sub_data))
        return layer

    # ─────────────────────────────────────────────────────────────────
    # 组合操作
    # ─────────────────────────────────────────────────────────────────

    def flatten(self) -> "TopoLayer":
        """展平所有子层到当前层 (递归收集)"""
        result = TopoLayer(name=self.name)
        result.modules = list(self.modules)
        result.connections = list(self.connections)
        result.coherence_domains = list(self.coherence_domains)
        result.metadata = dict(self.metadata)
        # Recursively flatten sublayers
        for sub in self.sublayers:
            flat_sub = sub.flatten()
            result.modules.extend(flat_sub.modules)
            result.connections.extend(flat_sub.connections)
            result.coherence_domains.extend(flat_sub.coherence_domains)
        result.sublayers = []
        return result

    def merge(self, other: "TopoLayer") -> "TopoLayer":
        """合并另一个层"""
        result = copy.deepcopy(self)
        result.modules.extend(other.modules)
        result.connections.extend(other.connections)
        result.coherence_domains.extend(other.coherence_domains)
        return result
