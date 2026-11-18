// src/tlm/nic_tlm.cc
// NICTLM: Network Interface Card TLM 模块实现
// 功能描述：实现 packetize/reassemble 和 PE↔Net 双向转发
// 作者 CppTLM Team / 日期 2026-04-24
#include "tlm/nic_tlm.hh"
#include "core/module_factory.hh"

namespace tlm {

    NICTLM::NICTLM(const std::string& name, EventQueue* eq, uint32_t node_id, uint32_t mesh_x,
                   uint32_t mesh_y)
        : ChStreamModuleBase(name, eq), node_id_(node_id), mesh_x_(mesh_x), mesh_y_(mesh_y),
          stat_group_("nic"),
          stats_flits_sent_(stat_group_.addScalar("flits_sent", "Total flits sent", "flits")),
          stats_flits_received_(
              stat_group_.addScalar("flits_received", "Total flits received", "flits")),
          stats_packets_sent_(
              stat_group_.addScalar("packets_sent", "Total packets sent", "packets")),
          stats_packets_received_(
              stat_group_.addScalar("packets_received", "Total packets received", "packets")),
          stats_credits_dropped_(
              stat_group_.addScalar("credits_dropped", "Total credit flits dropped", "flits")),
          stats_latency_(
              stat_group_.addDistribution("latency", "Packet end-to-end latency", "cycle")) {
    }

    void NICTLM::on_config_loaded() {
        // ── 声明即读取（`ADR-X.17` D-1）────────────────────────────────────
        // `node_id` **必填**（无默认值）—— 它是 CFG-08 曾经的唯一强制点，
        // 现由本行的缺省语义接管（`include/core/validate` 的 per-type 分支已退役）。
        load_param("node_id", node_id_);
        // `mesh_x` / `mesh_y` 可选（默认取构造期传入值）——
        // 它们是**网格维度**而非节点坐标，缺失不构成配置错误。
        load_param_opt("mesh_x", mesh_x_);
        load_param_opt("mesh_y", mesh_y_);

        // `address_regions` 是**非标量**（对象数组）⇒ `ADR-X.17` D-2 态②
        // （`load_param` 只承载标量）⇒ 模块内解析 + 显式登记「已消费」。
        ignore_param("address_regions");
        const auto& cfg = get_config();
        if (cfg.contains("address_regions")) {
            for (const auto& reg : cfg["address_regions"]) {
                add_address_region(reg["base"].get<uint64_t>(), reg["size"].get<uint64_t>(),
                                   reg["target_node"].get<uint32_t>(),
                                   reg.value("target_type", "MEMORY_CTRL"));
            }
        }
        DPRINTF(MODULE, "[CONFIG] NICTLM %s: node_id=%u mesh=(%u,%u) addr_regions=%zu\n",
                name.c_str(), node_id_, mesh_x_, mesh_y_, addr_map_.num_regions());
    }

    void NICTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
        adapter_ = adapter;
    }

    void NICTLM::tick() {
        // 处理输入请求（可能向队列添加 flits）
        handle_pe_req();
        handle_net_resp();

        // 发送队列中的 flit（如果有且输出空闲）
        if (!net_req_out_.valid() && !pending_flit_queue_.empty()) {
            net_req_out_.write(pending_flit_queue_.front());
            pending_flit_queue_.pop();
            ++stats_flits_sent_;
        }

        // 委托适配器 tick（输出方向数据搬运）
        if (adapter_)
            adapter_->tick();
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

        // credit flit 与数据 flit 共用同一通道（`RouterTLM` 的 credit 回流经
        // `BidirectionalPortAdapter::send_credit_impl` 写 `resp_out`，bundle 字段
        // `flit_category = CATEGORY_CREDIT`；`reverse_port(LOCAL) == LOCAL` ⇒ 经 LOCAL
        // 口进入 router 的 flit，其 credit 沿 `resp_out[LOCAL]` 回流到本模块）
        // ⇒ 显式丢弃：不计数、不进 reassemble、不写 pe_resp_out_
        //（否则产出事务号 / 地址 / 数据全 0 的垃圾响应）。
        if (flit.flit_category.read() == bundles::NoCFlitBundle::CATEGORY_CREDIT) {
            ++stats_credits_dropped_;
            return true;  // flit 已被消费 ⇒ 返回值与既有「处理了 1 个 flit」语义一致
        }

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

            pending_flit_queue_.push(flit);
        }

        ++stats_packets_sent_;
    }

    bool NICTLM::reassemble(const bundles::NoCFlitBundle& flit) {
        uint64_t tid = flit.transaction_id.read();

        auto it = std::find_if(pending_packets_.begin(), pending_packets_.end(),
                               [tid](const PendingPacket& p) { return p.transaction_id == tid; });

        bool is_new_packet = false;
        if (it == pending_packets_.end()) {
            if (flit.is_head()) {
                PendingPacket p;
                p.transaction_id = tid;
                p.dst_node = flit.dst_node.read();
                p.flits_received = 0;
                p.is_write = flit.is_write.read();
                p.address = flit.address.read();
                p.flits = {};
                uint8_t idx = flit.flit_index.read();
                if (idx < FLITS_PER_PACKET) {
                    p.flits[idx] = flit;
                }
                pending_packets_.push_back(p);
                it = std::prev(pending_packets_.end());
                is_new_packet = true;
            } else {
                return false;
            }
        }

        uint8_t idx = flit.flit_index.read();
        if (idx < FLITS_PER_PACKET) {
            it->flits[idx] = flit;
        }
        ++it->flits_received;

        if (flit.is_tail() || is_new_packet) {
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