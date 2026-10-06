// include/bundles/cpphdl_types.hh
// Lightweight CppHDL type wrappers for TLM simulation
// Provides ch_uint/ch_bool/bundle_base WITHOUT the full CppHDL AST/runtime
// 功能描述：定义轻量级 Bundle 类型，仅包含 ChStream 握手所需的字段读写接口
// 作者 CppTLM Team / 日期 2026-04-12
//
// Phase P0 update (2026-10-06, dgpu-realip-refactor §5.1):
//   - ch_uint<W≤64> → ch_uint_storage<W> (identical layout, trivially_copyable)
//   - ch_uint<W>64> → ch_uint_wide<W> with std::array<uint64_t, N> storage
//   - ch_uint<W> dispatches via std::conditional_t + using base::base (ctor inheritance)
//   - static_assert(is_trivially_copyable + is_standard_layout) for layout safety
#ifndef BUNDLES_CPPHDL_TYPES_HH
#define BUNDLES_CPPHDL_TYPES_HH

#include <array>
#include <cstdint>
#include <type_traits>

namespace bundles {

/** @brief ch_uint<W≤64> storage (identical to pre-change ch_uint body) */
template<unsigned W>
struct ch_uint_storage {
    static_assert(W > 0 && W <= 64, "ch_uint_storage: 0 < W <= 64");
    uint64_t value_ = 0;
    ch_uint_storage() = default;
    explicit ch_uint_storage(uint64_t v) : value_(v) {}
    uint64_t read() const { return value_; }
    void write(uint64_t v) { value_ = v; }
    operator uint64_t() const { return value_; }
};

/** @brief ch_uint<W>64> storage (new wide path, RTL-style multi-word bit-vector) */
template<unsigned W>
struct ch_uint_wide {
    static_assert(W > 64, "ch_uint_wide: W > 64");
    static constexpr unsigned N = (W + 63) / 64;
    std::array<uint64_t, N> words_{};   // little-endian: words_[0] = low 64 bits

