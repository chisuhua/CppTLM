// test/test_cep_ualink_inc.cc
// capability `cep-ualink-inc` —— 实现层可执行契约(UIA-01 ~ UIA-07 + V6-d 四条证书)
//
// 作者: CppTLM Team / 日期: 2026-11-04
//
// 纪律:
//   · 判据实现退化 = 用例红;判定结论为红 = 用例**仍绿**且结论登记为交付物
//   · 每条负向断言配**正控**(反向 fixture)—— 纯负向断言无对照 = 判据可能没生效
//   · 判据实现**唯一**(不另写一份重叠检测)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <filesystem>
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

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去注释(行 + 块)—— 禁项扫描只在**代码**上判定
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
                ++i;
                continue;
            }
            if (!line && !block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// **adapter 禁项清单**(per 本 change 的 `design §2.1` 禁项 ①)
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> v = {"Obligation::",
                                                   "BasisKind::",
                                                   "CertificateId::",
                                                   "UntestedId::",
                                                   "IncOp::",
                                                   "PendingId::",
                                                   "BasisCell",
                                                   "CertificateSpec",
                                                   "UntestedItem",
                                                   "PendingItem",
                                                   "PrimitiveSet",
                                                   "IncOpBasis",
                                                   "Ic1Discrimination",
                                                   "kBasisObligations",
                                                   "kBasisIncOps",
                                                   "kPendingSemanticItems",
                                                   "kCertificateSpecs",
                                                   "kUntestedItems",
                                                   "kIncOps",
                                                   "kIncOpPrimitives",
                                                   "kUsedPrimitiveNames",
                                                   "all_basis_valid",
                                                   "kSourceLicenseConfirmed"};
        return v;
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

    /// 造一个「已注册 + 已 drain」的 region
    bool prepare_active_region(cep::FabricCollectivePlane& plane, cep::group_id_t gid,
                               std::uint64_t base) {
        cep::AddrRange range;
        range.lo = base;
        range.hi = base + 0xFF;
        if (plane.group_register_region(gid, range, cep::AccessMode::Normal) != cep::Status::Ok)
            return false;
        return plane.complete_drain(gid, range) == cep::Status::Ok;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// UIA-01 —— `UalinkAdapter` **零自有表副本**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIA-01: UalinkAdapter 零自有表副本(禁项文本扫描)", "[cep][ualink-inc][adapter]") {
    const std::string raw = read_repo_file("include/tlm/cep/ualink_adapter.hh");
    REQUIRE(!raw.empty());
    const std::string code = strip_comments(raw);
    for (const auto& token : forbidden_tokens())
        REQUIRE(code.find(token) == std::string::npos);

    // **正控**:扫描器有牙 —— 规约载体**本身**含这些字面量
    const std::string carrier =
        strip_comments(read_repo_file("include/tlm/cep/ualink_inc_state_spec.hh"));
    bool carrier_has = false;
    for (const auto& token : forbidden_tokens()) {
        if (carrier.find(token) != std::string::npos) {
            carrier_has = true;
            break;
        }
    }
    REQUIRE(carrier_has);

    // adapter 的判定面**委托**规约载体(⛔ 不持有自己的表)
    REQUIRE(cep::UalinkAdapter::kObligationCount == cep::ualink_inc::kObligationCount);
    REQUIRE(cep::UalinkAdapter::kCertificateCount == cep::ualink_inc::kCertificateCount);
    REQUIRE(cep::UalinkAdapter::kUntestedCount == cep::ualink_inc::kUntestedCount);
    REQUIRE(cep::UalinkAdapter::m2_usable_on_network(true));
    REQUIRE_FALSE(cep::UalinkAdapter::m2_usable_on_network(false));
    REQUIRE(cep::UalinkAdapter::certificate_specs_wellformed());
    REQUIRE(cep::UalinkAdapter::untested_registry_clean());
    // ⛔ 三条**否定式**事实
    REQUIRE_FALSE(cep::UalinkAdapter::operator_family_defined_in_carrier());
    REQUIRE_FALSE(cep::UalinkAdapter::dag_rerun_is_certificate());
    REQUIRE_FALSE(cep::UalinkAdapter::region_table_exposed());
}

// ══════════════════════════════════════════════════════════════════════════
// UIA-02 —— `UalinkAdapter` **无 `transition_*`**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIA-02: UalinkAdapter 不暴露 INC 转移面", "[cep][ualink-inc][adapter]") {
    const std::string code = strip_comments(read_repo_file("include/tlm/cep/ualink_adapter.hh"));
    REQUIRE(!code.empty());
    REQUIRE(code.find("transition_") == std::string::npos);
    // **正控**:扫描器有牙
    REQUIRE(strip_comments("void transition_x() {}").find("transition_") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// UIA-03 —— `FabricCollectivePlane` 数据路径**真可达**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIA-03: FabricCollectivePlane 数据路径真可达(REDUCE/BROADCAST/MULTICAST)",
          "[cep][ualink-inc][plane]") {
    cep::FabricCollectivePlane plane;
    cep::member_set_t members{1u, 2u};
    const cep::group_id_t gid = plane.group_create(members, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);
    REQUIRE(prepare_active_region(plane, gid, 0x1000u));

    // ── REDUCE ──
    plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 1u, 10u, 0x1000u));
    REQUIRE(plane.pending_event_count() == 0u); // 未收齐 ⇒ 不产出事件
    plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 2u, 20u, 0x1000u));
    REQUIRE(plane.last_status() == cep::Status::Ok);
    REQUIRE(plane.pending_event_count() == 3u); // 2 个 Result + 1 个 Completion
    cep::CollectiveEvt evt;
    plane.recv(evt);
    REQUIRE(evt.kind() == cep::CollectiveEvt::Kind::Result);
    REQUIRE(evt.payload() == 30u); // Sum(10, 20)
    plane.recv(evt);               // 第二个成员的 Result
    REQUIRE(evt.kind() == cep::CollectiveEvt::Kind::Result);
    plane.recv(evt); // Completion
    REQUIRE(evt.kind() == cep::CollectiveEvt::Kind::Completion);
    REQUIRE(plane.pending_event_count() == 0u);

    // ── BROADCAST:首个贡献者的载荷为广播值 ──
    plane.send(make_req(gid, cep::CollectiveOp::BROADCAST, 1u, 77u, 0x1000u));
    plane.send(make_req(gid, cep::CollectiveOp::BROADCAST, 2u, 88u, 0x1000u));
    REQUIRE(plane.last_status() == cep::Status::Ok);
    REQUIRE(plane.pending_event_count() == 3u);
    plane.recv(evt);
    REQUIRE(evt.kind() == cep::CollectiveEvt::Kind::Result);
    REQUIRE(evt.payload() == 77u); // 广播值 = **首个**贡献者
    plane.recv(evt);
    plane.recv(evt);
    REQUIRE(plane.pending_event_count() == 0u);

    // ── MULTICAST:参与集 = 掩码选中的成员 ──
    cep::CollectiveReq mc = make_req(gid, cep::CollectiveOp::MULTICAST, 1u, 5u, 0x1000u);
    mc.multicast_mask = 0x2ull; // 掩码的**位下标 = 成员 id** ⇒ 只选成员 1
    plane.send(mc);
    REQUIRE(plane.pending_event_count() == 2u); // 1 个 Result + 1 个 Completion
    plane.recv(evt);
    REQUIRE(evt.payload() == 5u);

    // ── **可区分性**:同样操作在 `LocalCollectivePlane` 上仍是「未实现」──
    cep::LocalCollectivePlane local;
    const cep::group_id_t lgid = local.group_create(members, cep::GroupAttrs{});
    cep::AddrRange lrange;
    lrange.lo = 0x1000u;
    lrange.hi = 0x10FFu;
    local.group_register_region(lgid, lrange, cep::AccessMode::Normal);
    local.complete_drain(lgid, lrange);
    local.send(make_req(lgid, cep::CollectiveOp::REDUCE, 1u, 10u, 0x1000u));
    REQUIRE(local.last_status() == cep::Status::DataPathNotImplemented);

    // ── 负向形态:未注册区域 ⇒ 立即报错,**不**投递事件 ──
    cep::FabricCollectivePlane plane2;
    const cep::group_id_t gid2 = plane2.group_create(members, cep::GroupAttrs{});
    plane2.send(make_req(gid2, cep::CollectiveOp::REDUCE, 1u, 1u, 0x9999u));
    REQUIRE(plane2.last_status() == cep::Status::GroupRegisterError);
    REQUIRE(plane2.pending_event_count() == 0u);

    // ── 浮点归约 ⇒ **显式不支持**(⛔ 不是静默近似)──
    {
        cep::FabricCollectivePlane plane;
        cep::member_set_t members{1u, 2u};
        const cep::group_id_t gid = plane.group_create(members, cep::GroupAttrs{});
        REQUIRE(prepare_active_region(plane, gid, 0x5000u));
        cep::CollectiveReq f = make_req(gid, cep::CollectiveOp::REDUCE, 1u, 1u, 0x5000u);
        f.dtype = cep::DataType::F32;
        plane.send(f);
        REQUIRE(plane.last_status() == cep::Status::Unsupported);
        REQUIRE(plane.pending_event_count() == 0u);
    }

    // ── 跨 group 轮次**隔离**(轮次键含 `group_id`)──
    {
        cep::FabricCollectivePlane plane;
        cep::member_set_t m1{1u, 2u};
        cep::member_set_t m2{3u, 4u};
        const cep::group_id_t g1 = plane.group_create(m1, cep::GroupAttrs{});
        const cep::group_id_t g2 = plane.group_create(m2, cep::GroupAttrs{});
        REQUIRE(g1 != g2);
        REQUIRE(prepare_active_region(plane, g1, 0x6000u));
        REQUIRE(prepare_active_region(plane, g2, 0x7000u));
        // 同一 `(op, tag)`、**不同 group** ⇒ 两个**独立**轮次
        plane.send(make_req(g1, cep::CollectiveOp::REDUCE, 1u, 10u, 0x6000u));
        plane.send(make_req(g2, cep::CollectiveOp::REDUCE, 3u, 100u, 0x7000u));
        REQUIRE(plane.group_has_inflight(g1));
        REQUIRE(plane.group_has_inflight(g2));
        // 各自收齐 ⇒ 各自的结果**不串扰**
        plane.send(make_req(g1, cep::CollectiveOp::REDUCE, 2u, 20u, 0x6000u));
        REQUIRE_FALSE(plane.group_has_inflight(g1));
        REQUIRE(plane.group_has_inflight(g2));
        plane.send(make_req(g2, cep::CollectiveOp::REDUCE, 4u, 200u, 0x7000u));
        REQUIRE_FALSE(plane.group_has_inflight(g2));
        // 结果:g1 = 10+20 = 30;g2 = 100+200 = 300 ⇒ 逐组可辨
        std::vector<std::uint64_t> payloads;
        cep::CollectiveEvt e;
        while (plane.pending_event_count() > 0) {
            plane.recv(e);
            if (e.kind() == cep::CollectiveEvt::Kind::Result)
                payloads.push_back(e.payload());
        }
        REQUIRE(payloads.size() == 4u);
        REQUIRE(payloads[0] == 30u);
        REQUIRE(payloads[2] == 300u);
    }

    // ── 能力位:真可达 ⇒ 非 0(⛔ 不谎报)──
    REQUIRE(plane.query_capability(cep::CollectiveOp::REDUCE, 8u) != 0u);
    REQUIRE(plane.query_capability(cep::CollectiveOp::BROADCAST, 8u) != 0u);
    REQUIRE(plane.query_capability(cep::CollectiveOp::MULTICAST, 8u) != 0u);
    REQUIRE(plane.query_capability(cep::CollectiveOp::REDUCE, 0u) == 0u); // max_bytes == 0 ⇒ 不支持
}

