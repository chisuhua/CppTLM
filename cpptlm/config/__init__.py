"""cpptlm.config — Configuration generation and topology building."""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm_config.builder import ConfigBuilder
from cpptlm.config.topologies import MeshTopology, RingTopology, CrossbarTopology

__all__ = ["ConfigBuilder", "MeshTopology", "RingTopology", "CrossbarTopology"]