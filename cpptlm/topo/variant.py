"""
cpptlm/topo/variant.py — TopoVariant: 拓扑变体

支持基于基拓扑的差异化配置生成和多变体对比。
"""

from dataclasses import dataclass, field
from typing import List, Dict

from cpptlm.topo.layer import TopoLayer


@dataclass
class TopoVariant:
    """
    拓扑变体 - 基于基拓扑的差异化配置
    """
    name: str
    base: TopoLayer
    patches: List = field(default_factory=list)
    metrics: List[str] = field(default_factory=list)

    def build(self) -> TopoLayer:
        """构建变体拓扑"""
        import copy
        result = TopoLayer(
            name=f"{self.base.name}_{self.name}",
            metadata={**self.base.metadata, "variant": self.name}
        )
        result.modules = copy.deepcopy(self.base.modules)
        result.connections = copy.deepcopy(self.base.connections)
        result.coherence_domains = copy.deepcopy(self.base.coherence_domains)

        for patch in self.patches:
            result = patch.apply_to(result)
        return result


@dataclass
class TopoVariantSet:
    """
    拓扑变体集 - 管理多个变体用于比较
    """
    name: str
    variants: List[TopoVariant] = field(default_factory=list)

    def add_variant(self, variant: TopoVariant) -> "TopoVariantSet":
        self.variants.append(variant)
        return self

    def build_all(self) -> Dict[str, TopoLayer]:
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
                "hierarchy_depth": self._calc_depth(layer),
            }
        return comparison

    def _calc_depth(self, layer: TopoLayer, depth: int = 0) -> int:
        if not layer.sublayers:
            return depth
        return max(self._calc_depth(sub, depth + 1) for sub in layer.sublayers)