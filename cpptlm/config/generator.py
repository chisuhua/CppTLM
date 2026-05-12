"""cpptlm/config/generator.py — TopologyGenerator wrapper for cpptlm package."""

from __future__ import annotations

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from scripts.topology_generator import TopologyGenerator

__all__ = ["TopologyGenerator"]