// include/tlm/cep/protocol_engine_base.hh
// ProtocolEngine 端口面基类 + NullProtocolEngine(ADR 强制的默认实现)
//
// 功能:交付 `cpptlm-cep-cohif-carrier` 的 **Phase 2** —— ProtocolEngine 的
//       **ChStream 端口面**(承载 `bundles::CohIfBundle`)+ **`NullProtocolEngine`**
//       (per `ADR-CEP-07` 决策 4 / `INV-CEP-07-IC2` 的 10 流覆盖矩阵)。
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 定位(per `ADR-CEP-19` 决策 2 + `design.md D-ε`)────────────────────
// · ProtocolEngine 派生类 = **`ChStreamModuleBase`**(per `ADR-CEP-07` 决策 3
//   + `07 号 §6.3` 的 ModuleFactory seam)。
// · 端口成员 = `InputStreamAdapter<CohIfBundle>` / `OutputStreamAdapter<CohIfBundle>`
//   (照 `router_tlm.hh:139-141` 范式)—— **1 个 Bundle 类型 ⇒ N 个端口共用同一对
//   适配器类型**(per `design.md D-ε`)。
// · **端口数 N 是编译期模板参数** —— 理由:`ChStreamAdapterFactory::
//   registerBidirectionalPortAdapter<ModuleT, BundleT, N>` 的 `N` 是**模板参数**
//   (`include/framework/chstream_adapter_factory.hh:103`),而 adapter 内部按
//   `m->req_in()[i]` 索引 ⇒ N 必须编译期确定,否则「adapter 的 N」与「模块的
//   运行时端口数」会出现**两个真源**且可能静默分歧。
//
// ── 端口方向语义(per `design.md D-ε` + task 2.4)─────────────────────
//   `req_in[i]`  = **ingress**(收)
//   `resp_out[i]` = **egress**(发)
//   ⚠️ **流的语义方向由 `flow_id` 决定,不由端口方向决定** ——
//   `BidirectionalPortAdapter` 的 `req_in`/`resp_out` 是**通用 ingress/egress**
//   (命名层而非功能层);`snoop` / `writeback` 等**单向流**的语义落在
//   `flow_id` + 契约测试上。
//
// ── **不**引入协议语义基类(per `ADR-CEP-19` 决策 8)──────────────────
//   基类只持**类型擦除**的 `cpptlm::ProtocolBridgeEr*`(per `06 号 §4.4` 双层类形态:
//   类型擦除基类 + 模板子类)⇒ 本头**不**依赖任何具体协议。
//
// ── 依赖说明(诚实登记)────────────────────────────────────────────────
// · 本头 include `tlm/cep/stub_cache_core.hh` —— **只为**取平台级枚举
//   `tlm::cep::Perm` / `SnoopType` / `LineState`(canonical 流的取值域,
//   per `ADR-CEP-02` CEN-Ib11 / `ADR-CEP-05 §4.1`)。
//   ⚠️ 这些枚举当前**寄居在 stub 头**里。本头**只读**该头、**不改**它 ——
//   `StubCacheCore` 的「零触碰」是本 change 的硬约束(design `D-ζ` / tasks 归档前
//   复核第 6 项)⇒ **不做**「把枚举搬到独立头」的清理(那会改 stub 头)。
//   ⇒ 代价:PE → Stub 头存在一层**只读**依赖;后续若要解耦,需**另起** change。
// · 值域对应(`bundles::CohIfBundle::PERM_*` ↔ `tlm::cep::Perm` 等)由本头内的
//   **编译期 `static_assert` 交叉断言**钉住(见下方「值域交叉断言」段)——
//   不靠注释维护。
#ifndef TLM_CEP_PROTOCOL_ENGINE_BASE_HH
#define TLM_CEP_PROTOCOL_ENGINE_BASE_HH

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "tlm/cep/stub_cache_core.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cpptlm {
// 类型擦除的协议桥基类 —— 本头**只前向声明**(完整定义在
// `include/framework/protocol_bridge.hh`,per `06 号 §4.4` + tasks 3.1)。
// 前向声明足够:本基类持**非拥有**指针(生命周期由 ModuleFactory / 装配链负责,
// per tasks 3.6「注册与消费成对」)⇒ 无需完整类型即可析构。
class ProtocolBridgeEr;
}  // namespace cpptlm

