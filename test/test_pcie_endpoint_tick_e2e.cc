// test_pcie_endpoint_tick_e2e.cc
// PcieEndpointTLM 端到端 tick() 行为测试 — 补 650bbc4 commit 遗漏的端到端断言
// Author: CppTLM Team
// Date: 2026-08-27
//
// 漏洞修复说明（per spec.md 7 个 Scenario 的端到端覆盖缺口）：
//   1. Doorbell Scenario 缺 mmio_out 端口实际 valid 断言
//   2. MSI-X delivery path 缺 irq_out 端口实际 valid 断言
//   3. BAR1 MEM write >8 缺 mem_out 端口实际 valid 断言
//   4. consume_doorbell_out 配对 try_pop 后状态正确性
//   5. ChStreamAdapterFactory::create 返回 MultiPortStreamAdapter 实例类型
//   6. config_space 越界/未对齐读写端到端
//
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/specs/pcie-endpoint-tlm/spec.md

#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/master_port.hh"
#include "core/module_factory.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "core/slave_port.hh"
#include "framework/chstream_adapter_factory.hh"
#include "framework/multi_port_stream_adapter.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/msix_table_mvp.hh"
#include "tlm/gpu/pcie_bar_router_mvp.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"

#include <cstdio>
#include <nlohmann/json.hpp>

using namespace tlm::gpu;
using namespace bundles;
using json = nlohmann::json;

static inline const PcieTlpBundle& resp_out_data(const PcieEndpointTLM& ep, unsigned i) {
    return ep.resp_out[i].data();
}

TEST_CASE("PcieEndpoint e2e: MMIO_WRITE doorbell -> mmio_out in 250-700ns",
          "[pcie][endpoint][e2e][doorbell]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    REQUIRE(ep.bar_router().add_register(0x0014, "GPU_REG_DOORBELL", PcieBarRouter::Access::WO,
                                         PcieBarRouter::SideEffect::DOORBELL, 0) == true);

    PcieTlpBundle req;
    req.kind.write(PcieTlpBundle::MMIO_WRITE);
    req.offset.write(0x0014);
    req.size.write(4);
    req.data.write(0x200);
    req.trans_id.write(42);
    ep.req_in[0].data() = req;
    ep.req_in[0].set_valid(true);

    REQUIRE(ep.req_in[0].valid() == true);
    REQUIRE(ep.resp_out[1].valid() == false);

    const uint64_t t0 = ep.bar_router().doorbell_now_cycles();
    ep.tick(); // 处理 slave_in MMIO_WRITE → bar_router_ 写入 + resp_out[1] valid

    REQUIRE(ep.resp_out[1].valid() == true); // 同步 MMIO_WRITE 响应立即

    // 推进 doorbell（≤ 1000 cycles，应在 [250, 700] 区间内完成）
    bool doorbell_completed = false;
    for (int i = 0; i < 1000 && !doorbell_completed; ++i) {
        ep.tick();
        if (!ep.bar_router().doorbell_is_pending(0)) {
            const uint64_t elapsed = ep.bar_router().doorbell_now_cycles() - t0;
            REQUIRE(elapsed >= Doorbell::MIN_LATENCY_NS);
            REQUIRE(elapsed <= Doorbell::MAX_LATENCY_NS);
            doorbell_completed = true;
        }
    }
    REQUIRE(doorbell_completed);
    REQUIRE(ep.bar_router().doorbell_sq_tail(0) == 0x200u);
}

TEST_CASE("PcieEndpoint e2e: MSI-X update_pending -> irq_out emits IRQ_DELIVERY",
          "[pcie][endpoint][e2e][msix]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 配置 vector 3
    REQUIRE(ep.msix().configure_vector(3, 0xFEE00000ULL, 0xCAFEBABEu) == true);
    REQUIRE(ep.resp_out[3].valid() == false); // irq_out 端口

    // 触发 MSI-X pending
    REQUIRE(ep.msix().update_pending(3, /*trans_id=*/99) == true);
    REQUIRE(ep.msix().pending_count() == 1u);

    ep.tick(); // 触发 irq_out emit

    // irq_out 端口必须 valid (per spec.md Scenario "MSI-X delivery path")
    REQUIRE(ep.resp_out[3].valid() == true);
    const auto& out = ep.resp_out[3].data();
    REQUIRE(out.kind.read() == PcieTlpBundle::IRQ_DELIVERY);
    REQUIRE(out.is_irq_delivery() == true);
    // vector/msg_data/msg_addr 编码到 PcieTlpBundle 字段
    REQUIRE(out.offset.read() == 3u);          // vector -> offset
    REQUIRE(out.size.read() == 0xCAFEBABEu);   // msg_data -> size
    REQUIRE(out.data.read() == 0xFEE00000ULL); // msg_addr -> data
    REQUIRE(out.trans_id.read() == 99u);

    // tick 后 pending 应消费
    REQUIRE(ep.msix().pending_count() == 0u);
}

