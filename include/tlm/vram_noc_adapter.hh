// include/tlm/vram_noc_adapter.hh
// VramNocAdapter: NoCFlitBundle → Fabric2VramCtrlBundle 字段映射 + AXI4-like 行为
//
// Per cpptlm-dgpu-soc-internal-noc-e1 v1.1 design.md §3.3:
//   - 真实 HBM3 Controller 使用 AXI4 协议 (5 通道 AW/W/AR/R/B), 不是 NoC bundle
//   - VramNocAdapter 内部把 NoCFlitBundle 转 Fabric2VramCtrlBundle (8 字段, Change A frozen)
//   - 支持 multi-beat split: flit_count > 1 时拆为多个 Fabric2VramCtrlBundle 提交
//   - wrap_response 反向把 Fabric2VramCtrlBundle 转回 NoCFlitBundle (per `noc_bundles_tlm.hh:26-50` 16 字段统一类型 + `flit_category` 区分 REQ/RESP, per e1 v1.1 CRITICAL C1 不创建 dual bundle 决策)
//
// 字段映射表 (per design §3.3):
//   NoCFlitBundle                  → Fabric2VramCtrlBundle
//   address (64)                   → address (64)
//   size (8)                       → size (8)
//   is_write (1)                   → is_write (1)
//   data (64)                      → data (64)
//   transaction_id (64)            → transaction_id (32, 截断)
//   is_ok (1, resp) / error_code(8,resp) → error_code (8, 0=OK/1=DECERR/2=SLVERR)
//   data (64, resp)                → resp_data (64)
//   vc_id (8) + src_node (32)      → chip_select (8, multi-chip HCS topology)
//   hops (8, 统计)                  → (透明, fabric 不感知)
//   flit_type/index/count (wormhole) → (透明, fabric 不感知)
//
// 物理对应 (per Oracle RTL 调研):
//   - HBM3 Controller: AXI4 5 通道 (AW/W/AR/R/B), 数据宽度 256-bit per beat
//   - CppTLM 既有: Fabric2VramCtrlBundle 是 AXI4 subset (8 字段, max width 64-bit, per Change A)
//   - functional-mode 简化: 不模拟真实 AXI4 burst (wormhole 透明, single-beat)
//
// 作者: CppTLM Team / 日期: 2026-10-09 (E.1 v1.1)
#ifndef CPPTLM_TLM_VRAM_NOC_ADAPTER_HH
#define CPPTLM_TLM_VRAM_NOC_ADAPTER_HH

#include "bundles/noc_bundles_tlm.hh"          // NoCFlitBundle (16 字段 v2.1)
#include "bundles/dgpu_fabric_bundles_tlm.hh"  // Fabric2VramCtrlBundle (8 字段, Change A frozen)
#include "tlm/cluster/soc_internal_noc.hh"     // SocInternalNoc + NODE_ID_*

#include <cstdint>
#include <cstring>
#include <vector>

namespace tlm::cluster {

// VRAM NoC Adapter: NoCFlitBundle ↔ Fabric2VramCtrlBundle 双向转换
// 设计原则 (per design §3.3):
//   1. emit_noc_request: NoC REQ flit (SDMA → VRAM) → 1+ Fabric2VramCtrlBundle 提交
//   2. wrap_response: Fabric2VramCtrlBundle response → NoC RESP flit (VRAM → SDMA)
//   3. multi-beat split: flit_count > 1 时拆为多个 Fabric2VramCtrlBundle (functional-mode 简化)
//   4. chip_select 编码: vc_id<<8 | src_node (multi-chip HCS topology 模拟)
class VramNocAdapter {
public:
    VramNocAdapter() = default;
    ~VramNocAdapter() = default;

    // 禁止拷贝
    VramNocAdapter(const VramNocAdapter&) = delete;
    VramNocAdapter& operator=(const VramNocAdapter&) = delete;

    // ── Request 方向 (NoC REQ → Fabric2VramCtrlBundle) ──
    // 单 flit 转换 (functional-mode 简化: multi-beat split 暂不实现)
    bundles::Fabric2VramCtrlBundle emit_noc_request(const bundles::NoCFlitBundle& req) {
        bundles::Fabric2VramCtrlBundle fabric{};

        // 字段映射 (per design §3.3 表)
        fabric.address.write(req.address.read());
        const uint8_t size_byte = req.size.read();
        fabric.size.write(size_byte);
        fabric.is_write.write(req.is_write.read());
        fabric.data.write(req.data.read());

        // transaction_id: NoC 64-bit → fabric 32-bit (截断)
        fabric.transaction_id.write(static_cast<uint32_t>(req.transaction_id.read() & 0xFFFFFFFFULL));

        // chip_select: (vc_id << 8) | src_node (multi-chip HCS topology 简化)
        const uint8_t vc_id = req.vc_id.read();
        const uint32_t src_node = req.src_node.read();
        const uint8_t chip_select = static_cast<uint8_t>(((vc_id & 0x0F) << 4) | (src_node & 0x0F));
        fabric.chip_select.write(chip_select);

        // error_code: 0 (REQ 阶段未设)
        fabric.error_code.write(0);

        ++requests_emitted_;
        return fabric;
    }