namespace tlm::cep {

// ══════════════════════════════════════════════════════════════════════
//  canonical 10 流(per `ADR-CEP-02` §2 的流表)
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **本枚举是 canonical 10 流的全集** —— 而 `bundles::CohIfBundle::flow_id`
//    的域**只有 5 值**(跨 Stub↔PE 边界的 RN-side 流,per `ADR-CEP-19` 决策 2)。
//    另 5 条(`forward_data` / `back_invalidate` / `address_resolve` /
//    `barrier` / `snoop_completion_ack`)在 PE 内部终结或对接 Directory &
//    Ordering Plane ⇒ **不经 CohIf 腿**。
enum class CanonicalFlow : std::uint8_t {
    Request = 0,
    Completion = 1,
    Snoop = 2,
    SnoopResp = 3,
    Writeback = 4,
    ForwardData = 5,
    BackInvalidate = 6,
    AddressResolve = 7,
    Barrier = 8,
    SnoopCompletionAck = 9,
};

/// canonical 10 流数(per `ADR-CEP-02` §2)
inline constexpr std::size_t kCanonicalFlowCount = 10;

/// canonical 10 流的**名**(与 `protocol_instance.schema.json` 的 `flow_support`
/// 的 10 个 required key 逐字一致 —— 交叉核对由测试层做,per ADR-CEP-12 §4 抓取点 4)
inline constexpr std::array<const char*, kCanonicalFlowCount> kCanonicalFlowNames = {{
    "request", "completion", "snoop", "snoop_resp", "writeback",
    "forward_data", "back_invalidate", "address_resolve", "barrier",
    "snoop_completion_ack",
}};

/// canonical 10 流中**跨 Stub↔PE 边界**的 5 条(⇒ `CohIfBundle.flow_id` 的域)
inline constexpr std::size_t kCohIfFlowCount = 5;

// ══════════════════════════════════════════════════════════════════════
//  值域交叉断言(`bundles::CohIfBundle` ↔ `tlm::cep` 平台枚举)
// ══════════════════════════════════════════════════════════════════════
//
// `include/bundles/` 是**最底层**、**不**反向依赖 `include/tlm/cep/` ⇒ 值域对应
// 关系无法在 bundle 头内断言。此处(上层)做**编译期交叉断言** —— 任何一侧改动
// 值序都会**编译失败**,而不是静默错配。
static_assert(static_cast<std::uint8_t>(Perm::NONE) == bundles::CohIfBundle::PERM_NONE,
              "值域漂移:tlm::cep::Perm::NONE != bundles::CohIfBundle::PERM_NONE");
static_assert(static_cast<std::uint8_t>(Perm::READ) == bundles::CohIfBundle::PERM_READ,
              "值域漂移:tlm::cep::Perm::READ != bundles::CohIfBundle::PERM_READ");
static_assert(static_cast<std::uint8_t>(Perm::UNIQUE) == bundles::CohIfBundle::PERM_UNIQUE,
              "值域漂移:tlm::cep::Perm::UNIQUE != bundles::CohIfBundle::PERM_UNIQUE");
static_assert(static_cast<std::uint8_t>(Perm::WRITEBACK) == bundles::CohIfBundle::PERM_WRITEBACK,
              "值域漂移:tlm::cep::Perm::WRITEBACK != bundles::CohIfBundle::PERM_WRITEBACK");
static_assert(static_cast<std::uint8_t>(SnoopType::INVALIDATE) ==
                  bundles::CohIfBundle::SNOOP_INVALIDATE,
              "值域漂移:tlm::cep::SnoopType::INVALIDATE != CohIfBundle::SNOOP_INVALIDATE");
static_assert(static_cast<std::uint8_t>(SnoopType::CLEAN) == bundles::CohIfBundle::SNOOP_CLEAN,
              "值域漂移:tlm::cep::SnoopType::CLEAN != CohIfBundle::SNOOP_CLEAN");
static_assert(static_cast<std::uint8_t>(SnoopType::FORWARD) == bundles::CohIfBundle::SNOOP_FORWARD,
              "值域漂移:tlm::cep::SnoopType::FORWARD != CohIfBundle::SNOOP_FORWARD");
static_assert(static_cast<std::uint8_t>(LineState::NONE) == bundles::CohIfBundle::STATE_NONE,
              "值域漂移:tlm::cep::LineState::NONE != CohIfBundle::STATE_NONE");
static_assert(static_cast<std::uint8_t>(LineState::I) == bundles::CohIfBundle::STATE_I,
              "值域漂移:tlm::cep::LineState::I != CohIfBundle::STATE_I");
static_assert(static_cast<std::uint8_t>(LineState::S) == bundles::CohIfBundle::STATE_S,
              "值域漂移:tlm::cep::LineState::S != CohIfBundle::STATE_S");
static_assert(static_cast<std::uint8_t>(LineState::E) == bundles::CohIfBundle::STATE_E,
              "值域漂移:tlm::cep::LineState::E != CohIfBundle::STATE_E");
static_assert(static_cast<std::uint8_t>(LineState::M) == bundles::CohIfBundle::STATE_M,
              "值域漂移:tlm::cep::LineState::M != CohIfBundle::STATE_M");

// ══════════════════════════════════════════════════════════════════════
//  地址解析结果(per `ADR-CEP-07` 决策 4 的 `address_resolve` 出参)
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **形态细化**:ADR 决策 4 的草图用两个出参(`&home_node` / `&coherent`);
//    本实现改用**结果 struct** —— 语义等价,但避免「出参可被部分写入」的中间态。
struct AddressResolveResult {
    std::uint64_t home_node = 0;
    bool coherent = false;
};

// ══════════════════════════════════════════════════════════════════════
//  ProtocolEngineBase —— ProtocolEngine 的 ChStream 端口面
// ══════════════════════════════════════════════════════════════════════
//
// `N` = 端口数(编译期)。`static_assert(N > 1)`:N=1 会落 ModuleFactory 的
// single 分支 → `bidirectional_port_adapter.hh` 的 `bind_ports` 是**空操作 shim**
// ⇒ **静默不绑定**(per `design.md D-β` + tasks 4.3 的同一理由)。
template <std::size_t N>
class ProtocolEngineBase : public ChStreamModuleBase {
    static_assert(N > 1, "ProtocolEngine 端口数必须 > 1(N=1 会落 bind_ports 空 shim ⇒ 静默不绑定)");

public:
    using Bundle = bundles::CohIfBundle;
    using ReqAdapter = cpptlm::InputStreamAdapter<Bundle>;
    using RespAdapter = cpptlm::OutputStreamAdapter<Bundle>;

