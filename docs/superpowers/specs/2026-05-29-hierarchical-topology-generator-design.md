# HierarchicalTopologyGenerator 设计文档

> **Document ID**: IMPL-010-v4.1
> **Version**: 4.1
> **Date**: 2026-05-29
> **Status**: 🔄 Draft
> **Author**: Based on Oracle architecture review + brainstorming

---

## 1. 设计目标

### 1.1 用户需求

| # | 需求 | 优先级 |
|---|------|--------|
| 1 | 模块化生成：顶层生成新 JSON，子模块用已有配置拼装 | P0 |
| 2 | 局部重生成：只重连特定模块的连接，其他不变 | P0 |
| 3 | NoC 专项调整：只调整网络拓扑，CPU/Memory 等保持不变 | P1 |
| 4 | 连接模板扩展：基于模板继承扩展连接 | P1 |
| 5 | 多拓扑对比：一次生成多个拓扑变体，用于性能比较 | P2 |

### 1.2 设计原则

1. **分层 API (Option B)**: Level 1 底层 Builder + Level 2 高级编排
2. **Layer + Patch 模式**: 分层定义 + 补丁修改，而非继承覆盖
3. **最小惊讶原则**: API 直观，行为可预测
4. **可组合性**: 小部件可复用、可嵌套

---

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                    TopoOrchestrator (Level 2 - 编排层)                │
│  • 多拓扑变体生成                                                      │
│  • 局部修改 (Patch) 应用                                              │
│  • 模板继承 + 扩展                                                    │
│  • CLI 入口                                                          │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    TopoLayer (Level 1 - 基础层)                      │
│  • 模块定义 (ModuleSpec)                                             │
│  • 连接定义 (ConnectionSpec)                                         │
│  • 子层嵌套 (sublayers)                                              │
│  • coherence_domains                                                 │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    TopoPatch (修改机制)                               │
│  • selector: glob/正则选择目标                                      │
│  • action: ADD / REMOVE / REPLACE /REWIRE                           │
│  • template: 新内容                                                 │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 核心组件设计

### 3.1 TopoLayer — 拓扑层（基础构件）

```python
# cpptlm/topo/layer.py

from dataclasses import dataclass, field
from typing import Optional
from enum import Enum
import copy

class ConnectionAction(Enum):
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
    src: str          # e.g., "cpu0" or "cpu0.0"
    dst: str
    latency: int = 1
    bandwidth: Optional[int] = None
    vc_priorities: list[int] = field(default_factory=list)

@dataclass
class CoherenceDomainSpec:
    """一致性域规格"""
    name: str
    protocol: str = "MESI"  # MESI or MOESI
    members: list[str] = field(default_factory=list)
    bridges: dict[str, str] = field(default_factory=dict)  # target_domain -> bridge_name

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
    modules: list[ModuleSpec] = field(default_factory=list)
    connections: list[ConnectionSpec] = field(default_factory=list)
    coherence_domains: list[CoherenceDomainSpec] = field(default_factory=list)
    sublayers: list[TopoLayer] = field(default_factory=list)
    metadata: dict = field(default_factory=dict)

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
        self.connections = [c for c in self.connections if c.src != name and c.dst != name]
        return self

    # ─────────────────────────────────────────────────────────────────
    # 连接操作
    # ─────────────────────────────────────────────────────────────────

    def add_connection(self, src: str, dst: str, latency: int = 1,
                       bandwidth: Optional[int] = None) -> "TopoLayer":
        """添加连接 (链式调用)"""
        self.connections.append(ConnectionSpec(src=src, dst=dst, latency=latency, bandwidth=bandwidth))
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
                            members: list[str] = None) -> "TopoLayer":
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
                {"src": c.src, "dst": cdst, "latency": c.latency,
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
        """展平所有子层到当前层"""
        result = copy.deepcopy(self)
        result.sublayers = []
        return result

    def merge(self, other: "TopoLayer") -> "TopoLayer":
        """合并另一个层"""
        result = copy.deepcopy(self)
        result.modules.extend(other.modules)
        result.connections.extend(other.connections)
        result.coherence_domains.extend(other.coherence_domains)
        return result
```

---

### 3.2 TopoPatch — 局部修改机制

