// include/tlm/gpu/gpu_mesh_noc_tlm.hh
// GpuMeshNoC: GPC 之间 mesh interconnect (XY 路由)
// 功能: 简化的 NxN mesh 网络 + XY 维度序路由 + hops × latency 延迟模型
// 作者 CppTLM Team / 日期 2026-06-24 (扩展 2026-09-13 Stage 1.3b)
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.3
//       openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md §1.3b D2D NoC
// Phase 8.A Task 3 stub + Stage 1.3b D2D payload forwarding 扩展
// 类名 GpuMeshNoC 避免与 include/tlm/cluster/gpu_noc_cluster.hh 中已有的 GpuNoC (SimModule) 类冲突
#ifndef TLM_GPU_GPU_MESH_NOC_TLM_HH
#define TLM_GPU_GPU_MESH_NOC_TLM_HH

#include <cstdint>
#include <utility>
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"

namespace tlm {

    /**
     * @brief GPC 之间 mesh interconnect (XY 维度序路由)
     *
     * 简化模型 (per D2 决策): XY 路由 + hops × latency
     * 不模拟 VC 分配/拥塞控制
     *
     * Stage 1.3b 扩展 (D2D NoC payload forwarding):
     *   - d2d_forward_payload(): VRAM→VRAM payload 转发 (bypassing PCIe TLP)
     *   - payload_bytes_forwarded_ + last_transfer_latency_cycles_ 统计
     *   - simulated_throughput_GBps() getter (per spec Oracle O9 修订)
     */
    class GpuMeshNoC : public ChStreamModuleBase {
    public:
        explicit GpuMeshNoC(const std::string& name, EventQueue* eq)
            : ChStreamModuleBase(name, eq) {
        }
        ~GpuMeshNoC() override = default;

        std::string get_module_type() const override {
            return "GpuMeshNoC";
        }

        // ChStreamModuleBase required override
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
            adapter_ = adapter;
        }

        /**
         * @brief 计算 XY 路由延迟
         * @param src 源坐标 (x, y)
         * @param dst 目标坐标 (x, y)
         * @return 路由延迟 cycles = (|dx| + |dy|) × hops_latency
         */
        uint32_t route_latency(std::pair<uint32_t, uint32_t> src,
                               std::pair<uint32_t, uint32_t> dst) const;

        /// 配置加载（`ADR-X.17` D-1「读取即声明」）
        ///
        /// 两键**真被运行期读取**（`route_latency()` 与 `d2d_forward_payload()` 的延迟模型
        /// 都用 `hops_latency_`;`get_dim()` / 最坏路径循环用 `dim_`）⇒ **态①：接真实消费**
        /// （per `ADR-X.17` D-6 的行为判别义务;判别测试见 `test_param_effect_landing.cc`）。
        void on_config_loaded() override {
            load_param_opt("dim", dim_);
            load_param_opt("hops_latency", hops_latency_);
        }

        // === 程序化 setter (JSON 解析后注入；与上面的 load_param 等价，保留供 C++ 侧注入) ===
        void set_dim(uint32_t dim) {
            dim_ = dim;
        }
        void set_hops_latency(uint32_t hops_latency) {
            hops_latency_ = hops_latency;
        }

        uint32_t get_dim() const {
            return dim_;
        }
        uint32_t get_hops_latency() const {
            return hops_latency_;
        }

        // === Stage 1.3b: D2D payload forwarding ===
        //
        // D2D payload 转发 (per openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma §1.3b):
        //   - src_vram/dst_vram 是指向 VRAM backing store 的指针
        //   - len 是 payload 字节数
        //   - memcpy src→dst, 累加统计
        //   - 延迟模型: route_latency({0,0}, {dim-1,dim-1}) × hops_latency_ (最坏路径)
        //
        // Returns: latency_cycles 用于 simulated_throughput_GBps() 计算
        uint32_t d2d_forward_payload(void* dst_vram, const void* src_vram, uint64_t len);

        // 统计 getter (per Oracle O9 修订 measurement definition)
        uint64_t payload_bytes_forwarded() const noexcept {
            return payload_bytes_forwarded_;
        }
        uint32_t last_transfer_latency_cycles() const noexcept {
            return last_transfer_latency_cycles_;
        }

        // simulated_throughput_GBps = payload_bytes / simulated_latency_s
        //   cycle_period_s 默认 1ns (1e-9 s)
        //   throughput = bytes / (cycles × cycle_period_s)
        //   1 GB/s = 1e9 bytes/s; threshold: 100 GB/s = 100e9 bytes/s
        double simulated_throughput_GBps(double cycle_period_s = 1e-9) const;

        // cycle_period_s 程序化 setter (JSON 注入)
        void set_cycle_period_s(double p) noexcept {
            cycle_period_s_ = p;
        }

        void tick() override;

    private:
        cpptlm::StreamAdapterBase* adapter_ = nullptr;
        uint32_t dim_ = 2;
        uint32_t hops_latency_ = 2;
        uint64_t cycle_counter_ = 0;

        // Stage 1.3b 统计
        uint64_t payload_bytes_forwarded_ = 0;
        uint32_t last_transfer_latency_cycles_ = 0;
        double cycle_period_s_ = 1e-9; // 默认 1ns/cycle
    };

} // namespace tlm

#endif // TLM_GPU_GPU_MESH_NOC_TLM_HH