// ══════════════════════════════════════════════════════════════════════════
// UIA-04 —— **C-3 控制面与数据面互斥**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIA-04: C-3 —— in-flight 期间拒绝控制面操作", "[cep][ualink-inc][plane][certificate]") {
    cep::FabricCollectivePlane plane;
    cep::member_set_t members{1u, 2u};
    const cep::group_id_t gid = plane.group_create(members, cep::GroupAttrs{});
    REQUIRE(prepare_active_region(plane, gid, 0x2000u));

    plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 1u, 10u, 0x2000u));
    REQUIRE(plane.group_has_inflight(gid));

    // in-flight ⇒ 控制面操作被拒绝
    cep::AddrRange other;
    other.lo = 0x3000u;
    other.hi = 0x30FFu;
    REQUIRE(plane.group_register_region(gid, other, cep::AccessMode::Normal) ==
            cep::Status::ControlPlaneConflict);
    plane.group_destroy(gid);
    REQUIRE(plane.last_status() == cep::Status::ControlPlaneConflict);
    REQUIRE(plane.group_alive(gid)); // group 未被销毁 ⇒ 数据不丢

    // 收齐 ⇒ 恢复
    plane.send(make_req(gid, cep::CollectiveOp::REDUCE, 2u, 20u, 0x2000u));
    REQUIRE_FALSE(plane.group_has_inflight(gid));
    REQUIRE(plane.group_register_region(gid, other, cep::AccessMode::Normal) == cep::Status::Ok);

    // **正控**:无 in-flight 时注册成功(否则上面的拒绝无鉴别力)
    cep::FabricCollectivePlane plane2;
    const cep::group_id_t gid2 = plane2.group_create(members, cep::GroupAttrs{});
    cep::AddrRange r2;
    r2.lo = 0x4000u;
    r2.hi = 0x40FFu;
    REQUIRE(plane2.group_register_region(gid2, r2, cep::AccessMode::Normal) == cep::Status::Ok);

    // 拒绝状态**专属**(⛔ 不复用「不支持」)⇒ 调用方可区分「能力缺失」与「时序冲突」
    REQUIRE(std::string(cep::status_name(cep::Status::ControlPlaneConflict)) ==
            "control-plane-conflict");
    REQUIRE(std::string(cep::status_name(cep::Status::Unsupported)) != "control-plane-conflict");
}

