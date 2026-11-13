// include/tlm/cep/mesif_protocol_engine.hh
// MesifProtocolEngine —— MESIF 协议实例的**真插件**协议引擎
//
// 功能:交付队列 #9 `cpptlm-cep-mesif-adapter` Phase 2(判据 MA-06 / MA-07):
//       (1) **驱动 `MesifAdapter` 表**(不是空转)⇒ 每次转换留可观测快照 `(from,event,to)`
//       (2) **R-3 的写回接线** —— `M + SnpShared → F` 时把「写回发生」喂给 Sharer Tracker
//           ⇒ `LocalView.dirty` 由置转清(语义损失 ③ 的补偿**不会自动发生**,必须接线)
//       (3) **atomic 最小可用路径** —— 接受(不被拒绝闸误杀)+ 按 `vc_binding` 路由意图
//           + `CohIfBundle::atomic` 承载意图位
// 作者: CppTLM Team / 日期: 2026-10-30
//
// ── 为什么是「真插件」而不是 `NullProtocolEngine` ─────────────────────
// Null 的全部 `on_*` 把参数 `(void)` 丢弃、**零发射**、**零状态**(per
// `protocol_engine_base.hh` 的 Null 段)⇒ 它有计数器但没有**行为**。本类提供 Null
// **结构上不可能**具备的三类行为:
//   · 状态迁移(表驱动 + 快照)/ 行状态(挂 `MesifAdapter` 的 5 态)
//   · 发射(egress `CohIfBundle`)/ 收发双向(canonical 10 流)
//   · 目录写回接线(补偿语义损失 ③)
// ⇒ MA-06 / MA-07 的判据都钉在这三类行为上,「换成 Null ⇒ 必红」因此成立。
//
// ── R-3 补偿的维度(勿与损失 ① 混写)────────────────────────────────
// 语义损失 ③ = 「已写回(干净)」与「仍脏」在 4 值 `perm` 上**同值不可分辨**
// (两者都投影 `WRITEBACK`)⇒ 补偿维度是 **`LocalView.dirty` 掩码**,不是「谁持有」
// (那是损失 ① 的维度,由 `sharers` 掩码补偿)。
// ⚠️ 本类**必须**在 `M + SnpShared → F` 路径上调用目录的安装面(`set_sharer`),否则
//    「可补偿」只是口号 —— 载体 `mesif_state_spec.hh` 的措辞已承诺该能力。
//
// ── 已登记的建模决策(⚠️ 无 ArchForge 真源 ⇒ 逐条 `pending`,挂跨仓追认债)──────
//   | # | 决策 | 依据 | 待追认 |
//   |---|------|------|:-----:|
//   | E-1 | `F` 在 **wire** 上投影为 `LineState::S` + `dirty = 0`(`perm` 仍取 `F` 的投影 `WRITEBACK`) | `CohIfBundle::line_state` 只有 `NONE`/`I`/`S`/`E`/`M`,**无 F**;而 `F` 语义是「已写回 ⇒ 非脏」⇒ 只发 `S` 才不会让对端误记脏位(发 `M` 会违反「`M` ⇒ `dirty`」这条别处依赖的不变量) | ✅ |
//   | E-2 | `SnoopType::CLEAN` 与 `SnoopType::FORWARD` **都**映射为 `Event::SnpShared` | 规约只有 6 事件(`Rd`/`RdUnique`/`Wr`/`Upgrade`/`SnpShared`/`SnpUnique`)**没有**「降级为 S」的独立事件 ⇒ 「降到 S」与「成为转发点」在事件层不可分辨(这是 6 事件词表的表达能力缺口,不是本类省略) | ✅ |
//   | E-3 | `on_request`(线缆上收到请求)只**记账**不迁移;本地请求由 `submit_local_request()` 发起 | 本引擎建模的是**节点侧**(目录在 PE 之外)⇒ 请求的**响应**由 home 以 `on_completion` 回来;若在收到请求时就地迁态,等于把 home 的行为塞进节点侧 | ✅ |
//   | E-4 | `on_completion(perm = WRITEBACK)` ⇒ `RdUnique` **后接** `Wr`(两次表查询,快照留最后一次) | `E + Wr → M` 是本规约的**静默升级**(`is_silent_upgrade`);直接用一次迁移跳 `M` 会绕过该路径的记账 | ✅ |
//   | E-5 | `on_snoop_resp` / `on_forward_data` / `on_barrier` / `on_snoop_completion_ack` 只**记账** | 本引擎不建模「在飞的 snoop 集合」与屏障域(Stage 2);只记账 ⇒ 不会产出虚假行为 | ✅ |
//   | E-6 | VC 路由只记**意图**,不写进 Bundle | `CohIfBundle` **无** `vc` 字段(VC 由 fabric 按 `vc_binding` 分配)⇒ 引擎只能回答「按声明该走哪个 `(network, vc)`」 | ✅ |
//
// ── 边界 ────────────────────────────────────────────────────────────────
//   · **不注册** 23 ABI · **不改** frozen header · **不扩** `CohIfBundle` 字段
//   · **不扩** `perm` 4 值锁 · **不扩** `CATEGORY_*` 3 类词表 · **新增 invariant 数 = 0**
//   · 端口数固定 **N = 2**(与 `CohIfProtocolEngine` / `ChiRnfProtocolEngine` 同款)
//   · `vc_binding` **不由本类解析 JSON** —— 装配方(测试 / ModuleFactory 消费方)装载
//     声明后调 `set_route(...)` ⇒ 声明是**唯一真源**,本类只存与分派
#ifndef TLM_CEP_MESIF_PROTOCOL_ENGINE_HH
#define TLM_CEP_MESIF_PROTOCOL_ENGINE_HH