```python
# cpptlm/topo/patch.py

from dataclasses import dataclass, field
from typing import Optional, Callable
from enum import Enum
import re

class PatchAction(Enum):
    ADD = "add"
    REMOVE = "remove"
    REPLACE = "replace"
    REWIRE = "rewire"

@dataclass
class TopoPatch:
    """
    拓扑补丁 - 局部修改指令

    用法::

        # 移除所有到 xbar 的连接
        patch = TopoPatch(
            selector=ConnectionSelector(pattern="*.xbar.*"),
            action=PatchAction.REMOVE
        )

        # 重连 cpu0 -> cache 为 cpu0 -> new_cache
        patch = TopoPatch(
            selector=ConnectionSelector(pattern="cpu0.cache"),
            action=PatchAction.REWIRE,
            rewiring={src: "cpu0", dst: "new_cache"}
        )

        # 添加新 NoC 连接
        patch = TopoPatch(
            selector=LayerSelector(name="noc"),
            action=PatchAction.ADD,
            template=ConnectionSpec(src="r0", dst="r1", latency=1)
        )
    """

    selector: "Selector"
    action: PatchAction
    template: Optional[dict] = None
    rewiring: Optional[dict] = None  # {src: new_src, dst: new_dst}

    def apply_to(self, layer: TopoLayer) -> TopoLayer:
        """应用补丁到层"""
        if isinstance(self.selector, ConnectionSelector):
            return self._apply_connection_patch(layer)
        elif isinstance(self.selector, ModuleSelector):
            return self._apply_module_patch(layer)
        elif isinstance(self.selector, LayerSelector):
            return self._apply_layer_patch(layer)
        return layer

    def _apply_connection_patch(self, layer: TopoLayer) -> TopoLayer:
        """应用连接补丁"""
        matched = []
        for conn in layer.connections:
            if self.selector.match_connection(conn):
                matched.append(conn)

        if self.action == PatchAction.REMOVE:
            for conn in matched:
                layer.remove_connection(conn.src, conn.dst)
        elif self.action == PatchAction.REWIRE and self.rewiring:
            for conn in matched:
                if "src" in self.rewiring:
                    conn.src = self.rewiring["src"]
                if "dst" in self.rewiring:
                    conn.dst = self.rewiring["dst"]
        elif self.action == PatchAction.REPLACE and self.template:
            # 替换逻辑
            pass
        elif self.action == PatchAction.ADD and self.template:
            layer.connections.append(ConnectionSpec(**self.template))
        return layer

    def _apply_module_patch(self, layer: TopoLayer) -> TopoLayer:
        """应用模块补丁"""
        matched = [m for m in layer.modules if self.selector.match_module(m)]
        if self.action == PatchAction.REMOVE:
            for m in matched:
                layer.remove_module(m.name)
        return layer

    def _apply_layer_patch(self, layer: TopoLayer) -> TopoLayer:
        """应用层补丁"""
        if self.action == PatchAction.ADD and self.template:
            layer.add_sublayer(TopoLayer.from_dict(self.template))
        elif self.action == PatchAction.REMOVE:
            layer.sublayers = [s for s in layer.sublayers
                              if not self.selector.match_layer(s)]
        return layer


# ─────────────────────────────────────────────────────────────────
# Selector 抽象
# ─────────────────────────────────────────────────────────────────

class Selector:
    """选择器基类"""
    def match_module(self, module: ModuleSpec) -> bool:
        raise NotImplementedError
    def match_connection(self, conn: ConnectionSpec) -> bool:
        raise NotImplementedError
    def match_layer(self, layer: TopoLayer) -> bool:
        raise NotImplementedError

class ConnectionSelector(Selector):
    """连接选择器"""
    def __init__(self, pattern: str):
        self.pattern = pattern
        # 支持 glob 模式: "cpu*.cache", "*.xbar.*"
        self._regex = re.compile(pattern.replace("*", ".*").replace("?", "."))

    def match_connection(self, conn: ConnectionSpec) -> bool:
        src_match = self._regex.match(conn.src)
        dst_match = self._regex.match(conn.dst)
        return src_match or dst_match

class ModuleSelector(Selector):
    """模块选择器"""
    def __init__(self, pattern: str):
        self.pattern = pattern
        self._regex = re.compile(pattern.replace("*", ".*"))

    def match_module(self, module: ModuleSpec) -> bool:
        return bool(self._regex.match(module.name))

class LayerSelector(Selector):
    """层选择器"""
    def __init__(self, name: str = None, type: str = None):
        self.name = name
        self.type = type

    def match_layer(self, layer: TopoLayer) -> bool:
        if self.name and layer.name != self.name:
            return False
        if self.type and layer.metadata.get("type") != self.type:
            return False
        return True

    def match_module(self, module: ModuleSpec) -> bool:
        return False
    def match_connection(self, conn: ConnectionSpec) -> bool:
        return False
```