// ══════════════════════════════════════════════════════════════════════════
// UIA-05 / UIA-06 —— 声明面:`status` 2 值 + `certificate_scope`
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIA-05/06: 声明面 status 取值域 = 2 值 + certificate_scope 登记",
          "[cep][ualink-inc][declaration]") {
    const std::string decl_text = read_repo_file("include/tlm/cep/collective_planes.json");
    REQUIRE(!decl_text.empty());
    const auto doc = nlohmann::json::parse(decl_text);
    const auto decl = cep::parse_collective_plane_declaration(doc);
    REQUIRE_FALSE(decl.source_error);

    // 当前声明走 **M-2**
    REQUIRE(decl.vc_reservation_declaration.status == "shared_no_isolation");
    REQUIRE(decl.vc_reservation_declaration.complete());
    REQUIRE(decl.vc_reservation_declaration.certificate_scope.size() == 1u);
    REQUIRE(decl.vc_reservation_declaration.certificate_scope[0].plane == "FabricCollectivePlane");
    REQUIRE(decl.vc_reservation_declaration.certificate_scope[0].network == "ualink_fabric");

    const auto snap = cep::load_vc_binding_snapshot(
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/protocol_instances.json",
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/fabric.json");
    REQUIRE_FALSE(snap.source_error);

    // 正控:当前声明 ⇒ PASS
    std::string d;
    REQUIRE(cep::evaluate_vc_reservation(decl, snap, d) == cep::ContractVerdict::PASS);

    // 反 fixture:`status` 改成第三个取值 ⇒ 必红
    auto wrong = decl;
    wrong.vc_reservation_declaration.status = "shared";
    std::string dw;
    REQUIRE(cep::evaluate_vc_reservation(wrong, snap, dw) == cep::ContractVerdict::FAIL);

    // 反 fixture:`certificate_scope` 为空 ⇒ 必红
    auto no_scope = decl;
    no_scope.vc_reservation_declaration.certificate_scope.clear();
    std::string ds;
    REQUIRE(cep::evaluate_vc_reservation(no_scope, snap, ds) == cep::ContractVerdict::FAIL);

    // 反 fixture:scope 的 network 未在 fabric 登记 ⇒ 必红
    auto bad_net = decl;
    bad_net.vc_reservation_declaration.certificate_scope[0].network = "no_such_net";
    std::string dn;
    REQUIRE(cep::evaluate_vc_reservation(bad_net, snap, dn) == cep::ContractVerdict::FAIL);

    // 正控:`unavailable` 变体 ⇒ PASS(M-1 语义未被破坏)
    auto m1 = decl;
    m1.vc_reservation_declaration.status = "unavailable";
    std::string dm1;
    REQUIRE(cep::evaluate_vc_reservation(m1, snap, dm1) == cep::ContractVerdict::PASS);
}

// ══════════════════════════════════════════════════════════════════════════
// UIA-07 —— `ualink_4` **不**声明
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIA-07: ualink_4 实例不声明(收窄的可执行形态)", "[cep][ualink-inc][declaration]") {
    const std::string text = read_repo_file("include/tlm/cep/protocol_instances.json");
    REQUIRE(!text.empty());
    const auto doc = nlohmann::json::parse(text);
    REQUIRE(doc.contains("instances"));
    REQUIRE(doc["instances"].is_array());
    REQUIRE(doc["instances"].size() == 7u);
    for (const auto& inst : doc["instances"]) {
        REQUIRE(inst.contains("id"));
        REQUIRE(inst["id"].is_string());
        REQUIRE(inst["id"].get<std::string>() != "ualink_4");
    }
}

