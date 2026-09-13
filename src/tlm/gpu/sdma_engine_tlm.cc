// src/tlm/gpu/sdma_engine_tlm.cc
// SdmaEngineTLM 实现：5 端口 PCIe master 引擎（SDMA/copy engine）
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/sdma_engine_tlm.hh"

#include "bundles/dma_bundles_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "tlm/gpu/completion_ring_mvp.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>

namespace tlm::gpu {

    namespace {

        // 错误消息（per design.md §6）
        constexpr const char* ERR_MSG_TRANSLATE_FAULT = "SDMA: IOMMU translation fault";
        constexpr const char* ERR_MSG_SIZE_ZERO = "SDMA: descriptor size == 0";
        constexpr const char* ERR_MSG_VRAM_OOB = "SDMA: VRAM window out of range";
        constexpr const char* ERR_MSG_NO_TRANSLATE_CB = "SDMA: translate callback not registered";

        // Wire-format 编码辅助（per sdma_engine_tlm.hh "Wire-format 选择" 注释）：
        //   desc_in port  (KIND_DMA_DESC):
        //     kind       = KIND_DMA_DESC (7)
        //     bar_index  = 0
        //     offset[63:32] = host_iova 高 32 位
        //     offset[31:0]  = vram_offset 低 32 位
        //     size       = descriptor.size
        //     data[63:32] = descriptor.tag (32-bit, 居高位)
        //     data[31:0]  = descriptor.dir | (descriptor.tag >> 0? reuse)
        //     requester_id / trans_id = 0
        //
        //   done_out port (KIND_DMA_DONE):
        //     kind       = KIND_DMA_DONE (8)
        //     bar_index  = 0
        //     offset     = task_id
        //     size       = status (signed 32-bit reinterpret as uint32)
        //     data       = tag
        //     requester_id / trans_id = 0
        //
        // 详细编码见 to_pcie_tlp_descriptor / to_pcie_tlp_completion 实现。

    } // namespace

    // ============== Wire-format ↔ DmaDescriptor / CompletionBundle 转换 ==============

    bundles::PcieTlpBundle SdmaEngineTLM::to_pcie_tlp_descriptor(const DmaDescriptor& d) {
        bundles::PcieTlpBundle p;
        p.kind.write(KIND_DMA_DESC);
        p.bar_index.write(0);

        // 64-bit offset = (host_iova << 32) | (vram_offset & 0xFFFFFFFF)
        // 由于 ch_uint<64> 是 64-bit 单字段,我们在内部用 data 字段编码扩展信息
        // (per design.md §2: host_iova 是 64-bit 全字段, 无法塞进 32-bit 偏移)
        // → 重新设计: 把整个 DmaDescriptor 编码到 size + data + offset
        //   offset[63:32] = host_iova[63:32]
        //   offset[31:0]  = host_iova[31:0]  (用全 32 位, 但需要 64-bit)
        // 简化为: host_iova 整个放 data (64-bit), vram_offset 放 offset (64-bit)
        //   offset = vram_offset (64-bit, 截断)
        //   size   = (size 32-bit) | (dir 8-bit << 24)
        //   data   = host_iova (64-bit) | tag (高 32 位? 但 ch_uint<64> 是单字段)
        // → 实际 ch_uint<64>.read() 返回 uint64_t; 我们把整个 64-bit 数据塞进 uint64 字段
        p.offset.write(static_cast<uint64_t>(d.vram_offset));
        p.size.write(static_cast<uint32_t>(d.size) | (static_cast<uint32_t>(d.dir) << 24));
        p.data.write(d.host_iova);
        p.requester_id.write(0);
        p.trans_id.write(static_cast<uint32_t>(d.tag));
        return p;
    }

    DmaDescriptor SdmaEngineTLM::from_pcie_tlp_descriptor(const bundles::PcieTlpBundle& p) {
        DmaDescriptor d;
        const uint8_t k = p.kind.read();
        const uint64_t vram = p.offset.read();
        const uint32_t sz = p.size.read();
        const uint32_t dir_enc = (sz >> 24) & 0xFF;
        const uint64_t iova = p.data.read();
        const uint32_t tag = p.trans_id.read();
        d.dir = static_cast<DmaDescriptor::Dir>(dir_enc & 0xFF);
        d.host_iova = iova;
        d.vram_offset = vram;
        d.size = sz & 0x00FFFFFFu; // 低 24 位是 size, 高 8 位是 dir
        d.tag = tag;
        (void)k; // kind 在调方已校验为 KIND_DMA_DESC
        return d;
    }

