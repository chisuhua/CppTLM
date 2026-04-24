// src/tlm/router_tlm.cc
// RouterTLM: 5 端口多跳路由器实现
// 功能描述：实现六阶段流水线 (BW→RC→VA→SA→ST→LT) 和 Credit-based Flow Control
// 作者 CppTLM Team / 日期 2026-04-23
#include "tlm/router_tlm.hh"
#include "core/module_factory.hh"
#include "core/sim_core.hh"
#include <algorithm>
#include <cassert>

namespace tlm {

// ============================================================================
// XYRouting 实现
// ============================================================================

unsigned XYRouting::computeRoute(unsigned src_port, uint32_t dst_node,
                                unsigned node_x, unsigned node_y,
                                unsigned mesh_x, unsigned mesh_y) {
    unsigned dst_x = nodeToX(dst_node, mesh_x);
    unsigned dst_y = nodeToY(dst_node, mesh_x);

    // XY 路由：首先在 X 方向移动，然后在 Y 方向移动
    if (dst_x > node_x) {
        return static_cast<unsigned>(RouterPort::EAST);
    } else if (dst_x < node_x) {
        return static_cast<unsigned>(RouterPort::WEST);
    } else if (dst_y > node_y) {
        return static_cast<unsigned>(RouterPort::NORTH);
    } else if (dst_y < node_y) {
        return static_cast<unsigned>(RouterPort::SOUTH);
    } else {
        // 到达目标，返回本地端口
        return static_cast<unsigned>(RouterPort::LOCAL);
    }
}

// ============================================================================
// RouterTLM 构造函数
// ============================================================================

RouterTLM::RouterTLM(const std::string& name, EventQueue* eq,
                     unsigned node_x, unsigned node_y,
                     unsigned mesh_x, unsigned mesh_y)
    : ChStreamModuleBase(name, eq),
      node_x_(node_x),
      node_y_(node_y),
      mesh_x_(mesh_x),
      mesh_y_(mesh_y) {
    // 初始化默认路由算法
    routing_algo_ = new XYRouting();

    // 初始化 downstream credits (默认每 VC 8 credits = BUFFER_DEPTH)
    for (unsigned p = 0; p < NUM_PORTS; ++p) {
        for (unsigned v = 0; v < NUM_VCS; ++v) {
            downstream_credits_[p][v] = BUFFER_DEPTH;
        }
    }

    // 初始化统计
    stats_ = RouterStats{};
}

// ============================================================================
// ChStreamModuleBase 接口
// ============================================================================

void RouterTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    adapter_ = static_cast<PortAdapter*>(adapter);
}

// ============================================================================
// 路由算法设置
// ============================================================================

void RouterTLM::set_routing_algorithm(RoutingAlgorithm* algo) {
    if (routing_algo_) {
        delete routing_algo_;
    }
    routing_algo_ = algo;
}

// ============================================================================
// 六阶段流水线 tick()
// ============================================================================

void RouterTLM::tick() {
    current_cycle_ = getCurrentCycle();

    // 阶段 1: Buffer Write (BW)
    stage_buffer_write();

    // 阶段 2: Route Computation (RC)
    stage_route_computation();

    // 阶段 3: VC Allocation (VA)
    stage_vc_allocation();

    // 阶段 4: Switch Allocation (SA)
    stage_switch_allocation();

    // 阶段 5: Switch Traversal (ST)
    stage_switch_traversal();

    // 阶段 6: Link Traversal (LT)
    stage_link_traversal();

    // Credit 恢复 (隐式 return，防止永久阻塞)
    restore_credits();
}

// ============================================================================
// 阶段 1: Buffer Write
// ============================================================================

void RouterTLM::stage_buffer_write() {
    for (unsigned port = 0; port < NUM_PORTS; ++port) {
        auto& req_adapter = req_in_[port];
        if (req_adapter.valid()) {
            // 从 bundle 读取 VC ID
            unsigned vc = req_adapter.data().vc_id.read();
            if (vc >= NUM_VCS) vc = 0;  // 边界保护

            // 读取 flit
            RouterFlit flit(req_adapter.data(), port, vc, current_cycle_);
            req_adapter.consume();

            // 检查缓冲区空间
            if (input_buffer_[port][vc].size() < BUFFER_DEPTH) {
                input_buffer_[port][vc].push(flit);
            }
        }
    }
}

