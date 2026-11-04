// =============================================================================
// scripts/test/cep_ualink_inc_probe.cc
//   —— 队列 #14 **实现层** change `cpptlm-cep-ualink-inc` 的**跨载体探针**
//
// 作者: CppTLM Team / 日期: 2026-11-04
// 用法: g++ -std=c++17 -I <shadow-or-repo>/include -I external/json \
//            scripts/test/cep_ualink_inc_probe.cc -o <bin> && <bin> <root>
//
// 机制(照队列 #13 / #14 规约层先例):
//   · 驱动脚本把**被毒化的副本**放进影子目录 ⇒ 编译探针时 `-I $SHADOW/include`
//     **优先于** `-I include` ⇒ 探针吃到的是**毒化副本**,而仓内文件**零改动**;
//   · 探针的**文本扫描**与 **JSON 读取**都以影子根目录(argv[1])为基准。
//
// 失败输出格式:`PROBE:FAIL <归因 id> —— <说明>`(归因 id **精确相等**匹配,
//   ⛔ 不用子串前缀匹配 —— 否则 `CP-03` 会命中 `CP-03-VC-PRECONDITION`)。
// =============================================================================

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/collective_plane_contract.hh"
#include "tlm/cep/fabric_collective_plane.hh"
#include "tlm/cep/ualink_adapter.hh"

namespace {

    namespace cep = tlm::cep;

    std::string g_root = ".";

    int g_fail = 0;

    void expect(bool cond, const char* id, const std::string& what) {
        if (!cond) {
            std::printf("PROBE:FAIL %s —— %s\n", id, what.c_str());
            ++g_fail;
        }
    }

