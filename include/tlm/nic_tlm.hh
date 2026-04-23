// include/tlm/nic_tlm.hh
// NICTLM: Network Interface Card TLM 模块（v2.1 ChStream 模型）
// 功能描述：PE侧(CacheReq/CacheResp) ↔ Net侧(NoCFlitBundle) 双向转换
// 作者 CppTLM Team / 日期 2026-04-24
#ifndef TLM_NIC_TLM_HH
#define TLM_NIC_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "framework/dual_port_stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>
#include <vector>
#include <array>

namespace tlm {

/**
 * @brief 地址区域定义
 */
struct AddressRegion {
    uint64_t base_addr;
    uint64_t size;
    uint32_t target_node;
    std::string target_type;
};

/**
 * @brief 地址映射表 (查找地址 → 节点 ID)
 */
class AddressMap {
public:
    void add_region(uint64_t base, uint64_t size, uint32_t node, const std::string& type = "MEMORY_CTRL") {
        AddressRegion r;
        r.base_addr = base;
        r.size = size;
        r.target_node = node;
        r.target_type = type;
        regions_.push_back(r);
    }

    uint32_t lookup_node(uint64_t addr) const {
        for (const auto& r : regions_) {
            if (addr >= r.base_addr && addr < r.base_addr + r.size) {
                return r.target_node;
            }
        }
        return 0;
    }

    std::pair<uint32_t, uint32_t> node_to_coord(uint32_t node_id, uint32_t mesh_x) const {
        return {node_id % mesh_x, node_id / mesh_x};
    }

    uint32_t coord_to_node(uint32_t x, uint32_t y, uint32_t mesh_x) const {
        return y * mesh_x + x;
    }

private:
    std::vector<AddressRegion> regions_;
};

/**
 * @brief NICTLM: 网络接口卡 TLM 模块
 *
 * 端口拓扑（4 个访问器，遵循 DualPortStreamAdapter）:
 * - pe_req_in()    — 输入，接收 CacheReqBundle（来自 Cache/CPU）
 * - pe_resp_out()  — 输出，发送 CacheRespBundle（返回 PE）
 * - net_req_out()  — 输出，发送 NoCFlitBundle（到 Router）
 * - net_resp_in()  — 输入，接收 NoCFlitBundle（从 Router）
 *
 * 功能:
 * - packetize: CacheReq → NoC Flits 切分
 * - reassemble: NoC Flits → CacheResp 重组
 * - 地址映射: CacheReq.address → dst_node
 */
class NICTLM : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_VCS = 4;
    static constexpr unsigned FLITS_PER_PACKET = 4;
    static constexpr unsigned MAX_PENDING_PACKETS = 16;

    NICTLM(const std::string& name, EventQueue* eq,
           uint32_t node_id = 0, uint32_t mesh_x = 4, uint32_t mesh_y = 4);

    ~NICTLM() override = default;

    std::string get_module_type() const override { return "NICTLM"; }

    unsigned num_ports() const override { return 4; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;

    void tick() override;

    uint32_t node_id() const { return node_id_; }
    uint32_t mesh_x() const { return mesh_x_; }
    uint32_t mesh_y() const { return mesh_y_; }

    // PE 侧访问器
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>&   pe_req_in()   { return pe_req_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>&  pe_resp_out() { return pe_resp_out_; }

    // Network 侧访问器
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>&   net_req_out() { return net_req_out_; }
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>&    net_resp_in()  { return net_resp_in_; }

    // 地址映射
    void add_address_region(uint64_t base, uint64_t size, uint32_t node, const std::string& type = "MEMORY_CTRL") {
        addr_map_.add_region(base, size, node, type);
    }
    uint32_t lookup_node(uint64_t addr) const { return addr_map_.lookup_node(addr); }

private:
    void packetize(const bundles::CacheReqBundle& req);
    bool reassemble(const bundles::NoCFlitBundle& flit);

    bool handle_pe_req();
    bool handle_net_resp();

    struct PendingPacket {
        uint64_t transaction_id;
        uint32_t dst_node;
        std::array<bundles::NoCFlitBundle, FLITS_PER_PACKET> flits;
        uint8_t flits_received;
        bool is_write;
        uint64_t address;
    };

    uint32_t node_id_;
    uint32_t mesh_x_;
    uint32_t mesh_y_;

    AddressMap addr_map_;

    // PE 侧端口
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>   pe_req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> pe_resp_out_;

    // Network 侧端口
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>  net_req_out_;
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>  net_resp_in_;

    tlm_stats::StatGroup stat_group_;
    tlm_stats::Scalar& stats_flits_sent_;
    tlm_stats::Scalar& stats_flits_received_;
    tlm_stats::Scalar& stats_packets_sent_;
    tlm_stats::Scalar& stats_packets_received_;
    tlm_stats::Distribution& stats_latency_;

    std::vector<PendingPacket> pending_packets_;
};

} // namespace tlm

#endif // TLM_NIC_TLM_HH