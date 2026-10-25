// include/tlm/cep/address_resolver.hh
// Address Resolver —— 「pa → home node」归属判定,单方法接口
//
// 功能:把 `ADR-CEP-08 §1.3` 三子平面分工里的**第 1 子平面**(Address Resolver)落成最小接口。
//       它只回答「这条物理地址归谁管」,**不**感知协议形态、**不**维护 per-line 状态。
// 作者: CppTLM Team / 日期: 2026-10-25
//
// ── 与相邻平面的分工(per `ADR-CEP-08 §1.3`)───────────────────────────
//   · Address Resolver(本文件):拓扑归属判定(树遍历 / 窗口映射 / 哈希)
//   · Sharer Tracker(`sharer_tracker.hh`):per-node 状态 + caps + hazard
//   · Ordering Point 三判据(`ordering_point_checker.hh`):装配期静态检查
//   ⇒ 三者**正交**。把归属判定塞进 `lookup` 会强制 tracker 感知拓扑 ——
//     这正是 TileLink 的 `ESCALATE_UP` 需要的树信息与「per-node 状态维护」耦合的坑。
//
// ── 为什么不新增 canonical 流 ──────────────────────────────────────────
//   流 8 的 `address_resolve` 由 `ADR-CEP-02 CEN-Ib8` 规定**在 ProtocolEngine 内终结**;
//   本文件只给 ProtocolEngine 一个**可替换的 DI seam**,不改变 10 流的语义。
//
// ── 实现范围 ───────────────────────────────────────────────────────────
//   本 change 只落**接口 + 一个窗口实现**(供测试与装配期判据使用);
//   节点级的树遍历 / 哈希解析跟随各协议族(队列 #6~#9)。
#ifndef CPPTLM_CEP_ADDRESS_RESOLVER_HH
#define CPPTLM_CEP_ADDRESS_RESOLVER_HH

#include <cstdint>

#include "tlm/cep/sharer_tracker.hh"  // 共用 node_id_t / kInvalidNode

namespace tlm::cep {

// 恰好 1 个虚方法 + 虚析构(测试用 `virtual` 计数机检)
class AddressResolverIf {
public:
    virtual ~AddressResolverIf() = default;

    /// pa 归属的 home 节点;`kInvalidNode` = 未映射(调用方须按未映射处置,不得静默当作某个节点)
    virtual node_id_t resolve(std::uint64_t pa) const = 0;
};

/// 窗口实现:`[base, base + size)` 内的地址归属 `home`,窗口外未映射。
class WindowAddressResolver final : public AddressResolverIf {
public:
    WindowAddressResolver(node_id_t home, std::uint64_t base, std::uint64_t size)
        : home_(home), base_(base), size_(size) {}

    node_id_t resolve(std::uint64_t pa) const override {
        if (size_ == 0)
            return kInvalidNode;
        if (pa < base_)
            return kInvalidNode;
        const std::uint64_t off = pa - base_;
        return (off < size_) ? home_ : kInvalidNode;
    }

private:
    node_id_t home_;
    std::uint64_t base_;
    std::uint64_t size_;
};

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_ADDRESS_RESOLVER_HH
