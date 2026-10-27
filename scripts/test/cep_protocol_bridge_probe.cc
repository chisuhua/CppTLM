// scripts/test/cep_protocol_bridge_probe.cc
// ProtocolBridge 契约判据的**运行期探针** —— 反向验证的执行体
//
// 用途(per `cpptlm-cep-cohif-carrier` task 3.8 / 3.9 的反向 fixture):
//   把 `CohIfNoCProtocolBridge` 的实现**篡改**后,对应的契约判据**必须报红**。
//
// 用法:
//   `probe --poison <kind> <in 路径> <out 路径>` —— 生成毒化副本
//     kind ∈ {perm_to_category, swap_addr_data}
//   `probe --verify <kind>` —— 在**当前被 include 的**桥实现上跑对应判据
//     返回码:0 = 判据全绿 · 1 = 判据报红(反向验证期望) · 2 = 用法错误
//
// ⚠️ **为什么是运行期探针而不是编译期**:Phase 3 的判据是**运行期**语义
//    (round-trip / 字段映射 / 元数据)⇒ 篡改后必须**编得过**但**判据红**。
//    ⇒ 若篡改导致编译失败,判据就是「因错误的原因红」⇒ 反向验证无效。
#include "tlm/cep/cohif_noc_bridge.hh"

#include "cep_protocol_bridge_contract.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

cpptlm::cep::BridgePoison parse_poison(const std::string& name) {
    using P = cpptlm::cep::BridgePoison;
    if (name == "perm_to_category")
        return P::PermToCategory;
    if (name == "swap_addr_data")
        return P::SwapAddrData;
    throw std::runtime_error("unknown poison kind: " + name);
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bundles::CohIfBundle rich_bundle() {
    bundles::CohIfBundle b;
    b.flow_id = bundles::CohIfFlowId::SnoopResp;
    b.transaction_id.write(0x0123456789ABCDEFULL);
    b.address.write(0xFEDCBA9876543210ULL);
    b.data.write(0x0F1E2D3C4B5A6978ULL);
    b.size.write(64);
    b.perm.write(bundles::CohIfBundle::PERM_WRITEBACK);
    b.snoop_type.write(bundles::CohIfBundle::SNOOP_FORWARD);
    b.line_state.write(bundles::CohIfBundle::STATE_M);
    b.hit.write(1);
    b.dirty.write(1);
    b.error.write(1);
    b.atomic.write(1);
    return b;
}

void report(const char* label, const std::vector<std::string>& items) {
    std::cout << "  " << label << ":";
    for (const auto& s : items)
        std::cout << " " << s;
    std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: cep_protocol_bridge_probe --poison <kind> <in> <out>\n"
                  << "       cep_protocol_bridge_probe --verify <kind>\n";
        return 2;
    }
    const std::string mode = argv[1];

    if (mode == "--poison") {
        if (argc < 5) {
            std::cerr << "usage: --poison <kind> <in> <out>\n";
            return 2;
        }
        try {
            const auto kind = parse_poison(argv[2]);
            const auto poisoned = cpptlm::cep::poison_cohif_bridge(slurp(argv[3]), kind);
            std::ofstream out(argv[4], std::ios::binary | std::ios::trunc);
            if (!out) {
                std::cerr << "cannot write " << argv[4] << "\n";
                return 2;
            }
            out << poisoned;
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "poison generation failed: " << e.what() << "\n";
            return 2;
        }
    }

    if (mode == "--verify") {
        if (argc < 3) {
            std::cerr << "usage: --verify <kind>\n";
            return 2;
        }
        const std::string kind_name = argv[2];
        tlm::cep::CohIfNoCProtocolBridge bridge;
        const auto base = rich_bundle();

        if (kind_name == "perm_to_category") {
            // 篡改点:`perm` 被写进 `flit_category` ⇒ 字段映射探针必须报红
            const auto probe = cpptlm::cep::check_field_mapping(bridge, base);
            report("泄漏字段", probe.leaked);
            report("未映射字段", probe.not_mapped);
            if (!probe.leaked.empty())
                return 1;  // 判红(反向验证期望)
            std::cerr << "  ✗ 篡改未生效:声称丢弃的 `perm` 竟未泄漏\n";
            return 0;
        }
        if (kind_name == "swap_addr_data") {
            // 篡改点:`address` 与 `data` 对调 ⇒ 内容层 round-trip 必须报红
            const auto rt = cpptlm::cep::check_content_round_trip(bridge, base);
            report("共有字段不相等", rt.mismatched);
            if (!rt.mismatched.empty())
                return 1;
            std::cerr << "  ✗ 篡改未生效:共有字段 round-trip 竟仍然恒等\n";
            return 0;
        }
        std::cerr << "unknown verify kind: " << kind_name << "\n";
        return 2;
    }

    std::cerr << "unknown mode: " << mode << "\n";
    return 2;
}