    /// 端口数(编译期常量;派生类可覆写以支持**动态**端口拓扑 —— 但那样 adapter 的
    /// 模板参数 N 必须同步,否则两侧分歧)
    static constexpr std::size_t kPortCount = N;

    ProtocolEngineBase(const std::string& name, EventQueue* eq) : ChStreamModuleBase(name, eq) {}
    ~ProtocolEngineBase() override = default;

    // ── 端口访问器(照 `router_tlm.hh:139-141` 范式)────────────────────
    std::array<ReqAdapter, N>& req_in() { return req_in_; }
    std::array<RespAdapter, N>& resp_out() { return resp_out_; }
    const std::array<ReqAdapter, N>& req_in() const { return req_in_; }
    const std::array<RespAdapter, N>& resp_out() const { return resp_out_; }

    // ── ChStreamModuleBase 接口 ──────────────────────────────────────
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override { adapter_ = adapter; }
    unsigned num_ports() const override { return static_cast<unsigned>(N); }

    /// 已注入的 StreamAdapter(**类型擦除**)—— 派生类可用它做 `static_cast`
    /// 到自己的 `BidirectionalPortAdapter<Derived, CohIfBundle, N>`(照
    /// `RouterTLM::set_stream_adapter` 的做法)。
    cpptlm::StreamAdapterBase* stream_adapter() const { return adapter_; }

