// include/tlm/gpu/sdma_engine_tlm.hh
// SdmaEngineTLM: SOC 片内 PCIe master 引擎（SDMA/copy engine）
// 功能描述：dGPU SOC die 内的 PCIe master 模型；接收 host→endpoint TLP，
//           发起 upstream DMA 访问 host 内存（经 IOMMU translate callback），
//           访问 SOC VRAM 走内部 memory 端口，完成/错误经 done_out 通知
//           端口：desc_in / mem_in / mem_out / host_out / done_out (5 端口)
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/design.md §3,§4,§5,§6
//       ADR-SOC-07 D3 (PCIe master 归属 SOC)
//       ADR-088 §D3.8 (cpptlm_dma_translate_cb 同步签名)
#ifndef CPPTLM_TLM_GPU_SDMA_ENGINE_TLM_HH
#define CPPTLM_TLM_GPU_SDMA_ENGINE_TLM_HH

#include "bundles/dma_bundles_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/sim_object.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace tlm::gpu {

    // 前向声明
    class SdmaEngineTLM;

    /**
     * @brief DMA-translate 回调类型（与 ADR-088 §D3.8 同步签名对齐）
     *
     * 行为契约：
     *   - 输入：iova, size（请求的 IOVA + 字节数）
     *   - 输出：phys（输出参数，translate 后的物理地址）
     *   - 返回值：0=成功；<0=失败（per errno 语义，如 -EFAULT=-14）
     *
     * 实现位置：DGpuBoard 注入系统 IOMMU 翻译回调（per ADR-SOC-07 D3.1）
     *          本组件持有 std::function<cpptlm_dma_translate_cb>，调用方
     *          （DGpuBoard）通过 set_translate_cb() 注入。
     *
     * MVP stub：测试使用 fake translate_cb（见 test_sdma_engine_*.cc）。
     */
    using DmaTranslateCb = std::function<int(uint64_t iova, uint32_t size, uint64_t& phys)>;

    /**
     * @brief SDMA 错误回调类型（cpptlm_error_cb_t 路径，per design.md §6）
     *
     * 用于将 SDMA 内部错误（如 translate fault）上报到 board error channel。
     * 实现位置：DGpuBoard 注入。
     */
    using SdmaErrorCb = std::function<void(int err_code, const std::string& msg)>;

    /**
     * @brief SdmaEngineTLM：5 端口 ChStream PCIe master 引擎（SDMA/copy engine）
     *
     * 设计原则（per design.md §3,§4,§5,§6 + spec.md）：
     *   - 端口：
     *     0. desc_in   (ingress) → PcieTlpBundle (kind=DMA_DESC 编码 DmaDescriptor)
     *     1. mem_in    (ingress) → PcieTlpBundle                 VRAM 读响应 (D2H 路径)
     *     2. mem_out   (egress)  → PcieTlpBundle                 VRAM 读/写 (PcieTlpBundle::MEM_READ/MEM_WRITE)
     *     3. host_out  (egress)  → PcieTlpBundle                 upstream DMA 事务
     *     4. done_out  (egress)  → PcieTlpBundle (kind=DMA_DONE 编码 CompletionBundle)
     *
     *   - Wire-format 选择（MVP）：所有 5 端口共享 PcieTlpBundle 作为底层 wire-format
     *     （参考 PcieEndpointTLM 复用 PcieTlpBundle 承载 IRQ_DELIVERY 的设计）。
     *     desc_in / done_out 通过扩展的 kind 字段（DMA_DESC / DMA_DONE）携带 DMA
     *     语义负载（host_iova / vram_offset / size / tag / task_id / status），
     *     详见 to_descriptor() / from_completion() 内部辅助函数。
     *     升级路径：若未来引入专用 DmaDescBundle / DoneBundle 作为 wire-format，
     *     仅需修改 SdmaEngineTLM 内部转换函数 + ChStreamAdapterFactory 注册；
     *     外部 API（DmaDescriptor C++ 类型、JSON 配置）保持向后兼容。
     *
     *   - 子组件：无（逻辑内部使用 state machine 处理 inflight 描述符）
     *   - 多端口 StreamAdapter 注入路径：set_stream_adapter(adapters[5])
     *     每个 adapter 类型擦除为 cpptlm::StreamAdapterBase*
     *
     * JSON 参数（per design.md §5 + spec.md Scenario "JSON instantiation"）：
     *   {
     *     "max_inflight": 4,            // 同时在飞的描述符数上限，默认 4
     *     "translate_latency": 0,       // translate callback 同步延迟（cycles），默认 0
     *     "vram_size_bytes": 268435456  // SOC VRAM 大小（用于越界检查），默认 256MB
     *   }
     *
     * 反压（per design.md §5 + spec.md R3 缓解）：
     *   - 每 tick 处理至多 max_inflight 个描述符
     *   - 满窗口时 desc_in 收到新描述符不立即处理；待 done_out 释放后再次接受
     *   - 完成路径不依赖新描述符接收 → 避免死锁
     */
    class SdmaEngineTLM : public ChStreamModuleBase {
    public:
        static constexpr unsigned NUM_PORTS = 5;

        // 端口索引常量（per design.md §2.5 Port index ordering lock）
        static constexpr unsigned PORT_DESC_IN  = 0;  // ingress
        static constexpr unsigned PORT_MEM_IN   = 1;  // ingress
        static constexpr unsigned PORT_MEM_OUT  = 2;  // egress
        static constexpr unsigned PORT_HOST_OUT = 3;  // egress
        static constexpr unsigned PORT_DONE_OUT = 4;  // egress

        // 扩展 kind（PcieTlpBundle::kind 字段复用，标识 DMA wire-format 语义）
        static constexpr uint8_t KIND_DMA_DESC = 7;  // desc_in 端口：DmaDescriptorBundle 编码
        static constexpr uint8_t KIND_DMA_DONE = 8;  // done_out 端口：CompletionBundle 编码

        // 多端口 ChStream 端口（per MultiPortStreamAdapter 模板要求 public 访问）
        cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> req_in[NUM_PORTS];
        cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_PORTS];

        SdmaEngineTLM(const std::string& name, EventQueue* eq);
        ~SdmaEngineTLM() override;

        SdmaEngineTLM(const SdmaEngineTLM&) = delete;
        SdmaEngineTLM& operator=(const SdmaEngineTLM&) = delete;
        SdmaEngineTLM(SdmaEngineTLM&&) = delete;
        SdmaEngineTLM& operator=(SdmaEngineTLM&&) = delete;

        std::string get_module_type() const override {
            return "SdmaEngineTLM";
        }

        // ChStreamModuleBase 接口（5 端口）
        void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
        unsigned num_ports() const override {
            return NUM_PORTS;
        }

        // 模块业务逻辑
        void init() override;
        void tick() override;
        void do_reset(const ResetConfig&) override;

        // JSON 参数注入（ModuleFactory::set_config 触发 on_config_loaded 调用）
        void on_config_loaded() override;

        // 程序化 setter（测试用）
        void set_max_inflight(uint32_t n) {
            max_inflight_ = (n == 0) ? 1 : n;
        }
        void set_translate_latency(uint32_t cycles) {
            translate_latency_ = cycles;
        }
        void set_vram_size_bytes(uint64_t sz) {
            vram_size_bytes_ = sz;
        }

        // 测试断言 helper：5 个端口是否都接收到非空 adapter
        bool all_ports_have_adapter() const;

        // 单 adapter 访问（测试断言）
        cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const {
            return (idx < NUM_PORTS) ? adapters_[idx] : nullptr;
        }

        // 当前 inflight 数（测试断言）
        uint32_t inflight_count() const {
            return static_cast<uint32_t>(inflight_.size());
        }

        // 已完成描述符计数（测试断言）
        uint64_t completed_count() const {
            return completed_count_;
        }

        // 错误完成计数（测试断言）
        uint64_t error_count() const {
            return error_count_;
        }

        // IOMMU translate callback 注入（DGpuBoard 注入系统 IOMMU）
        void set_translate_cb(DmaTranslateCb cb) {
            translate_cb_ = std::move(cb);
        }
        DmaTranslateCb get_translate_cb() const {
            return translate_cb_;
        }

        // Board error callback 注入（错误路径上报）
        void set_error_cb(SdmaErrorCb cb) {
            error_cb_ = std::move(cb);
        }
        SdmaErrorCb get_error_cb() const {
            return error_cb_;
        }

        // Wire-format ↔ DmaDescriptor / CompletionBundle 转换（public for testability）
        static bundles::PcieTlpBundle to_pcie_tlp_descriptor(const DmaDescriptor& d);
        static DmaDescriptor from_pcie_tlp_descriptor(const bundles::PcieTlpBundle& p);
        static bundles::PcieTlpBundle to_pcie_tlp_completion(const bundles::CompletionBundle& c);
        static bundles::CompletionBundle from_pcie_tlp_completion(const bundles::PcieTlpBundle& p);

    private:
        // 5 端口 Bundle 适配器（per MultiPortStreamAdapter 模板要求）
        cpptlm::StreamAdapterBase* adapters_[NUM_PORTS] = {nullptr};

        // JSON 参数 / 默认值（per design.md §5）
        uint32_t max_inflight_ = 4;
        uint32_t translate_latency_ = 0;
        uint64_t vram_size_bytes_ = 256ull * 1024 * 1024;  // 256 MB

        // translate callback + error callback（DGpuBoard 注入）
        DmaTranslateCb translate_cb_;
        SdmaErrorCb    error_cb_;

        // inflight 描述符队列（per spec.md R3 + sdma-completion-ordering in-order 语义）
        //   - 仅保存 desc + age counter；完成时按 tag 顺序出队
        //   - 在 MVP 实现中,所有 desc 都按 FIFO 处理（无 in-order 跨 desc 调度）
        struct InflightDesc {
            DmaDescriptor desc;
            uint64_t      arrival_cycle;
        };
        std::vector<InflightDesc> inflight_;
        uint64_t completed_count_ = 0;
        uint64_t error_count_ = 0;

        // 内部：处理 desc_in 入口（每 tick 一次）
        void handle_desc_in();

        // 内部：H2D 处理（无 IOMMU callback stub：直接 emit host_out + mem_out）
        //   返回 0=ok, <0=errno
        int process_h2d(const DmaDescriptor& d, bundles::CompletionBundle& done);

        // 内部：D2H 处理
        int process_d2h(const DmaDescriptor& d, bundles::CompletionBundle& done);

        // 内部：emit done_out + 错误回调（如有错误）
        void emit_completion(const bundles::CompletionBundle& done, int err_code,
                             const std::string& err_msg);

        // 内部：VRAM 范围检查（per design.md §6 + spec.md Scenario "Invalid descriptor rejected"）
        bool is_vram_window_valid(uint64_t vram_offset, uint32_t size) const;
    };

} // namespace tlm::gpu

#endif // CPPTLM_TLM_GPU_SDMA_ENGINE_TLM_HH
