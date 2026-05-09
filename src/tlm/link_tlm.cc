// src/tlm/link_tlm.cc
// LinkTLM: 物理链路模块实现
// 功能描述：链路延迟建模和 Credit-based Flow Control
// 作者 CppTLM Team / 日期 2026-04-27
#include "tlm/link_tlm.hh"
#include "core/module_factory.hh"

namespace tlm {

// ============================================================================
// 构造函数
// ============================================================================

LinkTLM::LinkTLM(const std::string& name, EventQueue* eq, unsigned latency)
    : ChStreamModuleBase(name, eq),
      latency_(latency) {}

// ============================================================================
// 周期精确仿真 tick()
// ============================================================================

void LinkTLM::tick() {
    // -------- 处理延迟队列中的 flits --------
    // 将需要继续延迟的 flits 移到临时队列
    std::queue<DelayedFlit> next_delay_queue;
    while (!delay_queue_.empty()) {
        DelayedFlit df = delay_queue_.front();
        delay_queue_.pop();

        df.remaining_cycles--;
        if (df.remaining_cycles == 0) {
            // flit 到达目的地，通过 resp_out 发送
            resp_out_.write(df.flit);
            stats_.flits_forwarded++;
        } else {
            next_delay_queue.push(df);
        }
    }
    delay_queue_ = std::move(next_delay_queue);

    // -------- 处理 credit 返回队列 --------
    std::queue<CreditReturn> next_credit_queue;
    while (!credit_queue_.empty()) {
        CreditReturn cr = credit_queue_.front();
        credit_queue_.pop();

        cr.remaining_cycles--;
        if (cr.remaining_cycles == 0) {
            // credit 返回到上游（通过 req_in 反向传递）
            // 注意：这里只是占位调用，实际 credit 返回机制需要上层连接解析
            DPRINTF(MODULE, "[LinkTLM] %s: Credit return port=%u vc=%u\n",
                    getName().c_str(), cr.port, cr.vc);
            stats_.credits_returned++;
        } else {
            next_credit_queue.push(cr);
        }
    }
    credit_queue_ = std::move(next_credit_queue);

    // -------- 处理输入 flit --------
    if (req_in_.valid() && req_in_.ready()) {
        auto flit = req_in_.data();
        req_in_.consume();

        // 构造延迟 flit
        DelayedFlit df;
        df.flit = flit;
        df.remaining_cycles = latency_;
        df.src_port = flit.src_port.read();
        delay_queue_.push(df);

        // 启动 credit 返回延迟（与 flit 延迟相同方向，但反向传递）
        // Credit 从下游传回上游，所以这里模拟返回路径
        CreditReturn cr;
        cr.port = df.src_port;  // 源端口（上游路由器端口）
        cr.vc = flit.vc_id.read();
        cr.remaining_cycles = latency_;
        credit_queue_.push(cr);

        DPRINTF(MODULE, "[LinkTLM] %s: Received flit tid=%lu vc=%u, delay=%u cycles\n",
                getName().c_str(), flit.transaction_id.read(), flit.vc_id.read(), latency_);
    }

    // 委托适配器 tick（处理输出方向的数据搬运）
    if (adapter_) {
        adapter_->tick();
    }

    // 统计活跃周期
    if (!delay_queue_.empty() || !credit_queue_.empty()) {
        stats_.cycles_active++;
    }
}

// ============================================================================
// Credit 返回接口（供外部调用）
// ============================================================================

void LinkTLM::receive_credit(unsigned port, unsigned vc) {
    // 外部（下游路由器）调用此方法返回 credit
    // LinkTLM 将 credit 延迟后传回上游
    CreditReturn cr;
    cr.port = port;
    cr.vc = vc;
    cr.remaining_cycles = latency_;
    credit_queue_.push(cr);

    DPRINTF(MODULE, "[LinkTLM] %s: Received external credit port=%u vc=%u\n",
            getName().c_str(), port, vc);
}

// ============================================================================
// 注册到 ModuleFactory（静态注册）
// ============================================================================

namespace {
struct LinkTLMRegistrar {
    LinkTLMRegistrar() {
        ModuleFactory::registerObject<LinkTLM>("LinkTLM");
    }
};
static LinkTLMRegistrar g_registrar;
}

} // namespace tlm