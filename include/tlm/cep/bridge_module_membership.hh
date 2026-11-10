/**
 * @file bridge_module_membership.hh
 * @brief CEP 桥模块归属判据 —— **唯一实现**(装配路径 + 装配闸共用)
 *
 * ## 功能
 * 判定「某模块名是否出现在**多个** `cep.bridges[]` entry 的 `modules` 展开序列中」。
 * 命中 ⇒ 该模块的桥挂载槽(`bridge_` 单槽)会被**后挂的桥静默覆盖**
 * ⇒ 判红(`[BSS-01]` 归因 tag)。
 *
 * ## 为什么存在(唯一实现的理由)
 * - `include/framework/` 与 `src/core/` **不在** M1 冻结集,但 `src/core/module_factory.cc`
 *   是**红线面** ⇒ 重挂检测若在装配路径落地,必须与契约侧闸
 *   (`include/tlm/cep/instance_assembly_gate.hh` 的 `[BSS-01]`)是**同一份实现**,
 *   否则两侧必然语义漂移。
 * - `src/core/module_factory.cc:173` 已立纪律:「两个挂点(此处 + `validate_topology`
 *   跑的 CLI)调用**同一组**判据函数 —— **禁双实现**」。
 * - ⛔ 因此本判据**独立成头**(`tlm::cep` 公开命名空间),而不是塞进
 *   `instance_assembly_gate.hh` 的 `detail` 命名空间 ——
 *   ① `detail` 语义是「本头内部细节」,而本判据已是**跨模块共享契约**;
 *   ② `instance_assembly_gate.hh` 会拖入 `collective_plane.hh`(826 行)与
 *   `protocol_family.hh`,而框架 core 只需本判据 ⇒ 独立小头可避免依赖面扩大。
 *
 * ## 判定口径(与 `[BSS-01]` 现文本**逐字一致**)
 * 统计**全部** entry 的 `modules` 展开序列中每个模块名的出现次数;
 * 次数 > 1 ⇒ 命中。
 * ⛔ **无 entry 内去重** ⇒ **两个子情形都命中**:
 *   ① **跨 entry 重叠**:模块出现在 ≥2 条 entry 的 `modules` 中;
 *   ② **单 entry 内重复**:同一 entry 的 `modules` 列出同一模块两次(如 `["w","w"]`)。
 * ⇒ 口径既不得收窄也不得放宽(per 台账 Entry #319 ②)。
 *
 * ## 判据出处
 * - 契约侧:`docs/architecture/07-coherence-exploration-platform.md`
 *   **SU · 十七**(2026-11-09,per 台账 **Entry #319**);
 *   原「只测不拦 / 登记为 backlog」的表述在 **SU · 十五 ②④**。
 * - 归因 tag:`[BSS-01]`(main spec 载体 = `openspec/specs/cep-instance-assembly/spec.md`
 *   的 delta,per `cpptlm-cep-bridge-reattach-guard`)。
 * - ⛔ `.cc` 侧**只引用本 helper**(不引 ADR / 不写 `§` 节号),per
 *   `ADR-CEP-00A` 锚点纪律 + Entry #319 ④ N-3。
 *
 * @author CppTLM Team
 * @date 2026-11-09
 * @see include/tlm/cep/instance_assembly_gate.hh
 * @see src/core/module_factory.cc
 */

#ifndef TLM_CEP_BRIDGE_MODULE_MEMBERSHIP_HH
#define TLM_CEP_BRIDGE_MODULE_MEMBERSHIP_HH

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace tlm {
namespace cep {

/// 统计 `bridges` 全部 entry 的 `modules` 展开序列中每个模块名的出现次数
///
/// @param bridges `cep.bridges` 数组(⛔ 非整个 config —— 与调用侧取数对象同形)
/// @return 模块名 → 出现次数(⛔ 无 entry 内去重 ⇒ 单 entry 内重复亦计数)
inline std::map<std::string, int> count_bridge_module_membership(
    const nlohmann::json& bridges) {
    std::map<std::string, int> membership;
    if (!bridges.is_array())
        return membership;
    for (const auto& bridge : bridges) {
        if (!bridge.is_object() || !bridge.contains("modules") ||
            !bridge.at("modules").is_array())
            continue;
        for (const auto& m : bridge.at("modules")) {
            if (m.is_string())
                ++membership[m.get<std::string>()];
        }
    }
    return membership;
}

/// 找出「出现在多个 bridge entry 的 modules 展开序列中」的模块
///
/// @param bridges `cep.bridges` 数组
/// @return 违规项列表,每项 = {模块名, 出现次数};按模块名升序(⇒ 诊断输出稳定)
inline std::vector<std::pair<std::string, int>> find_bridge_module_conflicts(
    const nlohmann::json& bridges) {
    std::vector<std::pair<std::string, int>> conflicts;
    for (const auto& [mod_name, n] : count_bridge_module_membership(bridges)) {
        if (n > 1)
            conflicts.emplace_back(mod_name, n);
    }
    return conflicts;
}

/// `[BSS-01]` 的**规范诊断文本**(与装配闸现文本**逐字一致** ⇒ 归因 / grep 不失效)
inline std::string bridge_module_conflict_message(const std::string& mod_name, int count) {
    return "[BSS-01] module in multiple bridge entries: " + mod_name + " (" +
           std::to_string(count) + ")";
}

}  // namespace cep
}  // namespace tlm

#endif  // TLM_CEP_BRIDGE_MODULE_MEMBERSHIP_HH
