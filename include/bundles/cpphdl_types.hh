// include/bundles/cpphdl_types.hh
// Lightweight CppHDL type wrappers for TLM simulation
// Provides ch_uint/ch_bool/bundle_base WITHOUT the full CppHDL AST/runtime
// 功能描述：定义轻量级 Bundle 类型，仅包含 ChStream 握手所需的字段读写接口
// 作者 CppTLM Team / 日期 2026-04-12
#ifndef BUNDLES_CPPHDL_TYPES_HH
#define BUNDLES_CPPHDL_TYPES_HH

#include <cstdint>

namespace bundles {

/** @brief 轻量级 ch_uint 包装（仿真用，非 RTL） */
template<unsigned W = 64>
struct ch_uint {
    uint64_t value_;
    ch_uint() : value_(0) {}
    explicit ch_uint(uint64_t v) : value_(v) {}
    uint64_t read() const { return value_; }
    void write(uint64_t v) { value_ = v; }
    operator uint64_t() const { return value_; }
};

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

/** @brief 轻量级 Bundle 基类（无 AST 依赖，仅用于 memcpy 序列化） */
struct bundle_base {
    virtual ~bundle_base() = default;
};

} // namespace bundles

#endif // BUNDLES_CPPHDL_TYPES_HH