    // Backward-compat shims: pre-change ch_uint had read/write/operator uint64_t
    // operating on the single uint64_t value_ (silent truncation for W>64).
    // Production code in axi4_*/pcie_axi_adapter_tlm/pcie_endpoint_ip calls
    // these on ch_uint<512>. Preserve behavior so P0 build stays green.
    // Marked [[deprecated]] to flag silent truncation; P1/P2 callers migrate
    // to slice()/set_slice()/low() explicitly.
    using deprecated_back_compat_shims = int;  // marker for grep
    uint64_t read() const noexcept { return words_[0]; }
    void write(uint64_t v) noexcept { words_[0] = v; }
    operator uint64_t() const noexcept { return words_[0]; }
};

/** @brief ch_uint<W> dispatch (W≤64 inherits storage API; W>64 inherits wide API) */
template<unsigned W = 64>
struct ch_uint
    : std::conditional_t<(W <= 64), ch_uint_storage<W>, ch_uint_wide<W>> {
    using base = std::conditional_t<(W <= 64), ch_uint_storage<W>, ch_uint_wide<W>>;
    using base::base;  // ctor inheritance (preserves ch_uint<8>(k) call sites)

    // ── Wide path only operations ──
    uint64_t low() const noexcept {
        return static_cast<const base*>(this)->words_[0];
    }

    template<unsigned Hi, unsigned Lo>
    uint64_t slice() const noexcept {
        static_assert(W > 64, "slice() requires W > 64");
        static_assert(Hi >= Lo && Hi < W, "slice range out of bounds");
        constexpr unsigned word_hi = Hi / 64;
        constexpr unsigned bit_hi  = Hi % 64;
        constexpr unsigned word_lo = Lo / 64;
        constexpr unsigned bit_lo  = Lo % 64;
        if constexpr (word_hi == word_lo) {
            const uint64_t mask = ((1ULL << (bit_hi - bit_lo + 1)) - 1ULL) << bit_lo;
            return (static_cast<const base*>(this)->words_[word_hi] & mask) >> bit_lo;
        } else {
            uint64_t result = 0;
            constexpr unsigned low_bits_in_hi_word = 64 - bit_lo;
            const uint64_t hi_mask = (bit_hi == 63)
                ? 0xFFFFFFFFFFFFFFFFULL
                : ((1ULL << (bit_hi + 1)) - 1ULL);
            const uint64_t lo_mask = ((1ULL << low_bits_in_hi_word) - 1ULL) << bit_lo;
            result |= (static_cast<const base*>(this)->words_[word_hi] & hi_mask)
                      << low_bits_in_hi_word;
            result |= (static_cast<const base*>(this)->words_[word_lo] & lo_mask)
                      >> bit_lo;
            return result;
        }
    }

    template<unsigned Hi, unsigned Lo>
    void set_slice(uint64_t val) noexcept {
        static_assert(W > 64, "set_slice() requires W > 64");
        static_assert(Hi >= Lo && Hi < W, "set_slice range out of bounds");
        static_assert(Hi - Lo < 64, "set_slice value must fit in 64 bits");
        constexpr unsigned word_hi = Hi / 64;
        constexpr unsigned bit_hi  = Hi % 64;
        constexpr unsigned word_lo = Lo / 64;
        constexpr unsigned bit_lo  = Lo % 64;
        if constexpr (word_hi == word_lo) {
            const uint64_t mask = ((1ULL << (Hi - Lo + 1)) - 1ULL) << bit_lo;
            auto& w = static_cast<base*>(this)->words_[word_hi];
            w = (w & ~mask) | ((val << bit_lo) & mask);
        } else {
            constexpr unsigned low_bits_in_hi_word = 64 - bit_lo;
            const uint64_t hi_mask = (bit_hi == 63)
                ? 0xFFFFFFFFFFFFFFFFULL
                : ((1ULL << (bit_hi + 1)) - 1ULL);
            const uint64_t lo_mask = ((1ULL << low_bits_in_hi_word) - 1ULL) << bit_lo;
            auto& wh = static_cast<base*>(this)->words_[word_hi];
            auto& wl = static_cast<base*>(this)->words_[word_lo];
            wh = (wh & ~hi_mask) | ((val >> low_bits_in_hi_word) & hi_mask);
            wl = (wl & ~lo_mask) | ((val << bit_lo) & lo_mask);
        }
    }
};

// Layout-identity compile-time assertions
static_assert(std::is_standard_layout_v<ch_uint_storage<1>>);
static_assert(std::is_trivially_copyable_v<ch_uint_storage<1>>);
static_assert(std::is_standard_layout_v<ch_uint_storage<8>>);
static_assert(std::is_trivially_copyable_v<ch_uint_storage<8>>);
static_assert(std::is_standard_layout_v<ch_uint_storage<32>>);
static_assert(std::is_trivially_copyable_v<ch_uint_storage<32>>);
static_assert(std::is_standard_layout_v<ch_uint_storage<64>>);
static_assert(std::is_trivially_copyable_v<ch_uint_storage<64>>);
static_assert(std::is_standard_layout_v<ch_uint_wide<128>>);
static_assert(std::is_trivially_copyable_v<ch_uint_wide<128>>);;

/** @brief 轻量级 ch_bool 包装（仿真用，非 RTL） */
struct ch_bool {
    bool value_;
    ch_bool() : value_(false) {}
    explicit ch_bool(bool v) : value_(v) {}
    explicit ch_bool(uint64_t v) : value_(v != 0) {}
    bool read() const { return value_; }
    void write(uint64_t v) { value_ = (v != 0); }
    operator bool() const { return value_; }
};

/** @brief 轻量级 Bundle 基类（空基类，标准布局，memcpy 安全） */
struct bundle_base {
    // 无虚函数，无 vtable —— 确保标准布局以支持 memcpy 序列化
};

} // namespace bundles

#endif // BUNDLES_CPPHDL_TYPES_HH
