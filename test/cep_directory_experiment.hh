// test/cep_directory_experiment.hh
// H2 / H3 共用的 home 侧目录实验模型
//
// 功能:把 `SharerTrackerIf` 的行为差异转成**可测量的延迟与 snoop 数** ——
//   · H2:同一 workload 下三种目录策略(集中 / 分布 / 广播)的延迟对比
//   · H3:分布策略 CONSERVATIVE 完备性(上界不漏探)+ 运行时双不变量
// 作者: CppTLM Team / 日期: 2026-11-04
//
// ── 口径限定(必须在结论里重复)─────────────────────────────────
// 本模型**不是**协议级仿真:它固定 RN 侧为同一行为,只让 **home 侧目录策略**变化。
// ⇒ 结论的适用范围 =「目录策略对延迟的影响」,**不**是「CHI 协议 vs CXL 协议的延迟差」。
//   该限定须写入台账(防后续误读),per oracle 裁决 2026-11-04。
//
// ── 延迟参数来源 ────────────────────────────────────────────────
// `snoop_latency = 2` 取自 `include/tlm/cep/stub_cache_core.hh:158` 的
// `snoop_latency_cycles = 2`(**已有**的参数化固定延迟,不是本文件新造);
// `lookup_latency` / `data_return_latency` 是本模型**显式声明**的常量 ——
// 它们只影响绝对量级,**不**影响「策略之间的相对差」这一 H2 判据。
#ifndef CPPTLM_TEST_CEP_DIRECTORY_EXPERIMENT_HH
#define CPPTLM_TEST_CEP_DIRECTORY_EXPERIMENT_HH

#include <cstddef>
#include <cstdint>
#include <map>

#include "tlm/cep/sharer_tracker.hh"

namespace cep_exp {

    using tlm::cep::Intent;
    using tlm::cep::LocalView;
    using tlm::cep::LookupHazard;
    using tlm::cep::LookupResult;
    using tlm::cep::SharerTrackerIf;
    using tlm::cep::SnoopState;
    using tlm::cep::node_id_t;
    using tlm::cep::responder_mask_of;
    using tlm::cep::responder_node_of;
    using tlm::cep::responder_set_t;

    /// home 侧处理的固定成本(显式声明 —— 见文件头的口径限定)
    struct ServiceConfig {
        std::uint64_t lookup_latency = 4;
        std::uint64_t snoop_latency = 2;      // = StubCacheCore::Config::snoop_latency_cycles
        std::uint64_t data_return_latency = 6;
        /// 无任何 sharer 信息时的广播目标数(映射位只有 4 个 ⇒ 上限 4)
        std::uint64_t broadcast_fanout = 4;
    };

    /// 一次 home 侧访问的观测结果
    struct ServiceResult {
        LookupResult verdict = LookupResult::ANSWER_HERE;
        LookupHazard hazard = LookupHazard::NONE;
        std::size_t snoop_count = 0;
        std::uint64_t latency_cycles = 0;
        bool directory_hit = false;  // view.valid(即 home 对该地址有本地认知)
    };

    /// home 侧目录模型：lookup → 按 verdict 展开 snoop → 聚合延迟。
    class HomeNodeModel {
    public:
        HomeNodeModel(SharerTrackerIf& tracker, ServiceConfig cfg = {})
            : tracker_(tracker), cfg_(cfg) {}

