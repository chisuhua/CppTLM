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

#include <string>
#include <utility>
#include <vector>
#include "core/master_port.hh"
#include "tlm/crossbar_tlm.hh"

namespace cpptlm {
    namespace tlm {

        class CoherentXBarTLM : public CrossbarTLM {
        public:
            /// 配置加载（`ADR-X.17` D-1「读取即声明」）
            ///
            /// ⚠️ 两个键在本类**无对应概念** ⇒ 态②：**显式拒绝**（配置写了即装配失败并报出键名）。
            ///   · `domain_protocol` —— 本类**无协议状态机**（`snoop_broadcast()` 是无条件
            ///   write-through
            ///     透传，per ADR-CEP-15 的**协议语义边界冻结**）⇒ 协议标识符不入本类。
            ///   · `snoop_filter` —— 无 filter 概念（snoop 无条件广播给所有 peer）;ArchForge 契约
            ///     `microarchitecture/snoop_filter.md` 标「🟡 规划中」⇒ 该能力**尚未存在**。
            /// ⚠️ **必须调用基类** —— 否则继承自 `CrossbarTLM` 的 `port_count` / `ports`
            /// 拒绝路径失效。
            void on_config_loaded() override {
                reject_unsupported_param(
                    "domain_protocol",
                    "this class has no protocol state machine (snoop_broadcast is an unconditional "
                    "write-through passthrough); protocol identifiers do not belong here");
                reject_unsupported_param("snoop_filter",
                                         "there is no snoop filter in this class (snoops are "
                                         "broadcast unconditionally to all "
                                         "peers); the capability is still planned upstream");
                CrossbarTLM::on_config_loaded();
            }

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
            std::size_t peer_count() const {
                return peer_cache_req_outs_.size();
            }

        private:
            // peer cache 的 req_out 端口列表 + 名称 (调试日志用)
            std::vector<std::pair<std::string, MasterPort*>> peer_cache_req_outs_;
        };

    } // namespace tlm
} // namespace cpptlm

#endif // COHERENT_XBAR_TLM_HH