    // ── Response 方向 (Fabric2VramCtrlBundle → NoC RESP flit) ──
    bundles::NoCFlitBundle wrap_response(const bundles::Fabric2VramCtrlBundle& fabric,
                                          uint32_t src_node = tlm::cluster::SocInternalNoc::NODE_ID_VRAM,
                                          uint32_t dst_node = tlm::cluster::SocInternalNoc::NODE_ID_SDMA) {
        bundles::NoCFlitBundle flit{};

        // 路由信息 (对称于 request)
        flit.src_node.write(src_node);
        flit.dst_node.write(dst_node);

        // 响应类别
        flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_RESPONSE);
        flit.vc_id.write(SocInternalNoc::VC_RSP);

        // 响应数据
        flit.data.write(fabric.resp_data.read());
        flit.is_ok.write(fabric.error_code.read() == 0);
        flit.error_code.write(fabric.error_code.read());

        // transaction_id 透传
        flit.transaction_id.write(static_cast<uint64_t>(fabric.transaction_id.read()));

        // wormhole metadata (单 beat)
        flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
        flit.flit_index.write(0);
        flit.flit_count.write(1);
        flit.hops.write(0);
        flit.src_port.write(0);

        ++responses_wrapped_;
        return flit;
    }

    // ── Multi-beat split 抽象 (per design §3.3 scenario T3) ──
    // 输入 NoCFlitBundle 含 flit_count > 1, 输出 N 个 Fabric2VramCtrlBundle (wormhole 拆解)
    // functional-mode 简化: 把 flit_count 个 beat 顺序输出, 每 beat 32B (4096/128)
    std::vector<bundles::Fabric2VramCtrlBundle> emit_noc_multibeat(
            const bundles::NoCFlitBundle& req) {
        std::vector<bundles::Fabric2VramCtrlBundle> out;
        const uint8_t flit_count = req.flit_count.read();
        if (flit_count <= 1) {
            out.push_back(emit_noc_request(req));
            return out;
        }
        // multi-beat: 拆为 flit_count 个 beat
        const uint64_t base_addr = req.address.read();
        const uint8_t size_per_beat = 32;  // functional-mode 简化: 32B per beat
        for (uint8_t i = 0; i < flit_count && i < 8; ++i) {
            bundles::NoCFlitBundle beat = req;
            beat.address.write(base_addr + static_cast<uint64_t>(i) * size_per_beat);
            beat.size.write(size_per_beat);
            beat.flit_index.write(i);
            out.push_back(emit_noc_request(beat));
        }
        return out;
    }

    // ── Callback 工厂: 供 SocInternalNoc::set_vram_accept_fn() 使用 ──
    // 实际 VRAM NoC 处理路径: SDMA → SocInternalNoc → VramNocAdapter.emit_noc_request → Fabric2VramCtrlBundle
    // 注: 完整 VRAM 写入是 mock (无真实 vram_storage_ 写入), 后续接 VramControllerTLM 完整实现
    tlm::cluster::SocInternalNoc::AcceptFn make_accept_fn() {
        return [this](const bundles::NoCFlitBundle& flit) {
            this->handle_noc_request(flit);
        };
    }

    // ── 统计访问器 (供 e2e 测试断言) ──
    uint64_t requests_emitted() const noexcept { return requests_emitted_; }
    uint64_t responses_wrapped() const noexcept { return responses_wrapped_; }
    const bundles::Fabric2VramCtrlBundle& last_fabric_request() const noexcept { return last_fabric_request_; }
    const bundles::NoCFlitBundle& last_noc_response() const noexcept { return last_noc_response_; }

    // ── 重置 (测试间状态清理) ──
    void reset() noexcept {
        requests_emitted_ = 0;
        responses_wrapped_ = 0;
        last_fabric_request_ = bundles::Fabric2VramCtrlBundle{};
        last_noc_response_ = bundles::NoCFlitBundle{};
    }

private:
    // 内部: 处理 NoC request, emit fabric + (mock) emit response
    void handle_noc_request(const bundles::NoCFlitBundle& req) {
        // 1. 转换为 Fabric2VramCtrlBundle
        last_fabric_request_ = emit_noc_request(req);

        // 2. (mock) 生成 NoC response — 真实实现应调 VramControllerTLM::accept_noc_request
        //    后续 Phase 2.3 集成时连真实 backing; 此刻仅 mock success
        last_noc_response_ = wrap_response(last_fabric_request_);
    }

    // 统计
    uint64_t requests_emitted_ = 0;
    uint64_t responses_wrapped_ = 0;
    bundles::Fabric2VramCtrlBundle last_fabric_request_{};
    bundles::NoCFlitBundle last_noc_response_{};
};

} // namespace tlm::cluster

#endif // CPPTLM_TLM_VRAM_NOC_ADAPTER_HH