        /// 一次读 miss 在 home 侧的处理成本。
        /// @param resp_state 被 snoop 节点返回的状态(默认 `S` —— 读 snoop 后该节点**仍是** sharer;
        ///        ⚠️ 误用 `I` 会让 CONSERVATIVE tracker 合法收缩,把分布策略退化成全广播)
        /// @param data_supplied 是否由该节点供数
        ServiceResult service_read(std::uint64_t addr, SnoopState resp_state = SnoopState::S,
                                   bool data_supplied = false) {
            LookupResult verdict = LookupResult::ANSWER_HERE;
            LocalView view;
            LookupHazard hazard = LookupHazard::NONE;
            tracker_.lookup(addr, Intent::want_read, verdict, view, hazard);

            ServiceResult r;
            r.verdict = verdict;
            r.hazard = hazard;
            r.directory_hit = view.valid;

            if (hazard == LookupHazard::STALL) {
                // 同址 snoop 在飞 ⇒ 本次不产生数据移动,只付一次 lookup 重试成本
                r.latency_cycles = cfg_.lookup_latency;
                return r;
            }

            switch (verdict) {
            case LookupResult::ANSWER_HERE:
                r.latency_cycles = cfg_.lookup_latency;
                break;

            case LookupResult::FORWARD_TO: {
                const responder_set_t target = responder_mask_of(view.forward_to);
                r.snoop_count = target == 0 ? 0 : 1;
                if (r.snoop_count != 0)
                    drive_snoop(addr, target, resp_state, data_supplied);
                r.latency_cycles = cfg_.lookup_latency + r.snoop_count * cfg_.snoop_latency +
                                   cfg_.data_return_latency;
                break;
            }

            case LookupResult::BROADCAST:
            case LookupResult::ESCALATE_UP: {
                // 有本地视图 ⇒ 只探视图中可能持有的节点;无视图 ⇒ 退化为全体广播
                responder_set_t targets = view.sharers;
                if (targets == 0)
                    targets = mapped_prefix_mask(cfg_.broadcast_fanout);
                r.snoop_count = popcount(targets);
                if (r.snoop_count != 0)
                    drive_snoop(addr, targets, resp_state, data_supplied);
                r.latency_cycles = cfg_.lookup_latency + r.snoop_count * cfg_.snoop_latency +
                                   cfg_.data_return_latency;
                break;
            }
            }
            return r;
        }

        /// 一次写：lookup → 对**全部**可能 sharer 发 invalidate → 写完成。
        ServiceResult service_write(std::uint64_t addr) {
            LookupResult verdict = LookupResult::ANSWER_HERE;
            LocalView view;
            LookupHazard hazard = LookupHazard::NONE;
            tracker_.lookup(addr, Intent::want_write, verdict, view, hazard);

            ServiceResult r;
            r.verdict = verdict;
            r.hazard = hazard;
            r.directory_hit = view.valid;

            if (hazard == LookupHazard::STALL) {
                r.latency_cycles = cfg_.lookup_latency;
                return r;
            }

            // 写必须使全部已知 sharer 失效(FullMap / Dcoh 均以 view.sharers 为上界)
            responder_set_t targets = view.sharers;
            if (targets == 0 && verdict == LookupResult::BROADCAST)
                targets = mapped_prefix_mask(cfg_.broadcast_fanout);

            r.snoop_count = popcount(targets);
            if (r.snoop_count != 0)
                drive_snoop(addr, targets, SnoopState::I, /*data_supplied=*/false);

            r.latency_cycles = cfg_.lookup_latency + r.snoop_count * cfg_.snoop_latency +
                               cfg_.data_return_latency;
            return r;
        }

        std::uint64_t lookup_latency() const noexcept { return cfg_.lookup_latency; }
        const ServiceConfig& config() const noexcept { return cfg_; }

    private:
        static std::size_t popcount(responder_set_t m) {
            std::size_t n = 0;
            while (m != 0) {
                m &= (m - 1);
                ++n;
            }
            return n;
        }

        /// 前 n 个**已映射**节点(bit 0..3)的掩码。
        static responder_set_t mapped_prefix_mask(std::uint64_t n) {
            if (n > 4)
                n = 4;
            return (n == 0) ? responder_set_t{0} : ((responder_set_t{1} << n) - 1);
        }

        /// 驱动一次完整 snoop：begin → 逐节点 fold → observe(让 tracker 观察节点转 I)。
        void drive_snoop(std::uint64_t addr, responder_set_t targets, SnoopState resp_state,
                         bool data_supplied) {
            tlm::cep::snoop_handle_t handle = 0;
            tracker_.snoop_begin(addr, targets, handle);
            if (handle == 0)
                return;  // 期望集含未映射位 ⇒ 被拒;targets 来自映射位,不应发生

            responder_set_t remaining = targets;
            tlm::cep::MergedState merged = SnoopState::I;
            for (unsigned bit = 0; bit < 4; ++bit) {
                const responder_set_t m = responder_set_t{1} << bit;
                if ((targets & m) == 0)
                    continue;
                const node_id_t node = responder_node_of(bit);
                tracker_.snoop_fold(handle, node, resp_state, /*dirty=*/false, data_supplied,
                                    remaining, merged);
                tracker_.observe_snoop(addr, node, resp_state);
            }
        }

        SharerTrackerIf& tracker_;
        ServiceConfig cfg_;
    };

}  // namespace cep_exp

#endif  // CPPTLM_TEST_CEP_DIRECTORY_EXPERIMENT_HH
