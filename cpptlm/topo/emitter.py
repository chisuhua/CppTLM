"""cpptlm/topo/emitter.py — CxxCompatibleEmitter: TopoLayer → C++ ModuleFactory JSON

将 Python 端的 TopoLayer 树展开为 C++ ModuleFactory::instantiateAll() 可加载的
JSON dict。设计原则: C++ schema 零变更, Python 端做所有复杂性, emit 输出仅
C++ 已识别字段.

核心规则:
- 顶层键白名单: modules / connections / groups / hierarchy / coherence_domains /
                extends / include / plugin / metadata / name / description / version
- tag (Python 内部) → groups dict 展开
- sublayers → hierarchy 树
- module.metadata['layout'] → modules[].layout
- group 默认 placement: 'grid' (C++ topology_dumper 支持)
- 校验: module name 不与 C++ 保留键冲突; hierarchy 节点名匹配 module
"""
from __future__ import annotations
from typing import Any, Dict, List, Set

from cpptlm.topo.layer import TopoLayer, ModuleSpec, ConnectionSpec, CoherenceDomainSpec


class TopoEmitError(Exception):
    """emitter 检测到 schema 冲突或验证失败时抛出。"""


ALLOWED_TOP_LEVEL_KEYS = frozenset({
    "name", "description", "version", "metadata",
    "modules", "connections", "groups", "hierarchy",
    "coherence_domains", "extends", "include", "plugin",
})

RESERVED_MODULE_NAMES = frozenset({
    "groups", "modules", "connections", "hierarchy",
    "coherence_domains", "name", "description", "extends",
    "include", "plugin", "version", "metadata",
})

VALID_PLACEMENTS = frozenset({"grid", "linear", "radial", "circle"})


