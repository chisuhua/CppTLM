// include/core/param_loader.hh
// 模块参数「读取即声明」机制：一次调用完成 声明 + 类型校验 + 范围校验 + 默认值 + 赋值 + 已消费键登记
// 作者 CppTLM Team / 日期 2026-11-17
//
// 决策源：docs/adr/ADR-X.17-module-param-read-declares.md（D-1 / D-3a / D-5）
// 用法（模块内，经 SimObject 的 protected 成员模板转发）：
//
//     load_param("buffer_size", buffer_depth_, 1u, 64u, 8u);   // 可选 + 范围 + 默认值
//     load_param("node_x",      node_x_);                      // 必填（无默认值）
//
// 语义：
//   - 键缺失 && 有默认值  ⇒ 取默认值（**只写成员，不写回 JSON**）
//   - 键缺失 && 无默认值  ⇒ 抛 ParamValidationError（必填）
//   - 键存在             ⇒ 类型检查 → 范围检查 → 赋值
//   - 每次调用都把键登记进 ParamLoadContext ⇒ 加载末尾可检出「未知键」
//
// ⛔ 不使用宏 / 不使用声明 DSL（用户硬约束）。
#ifndef CORE_PARAM_LOADER_HH
#define CORE_PARAM_LOADER_HH

#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/param_errors.hh"

namespace cpptlm {

/**
 * @brief 单实例的参数加载上下文（由 SimObject 按实例持有）
 *
 * 负责两件事：
 *   1. 记住本次加载读的是哪份 JSON（params 子对象，或 legacy 扁平的整个 module entry）
 *   2. 记住哪些键已被消费 ⇒ 差集即「未知键」
 */
class ParamLoadContext {
public:
    /// 开始一次加载。`cfg` 的生命周期由调用方（SimObject 持有的 config 副本）保证。
    /// `from_params_object` = false 表示 legacy 扁平形式（整个 module entry 当 cfg）⇒ 未知键检查豁免。
    void begin(const nlohmann::json& cfg, std::string module_name, bool from_params_object) {
        cfg_ = &cfg;
        module_name_ = std::move(module_name);
        from_params_object_ = from_params_object;
        consumed_.clear();
    }

    const nlohmann::json& cfg() const { return *cfg_; }
    const std::string& module_name() const { return module_name_; }
    bool from_params_object() const { return from_params_object_; }

    void mark_consumed(std::string_view key) { consumed_.emplace(key); }

    /// cfg 中的键 − 已消费键（升序，便于稳定报错）
    std::vector<std::string> unknown_keys() const {
        std::vector<std::string> out;
        if (!cfg_ || !cfg_->is_object()) return out;
        for (auto it = cfg_->begin(); it != cfg_->end(); ++it) {
            if (consumed_.find(it.key()) == consumed_.end()) out.push_back(it.key());
        }
        return out;
    }

private:
    const nlohmann::json* cfg_ = nullptr;
    std::string module_name_;
    bool from_params_object_ = true;
    std::set<std::string, std::less<>> consumed_;
};

namespace detail {

/// 拼统一的失败原因文本（**必须**含键名 + 期望范围；模块名由异常字段承载）
inline std::string range_reason(std::string_view key, const std::string& expected,
                                const std::string& got) {
    std::string r(key);
    r += ": expected ";
    r += expected;
    r += ", got ";
    r += got;
    return r;
}

template <class T>
std::string value_to_string(const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(static_cast<long long>(v));
    } else {
        return std::to_string(v);
    }
}

} // namespace detail

/**
 * @brief 读取 + 校验 + 赋值 + 登记（唯一入口）
 *
 * @tparam T 成员存储类型（决定接受域与赋值目标）
 * @param ctx       本实例的加载上下文
 * @param key       配置键名
 * @param target    目标成员
 * @param lo        下界（含），nullopt = 不检查
 * @param hi        上界（含），nullopt = 不检查
 * @param def       默认值，nullopt = **必填**
 *
 * ⛔ 所有失败抛 cpptlm::ParamValidationError（不新造异常类型，ADR-X.17 D-3）
 */