---

### 3.3 TopoVariant — 拓扑变体

```python
# cpptlm/topo/variant.py

from dataclasses import dataclass, field
from typing import Optional, Callable
from .layer import TopoLayer
from .patch import TopoPatch

@dataclass
class TopoVariant:
    """
    拓扑变体 - 基于基拓扑的差异化配置

    用法::

        variant = TopoVariant(
            name="low_latency",
            base=base_layer,
            patches=[
                TopoPatch(
                    selector=ConnectionSelector(pattern="cpu*.cache"),
                    action=PatchAction.REWIRE,
                    rewiring={dst: "l2_cache"}
                )
            ],
            metrics=["latency", "bandwidth"]
        )
    """

    name: str
    base: TopoLayer
    patches: list[TopoPatch] = field(default_factory=list)
    metrics: list[str] = field(default_factory=list)

    def build(self) -> TopoLayer:
        """构建变体拓扑"""
        result = TopoLayer(
            name=f"{self.base.name}_{self.name}",
            metadata={**self.base.metadata, "variant": self.name}
        )
        # 深拷贝基拓扑
        import copy
        result.modules = copy.deepcopy(self.base.modules)
        result.connections = copy.deepcopy(self.base.connections)
        result.coherence_domains = copy.deepcopy(self.base.coherence_domains)

        # 应用补丁
        for patch in self.patches:
            result = patch.apply_to(result)
        return result


@dataclass
class TopoVariantSet:
    """
    拓扑变体集 - 管理多个变体用于比较

    用法::

        set = TopoVariantSet(name="soc_variants")
        set.add_variant(variant_a)
        set.add_variant(variant_b)
        set.add_variant(variant_c)

        # 批量构建
        for name, layer in set.build_all().items():
            layer.save(f"configs/{name}.json")
    """

    name: str
    variants: list[TopoVariant] = field(default_factory=list)

    def add_variant(self, variant: TopoVariant) -> "TopoVariantSet":
        self.variants.append(variant)
        return self

    def build_all(self) -> dict[str, TopoLayer]:
        """构建所有变体"""
        return {v.name: v.build() for v in self.variants}

    def compare(self) -> dict:
        """比较变体差异"""
        all_layers = self.build_all()
        comparison = {}
        for name, layer in all_layers.items():
            comparison[name] = {
                "module_count": len(layer.modules),
                "connection_count": len(layer.connections),
                "hierarchy_depth": self._get_depth(layer),
            }
        return comparison

    def _get_depth(self, layer: TopoLayer, depth: int = 0) -> int:
        if not layer.sublayers:
            return depth
        return max(self._get_depth(sub, depth + 1) for sub in layer.sublayers)
```

---

### 3.4 TopoOrchestrator — 高级编排层

