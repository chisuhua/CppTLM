// include/tlm/gpu/vector_regfile_tlm.hh
// VectorRegFileTLM: 简化向量寄存器文件
// 作者 CppTLM Team / 日期 2026-06-30
#ifndef TLM_GPU_VECTOR_REGFILE_TLM_HH
#define TLM_GPU_VECTOR_REGFILE_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tlm {

    struct PairHash {
        std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const noexcept {
            return (static_cast<std::size_t>(p.first) << 32) | static_cast<std::size_t>(p.second);
        }
    };

    class VectorRegFileTLM : public ChStreamModuleBase {
    public:
        explicit VectorRegFileTLM(const std::string& name, EventQueue* eq)
            : ChStreamModuleBase(name, eq) {
        }
        ~VectorRegFileTLM() override = default;

        std::string get_module_type() const override {
            return "VectorRegFileTLM";
        }

        void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
            adapter_ = adapter;
        }

        void set_num_regs(uint32_t n) {
            num_regs_ = n;
        }
        void set_num_banks(uint32_t n) {
            num_banks_ = n;
        }

        uint32_t get_num_regs() const {
            return num_regs_;
        }
        uint32_t get_num_banks() const {
            return num_banks_;
        }

        void write(uint32_t lane, uint32_t reg, uint32_t value) {
            storage_[{lane, reg}] = value;
        }

        uint32_t read(uint32_t lane, uint32_t reg) const {
            auto it = storage_.find({lane, reg});
            return it != storage_.end() ? it->second : 0;
        }

        uint32_t bank_conflict_cycles(const std::vector<uint32_t>& regs) const {
            if (regs.size() <= 1)
                return 1;
            std::unordered_map<uint32_t, uint32_t> bank_count;
            for (uint32_t r : regs) {
                bank_count[r % num_banks_]++;
            }
            uint32_t max_count = 0;
            for (const auto& kv : bank_count) {
                max_count = std::max(max_count, kv.second);
            }
            return 1 + (max_count - 1);
        }

        void tick() override {
        }

    private:
        cpptlm::StreamAdapterBase* adapter_ = nullptr;
        uint32_t num_regs_ = 64;
        uint32_t num_banks_ = 4;
        std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, PairHash> storage_;
    };

} // namespace tlm

#endif // TLM_GPU_VECTOR_REGFILE_TLM_HH
