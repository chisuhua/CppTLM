// include/tlm/gpu/shared_memory_tlm.hh
// SharedMemoryTLM: SM 内部 shared memory + L1 unified, 32 bank
// 功能: SM 内 shared memory 抽象 + bank conflict 周期模型
// 作者 CppTLM Team / 日期 2026-06-24
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.1
// Phase 8.A Task 1 stub (待 Sisyphus-Junior 子代理实现)
#ifndef TLM_GPU_SHARED_MEMORY_TLM_HH
#define TLM_GPU_SHARED_MEMORY_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>

namespace tlm {

/**
 * @brief SM 内部 shared memory + L1 unified (32 bank configurable)
 *
 * 简化模型 (按 D2 决策): base 1 cycle + 每个 conflict way +1 cycle
 * 不模拟真实 SM 内部 cache hierarchy
 */
class SharedMemoryTLM : public ChStreamModuleBase {
public:
    explicit SharedMemoryTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}
    ~SharedMemoryTLM() override = default;

    std::string get_module_type() const override { return "SharedMemoryTLM"; }

    // ChStreamModuleBase required override
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // === 程序化 setter (JSON 解析后注入) ===
    void set_size_kb(uint32_t size_kb) { size_kb_ = size_kb; }
    void set_banks(uint32_t banks) { banks_ = banks; }

    /**
     * @brief 计算 bank conflict 引起的额外 cycle 数
     * @param num_threads 并发访问线程数 (通常 32)
     * @param stride_bytes 步长 (字节)
     * @return 额外 cycle 数 (base 1 cyc + 每个 conflict way +1 cyc)
     */
    uint32_t bank_conflict_cycles(uint32_t num_threads, uint32_t stride_bytes) const;

    uint32_t get_size_kb() const { return size_kb_; }
    uint32_t get_banks() const { return banks_; }

    void tick() override;

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t size_kb_ = 64;   // 默认 64 KB (与 GB203 SM L1 一致)
    uint32_t banks_ = 32;     // 默认 32 bank (NVIDIA 标准)
};

}  // namespace tlm

#endif  // TLM_GPU_SHARED_MEMORY_TLM_HH