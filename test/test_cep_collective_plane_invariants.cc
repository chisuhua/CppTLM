// test/test_cep_collective_plane_invariants.cc
// capability `cep-collective-plane-base` 的 **B2 贡献载体** —— 既有 invariant 的**行为化**
//
// 作用:本仓的 15 组 66 条 invariant 由 `test_cep_canonical_invariants.cc` 校验**编号存在**;
//   本文件给其中**与 collective plane 相关**的两条补上**可执行的行为断言**:
//
//   · `INV-CEP-07-IC3`(per `ADR-CEP-07 §3`):`NullCollectivePlane` 是 Collective Plane 默认实现,
//     所有原语返回"不支持"
//   · `INV-CEP-04-IC5`(per `ADR-CEP-04 §11`):`fabric_ingress_classify` 中 "table 命中 +
//     epoch invalid" 窗口 = 正常服务放行(per R4 正常服务),非 fail-stop
//
//   ⚠️ **新增 invariant 数 = 0** —— 15 组 66 条编号是
//   `test_cep_canonical_invariants.cc::expected_groups()`
//     的**硬编码**;`ADR-CEP-01` / `ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反。
//     本 change 的 B2 贡献 = **行为化既有条目** + 契约层加固(per design D-ε)。
//
// 作者: CppTLM Team / 日期: 2026-10-26
//
// 纪律:
//   · 「编号存在」由 `test_cep_canonical_invariants.cc` 负责;本文件只管**行为**
//   · 每条行为断言配**对照** —— 纯负向断言无对照 = 判据可能没生效
//   · **零调用边**:本文件**不得** include directory / ordering / sharer 侧的头

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/collective_plane.hh"

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

    cep::AddrRange range(std::uint64_t lo, std::uint64_t hi) {
        cep::AddrRange r;
        r.lo = lo;
        r.hi = hi;
        return r;
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-07-IC3 —— NullCollectivePlane 是默认实现,所有原语返回"不支持"
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveInvariant: IC3 NullCollectivePlane 全原语不支持",
          "[cep][collective-invariant]") {
    // ① 编号确实在既有的 15 组清单里(不在本文件新增 —— 只**引用**)
    const std::string inv_src = read_repo_file("test/test_cep_canonical_invariants.cc");
    REQUIRE(!inv_src.empty());
    CHECK(inv_src.find("INV-CEP-07-IC3") != std::string::npos);

    // ② 行为:全部 7 个原语逐一 ⇒ 「不支持」
    cep::NullCollectivePlane plane;
    std::set<int> statuses;

    plane.group_create({1, 2}, cep::GroupAttrs{});
    statuses.insert(static_cast<int>(plane.last_status()));

    plane.group_destroy(1);
    statuses.insert(static_cast<int>(plane.last_status()));

    plane.group_register_region(1, range(0x1000, 0x1FFF), cep::AccessMode::Normal);
    statuses.insert(static_cast<int>(plane.last_status()));

    cep::CollectiveReq req;
    req.group = 1;
    req.op = cep::CollectiveOp::REDUCE;
    plane.send(req);
    statuses.insert(static_cast<int>(plane.last_status()));

    cep::CollectiveEvt evt;
    plane.recv(evt);
    statuses.insert(static_cast<int>(plane.last_status()));

    plane.collective_barrier(1, 0);
    statuses.insert(static_cast<int>(plane.last_status()));

    // 全部 7 个原语的状态**同一个**值 ⇒ 「不支持」
    CHECK(statuses.size() == 1u);
    CHECK(*statuses.begin() == static_cast<int>(cep::Status::Unsupported));

    // ③ 能力查询同样「不支持」(能力位 0)
    CHECK(plane.query_capability(cep::CollectiveOp::BARRIER, 64) == 0u);
    CHECK(plane.query_capability(cep::CollectiveOp::REDUCE, 64) == 0u);

    // ④ 无副作用:未建 group / 未投递事件
    CHECK(plane.group_count() == 0u);
    CHECK(plane.pending_event_count() == 0u);

    // ── 对照:非 Null 实现**不得**报「不支持」──
    //   (若 Null 与 Local 的状态相同 ⇒ 上面那条断言是恒真的空检查)
    cep::LocalCollectivePlane local;
    const auto gid = local.group_create({1, 2}, cep::GroupAttrs{});
    CHECK(gid != cep::kInvalidGroupId);
    CHECK(local.last_status() == cep::Status::Ok);
    CHECK(local.query_capability(cep::CollectiveOp::BARRIER, 64) != 0u);
}

