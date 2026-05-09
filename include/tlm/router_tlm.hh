// include/tlm/router_tlm.hh
// RouterTLM: 5 端口多跳路由器 (N/E/S/W/Local)
// 功能描述：NoC 路由器，实现六阶段流水线 (BW→RC→VA→SA→ST→LT) 和 Credit-based Flow Control
// 作者 CppTLM Team / 日期 2026-04-23
#ifndef TLM_ROUTER_TLM_HH
#define TLM_ROUTER_TLM_HH

#include "core/chstream_module.hh"
#include "core/param_rules.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "framework/bidirectional_port_adapter.hh"
#include "metrics/stats.hh"
#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace tlm {

/**
 * @brief 路由器端口方向枚举
 */
enum class RouterPort : unsigned {
    NORTH = 0,  // +Y 方向
    EAST  = 1,  // +X 方向
    SOUTH = 2,  // -Y 方向
    WEST  = 3,  // -X 方向
    LOCAL = 4,  // 本地 (连接 NICTLM)
    NUM_PORTS = 5
};

/**
 * @brief 路由器六阶段流水线状态
 */
struct RouterStageState {
    bool active = false;          // 本周期是否有 flit 处理
    bool vc_allocated = false;   // VA 阶段是否已分配 VC (避免重复分配)
    unsigned out_port = 0;        // 分配的输出端口
    unsigned out_vc = 0;          // 分配的输出 VC
    uint64_t packet_id = 0;      // 分组 ID (用于路由表查找)
};

/**
 * @brief 内部 Flit 封装 (包含流水线状态)
 */
struct RouterFlit {
    bundles::NoCFlitBundle bundle;
    unsigned in_port = 0;         // 输入端口
    unsigned in_vc = 0;          // 输入 VC
    RouterStageState stage;      // 流水线状态
    uint64_t cycle_received = 0; // 接收周期 (用于延迟统计)

    RouterFlit() = default;
    RouterFlit(const bundles::NoCFlitBundle& b, unsigned port, unsigned vc, uint64_t cycle)
        : bundle(b), in_port(port), in_vc(vc), cycle_received(cycle) {}
};

/**
 * @brief 路由算法接口 (可插拔)
 */
class RoutingAlgorithm {
public:
    virtual ~RoutingAlgorithm() = default;

    /**
     * @brief 计算路由方向
     * @param src_port 源端口
     * @param dst_node 目标节点 ID (Mesh XY 坐标编码)
     * @param node_x 本节点 X 坐标
     * @param node_y 本节点 Y 坐标
     * @param mesh_x Mesh X 维度
     * @param mesh_y Mesh Y 维度
     * @return 输出端口索引
     */
    virtual unsigned computeRoute(unsigned src_port, uint32_t dst_node,
                                  unsigned node_x, unsigned node_y,
                                  unsigned mesh_x, unsigned mesh_y) = 0;

    /**
     * @brief 从节点 ID 解码 X 坐标
     */
    static constexpr unsigned nodeToX(uint32_t node_id, unsigned mesh_x) {
        return node_id % mesh_x;
    }

    /**
     * @brief 从节点 ID 解码 Y 坐标
     */
    static constexpr unsigned nodeToY(uint32_t node_id, unsigned mesh_x) {
        return node_id / mesh_x;
    }

    /**
     * @brief 从 X,Y 坐标编码节点 ID
     */
    static constexpr uint32_t coordToNode(unsigned x, unsigned y, unsigned mesh_x) {
        return y * mesh_x + x;
    }
};

/**
 * @brief XY 维度顺序路由 (默认实现，死锁自由)
 */
class XYRouting : public RoutingAlgorithm {
public:
    unsigned computeRoute(unsigned src_port, uint32_t dst_node,
                          unsigned node_x, unsigned node_y,
                          unsigned mesh_x, unsigned mesh_y) override;
};

/**
 * @brief RouterTLM: 5 端口 NoC 路由器
 *
 * 特性:
 * - 5 个双向端口 (N/E/S/W/Local)
 * - 每端口 4 个虚拟通道 (VC)
 * - 每 VC 8 深 FIFO 输入缓冲区
 * - 六阶段流水线 (BW→RC→VA→SA→ST→LT)
 * - Credit-based Flow Control
 * - 可插拔路由算法 (默认 XY)
 *
 * 端口访问器 (遵循 BidirectionalPortAdapter 约定):
 *   req_in[port_idx]()  -> InputStreamAdapter<NoCFlitBundle>&   // 接收 flit
 *   resp_out[port_idx]() -> OutputStreamAdapter<NoCFlitBundle>&  // 发送 flit
 */
