// include/framework/protocol_bridge.hh
// ProtocolBridge 双层类形态 —— 类型擦除基类 + 模板子类 + identity 基线
//
// 功能:交付 `cpptlm-cep-cohif-carrier` 的 **Phase 3** —— 「CohIf → 具体通信网络」
//       的**特化桥**载体(per `ADR-CEP-19` 决策 3~5 + `06 号 §4.4` 原型)。
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 类形态(per `06 号 §4.4`,**首次落地**)────────────────────────────
//   `cpptlm::ProtocolBridgeEr`      —— 类型擦除基类(被 `ChStreamModuleBase` 持)
//   `cpptlm::ProtocolBridge<InB,OutB,Ordered>` —— 模板子类(编译期绑 InB/OutB)
//   `cpptlm::AbstractProtocolBridge<FlitT>`  —— identity 基线
//   `cpptlm::ProtocolBridgeHolder`  —— 非模板挂载接口(供 ModuleFactory 装配期注入)
//
//   `Ordered` 单 bool(per gem5 `MessageBuffer::m_strict_fifo(p.ordered)` 单参数对位,
//   per `06 号 §4.4`)—— **不**引入 mandatory/optional 三分类。
//
// ── **两个层次**的 pack/unpack(per CC-03 ① 的**分层判据**)──────────────
//   ⚠️ 本设计是 B′ 方案的落地:协议语义**不进**网络 flit 的字段空间。
//
//   【层 1 · 消息内容层】`pack(const InB&, OutB&)` / `unpack(const OutB&, InB&)`
//       —— Bundle 级**投影**:把 `InB` 中**网络 flit 能承载的共有字段**装进 `OutB`,
//          反向还原。`OutB` 的传输封装字段(`src_node`/`dst_node`/`vc_id`/`hops`/
//          `flit_type`/`flit_index`/`flit_count`/`src_port`)由**路由器**产生与改写
//          ⇒ **不参与** round-trip(per CC-03 ② 的职责边界)。
//       —— 被丢弃的协议语义字段由具体桥用 `dropped_fields()` **显式声明**(可测)。
//
//   【层 2 · 端到端层】`pack_with_packet(Packet*)` / `unpack_with_packet(Packet*)`
//       —— **完整语义**通道:`Packet` 的 payload 数据区承载 `InB` 的**原始字节**
//          (`InB` 是 `trivially_copyable` ⇒ 字节等价 ⇔ 逐字段相等)。
//       —— 这条通道是「CohIf 业务语义跨网络**不透损**」的**唯一**承载路径。
//
// ── 为什么协议语义**必须**走层 2(架构事实,per 2026-10-28 实测)─────────
//   `CohIfBundle.flow_id` 域 = **5 值**;`NoCFlitBundle.flit_category` 域 = **3 值**;
//   而队列 #6 的 6→3 消息类映射表是**刻意多对一**的(CC-06 ② 要求它「非恒等」)。
//   ⇒ **5 值 → 3 值是多对一有损映射** ⇒ 仅靠网络 flit 的字段空间**不可能**唯一
//      恢复 `flow_id` ⇒ Bundle 级「逐字段 round-trip 恒等」在数学上不可达。
//   ⇒ 协议语义(`perm`/`snoop_type`/`line_state`/`hit`/`dirty`/`atomic`/`flow_id`)
//      **必须**有自己的落脚点 —— 本设计落在 Packet payload 数据区。
//
// ── 与 `06 号 §4.5` 的关系(诚实登记)────────────────────────────────
//   `06 号 §4.5` 把 per-packet 承载标为「P3+ 真实需要时」,且 design `D-γ` 把它
//   排除在本 change 之外。本设计**不使用** `tlm_extension` 机制(那才是 D-γ 排除的
//   那个)—— 本设计用 **payload 数据区**承载完整语义 ⇒ **不违反** D-γ 的排除项。
//
// ── 边界(per `ADR-CEP-19` 决策 8 + `01 号 §1.1`)────────────────────
//   · **不**引入 StreamAdapter 子类 / factory 抽象类 / 协议语义基类
//   · **不**引入新的 invariant 编号(D-θ:新增 invariant 数 = 0)
#ifndef FRAMEWORK_PROTOCOL_BRIDGE_HH
#define FRAMEWORK_PROTOCOL_BRIDGE_HH

