// src/tlm/cluster/gpu_internal_fabric.cc
// P3 GpuInternalFabric: minimal SimModule impl (P3 Task 22 scaffold).
#include "tlm/cluster/gpu_internal_fabric.hh"

namespace tlm::cluster {

GpuInternalFabric::GpuInternalFabric(const std::string& name, EventQueue* eq)
    : SimModule(name, eq) {}

} // namespace tlm::cluster