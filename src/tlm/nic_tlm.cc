// src/tlm/nic_tlm.cc
// NICTLM: Network Interface Card TLM 模块实现
// 功能描述：实现 packetize/reassemble 和 PE↔Net 双向转发
// 作者 CppTLM Team / 日期 2026-04-24
#include "tlm/nic_tlm.hh"
#include "core/module_factory.hh"

namespace tlm {

NICTLM::NICTLM(const std::string& name, EventQueue* eq,
               uint32_t node_id, uint32_t mesh_x, uint32_t mesh_y)
    : ChStreamModuleBase(name, eq),
      node_id_(node_id),
      mesh_x_(mesh_x),
      mesh_y_(mesh_y),
      stat_group_("nic"),
      stats_flits_sent_(stat_group_.addScalar("flits_sent", "Total flits sent", "flits")),
      stats_flits_received_(stat_group_.addScalar("flits_received", "Total flits received", "flits")),
      stats_packets_sent_(stat_group_.addScalar("packets_sent", "Total packets sent", "packets")),
      stats_packets_received_(stat_group_.addScalar("packets_received", "Total packets received", "packets")),
      stats_latency_(stat_group_.addDistribution("latency", "Packet end-to-end latency", "cycle")) {
}

void NICTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    // DualPortStreamAdapter 通过内部指针存储，不需要额外处理
    (void)adapter;
}

void NICTLM::tick() {
    handle_pe_req();
    handle_net_resp();
}

bool NICTLM::handle_pe_req() {
    if (!pe_req_in_.valid() || !pe_req_in_.ready()) {
        return false;
    }

    bundles::CacheReqBundle req = pe_req_in_.data();
    pe_req_in_.consume();

    packetize(req);
    return true;
}

bool NICTLM::handle_net_resp() {
    if (!net_resp_in_.valid() || !net_resp_in_.ready()) {
        return false;
    }

    bundles::NoCFlitBundle flit = net_resp_in_.data();
    net_resp_in_.consume();

    ++stats_flits_received_;

    if (reassemble(flit)) {
        ++stats_packets_received_;
    }

    return true;
}

void NICTLM::packetize(const bundles::CacheReqBundle& req) {
    uint32_t dst_node = lookup_node(req.address.read());
    uint64_t tid = req.transaction_id.read();

    uint8_t total_flits = static_cast<uint8_t>((req.size.read() + 7) / 8);

    for (uint8_t i = 0; i < total_flits; ++i) {
        bundles::NoCFlitBundle flit;

        flit.transaction_id.write(tid);
        flit.src_node.write(node_id_);
        flit.dst_node.write(dst_node);
        flit.address.write(req.address.read());
        flit.vc_id.write(i % NUM_VCS);
        flit.flit_index.write(i);
        flit.flit_count.write(total_flits);
        flit.hops.write(0);
        flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);
        flit.is_write.write(req.is_write.read());
        flit.data.write(req.data.read());
        flit.size.write(req.size.read());

        if (total_flits == 1) {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
        } else if (i == 0) {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD);
        } else if (i == total_flits - 1) {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_TAIL);
        } else {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_BODY);
        }

        net_req_out_.write(flit);
        ++stats_flits_sent_;
    }

    ++stats_packets_sent_;
}

bool NICTLM::reassemble(const bundles::NoCFlitBundle& flit) {
    uint64_t tid = flit.transaction_id.read();

    auto it = std::find_if(pending_packets_.begin(), pending_packets_.end(),
        [tid](const PendingPacket& p) { return p.transaction_id == tid; });

    if (it == pending_packets_.end()) {
        if (flit.is_head()) {
            PendingPacket p;
            p.transaction_id = tid;
            p.dst_node = flit.dst_node.read();
            p.flits_received = 0;
            p.is_write = flit.is_write.read();
            p.address = flit.address.read();
            p.flits[flit.flit_index.read()] = flit;
            pending_packets_.push_back(p);
        }
        return false;
    }

    it->flits[flit.flit_index.read()] = flit;
    ++it->flits_received;

    if (flit.is_tail()) {
        bundles::CacheRespBundle resp;
        resp.transaction_id.write(tid);
        resp.is_hit.write(1);
        resp.error_code.write(flit.error_code.read());

        if (!it->is_write && it->flits_received > 0) {
            const auto& head_flit = it->flits[0];
            if (head_flit.is_response()) {
                resp.data.write(head_flit.data.read());
            }
        }

        pe_resp_out_.write(resp);

        pending_packets_.erase(it);
        return true;
    }

    return false;
}

} // namespace tlm