"""
cpptlm/topo/orchestrator.py — TopoOrchestrator: 高级编排层
"""

from typing import Optional, Callable, Dict, List
import json
import copy
import re

from cpptlm.topo.layer import TopoLayer, ConnectionSpec
from cpptlm.topo.variant import TopoVariant, TopoVariantSet
from cpptlm.topo.patch import TopoPatch, PatchAction, ConnectionSelector, ModuleSelector


class TopoOrchestrator:
    def __init__(self, name: str):
        self.name = name
        self._layers: Dict[str, TopoLayer] = {}
        self._variants: Dict[str, TopoVariantSet] = {}
        self._registry: Dict[str, str] = {}

    @staticmethod
    def layer(name: str) -> TopoLayer:
        return TopoLayer(name=name)

    @staticmethod
    def patch(selector: str, action: PatchAction, **kwargs) -> TopoPatch:
        if "*" in selector or "?" in selector:
            sel = ConnectionSelector(pattern=selector)
        else:
            sel = ModuleSelector(pattern=selector)
        return TopoPatch(selector=sel, action=action, **kwargs)

    def add_layer(self, name: str, layers: List[TopoLayer] = None) -> "TopoOrchestrator":
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
        with open(path) as f:
            data = json.load(f)
        layer_name = name or data.get("name", path)
        self._layers[layer_name] = TopoLayer.from_dict(data)
        self._registry[layer_name] = path
        return self

    def save(self, path: str, name: str = None) -> "TopoOrchestrator":
        layer_name = name or list(self._layers.keys())[0]
        layer = self._layers.get(layer_name)
        if not layer:
            raise ValueError(f"Layer '{layer_name}' not found")
        with open(path, "w") as f:
            json.dump(layer.to_dict(), f, indent=2)
        return self

    def add_variant(self, name: str, base: str,
                    patches: List[TopoPatch] = None,
                    metrics: List[str] = None) -> "TopoOrchestrator":
        base_layer = self._layers.get(base)
        if not base_layer:
            raise ValueError(f"Base layer '{base}' not found")
        if "default" not in self._variants:
            self._variants["default"] = TopoVariantSet(name=f"{self.name}_variants")
        self._variants["default"].add_variant(TopoVariant(
            name=name,
            base=base_layer,
            patches=patches or [],
            metrics=metrics or []
        ))
        return self

    def build_variant(self, name: str) -> Optional[TopoLayer]:
        if "default" not in self._variants:
            return None
        for v in self._variants["default"].variants:
            if v.name == name:
                return v.build()
        return None

    def build_all_variants(self) -> Dict[str, TopoLayer]:
        if "default" in self._variants:
            return self._variants["default"].build_all()
        return {}

    def create_template(self, name: str, base: str,
                       modify: Callable[[TopoLayer], None]) -> "TopoOrchestrator":
        base_layer = self._layers.get(base)
        if not base_layer:
            raise ValueError(f"Base layer '{base}' not found")
        new_layer = base_layer.flatten()
        new_layer.name = name
        modify(new_layer)
        self._layers[name] = new_layer
        return self

    def rewire_noc(self, old_topology: str, new_topology: str,
                   router_type: str = "RouterTLM") -> "TopoOrchestrator":
        noc_layer = self._layers.get(old_topology)
        if not noc_layer:
            return self

        new_noc = copy.deepcopy(noc_layer)
        new_noc.name = new_topology

        new_noc.connections = [
            c for c in new_noc.connections
            if not ("router" in c.src or "router" in c.dst)
        ]

        if new_topology == "torus":
            new_noc.connections.extend(self._generate_torus_links(new_noc, router_type))
        elif new_topology == "mesh":
            new_noc.connections.extend(self._generate_mesh_links(new_noc, router_type))

        self._layers[new_topology] = new_noc
        return self

    def _generate_mesh_links(self, layer: TopoLayer, router_type: str) -> list:
        routers = [m for m in layer.modules if router_type in m.type]
        connections = []
        for r in routers:
            coords = self._parse_router_coords(r.name)
            if not coords:
                continue
            r_x, r_y = coords
            for dx, dy in [(0, 1), (1, 0)]:
                neighbor = self._find_router_at(r_x + dx, r_y + dy, routers)
                if neighbor:
                    connections.append(ConnectionSpec(src=r.name, dst=neighbor.name, latency=1))
        return connections

    def _generate_torus_links(self, layer: TopoLayer, router_type: str) -> list:
        routers = [m for m in layer.modules if router_type in m.type]
        connections = []
        for r in routers:
            coords = self._parse_router_coords(r.name)
            if not coords:
                continue
            r_x, r_y = coords
            for dx, dy in [(0, 1), (1, 0)]:
                neighbor = self._find_router_at(r_x + dx, r_y + dy, routers)
                if neighbor:
                    connections.append(ConnectionSpec(src=r.name, dst=neighbor.name, latency=1))
        return connections

    def _parse_router_coords(self, name: str) -> tuple:
        m = re.search(r"router_(\d+)_(\d+)", name)
        if m:
            return int(m.group(1)), int(m.group(2))
        return None

    def _find_router_at(self, x: int, y: int, routers: list) -> Optional:
        for r in routers:
            coords = self._parse_router_coords(r.name)
            if coords and coords[0] == x and coords[1] == y:
                return r
        return None