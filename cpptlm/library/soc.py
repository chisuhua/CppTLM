"""cpptlm/library/soc.py — SoC orchestrator: fluent API for full topology assembly

用法::

    soc = SoC("my_soc")
    soc.add_cluster(cpu_l1_cluster(0).layout_grid(dx=4, dy=1, x_offset=0))
    soc.add_cluster(cpu_l1_cluster(1).layout_grid(dx=4, dy=1, x_offset=200))
    soc.add_module("xbar", "CrossbarTLM")
    soc.connect_group("compute", "xbar.0", latency=5)
    soc.save("configs/my_soc.json")
"""
from __future__ import annotations
from typing import Optional
import json
import os

from cpptlm.topo.layer import TopoLayer, ConnectionSpec
from cpptlm.topo.emitter import CxxCompatibleEmitter, TopoEmitError


class SoC:
    """Fluent API for assembling a full SoC topology."""

    def __init__(self, name: str, description: str = ""):
        self._root = TopoLayer(name=name, metadata={"description": description} if description else {})
        self._last_target = self._root
        self._last_target_is_root = True

    def add_cluster(self, layer: TopoLayer) -> "SoC":
        self._root.add_sublayer(layer)
        self._last_target = layer
        self._last_target_is_root = False
        return self

    def add_module(self, name: str, type: str, **params) -> "SoC":
        """添加模块到 *last cluster*（若已 add_cluster）或 root（若未 add_cluster）.

        ⚠️ 注意:  此方法 **不是** 总是写到 root. 在 `add_cluster(...)` 之后调用,
        `add_module` 写入到 *last cluster* (即上一个 add_cluster 的 layer),
        而非 root. 这允许 `soc.add_cluster(cpu_l1_cluster(0)).add_module("l1_ext", ...)`
        链式扩展 cluster 内的模块.

        若需要顶层 (root) 模块 (例如 crossbar), 应在所有 `add_cluster(...)` 之前
        调用 `add_module(...)`, 或在 `add_cluster` 后显式将模块放入 root 的 sublayer.

        示例::

            soc = SoC("test")
            # xbar 进入 cluster0 (作为 last_target)
            soc.add_cluster(cpu_l1_cluster(0)).add_module("l1_extra", "CacheTLM")
            # 顶层模块: 在任何 add_cluster 之前调用
            soc = SoC("test")
            soc.add_module("xbar", "CrossbarTLM")  # 进 root
            soc.add_cluster(cpu_l1_cluster(0))  # 此后 add_module 进 cluster
        """
        target = self._root if self._last_target_is_root else self._last_target
        target.add_module(name, type, **params)
        return self

    def connect(self, src: str, dst: str, latency: int = 1) -> "SoC":
        self._root.add_connection(src, dst, latency=latency)
        return self

    def connect_group(self, tag: str, dst: str, latency: int = 1) -> "SoC":
        self._root.add_connection(f"group:{tag}", dst, latency=latency)
        return self

    def tag(self, *tags: str) -> "SoC":
        self._last_target.tag(*tags)
        return self

    def layout_grid(self, dx: int, dy: int, x_offset: int = 0, y_offset: int = 0) -> "SoC":
        self._root.layout_grid(dx=dx, dy=dy, x_offset=x_offset, y_offset=y_offset)
        return self

    def save(self, path: str) -> None:
        emitter = CxxCompatibleEmitter()
        result = emitter.emit(self._root)
        out_dir = os.path.dirname(path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2, ensure_ascii=False)
