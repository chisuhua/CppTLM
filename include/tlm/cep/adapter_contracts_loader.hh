// include/tlm/cep/adapter_contracts_loader.hh
// Adapter Contracts Loader —— 把 JSON 声明源装成 `AdapterSchemaContracts::Inputs`
//
// 功能:为 `adapter_schema_contracts.hh` 的 5 条判据提供**唯一一份**的输入装载逻辑,
//       供 **Catch2 测试**与**离线 CLI**(`examples/cep_adapter_contracts.cc`)共用 ——
//       两端必须喂给判据**完全相同**的输入,否则「CLI 与测试的 verdict 逐条相同」
//       失去意义(禁双实现,per 队列 #5 `Entry #68` 的同源纪律)。
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 两层结构(per `ADR-CEP-12 §11` 的实例级权威分区 + 队列 #8 的机制扩孔)──
// 本装载层拆成**两层**,**判据本体 `adapter_schema_contracts.hh` 零改动**:
//
//   · **全局输入**(装载一次,AC-01 / AC-02 / AC-03):`perm` 值域 · 实例声明集 ·
//     静默升级声明源 · 无升级能力实例集。
//     ⇒ 实例声明集**同时**包含**镜像域实例**(ArchForge 声明)与**本仓域实例**
//       (本仓权威,登记在 `protocol_instances.json` 的顶层
//       `repo_owned_instance_ids[]`)—— AC-02 的存在性判据对**两个域**同时生效。
//
//   · **逐 adapter 输入**(每个 adapter 一份,AC-04 / AC-05):消息类映射行 ·
//     使用中消息类 · 恒等标志/理由 · 稳定态投影 · 升级转换目标态 · 权限秩。
//     ⇒ 以**模板参数分派**供给(编译期类型,非运行期字符串分发)——
//       adapter 是 header-only 常量 ⇒ 零开销 + 编译期发现缺符号。
//
// ── 为什么带 JSON 依赖 ────────────────────────────────────────────────
// 本头文件是**装载层**(读文件 + 转 POD),判据逻辑在 `adapter_schema_contracts.hh`。
// 两者分离:判据实现**不**依赖 JSON(可被其它前端复用),装载层**只有一份**。
// `nlohmann/json.hpp` 由根 `CMakeLists.txt` 的 `external/json` 提供,且随安装面
// 一并安装(`install(DIRECTORY external/json/ ...)`)⇒ 依赖可满足。

#ifndef CPPTLM_TLM_CEP_ADAPTER_CONTRACTS_LOADER_HH
#define CPPTLM_TLM_CEP_ADAPTER_CONTRACTS_LOADER_HH

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/adapter_schema_contracts.hh"
#include "tlm/cep/chi_rnf_adapter.hh"
#include "tlm/cep/moesi_gpu_adapter.hh"

namespace tlm::cep {

/// `Perm` 枚举 → 文本(**模板化**:各 adapter 的 `Perm` 是**不同类型**,
/// 枚举值序按 `ADR-CEP-02` CEN-Ib11 的 4 值锁统一 ⇒ 文本映射共用一份)
template <typename AdapterT>
inline const char* perm_name(typename AdapterT::Perm p) {
    switch (p) {
        case AdapterT::Perm::NONE:     return "NONE";
        case AdapterT::Perm::READ:     return "READ";
        case AdapterT::Perm::UNIQUE:   return "UNIQUE";
        case AdapterT::Perm::WRITEBACK: return "WRITEBACK";
    }
    return "NONE";
}

// ══════════════════════════════════════════════════════════════════════
//  逐 adapter 输入(AC-04 / AC-05)—— 模板参数分派
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **判据本体零改动**:本函数**只**产出 `Inputs` 的 POD;判据逻辑仍在
//    `adapter_schema_contracts.hh` **一份**(禁双实现,per 队列 #6 的同源纪律)。
//
// ⚠️ **AC-05 的字段名沿用 moesi 口径**(`e_write_target_state` / `e_state_perm_rank`
//    / `m_state_perm_rank`)—— 其**语义**是「**某个独占干净态**上执行写事件后,
//    目标态的 `perm` 秩 SHALL 严格升高」。对 CHI:`UC`(UNIQUE,秩 2)+ `Wr` → `UD`
//    (WRITEBACK,秩 3)⇒ 与 moesi 的 `E`→`M` **同构** ⇒ 复用同一判据,零机制改动。
//    (AC-05 的 detail 文案会写「E+Write→M」—— 已知措辞错位,登记为缺口,
//     **不改**机制;per change design `D-η` 的「AC-05 无 SKIP 路径」条目。)
//
// ⚠️ **瞬态态的处理**:瞬态**不**进 `stable_state_has_projection` ——
//    AC-05 的 ② 子条件要求「瞬态**无**投影」(per `INV-CEP-05-IC1`)。
//    瞬态由 adapter 自行声明(`kTransientStates`)。
//
// @param in  待填充的 `Inputs`(逐 adapter 一份)
/// 瞬态态判定(adapter 声明的 `kTransientStates` 命中即瞬态)
template <typename AdapterT>
inline bool is_transient_state(std::size_t state_index) {
    for (std::size_t i = 0; i < AdapterT::kTransientStateCount; ++i) {
        if (static_cast<std::size_t>(AdapterT::kTransientStates[i]) == state_index) return true;
    }
    return false;
}

template <typename AdapterT>
inline void fill_adapter_inputs(AdapterSchemaContracts::Inputs& in) {
    using A = AdapterT;

    // ── AC-04:消息类映射 ────────────────────────────────────────────
    // ⚠️ **先清空**:本函数是**追加**语义 ⇒ 若调用方复用了已填过映射的 `Inputs`
    //    (如「全局输入 + 逐 adapter 叠加」),不清理会得到**重复行** ⇒ AC-04 报
    //    「映射行数 = 2(应为恰好 1)」⇒ 清理让调用方**无需**手动重置。
    in.mapping.rows.clear();
    in.mapping.used_message_classes.clear();
    for (std::size_t i = 0; i < A::kMessageClassCount; ++i) {
        const auto& row = A::kMessageClassMap[i];
        in.mapping.rows.emplace_back(std::string(row.message_class), row.category);
        in.mapping.used_message_classes.emplace_back(row.message_class);
    }
    in.mapping.is_identity     = A::kMappingIsIdentity;
    in.mapping.identity_reason = A::kIdentityReason;

    // ── AC-05:稳定态投影(瞬态**排除**)────────────────────────────────
    in.stable_state_has_projection.clear();
    for (std::size_t s = 0; s < A::kStateCount; ++s) {
        if (is_transient_state<A>(s)) continue;
        typename A::Perm p = A::Perm::NONE;
        const bool has = A::perm_of(static_cast<A::State>(s), p);
        in.stable_state_has_projection.emplace_back(static_cast<int>(s), has);
    }

    // ── AC-05:「独占干净态 + 写事件 → 目标态」的三元组 ────────────────
    in.e_write_target_state =
        static_cast<int>(A::transition(A::kUniqueCleanState, A::kWriteEvent));
    typename A::Perm pc = A::Perm::NONE;
    typename A::Perm pd = A::Perm::NONE;
    const bool hc = A::perm_of(A::kUniqueCleanState, pc);
    const bool hd = A::perm_of(A::kUniqueDirtyState, pd);
    in.e_state_perm_rank = hc ? perm_rank(perm_name<A>(pc)) : -1;
    in.m_state_perm_rank = hd ? perm_rank(perm_name<A>(pd)) : -1;
}


/// 装载结果
struct LoadedAdapterContracts {
    AdapterSchemaContracts::Inputs inputs;
    bool        ok    = false;
    std::string error;
};


namespace detail {

inline bool read_json_file(const std::string& path, nlohmann::json& out, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "文件不可读:" + path;
        return false;
    }
    try {
        out = nlohmann::json::parse(in);
    } catch (const std::exception& exc) {
        err = std::string("JSON 解析失败:") + path + ":" + exc.what();
        return false;
    }
    return true;
}

}  // namespace detail

/// 从仓根目录装载全部判据输入
///
/// @param repo_root 仓根目录(其下应有 `include/tlm/cep/*.json`)
inline LoadedAdapterContracts load_adapter_contract_inputs(const std::string& repo_root) {
    using json = nlohmann::json;
    LoadedAdapterContracts out;

    const std::string cep_dir = repo_root + "/include/tlm/cep";
    json perm_schema, instances_doc, upgrades_doc;
    std::string err;

    if (!detail::read_json_file(cep_dir + "/protocol_instance.schema.json", perm_schema, err) ||
        !detail::read_json_file(cep_dir + "/protocol_instances.json", instances_doc, err) ||
        !detail::read_json_file(cep_dir + "/perm_upgrades.json", upgrades_doc, err)) {
        out.error = err;
        return out;
    }

    // ── ① `perm` 值域(枚举顺序 = 权限秩序)─────────────────────────────
    try {
        for (const auto& v : perm_schema.at("properties").at("perm").at("items").at("enum")) {
            out.inputs.perm_domain.values.push_back(v.get<std::string>());
        }
    } catch (const std::exception& exc) {
        out.inputs.perm_domain.source_error       = true;
        out.inputs.perm_domain.source_error_detail = std::string("perm 值域读取失败:") + exc.what();
    }

    // ── ② 实例声明 ─────────────────────────────────────────────────────
    try {
        for (const auto& inst : instances_doc.at("instances")) {
            ProtocolInstanceDecl d;
            d.id = inst.at("id").get<std::string>();
            for (const auto& p : inst.at("perm")) {
                d.perm.push_back(p.get<std::string>());
            }
            out.inputs.instances.instances.push_back(d);
        }
    } catch (const std::exception& exc) {
        out.inputs.instances.source_error       = true;
        out.inputs.instances.source_error_detail = std::string("实例声明读取失败:") + exc.what();
    }

    // ── ③ 静默升级声明源 ───────────────────────────────────────────────
    try {
        const auto& pu = upgrades_doc.at("perm_upgrade").at("silent_upgrades");
        for (const auto& u : pu) {
            SilentUpgradeDecl d;
            d.instance_id = u.at("instance_id").get<std::string>();
            d.from        = u.at("from").get<std::string>();
            d.to          = u.at("to").get<std::string>();
            out.inputs.upgrades.silent_upgrades.push_back(d);
        }
    } catch (const std::exception& exc) {
        out.inputs.upgrades.source_error       = true;
        out.inputs.upgrades.source_error_detail = std::string("静默升级声明读取失败:") + exc.what();
    }

    // ── ④ 无静默升级能力的实例(声明源里**不得**出现它们)────────────────
    {
        std::set<std::string> declared;
        for (const auto& u : out.inputs.upgrades.silent_upgrades) {
            declared.insert(u.instance_id);
        }
        for (const auto& inst : out.inputs.instances.instances) {
            if (declared.find(inst.id) == declared.end()) {
                out.inputs.no_upgrade_instances.push_back(inst.id);
            }
        }
    }

    // ── ⑤ / ⑥ 逐 adapter 输入(AC-04 / AC-05)────────────────────────────
    //    ⇒ 移交 `fill_adapter_inputs<AdapterT>()`(文件末尾)——
    //      **全局输入**与**逐 adapter 输入**分离,同一 adapter 可有多份逐 adapter 输入
    //      (每个 adapter 一份 `Inputs` 副本)。
    fill_adapter_inputs<MoesiGpuAdapter>(out.inputs);

    out.ok = true;
    return out;
}

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_ADAPTER_CONTRACTS_LOADER_HH
