// include/framework/axi4_stream_adapter.hh
// AXI4 / AXI4-Lite Stream Adapter（3 端口 valid/ready 握手）
// 功能描述：PcieEndpointIP 向 SoC 暴露的 AXI 事务边界适配器。
//           三端口：axi_master_out（EP 发起 SoC 访问）/ axi_slave_in（SoC 发起
//           进入 Endpoint 的事务）/ cfg_slave_in（AXI4-Lite 配置访问）。
//           支持 valid/ready 反压（不丢事务）与 outstanding 请求 ID 关联
//           （awid→bid / arid→rid，Phase 6 AXI4Mapper 消费）。
// 作者 CppTLM Team / 日期 2026-11-03
// 参考: openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/spec.md
//       openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §6
#ifndef FRAMEWORK_AXI4_STREAM_ADAPTER_HH
#define FRAMEWORK_AXI4_STREAM_ADAPTER_HH

#include "bundles/axi4_bundles_tlm.hh"

#include <deque>
#include <cstdint>

namespace cpptlm {

/**
 * @brief AXI4/AXI4-Lite Stream Adapter（3 端口）
 *
 * 端口语义（per spec.md）：
 *   - axi_master_out : EP（master）发起对 SoC 的读/写访问。EP 调用 master_req()
 *                      注入请求，下游经 valid/ready 握手消费；响应经 master_resp()
 *                      注入回 EP。
 *   - axi_slave_in   : SoC（master）发起进入 Endpoint 的事务。请求经 slave_req()
 *                      注入，EP 消费；EP 响应经 slave_resp() 返回。
 *   - cfg_slave_in   : AXI4-Lite 配置访问。请求经 cfg_req() 注入，EP 消费；
 *                      EP 响应经 cfg_resp() 返回。
 *
 * 握手规则（valid/ready backpressure）：
 *   - 每个方向的请求/响应通道有独立 valid/ready 信号。
 *   - 通道 empty 且 valid 时 tick() 仅在 ready=1 才推进（不丢事务）。
 *   - 生产者调用 master_req()/slave_req()/cfg_req() 设置 valid；
 *     消费者在 ready=1 且 tick() 后调用 consume() 清 valid。
 *
 * ID 关联（Phase 6 OOO 预留）：
 *   - master 侧跟踪 outstanding 写/读请求 ID（awid/arid）。
 *   - master_resp() 返回时按 bid/rid 匹配并移除对应 outstanding 条目。
 */
class Axi4StreamAdapter {
public:
    Axi4StreamAdapter() = default;
    Axi4StreamAdapter(const Axi4StreamAdapter&) = delete;
    Axi4StreamAdapter& operator=(const Axi4StreamAdapter&) = delete;

    // ===================== axi_master_out（EP → SoC）=====================
    // EP 注入 AXI4 请求（写或读）。返回 true 表示接受（valid 置位），
    // false 表示上一请求尚未被下游消费（backpressure）。
    bool master_req(const bundles::Axi4Bundle& req);
    // 下游 ready 信号（SoC 侧）
    void set_master_ready(bool ready);
    bool master_ready() const;
    // 请求通道状态
    bool master_req_valid() const;
    // 下游消费当前请求（ready=1 且 tick() 后调用）
    void master_req_consume();

    // SoC 返回响应注入（写响应 bid/bresp 或读响应 rid/rdata/rresp/rlast）
    bool master_resp(const bundles::Axi4Bundle& resp);
    bool master_resp_valid() const;
    const bundles::Axi4Bundle& master_resp_data() const;
    void master_resp_consume();

    // outstanding 请求 ID 计数
    std::size_t outstanding_wr() const { return outstanding_wr_ids_.size(); }
    std::size_t outstanding_rd() const { return outstanding_rd_ids_.size(); }

    // ===================== axi_slave_in（SoC → EP）=====================
    // SoC 注入 AXI4 请求（写或读）
    bool slave_req(const bundles::Axi4Bundle& req);
    void set_slave_ready(bool ready);
    bool slave_ready() const;
    bool slave_req_valid() const;
    void slave_req_consume();
    const bundles::Axi4Bundle& slave_req_data() const;

    // EP 返回响应（SoC 侧读取）
    bool slave_resp(const bundles::Axi4Bundle& resp);
    bool slave_resp_valid() const;
    void slave_resp_consume();
    const bundles::Axi4Bundle& slave_resp_data() const;

    // ===================== cfg_slave_in（AXI4-Lite 配置）=====================
    // SoC 注入 AXI4-Lite 配置请求
    bool cfg_req(const bundles::Axi4LiteBundle& req);
    void set_cfg_ready(bool ready);
    bool cfg_ready() const;
    bool cfg_req_valid() const;
    void cfg_req_consume();
    const bundles::Axi4LiteBundle& cfg_req_data() const;

    // EP 返回配置响应
    bool cfg_resp(const bundles::Axi4LiteBundle& resp);
    bool cfg_resp_valid() const;
    void cfg_resp_consume();
    const bundles::Axi4LiteBundle& cfg_resp_data() const;

    // 每周期推进：仅在 ready=1 时转移事务（backpressure 语义）
    void tick();

    // 全量复位
    void reset();

private:
    // ---- master 请求通道 ----
    bundles::Axi4Bundle master_req_data_{};
    bool master_req_valid_ = false;
    bool master_ready_ = true;   // 下游默认 ready

    // ---- master 响应通道 ----
    bundles::Axi4Bundle master_resp_data_{};
    bool master_resp_valid_ = false;

    // ---- slave 请求通道 ----
    bundles::Axi4Bundle slave_req_data_{};
    bool slave_req_valid_ = false;
    bool slave_ready_ = true;

    // ---- slave 响应通道 ----
    bundles::Axi4Bundle slave_resp_data_{};
    bool slave_resp_valid_ = false;

    // ---- cfg 请求通道 ----
    bundles::Axi4LiteBundle cfg_req_data_{};
    bool cfg_req_valid_ = false;
    bool cfg_ready_ = true;

    // ---- cfg 响应通道 ----
    bundles::Axi4LiteBundle cfg_resp_data_{};
    bool cfg_resp_valid_ = false;

    // ---- outstanding 请求 ID（master 侧）----
    std::deque<uint16_t> outstanding_wr_ids_;
    std::deque<uint16_t> outstanding_rd_ids_;
};

} // namespace cpptlm

#endif // FRAMEWORK_AXI4_STREAM_ADAPTER_HH
