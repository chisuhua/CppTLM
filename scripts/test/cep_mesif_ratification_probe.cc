// scripts/test/cep_mesif_ratification_probe.cc
//   —— MESIF 实现层**跨仓追认请求清单**的生成器 / 校验器
//
// 功能:从**机器可读载体**生成追认请求清单(逐条),并支持 `--check`(只比对不写)。
//       载体 = 唯一真源:
//         · `include/tlm/cep/sharer_tracker.hh` 的 `HierarchicalDirectory::kBehaviorDecisions[5]`
//           (跨仓债 #1)
//         · `include/tlm/cep/mesif_protocol_engine.hh` 的
//         `MesifProtocolEngine::kEngineDecisions[6]`
//           (跨仓债 #3)
//         · 跨仓债 #2(`atomic` 的 `consistency_intent` 协议侧定义)= **仅请求项**
//           (本仓**未**做该决策 ⇒ 无载体,只有问题)
//
// 为什么需要生成器(per 42 号先例):清单若手写,载体改了清单不会跟着改 ⇒
// 「清单与载体一致」会退化成纸面 ⇒ 由 `test_cep_mesif_ratification_request.cc` 逐条核对。
//
// 用法:cep_mesif_ratification_probe <repo_root> <output_path> [--check]
//   无 --check:写出清单;有 --check:只比对并以 rc=1 报「不一致」。
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/sharer_tracker.hh"

namespace {

    using TrackerDec = tlm::cep::HierarchicalDirectory::BehaviorDecision;
    using EngineDec = tlm::cep::MesifProtocolEngine::EngineDecision;

    struct Item {
        std::string debt;     ///< 跨仓债编号(#1 / #2 / #3)
        std::string id;       ///< 决策编号(B-1 … / E-1 … / A-1)
        std::string topic;    ///< 决策点
        std::string basis;    ///< 本仓依据
        std::string carrier;  ///< 机器可读载体指针
        std::string question; ///< 需要对方裁定的问题
    };

    std::vector<Item> collect() {
        std::vector<Item> out;
        for (std::size_t i = 0; i < tlm::cep::HierarchicalDirectory::kBehaviorDecisionCount; ++i) {
            const TrackerDec& d = tlm::cep::HierarchicalDirectory::kBehaviorDecisions[i];
            out.push_back(Item{"#1", d.id, d.topic, d.basis,
                               "include/tlm/cep/sharer_tracker.hh · "
                               "HierarchicalDirectory::kBehaviorDecisions",
                               "该行为语义在 ArchForge 侧无规约 ⇒ 请裁定:确认 / 纠正 / 补规约"});
        }
        for (std::size_t i = 0; i < tlm::cep::MesifProtocolEngine::kEngineDecisionCount; ++i) {
            const EngineDec& d = tlm::cep::MesifProtocolEngine::kEngineDecisions[i];
            out.push_back(Item{"#3", d.id, d.topic, d.basis,
                               "include/tlm/cep/mesif_protocol_engine.hh · "
                               "MesifProtocolEngine::kEngineDecisions",
                               "该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约"});
        }
        // 债 #2:`atomic` 的 `consistency_intent` 协议侧定义 —— 本仓**只做了最小可用路径**
        // (无独立字段,意图位走 CohIfBundle::atomic)⇒ 这是**纯请求**,本仓无载体。
        out.push_back(Item{"#2", "A-1",
                           "atomic 的 consistency_intent 在协议侧的精确定义(是否需要独立字段 / "
                           "取值域 / 与 perm 的正交关系)",
                           "本仓当前处置:最小可用路径 —— 意图位走 CohIfBundle::atomic(1-bit),"
                           "perm 与 intent 正交;不新增字段(per CEN-Ib11 的 4 值锁)",
                           "**无载体**(本仓未做该决策)⇒ 纯请求项",
                           "请给出 consistency_intent 的协议侧定义:字段必要性 / 取值域 / "
                           "与 perm 的正交约束 / atomic 类型的分类"});
        return out;
    }

