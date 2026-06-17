"""cpptlm/config/generator.py — TopologyGenerator wrapper for cpptlm package.

DEPRECATED (2026-06-17): scripts.topology_generator backend 已不存在.
此 re-export 触发 DeprecationWarning; 新代码请直接用 cpptlm.library.
"""

from __future__ import annotations

import sys
import os
import warnings

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

_warned = False
def _warn_legacy():
    global _warned
    if not _warned:
        warnings.warn(
            "cpptlm.config.generator (TopologyGenerator) is deprecated; "
            "use cpptlm.library instead.",
            DeprecationWarning, stacklevel=3,
        )
        _warned = True

_warn_legacy()

try:
    from scripts.topology_generator import TopologyGenerator
except ImportError:
    TopologyGenerator = None

__all__ = ["TopologyGenerator"]