template <class T>
void load_param(ParamLoadContext& ctx, std::string_view key, T& target,
                std::optional<T> lo = std::nullopt, std::optional<T> hi = std::nullopt,
                std::optional<T> def = std::nullopt) {
    const nlohmann::json& cfg = ctx.cfg();
    const std::string k(key);

    auto it = cfg.is_object() ? cfg.find(k) : cfg.end();

    // ---- 缺失 ----
    if (it == cfg.end()) {
        if (!def.has_value()) {
            throw ParamValidationError(ctx.module_name(), k, k + ": required parameter missing");
        }
        target = *def;
        ctx.mark_consumed(key);
        return;
    }

    // ---- 存在：类型 ----
    T value{};
    if constexpr (std::is_same_v<T, bool>) {
        if (!it->is_boolean()) {
            throw ParamValidationError(ctx.module_name(), k,
                                       detail::range_reason(key, "boolean", it->dump()));
        }
        value = it->get<bool>();
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        // ⛔ 只接受整数 JSON：8.0 / 1e20 / true 一律拒绝（不依赖 get<int64_t>() 的隐式转换）
        if (!it->is_number_integer()) {
            throw ParamValidationError(ctx.module_name(), k,
                                       detail::range_reason(key, "integer", it->dump()));
        }
        const long long v = it->get<long long>();
        if (v < static_cast<long long>(std::numeric_limits<T>::min()) ||
            v > static_cast<long long>(std::numeric_limits<T>::max())) {
            throw ParamValidationError(
                ctx.module_name(), k,
                detail::range_reason(key,
                                     "storable range [" +
                                         std::to_string(static_cast<long long>(
                                             std::numeric_limits<T>::min())) +
                                         "," +
                                         std::to_string(static_cast<long long>(
                                             std::numeric_limits<T>::max())) +
                                         "]",
                                     std::to_string(v)));
        }
        value = static_cast<T>(v);
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        if (it->is_number_unsigned()) {
            const unsigned long long v = it->get<unsigned long long>();
            if (v > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
                throw ParamValidationError(
                    ctx.module_name(), k,
                    detail::range_reason(key,
                                         "storable range [0," +
                                             std::to_string(static_cast<unsigned long long>(
                                                 std::numeric_limits<T>::max())) +
                                             "]",
                                         std::to_string(v)));
            }
            value = static_cast<T>(v);
        } else if (it->is_number_integer()) {
            const long long v = it->get<long long>();
            if (v < 0) {
                throw ParamValidationError(ctx.module_name(), k,
                                           detail::range_reason(key, "unsigned integer",
                                                                std::to_string(v)));
            }
            if (static_cast<unsigned long long>(v) >
                static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
                throw ParamValidationError(
                    ctx.module_name(), k,
                    detail::range_reason(key,
                                         "storable range [0," +
                                             std::to_string(static_cast<unsigned long long>(
                                                 std::numeric_limits<T>::max())) +
                                             "]",
                                         std::to_string(v)));
            }
            value = static_cast<T>(v);
        } else {
            throw ParamValidationError(ctx.module_name(), k,
                                       detail::range_reason(key, "unsigned integer", it->dump()));
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        if (!it->is_number()) {
            throw ParamValidationError(ctx.module_name(), k,
                                       detail::range_reason(key, "number", it->dump()));
        }
        value = it->get<T>();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!it->is_string()) {
            throw ParamValidationError(ctx.module_name(), k,
                                       detail::range_reason(key, "string", it->dump()));
        }
        value = it->get<std::string>();
    } else {
        static_assert(std::is_arithmetic_v<T> || std::is_same_v<T, std::string>,
                      "load_param: unsupported target type (仅支持整型 / 浮点 / bool / string)");
    }

    // ---- 范围 ----
    const bool has_lo = lo.has_value();
    const bool has_hi = hi.has_value();
    if ((has_lo && value < *lo) || (has_hi && value > *hi)) {
        std::string expected = "[";
        expected += has_lo ? detail::value_to_string(*lo) : "-inf";
        expected += ",";
        expected += has_hi ? detail::value_to_string(*hi) : "+inf";
        expected += "]";
        throw ParamValidationError(ctx.module_name(), k,
                                   detail::range_reason(key, "range " + expected,
                                                        detail::value_to_string(value)));
    }

    target = value;
    ctx.mark_consumed(key);
}

} // namespace cpptlm

#endif // CORE_PARAM_LOADER_HH