// ══════════════════════════════════════════════════════════════════════════
// V6-d —— 四条证书(C-1′ / C-2 / C-3 / C-4)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("V6-d: M-2 共享模式证书全绿(C-1′/C-2/C-3/C-4)", "[cep][ualink-inc][certificate]") {
    const std::string decl_text = read_repo_file("include/tlm/cep/collective_planes.json");
    const auto doc = nlohmann::json::parse(decl_text);
    const auto decl = cep::parse_collective_plane_declaration(doc);
    const auto snap = cep::load_vc_binding_snapshot(
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/protocol_instances.json",
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/fabric.json");
    REQUIRE_FALSE(snap.source_error);

    std::string detail;
    REQUIRE(cep::evaluate_shared_mode_certificate(decl, snap, cep::default_untested_registry(),
                                                  detail) == cep::ContractVerdict::PASS);
    REQUIRE(detail.rfind("[CP-03-VC-CERTIFICATE]", 0) == 0);

    // ── C-1′:该网实例映射**不一致** ⇒ 必红(前缀**精确**)──
    {
        auto poisoned = snap;
        auto& mappings = poisoned.per_network_mappings["ualink_fabric"];
        REQUIRE(mappings.size() >= 2u);
        mappings[1]["query"] = 3u; // 与 mappings[0] 不一致
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(decl, poisoned,
                                                      cep::default_untested_registry(),
                                                      d) == cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-03-VC-CERTIFICATE/C-1\u2032]", 0) == 0);
    }
    // ── C-1′ **结构面**:声明源含 `collective_plane_vc_binding` ⇒ 必红(per `ADR-CEP-01` E.9)──
    {
        auto poisoned = decl;
        poisoned.collective_plane_vc_binding_present = true;
        poisoned.collective_plane_vc_binding_entries.emplace_back(
            "FabricCollectivePlane -> {\"ualink_fabric\":{\"request\":0}}");
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(poisoned, snap,
                                                      cep::default_untested_registry(), d) ==
                cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-03-VC-CERTIFICATE/C-1\u2032]", 0) == 0);
    }
    // ── C-2:`vc_count` 调大 ⇒ 覆盖不全 ⇒ 必红 ──
    {
        auto poisoned = snap;
        poisoned.vc_count["ualink_fabric"] = 5u;
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(decl, poisoned,
                                                      cep::default_untested_registry(),
                                                      d) == cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-09-IC1]", 0) == 0);
    }
    // ── C-3:plane **未登记** ⇒ 必红 ──
    {
        auto poisoned = decl;
        poisoned.vc_reservation_declaration.certificate_scope[0].plane = "NoSuchPlane";
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(poisoned, snap,
                                                      cep::default_untested_registry(),
                                                      d) == cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-03-VC-CERTIFICATE/C-3]", 0) == 0);
    }
    // ── C-3:plane 是**本地模型** ⇒ 不在 M-2 覆盖内 ⇒ 必红 ──
    {
        auto poisoned = decl;
        poisoned.vc_reservation_declaration.certificate_scope[0].plane = "LocalCollectivePlane";
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(poisoned, snap,
                                                      cep::default_untested_registry(),
                                                      d) == cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-03-VC-CERTIFICATE/C-3]", 0) == 0);
    }
    // ── C-4:未检验项被冒充成「已验证」⇒ 必红 ──
    {
        cep::UntestedRegistry dirty;
        dirty.items.emplace_back("U-1", "「VC 隔离确实避免协议级死锁」—— 已验证");
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(decl, snap, dirty, d) ==
                cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-03-VC-CERTIFICATE/C-4]", 0) == 0);
    }
    // ── C-4:登记**为空** ⇒ 必红 ──
    {
        cep::UntestedRegistry empty;
        std::string d;
        REQUIRE(cep::evaluate_shared_mode_certificate(decl, snap, empty, d) ==
                cep::ContractVerdict::FAIL);
        REQUIRE(d.rfind("[CP-03-VC-CERTIFICATE/C-4]", 0) == 0);
    }
    // ── 正控:未检验项登记**由规约载体提供**(⛔ 判据面不复制)──
    {
        const auto reg = cep::default_untested_registry();
        REQUIRE(reg.items.size() == 2u);
        REQUIRE(reg.items[0].first == "U-1");
        REQUIRE(reg.items[1].first == "U-2");
    }
}