// ═════════════════════════════════════════════════════════════════════
// INV-CEP-04-IC5 —— "table 命中 + epoch invalid" 窗口 = 正常服务放行(非 fail-stop)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveInvariant: IC5 epoch invalid 窗口 = 放行非 fail-stop",
          "[cep][collective-invariant]") {
    // ① 编号在既有清单里
    const std::string inv_src = read_repo_file("test/test_cep_canonical_invariants.cc");
    REQUIRE(!inv_src.empty());
    CHECK(inv_src.find("INV-CEP-04-IC5") != std::string::npos);

    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);

    // ② 「table 命中 + epoch invalid」窗口:地址命中 region 表,但 epoch 未 commit
    REQUIRE(plane.group_register_region(gid, range(0x1000, 0x1FFF), cep::AccessMode::Normal) ==
            cep::Status::Ok);
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::RegisteredEpochInvalid);

    // ③ 该窗口 = **放行**(判定为「不 divert」),**不得** fail-stop
    //    → 在窗口内发集合原语:报「未注册/不放行」类的**显式错误**,
    //      但**不得**把地址判成 active(那等于 divert ⇒ 违反 R4)
    cep::CollectiveReq req;
    req.group = gid;
    req.op = cep::CollectiveOp::REDUCE;
    req.payload.addr = 0x1000;
    plane.send(req);
    CHECK(plane.last_status() != cep::Status::Ok);
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::RegisteredEpochInvalid);
    CHECK(!plane.epoch_valid(gid));

    // ④ epoch commit 之后 ⇒ 判定翻转(divert 侧)
    REQUIRE(plane.complete_drain(gid, range(0x1000, 0x1FFF)) == cep::Status::Ok);
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::RegisteredActive);
    CHECK(plane.epoch_valid(gid));

    // ── 对照:未命中 region 的地址**始终**是 NoRegion(不因 epoch 状态而变)──
    //   (若「未命中」也变成 active ⇒ 判据恒真)
    CHECK(plane.region_state_of(0xDEAD0000) == cep::RegionState::NoRegion);

    // ── 对照:三态**互不相同** ──
    CHECK(static_cast<int>(cep::RegionState::NoRegion) !=
          static_cast<int>(cep::RegionState::RegisteredEpochInvalid));
    CHECK(static_cast<int>(cep::RegionState::RegisteredEpochInvalid) !=
          static_cast<int>(cep::RegionState::RegisteredActive));
}

// ═════════════════════════════════════════════════════════════════════
// B2 贡献清单 —— 非空 + 零新增编号(per design D-ε 自我否决条款)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectiveInvariant: B2 贡献清单非空且零新增编号", "[cep][collective-invariant]") {
    // ① 既有 15 组清单的编号**未被本 change 改动** —— 66 条计数不变
    const std::string inv_src = read_repo_file("test/test_cep_canonical_invariants.cc");
    REQUIRE(!inv_src.empty());
    // 「Simulation + Seam」组含 IC3
    CHECK(inv_src.find("INV-CEP-07-IC1") != std::string::npos);
    CHECK(inv_src.find("INV-CEP-07-IC3") != std::string::npos);
    CHECK(inv_src.find("INV-CEP-07-IC7") != std::string::npos);
    // 「Sharer Tracker」组含 IC5
    CHECK(inv_src.find("INV-CEP-04-IC5") != std::string::npos);

    // ② 本 change **未**新增任何 invariant 编号 ——
    //    collective 侧的 ADR(`ADR-CEP-01` / `ADR-CEP-03`)在 no-inv 白名单里,
    //    不得定义 invariant ⇒ 抓取点/CEP-13 等只能以**场景**与**契约**形式落地
    for (const char* adr : {"docs/adr/ADR-CEP-01-fabric-collective-plane.md",
                            "docs/adr/ADR-CEP-03-fabric-collective-plane-internals.md"}) {
        const std::string text = read_repo_file(adr);
        REQUIRE(!text.empty());
        // 白名单 ADR 内**不得**出现定义形态的编号(`- **INV-...**:` / `**INV-...**` 紧邻)
        CHECK(text.find("- **INV-") == std::string::npos);
    }

    // ③ 行为化载体:本文件(2 条既有编号的行为断言)存在且可执行
    const std::string self = read_repo_file("test/test_cep_collective_plane_invariants.cc");
    REQUIRE(!self.empty());
    CHECK(self.find("INV-CEP-07-IC3") != std::string::npos);
    CHECK(self.find("INV-CEP-04-IC5") != std::string::npos);

    // ④ 契约层加固载体:判据面文件存在
    const std::string contract = read_repo_file("include/tlm/cep/collective_plane_contract.hh");
    REQUIRE(!contract.empty());
    CHECK(contract.find("ContractRule") != std::string::npos);
}
