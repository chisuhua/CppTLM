// include/tlm/cep/adapter_contracts_loader.hh
// Adapter Contracts Loader —— 把 JSON 声明源装成 `AdapterSchemaContracts::Inputs`
//
// 功能:为 `adapter_schema_contracts.hh` 的 5 条判据提供**唯一一份**的输入装载逻辑,
//       供 **Catch2 测试**与**离线 CLI**(`examples/cep_adapter_contracts.cc`)共用 ——
//       两端必须喂给判据**完全相同**的输入,否则「CLI 与测试的 verdict 逐条相同」
//       失去意义(禁双实现,per 队列 #5 `Entry #68` 的同源纪律)。
// 作者: CppTLM Team / 日期: 2026-10-27
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
#include "tlm/cep/moesi_gpu_adapter.hh"

namespace tlm::cep {

/// 装载结果
struct LoadedAdapterContracts {
    AdapterSchemaContracts::Inputs inputs;
    bool        ok    = false;
    std::string error;
};

/// `Perm` 枚举 → 文本(契约侧权限秩序的键)
inline const char* perm_name(MoesiGpuAdapter::Perm p) {
    switch (p) {
        case MoesiGpuAdapter::Perm::NONE:     return "NONE";
        case MoesiGpuAdapter::Perm::READ:     return "READ";
        case MoesiGpuAdapter::Perm::UNIQUE:   return "UNIQUE";
        case MoesiGpuAdapter::Perm::WRITEBACK: return "WRITEBACK";
    }
    return "NONE";
}

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

    // ── ⑤ 消息类映射(逐 adapter 提供)───────────────────────────────────
    {
        for (std::size_t i = 0; i < MoesiGpuAdapter::kMessageClassCount; ++i) {
            const auto& row = MoesiGpuAdapter::kMessageClassMap[i];
            out.inputs.mapping.rows.emplace_back(std::string(row.message_class), row.category);
            out.inputs.mapping.used_message_classes.emplace_back(row.message_class);
        }
        out.inputs.mapping.is_identity    = MoesiGpuAdapter::kMappingIsIdentity;
        out.inputs.mapping.identity_reason = MoesiGpuAdapter::kIdentityReason;
    }

    // ── ⑥ 稳定态投影 + `E+Write→M` ────────────────────────────────────
    {
        using A = MoesiGpuAdapter;
        for (std::size_t s = 0; s < A::kStateCount; ++s) {
            A::Perm p = A::Perm::NONE;
            const bool has = A::perm_of(static_cast<A::State>(s), p);
            const bool stable = (s != static_cast<std::size_t>(A::State::T));
            if (stable) {
                out.inputs.stable_state_has_projection.emplace_back(static_cast<int>(s), has);
            }
        }
        out.inputs.e_write_target_state =
            static_cast<int>(A::transition(A::State::E, A::Event::Wr));
        A::Perm pe = A::Perm::NONE;
        A::Perm pm = A::Perm::NONE;
        const bool he = A::perm_of(A::State::E, pe);
        const bool hm = A::perm_of(A::State::M, pm);
        out.inputs.e_state_perm_rank = he ? perm_rank(perm_name(pe)) : -1;
        out.inputs.m_state_perm_rank = hm ? perm_rank(perm_name(pm)) : -1;
    }

    out.ok = true;
    return out;
}

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_ADAPTER_CONTRACTS_LOADER_HH
