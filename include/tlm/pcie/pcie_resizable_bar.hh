// include/tlm/pcie/pcie_resizable_bar.hh
// Resizable BAR Capability (Stage 2.1 §2.3) - INV-C disable/reprogram/enable sequence
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §2.3
//
// INV-C: Resizable BAR Cap 必须保持向后兼容 (BAR size 调整前先 disable + reprogram)
// 作者 CppTLM Team / 日期 2027-02-09
#ifndef CPPTLM_PCIE_RESIZABLE_BAR_HH
#define CPPTLM_PCIE_RESIZABLE_BAR_HH

#include <cstdint>

namespace tlm::pcie {

    class ResizableBar {
    public:
        enum class State : uint8_t { Disabled = 0, Programming = 1, Enabled = 2 };

        // Initial state: Disabled (default per spec)
        ResizableBar() = default;

        // INV-C sequence: disable → reprogram → enable
        // 任何 enable 请求必须经 disable, 否则拒绝
        bool reprogram_size(uint32_t new_size_bytes) noexcept {
            if (state_ == State::Enabled) {
                // 必须先 disable (INV-C)
                return false;
            }
            new_size_bytes_ = new_size_bytes;
            state_ = State::Programming;
            return true;
        }

        bool enable() noexcept {
            // 必须先经 Programming (reprogram 后) 才可 enable
            if (state_ != State::Programming) {
                return false;
            }
            state_ = State::Enabled;
            return true;
        }

        void disable() noexcept {
            state_ = State::Disabled;
        }

        [[nodiscard]] State state() const noexcept { return state_; }
        [[nodiscard]] uint32_t size_bytes() const noexcept { return new_size_bytes_; }
        [[nodiscard]] bool enabled() const noexcept { return state_ == State::Enabled; }

    private:
        State state_ = State::Disabled;
        uint32_t new_size_bytes_ = 0;
    };

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_RESIZABLE_BAR_HH