class CxxCompatibleEmitter:
    """将 TopoLayer 树展开为 C++ ModuleFactory 可加载的 JSON dict。"""

    def emit(self, root: TopoLayer) -> Dict[str, Any]:
        """入口: 展开 root 层为 JSON dict。

        流程: 1) 校验保留名 2) 收集 modules 3) 收集 connections
              4) 展开 tag → groups 5) 构造 hierarchy 6) 校验绑定
        """
        modules = self._collect_modules(root)
        self._check_reserved_names(modules)
        self._validate_hierarchy_binding(root, modules)

        result: Dict[str, Any] = {
            "modules": modules,
            "connections": self._collect_connections(root),
        }

        if root.name:
            result["name"] = root.name
        if root.metadata.get("description"):
            result["description"] = root.metadata["description"]
        if root.metadata.get("version"):
            result["version"] = root.metadata["version"]

        groups = self._collect_groups(root, modules)
        if groups:
            result["groups"] = groups

        hierarchy = self._collect_hierarchy(root)
        if hierarchy is not None:
            result["hierarchy"] = hierarchy

        if root.coherence_domains:
            result["coherence_domains"] = [
                self._serialize_coherence(d) for d in root.coherence_domains
            ]

        return result

    def _collect_modules(self, root: TopoLayer) -> List[Dict[str, Any]]:
        """递归收集所有子层的模块, 序列化为 JSON dict 列表。"""
        out: List[Dict[str, Any]] = []
        for m in root._all_modules():
            entry: Dict[str, Any] = {"name": m.name, "type": m.type}
            if m.params:
                entry["params"] = dict(m.params)
            layout = m.metadata.get("layout")
            if layout:
                entry["layout"] = layout
            out.append(entry)
        return out

    def _collect_connections(self, root: TopoLayer) -> List[Dict[str, Any]]:
        """递归收集所有层的 connections, 序列化为 JSON dict 列表。"""
        out: List[Dict[str, Any]] = []
        for layer in [root] + root._all_sublayers():
            for c in layer.connections:
                entry: Dict[str, Any] = {"src": c.src, "dst": c.dst}
                if c.latency:
                    entry["latency"] = c.latency
                if c.bandwidth is not None:
                    entry["bandwidth"] = c.bandwidth
                if c.vc_priorities:
                    entry["vc_priorities"] = list(c.vc_priorities)
                out.append(entry)
        return out

    def _collect_groups(self, root: TopoLayer,
                       modules: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Python tag 展平成 C++ `groups` dict (flat form: name → [members]).

        C++ 端 module_factory.cc:297-305 期望 flat form (members 是字符串列表).
        placement 字段不写入 JSON; 它是 Python 端 `metadata` 的一部分, .dot 生成时
        可通过其他途径提供 (例如 cpptlm.topo.topology_dumper 增强).

        规则:
        - 每个 layer 的 tag 展开为 group, members = 该层所有 module names
        - 同一 tag 跨多层合并 (去重, 保持插入顺序)
        - 显式定义的 group (在 layer.metadata['groups'] 中) 也合并
        """
        groups: Dict[str, List[str]] = {}

        for layer in [root] + root._all_sublayers():
            local_names = [m.name for m in layer.modules]
            for tag in layer.tags:
                if tag not in groups:
                    groups[tag] = list(local_names)
                else:
                    for n in local_names:
                        if n not in groups[tag]:
                            groups[tag].append(n)

        explicit = root.metadata.get("groups") or {}
        if isinstance(explicit, dict):
            for gname, members in explicit.items():
                if gname in groups:
                    for n in (members if isinstance(members, list) else [members]):
                        if n not in groups[gname]:
                            groups[gname].append(n)
                else:
                    groups[gname] = list(members) if isinstance(members, list) else [members]

        return groups

    def _collect_hierarchy(self, root: TopoLayer) -> Dict[str, Any] | None:
        """递归构造 hierarchy 树: {name, children: [recursive]}。

        仅当 root 有 sublayers 时才输出 hierarchy 字段 (符合 C++ 端对 hierarchy
        作为"嵌套子结构"而非"扁平命名空间"的语义约定)。
        """
        if not root.sublayers:
            return None
        return self._hierarchy_node(root)

    def _hierarchy_node(self, layer: TopoLayer) -> Dict[str, Any]:
        if layer.sublayers:
            children = [self._hierarchy_node(sub) for sub in layer.sublayers]
        else:
            children = []
        return {"name": layer.name, "children": children}

    def _validate_hierarchy_binding(self, root: TopoLayer,
                                   modules: List[Dict[str, Any]]) -> None:
        """校验 hierarchy 节点名在 module names 中, 或为 cluster 级结构名。

        Cluster 级结构名: 该名字在 sublayers 中存在 (作为子层 name)。
        Module 级名: 必须在 modules 列表中。
        """
        module_names = {m["name"] for m in modules}
        sublayer_names = {sub.name for sub in root._all_sublayers()}

        for h_node in self._walk_hierarchy(root):
            name = h_node["name"]
            if name in module_names or name in sublayer_names or name == root.name:
                continue
            raise TopoEmitError(
                f"hierarchy node '{name}' has no matching module or sublayer; "
                f"hierarchy binding validation failed"
            )

    def _walk_hierarchy(self, root: TopoLayer):
        """yield 每个 hierarchy 节点 dict (含 root 自身)。"""
        yield {"name": root.name}
        for sub in root.sublayers:
            yield from self._walk_hierarchy(sub)

    def _check_reserved_names(self, modules: List[Dict[str, Any]]) -> None:
        """模块名不得与 C++ 保留键冲突。"""
        for m in modules:
            if m["name"] in RESERVED_MODULE_NAMES:
                raise TopoEmitError(
                    f"module name '{m['name']}' is reserved (conflicts with C++ schema key)"
                )

    def _serialize_coherence(self, d: CoherenceDomainSpec) -> Dict[str, Any]:
        out: Dict[str, Any] = {"name": d.name}
        if d.protocol:
            out["protocol"] = d.protocol
        if d.members:
            out["members"] = list(d.members)
        if d.bridges:
            out["bridges"] = dict(d.bridges)
        return out
