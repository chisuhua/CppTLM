// include/tlm/gpu/sdma_noc_adapter.hh
// SDMANocAdapter: PcieTlpBundle ↔ NoCFlitBundle 双向转换 (E.1 v1.1 DataNoC only)
//
// Per cpptlm-dgpu-soc-internal-noc-e1 v1.1 design.md §3.2:
//   - 保留 SDMA 既有 5 端口 PcieTlpBundle 路径 (向后兼容)
//   - 新增 NoCFlitBundle 路径 (DataNoC bulk DMA 观测)
//   - 通过 SocInternalNoc::set_sdma_accept_fn() 注册 callback
//   - 0 触发既有 SDMA API 改变 (v1.1 scope 缩减: 不嵌入 SdmaEngineTLM)
//
// 物理对应 (per Oracle RTL 调研):
//   - NVIDIA Hopper SDMA: tag-based outstanding (~5 concurrent), 完成时 fence→MSI-X
//   - ARM CMN-700: 5 VC 通道, packet-based + credit-based flow control
//   - CppTLM 既有: h2d_pending_ (tag→descriptor) 已实现 (per sdma_engine_tlm.hh:475)
//
// 作者: CppTLM Team / 日期: 2026-10-09 (E.1 v1.1)
#ifndef CPPTLM_TLM_GPU_SDMA_NOC_ADAPTER_HH
#define CPPTLM_TLM_GPU_SDMA_NOC_ADAPTER_HH

#include "bundles/noc_bundles_tlm.hh"      // NoCFlitBundle (16 字段 v2.1)
#include "bundles/pcie_bundles_tlm.hh"     // PcieTlpBundle (8 字段)
#include "tlm/cluster/soc_internal_noc.hh" // NODE_ID_* 常量

#include <cstdint>
#include <cstring>

namespace tlm::gpu {

// SDMA NoC Adapter: PcieTlpBundle ↔ NoCFlitBundle 双向转换
// 设计原则 (per design §3.2):
//   1. emit_noc_request: 由 SDMA bulk DMA 提交触发, 生成 NoC REQ flit
//   2. accept_noc_response: VRAM 响应经 NoC 返回, 提取 transaction_id 触发 SDMA task_complete
//   3. callback 注册: 通过 set_sdma_accept_fn() 接入 SocInternalNoc
class SDMANocAdapter {
public:
    SDMANocAdapter() = default;
    ~SDMANocAdapter() = default;

    // 禁止拷贝 (callback 状态 + NoC 注入有状态)
    SDMANocAdapter(const SDMANocAdapter&) = delete;
    SDMANocAdapter& operator=(const SDMANocAdapter&) = delete;

    // ── Request 方向 (SDMA → SocInternalNoc → VRAM) ──
    // 输入: SDMA bulk DMA 操作 (op, addr, size, tag)
    // 输出: NoCFlitBundle (REQ) (per `noc_bundles_tlm.hh:26-50` 16 字段统一类型 + `flit_category=REQ` 区分)
    //   - src_node = NODE_ID_SDMA (1) (SDMA 是请求源)
    //   - dst_node = NODE_ID_VRAM (3) (VRAM 是目标, e1 v1.1 唯一入向 + 出向)
    //   - flit_category = CATEGORY_REQUEST (0)
    //   - transaction_id = tag (SDMA tag)
    //   - is_write = op (1=WRITE/0=READ, 与 PcieTlpBundle::is_write 一致)
    //   - address = addr
    //   - size = size (1-128 字节, 与 PcieTlpBundle::size 一致)
    bundles::NoCFlitBundle emit_noc_request(uint8_t op, uint64_t addr,
                                             uint32_t size, uint16_t tag) {
        bundles::NoCFlitBundle flit{};
        flit.src_node.write(tlm::cluster::SocInternalNoc::NODE_ID_SDMA);
        flit.dst_node.write(tlm::cluster::SocInternalNoc::NODE_ID_VRAM);
        flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);
        flit.transaction_id.write(static_cast<uint64_t>(tag));
        flit.is_write.write(op != 0);  // 0=READ, 1=WRITE
        flit.address.write(addr);
        flit.size.write(static_cast<uint8_t>(size > 255 ? 255 : size));
        flit.vc_id.write(tlm::cluster::SocInternalNoc::VC_REQ);
        flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
        flit.flit_index.write(0);
        flit.flit_count.write(1);
        flit.hops.write(0);
        flit.src_port.write(0);
        flit.data.write(0);  // REQ 阶段 data 字段未用
        flit.is_ok.write(false);  // REQ 阶段未设
        flit.error_code.write(0);
        return flit;
    }

    // ── Response 方向 (VRAM → SocInternalNoc → SDMA) ──
    // 输入: NoCFlitBundle (RSP) (transaction_id, data, is_ok, error_code, per `noc_bundles_tlm.hh:26-50`)
    // 输出: 解析为 SDMA task_complete 所需信息 (tag, status)
    //   - 成功: SDMA::task_complete(tag, OK) (但 v1.1 不嵌入 SDMA, 仅记录)
    //   - 失败: 记录 error_code
    void accept_noc_response(const bundles::NoCFlitBundle& resp) noexcept {
        last_response_ = resp;
        const uint8_t cat = resp.flit_category.read();
        // 仅 RESP 类别记录
        if (cat == bundles::NoCFlitBundle::CATEGORY_RESPONSE) {
            ++received_responses_;
            last_response_tag_ = static_cast<uint16_t>(resp.transaction_id.read());
            last_response_ok_ = resp.is_ok.read();
            last_response_error_code_ = resp.error_code.read();
        } else {
            // 非 RESP 类型响应 → 记录但不计 received_responses_
            ++unexpected_responses_;
        }
    }

    // ── Callback 工厂: 供 SocInternalNoc::set_sdma_accept_fn() 使用 ──
    // 返回 std::function<void(const NoCFlitBundle&)> 包装, 内部调 accept_noc_response
    tlm::cluster::SocInternalNoc::AcceptFn make_accept_fn() {
        return [this](const bundles::NoCFlitBundle& flit) {
            this->accept_noc_response(flit);
        };
    }

    // ── 统计访问器 (供 e2e 测试断言) ──
    uint64_t received_responses() const noexcept { return received_responses_; }
    uint64_t unexpected_responses() const noexcept { return unexpected_responses_; }
    uint16_t last_response_tag() const noexcept { return last_response_tag_; }
    bool     last_response_ok()   const noexcept { return last_response_ok_; }
    uint8_t  last_response_error_code() const noexcept { return last_response_error_code_; }
    const bundles::NoCFlitBundle& last_response() const noexcept { return last_response_; }

    // ── 重置 (测试间状态清理) ──
    void reset() noexcept {
        received_responses_ = 0;
        unexpected_responses_ = 0;
        last_response_tag_ = 0;
        last_response_ok_ = false;
        last_response_error_code_ = 0;
        last_response_ = bundles::NoCFlitBundle{};
    }

private:
    // 统计 (测试可观察)
    uint64_t received_responses_ = 0;
    uint64_t unexpected_responses_ = 0;
    uint16_t last_response_tag_ = 0;
    bool     last_response_ok_ = false;
    uint8_t  last_response_error_code_ = 0;
    bundles::NoCFlitBundle last_response_{};
};

} // namespace tlm::gpu

#endif // CPPTLM_TLM_GPU_SDMA_NOC_ADAPTER_HH
