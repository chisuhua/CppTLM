// include/tlm/gpu/gpu_tlm.hh
// GPUTLM v0 — GPU 黑盒发起器（ChStreamModuleBase 派生）
// 功能描述：作为单端口 Initiator，tick() 中按 kernel_duration_ 周期发出
//           ComputeReqBundle。v0 不模拟 SIMD pipeline / ISA / LDS / HSA
//           Runtime（D2/D3/D4 决策：推迟到 Phase7.B+）。
//           5 个程序化 setter 控制 num_kernels / kernel_duration /
//           num_workgroups / workgroup_size / coalescing_factor。
// 作者 CppTLM Team / 日期 2026-06-11
// 参考：docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md §3
//      gem5 src/gpu-compute/ComputeUnit.py (字段语义对位)
//      gem5 src/gpu-compute/Wavefront.py (status_e 状态机延后)
#ifndef TLM_GPU_GPU_TLM_HH
#define TLM_GPU_GPU_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/compute_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>
#include <unordered_map>
#include <random>

class GPUTLM : public ChStreamModuleBase {
private:
    // === 适配器（与 CPUTLM 完全同型）===
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>  resp_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>  req_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // === 黑盒参数默认值 ===
    uint32_t num_kernels_       = 1;
    uint32_t kernel_duration_   = 100;
    uint32_t num_workgroups_    = 4;
    uint32_t workgroup_size_    = 64;
    uint32_t coalescing_factor_ = 1;

    // === 运行期状态 ===
    uint32_t cur_kernel_id_       = 0;
    uint64_t next_txn_id_         = 1;
    uint32_t cycles_since_launch_ = 0;
    bool     kernel_active_       = false;

    std::unordered_map<uint64_t, uint64_t> inflight_txns_; // txn_id → issue_cycle
    std::mt19937 rng_;

public:
    explicit GPUTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          rng_(std::random_device{}()) {}

    std::string get_module_type() const override { return "GPUTLM"; }

    // === 适配器注入 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // === 黑盒行为参数（程序化 setter，JSON 解析在 Phase7.B）===
    void set_num_kernels(uint32_t n)         { num_kernels_       = n; }
    void set_kernel_duration(uint32_t cyc)   { kernel_duration_   = cyc; }
    void set_num_workgroups(uint32_t n)      { num_workgroups_    = n; }
    void set_workgroup_size(uint32_t sz)     { workgroup_size_    = sz; }
    void set_coalescing_factor(uint32_t cf)  { coalescing_factor_ = cf; }

    // === 统计 getter（测试使用，v0 公开直接访问）===
    uint64_t stats_requests_issued()      const { return requests_issued_.value(); }
    uint64_t stats_requests_completed()   const { return requests_completed_.value(); }
    uint64_t stats_kernels_launched()     const { return kernels_launched_.value(); }
    uint64_t stats_workgroups_dispatched() const { return workgroups_dispatched_.value(); }
    uint64_t stats_writes()                const { return writes_.value(); }
    uint64_t stats_reads()                 const { return reads_.value(); }

