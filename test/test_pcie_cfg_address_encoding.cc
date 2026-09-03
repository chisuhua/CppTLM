// test/test_pcie_cfg_address_encoding.cc
// PCIe Config Space 地址编码回归测试
//
// 目的: 验证 PcieEndpointIP 在 AXI 通道上接收 PCIe 配置请求时,
// 按 PCIe 规范解码 AW 地址:
//   - 低 2 bit [1:0] 为对齐保留位,必须为 0
//   - 配置空间 dword offset 来自 bits[7:2]
//   - byte offset = (awaddr >> 2) & 0x3F (针对 256-byte 兼容配置头)
//                    或 & 0xFFF (4096-byte extended 配置空间)
//   - BAR 访问必须经过该地址范围之外
//
// 历史: AGENTS.md §已知问题 (Minor) 记录 Phase 8 M1 接线简化用 awaddr
//       直接当 offset;本测试固化 PCIe 规范语义。

#include "bundles/axi4_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/host_bypass_tlm.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <cstdint>
#include <nlohmann/json.hpp>

using namespace bundles;
using namespace tlm::pcie;
using json = nlohmann::json;

namespace {

// 用符合 PCIe 规范的 awaddr 编码方式,经 HostBypassTLM 发起 cfg 读写。
// 语义:
//   awaddr = dword_offset << 2
//   例: Command Register 位于 dword offset 1 → awaddr = 0x04
//       BAR0 位于 dword offset 4 → awaddr = 0x10
//       状态寄存器 dword offset 6 → awaddr = 0x18
struct CfgAddrFixture {
    EventQueue eq;
    PcieEndpointIP ep;
    HostBypassTLM hb;

    CfgAddrFixture()
        : ep("pcie_ep_cfg_addr", &eq), hb("hb_cfg_addr", &eq) {
        ep.init();
        json cfg;
        cfg["axi_adapter"] = json::object();
        ep.set_config(cfg);
        ep.on_config_loaded();

        hb.init();
        hb.attach_to_endpoint(&ep);
    }

    // 写入并驱动直到响应到达
    bool axi_write(uint64_t awaddr, uint32_t wdata, uint16_t id) {
        Axi4Bundle req;
        req.awid.write(id);
        req.awaddr.write(awaddr);
        req.awlen.write(0);
        req.awsize.write(2);     // 4 bytes
        req.awburst.write(1);
        req.wdata.write(wdata);
        req.wstrb.write(0xF);
        req.wlast.write(1);
        if (!hb.axi_master_req(req)) return false;
        hb.set_axi_master_ready(true);
        for (int i = 0; i < 100 && (hb.axi_master_req_valid()
                                    || hb.axi_outstanding_wr() > 0); ++i) {
            ep.tick();
            hb.tick();
        }
        if (!hb.axi_master_resp_valid()) return false;
        hb.axi_master_resp_consume();
        return true;
    }

    bool axi_read(uint64_t araddr, uint64_t& rdata, uint16_t id) {
        Axi4Bundle req;
        req.arid.write(id);
        req.araddr.write(araddr);
        req.arlen.write(0);
        req.arsize.write(2);
        req.arburst.write(1);
        if (!hb.axi_master_req(req)) return false;
        hb.set_axi_master_ready(true);
        for (int i = 0; i < 100 && (hb.axi_master_req_valid()
                                    || hb.axi_outstanding_rd() > 0); ++i) {
            ep.tick();
            hb.tick();
        }
        if (!hb.axi_master_resp_valid()) return false;
        rdata = hb.axi_master_resp_data().rdata.read();
        hb.axi_master_resp_consume();
        return true;
    }
};

} // namespace

TEST_CASE("PCIe cfg: AWADDR 按 bits[7:2] 解码为 dword offset (低位 2bit 不影响 offset)",
          "[pcie][axi][cfg-encoding]") {
    CfgAddrFixture f;

    // 写 Command Register (dword offset 1, byte offset 0x04)
    // 按 PCIe 规范, awaddr = 0x04
    REQUIRE(f.axi_write(0x04, 0x0007u, 0x100) == true);

    // 验证: Command Register (offset 0x04) 写入 0x0007
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x04) == 0x0007u);

    // 用不同的低 2 bit 重发(同 dword offset),结果应该命中同一寄存器
    // awaddr = 0x05 (低 2 bit = 01),按规范应等同 awaddr = 0x04
    REQUIRE(f.axi_write(0x05, 0x0006u, 0x101) == true);
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x04) == 0x0006u);

    // awaddr = 0x07 (低 2 bit = 11),按规范应等同 awaddr = 0x04
    REQUIRE(f.axi_write(0x07, 0x0005u, 0x102) == true);
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x04) == 0x0005u);
}

TEST_CASE("PCIe cfg: 不同 dword offset 写入不同寄存器 (awaddr=0x10 vs 0x14)",
          "[pcie][axi][cfg-encoding]") {
    CfgAddrFixture f;

    // BAR0 寄存器位于 dword offset 4 → byte offset 0x10
    REQUIRE(f.axi_write(0x10, 0x10000008u, 0x200) == true);
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x10) == 0x10000008u);

    // 紧邻 dword offset (dword 5 → byte 0x14),不应串扰 BAR0
    // 当前实现把 0x14 直接当 byte offset,会写入 offset 0x14 (位于 BAR1)
    // 修复后: dword offset 5 (byte 0x14) 应写 BAR1
    REQUIRE(f.axi_write(0x14, 0x00000000u, 0x201) == true);
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x10) == 0x10000008u); // BAR0 不变
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x14) == 0x00000000u); // BAR1 写入
}

TEST_CASE("PCIe cfg: awaddr > config_size 不进入 cfg 路径 (走 BAR)",
          "[pcie][axi][cfg-encoding]") {
    CfgAddrFixture f;

    // config_size = 4096 字节 = 0x1000
    // 超过该范围的地址应走 BAR 路径,而非 cfg 路径
    constexpr uint64_t BAR_ADDR = 0x10000000ULL;

    // 使用 32-bit 写入值,避免 ch_uint<512>/uint64 隐式转换歧义
    REQUIRE(f.axi_write(BAR_ADDR, 0xDEADBEEFu, 0x300) == true);

    // cfg 路径不应被触发: 选 offset 0x3C 作为探测器 (init() 未触碰)
    REQUIRE(f.ep.vf_pool().config_of(0).read(0x3C) == 0u);

    // 读回 BAR 空间
    uint64_t rdata = 0;
    REQUIRE(f.axi_read(BAR_ADDR, rdata, 0x301) == true);
    REQUIRE(rdata == 0xDEADBEEFu);
}

TEST_CASE("PCIe cfg: 低位 2 bit 不参与 cfg 边界判定 (addr=0xFFC 不越界)",
          "[pcie][axi][cfg-encoding]") {
    CfgAddrFixture f;

    // 按规范, awaddr = 0xFFC 在 cfg 范围内 (dword offset 0x3FF, byte offset 0xFFC)
    // 修复后: awaddr & ~3 = 0xFFC,正常 cfg 访问 offset 0xFFC
    REQUIRE(f.axi_write(0xFFC, 0xCAFE0001u, 0x400) == true);
    REQUIRE(f.ep.vf_pool().config_of(0).read(0xFFC) == 0xCAFE0001u);

    // 低 2 bit 不影响: awaddr=0xFFF 等同 0xFFC
    REQUIRE(f.axi_write(0xFFF, 0xBABE0002u, 0x401) == true);
    REQUIRE(f.ep.vf_pool().config_of(0).read(0xFFC) == 0xBABE0002u);
}
