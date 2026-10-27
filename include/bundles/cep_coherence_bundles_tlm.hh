// include/bundles/cep_coherence_bundles_tlm.hh
// CohIf Bundle —— CEP Coherence Interface 层的通信载体(跨 Stub↔PE 边界的 5 条 RN-side 流)
//
// 功能:定义 `bundles::CohIfBundle` —— 承载 canonical 10 流中**跨 Stub↔PE 边界的 5 条**
//       RN-side 流(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`),
//       由 `flow_id` 区分;字段集为**业务语义**,**不含任何网络层字段**。
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 定位(per `ADR-CEP-19` 决策 1~3 + `design.md D-α`/`D-δ`)─────────────
// · **单一** Bundle 类型(不是每流一类)—— Bundle 类型数收敛为 1 ⇒ 框架层**零新增**
//   StreamAdapter 子类(仓内 `BidirectionalPortAdapter<M, CohIfBundle, N>` 已存在)。
// · **只覆盖 5 条流**:`forward_data` / `back_invalidate` / `address_resolve` /
//   `barrier` / `snoop_completion_ack` 在 ProtocolEngine 内部终结或对接 Directory &
//   Ordering Plane ⇒ **不跨 Stub↔PE 边界**(per `ADR-CEP-02` §2 表 + CEN-Ib8)。
//   ⇒ 「对 stub 不可见」是**类型保证**(`flow_id` 为强类型枚举,域恰好 5 值),
//     **不是**运行期纪律。
// · **归类**:跨模块共享的**业务** Bundle(真源 1 `06 号 §3.3` 第三类)——
//   字段形态按业务语义(无网络层字段),但**取网络侧的演进纪律**
//   (`BUNDLE_FIELD_WIDTH` 编译期锁 + 扩展走 OpenSpec change)。
// · **协议中立**:本头文件**不含任何协议标识符**(per `ADR-CEP-07A` `INV-CEP-07A-IC2`)
//   —— 字段语义取自平台级 canonical 流契约(`ADR-CEP-02`),不取自任何具体协议。
//
// ── 字段来源(逐条可追,per `ADR-CEP-02` §2 canonical 10 流表)────────────
// | 流 | 载荷字段(per ADR-CEP-02)              | 本 Bundle 的承载            |
// |----|--------------------------------------|----------------------------|
// | 1 `request`     | read / unique / writeback / evict | `address` `perm` `size` `atomic` `transaction_id` |
// | 2 `completion`  | data(若 inline)+ perm + error     | `address` `data` `perm` `error` `transaction_id` |
// | 3 `snoop`       | 查询 / invalidate / clean / forward | `address` `snoop_type` `transaction_id` |
// | 4 `snoop_resp`  | hit + state + dirty + data + txn_id| `hit` `line_state` `dirty` `data` `transaction_id` |
// | 5 `writeback`   | addr + data + dirty                | `address` `data` `dirty` |
//
// ── 与 `tlm::cep` 的值域对应(跨层,**不** include `tlm/cep` 头)──────────
// `perm` / `snoop_type` / `line_state` 三个字段的取值集合与
// `tlm::cep::Perm` / `tlm::cep::SnoopType` / `tlm::cep::LineState` **逐值对应**
// (见 `tlm/cep/stub_cache_core.hh`)。本头位于 `include/bundles/`(**最底层**),
// **不**反向依赖 `include/tlm/cep/` ⇒ 值域对应关系由测试层做**编译期交叉断言**,
// 不靠注释维护。
//
// ── **为何没有 `is_write` 字段**(有意省略,不是遗漏)────────────────────
// 读写方向已由 `perm`(4 值:`NONE`/`READ`/`UNIQUE`/`WRITEBACK`)唯一决定 ⇒
// 若再加一个 `is_write`,同一事实会有**两个真源** ⇒ 二者可能不一致 ⇒
// Phase 3 的 `ProtocolBridge` round-trip(CC-03 ①「逐字段双向可逆」)将**不可逆**。
// ⇒ 桥接侧从 `perm` **派生** `is_write`,不冗余存字段。
#ifndef BUNDLES_CEP_COHERENCE_BUNDLES_TLM_HH
#define BUNDLES_CEP_COHERENCE_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"

#include <cstdint>
#include <type_traits>