```python
# cpptlm/topo/orchestrator.py

from typing import Optional, Callable
from .layer import TopoLayer
from .variant import TopoVariant, TopoVariantSet
from .patch import TopoPatch, PatchAction, ConnectionSelector, ModuleSelector

class TopoOrchestrator:
    """
    拓扑编排器 - 高级编排 API

    用法::

        orch = TopoOrchestrator("my_soc")

        # 方式 1: 从零开始构建
        orch.add_layer("system", layers=[
            orch.layer("cluster0", modules=[...]),
            orch.layer("noc", modules=[...])
        ])

        # 方式 2: 从已有配置加载
        orch.load("configs/base.json")

        # 方式 3: 生成变体
        orch.add_variant("mesh", base="system", patches=[...])
        orch.add_variant("torus", base="system", patches=[...])

        # 方式 4: 局部修改
        orch.patch("system", selector="*.noc.*", action=PatchAction.REMOVE)

        # 导出
        orch.save_all("configs/variants/")
    """

    def __init__(self, name: str):
        self.name = name
        self._layers: dict[str, TopoLayer] = {}
        self._variants: dict[str, TopoVariantSet] = {}
        self._registry: dict[str, str] = {}  # name -> config_path

    # ─────────────────────────────────────────────────────────────────
    # 工厂方法
    # ─────────────────────────────────────────────────────────────────

    @staticmethod
    def layer(name: str) -> TopoLayer:
        """创建新层"""
        return TopoLayer(name=name)

    @staticmethod
    def patch(selector: str, action: PatchAction, **kwargs) -> TopoPatch:
        """创建补丁"""
        if "*" in selector or "?" in selector:
            sel = ConnectionSelector(pattern=selector)
        else:
            sel = ModuleSelector(pattern=selector)
        return TopoPatch(selector=sel, action=action, **kwargs)

    # ─────────────────────────────────────────────────────────────────
    # 层操作
    # ─────────────────────────────────────────────────────────────────

    def add_layer(self, name: str, layers: list[TopoLayer] = None) -> "TopoOrchestrator":
        """添加层到系统"""
        if layers:
            composite = TopoLayer(name=name)
            for l in layers:
                composite.add_sublayer(l)
            self._layers[name] = composite
        else:
            self._layers[name] = TopoLayer(name=name)
        return self

    def get_layer(self, name: str) -> Optional[TopoLayer]:
        return self._layers.get(name)

    def remove_layer(self, name: str) -> "TopoOrchestrator":
        if name in self._layers:
            del self._layers[name]
        return self

    def load(self, path: str, name: str = None) -> "TopoOrchestrator":
        """从 JSON 文件加载层"""
        import json
        with open(path) as f:
            data = json.load(f)
        layer_name = name or data.get("name", path)
        self._layers[layer_name] = TopoLayer.from_dict(data)
        self._registry[layer_name] = path
        return self

    def save(self, path: str, name: str = None) -> "TopoOrchestrator":
        """保存层到 JSON 文件"""
        layer_name = name or list(self._layers.keys())[0]
        layer = self._layers.get(layer_name)
        if not layer:
            raise ValueError(f"Layer '{layer_name}' not found")
        import json
        with open(path, "w") as f:
            json.dump(layer.to_dict(), f, indent=2)
        return self

    # ─────────────────────────────────────────────────────────────────
    # 变体操作
    # ─────────────────────────────────────────────────────────────────

    def add_variant(self, name: str, base: str,
                    patches: list[TopoPatch] = None,
                    metrics: list[str] = None) -> "TopoOrchestrator":
        """添加拓扑变体"""
        base_layer = self._layers.get(base)
        if not base_layer:
            raise ValueError(f"Base layer '{base}' not found")
        if "variants" not in self._variants:
            self._variants["default"] = TopoVariantSet(name=f"{self.name}_variants")
        self._variants["default"].add_variant(TopoVariant(
            name=name,
            base=base_layer,
            patches=patches or [],
            metrics=metrics or []
        ))
        return self

    def build_variant(self, name: str) -> Optional[TopoLayer]:
        """构建单个变体"""
        if "default" not in self._variants:
            return None
        for v in self._variants["default"].variants:
            if v.name == name:
                return v.build()
        return None

    def build_all_variants(self) -> dict[str, TopoLayer]:
        """构建所有变体"""
        if "default" in self._variants:
            return self._variants["default"].build_all()
        return {}

    # ─────────────────────────────────────────────────────────────────
    # 模板操作
    # ─────────────────────────────────────────────────────────────────

    def create_template(self, name: str, base: str,
                       modify: Callable[[TopoLayer], None]) -> "TopoOrchestrator":
        """
        基于模板创建新拓扑

        用法::

            orch.create_template("mesh_4x4", "mesh_2x2",
                modify=lambda l: l.add_module("cpu4", "CPUTLM")
            )
        """
        base_layer = self._layers.get(base)
        if not base_layer:
            raise ValueError(f"Base layer '{base}' not found")
        import copy
        new_layer = copy.deepcopy(base_layer)
        new_layer.name = name
        modify(new_layer)
        self._layers[name] = new_layer
        return self

    # ─────────────────────────────────────────────────────────────────
    # NoC 专项操作
    # ─────────────────────────────────────────────────────────────────

    def rewire_noc(self, old_topology: str, new_topology: str,
                   router_type: str = "RouterTLM") -> "TopoOrchestrator":
        """
        重连 NoC 拓扑

        用法::

            orch.rewire_noc("mesh", "torus")
            # 保留 cpu*, mem* 等模块，只修改 router* 连接
        """
        noc_layer = self._layers.get(old_topology)
        if not noc_layer:
            return self

        import copy
        new_noc = copy.deepcopy(noc_layer)
        new_noc.name = new_topology

        # 移除旧的 router 连接
        new_noc.connections = [
            c for c in new_noc.connections
            if not ("router" in c.src or "router" in c.dst)
        ]

        # 根据新拓扑类型生成连接
        if new_topology == "torus":
            new_noc.connections.extend(self._generate_torus_links(new_noc, router_type))
        elif new_topology == "mesh":
            new_noc.connections.extend(self._generate_mesh_links(new_noc, router_type))

        self._layers[new_topology] = new_noc
        return self

    def _generate_mesh_links(self, layer: TopoLayer, router_type: str) -> list:
        """生成 mesh 连接"""
        routers = [m for m in layer.modules if router_type in m.type]
        connections = []
        for r in routers:
            coords = self._parse_router_coords(r.name)
            if not coords:
                continue
            r_x, r_y = coords
            # 四个方向连接
            for dx, dy in [(0, 1), (1, 0)]:
                neighbor = self._find_router_at(r_x + dx, r_y + dy, routers)
                if neighbor:
                    connections.append(ConnectionSpec(src=r.name, dst=neighbor.name, latency=1))
        return connections

    def _generate_torus_links(self, layer: TopoLayer, router_type: str) -> list:
        """生成 torus 连接"""
        # 类似 mesh 但添加环形 wrap-around
        pass

    def _parse_router_coords(self, name: str) -> tuple:
        """解析路由器坐标"""
        import re
        m = re.search(r"router_(\d+)_(\d+)", name)
        if m:
            return (int(m.group(1)), int(m.group(2)))
        return None

    def _find_router_at(self, x: int, y: int, routers: list) -> Optional[ModuleSpec]:
        """查找指定坐标的路由器"""
        for r in routers:
            coords = self._parse_router_coords(r.name)
            if coords and coords == (x, y):
                return r
        return None
```