// ============================================================================
// 阶段 2: Route Computation
// ============================================================================

void RouterTLM::stage_route_computation() {
    for (unsigned port = 0; port < NUM_PORTS; ++port) {
        for (unsigned vc = 0; vc < NUM_VCS; ++vc) {
            auto& buf = input_buffer_[port][vc];
            if (buf.empty()) continue;

            RouterFlit& flit = buf.front();
            auto& stage = flit.stage;

            if (stage.active) continue;  // 已在流水线中

            // HEAD flit 需要路由计算
            if (flit.bundle.is_head()) {
                unsigned out_port = routing_algo_->computeRoute(
                    port,
                    flit.bundle.dst_node.read(),
                    node_x_, node_y_,
                    mesh_x_, mesh_y_
                );

                // 更新流水线状态
                stage.active = true;
                stage.out_port = out_port;
                stage.out_vc = vc;
                stage.packet_id = flit.bundle.transaction_id.read();

                // 记录到路由表
                RoutingEntry entry{out_port, vc, true};
                routing_table_[stage.packet_id] = entry;
            } else {
                // BODY/TAIL flit: 查表获取已计算的路由
                uint64_t pkt_id = flit.bundle.transaction_id.read();
                auto it = routing_table_.find(pkt_id);
                if (it != routing_table_.end() && it->second.valid) {
                    stage.active = true;
                    stage.out_port = it->second.out_port;
                    stage.out_vc = it->second.out_vc;
                    stage.packet_id = pkt_id;
                }
            }
        }
    }
}

// ============================================================================
// 阶段 3: VC Allocation
// ============================================================================

void RouterTLM::stage_vc_allocation() {
    for (unsigned port = 0; port < NUM_PORTS; ++port) {
        for (unsigned vc = 0; vc < NUM_VCS; ++vc) {
            auto& buf = input_buffer_[port][vc];
            if (buf.empty()) continue;

            RouterFlit& flit = buf.front();
            if (!flit.stage.active) continue;
            if (flit.stage.vc_allocated) continue;  // 跳过已分配 VC 的 flit

            unsigned out_port = flit.stage.out_port;
            unsigned out_vc = allocate_vc(out_port);

            if (out_vc < NUM_VCS) {
                // VA 成功，更新 out_vc 并标记
                flit.stage.out_vc = out_vc;
                flit.stage.vc_allocated = true;
            }
        }
    }
}

// ============================================================================
// 阶段 4: Switch Allocation
// ============================================================================

void RouterTLM::stage_switch_allocation() {
    // 清零仲裁结果
    sa_grant_ = false;
    sa_winner_port_ = NUM_PORTS;
    sa_winner_vc_ = NUM_VCS;

    // 遍历所有输入端口，寻找请求同一输出端口的 flits
    for (unsigned out_port = 0; out_port < NUM_PORTS; ++out_port) {
        // 检查下游 credit
        for (unsigned vc = 0; vc < NUM_VCS; ++vc) {
            if (!has_credit(out_port, vc)) continue;

            // 找到第一个等待此输出端口的 flit
            for (unsigned in_port = 0; in_port < NUM_PORTS; ++in_port) {
                for (unsigned in_vc = 0; in_vc < NUM_VCS; ++in_vc) {
                    auto& buf = input_buffer_[in_port][in_vc];
                    if (buf.empty()) continue;

                    RouterFlit& flit = buf.front();
                    if (!flit.stage.active) continue;
                    if (flit.stage.out_port != out_port) continue;

                    // 选中此 flit
                    sa_grant_ = true;
                    sa_winner_port_ = in_port;
                    sa_winner_vc_ = in_vc;
                    return;  // 每个周期只仲裁一次
                }
            }
        }
    }
}

// ============================================================================
// 阶段 5: Switch Traversal
// ============================================================================

