// include/tlm/cep/ssp_loader.hh
// SSP 声明面的 loader —— **fail-closed**（per `ADR-CEP-24` 决策 2.3：只保「形态 + 引用闭环」，
// ⛔ **不承诺**语义：活性 / 无死锁 / 一致性 / 兼容性）。
//
// 校验深度（逐条 · 每条带**红因签名**）：
//   E1  schema 字面量 != cep-ssp-declaration/v1
//   E2  ssp.id 缺失 / 形态非法（^[a-z][a-z0-9_]*$）/ 重复
//   E3  states 空 / 重名
//   E4  events 空 / 重名
//   E5  transition 维度 != states × events
//   E6  transition 目标 ∉ states 且 != null（null = 空洞）
//   E7  事件的 anchor ∉ 14 核心锚 **且** 无 (flow ∈ 10 流 + architect_approval 非空)
//   E8  非空 flow ∉ canonical 10 流
//   E9  perm ∉ {none,read,unique,writeback}（4 值锁 · CEN-Ib11）
//   E10 basis_kind ∉ {primary,structural,analogical,derived}
//   E11 basis_kind / basis_pending 维度 != states × events
//   E12 basis_projection_* 维度 != states
//   E13 transition **全为空洞**（退化 SSP · ADR-CEP-24 §4 边界 E1）
//
// ⛔ 本头**不**含协议名（通用层 · 铁律 2）；协议名只允许出现在**数据**（`ssps.json`）。
//
// 作者：CppTLM Team / 日期：2026-11-26
#ifndef CPPTLM_TLM_CEP_SSP_LOADER_HH
#define CPPTLM_TLM_CEP_SSP_LOADER_HH

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/event_alphabet.hh"

namespace tlm::cep::ssp {

    inline constexpr const char* kSspSchemaLiteral = "cep-ssp-declaration/v1";

    /// canonical 10 流名（逐字同 `kCanonicalFlowNames`；本头不 include 引擎基类 ⇒ 由测试交叉核对）
    inline constexpr const char* kSspFlowNames[10] = {
        "request", "completion", "snoop", "snoop_resp", "writeback",
        "forward_data", "back_invalidate", "address_resolve", "barrier", "snoop_completion_ack",
    };

    /// perm 4 值锁（`CEN-Ib11`）
    inline constexpr const char* kSspPermNames[4] = {"none", "read", "unique", "writeback"};
    /// BasisKind 四值（同 `mesif_state_spec.hh` 的 `BasisKind`）
    inline constexpr const char* kSspBasisKindNames[4] = {"primary", "structural", "analogical", "derived"};

    /// 单条校验结果（`ok == false` ⇒ `code` 非空，**红因签名**）
    struct SspCheck {
        bool        ok;
        std::string code;   ///< 红因码（E1..E12）
        std::string detail; ///< 人读细节（含 ssp id / 索引）
    };

    namespace detail {

        inline bool in_list(const char* const* arr, std::size_t n, const std::string& v) {
            for (std::size_t i = 0; i < n; ++i)
                if (v == arr[i]) return true;
            return false;
        }

        inline bool id_ok(const std::string& s) {
            if (s.empty()) return false;
            if (!(s[0] >= 'a' && s[0] <= 'z')) return false;
            for (char c : s) {
                const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
                if (!ok) return false;
            }
            return true;
        }

        inline SspCheck fail(const char* code, std::string d) { return {false, code, std::move(d)}; }

    } // namespace detail