    std::string read_file(const std::string& rel) {
        std::ifstream in(g_root + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去注释(行 + 块)—— 禁项扫描只在**代码**上判定(注释里的说明文字不算)
    std::string strip_comments(const std::string& src) {
        std::string out;
        bool line = false;
        bool block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (!line && !block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                line = true;
                ++i;
                continue;
            }
            if (!line && !block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                block = true;
                ++i;
                continue;
            }
            if (line && src[i] == '\n') {
                line = false;
                out.push_back('\n');
                continue;
            }
            if (block && src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                block = false;
                i++;
                continue;
            }
            if (!line && !block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// **adapter 禁项清单**(per 本 change 的 `design §2.1` 禁项 ①)
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> v = {
            "Obligation::",  "BasisKind::",   "CertificateId::", "UntestedId::",
            "IncOp::",       "PendingId::",   "BasisCell",       "CertificateSpec",
            "UntestedItem",  "PendingItem",   "PrimitiveSet",    "IncOpBasis",
            "Ic1Discrimination", "kBasisObligations", "kBasisIncOps", "kPendingSemanticItems",
            "kCertificateSpecs",  "kUntestedItems",    "kIncOps",     "kIncOpPrimitives",
            "kUsedPrimitiveNames", "all_basis_valid",  "kSourceLicenseConfirmed"};
        return v;
    }

    bool json_load(const std::string& rel, nlohmann::json& out) {
        const std::string text = read_file(rel);
        if (text.empty())
            return false;
        try {
            out = nlohmann::json::parse(text);
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    /// 建一个「已注册 + 已 drain」的 region(数据路径可执行)
    bool prepare_active_region(cep::FabricCollectivePlane& plane, cep::group_id_t gid,
                               std::uint64_t base) {
        cep::AddrRange range;
        range.lo = base;
        range.hi = base + 0xFF;
        if (plane.group_register_region(gid, range, cep::AccessMode::Normal) != cep::Status::Ok)
            return false;
        return plane.complete_drain(gid, range) == cep::Status::Ok;
    }

    cep::CollectiveReq make_req(cep::group_id_t gid, cep::CollectiveOp op, std::uint32_t member,
                                 std::uint64_t value, std::uint64_t addr) {
        cep::CollectiveReq req;
        req.group = gid;
        req.op = op;
        req.tag = 7u;
        req.src_member = member;
        req.payload.addr = addr;
        req.payload.len = 8u;
        req.payload.inline_payload = true;
        req.payload.inline_value = value;
        req.reduce_op = cep::ReduceOp::Sum;
        req.dtype = cep::DataType::U64;
        req.multicast_mask = 0xFFFFFFFFFFFFFFFFull;
        return req;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc > 1)
        g_root = argv[1];

    // ═══════════════════════════════════════════════════════════════
    // UIA-01 —— adapter **零自有表副本**(禁项文本扫描)
    // ═══════════════════════════════════════════════════════════════
    {
        const std::string raw = read_file("include/tlm/cep/ualink_adapter.hh");
        expect(!raw.empty(), "UIA-01", "ualink_adapter.hh 不可读");
        const std::string code = strip_comments(raw);
        for (const auto& token : forbidden_tokens()) {
            expect(code.find(token) == std::string::npos, "UIA-01",
                   "adapter 文本含载体成员字面量 `" + token + "` ⇒ 零自有表副本判据红");
        }
        // **正控**:扫描器有牙 —— 规约载体**本身**含这些字面量
        const std::string carrier = strip_comments(read_file("include/tlm/cep/ualink_inc_state_spec.hh"));
        bool carrier_has = false;
        for (const auto& token : forbidden_tokens()) {
            if (carrier.find(token) != std::string::npos) {
                carrier_has = true;
                break;
            }
        }
        expect(carrier_has, "UIA-01", "**正控**失守:规约载体竟不含任何禁项字面量 ⇒ 扫描器无鉴别力");
    }

    // ═══════════════════════════════════════════════════════════════
    // UIA-02 —— adapter **无 `transition_*`**
    // ═══════════════════════════════════════════════════════════════
    {
        const std::string code = strip_comments(read_file("include/tlm/cep/ualink_adapter.hh"));
        expect(code.find("transition_") == std::string::npos, "UIA-02",
               "adapter 暴露 `transition_*` ⇒ INC 转移面被自造(转移语义仍 pending)");
    }

    // ═══════════════════════════════════════════════════════════════
    // UIA-03 —— `FabricCollectivePlane` 数据路径**真可达**
    // ═══════════════════════════════════════════════════════════════
    {
        cep::FabricCollectivePlane plane;
        cep::member_set_t members{1u, 2u};
        const cep::group_id_t gid = plane.group_create(members, cep::GroupAttrs{});
        expect(gid != cep::kInvalidGroupId, "UIA-03", "group_create 失败");
        expect(prepare_active_region(plane, gid, 0x1000u), "UIA-03", "region 注册 / drain 失败");

        plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 1u, 10u, 0x1000u));
        expect(plane.pending_event_count() == 0u, "UIA-03",
               "REDUCE **未收齐**却已产出事件 ⇒ 提前释放");
        plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 2u, 20u, 0x1000u));
        expect(plane.last_status() == cep::Status::Ok, "UIA-03", "REDUCE 收齐后状态非 Ok");
        expect(plane.pending_event_count() == 3u, "UIA-03",
               "REDUCE 收齐后 SHALL 产出 2 个 Result + 1 个 Completion");
        cep::CollectiveEvt evt;
        plane.recv(evt);
        expect(evt.kind() == cep::CollectiveEvt::Kind::Result, "UIA-03", "首个事件不是 Result");
        expect(evt.payload() == 30u, "UIA-03", "REDUCE(Sum) 结果不是 30");
        // **可区分性**:同样操作在 `LocalCollectivePlane` 上仍是「未实现」
        cep::LocalCollectivePlane local;
        const cep::group_id_t lgid = local.group_create(members, cep::GroupAttrs{});
        cep::AddrRange lrange;
        lrange.lo = 0x1000u;
        lrange.hi = 0x10FFu;
        local.group_register_region(lgid, lrange, cep::AccessMode::Normal);
        local.complete_drain(lgid, lrange);
        local.send(make_req(lgid, cep::CollectiveOp::REDUCE, 1u, 10u, 0x1000u));
        expect(local.last_status() == cep::Status::DataPathNotImplemented, "UIA-03",
               "`LocalCollectivePlane` 的数据路径状态变了 ⇒ 两实现**不可区分**");
    }

    // ═══════════════════════════════════════════════════════════════
    // UIA-04 —— **C-3 控制面与数据面互斥**
    // ═══════════════════════════════════════════════════════════════
    {
        cep::FabricCollectivePlane plane;
        cep::member_set_t members{1u, 2u};
        const cep::group_id_t gid = plane.group_create(members, cep::GroupAttrs{});
        expect(prepare_active_region(plane, gid, 0x2000u), "UIA-04", "region 注册 / drain 失败");
        plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 1u, 10u, 0x2000u));
        expect(plane.group_has_inflight(gid), "UIA-04", "单方贡献后 SHALL 处于 in-flight");
        // in-flight ⇒ 控制面操作被拒绝
        cep::AddrRange other;
        other.lo = 0x3000u;
        other.hi = 0x30FFu;
        expect(plane.group_register_region(gid, other, cep::AccessMode::Normal) ==
                   cep::Status::ControlPlaneConflict,
               "UIA-04", "in-flight 期间 `group_register_region` **未被拒绝**");
        plane.group_destroy(gid);
        expect(plane.last_status() == cep::Status::ControlPlaneConflict, "UIA-04",
               "in-flight 期间 `group_destroy` **未被拒绝**");
        expect(plane.group_alive(gid), "UIA-04", "in-flight 期间 group 被销毁 ⇒ 数据丢失");
        // 收齐 ⇒ 恢复接受控制面操作
        plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 2u, 20u, 0x2000u));
        expect(!plane.group_has_inflight(gid), "UIA-04", "收齐后仍处于 in-flight");
        expect(plane.group_register_region(gid, other, cep::AccessMode::Normal) == cep::Status::Ok,
               "UIA-04", "收齐后控制面操作**未恢复**");
    }

    // ═══════════════════════════════════════════════════════════════
    // UIA-05 —— 声明面 `status` 取值域 = **2 值**
    // ═══════════════════════════════════════════════════════════════
    nlohmann::json planes_doc;
    bool planes_ok = json_load("include/tlm/cep/collective_planes.json", planes_doc);
    cep::CollectivePlaneDeclaration decl;
    if (planes_ok)
        decl = cep::parse_collective_plane_declaration(planes_doc);
    expect(planes_ok, "UIA-05", "collective_planes.json 不可读 / 畸形");
    if (planes_ok) {
        const std::string& st = decl.vc_reservation_declaration.status;
        expect(st == "unavailable" || st == "shared_no_isolation", "UIA-05",
               "声明的 status = \"" + st + "\" 不在 2 值域内");
    }

    // ═══════════════════════════════════════════════════════════════
    // UIA-06 —— 证书作用域(`certificate_scope`)登记
    // ═══════════════════════════════════════════════════════════════
    if (planes_ok) {
        expect(!decl.vc_reservation_declaration.certificate_scope.empty(), "UIA-06",
               "`certificate_scope` 为空 ⇒ M-2 覆盖范围不可判定");
    }

    // ═══════════════════════════════════════════════════════════════
    // UIA-07 —— `ualink_4` **不**声明
    // ═══════════════════════════════════════════════════════════════
    {
        nlohmann::json inst_doc;
        const bool ok = json_load("include/tlm/cep/protocol_instances.json", inst_doc);
        expect(ok, "UIA-07", "protocol_instances.json 不可读 / 畸形");
        if (ok) {
            std::size_t count = 0;
            bool has_ualink = false;
            for (const auto& inst : inst_doc["instances"]) {
                ++count;
                if (inst.contains("id") && inst["id"].is_string() &&
                    inst["id"].get<std::string>() == "ualink_4")
                    has_ualink = true;
            }
            expect(!has_ualink, "UIA-07",
                   "`ualink_4` 被声明 ⇒ 取值不可判定却已落盘(推理填空)");
            expect(count == 7u, "UIA-07", "协议实例数不是 7(声明面被静默改动)");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // V6-d —— 四条证书(C-1′ / C-2 / C-3 / C-4)
    // ═══════════════════════════════════════════════════════════════
    if (planes_ok) {
        const cep::VcBindingSnapshot snap = cep::load_vc_binding_snapshot(
            g_root + "/include/tlm/cep/protocol_instances.json",
            g_root + "/include/tlm/cep/fabric.json");
        expect(!snap.source_error, "CP-09-IC1",
               "VC 绑定快照不可读: " + snap.source_error_detail);
        if (!snap.source_error) {
            std::string detail;
            const cep::ContractVerdict v = cep::evaluate_shared_mode_certificate(
                decl, snap, cep::default_untested_registry(), detail);
            if (v != cep::ContractVerdict::PASS) {
                // 归因 = detail 的**首个** bracket token(精确相等)
                std::string id = "CP-03-VC-CERTIFICATE";
                const std::size_t lb = detail.find('[');
                const std::size_t rb = detail.find(']');
                if (lb != std::string::npos && rb != std::string::npos && rb > lb + 1)
                    id = detail.substr(lb + 1, rb - lb - 1);
                std::printf("PROBE:FAIL %s —— %s\n", id.c_str(), detail.c_str());
                ++g_fail;
            }
        }
    }

    if (g_fail == 0)
        std::printf("PROBE:OK —— 队列 #14 实现层探针全绿\n");
    return g_fail == 0 ? 0 : 1;
}
