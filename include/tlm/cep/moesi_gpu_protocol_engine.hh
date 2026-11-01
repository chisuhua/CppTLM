// include/tlm/cep/moesi_gpu_protocol_engine.hh
// MoesiGpuProtocolEngine —— `moesi_gpu` 协议实例的**运行期载体**(R11 的 V-2 前提)
//
// 功能:交付队列 #10 之后的 **R11 语义层缺口**(台账 Entry #133 ②)——
//       `moesi_gpu` 的 `O→WRITEBACK` **合并损失**的**补偿路径**:
//       (1) 提供**唯一的运行期对象**,能**持有并查询** `SparseSnoopFilter`
//       (2) 在 `O` 态被投影为 `WRITEBACK` 时**强制查询** tracker 并**登记**补偿结论
//       (3) 补偿结论的**判定依据 = tracker 的 `completeness`**(不是返回值)
// 作者: CppTLM Team / 日期: 2026-11-01
//
// ── 为什么需要它(实测根因,per 台账 Entry #124 / #125 / #131 / #133)────
// `MoesiGpuAdapter` 是**纯静态**的(全部 `static constexpr`)**且 `moesi_gpu` 无
// protocol engine** ⇒ **没有任何运行期对象**可以持有并查询 SF
// ⇒ 「`O→WRITEBACK` 补偿路径要有**可执行**判据」在无载体的前提下**结构性不可达**
// ⇒ 本类**就是**那个载体 ⇒ R11 降级三条件中的 **V-2** 才有可能达成。
//
// ── ⚠️ 补偿语义的**判定依据**(per `ADR-CEP-04` SU(2026-11-01) 的 V-2 补记)────
// 「该『判据』SHALL 判**保守补偿**语义,SHALL **NOT** 读作『**等价恢复**』——
//   `SparseSnoopFilter` = `CONSERVATIVE`(宁多探不漏探 = **不精确**)
//   ⇒ 只能给『**可能**还有其他 sharer』的保守近似,**无法**精确恢复 `O` 态携带的
//   『**没有**其他 sharer』」
// ⇒ ⇒ **verdict 由 tracker 的 `completeness` 决定**,**不**由本次 `lookup` 的返回值决定:
//   · `completeness == CONSERVATIVE` ⇒ `ConservativeOnly`(唯一合法档)
//   · `completeness == EXACT`        ⇒ `ExactRestorable`(此时 `ANSWER_HERE` 才**确实**
//     证明「没有其他 sharer」)
// ⇒ 真实的 `SparseSnoopFilter` 是 **CONSERVATIVE** ⇒ 恒 `ConservativeOnly`
// ⇒ ⇒ **不得**声称等价恢复。
//
// ── 引擎侧建模决策(逐条可判)────────────────────────────────────────
// | # | 决策 | 依据 |
// |---|------|------|
// | **M-1** | 补偿**触发点** = 「`O` 态的 perm 被投影出去」(即 `O→WRITEBACK` 的合并点)⇒ 每次合并都查 tracker | 合并点就是**信息丢失发生**的那一步 ⇒ 事后补偿 = 丢信息 ⇒ 必须在投影**前**查 |
// | **M-2** | `O` 态的**写事件**(`Wr` → `M`)走 `submit_local_request` ⇒ **先补偿、后迁移** | `O→M` 会**丢掉**「可能有其他 sharer」⇒ 迁移前必须查清 |
// | **M-3** | 未接 tracker ⇒ **拒绝**投影(返回 `false`)+ 计数 `compensation_skipped` ⇒ **不静默降级** | 与平台既有纪律一致(配置错误 ⇒ 装配期 fail,不静默降级) |
// | **M-4** | 补偿记录**逐字镜像** tracker 的实际 `lookup` 返回值 + `hazard` + `forward_to` ⇒ 可观测 | 补偿结论若只有散文 ⇒ 不可机器核对 |
// | **M-5** | `O` 态**之外**的投影**不**查 tracker(不触发补偿) | 只有 `O↔M` 的合并才有损失;其余态无「是否有其他 sharer」歧义 |
// | **M-6** | canonical 10 流的 `on_*` **只记账**(不建模在飞 snoop 集合 / 屏障域) | 本载体只服务**补偿路径**;冒充完整协议引擎会产出虚假行为 |
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **header-only** · 端口数 **N = 2**(与 `MesifProtocolEngine` / `ChiRnfProtocolEngine` 同款)
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `perm` 4 值锁
//   · **不扩** `CATEGORY_*` 3 类词表 · **新增 invariant 数 = 0**
//   · **不改** `MoesiGpuAdapter`(纯静态载体,零语义变更)
//   · **不改** `sharer_tracker.hh`(`SparseSnoopFilter` 行为面已落地,per Entry #129)
#ifndef TLM_CEP_MOESI_GPU_PROTOCOL_ENGINE_HH
#define TLM_CEP_MOESI_GPU_PROTOCOL_ENGINE_HH