    bundles::PcieTlpBundle
    SdmaEngineTLM::to_pcie_tlp_completion(const bundles::CompletionBundle& c) {
        bundles::PcieTlpBundle p;
        p.kind.write(KIND_DMA_DONE);
        p.bar_index.write(0);
        p.offset.write(static_cast<uint64_t>(c.task_id.read()));
        p.size.write(c.status.read());
        p.data.write(static_cast<uint64_t>(c.tag.read()));
        p.requester_id.write(0);
        p.trans_id.write(0);
        return p;
    }

    bundles::CompletionBundle
    SdmaEngineTLM::from_pcie_tlp_completion(const bundles::PcieTlpBundle& p) {
        bundles::CompletionBundle c;
        c.task_id.write(static_cast<uint32_t>(p.offset.read()));
        c.status.write(p.size.read());
        c.tag.write(static_cast<uint32_t>(p.data.read()));
        return c;
    }

    // ============== SdmaEngineTLM 实现 ==============

    SdmaEngineTLM::SdmaEngineTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
        inflight_.reserve(64);
    }

    SdmaEngineTLM::~SdmaEngineTLM() = default;

    void SdmaEngineTLM::init() {
        // 初始化 5 个端口
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            req_in[i].reset();
            resp_out[i].reset();
        }
        inflight_.clear();
        completed_count_ = 0;
        error_count_ = 0;
        host_out_tx_count_ = 0;  // Stage 1.3b
        fence_queue_.clear();    // Stage 1.3d
        // Stage 1.3a 接入: ring_mode_ 保留 (enable_ring_mode 调用后才启用); 但统计重置
        dropped_desc_in_count_ = 0;
        ring_consumed_count_ = 0;
        last_err_msg_.clear();
    }

    void SdmaEngineTLM::do_reset(const ResetConfig&) {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            req_in[i].reset();
            resp_out[i].reset();
        }
        inflight_.clear();
    }

    void SdmaEngineTLM::set_stream_adapter(cpptlm::StreamAdapterBase* a) {
        // 单端口回退（向后兼容）：仅处理 port 0
        adapters_[0] = a;
    }

    void SdmaEngineTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        // 5 端口多端口注入（per design.md §2.5 Port index ordering lock）
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            adapters_[i] = adapters[i];
        }
    }

    bool SdmaEngineTLM::all_ports_have_adapter() const {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (adapters_[i] == nullptr)
                return false;
        }
        return true;
    }

    void SdmaEngineTLM::on_config_loaded() {
        const auto& cfg = get_config();
        if (!cfg.is_object())
            return;

        if (cfg.contains("max_inflight")) {
            const uint32_t n = cfg.value("max_inflight", 4u);
            set_max_inflight(n);
        }
        if (cfg.contains("translate_latency")) {
            const uint32_t lat = cfg.value("translate_latency", 0u);
            set_translate_latency(lat);
        }
        if (cfg.contains("vram_size_bytes")) {
            const uint64_t sz = cfg.value("vram_size_bytes", 256ull * 1024 * 1024);
            set_vram_size_bytes(sz);
        }
    }

    bool SdmaEngineTLM::is_vram_window_valid(uint64_t vram_offset, uint32_t size) const {
        // 防御性 size==0 检查（per design.md §6 + spec.md Scenario "Invalid descriptor rejected"）
        if (size == 0)
            return false;
        // 越界检查：vram_offset + size 不能超过 VRAM 大小
        // 32-bit size + 64-bit offset 加法 → 检查 overflow
        if (vram_offset >= vram_size_bytes_)
            return false;
        const uint64_t end = vram_offset + static_cast<uint64_t>(size);
        if (end < vram_offset) // overflow
            return false;
        if (end > vram_size_bytes_)
            return false;
        return true;
    }

    int SdmaEngineTLM::process_h2d(const DmaDescriptor& d, bundles::CompletionBundle& done) {
        // H2D: host → VRAM (per design.md §4)
        //   1. 检查 size > 0
        //   2. 检查 VRAM 范围
        //   3. 调用 translate_cb (iova → phys)
        //   4. 失败 → RequesterCompleterAbort (errno = -EIO)
        //   5. 成功 → emit host_out MEM_READ + mem_out MEM_WRITE
        //   6. emit done

        // size 检查（防御性，与 is_vram_window_valid 重复但更明确）
        if (d.size == 0) {
            done.status.write(static_cast<uint32_t>(-EINVAL));
            return -EINVAL;
        }

        // VRAM 范围检查（per spec.md Scenario "Invalid descriptor rejected"）
        if (!is_vram_window_valid(d.vram_offset, d.size)) {
            done.status.write(static_cast<uint32_t>(-EINVAL));
            return -EINVAL;
        }

        // translate callback 检查（per spec.md Scenario "IOMMU translation fault"）
        if (!translate_cb_) {
            // 未注册 callback → 模拟 fault
            done.status.write(static_cast<uint32_t>(-EIO));
            return -EIO;
        }

        // 调用 translate callback
        uint64_t phys = 0;
        int rc = translate_cb_(d.host_iova, d.size, phys);
        if (rc != 0) {
            done.status.write(static_cast<uint32_t>(-EIO)); // RequesterCompleterAbort
            return -EIO;
        }

        // 数据搬运（per spec.md R3-S1 "VRAM write visibility"）
        //   仅当 host_backdoor + vram_backdoor 都已注入且范围合法时执行。
        //   这是测试 + MVP 数据路径；生产环境由 Board backdoor ABI 替代。
        if (host_backdoor_ && vram_backdoor_) {
            const uint64_t src_end = phys + static_cast<uint64_t>(d.size);
            const uint64_t dst_end = d.vram_offset + static_cast<uint64_t>(d.size);
            if (src_end > phys && src_end <= host_backdoor_size_ && dst_end > d.vram_offset &&
                dst_end <= vram_backdoor_size_) {
                std::memcpy(static_cast<uint8_t*>(vram_backdoor_) + d.vram_offset,
                            static_cast<uint8_t*>(host_backdoor_) + phys, d.size);
            }
            // 越界时不抛错（MVP 容忍：仍 emit TLP，让测试看到 TLP emitted 但数据可能不全）
        }

        // 成功路径：emit host_out MEM_READ TLP
        // (per design.md §4 + ADR-SOC-07 Status Update Q3: descriptor-only TLP, bulk data via
        // backdoor)
        bundles::PcieTlpBundle host_tlp;
        host_tlp.kind.write(bundles::PcieTlpBundle::MEM_READ);
        host_tlp.bar_index.write(0);
        host_tlp.offset.write(phys); // 经 translate 后的 PA
        host_tlp.size.write(d.size);
        host_tlp.data.write(0); // descriptor-only, bulk data via backdoor
        host_tlp.requester_id.write(0);
        host_tlp.trans_id.write(static_cast<uint32_t>(d.tag));
        resp_out[PORT_HOST_OUT].write(host_tlp);
        host_out_tx_count_++;  // Stage 1.3b: host_out emit 计数

        // 同步 emit mem_out MEM_WRITE TLP (数据写入 VRAM)
        bundles::PcieTlpBundle mem_tlp;
        mem_tlp.kind.write(bundles::PcieTlpBundle::MEM_WRITE);
        mem_tlp.bar_index.write(1); // BAR1 = VRAM aperture (per pcie_bundles_tlm.hh doc)
        mem_tlp.offset.write(d.vram_offset);
        mem_tlp.size.write(d.size);
        mem_tlp.data.write(0);
        mem_tlp.requester_id.write(0);
        mem_tlp.trans_id.write(static_cast<uint32_t>(d.tag));
        resp_out[PORT_MEM_OUT].write(mem_tlp);

        done.status.write(0); // success
        return 0;
    }

    int SdmaEngineTLM::process_d2h(const DmaDescriptor& d, bundles::CompletionBundle& done) {
        // D2H: VRAM → host (per design.md §4)
        //   1. 检查 size > 0
        //   2. 检查 VRAM 范围
        //   3. emit mem_in MEM_READ (触发下游 VRAM 读响应)
        //   4. emit host_out MEM_WRITE (upstream 写)
        //   5. emit done

        if (d.size == 0) {
            done.status.write(static_cast<uint32_t>(-EINVAL));
            return -EINVAL;
        }

        if (!is_vram_window_valid(d.vram_offset, d.size)) {
            done.status.write(static_cast<uint32_t>(-EINVAL));
            return -EINVAL;
        }

        if (!translate_cb_) {
            done.status.write(static_cast<uint32_t>(-EIO));
            return -EIO;
        }

        // 数据搬运：VRAM → host（per spec.md R3 "D2H host write visibility"）
        //   仅当 backdoor ptrs 都已注入且范围合法时执行；与 H2D 同策略。
        if (vram_backdoor_ && host_backdoor_) {
            const uint64_t src_end = d.vram_offset + static_cast<uint64_t>(d.size);
            const uint64_t dst_end = d.host_iova + static_cast<uint64_t>(d.size);
            if (src_end > d.vram_offset && src_end <= vram_backdoor_size_ &&
                dst_end > d.host_iova && dst_end <= host_backdoor_size_) {
                std::memcpy(static_cast<uint8_t*>(host_backdoor_) + d.host_iova,
                            static_cast<uint8_t*>(vram_backdoor_) + d.vram_offset, d.size);
            }
        }

        // emit mem_out MEM_READ (D2H: VRAM → host, 先读 VRAM)
        bundles::PcieTlpBundle mem_tlp;
        mem_tlp.kind.write(bundles::PcieTlpBundle::MEM_READ);
        mem_tlp.bar_index.write(1); // BAR1 = VRAM aperture
        mem_tlp.offset.write(d.vram_offset);
        mem_tlp.size.write(d.size);
        mem_tlp.data.write(0);
        mem_tlp.requester_id.write(0);
        mem_tlp.trans_id.write(static_cast<uint32_t>(d.tag));
        resp_out[PORT_MEM_OUT].write(mem_tlp);

        // emit host_out MEM_WRITE (upstream 写)
        bundles::PcieTlpBundle host_tlp;
        host_tlp.kind.write(bundles::PcieTlpBundle::MEM_WRITE);
        host_tlp.bar_index.write(0);
        host_tlp.offset.write(d.host_iova);
        host_tlp.size.write(d.size);
        host_tlp.data.write(0); // descriptor-only
        host_tlp.requester_id.write(0);
        host_tlp.trans_id.write(static_cast<uint32_t>(d.tag));
        resp_out[PORT_HOST_OUT].write(host_tlp);
        host_out_tx_count_++;  // Stage 1.3b: host_out emit 计数

        done.status.write(0); // success
        return 0;
    }

    void SdmaEngineTLM::emit_completion(const bundles::CompletionBundle& done, int err_code,
                                        const std::string& err_msg) {
        // emit done_out TLP (encoded with KIND_DMA_DONE)
        bundles::PcieTlpBundle done_tlp = to_pcie_tlp_completion(done);
        resp_out[PORT_DONE_OUT].write(done_tlp);

        // 调用 board error callback (per design.md §6 + spec.md "IOMMU translation fault")
        if (err_code != 0 && error_cb_) {
            error_cb_(err_code, err_msg);
        }

        // 更新统计
        if (err_code == 0) {
            completed_count_++;
        } else {
            error_count_++;
        }
    }

    void SdmaEngineTLM::handle_desc_in() {
        // 反压：满窗口则不处理 (per design.md §5 R3 缓解)
        if (inflight_.size() >= max_inflight_) {
            return;
        }

        if (!req_in[PORT_DESC_IN].valid())
            return;

        // Stage 1.3a 接入: ring 模式互斥规则
        //   - ring 模式下 desc_in 收到包 → dropped_desc_in_count_++ + error_cb
        //   - 严禁同生命周期双路径产生 inflight (fence/in-order 语义要求单一提交点)
        if (ring_mode_) {
            dropped_desc_in_count_++;
            last_err_msg_ = "desc_in disabled in ring mode (use BAR1+0x10010000 doorbell)";
            if (error_cb_) {
                error_cb_(-EINVAL, last_err_msg_);
            }
            req_in[PORT_DESC_IN].consume();
            return;
        }

        const auto& req = req_in[PORT_DESC_IN].data();
        if (req.kind.read() != KIND_DMA_DESC) {
            // 未知 kind 静默丢弃（与 PcieEndpointTLM 一致）
            req_in[PORT_DESC_IN].consume();
            return;
        }

        DmaDescriptor d = from_pcie_tlp_descriptor(req);
        bundles::CompletionBundle done;
        done.task_id.write(static_cast<uint32_t>(d.tag)); // MVP: task_id == tag
        done.tag.write(static_cast<uint32_t>(d.tag));

        int rc = 0;
        if (d.dir == DmaDescriptor::Dir::H2D) {
            rc = process_h2d(d, done);
        } else {
            rc = process_d2h(d, done);
        }

        // 错误消息（仅用于 board error callback）
        std::string err_msg;
        int err_code_for_cb = rc;
        if (rc != 0) {
            if (rc == -EINVAL && d.size == 0) {
                err_msg = ERR_MSG_SIZE_ZERO;
            } else if (rc == -EINVAL) {
                err_msg = ERR_MSG_VRAM_OOB;
            } else if (rc == -EIO && !translate_cb_) {
                err_msg = ERR_MSG_NO_TRANSLATE_CB;
            } else {
                err_msg = ERR_MSG_TRANSLATE_FAULT;
            }
        }

        // emit done_out completion
        emit_completion(done, err_code_for_cb, err_msg);

        // 消费 desc_in
        req_in[PORT_DESC_IN].consume();

        // 记录到 inflight_ 队列（用于 done 释放后窗口计数）
        // MVP: 所有 desc 都在同 tick 处理 + emit done, inflight_ 队列仅用于占位计数
        inflight_.push_back({d, getCurrentCycle()});
        // 立即弹出（无异步语义）
        inflight_.pop_back();
    }

    void SdmaEngineTLM::tick() {
        // 1. 处理 desc_in 入口（带反压）
        handle_desc_in();

        // Stage 1.3d: 处理 Fence queue (Fence → CompletionRing → MSI-X vector 0)
        process_fence_queue();

        // 2. 调用各 adapter 的 tick()（如需要）
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (adapters_[i])
                adapters_[i]->tick();
        }
    }

    // Stage 1.3b D2D NoC payload forwarding (per openspec/.../2026-09-10-...):
    //   VRAM→VRAM payload 转发, 不经 host_out (bypassing PCIe TLP).
    //   - 依赖 vram_backdoor 注入 (与 H2D/D2H 同策略)
    //   - 若 d2d_noc_ 已注入 → 走 NoC payload 路径 (累加 noc 统计)
    //   - 否则 → 直 memcpy 路径 (MVP fallback)
    //   - 不增加 host_out_tx_count_ (per spec "bypassing host_out")
    void SdmaEngineTLM::d2d_forward(uint64_t src_va, uint64_t dst_va, uint32_t len) {
        if (len == 0)
            return;
        if (vram_backdoor_ == nullptr)
            return;  // MVP 容忍: 无 backdoor 不做事
        // 越界检查
        if (src_va + len > vram_backdoor_size_ || dst_va + len > vram_backdoor_size_) {
            return;  // MVP 容忍: 越界静默 skip
        }

        // 走 D2D NoC payload 路径 (若已注入)
        if (d2d_noc_ != nullptr) {
            d2d_noc_->d2d_forward_payload(
                static_cast<uint8_t*>(vram_backdoor_) + dst_va,
                static_cast<uint8_t*>(vram_backdoor_) + src_va,
                len);
        } else {
            // 直 memcpy fallback (测试场景下与 noc 路径等价)
            std::memcpy(static_cast<uint8_t*>(vram_backdoor_) + dst_va,
                        static_cast<uint8_t*>(vram_backdoor_) + src_va, len);
        }
    }

    // Stage 1.3a 接入: ring mode 启用 + BAR1 doorbell 触发 ring consume
    void SdmaEngineTLM::enable_ring_mode(::tlm::gpu::SdmaRingBuffer::RingSize size,
                                        ::tlm::gpu::SdmaRingBuffer::EntrySize entry_sz) {
        ring_buffer_ = std::make_unique<::tlm::gpu::SdmaRingBuffer>(size, entry_sz);
        ring_mode_ = true;
        ring_consumed_count_ = 0;
    }

    // UE 经 BAR1 窗口写 entry (测试 + 真实路径)
    bool SdmaEngineTLM::ring_write_entry(uint32_t index, const uint8_t* data, size_t len) {
        if (!ring_mode_ || ring_buffer_ == nullptr || data == nullptr) {
            return false;
        }
        return ring_buffer_->write_entry(index, data, len);
    }

    // 公开 ABI: mmio_write (与 PcieEndpointIP::mmio_write 独立; 测试 + UE dlopen ABI 入口)
    bool SdmaEngineTLM::mmio_write(uint32_t bar, uint64_t offset, uint64_t data) {
        if (!ring_mode_ || ring_buffer_ == nullptr) {
            return true;  // 非 ring mode, 接受但 no-op (per spec non-doorbell 落入 BAR space)
        }
        // BAR1+0x10010000: doorbell → consume ring[RPTR..WPTR=data]
        if (bar == 1 && offset == 0x10010000ULL) {
            const uint32_t wptr_count = static_cast<uint32_t>(data);
            const uint32_t cur_wptr = ring_buffer_->wptr().load();
            // consume from current wptr to (cur_wptr + wptr_count)
            for (uint32_t i = 0; i < wptr_count; ++i) {
                const uint32_t idx = cur_wptr + i;
                if (idx >= ring_buffer_->capacity_entries()) {
                    break;  // 越界 (per spec WPTR 不应越界; 这里防御性)
                }
                auto buf = ring_buffer_->read_entry(idx);
                DmaDescriptor d = SdmaPacket::deserialize_descriptor(buf.data(), buf.size());
                // 处理 (H2D / D2H, 与既有 handle_desc_in 等价)
                bundles::CompletionBundle done;
                done.task_id.write(static_cast<uint32_t>(d.tag));
                done.tag.write(static_cast<uint32_t>(d.tag));
                int rc = 0;
                if (d.dir == DmaDescriptor::Dir::H2D) {
                    rc = process_h2d(d, done);
                } else {
                    rc = process_d2h(d, done);
                }
                // emit completion (KIND_DMA_DONE on done_out)
                std::string err_msg;
                int err_code_for_cb = rc;
                emit_completion(done, err_code_for_cb, err_msg);
            }
            ring_buffer_->wptr().fetch_add(wptr_count, std::memory_order_acq_rel);
            ring_consumed_count_ += wptr_count;
            return true;
        }
        // 其他 BAR/offset: 接受但 no-op (PcieEndpointIP::bar_store_ 已独立处理)
        return true;
    }

    // Stage 1.3d: 处理 fence_queue_ 内的所有 Fence descriptor
    //   - push entry 到 CompletionRing (if injected)
    //   - 触发 MSI-X vector 0 (via fence_msix_handler_ async)
    void SdmaEngineTLM::process_fence_queue() {
        while (!fence_queue_.empty()) {
            FenceDescriptor fence = fence_queue_.front();
            fence_queue_.erase(fence_queue_.begin());

            // 1. push entry 到 CompletionRing (legacy push API 接受 task_id/status)
            if (completion_ring_ != nullptr) {
                bundles::CompletionEntry entry;
                entry.task_id.write(static_cast<uint32_t>(fence.fence_id));
                entry.status.write(0);  // Fence 成功 status=0
                entry.tag.write(fence.tag);
                completion_ring_->push(entry);
            }

            // 2. 异步触发 MSI-X vector kSdmaFenceVector (= 0) via handler
            //    200ms 内必触发 (per spec); 测试通过 sleep 验证
            if (fence_msix_handler_) {
                // detach 异步线程模拟 sim_loop drain 后延迟触发
                FenceMsixHandler h = fence_msix_handler_;
                uint16_t vector = kSdmaFenceVector;
                uint32_t payload = static_cast<uint32_t>(fence.fence_id);
                std::thread([h, vector, payload]() {
                    // 短延迟 (模拟 sim_loop drain; 测试 < 200ms)
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    h(vector, payload);
                }).detach();
            }
        }
    }

} // namespace tlm::gpu