    // ── 协议桥(类型擦除;**非拥有**)───────────────────────────────
    // 生命周期由 ModuleFactory / 装配链负责(per tasks 3.6「注册与消费成对」)
    // ⇒ 本基类**不**持有所有权、不删除指针。
    void set_bridge(cpptlm::ProtocolBridgeEr* bridge) { bridge_ = bridge; }
    cpptlm::ProtocolBridgeEr* bridge() const { return bridge_; }

    // ── canonical 10 流接口(per `ADR-CEP-02` §2 + `ADR-CEP-07` 决策 4)──
    //
    // **全部为纯虚** ⇒ 任何 ProtocolEngine 派生类**必须**实现全部 10 条流
    // (per `ADR-CEP-07` 决策 4 草图注释「必须全 10 流」+ oracle v0.4 B6 修订)
    // ⇒ 「10 流覆盖」由**类型系统**保证,而非运行期纪律。
    //
    // ⚠️ 其中 5 条(`forward_data` / `back_invalidate` / `address_resolve` /
    //    `barrier` / `snoop_completion_ack`)**不经 CohIf 腿** —— 它们是 PE
    //    **内部**接口(PE 内部终结 / 对接 Directory & Ordering Plane)。
    virtual void on_request(std::uint64_t addr, Perm perm, std::uint64_t txn_id) = 0;
    virtual void on_completion(std::uint64_t txn_id, std::uint64_t data, Perm perm,
                               bool error) = 0;
    virtual void on_snoop(std::uint64_t addr, SnoopType type, std::uint64_t txn_id) = 0;
    virtual void on_snoop_resp(std::uint64_t addr, bool hit, LineState state, bool dirty,
                               std::uint64_t data, std::uint64_t txn_id) = 0;
    virtual void on_writeback(std::uint64_t addr, std::uint64_t data, bool dirty) = 0;
    virtual void on_forward_data(std::uint64_t src_addr, std::uint64_t dst_addr,
                                 std::uint64_t data, Perm perm, std::uint64_t txn_id) = 0;
    virtual void on_back_invalidate(std::uint64_t addr, SnoopType type,
                                    std::uint64_t txn_id) = 0;
    virtual void on_address_resolve(std::uint64_t pa, std::uint64_t node_id,
                                    AddressResolveResult& out) = 0;
    virtual void on_barrier(std::uint64_t domain, std::uint64_t txn_id) = 0;
    virtual void on_snoop_completion_ack(std::uint64_t addr, std::uint64_t node_id,
                                         std::uint64_t txn_id) = 0;

    // ── 周期推进(通用 ingress 处理)──────────────────────────────────
    //
    /// 逐端口取 `req_in` 中已就绪的 Bundle,按 `flow_id` 分派到对应的
    /// canonical 流处理方法。**协议特有**的处理仍在派生类的 `on_*` 里 ——
    /// 本方法只做「取包 + 分派」。
    ///
    /// ⚠️ 这是**协议中立**的通用骨架:任何 PE 派生类若不需要额外行为,直接继承即可。
    void tick() override {
        for (std::size_t i = 0; i < N; ++i) {
            auto& in = req_in_[i];
            if (!in.valid())
                continue;
            dispatch_cohif(in.data());
            in.consume();
        }
    }