class RouterTLM : public ChStreamModuleBase {
public:
    // ========== 常量配置 ==========
    static constexpr unsigned NUM_PORTS = 5;
    static constexpr unsigned NUM_VCS = 4;
    static constexpr unsigned BUFFER_DEPTH = 8;
    static constexpr unsigned DEFAULT_MESH_X = 2;
    static constexpr unsigned DEFAULT_MESH_Y = 2;

    // ========== 类型别名 ==========
    using FlitBundle = bundles::NoCFlitBundle;
    using ReqAdapter = cpptlm::InputStreamAdapter<FlitBundle>;
    using RespAdapter = cpptlm::OutputStreamAdapter<FlitBundle>;
    using PortAdapter = cpptlm::BidirectionalPortAdapter<RouterTLM, FlitBundle, NUM_PORTS>;

    // ========== 构造函数 ==========
    /**
     * @brief 构造路由器
     * @param name 模块名称
     * @param eq 事件队列
     * @param node_x 本节点 X 坐标
     * @param node_y 本节点 Y 坐标
     * @param mesh_x Mesh X 维度
     * @param mesh_y Mesh Y 维度
     */
    RouterTLM(const std::string& name, EventQueue* eq,
              unsigned node_x = 0, unsigned node_y = 0,
              unsigned mesh_x = DEFAULT_MESH_X, unsigned mesh_y = DEFAULT_MESH_Y);
    ~RouterTLM() override = default;

    // ========== 端口访问器 (BidirectionalPortAdapter 需要) ==========
    std::array<ReqAdapter, NUM_PORTS>& req_in() { return req_in_; }
    std::array<RespAdapter, NUM_PORTS>& resp_out() { return resp_out_; }
    const std::array<ReqAdapter, NUM_PORTS>& req_in() const { return req_in_; }
    const std::array<RespAdapter, NUM_PORTS>& resp_out() const { return resp_out_; }

    // ========== ChStreamModuleBase 接口 ==========
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    unsigned num_ports() const override { return NUM_PORTS; }

    // ========== 周期精确仿真 ==========
    void tick() override;

    // ========== 路由算法 ==========
    void set_routing_algorithm(std::unique_ptr<RoutingAlgorithm> algo);
    RoutingAlgorithm* routing_algorithm() const { return routing_algo_.get(); }

    // ========== 拓扑配置 ==========
    unsigned node_x() const { return node_x_; }
    unsigned node_y() const { return node_y_; }
    unsigned mesh_x() const { return mesh_x_; }
    unsigned mesh_y() const { return mesh_y_; }
    uint32_t node_id() const { return node_x_ + node_y_ * mesh_x_; }

    // ========== 配置管理（Phase 1）==========
    void on_config_loaded() override;

    // ========== 参数规则（Phase 3.1）==========
    static cpptlm::ParamRules get_param_rules();

    // ========== Credit 超时配置 ==========
    void set_credit_timeout(uint64_t cycles) { credit_timeout_ = cycles; }
    uint64_t credit_timeout() const { return credit_timeout_; }

    // ========== Credit Flow Control 接口 (供 BidirectionalPortAdapter 调用) ==========
    void receive_credit(unsigned in_port, unsigned vc);

    // ========== 统计接口 ==========
    tlm_stats::StatGroup& stats() { return stat_group_; }

private:
    // ========== 流水线阶段 ==========
    void stage_buffer_write();
    void stage_route_computation();
    void stage_vc_allocation();
    void stage_switch_allocation();
    void stage_switch_traversal();
    void stage_link_traversal();

    // ========== 内部方法 ==========
    unsigned compute_xy_route(uint32_t dst_node);
    unsigned allocate_vc(unsigned out_port);
    void release_vc(unsigned out_port, unsigned vc);
    bool has_credit(unsigned out_port, unsigned vc) const;
    void consume_credit(unsigned out_port, unsigned vc);

