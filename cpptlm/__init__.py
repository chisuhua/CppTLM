"""cpptlm — CppTLM Python Library

Chip design generation, topology visualization, and performance visualization
for the CppTLM simulation framework.

Usage::

    from cpptlm.config import ConfigBuilder, MeshTopology

    # Generate mesh topology
    mesh = MeshTopology(rows=4, cols=4)
    config = mesh.build()

    # Or use builder directly
    from cpptlm.config import ConfigBuilder
    builder = ConfigBuilder(name="my_design")
    builder.add_module(...)
    config = builder.build()
"""

__version__ = "0.1.0"

from cpptlm.config import ConfigBuilder
from cpptlm.simulation import SimulationRunner
from cpptlm.visualization import PerformanceDashboard

__all__ = ["ConfigBuilder", "SimulationRunner", "PerformanceDashboard"]