    // ── CohIf 分派(ingress 侧)──────────────────────────────────────
    //
    /// 把 ingress 收到的 `CohIfBundle` 按 `flow_id` 分派到对应的 canonical 流
    /// 处理方法。**流语义由 `flow_id` 决定**(per `design.md D-ε`)。
    ///
    /// @return true = 已分派;false = `flow_id` 越界(防御性;正常路径不可能 ——
    ///         `CohIfFlowId` 是强类型,域外值只能来自 `static_cast` 伪造)。
    bool dispatch_cohif(const Bundle& b) {
        switch (b.flow_id) {
            case bundles::CohIfFlowId::Request:
                on_request(b.address.read(), perm_of(b), b.transaction_id.read());
                return true;
            case bundles::CohIfFlowId::Completion:
                on_completion(b.transaction_id.read(), b.data.read(), perm_of(b),
                              b.error.read() != 0);
                return true;
            case bundles::CohIfFlowId::Snoop:
                on_snoop(b.address.read(), snoop_type_of(b), b.transaction_id.read());
                return true;
            case bundles::CohIfFlowId::SnoopResp:
                on_snoop_resp(b.address.read(), b.hit.read() != 0, line_state_of(b),
                              b.dirty.read() != 0, b.data.read(), b.transaction_id.read());
                return true;
            case bundles::CohIfFlowId::Writeback:
                on_writeback(b.address.read(), b.data.read(), b.dirty.read() != 0);
                return true;
        }
        return false;  // 越界(防御性分支)
    }