    std::string render(const std::vector<Item>& items) {
        std::ostringstream ss;
        ss << "# MESIF 实现层跨仓追认请求清单(B-1~B-5 / E-1~E-6 / A-1)\n\n"
           << "> **来源**:CppTLM 仓队列 #9 `cpptlm-cep-mesif-adapter`(实现层,已归档 2026-10-30)\n"
           << "> **规范性载体**:`include/tlm/cep/sharer_tracker.hh`(B-1 ~ B-5)+ "
              "`include/tlm/cep/mesif_protocol_engine.hh`(E-1 ~ E-6)—— 二者是**唯一真源**\n"
           << "> **生成方式**:`scripts/test/generate_mesif_ratification_request.sh`"
           << "(入口)+ `scripts/test/cep_mesif_ratification_probe.cc`(生成器)—— "
              "本清单**由载体生成**\n"
           << "> ⇒ 载体变更 ⇒ 跑一次生成器;一致性由 `test_cep_mesif_ratification_request.cc` "
              "逐条核对\n"
           << "> **日期**:2026-10-30 · **跨仓结论载体**:待 ArchForge 侧出具(先例:`42 号` "
              "逐格追认结论表)\n"
           << "> **本仓处置**:3 项跨仓债的**本仓侧**部分已全部完成(`basis` 登记 + `pending` "
              "标记);\n"
           << "> 本清单是**请求侧**入口 ⇒ 对方出具结论后,本仓按结论翻转 `pending` 并回填 `ref`\n\n"
           << "---\n\n"
           << "## 0. 追认口径(per `42 号` 先例)\n\n"
           << "| 项 | 口径 |\n|---|---|\n"
           << "| **确认** | 该决策的依据在 ArchForge 侧**仍然成立**,且不违反对方既有架构约束 |\n"
           << "| **纠正** | 对方给出**不同取值** ⇒ 本仓按对方结论改载体(并更新 `basis`) |\n"
           << "| **补规约** | 该行为语义应进对方规约层 ⇒ 本仓 `pending` 保留,直到对方规约落地 |\n"
           << "| **未回执** | 对方未回执 ⇒ 本仓 `pending` **保持 true**,**不得**写成「已裁决」 "
              "|\n\n"
           << "---\n\n"
           << "## 1. 请求项(" << items.size()
           << " 条 = 债 #1 的 5 条 + 债 #3 的 6 条 + 债 #2 的 1 条)\n\n";

        for (const auto& it : items) {
            ss << "### " << it.id << " —— " << it.topic << "\n\n"
               << "| 字段 | 内容 |\n|---|---|\n"
               << "| **跨仓债** | " << it.debt << " |\n"
               << "| **决策点** | " << it.topic << " |\n"
               << "| **本仓当前取值** | " << it.topic << " |\n"
               << "| **本仓依据** | " << it.basis << " |\n"
               << "| **机器载体** | `" << it.carrier << "` |\n"
               << "| **待裁定** | " << it.question << " |\n"
               << "| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;"
               << "纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;"
               << "补规约 ⇒ `pending` 保持 true |\n\n";
        }

        ss << "---\n\n"
           << "## 2. 残余风险(不得写成「已消除」)\n\n"
           << "| # | 风险 | 状态 |\n|---|---|---|\n"
           << "| **A** | `HierarchicalDirectory` 行为语义**无真源** ⇒ 实现者被迫自推 | "
              "🟡 中 —— 5 条已登记待追认(B-1 ~ B-5);`pending` 全 true |\n"
           << "| **C** | `atomic.supported = true` 与实现可能脱节(`consistency_intent` 无独立字段) "
              "| "
              "🟡 中 —— 已做最小可用路径 + 显式登记;协议侧定义待对方给出(A-1) |\n"
           << "| **E** | 引擎侧建模决策 E-1 ~ E-6 为本仓自推 | "
              "🟡 中 —— 6 条已登记待追认;`pending` 全 true |\n\n"
           << "> **口径**:本仓**不**把「已登记」写成「已裁决」;未回执 ⇒ `pending` 保持 true。\n\n"
           << "---\n\n"
           << "## 3. 维护记录\n\n"
           << "| 日期 | 变更 |\n|---|---|\n"
           << "| 2026-10-30 | 初版:3 项跨仓债的 12 条请求项(B-1 ~ B-5 / E-1 ~ E-6 / A-1);"
           << "**由载体生成** |\n";
        return ss.str();
    }

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: %s <repo_root> <output_path> [--check]\n", argv[0]);
        return 2;
    }
    const std::string out_path = argv[2];
    const bool check_only = (argc >= 4 && std::string(argv[3]) == "--check");

    const auto items = collect();
    if (items.empty()) {
        std::printf("FAIL: 载体为空(决策表未被读入)⇒ 清单会空转\n");
        return 1;
    }
    const std::string text = render(items);

    if (check_only) {
        std::ifstream in(out_path, std::ios::binary);
        if (!in) {
            std::printf("FAIL: 清单不存在:%s\n", out_path.c_str());
            return 1;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        if (ss.str() != text) {
            std::printf("FAIL: 清单与载体不一致(载体变更后须重跑生成器):%s\n", out_path.c_str());
            return 1;
        }
        std::printf("PASS: 清单与载体一致(%zu 条请求项)\n", items.size());
        return 0;
    }

    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::printf("FAIL: 无法写出:%s\n", out_path.c_str());
        return 1;
    }
    out << text;
    std::printf("WROTE: %s(%zu 条请求项)\n", out_path.c_str(), items.size());
    return 0;
}