#include "core/packet.hh"

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace cpptlm {

// ══════════════════════════════════════════════════════════════════════
//  类型擦除基类 —— `ChStreamModuleBase` / `ProtocolEngineBase` 持有
// ══════════════════════════════════════════════════════════════════════
class ProtocolBridgeEr {
public:
    virtual ~ProtocolBridgeEr() = default;

    /// 【层 2】把**完整协议语义**写入 `Packet` 的 payload 数据区。
    ///
    /// 约定:payload 数据区承载 `InB` 的原始字节(具体桥的 `InB` 类型由模板子类绑定)。
    /// 本函数对类型擦除基类是**纯虚** —— 具体桥负责「读出 → 规范化 → 写回」。
    virtual void pack_with_packet(Packet* pkt) = 0;

    /// 【层 2】从 `Packet` 的 payload 数据区还原**完整协议语义**。
    virtual void unpack_with_packet(Packet* pkt) = 0;

    /// 【层 1】投影**丢弃**的协议语义字段名(可读清单)。
    ///
    /// ⚠️ **非空清单是正常的** —— 它是「哪些字段不在网络 flit 字段空间内」的
    ///    **显式登记** ⇒ 可被测试读取并与实际行为交叉核对(不藏)。
    virtual std::vector<std::string> dropped_fields() const { return {}; }

    /// 桥的**注册名**(与 `ModuleFactory` 注册表键一致;空 = 未命名)。
    virtual std::string name() const { return {}; }

    // ── per-bridge 静态元数据(per `06 号 §4.5` 的 P1 范围)──────────
    uint16_t bridge_id() const { return bridge_id_; }
    void set_bridge_id(uint16_t id) { bridge_id_ = id; }
    uint8_t protocol_id() const { return protocol_id_; }
    void set_protocol_id(uint8_t id) { protocol_id_ = id; }

    /// 身份自检 —— 供 ModuleFactory 装配期判据调用(per CC-05 ④)。
    ///
    /// ⚠️ 比 `06 号 §4.4` 原型**更严**一处:原型允许 `bridge_id_ == 0`,
    ///    本实现要求**非零** —— 理由:`0` 需要能表达「未分配」这一可测状态,
    ///    否则「忘了分配 id」与「合法 id 0」不可分辨。**登记备查**。
    virtual bool metadata_ok(std::string* why) const {
        if (protocol_id_ == 0) {
            if (why)
                *why = "protocol_id_ 为 0(未分配协议族编号)";
            return false;
        }
        if (bridge_id_ == 0) {
            if (why)
                *why = "bridge_id_ 为 0(未分配)";
            return false;
        }
        return true;
    }

    struct Stats {
        uint64_t pack_cycles = 0;
        uint64_t unpack_cycles = 0;
        uint64_t conversions = 0;
    };
    Stats stats_;

private:
    uint16_t bridge_id_ = 0;
    uint8_t protocol_id_ = 0;
};

// ══════════════════════════════════════════════════════════════════════
//  模板子类 —— 编译期绑定 InB / OutB
// ══════════════════════════════════════════════════════════════════════
template <typename InB, typename OutB, bool Ordered = false>
class ProtocolBridge : public ProtocolBridgeEr {
public:
    // 端到端「逐字段恒等」依赖**字节等价** ⇒ `InB` 必须 trivially copyable。
    static_assert(std::is_trivially_copyable_v<InB>,
                  "ProtocolBridge: InB 必须 trivially copyable(端到端逐字段恒等依赖字节等价)");
    static_assert(std::is_trivially_copyable_v<OutB>,
                  "ProtocolBridge: OutB 必须 trivially copyable");

    /// 【层 1】消息内容投影:`InB` → `OutB`。
    virtual void pack(const InB& in, OutB& out) = 0;

    /// 【层 1】消息内容投影:`OutB` → `InB`。
    virtual void unpack(const OutB& in, InB& out) = 0;

    // ── 层 2 的**辅助**(具体桥 + 判据测试复用)────────────────────
    // ⚠️ **public 而非 protected** —— 判据测试需要用它们在 payload 里**预置**
    //    `InB` 的字节(层 2 的调用约定:payload 数据区**已**承载 `InB` 原始字节)。
    /// 从 payload 数据区读出 `InB`(字节等价)。
    /// @return false = payload 为空 / 长度不足 / data 指针为空
    static bool read_payload_inb(const Packet* pkt, InB& out) {
        if (!pkt || !pkt->payload)
            return false;
        const unsigned char* p = pkt->payload->get_data_ptr();
        if (!p || pkt->payload->get_data_length() < sizeof(InB))
            return false;
        std::memcpy(&out, p, sizeof(InB));
        return true;
    }

    /// 把 `InB` 写入 payload 数据区(字节等价)。
    static bool write_payload_inb(Packet* pkt, const InB& in) {
        if (!pkt || !pkt->payload)
            return false;
        unsigned char* p = pkt->payload->get_data_ptr();
        if (!p || pkt->payload->get_data_length() < sizeof(InB))
            return false;
        std::memcpy(p, &in, sizeof(InB));
        return true;
    }
};

// ══════════════════════════════════════════════════════════════════════
//  identity 基线 —— 前期 abstract 模式 baseline(per `06 号 §4.4`)
// ══════════════════════════════════════════════════════════════════════
template <typename FlitT>
class AbstractProtocolBridge : public ProtocolBridge<FlitT, FlitT, false> {
public:
    void pack(const FlitT& in, FlitT& out) override { out = in; }
    void unpack(const FlitT& in, FlitT& out) override { out = in; }

    /// 层 2 的默认实现:**直通**(读出 → 写回,字节不变)。
    ///
    /// ⚠️ `this->stats_` 的 `this->` **不可省** —— 基类 `ProtocolBridge<FlitT,...>`
    ///    是**依赖基类**(dependent base)⇒ 按 [temp.dep.expr],非静态数据成员
    ///    `stats_` 在**模板定义期**的名字查找中**不可见** ⇒ 必须显式限定,
    ///    否则编译报「`stats_` was not declared in this scope」(实测踩过)。
    void pack_with_packet(Packet* pkt) override {
        FlitT b{};
        if (!read_payload_inb(pkt, b))
            return;
        write_payload_inb(pkt, b);
        ++this->stats_.pack_cycles;
        ++this->stats_.conversions;
    }
    void unpack_with_packet(Packet* pkt) override {
        FlitT b{};
        if (!read_payload_inb(pkt, b))
            return;
        write_payload_inb(pkt, b);
        ++this->stats_.unpack_cycles;
        ++this->stats_.conversions;
    }
};

// ══════════════════════════════════════════════════════════════════════
//  挂载点的说明(为什么不用独立 mixin)
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **不能用 `dynamic_cast<ProtocolBridgeHolder*>(SimObject*)`** ——
//    `ProtocolBridgeHolder` 与 `SimObject` **无继承关系** ⇒ 该 cast **编译不过**
//    (downcast 只允许到源类型的基类或派生类)。
//    让 mixin 继承 `SimObject` 则引入**菱形继承**(双 `SimObject` 子对象)⇒ 更糟。
//
// ⇒ 改用 **`ChStreamModuleBase` 上的可选挂载虚函数**
//    (`attach_protocol_bridge`,**默认实现返回 false** ⇒ 零波及):
//    所有既有 ChStream 派生类**无需改动**;`ProtocolEngineBase` 覆写它即可。
//
// ⇒ 因此本头**不**提供 `ProtocolBridgeHolder` mixin(避免留下无用接口)。

}  // namespace cpptlm

#endif  // FRAMEWORK_PROTOCOL_BRIDGE_HH