    void tick() override {
        // 1. 响应消费
        if (resp_in_.valid() && resp_in_.ready()) {
            auto& resp = resp_in_.data();
            uint64_t txn_id = resp.transaction_id.read();
            auto it = inflight_txns_.find(txn_id);
            if (it != inflight_txns_.end()) {
                uint64_t issue_cycle = it->second;
                uint64_t cur_cycle = getCurrentCycle();
                latency_.sample(cur_cycle - issue_cycle);
                inflight_txns_.erase(it);
            }
            resp_in_.consume();
            requests_completed_++;
        }

        // 2. 请求发起
        if (!kernel_active_) {
            if (cur_kernel_id_ < num_kernels_) {
                kernel_active_ = true;
                cycles_since_launch_ = 0;
                cur_kernel_id_++;
                kernels_launched_++;
            }
        } else {
            if (cycles_since_launch_ < kernel_duration_) {
                uint32_t reqs_per_wg =
                    (workgroup_size_ + coalescing_factor_ - 1) / coalescing_factor_;
                for (uint32_t wg = 0; wg < num_workgroups_; ++wg) {
                    for (uint32_t wf = 0; wf < reqs_per_wg; ++wf) {
                        bool is_wr = (rng_() % 2 == 0);
                        uint64_t addr = 0x10000ULL + uint64_t(wg) * 0x1000ULL;
                        uint64_t cur_cycle = getCurrentCycle();

                        bundles::ComputeReqBundle req;
                        req.transaction_id.write(next_txn_id_);
                        req.parent_id.write(0);
                        req.fragment_id.write(0);
                        req.fragment_total.write(1);
                        req.address.write(addr);
                        req.size.write(4);
                        req.is_write.write(is_wr);
                        req.data.write(0xCAFEBABEULL);
                        req.kernel_id.write(cur_kernel_id_);
                        req.workgroup_id.write(wg);
                        req.wavefront_id.write(wf);
                        req.coalescing_factor.write(coalescing_factor_);

                        inflight_txns_[next_txn_id_] = cur_cycle;
                        req_out_.write(req);
                        next_txn_id_++;
                        requests_issued_++;
                        if (is_wr) writes_++; else reads_++;
                    }
                }
                workgroups_dispatched_ += num_workgroups_;
            } else {
                kernel_active_ = false;
            }
        }

        // 3. 周期计数
        cycles_since_launch_++;

        // 4. Adapter tick
        if (adapter_) adapter_->tick();
    }

    void do_reset(const ResetConfig& config) override {
        (void)config;
        resp_in_.reset();
        req_out_.reset();
        cur_kernel_id_ = 0;
        next_txn_id_ = 1;
        cycles_since_launch_ = 0;
        kernel_active_ = false;
        inflight_txns_.clear();
        kernels_launched_.reset();
        workgroups_dispatched_.reset();
        requests_issued_.reset();
        requests_completed_.reset();
        writes_.reset();
        reads_.reset();
        latency_.reset();
    }

    tlm_stats::StatGroup* get_stats_group() override {
        if (!stats_group_) {
            stats_group_ = std::make_unique<tlm_stats::StatGroup>(getName());
            stats_group_->addScalar("kernels_launched");
            stats_group_->addScalar("workgroups_dispatched");
            stats_group_->addScalar("requests_issued");
            stats_group_->addScalar("requests_completed");
            stats_group_->addScalar("writes");
            stats_group_->addScalar("reads");
            stats_group_->addDistribution("latency");
        }
        // 把当前 counter 同步进 group（v0 简化：仅 reset 时清零，每次 get 都重新同步）
        return stats_group_.get();
    }

    // 适配器访问器（StreamAdapter::tick() 用，与 CPUTLM 同型）
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>&  resp_in() { return resp_in_; }
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>&  req_out() { return req_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

    // Initiator 不接收请求：dummy 适配器满足 StreamAdapter 接口
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle>& resp_out() {
        static cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>& req_in() {
        static cpptlm::InputStreamAdapter<bundles::ComputeReqBundle> dummy;
        return dummy;
    }

private:
    // === StatGroup 持有（lazy init）===
    std::unique_ptr<tlm_stats::StatGroup> stats_group_;

    // === 统计实例 ===
    tlm_stats::Scalar       kernels_launched_{"kernels_launched"};
    tlm_stats::Scalar       workgroups_dispatched_{"workgroups_dispatched"};
    tlm_stats::Scalar       requests_issued_{"requests_issued"};
    tlm_stats::Scalar       requests_completed_{"requests_completed"};
    tlm_stats::Scalar       writes_{"writes"};
    tlm_stats::Scalar       reads_{"reads"};
    tlm_stats::Distribution latency_{"latency"};
};

#endif // TLM_GPU_GPU_TLM_HH