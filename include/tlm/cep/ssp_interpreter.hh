// include/tlm/cep/ssp_interpreter.hh
// SSP 声明面的 **functional-only 解释器**（批 2 `cep-ssp-batch2` · ⑦）
//
// 功能:把 SSP **声明表**跑成「状态迁移 + perm 投影」—— ⛔ **零时序**。
//
// ⚠️ **零时序**（per `ADR-CEP-24` 决策 2.3 / 改进 ⑦ 明写「不含 hop / 消息级延迟」）:
//   本头**不产出**任何时长 / 延迟 / hop 数 ⇒ 时序面属 S2 `ADR-CEP-25`（待起草）。
//   ⇒ 解释结果**只**支撑「形态 + 引用闭环 + 路径延迟常数」档
//     （per `ADR-CEP-20` 决策 6 的档位纪律）⇒ ⛔ **不得**被表述为
//     「协议语义已真实链路验证」。
//
// ⚠️ **洞命中**（运行期事件落声明表的 `0xFF` 空洞 / 空集 · per `ADR-CEP-24` 决策 6.1）:
//   ⇒ **显式**计数该次违例（⛔ 不静默跳过 · ⛔ 不 panic）+ 承载该次运行的实验
//     判「**不可判**」（⛔ 不得记为「一致」或「通过」）。
//
// ⚠️ **瞬态态零泄漏**（per `INV-CEP-05-IC1`）:本头**不产生**任何瞬态态 ——
//   解释器只读**声明面**（稳定态）⇒ 协议私有状态只住插件内部。
//
// ⚠️ 本头**不**含协议名（通用层 · 铁律 2）；协议名只允许出现在**数据**（`ssps.json`）。
//
// 作者: CppTLM Team / 日期: 2026-11-27
#ifndef CPPTLM_TLM_CEP_SSP_INTERPRETER_HH
#define CPPTLM_TLM_CEP_SSP_INTERPRETER_HH

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tlm::cep::ssp {

    /// 一步解释结果 —— ⛔ **零时序**:本结构**不承载**任何时长 / 延迟 / hop 数。
    struct SspStep {
        std::string source_state;  ///< 迁移起点（声明面里的 state 名）
        std::string target_state;  ///< 迁移终点；⛔ **空串 = 洞命中**（无合法目标）
        std::string perm;          ///< 目标状态的 **perm 投影**（4 值锁 · CEN-Ib11）
    };

    /// ⚠️ **编译期零时序锁**:若有人往 `SspStep` 加数值型（时长 / 延迟 / hop）字段
    ///   ⇒ 本断言立即失败 ⇒ 强制走 S2 `ADR-CEP-25` 的时序面重新设计。
    static_assert(sizeof(SspStep) == 3 * sizeof(std::string),
                  "⛔ SspStep 不得承载时长/延迟/hop 字段（零时序 · 时序面属 S2 ADR-CEP-25）");

    /// 一次解释的完整结果。
    struct SspInterpretation {
        bool ok = false;              ///< 形态层是否**可解释**（⛔ 洞命中**不**影响本项）
        std::string error;            ///< 形态错误说明（起始态 / 事件不存在 · SSP 未登记）
        std::vector<SspStep> steps;   ///< 逐步结果（⛔ 无时长字段）
        std::vector<std::uint8_t> masks;  ///< 逐步的目标掩码（0 = 单目标形态）
        std::size_t violation_count = 0;  ///< **洞命中次数**（⛔ 不静默跳过 · ⛔ 不 panic）
        bool judgeable = true;        ///< 洞命中 ⇒ false ⇒ 该次实验判「**不可判**」
    };

    /// functional-only 解释:SSP 声明表 → 状态迁移 + perm 投影。
    ///
    /// @param doc        SSP 声明面文档（`ssps.json` 解析结果）
    /// @param ssp_id     要解释的 SSP id（须在 `doc["ssps"]` 中已登记）
    /// @param start_state 起始状态名（须在 `states` 中已声明）
    /// @param event_seq  事件名序列（须逐个在 `events` 中已声明）
    ///
    /// ⛔ **零时序**（无时长 / 延迟 / hop）· ⛔ **不产生瞬态态**。
    /// ⚠️ 掩码形态（`{"target_mask": N}`）的**多目标** ⇒ 取**声明顺序首个**命中位
    ///   （⛔ 不做时序推断 · ⛔ 不承诺集合语义）；掩码为**空集** ⇒ 按**洞命中**处理。
    inline SspInterpretation interpret_ssp(const nlohmann::json& doc,
                                           const std::string& ssp_id,
                                           const std::string& start_state,
                                           const std::vector<std::string>& event_seq) {
        SspInterpretation res;

        // ── 定位 SSP(须**已登记**)────────────────────────────────────────────
        const nlohmann::json* ssp = nullptr;
        if (doc.is_object() && doc.contains("ssps") && doc["ssps"].is_array()) {
            for (const auto& s : doc["ssps"]) {
                if (s.is_object() && s.value("id", std::string{}) == ssp_id) {
                    ssp = &s;
                    break;
                }
            }
        }
        if (ssp == nullptr) {
            res.error = "ssp 未登记: " + ssp_id;
            return res;
        }

        // ── 读取声明面(稳定态)───────────────────────────────────────────────
        std::vector<std::string> states;
        std::vector<std::string> events;
        for (const auto& s : (*ssp)["states"]) states.push_back(s.get<std::string>());
        for (const auto& e : (*ssp)["events"]) events.push_back(e.value("name", std::string{}));
        const auto& transition = (*ssp)["transition"];
        const auto& perm = (*ssp)["perm"];

        // ── 起始状态必须已声明 --------------------------------------------───
        std::size_t cur = states.size();
        for (std::size_t i = 0; i < states.size(); ++i) {
            if (states[i] == start_state) {
                cur = i;
                break;
            }
        }
        if (cur == states.size()) {
            res.error = "起始状态未声明: " + start_state;
            return res;
        }

        res.ok = true;

        // ── 逐步解释(⛔ 零时序)────────────────────────────────------------──
        for (const auto& ev_name : event_seq) {
            std::size_t ei = events.size();
            for (std::size_t j = 0; j < events.size(); ++j) {
                if (events[j] == ev_name) {
                    ei = j;
                    break;
                }
            }
            if (ei == events.size()) {
                res.ok = false;
                res.error = "事件未声明: " + ev_name;
                return res;
            }

            const std::string src_name = states[cur];
            std::string tgt_name;
            std::uint8_t mask = 0;

            const auto& cell = transition[cur][ei];
            if (cell.is_string()) {
                // ① 单目标形态
                tgt_name = cell.get<std::string>();
            } else if (cell.is_object()) {
                // ② 掩码形态(目标为**集合**)—— 取声明顺序首个命中位
                mask = cell.value("target_mask", static_cast<std::uint8_t>(0));
                bool hit = false;
                for (std::size_t k = 0; k < states.size(); ++k) {
                    const std::uint8_t bit = static_cast<std::uint8_t>(1u << k);
                    if ((mask & bit) != 0) {
                        tgt_name = states[k];
                        hit = true;
                        break;
                    }
                }
                if (!hit) {
                    // 空集 ⇒ 规范明确的「该转移不存在」⇒ 按**洞命中**处理
                    tgt_name.clear();
                    mask = 0;
                }
            } else {
                // null（空洞）⇒ **洞命中**
                tgt_name.clear();
                mask = 0;
            }

            if (tgt_name.empty()) {
                // ── **洞命中** ⇒ 显式违例计数 + 该次实验判「不可判」──────────
                ++res.violation_count;
                res.judgeable = false;
                res.steps.push_back(SspStep{src_name, std::string{}, std::string{}});
                res.masks.push_back(0);
                continue;  // ⛔ **不静默跳过**:步照样产出(带空目标)· 源状态**保持**不变
            }

            // perm 投影 = **目标状态**的 perm(4 值锁)
            std::string perm_name;
            for (std::size_t k = 0; k < states.size(); ++k) {
                if (states[k] == tgt_name) {
                    perm_name = perm[k].get<std::string>();
                    break;
                }
            }
            res.steps.push_back(SspStep{src_name, tgt_name, perm_name});
            res.masks.push_back(mask);

            for (std::size_t k = 0; k < states.size(); ++k) {
                if (states[k] == tgt_name) {
                    cur = k;
                    break;
                }
            }
        }

        return res;
    }

}  // namespace tlm::cep::ssp

#endif // CPPTLM_TLM_CEP_SSP_INTERPRETER_HH
