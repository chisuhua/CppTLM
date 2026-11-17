// include/core/param_admission.hh
// 参数加载机制的「准入表」：哪些模块类型已迁移到 load_param（⇒ 启用未知键检查）
// 作者 CppTLM Team / 日期 2026-11-17
//
// 决策源：docs/adr/ADR-X.17-module-param-read-declares.md（D-3 / D-3a）
//
// ⚠️ 本表是**唯一 C++ 真源**：运行时（SimObject::set_config）与任何仓库级测试/闸
//    都必须经由本头文件的接口判定，⛔ 不得在脚本或测试里硬编码类型名。
//
// ⚠️ **header-only 是必须的**：`SimObject::set_config` 的 inline 成员（enforce_param_contract）
//    调用本表 ⇒ 若本表在 .cc 里定义，任何「直接编译 include/ 的独立探针」都会链接失败
//    （实证：`scripts/test/cep_*_probe.cc` 族）。⇒ 表用 function-local static，跨 TU 唯一。
//
// 准入条件（ADR-X.17 D-3 的「T 迁移完成」）：
//   (a) T 的全部参数均由 load_param 声明，或已登记为「已消费但不经 load_param」
//   (b) getParamRulesRegistry() 中无 T 的条目
//   (c) CFG-08 的 T 分支已删除
#ifndef CORE_PARAM_ADMISSION_HH
#define CORE_PARAM_ADMISSION_HH

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cpptlm {

namespace detail {

/// 准入类型集合（function-local static ⇒ inline 函数内跨 TU 唯一）
inline std::set<std::string, std::less<>>& param_admission_table() {
    static std::set<std::string, std::less<>> table = {
        // 已迁移：模块内 load_param 声明 + 无 ParamRules 条目 + 无 CFG-08 分支
        "RouterTLM",
    };
    return table;
}

} // namespace detail

/// 该模块类型是否已准入（⇒ 启用未知键检查 + 无 params 也加载）
inline bool is_param_admitted(std::string_view type) {
    const auto& t = detail::param_admission_table();
    return t.find(type) != t.end();
}

/// 已准入类型的完整清单（升序）—— 供仓库级对拍从本表派生，避免第二份真源
inline std::vector<std::string> admitted_param_types() {
    const auto& t = detail::param_admission_table();
    return std::vector<std::string>(t.begin(), t.end());
}

/// 注册一个准入类型。**生产代码不应调用**；仅测试夹具与迁移期新类型使用。幂等。
inline void register_param_admitted(std::string_view type) {
    detail::param_admission_table().emplace(type);
}

} // namespace cpptlm

#endif // CORE_PARAM_ADMISSION_HH