---

## 4. CLI 接口设计

```python
# cpptlm/topo/cli.py

import argparse
import json
import sys

def main():
    parser = argparse.ArgumentParser(description="CppTLM Hierarchical Topology Generator")

    subparsers = parser.add_subparsers(dest="command", help="Commands")

    # gen 命令 - 生成单个或多个拓扑
    gen_parser = subparsers.add_parser("gen", help="Generate topology")
    gen_parser.add_argument("--name", "-n", required=True, help="Topology name")
    gen_parser.add_argument("--template", "-t", help="Template file or name")
    gen_parser.add_argument("--output", "-o", help="Output file")
    gen_parser.add_argument("--noc", choices=["mesh", "torus", "ring", "crossbar"],
                            help="NoC topology type")
    gen_parser.add_argument("--clusters", "-c", type=int, help="Number of clusters")
    gen_parser.add_argument("--cpus-per-cluster", type=int, help="CPUs per cluster")

    # variant 命令 - 生成变体
    variant_parser = subparsers.add_parser("variant", help="Generate topology variants")
    variant_parser.add_argument("--base", "-b", required=True, help="Base topology")
    variant_parser.add_argument("--variants", "-v", nargs="+", required=True,
                               help="Variant names")
    variant_parser.add_argument("--output-dir", "-o", required=True, help="Output directory")

    # patch 命令 - 应用补丁
    patch_parser = subparsers.add_parser("patch", help="Apply patch to topology")
    patch_parser.add_argument("--input", "-i", required=True, help="Input topology")
    patch_parser.add_argument("--output", "-o", required=True, help="Output topology")
    patch_parser.add_argument("--selector", "-s", required=True, help="Selection pattern")
    patch_parser.add_argument("--action", "-a", required=True,
                             choices=["add", "remove", "replace", "rewire"],
                             help="Patch action")
    patch_parser.add_argument("--template", "-t", help="Template file for add/replace")

    # compare 命令 - 比较变体
    compare_parser = subparsers.add_parser("compare", help="Compare topology variants")
    compare_parser.add_argument("--base", "-b", required=True, help="Base topology")
    compare_parser.add_argument("--variants", "-v", nargs="+", required=True,
                              help="Variant names")

    args = parser.parse_args()

    if args.command == "gen":
        from .orchestrator import TopoOrchestrator
        orch = TopoOrchestrator(args.name)

        if args.template:
            orch.load(args.template)

        if args.noc:
            from .layer import TopoLayer
            noc_layer = TopoLayer(name="noc")
            # 根据 --noc 类型生成
            orch.add_layer("noc", layers=[noc_layer])

        if args.clusters and args.cpus_per_cluster:
            for i in range(args.clusters):
                cluster = orch.layer(f"cluster{i}")
                for j in range(args.cpus_per_cluster):
                    cluster.add_module(f"cpu{i}_{j}", "CPUTLM")
                orch.add_layer(f"cluster{i}", layers=[cluster])

        if args.output:
            orch.save(args.output)
        else:
            print(json.dumps(orch.get_layer(args.name).to_dict(), indent=2))

    elif args.command == "variant":
        # 变体生成逻辑
        pass

    elif args.command == "patch":
        # 补丁应用逻辑
        pass

    elif args.command == "compare":
        # 比较逻辑
        pass

if __name__ == "__main__":
    main()
```