    // ── 值域映射(Bundle → 平台枚举)─────────────────────────────────
    // 上方的编译期 `static_assert` 已保证两侧值**逐值相等** ⇒ 此处可安全
    // `static_cast`;但仍走**具名**函数而非裸 cast,便于阅读与调试。
    static Perm perm_of(const Bundle& b) {
        return static_cast<Perm>(b.perm.read());
    }
    static SnoopType snoop_type_of(const Bundle& b) {
        return static_cast<SnoopType>(b.snoop_type.read());
    }
    static LineState line_state_of(const Bundle& b) {
        return static_cast<LineState>(b.line_state.read());
    }

private:
    std::array<ReqAdapter, N> req_in_;
    std::array<RespAdapter, N> resp_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    cpptlm::ProtocolBridgeEr* bridge_ = nullptr;  // 非拥有
};

// ══════════════════════════════════════════════════════════════════════
//  NullProtocolEngine —— ProtocolEngine 的默认实现
// ══════════════════════════════════════════════════════════════════════
//
// per `ADR-CEP-07` 决策 4 + `INV-CEP-07-IC2`「`NullProtocolEngine` 是
// ProtocolEngine 默认实现,…用于解耦测试」。
//
// ── 10 流覆盖矩阵(单一真源 = `kNullFlowMatrix`)────────────────────────
// 逐流行为 per `07 号 §6.4`:
//   流 1/3/4/5/6/7/10 = **no-op**
//   流 2 = **immediate-complete**
//   流 8 = **immediate-resolve**
//   流 9 = **immediate-complete**
//
// ⚠️ **矩阵用 C 数组 + 显式 size**(非 `std::array<T,10>`)—— 理由:反向 fixture
//    「**缺 1 流**」必须表现为「size 变成 9」并被**覆盖完整性**断言抓到;
//    若用 `std::array<T,10>`,少一个元素会**编译不过**(理由不对:不是「覆盖
//    矩阵断言」抓到的,而是初始化器个数不对)⇒ 判据会「因错误的原因红」。
enum class NullFlowBehavior : std::uint8_t {
    NoOp = 0,             // 不产生任何协议动作
    ImmediateComplete = 1, // 立即完成(不引入任何协议延迟)
    ImmediateResolve = 2,  // 立即解析(不引入任何协议延迟)
};

struct NullFlowMatrixEntry {
    CanonicalFlow flow;
    NullFlowBehavior behavior;
};

inline constexpr NullFlowMatrixEntry kNullFlowMatrix[] = {
    {CanonicalFlow::Request, NullFlowBehavior::NoOp},              // 流 1
    {CanonicalFlow::Completion, NullFlowBehavior::ImmediateComplete},  // 流 2
    {CanonicalFlow::Snoop, NullFlowBehavior::NoOp},                // 流 3
    {CanonicalFlow::SnoopResp, NullFlowBehavior::NoOp},            // 流 4
    {CanonicalFlow::Writeback, NullFlowBehavior::NoOp},            // 流 5
    {CanonicalFlow::ForwardData, NullFlowBehavior::NoOp},          // 流 6
    {CanonicalFlow::BackInvalidate, NullFlowBehavior::NoOp},       // 流 7
    {CanonicalFlow::AddressResolve, NullFlowBehavior::ImmediateResolve},  // 流 8
    {CanonicalFlow::Barrier, NullFlowBehavior::ImmediateComplete}, // 流 9
    {CanonicalFlow::SnoopCompletionAck, NullFlowBehavior::NoOp},   // 流 10
};

inline constexpr std::size_t kNullFlowMatrixSize =
    sizeof(kNullFlowMatrix) / sizeof(kNullFlowMatrix[0]);

/// 矩阵项的**期望**行为(per `07 号 §6.4` 的逐流口径,编码为**规则**而非第二份表
/// ⇒ 避免「矩阵」与「期望表」两份数据互相漂移)
constexpr NullFlowBehavior expected_null_behavior(CanonicalFlow flow) {
    switch (flow) {
        case CanonicalFlow::Completion:
            return NullFlowBehavior::ImmediateComplete;
        case CanonicalFlow::AddressResolve:
            return NullFlowBehavior::ImmediateResolve;
        case CanonicalFlow::Barrier:
            return NullFlowBehavior::ImmediateComplete;
        default:
            return NullFlowBehavior::NoOp;
    }
}

/// 覆盖**完整性**:canonical 10 流**各出现恰好一次**且总数为 10
constexpr bool null_flow_matrix_is_complete() {
    if (kNullFlowMatrixSize != kCanonicalFlowCount)
        return false;
    for (std::size_t i = 0; i < kNullFlowMatrixSize; ++i) {
        std::size_t hits = 0;
        for (std::size_t j = 0; j < kNullFlowMatrixSize; ++j) {
            if (kNullFlowMatrix[i].flow == kNullFlowMatrix[j].flow)
                ++hits;
        }
        if (hits != 1)
            return false;
    }
    return true;
}

/// 逐流行为**与期望一致**(per `07 号 §6.4`)
constexpr bool null_flow_matrix_behaviors_are_expected() {
    for (std::size_t i = 0; i < kNullFlowMatrixSize; ++i) {
        if (kNullFlowMatrix[i].behavior != expected_null_behavior(kNullFlowMatrix[i].flow))
            return false;
    }
    return true;
}

// 两条编译期锁 —— 各自对应一个**反向 fixture**(tasks 2.6 / 2.7):
//   缺 1 流 ⇒ 第一条触发;某流语义退化 ⇒ 第二条触发。
static_assert(null_flow_matrix_is_complete(),
              "INV-CEP-07-IC2: NullProtocolEngine 的 10 流覆盖矩阵必须恰好覆盖 canonical 10 流");
static_assert(null_flow_matrix_behaviors_are_expected(),
              "INV-CEP-07-IC2: NullProtocolEngine 的逐流行为必须与 07 号 §6.4 口径一致");

/// Null 引擎的可观测状态(per 判据纪律:观测点必须能**区分**行为差异 —— 只记
/// 「调用次数」的计数器无法区分 no-op 与 immediate-complete ⇒ 不算有判别力)
struct NullProtocolEngineCounters {
    std::array<std::uint64_t, kCanonicalFlowCount> per_flow{};  // 每流处理次数
    std::uint64_t no_op = 0;              // 走 no-op 的次数
    std::uint64_t immediate_complete = 0;  // 走 immediate-complete 的次数
    std::uint64_t immediate_resolve = 0;   // 走 immediate-resolve 的次数
    std::uint64_t out_of_domain = 0;       // `flow_id` 越界(防御性计数)
};

template <std::size_t N>
class NullProtocolEngine : public ProtocolEngineBase<N> {
public:
    using Base = ProtocolEngineBase<N>;
    using Counters = NullProtocolEngineCounters;

    NullProtocolEngine(const std::string& name, EventQueue* eq) : Base(name, eq) {}
    ~NullProtocolEngine() override = default;

    const Counters& counters() const { return ctr_; }

    /// 最近一次 `address_resolve` 的结果(immediate-resolve 的可观测形态)
    const AddressResolveResult& last_resolve() const { return last_resolve_; }

