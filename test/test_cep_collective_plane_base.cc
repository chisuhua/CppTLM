// test/test_cep_collective_plane_base.cc
// capability `cep-collective-plane-base` —— 13 Scenario / 13 锚点(
//   接口面 7 方法 + 4 原语 / 类型面含 Fault 变体 / Null 全原语不支持 /
//   group 生命周期 + destroy ⇒ Fault / BARRIER rendezvous / 归属判定三态(R2 判读侧) /
//   区域互斥(CEP-13) / 未注册区域报错 / 已注册区域数据原语未实现 / functional 零延迟 /
//   capability 边界 / 完成域判别反向面 / 零调用边)
//
// 作者: CppTLM Team / 日期: 2026-10-26
//
// 纪律:
//   · 每条**负向**断言配**对照**(正控 / 反向 fixture)—— 纯负向断言无对照 = 判据可能没生效
//   · 「不支持」「未实现」「未注册」三态**必须可区分**(per design D-ι 裁决 3/4)
//   · 本文件**不得** include directory / ordering / sharer 侧的头(零调用边的**行为面**侧)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/collective_plane_contract.hh"

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

    /// 去掉注释后的源码文本(用于静态扫描;per 队列 #3/#4 先例)
    std::string strip_comments(const std::string& src) {
        std::string out;
        bool in_line = false, in_block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (!in_block && !in_line && i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/') {
                in_line = true;
                ++i;
                continue;
            }
            if (!in_block && !in_line && i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
                in_block = true;
                ++i;
                continue;
            }
            if (in_line && src[i] == '\n') {
                in_line = false;
                out.push_back('\n');
                continue;
            }
            if (in_block && i + 1 < src.size() && src[i] == '*' && src[i + 1] == '/') {
                in_block = false;
                ++i;
                continue;
            }
            if (!in_line && !in_block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// 名字是否出现在某个**含 "virtual" 的声明语句**里(返回类型在中间,故按语句回溯)
    bool declared_in_pure_virtual(const std::string& src, const std::string& name) {
        std::size_t pos = 0;
        while ((pos = src.find(name, pos)) != std::string::npos) {
            const auto semi = src.rfind(';', pos);
            const std::size_t start = (semi == std::string::npos) ? 0 : semi + 1;
            const std::string stmt = src.substr(start, pos - start + name.size());
            if (stmt.find("virtual") != std::string::npos)
                return true;
            pos += name.size();
        }
        return false;
    }

    cep::AddrRange range(std::uint64_t lo, std::uint64_t hi) {
        cep::AddrRange r;
        r.lo = lo;
        r.hi = hi;
        return r;
    }

} // namespace

// ═════════════════════════════════════════════════════════════════════
// Scenario 1:接口面 —— 7 方法 + 原语恰好 4 值 + AllReduce 不出现
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: 接口 7 方法 + 原语恰好 4 值", "[cep][collective-plane]") {
    // 7 个虚方法(逐字对齐 `ADR-CEP-03 §2 Decision 2`)
    const std::string code = read_repo_file("include/tlm/cep/collective_plane.hh");
    REQUIRE(!code.empty());
    // 方法名须出现在某个**含 "virtual" 的纯虚声明语句**里。
    // ⚠️ 不能写 `virtual <name>` —— `virtual` 之后是**返回类型**(`virtual group_id_t
    // group_create`)。
    for (const char* m : {"group_create", "group_destroy", "group_register_region", "send", "recv",
                          "collective_barrier", "query_capability"}) {
        CHECK(declared_in_pure_virtual(strip_comments(code), m));
    }
    // 纯虚方法恰为 7 —— 用 `= 0;` 计数(装饰性 override 不算)
    // ⚠️ 必须**回溯到语句起点**再找 "virtual":跨行声明的 `= 0;` 落在**没有** "virtual"
    //    的那一行(实测 `group_register_region` 如此)⇒ 按行找会漏计(6 ≠ 7)。
    // ⚠️ 计数基于**去注释后**的文本,否则注释里的 `= 0;` 会被计入。
    std::size_t pure_virtual = 0;
    {
        const std::string src = strip_comments(code);
        std::size_t pos = 0;
        while ((pos = src.find("= 0;", pos)) != std::string::npos) {
            const auto semi = src.rfind(';', pos);
            const std::size_t start = (semi == std::string::npos) ? 0 : semi + 1;
            const std::string stmt = src.substr(start, pos - start);
            if (stmt.find("virtual") != std::string::npos)
                ++pure_virtual;
            pos += 4;
        }
    }
    CHECK(pure_virtual == 7u);

    // 原语恰好 4 值
    CHECK(cep::kCollectiveOpCount == 4u);
    const std::vector<cep::CollectiveOp> ops = {
        cep::CollectiveOp::REDUCE, cep::CollectiveOp::BROADCAST, cep::CollectiveOp::MULTICAST,
        cep::CollectiveOp::BARRIER};
    std::set<std::uint8_t> seen;
    for (const auto op : ops) {
        seen.insert(static_cast<std::uint8_t>(op));
        CHECK(std::string(cep::collective_op_name(op)).size() > 0u);
    }
    CHECK(seen.size() == 4u);

    // AllReduce **不得**作为原语出现
    const std::string stripped = strip_comments(code);
    CHECK(stripped.find("AllReduce") == std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 2:类型面 —— CollectiveEvt 含 Fault 变体(缺 ⇒ 红)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: CollectiveEvt 含 Fault 变体", "[cep][collective-plane]") {
    CHECK(cep::kCollectiveEvtKindCount == 3u);

    const cep::CollectiveEvt res = cep::CollectiveEvt::make_result(1, 7, 42);
    const cep::CollectiveEvt comp = cep::CollectiveEvt::make_completion(1, 7);
    const cep::CollectiveEvt fault =
        cep::CollectiveEvt::make_fault(1, 3, cep::Status::InvalidGroup);

    CHECK(static_cast<int>(res.kind()) != static_cast<int>(fault.kind()));
    CHECK(static_cast<int>(comp.kind()) != static_cast<int>(fault.kind()));
    CHECK(res.kind() == cep::CollectiveEvt::Kind::Result);
    CHECK(comp.kind() == cep::CollectiveEvt::Kind::Completion);
    CHECK(fault.kind() == cep::CollectiveEvt::Kind::Fault);
    CHECK(fault.is_fault());
    CHECK(fault.culprit() == 3u);
    CHECK(fault.cause() == cep::Status::InvalidGroup);
    CHECK(std::string(fault.kind_name()) == "Fault");

    // 对照:三个变体**互不相同**(否则「缺 Fault」这条断言恒真)
    CHECK(static_cast<int>(res.kind()) != static_cast<int>(comp.kind()));
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 3:NullCollectivePlane 全原语不支持 + 无副作用 —— 反向 fixture
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: Null 全原语返回不支持", "[cep][collective-plane]") {
    cep::NullCollectivePlane null_plane;

    CHECK(null_plane.group_create({1, 2}, cep::GroupAttrs{}) == cep::kInvalidGroupId);
    CHECK(null_plane.last_status() == cep::Status::Unsupported);

    null_plane.group_destroy(1);
    CHECK(null_plane.last_status() == cep::Status::Unsupported);

    CHECK(null_plane.group_register_region(1, range(0x1000, 0x1FFF), cep::AccessMode::Normal) ==
          cep::Status::Unsupported);

    cep::CollectiveReq req;
    req.group = 1;
    req.op = cep::CollectiveOp::REDUCE;
    null_plane.send(req);
    CHECK(null_plane.last_status() == cep::Status::Unsupported);

    cep::CollectiveEvt evt;
    null_plane.recv(evt);
    CHECK(null_plane.last_status() == cep::Status::Unsupported);

    null_plane.collective_barrier(1, 0);
    CHECK(null_plane.last_status() == cep::Status::Unsupported);

    CHECK(null_plane.query_capability(cep::CollectiveOp::BARRIER, 64) == 0u);

    // 无副作用:未建 group / 未投递事件
    CHECK(null_plane.group_count() == 0u);
    CHECK(null_plane.pending_event_count() == 0u);

    // ── 反向 fixture:注入一个「假装支持」的 plane ⇒ 「不支持」这条判定必须能区分 ──
    //   若判据恒真(如"任何实现都返回 Unsupported"),下面这条对照会失败。
    cep::LocalCollectivePlane supporting;
    const auto gid = supporting.group_create({1, 2}, cep::GroupAttrs{});
    CHECK(gid != cep::kInvalidGroupId);
    CHECK(supporting.last_status() == cep::Status::Ok);
    CHECK(supporting.query_capability(cep::CollectiveOp::BARRIER, 64) != 0u);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 4:group 生命周期 + group_destroy 对 in-flight 发 Fault
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: group 生命周期 + destroy ⇒ Fault", "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2, 3}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);
    CHECK(plane.group_alive(gid));
    CHECK(plane.group_count() == 1u);

    // 成员 1 到达 barrier(未满 N)⇒ 无完成事件
    plane.set_current_member(1);
    plane.collective_barrier(gid, 0);
    CHECK(plane.pending_event_count() == 0u);

    // destroy ⇒ 对 in-flight 成员发 Fault,且**不阻塞**
    plane.group_destroy(gid);
    CHECK(!plane.group_alive(gid));
    CHECK(plane.pending_event_count() == 1u);

    cep::CollectiveEvt evt;
    plane.recv(evt);
    CHECK(evt.is_fault());
    CHECK(evt.group() == gid);
    CHECK(evt.culprit() == 1u);

    // 对照:destroy 一个**不存在**的 group ⇒ InvalidGroup(与上面的 Fault 路径可区分)
    plane.group_destroy(9999);
    CHECK(plane.last_status() == cep::Status::InvalidGroup);

    // ── destroy 后 region 表**移除**(否则残留会永久阻塞重注册)──
    cep::LocalCollectivePlane p2;
    const auto gid2 = p2.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid2 != cep::kInvalidGroupId);
    REQUIRE(p2.group_register_region(gid2, range(0x1000, 0x1FFF),
                                     cep::AccessMode::Normal) == cep::Status::Ok);
    CHECK(p2.region_state_of(0x1000) == cep::RegionState::RegisteredEpochInvalid);

    p2.group_destroy(gid2);
    // region 表已移除 ⇒ 该地址回到 NoRegion(R4「区域回归 coherence 域」)
    CHECK(p2.region_state_of(0x1000) == cep::RegionState::NoRegion);

    // **关键**:同一地址段能被**新 group 重新注册**(残留会让它永久报废)
    const auto gid3 = p2.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid3 != cep::kInvalidGroupId);
    CHECK(p2.group_register_region(gid3, range(0x1000, 0x1FFF),
                                   cep::AccessMode::Normal) == cep::Status::Ok);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 5:BARRIER rendezvous —— N-1 不释放 / N 释放
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: BARRIER rendezvous N-1 不释放 N 释放", "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({10, 20, 30}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);

    plane.set_current_member(10);
    plane.collective_barrier(gid, 0);
    CHECK(plane.pending_event_count() == 0u); // 1/3

    plane.set_current_member(20);
    plane.collective_barrier(gid, 0);
    CHECK(plane.pending_event_count() == 0u); // 2/3 = N-1 ⇒ **不**释放

    plane.set_current_member(30);
    plane.collective_barrier(gid, 0);
    CHECK(plane.pending_event_count() == 3u); // 3/3 = N ⇒ 释放,每个成员一个完成事件

    cep::CollectiveEvt evt;
    plane.recv(evt);
    CHECK(evt.kind() == cep::CollectiveEvt::Kind::Completion);
    CHECK(evt.group() == gid);
    CHECK(evt.tag() == 0u);

    // ── 跨相位有序 ──
    // ⚠️ 用**相对计数**(前一段已释放 3 个完成事件、且上面 recv 只消耗了 1 个 ⇒ 基线非 0)
    const std::size_t base = plane.pending_event_count();
    // 对照:相位 1 的到达**不得**释放(相位 0 已释放完;相位 1 自身未满 N)
    plane.set_current_member(10);
    plane.collective_barrier(gid, 1);
    CHECK(plane.pending_event_count() == base); // 相位 1 只到 1/3
    plane.set_current_member(20);
    plane.collective_barrier(gid, 1);
    CHECK(plane.pending_event_count() == base); // 相位 1 到 2/3 = N-1
    plane.set_current_member(30);
    plane.collective_barrier(gid, 1);
    CHECK(plane.pending_event_count() == base + 3u); // 相位 1 释放,3 个成员各一个完成事件

    // ── 非成员到达**不得**计入阈值(否则 N-1 规则被绕过)──
    {
        cep::LocalCollectivePlane plane3;
        const auto gid3 = plane3.group_create({10, 20, 30}, cep::GroupAttrs{});
        REQUIRE(gid3 != cep::kInvalidGroupId);

        // 非成员 99 到达 ⇒ 拒绝,不得计入
        plane3.set_current_member(99);
        plane3.collective_barrier(gid3, 0);
        CHECK(plane3.last_status() == cep::Status::NotMember);
        CHECK(plane3.pending_event_count() == 0u);

        // 再加 2 个真成员 ⇒ 仍不足 N ⇒ 不释放(若非成员被计入,此刻就会释放)
        plane3.set_current_member(10);
        plane3.collective_barrier(gid3, 0);
        plane3.set_current_member(20);
        plane3.collective_barrier(gid3, 0);
        CHECK(plane3.pending_event_count() == 0u);

        // 第 3 个真成员到达 ⇒ 释放
        plane3.set_current_member(30);
        plane3.collective_barrier(gid3, 0);
        CHECK(plane3.pending_event_count() == 3u);
    }

    // ── 反向 fixture:若阈值被改成 N-1,上面第 2 步(2/3)就会释放 ──
    //   ⇒ 「2/3 ⇒ 0 个事件」这条断言就是阈值判别力的**对照**
    //   另:重复到达同一成员**不得**重复计数(否则 1 人可达 N)
    cep::LocalCollectivePlane plane2;
    const auto gid2 = plane2.group_create({10, 20, 30}, cep::GroupAttrs{});
    plane2.set_current_member(10);
    plane2.collective_barrier(gid2, 0);
    plane2.collective_barrier(gid2, 0);
    plane2.collective_barrier(gid2, 0);
    CHECK(plane2.pending_event_count() == 0u); // 同成员重复到达不计数
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 6:归属判定三态(R2 判读侧)—— epoch invalid ⇒ 不 divert
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: 归属判定三态 epoch invalid ⇒ 不 divert",
          "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);

    // 未注册 ⇒ NoRegion
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::NoRegion);

    // 注册后(drain 未回告)⇒ RegisteredEpochInvalid ⇒ **不 divert**(放行)
    REQUIRE(plane.group_register_region(gid, range(0x1000, 0x1FFF), cep::AccessMode::Normal) ==
            cep::Status::Ok);
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::RegisteredEpochInvalid);
    CHECK(plane.region_state_of(0x1FFF) == cep::RegionState::RegisteredEpochInvalid);
    CHECK(plane.region_state_of(0x2000) == cep::RegionState::NoRegion); // 区间外
    CHECK(!plane.epoch_valid(gid));

    // drain 回告后 ⇒ RegisteredActive ⇒ divert
    REQUIRE(plane.complete_drain(gid, range(0x1000, 0x1FFF)) == cep::Status::Ok);
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::RegisteredActive);
    CHECK(plane.epoch_valid(gid));

    // ── 反向 fixture:若 epoch 有效性检查被删(永远判 active)⇒ 前面的
    //    「注册后 ⇒ RegisteredEpochInvalid」就会变成 RegisteredActive ⇒ 断言必红。
    //    对照:同一地址在 drain 前后**结论不同** ⇒ 判据有判别力(不是恒真)。
    // 另:「放行」语义 ⇒ epoch invalid 窗口内发数据原语**不得**执行(只报错)
    cep::LocalCollectivePlane plane2;
    const auto gid2 = plane2.group_create({1, 2}, cep::GroupAttrs{});
    plane2.group_register_region(gid2, range(0x1000, 0x1FFF), cep::AccessMode::Normal);
    cep::CollectiveReq req;
    req.group = gid2;
    req.op = cep::CollectiveOp::REDUCE;
    req.payload.addr = 0x1000;
    plane2.send(req);
    CHECK(plane2.last_status() == cep::Status::GroupRegisterError);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 7:区域互斥(CEP-13)—— 重叠 ⇒ 报错;drain 失败 ⇒ 不建 epoch
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: 区域互斥(CEP-13)", "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);

    // 正控:不重叠的合法注册成功
    REQUIRE(plane.group_register_region(gid, range(0x1000, 0x1FFF), cep::AccessMode::Normal) ==
            cep::Status::Ok);

    // 重叠 ⇒ 报错
    CHECK(plane.group_register_region(gid, range(0x1800, 0x2800), cep::AccessMode::Normal) ==
          cep::Status::RegionOverlap);
    CHECK(plane.last_status() == cep::Status::RegionOverlap);

    // 非法区间 ⇒ InvalidRange
    CHECK(plane.group_register_region(gid, range(0x5000, 0x4000), cep::AccessMode::Normal) ==
          cep::Status::InvalidRange);

    // 无效 group ⇒ InvalidGroup
    CHECK(plane.group_register_region(9999, range(0x9000, 0x9FFF), cep::AccessMode::Normal) ==
          cep::Status::InvalidGroup);

    // 对照:重叠注册**不得**改变既有 region 的生效状态
    CHECK(plane.region_state_of(0x1000) == cep::RegionState::RegisteredEpochInvalid);
    CHECK(plane.region_state_of(0x2800) == cep::RegionState::NoRegion); // 重叠部分未生效
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 8:未注册区域 ⇒ 立即报错(不静默)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: 未注册区域 ⇒ 立即报错", "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);

    cep::CollectiveReq req;
    req.group = gid;
    req.op = cep::CollectiveOp::MULTICAST;
    req.payload.addr = 0xDEAD0000; // 未注册

    plane.send(req);
    CHECK(plane.last_status() == cep::Status::GroupRegisterError);

    // ⚠️ **不得投递任何事件**(spec 明文)—— 错误只经**状态**传达
    //   (事件通道留给「已注册但被中断」这类真正的 partial failure)
    CHECK(plane.pending_event_count() == 0u);
    // 对照:错误**确实**被报告(不是静默)
    CHECK(plane.last_status() == cep::Status::GroupRegisterError);

    // 「未注册」与「未实现」与「不支持」三态**可区分**
    CHECK(cep::Status::GroupRegisterError != cep::Status::DataPathNotImplemented);
    CHECK(cep::Status::GroupRegisterError != cep::Status::Unsupported);
    CHECK(std::string(cep::status_name(cep::Status::GroupRegisterError)) == "group-register-error");
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 9:已注册 + epoch 生效区域上的数据原语 ⇒ 未实现(显式,不静默)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: 已注册区域数据原语 ⇒ 未实现而非静默", "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1, 2}, cep::GroupAttrs{});
    REQUIRE(gid != cep::kInvalidGroupId);
    REQUIRE(plane.group_register_region(gid, range(0x1000, 0x1FFF), cep::AccessMode::Normal) ==
            cep::Status::Ok);
    REQUIRE(plane.complete_drain(gid, range(0x1000, 0x1FFF)) == cep::Status::Ok);

    cep::CollectiveReq req;
    req.group = gid;
    req.op = cep::CollectiveOp::REDUCE;
    req.payload.addr = 0x1000;
    plane.send(req);

    // 「数据路径未实现」必须与「未注册」「不支持」**可区分**
    CHECK(plane.last_status() == cep::Status::DataPathNotImplemented);
    CHECK(plane.last_status() != cep::Status::GroupRegisterError);
    CHECK(plane.last_status() != cep::Status::Unsupported);
    // 不投递事件(未实现 ≠ 失败事件)
    CHECK(plane.pending_event_count() == 0u);

    // `recv` 空队列 ⇒ `NoEvent`(与「取到事件」可区分;调用方不会把旧事件误读为新事件)
    cep::CollectiveEvt stale;
    stale = cep::CollectiveEvt::make_fault(7, 3, cep::Status::InvalidGroup);
    plane.recv(stale);
    CHECK(plane.last_status() == cep::Status::NoEvent);
    CHECK_FALSE(stale.is_fault());

    // 对照:BARRIER 在同一 group 上**可用**(能力位非 0)⇒ 不是「整个 plane 不支持」
    CHECK(plane.query_capability(cep::CollectiveOp::BARRIER, 64) != 0u);
    CHECK(plane.query_capability(cep::CollectiveOp::REDUCE, 64) == 0u);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 10:functional 模式 —— collective 不产生 latency
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: functional 模式不产生 latency", "[cep][collective-plane]") {
    // collective 平面**不引入**周期/延迟概念 ⇒ 事件里没有时间戳字段可断言
    const std::string code = read_repo_file("include/tlm/cep/collective_plane.hh");
    REQUIRE(!code.empty());
    const std::string stripped = strip_comments(code);
    for (const char* forbidden : {"latency", "cycle_count", "timestamp", "delay"}) {
        CHECK(stripped.find(forbidden) == std::string::npos);
    }
    // 对照:barrier 完成是**零延迟即时** —— 调用后事件立即可取
    cep::LocalCollectivePlane plane;
    const auto gid = plane.group_create({1}, cep::GroupAttrs{});
    plane.set_current_member(1);
    plane.collective_barrier(gid, 0);
    CHECK(plane.pending_event_count() == 1u); // 立即可得,无等待
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 11:query_capability 边界 —— 未知 op / max_bytes == 0
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: capability 边界契约", "[cep][collective-plane]") {
    cep::LocalCollectivePlane plane;

    // 已知 op
    CHECK(plane.query_capability(cep::CollectiveOp::BARRIER, 64) != 0u);
    CHECK(plane.query_capability(cep::CollectiveOp::REDUCE, 64) == 0u);

    // max_bytes == 0 ⇒ 明确返回,**不抛异常**
    const auto cap0 = plane.query_capability(cep::CollectiveOp::BARRIER, 0);
    CHECK(cap0 == plane.query_capability(cep::CollectiveOp::BARRIER, 64));

    // 未知 op(越界枚举值)⇒ 明确返回 0,**不抛异常**
    const auto bogus = static_cast<cep::CollectiveOp>(200);
    CHECK(plane.query_capability(bogus, 64) == 0u);

    // Null 侧同样不抛
    cep::NullCollectivePlane null_plane;
    CHECK(null_plane.query_capability(bogus, 0) == 0u);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 12:完成域判别的反向面 —— group_id 不在 coherence 侧
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: group_id 不出现在 coherence 侧", "[cep][collective-plane]") {
    // coherence 侧 = ordering_point_checker.hh(队列 #4 产物,承载 Ordering Point 判据)
    const std::string coherence =
        strip_comments(read_repo_file("include/tlm/cep/ordering_point_checker.hh"));
    REQUIRE(!coherence.empty());
    for (const char* g :
         {"group_id", "member_set", "CollectiveReq", "CollectiveEvt", "group_create"}) {
        CHECK(coherence.find(g) == std::string::npos);
    }
    // 对照:collective 侧**确实**有这些标识符(否则上面的断言是恒真的空检查)
    const std::string collective =
        strip_comments(read_repo_file("include/tlm/cep/collective_plane.hh"));
    REQUIRE(!collective.empty());
    CHECK(collective.find("group_id") != std::string::npos);
    CHECK(collective.find("CollectiveReq") != std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 13:零调用边(行为面侧)—— 反向 fixture
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP CollectivePlane: 零调用边(行为面侧)", "[cep][collective-plane]") {
    const std::string code = read_repo_file("include/tlm/cep/collective_plane.hh");
    REQUIRE(!code.empty());
    const std::string stripped = strip_comments(code);

    // 本文件**不得** include directory / ordering / sharer 侧的头
    for (const char* forbidden : {"sharer_tracker", "address_resolver", "ordering_point",
                                  "channel_dependency", "stub_cache_core"}) {
        CHECK(stripped.find(std::string("#include") + " \"tlm/cep/" + forbidden) ==
              std::string::npos);
    }

    // ── 反向 fixture:构造一份含 forbidden include 的文本 ⇒ 判据必须判红 ──
    const std::string poisoned = "#include \"tlm/cep/sharer_tracker.hh\"\n";
    CHECK(cep::header_include_is_forbidden(poisoned));

    // 对照:干净文本必须不判红(否则该判据恒真)
    const std::string clean = "#include \"tlm/cep/collective_plane.hh\"\n#include <map>\n";
    CHECK(!cep::header_include_is_forbidden(clean));

    // ── 反向 fixture ②:本行为面自己的扫描也必须能抓到 poisoned 文本 ──
    //   (此前反向 fixture 只打在判据面的函数上,行为面扫描**从未**被注入验证过)
    CHECK(poisoned.find("sharer_tracker") != std::string::npos);
    // 对照:clean 文本里不含禁用名
    CHECK(clean.find("sharer_tracker") == std::string::npos);

    // 另一条:本仓的 collective 头**不得** include `ordering_point_checker.hh`
    //   (那是 coherence 侧的判据头 —— 会被 CP-05 的完成域判别抓到)
    CHECK(stripped.find("ordering_point_checker") == std::string::npos);
}