---

## 5. 使用示例

### 5.1 基础用法

```python
from cpptlm.topo import TopoOrchestrator, TopoLayer, TopoPatch

# 创建系统
orch = TopoOrchestrator("my_soc")

# 添加 CPU 集群层
cluster = TopoLayer(name="cluster0")
cluster.add_module("cpu0", "CPUTLM")
cluster.add_module("cpu1", "CPUTLM")
cluster.add_module("l2_cache", "CacheTLM")
cluster.add_connection("cpu0", "l2_cache", latency=1)
cluster.add_connection("cpu1", "l2_cache", latency=1)
cluster.add_coherence_domain("cpu_domain", "MESI", ["cpu0", "cpu1"])
orch.add_layer("cluster0", layers=[cluster])

# 保存
orch.save("configs/my_soc.json")
```

### 5.2 NoC 局部重生成

```python
# 只修改 NoC 拓扑，保留其他层
orch.rewire_noc("noc_mesh", "noc_torus")
```

### 5.3 模板继承

```python
# 基于模板创建变体，只修改特定部分
orch.create_template("mesh_4x4", base="mesh_2x2",
    modify=lambda l: l.add_module("cpu4", "CPUTLM")
)
)
```

### 5.4 多拓扑对比

```python
from cpptlm.topo import TopoOrchestrator, TopoVariant

orch = TopoOrchestrator("soc_comparison")
orch.load("configs/base_soc.json")

# 添加变体
orch.add_variant("low_latency", base="base_soc",
    patches=[
        orch.patch("*.cache.*", action="rewire", dst="l2_fast")
    ]
)

orch.add_variant("high_bandwidth", base="base_soc",
    patches=[
        orch.patch("*.link.*", action="replace", template={bandwidth: 200})
    ]
)

# 构建所有变体
all_variants = orch.build_all_variants()
for name, layer in all_variants.items():
    layer.save(f"configs/variants/{name}.json")
```

---

## 6. 与现有代码的集成

| 现有组件 | 集成方式 |
|---------|---------|
| `TopologyGenerator` (scripts/) | 保留，通过 `TopologyAdapter` 转换 |
| `ConfigBuilder` (cpptlm_config/) | Level 1 Builder 扩展，添加 `add_layer()` 等方法 |
| `mergeConfigs` (C++) | 保持作为低级别合并引擎 |
| `TopologyAdapter` | 已有的适配器模式复用 |

---

## 7. 文件结构

```
cpptlm/
├── __init__.py
├── topo/
│   ├── __init__.py
│   ├── layer.py          # TopoLayer, ModuleSpec, ConnectionSpec
│   ├── patch.py          # TopoPatch, Selector
│   ├── variant.py        # TopoVariant, TopoVariantSet
│   ├── orchestrator.py    # TopoOrchestrator
│   └── cli.py            # CLI 入口
└── ...
```

---

## 8. 下一步

1. **确认设计**: 用户确认后进入实现
2. **实现顺序**:
   - Phase 1: `TopoLayer` + 基础序列化
   - Phase 2: `TopoPatch` + Selector
   - Phase 3: `TopoVariant` + `TopoVariantSet`
   - Phase 4: `TopoOrchestrator`
   - Phase 5: CLI 集成
   - Phase 6: 与现有 `TopologyGenerator` 集成

---

## 附录: 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| API 分层 | Option B (Level 1 + Level 2) | 灵活，兼顾简单和复杂场景 |
| 修改机制 | Layer + Patch | 可局部修改，支持模板扩展 |
| 多变体 | TopoVariantSet | 支持批量生成和比较 |
| CLI | 统一入口 | 与 Python API 共用同一逻辑 |
| 与现有代码 | 适配器模式 | 最小改动，最大复用 |