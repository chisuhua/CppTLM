// src/tlm/crossbar_tlm.cc
// CrossbarTLM 实现 + P3 helper 方法
// 功能描述: 包含 CrossbarTLM 的 ChStream helper 方法实现 (P3 partial, 不依赖 D.1)
// 作者: CppTLM Team
// 日期: 2026-06-19
#include "tlm/crossbar_tlm.hh"
#include "core/chstream_module.hh"
#include "core/master_port.hh"
#include "core/port_manager.hh"
#include "core/simple_port.hh"
#include "core/slave_port.hh"
#include <stdexcept>

// P3 partial: 不依赖 D.1, lazy 注册 cpu_side 端口 + bind 上游 bus
// 端口方向约定: CrossbarTLM.cpu_side (upstream/输入) ← bus.mem_side (upstream/输入)
void CrossbarTLM::connectCPUSideBus(ChStreamModuleBase* bus) {
    if (!bus) {
        throw std::runtime_error("CrossbarTLM::connectCPUSideBus: bus is null");
    }

    if (!getPortManager().getUpstreamPort("cpu_side")) {
        getPortManager().addUpstreamPort(this, {4}, {}, "cpu_side");
    }
    auto* cpu_side = getPortManager().getUpstreamPort("cpu_side");

    if (!bus->getPortManager().getUpstreamPort("mem_side")) {
        bus->getPortManager().addUpstreamPort(bus, {4}, {}, "mem_side");
    }
    auto* bus_mem_side = bus->getPortManager().getUpstreamPort("mem_side");

    if (!cpu_side || !bus_mem_side) {
        throw std::runtime_error(
            "CrossbarTLM::connectCPUSideBus: port not found (cpu_side/bus.mem_side)");
    }

    helper_pairs_.emplace_back(std::make_unique<PortPair>(cpu_side, bus_mem_side));
}

// P3 partial: 不依赖 D.1, lazy 注册 mem_side 端口 + bind 下游 bus
// 端口方向约定: CrossbarTLM.mem_side (upstream/输入) ← bus.cpu_side (upstream/输入)
void CrossbarTLM::connectMemSideBus(ChStreamModuleBase* bus) {
    if (!bus) {
        throw std::runtime_error("CrossbarTLM::connectMemSideBus: bus is null");
    }

    if (!getPortManager().getUpstreamPort("mem_side")) {
        getPortManager().addUpstreamPort(this, {4}, {}, "mem_side");
    }
    auto* mem_side = getPortManager().getUpstreamPort("mem_side");

    if (!bus->getPortManager().getUpstreamPort("cpu_side")) {
        bus->getPortManager().addUpstreamPort(bus, {4}, {}, "cpu_side");
    }
    auto* bus_cpu_side = bus->getPortManager().getUpstreamPort("cpu_side");

    if (!mem_side || !bus_cpu_side) {
        throw std::runtime_error(
            "CrossbarTLM::connectMemSideBus: port not found (mem_side/bus.cpu_side)");
    }

    helper_pairs_.emplace_back(std::make_unique<PortPair>(mem_side, bus_cpu_side));
}
