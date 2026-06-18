// src/tlm/cache_tlm.cc
// CacheTLM 实现 + P3 helper 方法
// 功能描述: 包含 CacheTLM 的 ChStream helper 方法实现 (P3 partial, 不依赖 D.1)
// 作者: CppTLM Team
// 日期: 2026-06-19
#include "tlm/cache_tlm.hh"
#include "core/chstream_module.hh"
#include "core/port_manager.hh"
#include "core/simple_port.hh"
#include "core/slave_port.hh"
#include "core/master_port.hh"
#include <stdexcept>

// P3 partial: 不依赖 D.1, lazy 注册 mem_side 端口 + bind 总线
// 端口方向约定: CacheTLM.mem_side (upstream/输入) ← bus.cpu_side (upstream/输入)
void CacheTLM::connectBus(ChStreamModuleBase* bus) {
    if (!bus) {
        throw std::runtime_error("CacheTLM::connectBus: bus is null");
    }

    // P3 partial: lazy 注册 CacheTLM 的 mem_side upstream 端口
    if (!getPortManager().getUpstreamPort("mem_side")) {
        getPortManager().addUpstreamPort(this, {4}, {}, "mem_side");
    }
    auto* mem_side = getPortManager().getUpstreamPort("mem_side");

    // P3 partial: lazy 注册 bus 的 cpu_side upstream 端口 (与 gem5 语义: bus.cpu_side 也是 upstream/输入)
    if (!bus->getPortManager().getUpstreamPort("cpu_side")) {
        bus->getPortManager().addUpstreamPort(bus, {4}, {}, "cpu_side");
    }
    auto* bus_port = bus->getPortManager().getUpstreamPort("cpu_side");

    if (!mem_side || !bus_port) {
        throw std::runtime_error("CacheTLM::connectBus: port not found (mem_side/cpu_side)");
    }

    // 借助 PortPair 完成 bind (SimplePort::bind 需 PortPair* 上下文,非直接 port-to-port)
    helper_pairs_.emplace_back(std::make_unique<PortPair>(mem_side, bus_port));
}
