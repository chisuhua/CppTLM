// include/tlm/coherent_xbar_tlm.hh
// CoherentXBarTLM - APU 顶层跨域 snoop 广播总线
//
// 设计意图:
//   - Phase 7.A/7.B (当前 P0): 继承 CrossbarTLM, 加 snoop_broadcast 通道
//     当前实现为 write-through 透传, 不下场做 coherence 决策
//   - Phase 7.C (未来): 引入 6×6 state transition switch 表 (per ADR-SOC-01)
//
// 依赖 D.1: registerPeerCache 通过 getInternalOutputPort("cacheN.req_out") 拿 peer 端口
// 命名空间: cpptlm::tlm (与 CrossbarTLM 一致)
//
// 作者: CppTLM Team / 日期: 2026-06-19
#ifndef COHERENT_XBAR_TLM_HH
#define COHERENT_XBAR_TLM_HH

#include "tlm/crossbar_tlm.hh"
#include "core/master_port.hh"
#include <string>
#include <utility>
#include <vector>

namespace cpptlm {
namespace tlm {

class CoherentXBarTLM : public CrossbarTLM {
public:
    explicit CoherentXBarTLM(const std::string& n, EventQueue* eq);
    ~CoherentXBarTLM() override = default;

    // P5 incorporate_parent 钩子调用: 注册 peer cache 的 req_out
    // snoop broadcast 触发时转发 Packet 副本到所有 peer
    // 约束: req_out 必须非空, 由 D.1 修复后 PortManager.getDownstreamPort 提供
    void registerPeerCache(const std::string& cache_name, MasterPort* req_out);

    // snoop 入口: Phase 7.A/7.B = write-through 透传到所有 peer
    // Phase 7.C 将改为: 先读 CoherenceState 再决定是否广播
    // 约束: pkt 必须非空
    void snoop_broadcast(Packet* pkt);

    // 测试辅助: 返回当前注册的 peer cache 数量
    std::size_t peer_count() const { return peer_cache_req_outs_.size(); }

private:
    // peer cache 的 req_out 端口列表 + 名称 (调试日志用)
    std::vector<std::pair<std::string, MasterPort*>> peer_cache_req_outs_;
};

} // namespace tlm
} // namespace cpptlm

#endif // COHERENT_XBAR_TLM_HH