    // ========== Credit 返回机制 ==========
    // 获取反向端口索引 (NORTH→SOUTH, EAST→WEST, etc.)
    static constexpr unsigned reverse_port(unsigned port) {
        switch (port) {
            case 0: return 2;  // NORTH → SOUTH
            case 1: return 3;  // EAST → WEST
            case 2: return 0;  // SOUTH → NORTH
            case 3: return 1;  // WEST → EAST
            case 4: return 4;  // LOCAL → LOCAL
            default: return 0;
        }
    }

    // 待返回的 Credit 信息
    struct PendingCreditReturn {
        unsigned port;      // 反向端口索引
        unsigned vc;        // VC ID
        unsigned remaining_cycles;  // 剩余延迟周期
    };
    std::queue<PendingCreditReturn> pending_credit_returns_;

    // 触发 Credit 返回到上游
    void return_credit_to_upstream(unsigned in_port, unsigned vc);
    // 发送 Credit 到上游（链路延迟后）
    void send_credit_to_upstream(unsigned reverse_port, unsigned vc);
    // 通过 BidirectionalPortAdapter 发送 Credit 信号（公共方法供 adapter 调用）
    void send_credit_signal(unsigned port, unsigned vc);

    // ========== 端口数组 ==========
    std::array<ReqAdapter, NUM_PORTS> req_in_;
    std::array<RespAdapter, NUM_PORTS> resp_out_;
    PortAdapter* adapter_ = nullptr;

    // ========== 拓扑配置 ==========
    unsigned node_x_ = 0;
    unsigned node_y_ = 0;
    unsigned mesh_x_ = DEFAULT_MESH_X;
    unsigned mesh_y_ = DEFAULT_MESH_Y;

    // ========== 路由算法 ==========
    std::unique_ptr<RoutingAlgorithm> routing_algo_;

    // ========== 输入缓冲区 [port][vc] ==========
    std::array<std::array<std::queue<RouterFlit>, NUM_VCS>, NUM_PORTS> input_buffer_;

    // ========== VC 状态 [port][vc] ==========
    struct VCState {
        bool allocated = false;
        bool has_credit = true;  // 默认有 credit
        unsigned out_port = 0;
        unsigned out_vc = 0;
        uint64_t packet_id = 0;
    };
    std::array<std::array<VCState, NUM_VCS>, NUM_PORTS> vc_state_;

    // ========== Credit 计数 [port][vc] ==========
    std::array<std::array<uint8_t, NUM_VCS>, NUM_PORTS> downstream_credits_;

    // ========== 路由表 [packet_id] -> {out_port, out_vc} ==========
    struct RoutingEntry {
        unsigned out_port;
        unsigned out_vc;
        bool valid;
    };
    std::unordered_map<uint64_t, RoutingEntry> routing_table_;

    // ========== 统计 (StatGroup) ==========
    tlm_stats::StatGroup stat_group_;
    tlm_stats::Scalar& stats_flits_forwarded_;
    tlm_stats::Scalar& stats_packets_forwarded_;
    tlm_stats::Scalar& stats_total_hops_;
    tlm_stats::Distribution& stats_latency_;

    // ========== 当前周期 (由 SimCore 驱动) ==========
    uint64_t current_cycle_ = 0;

    // ========== 流水线寄存器 (每周期保存中间状态) ==========
    RouterStageState pipe_reg_[NUM_PORTS][NUM_VCS];

    // ========== Credit 超时检测 (防止死锁) ==========
    uint64_t credit_timeout_ = 0;  // 默认关闭 (>0 时启用)
    std::array<std::array<uint64_t, NUM_VCS>, NUM_PORTS> last_credit_return_cycle_{};
    void credit_safety_reset();

    // ========== LT 阶段 pending queue (链路延迟建模) ==========
    struct PendingFlit {
        bundles::NoCFlitBundle bundle;
        unsigned out_port;
    };
    std::queue<PendingFlit> pending_link_;  // ST → LT 之间的 1 周期延迟队列

    // ========== SA 多 winner 支持 (每周期可转发多个 flit) ==========
    struct SAWinner {
        unsigned in_port;
        unsigned in_vc;
        unsigned out_port;
        unsigned out_vc;
    };
    std::vector<SAWinner> sa_winners_;  // 每周期可选中多个 winner（不同输出端口）
};

} // namespace tlm

#endif // TLM_ROUTER_TLM_HH