void RouterTLM::stage_switch_traversal() {
    if (!sa_grant_) return;
    if (sa_winner_port_ >= NUM_PORTS || sa_winner_vc_ >= NUM_VCS) return;

    auto& buf = input_buffer_[sa_winner_port_][sa_winner_vc_];
    if (buf.empty()) return;

    RouterFlit flit = buf.front();
    buf.pop();

    unsigned out_port = flit.stage.out_port;
    unsigned out_vc = flit.stage.out_vc;

    // 消耗下游 credit
    consume_credit(out_port, out_vc);

    // 更新 hop 计数
    flit.bundle.hops.write(flit.bundle.hops.read() + 1);

    // 写入 pending link 队列 (LT 阶段才会真正发送到 resp_out_)
    pending_link_.push({flit.bundle, out_port});

    // 更新统计
    stats_.flits_forwarded++;
    stats_.total_hops += flit.bundle.hops.read();
    stats_.total_latency_cycles += (current_cycle_ - flit.cycle_received);

    // TAIL flit: 释放 VC，更新 packet 统计
    if (flit.bundle.is_tail()) {
        release_vc(out_port, out_vc);
        stats_.packets_forwarded++;

        // 清理路由表
        uint64_t pkt_id = flit.bundle.transaction_id.read();
        routing_table_.erase(pkt_id);
    }

    // 清理流水线状态
    flit.stage.active = false;
}

// ============================================================================
// 阶段 6: Link Traversal
// ============================================================================

void RouterTLM::stage_link_traversal() {
    // 从 pending_link_ 队列取出 flit，发送到 resp_out_
    // 这实现了 1 周期链路传输延迟建模
    while (!pending_link_.empty()) {
        auto pf = pending_link_.front();
        resp_out_[pf.out_port].write(pf.bundle);
        pending_link_.pop();
    }
}

// ============================================================================
// VC 分配
// ============================================================================

unsigned RouterTLM::allocate_vc(unsigned out_port) {
    for (unsigned vc = 0; vc < NUM_VCS; ++vc) {
        if (!vc_state_[out_port][vc].allocated) {
            vc_state_[out_port][vc].allocated = true;
            return vc;
        }
    }
    return NUM_VCS;  // 无可用 VC
}

void RouterTLM::release_vc(unsigned out_port, unsigned vc) {
    if (vc < NUM_VCS) {
        vc_state_[out_port][vc].allocated = false;
    }
}

// ============================================================================
// Credit-based Flow Control
// ============================================================================

bool RouterTLM::has_credit(unsigned out_port, unsigned vc) const {
    if (out_port >= NUM_PORTS || vc >= NUM_VCS) return false;
    return downstream_credits_[out_port][vc] > 0;
}

void RouterTLM::consume_credit(unsigned out_port, unsigned vc) {
    if (out_port >= NUM_PORTS || vc >= NUM_VCS) return;
    if (downstream_credits_[out_port][vc] > 0) {
        downstream_credits_[out_port][vc]--;
    }
}

void RouterTLM::receive_credit(unsigned in_port, unsigned vc) {
    if (in_port >= NUM_PORTS || vc >= NUM_VCS) return;
    if (downstream_credits_[in_port][vc] < BUFFER_DEPTH) {
        downstream_credits_[in_port][vc]++;
    }
}

void RouterTLM::restore_credits() {
    if (current_cycle_ - last_credit_restore_cycle_ >= BUFFER_DEPTH) {
        for (unsigned p = 0; p < NUM_PORTS; ++p) {
            for (unsigned v = 0; v < NUM_VCS; ++v) {
                downstream_credits_[p][v] = BUFFER_DEPTH;
            }
        }
        last_credit_restore_cycle_ = current_cycle_;
    }
}

// ============================================================================
// XY 路由便捷方法
// ============================================================================

unsigned RouterTLM::compute_xy_route(uint32_t dst_node) {
    return routing_algo_->computeRoute(
        0,  // src_port 仅用于边界情况
        dst_node,
        node_x_, node_y_,
        mesh_x_, mesh_y_
    );
}

// ============================================================================
// 注册到 ModuleFactory
// ============================================================================

namespace {
struct RouterTLMRegistrar {
    RouterTLMRegistrar() {
        ModuleFactory::registerObject<RouterTLM>("RouterTLM");
    }
};
static RouterTLMRegistrar g_registrar;
}

} // namespace tlm
