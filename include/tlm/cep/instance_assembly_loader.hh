// include/tlm/cep/instance_assembly_loader.hh
// **实例级装配闸的输入加载器** —— 供**装配路径臂**与**离线 CLI 臂**共用(禁双实现)
//
// 存在理由(per 台账 Entry #379 的 D-A2 / architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md`):
//   IAG 判据(`tlm::cep::run_instance_assembly_gate`)需要**三份输入** —— 拓扑 config +
//   `protocol_instances.json`(声明层)+ `collective_planes.json`(平面层)。
//   装配路径臂(`src/core/module_factory.cc`)与离线 CLI(`examples/cep_instance_assembly_gate.cc`)
//   必须喂**同一份**输入 ⇒ 加载规则也必须**同一份** ⇒ 本文件是加载规则的**唯一实现**。
//   ⛔ 两处均**不得**各自实现「读 config → 定位声明源 → 读文件」这套逻辑。
//
// 路径解析口径(与 `tlm::cep::parse_channel_declaration_from_config` **逐条对齐**):
//   · `config["cep"]["protocol_instances"]` 是**字符串** ⇒ **显式**声明源(相对路径按
//     **当前工作目录**解析)⇒ 不可读 / JSON 畸形 / 解析出 0 实例 ⇒ **source_error**
//     (⇒ 作者写错了 ⇒ 判红,不是外部条件);
//   · 是**对象 / 数组** ⇒ 内联声明 ⇒ 直接用;
//   · **缺失** ⇒ 落到本仓默认声明 `tlm::cep::kDefaultDeclarationPath`(相对 CWD)
//     ⇒ 不可读 ⇒ **SKIP**(跨工作目录 = 外部条件,不阻断装配)。
//
// 平面层(`collective_planes.json`)的缺失**不阻断**:它只影响 `[IAG-04]`/`[IAG-05]`
//   (plane 绑定层)⇒ 不可读时用**空文档** ⇒ 04/05 自然不判 ⇒ 其余 01/02/03/06/07 照判。
//   ⇒ 与「声明层缺失 = SKIP」的处置**故意不同**,原因见上。
#ifndef CPPT_LM_CEP_INSTANCE_ASSEMBLY_LOADER_HH
#define CPPT_LM_CEP_INSTANCE_ASSEMBLY_LOADER_HH

#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "tlm/cep/channel_dependency_graph.hh"

namespace tlm::cep {

/// 平面层声明源缺省路径(相对仓库根;与 CLI 共用同一常量,避免两处各写一份)
inline constexpr const char* kDefaultCollectivePlanesPath = "include/tlm/cep/collective_planes.json";

/// 装配闸的输入三元组 + 加载诊断。
struct InstanceAssemblyInputs {
    nlohmann::json instances_doc;  ///< 声明层(protocol_instances.json 或内联)
    nlohmann::json planes_doc;     ///< 平面层(collective_planes.json;不可读 ⇒ 空文档)
    bool source_error = false;     ///< 显式声明源写错(不可读 / 畸形 / 0 实例)⇒ 判红
    std::string source_error_detail;
    bool skip = false;             ///< 外部条件(默认声明源不可读)⇒ 不阻断
    std::string skip_reason;
    bool planes_unavailable = false;  ///< 平面层不可读(⛔ 不阻断,只影响 04/05)
    std::string planes_unavailable_detail;
};

namespace detail {

/// 读一个 JSON 文件;失败返回 false 并给出原因。
inline bool read_json_file(const std::string& path, nlohmann::json& out, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "文件不可读: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    try {
        out = nlohmann::json::parse(ss.str());
    } catch (const std::exception& exc) {
        err = "JSON 畸形: " + path + " (" + exc.what() + ')';
        return false;
    }
    return true;
}

}  // namespace detail

/// 按 config 定位并加载装配闸的三份输入(**唯一实现**)。
inline InstanceAssemblyInputs load_instance_assembly_inputs(const nlohmann::json& config) {
    InstanceAssemblyInputs in;

    // ── 声明层 ────────────────────────────────────────────────────────────
    if (!config.is_object() || !config.contains("cep") || !config.at("cep").is_object()) {
        // 无 `cep` 段 ⇒ 本 config 不涉及 CEP ⇒ 交由判据本身判 SKIP。
        return in;
    }
    const auto& cep = config.at("cep");
    if (!cep.contains("protocol_instances")) {
        // 缺省声明源(相对 CWD)—— 不可读 = 外部条件 ⇒ SKIP。
        std::string err;
        if (!detail::read_json_file(kDefaultDeclarationPath, in.instances_doc, err)) {
            in.skip = true;
            in.skip_reason = "缺省声明源不可读(" + err + ")⇒ 跨工作目录的外部条件,不阻断";
            return in;
        }
    } else {
        const auto& pi = cep.at("protocol_instances");
        if (pi.is_string()) {
            std::string err;
            if (!detail::read_json_file(pi.get<std::string>(), in.instances_doc, err)) {
                in.source_error = true;
                in.source_error_detail = "显式指定的声明源" + err;
                return in;
            }
            // 显式给了声明源却解析出 0 个实例 ⇒ 作者写错 ⇒ 判红(对齐既有口径)。
            const bool has_instances = in.instances_doc.is_object() &&
                                       in.instances_doc.contains("instances") &&
                                       in.instances_doc.at("instances").is_array() &&
                                       !in.instances_doc.at("instances").empty();
            if (!has_instances) {
                in.source_error = true;
                in.source_error_detail =
                    "显式指定的声明源解析出 0 个协议实例: " + pi.get<std::string>();
                return in;
            }
        } else if (pi.is_object() || pi.is_array()) {
            in.instances_doc = pi;  // 内联声明
        } else {
            in.source_error = true;
            in.source_error_detail = "config 的 `cep.protocol_instances` 类型非法(实际为 " +
                                    std::string(pi.type_name()) + ')';
            return in;
        }
    }

    // ── 平面层(不可读 ⇒ 空文档,不阻断)────────────────────────────────────
    std::string err;
    if (!detail::read_json_file(kDefaultCollectivePlanesPath, in.planes_doc, err)) {
        in.planes_unavailable = true;
        in.planes_unavailable_detail = err + "(⛔ 只影响 [IAG-04]/[IAG-05],其余判据照判)";
    }
    return in;
}

}  // namespace tlm::cep

#endif  // CPPT_LM_CEP_INSTANCE_ASSEMBLY_LOADER_HH