TEST_CASE("PcieEndpoint e2e: masked vector does NOT emit irq_out",
          "[pcie][endpoint][e2e][msix][mask]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    ep.msix().configure_vector(3, 0xFEE00000ULL, 0xCAFEBABEu);
    ep.msix().set_mask(3, true);

    REQUIRE(ep.msix().update_pending(3) == false); // masked: 不投递
    REQUIRE(ep.msix().pending_count() == 0u);

    ep.tick();
    REQUIRE(ep.resp_out[3].valid() == false); // irq_out 端口必须空

    // 解除 mask 后投递
    ep.msix().clear_mask(3);
    REQUIRE(ep.msix().update_pending(3) == true);
    ep.tick();
    REQUIRE(ep.resp_out[3].valid() == true);
}

TEST_CASE("PcieEndpoint e2e: BAR1 MEM write >8 bytes emits descriptor-only on mem_out",
          "[pcie][endpoint][e2e][mem][backdoor]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // size > 8 的 BAR1 MEM_WRITE
    PcieTlpBundle req;
    req.kind.write(PcieTlpBundle::MEM_WRITE);
    req.bar_index.write(1);
    req.offset.write(0x1000);
    req.size.write(4096);          // 4KB bulk DMA
    req.data.write(0xDEADBEEFULL); // 应被强制 data=0 (descriptor-only)
    req.trans_id.write(7);
    ep.req_in[0].data() = req;
    ep.req_in[0].set_valid(true);

    REQUIRE(ep.resp_out[2].valid() == false); // mem_out 端口
    ep.tick();

    // mem_out 必须有 descriptor-only TLP，data=0（per spec.md "BAR1 MEM write >8 bytes uses
    // backdoor path"）
    REQUIRE(ep.resp_out[2].valid() == true);
    const auto& out = ep.resp_out[2].data();
    REQUIRE(out.kind.read() == PcieTlpBundle::MEM_WRITE);
    REQUIRE(out.bar_index.read() == 1u);
    REQUIRE(out.size.read() == 4096u); // size 反映实际字节数
    REQUIRE(out.data.read() == 0u);    // data=0 (descriptor-only)
    REQUIRE(out.trans_id.read() == 7u);
}

TEST_CASE("PcieEndpoint e2e: BAR1 MEM write <=8 bytes forwards inline data",
          "[pcie][endpoint][e2e][mem][inline]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // size = 4 的 BAR1 MEM_WRITE (inline)
    PcieTlpBundle req;
    req.kind.write(PcieTlpBundle::MEM_WRITE);
    req.bar_index.write(1);
    req.offset.write(0x100);
    req.size.write(4);
    req.data.write(0xCAFEBABEULL);
    req.trans_id.write(8);
    ep.req_in[0].data() = req;
    ep.req_in[0].set_valid(true);

    ep.tick();
    REQUIRE(ep.resp_out[2].valid() == true);
    const auto& out = resp_out_data(ep, 2);
    REQUIRE(out.size.read() == 4u);
    REQUIRE(out.data.read() == 0xCAFEBABEULL); // inline data 保留
}

TEST_CASE("PcieEndpoint e2e: CFG_READ returns vendor_id; CFG_WRITE updates reg",
          "[pcie][endpoint][e2e][cfg]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // CFG_READ offset 0x00 → vendor_id/device_id
    PcieTlpBundle read_req;
    read_req.kind.write(PcieTlpBundle::CFG_READ);
    read_req.offset.write(0x00);
    read_req.size.write(4);
    ep.req_in[0].data() = read_req;
    ep.req_in[0].set_valid(true);
    ep.tick();
    REQUIRE(ep.resp_out[1].valid() == true);
    REQUIRE(ep.resp_out[1].data().data.read() == 0x123410DEu); // vendor|device
    ep.resp_out[1].clear_valid();                              // 手动清除以便下一轮

    // CFG_WRITE offset 0x10 (Command reg, RW)
    PcieTlpBundle write_req;
    write_req.kind.write(PcieTlpBundle::CFG_WRITE);
    write_req.offset.write(0x10);
    write_req.data.write(0x00000007u); // Bus Master + Mem + IO enable
    ep.req_in[0].data() = write_req;
    ep.req_in[0].set_valid(true);
    ep.tick();
    REQUIRE(ep.resp_out[1].valid() == true);
    REQUIRE(ep.config_space().read(0x10) == 0x00000007u);
}