#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm::cep {

    // ══════════════════════════════════════════════════════════════════════
    //  6 类一致性消息词表(与 `MesifAdapter::kMessageClassMap` 同序)
    // ══════════════════════════════════════════════════════════════════════
    inline constexpr std::size_t kMesifMessageClassCount = MesifAdapter::kMessageClassCount;

    inline constexpr std::array<const char*, kMesifMessageClassCount> kMesifMessageClasses = {{
        "request", "response", "snoop", "snoop_resp", "atomic", "query",
    }};

    /// 消息类名 → 下标(未知 ⇒ `kMesifMessageClassCount`,越界哨兵)
    constexpr std::size_t mesif_message_class_index(const char* mc) {
        for (std::size_t i = 0; i < kMesifMessageClassCount; ++i) {
            const char* a = kMesifMessageClasses[i];
            const char* b = mc;
            if (a == nullptr || b == nullptr)
                continue;
            while (*a != '\0' && *a == *b) {
                ++a;
                ++b;
            }
            if (*a == '\0' && *b == '\0')
                return i;
        }
        return kMesifMessageClassCount;
    }

    // ══════════════════════════════════════════════════════════════════════
    //  可观测面(判据的观测点)
    // ══════════════════════════════════════════════════════════════════════
    struct MesifEngineCounters {
        // ── MA-06 ⑥:区分「走了 adapter 表」与「空转」─────────────────
        std::uint64_t adapter_transitions = 0;  ///< 表查询次数(**每次**迁移 +1)
        std::uint64_t local_events = 0;         ///< 本地事件(`Rd`/`RdUnique`/`Wr`/`Upgrade`)
        std::uint64_t remote_events = 0;        ///< 远端事件(`SnpShared`/`SnpUnique`)
        // ── 10 流覆盖(逐流记账 ⇒ 「哪条流被真正驱动」可分辨)──────────
        std::array<std::uint64_t, kCanonicalFlowCount> flow_events{};
        std::array<std::uint64_t, kMesifMessageClassCount> by_class{};
        // ── 行 / 完成 ─────────────────────────────────────────────────
        std::uint64_t line_fills = 0;              ///< 由 `on_completion` 装行
        std::uint64_t completion_errors = 0;       ///< 带 `error` 的完成
        std::uint64_t completion_unknown_txn = 0;  ///< 完成找不到请求侧登记(收敛:不静默装错行)
        // ── 发射 ──────────────────────────────────────────────────────
        std::uint64_t snoop_resp_emitted = 0;
        std::uint64_t request_emitted = 0;
        std::uint64_t atomic_accepted = 0;
        std::uint64_t atomic_rejected = 0;  ///< 声明不支持 / 无 VC 声明 ⇒ **拒绝**(不静默降级)
        std::uint64_t egress_backpressure = 0;
        // ── 拒绝闸(声明 `flow_support = false` ⇒ 拒)──────────────────
        std::uint64_t unsupported_flow_rejected = 0;
        // ── R-3 写回接线(MA-07)───────────────────────────────────────
        std::uint64_t tracker_feeds = 0;    ///< 把本地态同步给目录的次数
        std::uint64_t tracker_missing = 0;  ///< 需同步但**未接线**(缺口观测:断接线 ⇒ 这里涨)
        std::uint64_t writebacks_cleared = 0;  ///< `M + SnpShared → F` 触发写回的次数
    };

    // ══════════════════════════════════════════════════════════════════════
    //  MesifProtocolEngine
    // ══════════════════════════════════════════════════════════════════════
    class MesifProtocolEngine : public ProtocolEngineBase<2> {
    public:
        using Base = ProtocolEngineBase<2>;
        using Bundle = bundles::CohIfBundle;
        using State = MesifAdapter::State;
        using Event = MesifAdapter::Event;
        /// 适配器侧 4 值(`MesifStateSpec::Perm`)
        using Perm = MesifAdapter::Perm;
        /// 平台侧 4 值(`tlm::cep::Perm`,经 CohIf 腿)——
        /// ⚠️ 与 `MesifAdapter::Perm` 是**两个不同枚举**(值逐值相等,见下方 static_assert)
        ///    ⇒ canonical 流签名**必须**用平台侧,否则 `override` 不成立(编译器不警告「参数类型不同」)。
        using PlatformPerm = tlm::cep::Perm;
        using Counters = MesifEngineCounters;

        // ── 两侧 4 值**逐值相等**(CEN-Ib11 的 4 值锁在两侧同构,映射因此可安全 cast)──
        static_assert(static_cast<int>(PlatformPerm::NONE) == static_cast<int>(Perm::NONE),
                      "perm 值域漂移:NONE");
        static_assert(static_cast<int>(PlatformPerm::READ) == static_cast<int>(Perm::READ),
                      "perm 值域漂移:READ");
        static_assert(static_cast<int>(PlatformPerm::UNIQUE) == static_cast<int>(Perm::UNIQUE),
                      "perm 值域漂移:UNIQUE");
        static_assert(static_cast<int>(PlatformPerm::WRITEBACK) ==
                          static_cast<int>(Perm::WRITEBACK),
                      "perm 值域漂移:WRITEBACK");

        static constexpr Perm from_platform(PlatformPerm p) {
            return static_cast<Perm>(static_cast<std::uint8_t>(p));
        }
        static constexpr PlatformPerm to_platform(Perm p) {
            return static_cast<PlatformPerm>(static_cast<std::uint8_t>(p));
        }

        // ══════════════════════════════════════════════════════════════════════
        //  引擎侧建模决策登记(机器可读的那一份;散文版见本文件头注释)
        // ══════════════════════════════════════════════════════════════════════
        //
        // ⚠️ **为什么做成结构化字段**:E-1 ~ E-6 全部是**本仓自推**的建模决策
        //    (ArchForge 侧**无规约**)⇒ 若只留在注释里,跨仓追认债会退化成
        //    **不可审计的散文** ⇒ 做成与 `HierarchicalDirectory::kBehaviorDecisions`
        //    同款的**可判据**载体(逐条 `basis` 非空 + `pending` 全 true)。
        struct EngineDecision {
            const char* id;      ///< 决策编号(`E-1` …)
            const char* topic;   ///< 决策点
            const char* basis;   ///< 推导依据(**非空**;缺失 ⇒ 判据必红)
            bool        pending; ///< 待追认标记(**全部为 true**)
        };

        /// 逐条引擎侧建模决策(per 本 change 的 design `D-λ` + 跨仓债 #3)
        static constexpr EngineDecision kEngineDecisions[] = {
            {"E-1", "F 态在 wire 上投影为 LineState::S + dirty = 0(perm 仍取 WRITEBACK)",
             "CohIfBundle::line_state 只有 NONE/I/S/E/M,无 F;而 F 语义是「已写回 ⇒ 非脏」"
             " ⇒ 发 S 才不让对端误记脏位(发 M 会违反「M ⇒ dirty」这条别处依赖的不变量)", true},
            {"E-2", "SnoopType::CLEAN 与 SnoopType::FORWARD 都映射为 Event::SnpShared",
             "规约的 6 事件词表(Rd/RdUnique/Wr/Upgrade/SnpShared/SnpUnique)没有「降级为 S」的"
             "独立事件 ⇒「降到 S」与「成为转发点」在事件层不可分辨(6 事件的表达能力缺口)", true},
            {"E-3", "on_request(线缆上收到请求)只记账不迁态;本地请求由 submit_local_request 发起",
             "本引擎建模的是节点侧(目录在 PE 之外)⇒ 请求的响应由 home 以 on_completion 回来;"
             "收到请求就地迁态等于把 home 的行为塞进节点侧", true},
            {"E-4", "on_completion(perm = WRITEBACK) ⇒ RdUnique 后接 Wr(两次表查询)",
             "E + Wr → M 是本规约的静默升级(is_silent_upgrade);一次直接跳 M 会绕过该路径的记账", true},
            {"E-5", "on_snoop_resp / on_forward_data / on_barrier / on_snoop_completion_ack 只记账",
             "本引擎不建模在飞 snoop 集合与屏障域(归 Stage 2)⇒ 只记账不会产出虚假行为", true},
            {"E-6", "VC 路由只记意图(last_atomic_network / last_atomic_vc),不写进 Bundle",
             "CohIfBundle 无 vc 字段(VC 由 fabric 按 vc_binding 分配)⇒ 引擎只能回答「按声明"
             "该走哪个 (network, vc)」;扩字段会破 CohIfBundle 零改动边界", true},
        };

        /// 引擎侧决策条数
        static constexpr std::size_t kEngineDecisionCount =
            sizeof(kEngineDecisions) / sizeof(kEngineDecisions[0]);

        /// 引擎侧决策表的**自洽谓词**(可变异形态 ⇒ 反向 fixture 可传「删了一条 / basis 置空」的副本)
        static constexpr bool engine_decisions_are_consistent(const EngineDecision* list,
                                                               std::size_t n) noexcept {
            for (std::size_t i = 0; i < n; ++i) {
                if (list[i].id == nullptr || list[i].id[0] == '\0')
                    return false;
                if (list[i].topic == nullptr || list[i].topic[0] == '\0')
                    return false;
                if (list[i].basis == nullptr || list[i].basis[0] == '\0')
                    return false;
                if (!list[i].pending)
                    return false;
            }
            return true;
        }

        /// 本类的决策表 SHALL 自洽
        static constexpr bool engine_decisions_are_consistent() noexcept {
            return engine_decisions_are_consistent(kEngineDecisions, kEngineDecisionCount);
        }
        /// 一次表迁移的快照(MA-06 的判别点:Null **结构上不可能**有非空快照)
        struct TransitionSnapshot {
            State from = State::I;
            Event event = Event::Rd;
            State to = State::I;
            bool valid = false;
        };

        /// 声明侧能力(默认 = `mesif` 的声明值;`set_support` 供反向 fixture 覆写)
        struct Support {
            bool atomic = true;                            ///< `atomic.supported`
            std::array<bool, kCanonicalFlowCount> flow{};   ///< `flow_support` 逐流
            Support() { flow.fill(true); }
        };

        /// 一条 VC 路由(`vc_binding[network][message_class] = vc`)
        struct Route {
            std::string network;
            std::string message_class;
            unsigned vc = 0;
        };

        explicit MesifProtocolEngine(const std::string& name, EventQueue* eq) : Base(name, eq) {}
        ~MesifProtocolEngine() override = default;

        const Counters& counters() const { return ctr_; }
        const TransitionSnapshot& last_transition() const { return snap_; }
        std::uint64_t adapter_transitions() const { return ctr_.adapter_transitions; }

        // ── 声明侧能力(装配期设置;默认 = 声明值)───────────────────────
        void set_support(const Support& s) { support_ = s; }
        const Support& support() const { return support_; }

        // ══════════════════════════════════════════════════════════════
        //  VC 路由(声明装载后注入 ⇒ 声明是唯一真源)
        // ══════════════════════════════════════════════════════════════
        /// 登记一条 `vc_binding` 条目(重复登记 ⇒ **覆盖**,装配期幂等)
        void set_route(const std::string& network, const std::string& message_class, unsigned vc) {
            for (auto& r : routes_) {
                if (r.network == network && r.message_class == message_class) {
                    r.vc = vc;
                    return;
                }
            }
            routes_.push_back(Route{network, message_class, vc});
        }

        /// 查 `(network, message_class)` 的 VC;未声明 ⇒ `-1`(**不**默认到 0)
        int vc_of(const std::string& network, const std::string& message_class) const {
            for (const auto& r : routes_) {
                if (r.network == network && r.message_class == message_class)
                    return static_cast<int>(r.vc);
            }
            return -1;
        }

        /// 声明了该消息类的**唯一**网络;无声明 或 **多个**网络都声明 ⇒ `nullptr`
        ///
        /// ⚠️ 「多个网络都声明」必须返回 `nullptr` 而不是随便挑一个 —— 挑一个等于
        ///    **静默降级**(`ADR-CEP-11` 决策 3 的处置口径)。
        const char* network_of(const std::string& message_class) const {
            const char* hit = nullptr;
            for (const auto& r : routes_) {
                if (r.message_class != message_class)
                    continue;
                if (hit != nullptr)
                    return nullptr;  // 歧义
                hit = r.network.c_str();
            }
            return hit;
        }

        std::size_t route_count() const { return routes_.size(); }

        // ══════════════════════════════════════════════════════════════
        //  目录接线(R-3 的补偿路径;`nullptr` = **未接线**)
        // ══════════════════════════════════════════════════════════════
        /// ⚠️ 接线目标的具体类型是 `HierarchicalDirectory` 而非 `SharerTrackerIf`:
        ///    单接口的 4 个方法**没有**「安装/更新条目」这一面(`set_sharer` 是
        ///    `FullMapDirectory` 先例里的 4 方法之外安装面)⇒ 若只持 `SharerTrackerIf*`
        ///    则补偿在**类型层**做不到。`mesif` 的声明把 `impl` 钉成
        ///    `HierarchicalDirectory` ⇒ 这里按**具体实现**接线是诚实的;要支持其它
        ///    实现须另立安装面接口(本 change 不做,登记为演进项)。
        void set_tracker(HierarchicalDirectory* tracker, node_id_t node, std::size_t level) {
            tracker_ = tracker;
            node_id_ = node;
            level_ = level;
            // ── 装配期硬断言(per change `cep-evidence-tier2-minimal-slice` · AC-4)──
            // ⚠️ **失败 ⇒ 置观测面「不可判」**(⛔ **不是** 0)—— 判「不可判」与
            //    判「无违例」是两件不同的事,混淆即造假。
            //
            // ⚠️ **为什么必须在这里**:三处**静默退化**会让 sharer 维度**恒空**
            //    ⇒ 观测面会报出「0 违例」的假象:
            //      ① `tracker_ == nullptr` ⇒ `sync_tracker` 走 `tracker_missing` 分支
            //      ② `responder_mask_of(node) == 0` ⇒ `set_sharer` **静默 no-op**
            //         (`sharer_tracker.hh` 的 bit→node 映射只有 4 个 bit)
            //      ③ `level >= kLevelCount` ⇒ `set_sharer` / `clear_sharer` **静默 no-op**
            //    ⇒ 且 `sync_tracker` 在静默 no-op 时**仍** `++tracker_feeds`
            //       ⇒ 「接线良好」的假象。
            if (tracker == nullptr)
                mark_observation_invalid();
            if (responder_mask_of(node) == 0)
                mark_observation_invalid();
            if (level >= HierarchicalDirectory::kLevelCount)
                mark_observation_invalid();
            // ⚠️ **第 4 条 = 防御断言(非生产不变式)**: `HierarchicalDirectory` 构造时
            //    把 `caps` 写死为 `EXACT` ⇒ 该断言在**生产路径恒真** ⇒ 它的唯一作用是
            //    「防护实现被换成保守 tracker」⇒ **不要求**反 fixture(AC-4)。
            if (tracker != nullptr && tracker->caps.completeness != Completeness::EXACT)
                mark_observation_invalid();
        }
        bool tracker_wired() const { return tracker_ != nullptr; }

        // ══════════════════════════════════════════════════════════════
        //  行状态 + 事件驱动(本类的**核心**行为)
        // ══════════════════════════════════════════════════════════════
        void set_line(std::uint64_t addr, State s, std::uint64_t data) {
            if (s == State::I) {
                lines_.erase(addr);
                return;
            }
            lines_[addr] = Line{s, data};
        }
        State state_of(std::uint64_t addr) const {
            const auto it = lines_.find(addr);
            return (it == lines_.end()) ? State::I : it->second.state;
        }
        bool line_data(std::uint64_t addr, std::uint64_t& out) const {
            const auto it = lines_.find(addr);
            if (it == lines_.end())
                return false;
            out = it->second.data;
            return true;
        }
        std::size_t line_count() const { return lines_.size(); }

        /// 应用一次**协议事件** —— 表驱动迁移 + 快照 + 目录同步(写回接线在此)
        ///
        /// @return true = 已应用;false = 该事件的出向流被声明为不支持(拒绝闸)
        ///
        /// ⚠️ **本重载 = 外部直调路径** ⇒ 观测归因 `DirectInject`
        ///    ⇒ ⛔ **不计入**任何生产流判据(per 裁决 R5)。
        ///    引擎内部的一切迁移走 `apply_event_as(origin, ...)`。
        bool apply_event(std::uint64_t addr, Event e) {
            return apply_event_as(EventObservation::Origin::DirectInject, addr, e);
        }

        /// 内部迁移入口(带归因)—— **唯一**产生 `kind = transition` 观测记录的地方
        ///
        /// ⚠️ `apply_event` 的注释称「false = 该事件的出向流被声明为不支持(拒绝闸)」,
        ///    但实现**恒返回 `true`** ⇒ 该注释**过期**(per oracle 复核)⇒ 本次**只改
        ///    注释、不改返回值语义**(改动会静默影响所有调用点)⇒ 过期注释的修正登记在
        ///    design 的「已知边界」段。
        bool apply_event_as(EventObservation::Origin origin, std::uint64_t addr, Event e) {
            const State from = state_of(addr);
            const State to = static_cast<State>(MesifAdapter::transition(from, e));
            // ① 表查询留痕(空转的引擎到达不了非空快照)
            ++ctr_.adapter_transitions;
            snap_ = TransitionSnapshot{from, e, to, true};
            if (e == Event::SnpShared || e == Event::SnpUnique)
                ++ctr_.remote_events;
            else
                ++ctr_.local_events;
            // ② 写回事实(`M + SnpShared` ⇒ 脏数据已回送)⇒ 计数 + 目录同步
            if (from == State::M && e == Event::SnpShared)
                ++ctr_.writebacks_cleared;
            // ── 观测记录(transition)──
            // ⚠️ **必须在 `sync_tracker` 之前**取 sharer 快照 ⇒ 那是「**迁移前**」视图
            //    (per 裁决 R4);`sync_tracker` 在本函数**末尾**才写目录。
            record_transition(origin, addr, e, from, to);
            // ③ 提交态
            if (to == State::I)
                lines_.erase(addr);
            else
                lines_[addr].state = to;
            // ④ 目录同步(补偿语义损失 ③:**不会自动发生**,必须接线)
            sync_tracker(addr, to);
            return true;
        }

        // ══════════════════════════════════════════════════════════════
        //  本地请求发起(节点侧:发 Request / Atomic ⇒ 等 on_completion)
        // ══════════════════════════════════════════════════════════════
        /// 发起一次本地请求(事件只用于选 `perm`,**不**在此迁态 —— 迁移发生在完成时)
        bool submit_local_request(std::uint64_t addr, Event e, std::uint64_t txn_id) {
            if (!gate(CanonicalFlow::Request))
                return false;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Request)];
            ++ctr_.by_class[mesif_message_class_index("request")];

            Bundle b;
            b.flow_id = bundles::CohIfFlowId::Request;
            b.transaction_id.write(txn_id);
            b.address.write(addr);
            b.perm.write(static_cast<std::uint8_t>(to_platform(perm_requested_by(e))));
            txn_addr_[txn_id] = addr;
            ++ctr_.request_emitted;
            return write_to_egress(b);
        }

        /// atomic 的最小可用路径(MA-06 ③④):**接受** + 按 `vc_binding` 路由 + 意图位
        ///
        /// ⚠️ 拒绝的两种情形都是**显式**的(不静默降级):
        ///   ① 声明 `atomic.supported = false`；② 6 类词表里没有任何网络声明 `atomic`。
        bool submit_atomic(std::uint64_t addr, std::uint64_t data, PlatformPerm perm,
                           std::uint64_t txn_id) {
            if (!support_.atomic) {
                ++ctr_.atomic_rejected;
                return false;
            }
            const char* net = network_of("atomic");
            const int vc = (net != nullptr) ? vc_of(net, "atomic") : -1;
            if (net == nullptr || vc < 0) {
                ++ctr_.atomic_rejected;
                return false;
            }
            ++ctr_.atomic_accepted;
            ++ctr_.by_class[mesif_message_class_index("atomic")];
            last_atomic_network_ = net;
            last_atomic_vc_ = static_cast<unsigned>(vc);

            Bundle b;
            b.flow_id = bundles::CohIfFlowId::Request;  // atomic 走 request 槽(6 类词表口径)
            b.transaction_id.write(txn_id);
            b.address.write(addr);
            b.data.write(data);
            b.perm.write(static_cast<std::uint8_t>(perm));
            b.atomic.write(1);  ///< 意图位(与 `perm` **正交**,per CEN-Ib11)
            txn_addr_[txn_id] = addr;
            return write_to_egress(b);
        }

        /// 最近一次 atomic 的**路由意图**(VC 不进 Bundle,per 决策 E-6)
        const std::string& last_atomic_network() const { return last_atomic_network_; }
        unsigned last_atomic_vc() const { return last_atomic_vc_; }

        // ══════════════════════════════════════════════════════════════
        //  canonical 10 流(CohIf ingress 侧)
        // ══════════════════════════════════════════════════════════════
        /// 流 1 —— 线缆上收到请求:只记账(决策 E-3;home 的响应由 `on_completion` 回来)
        void on_request(std::uint64_t, PlatformPerm, std::uint64_t) override {
            if (!gate(CanonicalFlow::Request))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Request)];
            ++ctr_.by_class[mesif_message_class_index("request")];
        }

        /// 流 2 —— 完成:按 `perm` 选事件迁移(WRITEBACK ⇒ `RdUnique` + `Wr`,决策 E-4)
        void on_completion(std::uint64_t txn_id, std::uint64_t data, PlatformPerm perm,
                           bool error) override {
            if (!gate(CanonicalFlow::Completion))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Completion)];
            ++ctr_.by_class[mesif_message_class_index("response")];
            if (error) {
                ++ctr_.completion_errors;
                txn_addr_.erase(txn_id);
                return;
            }
            const auto it = txn_addr_.find(txn_id);
            if (it == txn_addr_.end()) {
                ++ctr_.completion_unknown_txn;  // 收敛:不猜地址(猜错会装脏行)
                return;
            }
            const std::uint64_t addr = it->second;
            // ⚠️ 装行前的旧值(⛔ 用 `find` ⇒ 不引入额外插入)
            std::uint64_t data_before = 0;
            {
                const auto dit = lines_.find(addr);
                if (dit != lines_.end())
                    data_before = dit->second.data;
            }

            if (perm == PlatformPerm::NONE) {
                apply_event_as(EventObservation::Origin::CompletionFill, addr,
                                Event::SnpUnique);  // 无权 ⇒ 本地失效
                txn_addr_.erase(it);
                return;
            }
            apply_event_as(EventObservation::Origin::CompletionFill, addr,
                            (perm == PlatformPerm::READ) ? Event::Rd : Event::RdUnique);
            if (perm == PlatformPerm::WRITEBACK)
                apply_event_as(EventObservation::Origin::CompletionFill, addr,
                                Event::Wr);  // `E + Wr → M` 静默升级(决策 E-4)
            lines_[addr].data = data;
            ++ctr_.line_fills;
            // ── 观测记录(fill)──
            // ⚠️ **必须在装行之后**(per 裁决 R4:否则 `data_after` 恒为旧值)
            // ⚠️ `data_before` 用 `find` 取(⛔ 不用 `operator[]`)⇒ 不引入额外插入
            record_fill(addr, (perm == PlatformPerm::READ) ? Event::Rd : Event::Wr, data_before,
                        data);
            txn_addr_.erase(it);
        }

        /// 流 3 —— snoop:`INVALIDATE` ⇒ `SnpUnique`;`CLEAN` / `FORWARD` ⇒ `SnpShared`(决策 E-2)
        void on_snoop(std::uint64_t addr, SnoopType type, std::uint64_t txn_id) override {
            if (!gate(CanonicalFlow::Snoop))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Snoop)];
            ++ctr_.by_class[mesif_message_class_index("snoop")];

            const State pre = state_of(addr);
            const Event e = (type == SnoopType::INVALIDATE) ? Event::SnpUnique : Event::SnpShared;
            if (pre == State::I) {
                // 未命中:表仍被查询(`I + Snp* → I`),回 `hit = 0`
                apply_event_as(EventObservation::Origin::RemoteSnoop, addr, e);
                emit_snoop_resp(addr, /*hit=*/false, State::I, /*dirty=*/false, 0, txn_id);
                return;
            }
            std::uint64_t payload = 0;
            line_data(addr, payload);
            const bool was_dirty = (pre == State::M);
            apply_event_as(EventObservation::Origin::RemoteSnoop, addr,
                            e);  // 内含 `M + SnpShared → F` 的写回接线
            emit_snoop_resp(addr, /*hit=*/true, state_of(addr), was_dirty, payload, txn_id);
        }

        /// 流 4 —— snoop 响应:只记账(决策 E-5;本引擎不建模在飞 snoop 集合)
        void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                           std::uint64_t) override {
            if (!gate(CanonicalFlow::SnoopResp))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::SnoopResp)];
            ++ctr_.by_class[mesif_message_class_index("snoop_resp")];
        }

        /// 流 5 —— 写回:本节点淘汰脏行 ⇒ 本地失效 + 目录清位
        void on_writeback(std::uint64_t addr, std::uint64_t, bool) override {
            if (!gate(CanonicalFlow::Writeback))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Writeback)];
            if (state_of(addr) == State::I)
                return;
            apply_event_as(EventObservation::Origin::Writeback, addr,
                            Event::SnpUnique);  // 写回后本地无副本
        }

        /// 流 6 —— 转发数据:只记账(决策 E-5)
        void on_forward_data(std::uint64_t, std::uint64_t, std::uint64_t, PlatformPerm,
                             std::uint64_t) override {
            if (!gate(CanonicalFlow::ForwardData))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::ForwardData)];
        }

        /// 流 7 —— 反向失效:home 要求失效 ⇒ 与 `SnpUnique` 同款本地失效
        void on_back_invalidate(std::uint64_t addr, SnoopType, std::uint64_t) override {
            if (!gate(CanonicalFlow::BackInvalidate))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::BackInvalidate)];
            if (state_of(addr) == State::I)
                return;
            apply_event_as(EventObservation::Origin::BackInvalidate, addr,
                            Event::SnpUnique);
        }

        /// 流 8 —— 地址解析:最小诚实语义(解析为请求者自身,**不**做一致性声称)
        void on_address_resolve(std::uint64_t pa, std::uint64_t node_id,
                                AddressResolveResult& out) override {
            if (!gate(CanonicalFlow::AddressResolve))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::AddressResolve)];
            (void)pa;
            out.home_node = node_id;
            out.coherent = false;
        }

        /// 流 9 —— 屏障:只记账(决策 E-5)
        void on_barrier(std::uint64_t, std::uint64_t) override {
            if (!gate(CanonicalFlow::Barrier))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::Barrier)];
        }

        /// 流 10 —— snoop 完成确认:只记账(决策 E-5)
        void on_snoop_completion_ack(std::uint64_t, std::uint64_t, std::uint64_t) override {
            if (!gate(CanonicalFlow::SnoopCompletionAck))
                return;
            ++ctr_.flow_events[static_cast<std::size_t>(CanonicalFlow::SnoopCompletionAck)];
        }

        // ══════════════════════════════════════════════════════════════
        //  周期推进
        // ══════════════════════════════════════════════════════════════
        void tick() override { Base::tick(); }

        // ══════════════════════════════════════════════════════════════
        //  值域映射(表驱动 ⇒ 判据可对拍)
        // ══════════════════════════════════════════════════════════════
        /// `F` 的 wire 投影(决策 E-1):`CohIfBundle::line_state` **无 F** ⇒ 投影为 `S`
        static constexpr LineState wire_state(State s) {
            switch (s) {
                case State::I:
                    return LineState::I;
                case State::S:
                    return LineState::S;
                case State::E:
                    return LineState::E;
                case State::M:
                    return LineState::M;
                case State::F:
                    return LineState::S;  // 已写回 ⇒ 非脏;发 `M` 会让对端误记脏位
            }
            return LineState::NONE;
        }

        /// 本地请求事件 → 请求 `perm`(表驱动:事件决定要什么权限)
        static constexpr Perm perm_requested_by(Event e) {
            switch (e) {
                case Event::Rd:
                    return Perm::READ;
                case Event::RdUnique:
                case Event::Upgrade:
                    return Perm::UNIQUE;
                case Event::Wr:
                case Event::SnpShared:
                case Event::SnpUnique:
                    return Perm::WRITEBACK;
            }
            return Perm::NONE;
        }

        /// 状态 → wire `perm`(委托 `MesifAdapter::perm_of`;瞬态无投影 ⇒ `NONE`)
        static constexpr Perm wire_perm(State s) {
            Perm p = Perm::NONE;
            MesifAdapter::perm_of(s, p);
            return p;
        }

    private:
        struct Line {
            State state = State::I;
            std::uint64_t data = 0;
        };

        /// 声明侧拒绝闸(逐流) —— `flow_support = false` ⇒ 拒(不静默降级)
        bool gate(CanonicalFlow f) {
            if (support_.flow[static_cast<std::size_t>(f)])
                return true;
            ++ctr_.unsupported_flow_rejected;
            return false;
        }

        /// 目录同步(R-3 的补偿路径):本地态 → 目录视图
        ///
        /// ⚠️ 映射 `F → SnoopState::S`:目录的 `SnoopState` 只有 `I`/`S`/`E`/`M`
        ///    **无 F**(`HierarchicalDirectory` 的 B-5 已登记该缺口)⇒ 这里把
        ///    「转发点」降格为「干净 sharer」,并因此**只清 dirty 位、保 sharer 位**
        ///    ⇒ 这正是语义损失 ③ 的补偿(可分辨「已写回」与「仍脏」)。
        void sync_tracker(std::uint64_t addr, State to) {
            if (tracker_ == nullptr) {
                ++ctr_.tracker_missing;
                return;
            }
            switch (to) {
                case State::I:
                    tracker_->clear_sharer(addr, node_id_, level_);
                    break;
                case State::S:
                case State::F:
                    tracker_->set_sharer(addr, node_id_, level_, SnoopState::S);
                    break;
                case State::E:
                    tracker_->set_sharer(addr, node_id_, level_, SnoopState::E);
                    break;
                case State::M:
                    tracker_->set_sharer(addr, node_id_, level_, SnoopState::M);
                    break;
            }
            ++ctr_.tracker_feeds;
        }

        void emit_snoop_resp(std::uint64_t addr, bool hit, State st, bool dirty,
                             std::uint64_t data, std::uint64_t txn_id) {
            Bundle b;
            b.flow_id = bundles::CohIfFlowId::SnoopResp;
            b.transaction_id.write(txn_id);
            b.address.write(addr);
            b.data.write(data);
            b.hit.write(hit ? 1 : 0);
            b.dirty.write(dirty ? 1 : 0);
            b.line_state.write(static_cast<std::uint8_t>(wire_state(st)));
            b.perm.write(static_cast<std::uint8_t>(wire_perm(st)));
            ++ctr_.snoop_resp_emitted;
            write_to_egress(b);
        }

        /// 写 egress(轮转端口;全忙 ⇒ 记背压并返回 false,不丢包也不覆盖)
        bool write_to_egress(const Bundle& b) {
            for (std::size_t i = 0; i < Base::kPortCount; ++i) {
                const std::size_t p = (next_port_ + i) % Base::kPortCount;
                if (!this->resp_out()[p].valid()) {
                    this->resp_out()[p].write(b);
                    next_port_ = (p + 1) % Base::kPortCount;
                    return true;
                }
            }
            ++ctr_.egress_backpressure;
            return false;
        }

        /// 落一条 `kind = transition` 观测记录(per 裁决 R4 / R5)
        ///
        /// ⚠️ **调用点约束**:必须在 `sync_tracker` **之前**调用 ⇒ 快照是
        ///    「**迁移前**」视图。
        void record_transition(EventObservation::Origin origin, std::uint64_t addr, Event e,
                               State from, State to) {
            if (!observations_enabled())
                return;
            EventObservation obs;
            obs.kind = EventObservation::Kind::Transition;
            obs.origin = origin;
            obs.addr = addr;
            obs.event = static_cast<std::int32_t>(e);
            obs.from = static_cast<std::int32_t>(from);
            obs.to = static_cast<std::int32_t>(to);
            {
                // ⛔ 用 `find`(⛔ 不用 `operator[]`)⇒ 不得**提前插入**空行
                //    (那会改 `line_count()` ⇒ 影响既有断言)
                const auto dit = lines_.find(addr);
                if (dit != lines_.end())
                    obs.data_before = dit->second.data;
            }
            obs.data_after = obs.data_before;  // transition **不改** data
            // ⚠️ sharer 快照 = **迁移前**视图
            // ⚠️ ⛔ **必须**用 `view_of`(const 纯查询)—— ⛔ **不得**用 `lookup`
            //    (它有副作用:首次 forward 时 `dir_.emplace` + 记 `AllocateOnFirstForward`
            //     ⇒ 会污染 tracker 的事件流 ⇒ 影响既有断言)
            if (tracker_ != nullptr)
                obs.sharers_snapshot = tracker_->view_of(addr).sharers;
            const std::uint64_t self = responder_mask_of(node_id_);
            obs.self_mask = self;
            obs.self_mask_valid = (self != 0);
            obs.level = level_;
            obs.node = static_cast<std::uint32_t>(node_id_);
            record_observation(obs);
        }

        /// 落一条 `kind = fill` 观测记录(per 裁决 R4)
        ///
        /// ⚠️ **调用点约束**:必须在**装行之后**调用 ⇒ `data_after` 是新值。
        void record_fill(std::uint64_t addr, Event e, std::uint64_t data_before,
                         std::uint64_t data_after) {
            if (!observations_enabled())
                return;
            EventObservation obs;
            obs.kind = EventObservation::Kind::Fill;
            obs.origin = EventObservation::Origin::CompletionFill;
            obs.addr = addr;
            obs.event = static_cast<std::int32_t>(e);
            const State st = state_of(addr);
            obs.from = static_cast<std::int32_t>(st);
            obs.to = static_cast<std::int32_t>(st);  // 装行**不改**状态
            obs.data_before = data_before;
            obs.data_after = data_after;
            if (tracker_ != nullptr)
                obs.sharers_snapshot = tracker_->view_of(addr).sharers;
            const std::uint64_t self = responder_mask_of(node_id_);
            obs.self_mask = self;
            obs.self_mask_valid = (self != 0);
            obs.level = level_;
            obs.node = static_cast<std::uint32_t>(node_id_);
            record_observation(obs);
        }

        Counters ctr_;
        Support support_{};
        TransitionSnapshot snap_{};
        std::vector<Route> routes_;
        std::unordered_map<std::uint64_t, Line> lines_;
        std::unordered_map<std::uint64_t, std::uint64_t> txn_addr_;
        HierarchicalDirectory* tracker_ = nullptr;  // 非拥有
        node_id_t node_id_ = 0;
        std::size_t level_ = 0;
        std::string last_atomic_network_{};
        unsigned last_atomic_vc_ = 0;
        std::size_t next_port_ = 0;
    };

}  // namespace tlm::cep

#endif  // TLM_CEP_MESIF_PROTOCOL_ENGINE_HH