namespace bundles {

// ══════════════════════════════════════════════════════════════════════
//  流标识 —— 强类型,域**恰好 5 值**
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **为何是强类型枚举而非 `ch_uint<8>`**:canonical 10 流中有 5 条
//    (`forward_data` / `back_invalidate` / `address_resolve` / `barrier` /
//    `snoop_completion_ack`)**不跨 Stub↔PE 边界**。若 `flow_id` 是裸整数,
//    这 5 条流在**类型层**仍可表达 ⇒ 「对 stub 不可见」退化为**运行期纪律**
//    (与 `design.md D-α` 的裁决相反)。强类型枚举 ⇒ 域外流名**不可表达**。
//
//    宽度由 `BUNDLE_FIELD_WIDTH` 编译期锁(8 bit)—— 枚举本身无成员,
//    故 `utils/port_contract_asserts.hh` 的宽度 trait 需支持枚举类型
//    (per `ADR-CEP-19` 决策 2 / `tasks.md 1.2` + `1.4`)。
//
//    **域恰好 5 值** —— 任何第 6 个枚举值都是**契约违反**
//    (由 `test/test_cep_cohif_carrier.cc` 的静态扫描用例 + 反向 fixture 钉住)。
enum class CohIfFlowId : std::uint8_t {
    Request    = 0,  // 流 1:本 stub **发起**缓存请求
    Completion = 1,  // 流 2:本 stub **消费**请求完成
    Snoop      = 2,  // 流 3:本 stub **消费**snoop 查询
    SnoopResp  = 3,  // 流 4:本 stub **发起**snoop 响应
    Writeback  = 4,  // 流 5:本 stub **发起**写回
};

/// CohIf 流数(canonical 10 流中跨 Stub↔PE 边界的条数)—— 供契约断言用
static constexpr std::size_t kCohIfFlowCount = 5;

// ══════════════════════════════════════════════════════════════════════
//  CohIfBundle —— 跨模块共享的业务 Bundle(真源 1 `06 号 §3.3` 第三类)
// ══════════════════════════════════════════════════════════════════════
//
// **字段集不含网络层字段**(`src_node` / `dst_node` / `flit_category` / `vc_id`
//  一律无)—— 那些是网络 Bundle 的形态(per `06 号 §3.1` 字段维度判定)。
// 网络层语义由 `ProtocolBridge` 在 `CohIfBundle ↔ NoCFlitBundle` 边界上**特化**
// (per `design.md D-γ`),**不**下沉到本 Bundle。
struct CohIfBundle : public bundle_base {
    // ── 流标识(强类型,域恰好 5 值)──────────────────────────────────
    CohIfFlowId flow_id = CohIfFlowId::Request;

    // ── 事务标识(配对 `request`↔`completion` / `snoop`↔`snoop_resp`)──
    ch_uint<64> transaction_id;

    // ── 地址与数据 ─────────────────────────────────────────────────
    ch_uint<64> address;  // 业务地址(per 各流)
    ch_uint<64> data;     // 数据载荷(1 拍;`completion` / `snoop_resp` / `writeback`)
    ch_uint<8>  size;     // 字节数(与网络 Bundle 的 `size` 同名同宽,便于桥接)

    // ── 权限(4 值锁,per `ADR-CEP-02` CEN-Ib11)──────────────────────
    ch_uint<8>  perm;

    // ── snoop 类型(3 值,per `ADR-CEP-05` 决策 3)────────────────────
    ch_uint<8>  snoop_type;

    // ── 稳定态(5 值,per `ADR-CEP-05` §4.1)────────────────────────
    ch_uint<8>  line_state;

    // ── 标志位 ─────────────────────────────────────────────────────
    ch_uint<1>  hit;     // `snoop_resp`:命中
    ch_uint<1>  dirty;   // `snoop_resp` / `writeback`:脏
    ch_uint<1>  error;   // `completion`:错误
    ch_uint<1>  atomic;  // `request`:atomic 意图(与 `perm` **正交**的独立属性,per CEN-Ib11)

    CohIfBundle() = default;

    // ══════════════════════════════════════════════════════════════
    //  值域常量(逐值对应 `tlm::cep::Perm` / `SnoopType` / `LineState`)
    // ══════════════════════════════════════════════════════════════

    // `perm` —— 4 值锁(NONE / READ / UNIQUE / WRITEBACK);**不**新增 ATOMIC 值
    static constexpr std::uint8_t PERM_NONE      = 0;
    static constexpr std::uint8_t PERM_READ      = 1;
    static constexpr std::uint8_t PERM_UNIQUE    = 2;
    static constexpr std::uint8_t PERM_WRITEBACK = 3;

    // `snoop_type` —— 3 值(INVALIDATE / CLEAN / FORWARD)
    static constexpr std::uint8_t SNOOP_INVALIDATE = 0;
    static constexpr std::uint8_t SNOOP_CLEAN      = 1;
    static constexpr std::uint8_t SNOOP_FORWARD    = 2;

    // `line_state` —— 5 值稳定态(不在缓存中 / I / S / E / M)
    static constexpr std::uint8_t STATE_NONE = 0;
    static constexpr std::uint8_t STATE_I    = 1;
    static constexpr std::uint8_t STATE_S    = 2;
    static constexpr std::uint8_t STATE_E    = 3;
    static constexpr std::uint8_t STATE_M    = 4;
};

// CC-02 ② —— `is_trivially_copyable` 编译期锁(随类型走,不依赖测试 TU)
// ChStream 端口按值拷贝 Bundle;带虚函数 ⇒ 引入 vptr ⇒ 不再 trivially copyable。
// ⚠️ 该断言写在**头文件**里 ⇒ 任何 include 本头的 TU 都受约束
//    (测试 TU 另有同款断言,per CC-02 ②「test TU 内」)。
static_assert(std::is_trivially_copyable_v<CohIfBundle>,
              "CC-02 ②: CohIfBundle 必须是 trivially copyable(不得含虚函数)");

}  // namespace bundles

// 编译期字段宽度契约锁(真源 1 `06 号 §3.3` 第三类 —— 取网络侧纪律;
// per `ADR-CEP-19` 决策 3 + `tasks.md 1.4` / CC-02 ③「覆盖**全部**字段」)
#include "utils/port_contract_asserts.hh"
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, flow_id, 8);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, transaction_id, 64);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, address, 64);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, data, 64);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, size, 8);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, perm, 8);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, snoop_type, 8);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, line_state, 8);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, hit, 1);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, dirty, 1);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, error, 1);
BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, atomic, 1);

#endif  // BUNDLES_CEP_COHERENCE_BUNDLES_TLM_HH