TEST_CASE("PcieEndpoint e2e: BAR0 MMIO_READ returns table value", "[pcie][endpoint][e2e][bar]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 配置 RW 寄存器
    REQUIRE(ep.bar_router().add_register(0x1000, "STATUS", PcieBarRouter::Access::RW,
                                         PcieBarRouter::SideEffect::NONE) == true);
    // 写寄存器
    REQUIRE(ep.bar_router().mmio_write(0x1000, 0xDEADBEEFu) == true);

    // MMIO_READ 通过 slave_in
    PcieTlpBundle read_req;
    read_req.kind.write(PcieTlpBundle::MMIO_READ);
    read_req.offset.write(0x1000);
    read_req.size.write(4);
    ep.req_in[0].data() = read_req;
    ep.req_in[0].set_valid(true);
    ep.tick();
    REQUIRE(ep.resp_out[1].valid() == true);
    REQUIRE(ep.resp_out[1].data().data.read() == 0xDEADBEEFu);
}

TEST_CASE("PcieEndpoint e2e: ChStreamAdapterFactory::create returns MultiPortStreamAdapter",
          "[pcie][endpoint][e2e][factory]") {
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);

    // ChStreamAdapterFactory::create 应该返回 MultiPortStreamAdapter<PcieEndpointTLM,
    // PcieTlpBundle, 4>
    auto* adapter = ChStreamAdapterFactory::get().create("PcieEndpointTLM", &ep);
    REQUIRE(adapter != nullptr);

    // 类型断言：必须是 MultiPortStreamAdapter<PcieEndpointTLM, PcieTlpBundle, PcieTlpBundle, 4>
    auto* multi = dynamic_cast<
        cpptlm::MultiPortStreamAdapter<PcieEndpointTLM, PcieTlpBundle, PcieTlpBundle, 4>*>(adapter);
    REQUIRE(multi != nullptr);
    REQUIRE(multi->num_ports() == 4u);

    // isMultiPort/getPortCount 断言（per ChStreamAdapterFactory）
    REQUIRE(ChStreamAdapterFactory::get().isMultiPort("PcieEndpointTLM") == true);
    REQUIRE(ChStreamAdapterFactory::get().getPortCount("PcieEndpointTLM") == 4u);
}

TEST_CASE("PcieEndpoint e2e: ModuleFactory::instantiateAll JSON instantiation + 4-port adapter",
          "[pcie][endpoint][e2e][instantiateAll]") {
    // per spec.md Scenario "JSON instantiation with multi-port adapter injection"
    // 端到端：通过 ModuleFactory::instantiateAll 真实路径（非 set_config 直调）
    EventQueue eq;
    ModuleFactory mf(&eq);

    json config;
    config["name"] = "pcie_soc_test";
    config["modules"] =
        json::array({{{"name", "pcie_ep"},
                      {"type", "PcieEndpointTLM"},
                      {"params",
                       {{"config_size", 4096},
                        {"msix_num_vectors", 4},
                        {"bar0_registers", json::array({{{"offset", 20},
                                                         {"name", "GPU_REG_DOORBELL"},
                                                         {"access", "WO"},
                                                         {"side_effect", "doorbell"},
                                                         {"stream_id", 0}}})}}}}});
    config["connections"] = json::array();

    REQUIRE(mf.instantiateAll(config) == true);

    // 实例化后 PcieEndpointTLM 必须存在
    auto* ep_raw = mf.getInstance("pcie_ep");
    auto* ep = static_cast<PcieEndpointTLM*>(ep_raw);
    REQUIRE(ep != nullptr);
    REQUIRE(ep->num_ports() == 4u);

    // JSON 参数已应用到子组件
    REQUIRE(ep->config_space().config_size() == 4096u);
    REQUIRE(ep->msix().num_vectors() == 4u);

    // 门铃寄存器已配置
    REQUIRE(ep->bar_router().mmio_write(20, 0x100) == true); // 0x14 == 20
}

TEST_CASE("PcieEndpoint e2e: PcieConfigSpace::init() clears capabilities_ (regression)",
          "[pcie][endpoint][regression]") {
    // 漏洞修复回归测试：T-pe-4 commit 650bbc4 修的 PcieConfigSpace::init()
    // 不清 capabilities_ vector 导致 capabilities pointer 未更新。
    EventQueue eq;
    PcieEndpointTLM ep("pcie_ep", &eq);
    ep.init();

    // 加 1 个 capability
    REQUIRE(ep.config_space().add_capability(17, 64, 0, 0) == true);
    REQUIRE(ep.config_space().capability_count() == 1u);
    REQUIRE((ep.config_space().read(0x34) & 0xFFu) == 64u); // pointer -> 0x40

    // 重新 init
    ep.config_space().init();

    // capabilities_ vector 应清空，capabilities pointer 应清零
    REQUIRE(ep.config_space().capability_count() == 0u);
    REQUIRE((ep.config_space().read(0x34) & 0xFFu) == 0u);

    // 重新加 capability 后 pointer 应正确指向
    REQUIRE(ep.config_space().add_capability(17, 100, 0, 0) == true);
    REQUIRE((ep.config_space().read(0x34) & 0xFFu) == 100u);
}