    /// 校验整个 SSP 文档（fail-closed）。`std::nullopt` 语义 = `ok == true`。
    inline SspCheck check_ssp_document(const nlohmann::json& doc) {
        // E1
        if (!doc.contains("schema") || !doc["schema"].is_string() ||
            doc["schema"].get<std::string>() != kSspSchemaLiteral) {
            return detail::fail("E1", "schema 字面量须为 cep-ssp-declaration/v1");
        }
        if (!doc.contains("ssps") || !doc["ssps"].is_array() || doc["ssps"].empty()) {
            return detail::fail("E2", "ssps 须为非空数组");
        }

        std::set<std::string> ids;
        for (const auto& ssp : doc["ssps"]) {
            const std::string sid = ssp.value("id", std::string{});
            // E2
            if (!detail::id_ok(sid)) return detail::fail("E2", "ssp.id 缺失或形态非法: '" + sid + "'");
            if (!ids.insert(sid).second) return detail::fail("E2", "ssp.id 重复: " + sid);

            // E3 states
            if (!ssp.contains("states") || !ssp["states"].is_array() || ssp["states"].empty())
                return detail::fail("E3", sid + ": states 须为非空数组");
            std::set<std::string> states;
            for (const auto& s : ssp["states"]) {
                if (!s.is_string()) return detail::fail("E3", sid + ": state 须为字符串");
                if (!states.insert(s.get<std::string>()).second)
                    return detail::fail("E3", sid + ": state 重名: " + s.get<std::string>());
            }
            const std::size_t ns = states.size();

            // E4 events
            if (!ssp.contains("events") || !ssp["events"].is_array() || ssp["events"].empty())
                return detail::fail("E4", sid + ": events 须为非空数组");
            std::set<std::string> evnames;
            const auto& evs = ssp["events"];
            for (std::size_t j = 0; j < evs.size(); ++j) {
                const auto& e = evs[j];
                const std::string en = e.value("name", std::string{});
                if (en.empty()) return detail::fail("E4", sid + ": event[" + std::to_string(j) + "] 缺 name");
                if (!evnames.insert(en).second) return detail::fail("E4", sid + ": event 重名: " + en);

                // E7 / E8：anchor 优先；无 anchor ⇒ 须 flow + architect_approval
                const std::string anchor = e.value("anchor", std::string{});
                const std::string flow   = e.value("flow", std::string{});
                if (!anchor.empty()) {
                    const AnchorInfo* info = find_core_anchor(anchor);
                    if (info == nullptr)
                        return detail::fail("E7", sid + ": event '" + en + "' 的 anchor 非核心锚: " + anchor);
                    // 若同时给了 flow ⇒ 须与锚的 flow 一致（锚有 flow 时）
                    if (!flow.empty() && info->flow != nullptr && flow != info->flow)
                        return detail::fail("E8", sid + ": event '" + en + "' 的 flow 与锚不符");
                } else {
                    if (flow.empty())
                        return detail::fail("E7", sid + ": event '" + en + "' 既无 anchor 也无 flow");
                    if (!detail::in_list(kSspFlowNames, 10, flow))
                        return detail::fail("E8", sid + ": event '" + en + "' 的 flow 非 10 流: " + flow);
                    const std::string appr = e.value("architect_approval", std::string{});
                    if (appr.empty())
                        return detail::fail("E7", sid + ": 私有事件 '" + en + "' 无等价锚 ⇒ 须 architect_approval");
                }
            }
            const std::size_t ne = evnames.size();

            // E5 / E6 transition
            if (!ssp.contains("transition") || !ssp["transition"].is_array() ||
                ssp["transition"].size() != ns)
                return detail::fail("E5", sid + ": transition 行数 != states");
            std::size_t non_hole = 0;
            for (std::size_t i = 0; i < ns; ++i) {
                const auto& row = ssp["transition"][i];
                if (!row.is_array() || row.size() != ne)
                    return detail::fail("E5", sid + ": transition[" + std::to_string(i) + "] 列数 != events");
                for (std::size_t j = 0; j < ne; ++j) {
                    const auto& cell = row[j];
                    if (cell.is_null()) continue; // 空洞
                    if (!cell.is_string()) return detail::fail("E6", sid + ": 转移目标须为 state 名或 null");
                    const std::string tgt = cell.get<std::string>();
                    if (states.count(tgt) == 0)
                        return detail::fail("E6", sid + ": 转移目标不是已声明 state: " + tgt);
                    ++non_hole;
                }
            }
            // E13 —— 退化 SSP：transition **全为空洞** ⇒ fail-closed
            //   per `ADR-CEP-24` §4 边界 E1「空 / 退化 SSP（全 0xFF 洞表）⇒ loader fail-closed；
            //   ⛔ 不得静默通过」。⛔ 局部空洞合法（协议常有不合法转移），**全洞**才是退化。
            if (non_hole == 0)
                return detail::fail("E13", sid + ": transition 全为空洞（退化 SSP ⇒ 无任何合法转移）");

            // E9 perm
            if (!ssp.contains("perm") || !ssp["perm"].is_array() || ssp["perm"].size() != ns)
                return detail::fail("E9", sid + ": perm 须为长度 == states 的数组");
            for (const auto& p : ssp["perm"]) {
                if (!p.is_string() || !detail::in_list(kSspPermNames, 4, p.get<std::string>()))
                    return detail::fail("E9", sid + ": perm 值越出 4 值锁");
            }

            // E10 / E11 basis_kind / basis_pending
            for (const char* key : {"basis_kind", "basis_pending"}) {
                if (!ssp.contains(key) || !ssp[key].is_array() || ssp[key].size() != ns)
                    return detail::fail("E11", sid + ": " + key + " 行数 != states");
                for (std::size_t i = 0; i < ns; ++i) {
                    const auto& row = ssp[key][i];
                    if (!row.is_array() || row.size() != ne)
                        return detail::fail("E11", sid + ": " + key + "[" + std::to_string(i) + "] 列数 != events");
                    if (std::string(key) == "basis_kind") {
                        for (const auto& k : row)
                            if (!k.is_string() || !detail::in_list(kSspBasisKindNames, 4, k.get<std::string>()))
                                return detail::fail("E10", sid + ": basis_kind 值非法");
                    } else {
                        for (const auto& b : row)
                            if (!b.is_boolean()) return detail::fail("E11", sid + ": basis_pending 须为 bool");
                    }
                }
            }

            // E12 basis_projection_*
            for (const char* key : {"basis_projection_kind", "basis_projection_pending"}) {
                if (!ssp.contains(key) || !ssp[key].is_array() || ssp[key].size() != ns)
                    return detail::fail("E12", sid + ": " + key + " 长度 != states");
                for (const auto& v : ssp[key]) {
                    if (std::string(key) == "basis_projection_kind") {
                        if (!v.is_string() || !detail::in_list(kSspBasisKindNames, 4, v.get<std::string>()))
                            return detail::fail("E10", sid + ": basis_projection_kind 值非法");
                    } else if (!v.is_boolean()) {
                        return detail::fail("E12", sid + ": basis_projection_pending 须为 bool");
                    }
                }
            }
        }
        return {true, "", ""};
    }

} // namespace tlm::cep::ssp

#endif // CPPTLM_TLM_CEP_SSP_LOADER_HH