    // ── canonical 10 流实现(逐流查矩阵 —— 矩阵是唯一真源)───────────
    void on_request(std::uint64_t addr, Perm perm, std::uint64_t txn_id) override {
        (void)addr;
        (void)perm;
        (void)txn_id;
        apply(CanonicalFlow::Request);
    }
    void on_completion(std::uint64_t txn_id, std::uint64_t data, Perm perm,
                       bool error) override {
        (void)txn_id;
        (void)data;
        (void)perm;
        (void)error;
        apply(CanonicalFlow::Completion);
    }
    void on_snoop(std::uint64_t addr, SnoopType type, std::uint64_t txn_id) override {
        (void)addr;
        (void)type;
        (void)txn_id;
        apply(CanonicalFlow::Snoop);
    }
    void on_snoop_resp(std::uint64_t addr, bool hit, LineState state, bool dirty,
                       std::uint64_t data, std::uint64_t txn_id) override {
        (void)addr;
        (void)hit;
        (void)state;
        (void)dirty;
        (void)data;
        (void)txn_id;
        apply(CanonicalFlow::SnoopResp);
    }
    void on_writeback(std::uint64_t addr, std::uint64_t data, bool dirty) override {
        (void)addr;
        (void)data;
        (void)dirty;
        apply(CanonicalFlow::Writeback);
    }
    void on_forward_data(std::uint64_t src_addr, std::uint64_t dst_addr, std::uint64_t data,
                         Perm perm, std::uint64_t txn_id) override {
        (void)src_addr;
        (void)dst_addr;
        (void)data;
        (void)perm;
        (void)txn_id;
        apply(CanonicalFlow::ForwardData);
    }
    void on_back_invalidate(std::uint64_t addr, SnoopType type,
                            std::uint64_t txn_id) override {
        (void)addr;
        (void)type;
        (void)txn_id;
        apply(CanonicalFlow::BackInvalidate);
    }
    void on_address_resolve(std::uint64_t pa, std::uint64_t node_id,
                            AddressResolveResult& out) override {
        (void)pa;
        apply(CanonicalFlow::AddressResolve);
        // Null 解析口径(**最小诚实语义**):无真实目录 ⇒ 解析为「请求者自身」
        // (`home_node = node_id`),且**不做任何一致性声称**(`coherent = false`)。
        out.home_node = node_id;
        out.coherent = false;
        last_resolve_ = out;
    }
    void on_barrier(std::uint64_t domain, std::uint64_t txn_id) override {
        (void)domain;
        (void)txn_id;
        apply(CanonicalFlow::Barrier);
    }
    void on_snoop_completion_ack(std::uint64_t addr, std::uint64_t node_id,
                                 std::uint64_t txn_id) override {
        (void)addr;
        (void)node_id;
        (void)txn_id;
        apply(CanonicalFlow::SnoopCompletionAck);
    }

private:
    /// 逐流行为派发 —— **矩阵是唯一真源**(行为与矩阵不一致 ⇒ 编译期已红)
    void apply(CanonicalFlow flow) {
        const auto idx = static_cast<std::size_t>(flow);
        if (idx >= kCanonicalFlowCount) {
            ++ctr_.out_of_domain;
            return;
        }
        ++ctr_.per_flow[idx];
        for (std::size_t i = 0; i < kNullFlowMatrixSize; ++i) {
            if (kNullFlowMatrix[i].flow != flow)
                continue;
            switch (kNullFlowMatrix[i].behavior) {
                case NullFlowBehavior::NoOp:
                    ++ctr_.no_op;
                    break;
                case NullFlowBehavior::ImmediateComplete:
                    ++ctr_.immediate_complete;
                    break;
                case NullFlowBehavior::ImmediateResolve:
                    ++ctr_.immediate_resolve;
                    break;
            }
            return;
        }
        ++ctr_.out_of_domain;  // 矩阵缺该流(编译期已红;此处仅防御)
    }

    Counters ctr_;
    AddressResolveResult last_resolve_;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_PROTOCOL_ENGINE_BASE_HH