#include "tlm/cep/moesi_gpu_adapter.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm::cep {

    class MoesiGpuProtocolEngine : public ProtocolEngineBase<2> {
    public:
        using Base = ProtocolEngineBase<2>;
        using State = MoesiGpuAdapter::State;
        using Event = MoesiGpuAdapter::Event;
        /// 适配器侧 4 值(`MoesiGpuAdapter::Perm`)
        using AdapterPerm = MoesiGpuAdapter::Perm;
        /// 平台侧 4 值(`tlm::cep::Perm`,经 CohIf 腿)
        /// ⚠️ 与 `AdapterPerm` 是**两个不同枚举**(值逐值相等,见下方 `static_assert`)
        ///    ⇒ canonical 流签名**必须**用平台侧,否则 `override` 不成立。
        using PlatformPerm = tlm::cep::Perm;

        // ── 两侧 4 值**逐值相等**(CEN-Ib11 的 4 值锁在两侧同构)────────────
        static_assert(static_cast<int>(AdapterPerm::NONE) == static_cast<int>(PlatformPerm::NONE));
        static_assert(static_cast<int>(AdapterPerm::READ) == static_cast<int>(PlatformPerm::READ));
        static_assert(static_cast<int>(AdapterPerm::UNIQUE) ==
                      static_cast<int>(PlatformPerm::UNIQUE));
        static_assert(static_cast<int>(AdapterPerm::WRITEBACK) ==
                      static_cast<int>(PlatformPerm::WRITEBACK));

        static constexpr AdapterPerm from_platform(PlatformPerm p) {
            return static_cast<AdapterPerm>(static_cast<std::uint8_t>(p));
        }
        static constexpr PlatformPerm to_platform(AdapterPerm p) {
            return static_cast<PlatformPerm>(static_cast<std::uint8_t>(p));
        }

        // ══════════════════════════════════════════════════════════════════
        //  补偿结论(**唯一**可登记的两种取值)
        // ══════════════════════════════════════════════════════════════════

        /// 补偿结论档位 —— **由 tracker 的 `completeness` 决定**,不由本次 `lookup` 返回值决定
        /// (per 本文件头「补偿语义的判定依据」段 + `ADR-CEP-04` SU(2026-11-01) 的 V-2 补记)。
        enum class CompensationVerdict : std::uint8_t {
            /// tracker 声明 **CONSERVATIVE** ⇒ 只能给「**可能**还有其他 sharer」的保守近似
            /// ⇒ **不得**读作「等价恢复」
            ConservativeOnly = 0,
            /// tracker 声明 **EXACT** ⇒ 其 `ANSWER_HERE` **确实**证明「没有其他 sharer」
            /// ⇒ 此时合并损失**可被等价恢复**
            ExactRestorable = 1,
        };

        static constexpr std::size_t kCompensationVerdictCount = 2;

        /// 补偿结论 → 散文(**可观测**;判据断言它与 `completeness` 一致)
        static constexpr const char* verdict_name(CompensationVerdict v) {
            switch (v) {
                case CompensationVerdict::ConservativeOnly: return "conservative-only";
                case CompensationVerdict::ExactRestorable:  return "exact-restorable";
            }
            return "";
        }

        /// 一条补偿记录(**可观测**;判据逐字段核对)
        struct CompensationRecord {
            std::uint64_t addr = 0;
            State        from = State::I;      ///< 合并前的态(恒 `O`)
            Event        event = Event::Rd;   ///< 触发事件
            AdapterPerm  projected = AdapterPerm::NONE;  ///< 合并后的 perm(恒 `WRITEBACK`)
            LookupResult lookup = LookupResult::BROADCAST;  ///< tracker 的**实际**返回值(逐字镜像)
            LookupHazard hazard = LookupHazard::NONE;       ///< tracker 的**实际** hazard(逐字镜像)
            node_id_t    forward_to = kInvalidNode;         ///< `FORWARD_TO` 时的目标节点
            Completeness tracker_completeness = Completeness::CONSERVATIVE;  ///< tracker 的 caps
            CompensationVerdict verdict = CompensationVerdict::ConservativeOnly;
        };

        /// 计数器(**可观测**)
        struct Counters {
            std::uint64_t flow_events[10] = {};   ///< 逐 canonical 流计数
            std::uint64_t local_requests = 0;      ///< `submit_local_request` 次数
            std::uint64_t adapter_transitions = 0;///< 迁移次数
            std::uint64_t compensation_queries = 0;///< 向 tracker 发起的补偿查询次数
            std::uint64_t compensation_skipped = 0;///< 因未接 tracker 而**拒绝**投影的次数
            std::uint64_t unsupported_flow_rejected = 0;
        };

        // ══════════════════════════════════════════════════════════════════
        //  构造 / 访问器
        // ══════════════════════════════════════════════════════════════════

        explicit MoesiGpuProtocolEngine(const std::string& name, EventQueue* eq) : Base(name, eq) {}
        ~MoesiGpuProtocolEngine() override = default;

        const Counters& counters() const { return ctr_; }

        /// 装配期注入 tracker(**唯一**能持有并查询 SF 的运行期对象)
        void set_sharer_tracker(SharerTrackerIf* t) { tracker_ = t; }
        SharerTrackerIf* sharer_tracker() const { return tracker_; }

        const std::vector<CompensationRecord>& compensation_log() const { return comp_log_; }
        std::size_t compensation_count() const { return comp_log_.size(); }

        /// 最后一条补偿记录(无 ⇒ `nullptr`)
        const CompensationRecord* last_compensation() const {
            return comp_log_.empty() ? nullptr : &comp_log_.back();
        }

        // ══════════════════════════════════════════════════════════════════
        //  (1) **补偿路径** —— R11 V-2 的核心
        // ══════════════════════════════════════════════════════════════════

        /// 把 `state` 的 4 值 `perm` **投影**出去;若该投影**发生合并损失**
        /// (`O → WRITEBACK`,决策 M-1)⇒ **强制查询** tracker 并登记补偿结论。
        ///
        /// ⚠️ **未接 tracker ⇒ 返回 `false`**(决策 M-3)—— **不静默降级**:
        ///    没有 tracker 就**不知道**「是否还有其他 sharer」⇒ 投影 `WRITEBACK`
        ///    就是在**未补偿**的情况下丢信息 ⇒ 拒绝。
        ///
        /// @param out_perm 投影结果(成功时写入)
        /// @return `true` = 投影成功(合并发生时**已**完成补偿查询);`false` = 拒绝
        bool project_with_compensation(std::uint64_t addr, State state, AdapterPerm& out_perm) {
            if (!MoesiGpuAdapter::perm_of(state, out_perm))
                return false;  // 瞬态 `T` 无投影(per `INV-CEP-05-IC1`)

            // 只有 `O` 态的投影**发生合并**(决策 M-5)
            if (state != State::O)
                return true;

            if (tracker_ == nullptr) {
                ++ctr_.compensation_skipped;
                return false;
            }

            LookupResult res = LookupResult::BROADCAST;
            LocalView view{};
            LookupHazard hazard = LookupHazard::NONE;
            // ⚠️ intent = `want_own`(`O→M` 的真实意图 = 「我要成为唯一写者」)
            tracker_->lookup(addr, Intent::want_own, res, view, hazard);
            ++ctr_.compensation_queries;

            CompensationRecord rec;
            rec.addr = addr;
            rec.from = state;
            rec.event = Event::Wr;
            rec.projected = out_perm;
            rec.lookup = res;                        // 逐字镜像(决策 M-4)
            rec.hazard = hazard;                     // 逐字镜像
            rec.forward_to = view.forward_to;        // 逐字镜像
            rec.tracker_completeness = tracker_->caps.completeness;
            rec.verdict = verdict_of(tracker_->caps.completeness);
            comp_log_.push_back(rec);
            return true;
        }

        /// verdict 的**唯一**判定入口:由 `completeness` 决定(**不**由返回值决定)
        static constexpr CompensationVerdict verdict_of(Completeness c) {
            return (c == Completeness::EXACT) ? CompensationVerdict::ExactRestorable
                                               : CompensationVerdict::ConservativeOnly;
        }

        // ══════════════════════════════════════════════════════════════════
        //  (2) 本地请求(迁移 + 补偿)
        // ══════════════════════════════════════════════════════════════════

        /// 应用一次**本地协议事件** —— 表驱动迁移 + **先补偿、后迁移**(决策 M-2)。
        ///
        /// ⚠️ 若该事件**离开** `O` 态(`O + Wr → M`)⇒ 迁移**前**必须完成补偿查询
        ///    ⇒ 否则「可能有其他 sharer」这一信息被**丢弃**。
        ///
        /// @return 迁移后的态;补偿被拒(无 tracker)⇒ 返回**原态**(迁移**不发生**)
        State submit_local_request(std::uint64_t addr, State from, Event e) {
            ++ctr_.local_requests;

            const auto to = static_cast<State>(MoesiGpuAdapter::transition(from, e));
            const bool leaves_merged = (from == State::O && to != State::O);

            if (leaves_merged) {
                AdapterPerm p = AdapterPerm::NONE;
                if (!project_with_compensation(addr, from, p)) {
                    // 补偿被拒 ⇒ **不迁移**(不得在未补偿的情况下丢信息)
                    return from;
                }
            }

            lines_[addr].state = to;
            ++ctr_.adapter_transitions;
            return to;
        }

        /// 某地址的当前态(未登记 ⇒ `I`)
        State state_of(std::uint64_t addr) const {
            const auto it = lines_.find(addr);
            return (it == lines_.end()) ? State::I : it->second.state;
        }
        std::size_t line_count() const { return lines_.size(); }

        // ══════════════════════════════════════════════════════════════════
        //  (3) canonical 10 流 —— **只记账**(决策 M-6)
        // ══════════════════════════════════════════════════════════════════

        void on_request(std::uint64_t, PlatformPerm, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Request)];
        }

        void on_completion(std::uint64_t, std::uint64_t, PlatformPerm, bool) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Completion)];
        }

        void on_snoop(std::uint64_t, SnoopType, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Snoop)];
        }

        void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                           std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::SnoopResp)];
        }

        void on_writeback(std::uint64_t, std::uint64_t, bool) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Writeback)];
        }

        void on_forward_data(std::uint64_t, std::uint64_t, std::uint64_t, PlatformPerm,
                             std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::ForwardData)];
        }

        void on_back_invalidate(std::uint64_t, SnoopType, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::BackInvalidate)];
        }

        void on_address_resolve(std::uint64_t, std::uint64_t, AddressResolveResult& out) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::AddressResolve)];
            out.home_node = 0;
        }

        void on_barrier(std::uint64_t, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Barrier)];
        }

        void on_snoop_completion_ack(std::uint64_t, std::uint64_t, std::uint64_t) override {
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::SnoopCompletionAck)];
        }

    private:
        struct Line {
            State state = State::I;
        };

        SharerTrackerIf* tracker_ = nullptr;
        std::unordered_map<std::uint64_t, Line> lines_;
        std::vector<CompensationRecord> comp_log_;
        Counters ctr_{};
    };

}  // namespace tlm::cep

#endif  // TLM_CEP_MOESI_GPU_PROTOCOL_ENGINE_HH
