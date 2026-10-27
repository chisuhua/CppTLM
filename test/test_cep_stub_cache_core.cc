// test/test_cep_stub_cache_core.cc
// CEP Stub Cache Core 的可执行契约(delta capability `cep-stub-cache-core`)
//
// 功能/作者/日期:把 `openspec/changes/cpptlm-cep-stub-cache-core/specs/cep-stub-cache-core/spec.md`
//               的**每条 Scenario** 对上**可执行证据** / CppTLM build agent / 2026-10-25
//
// 覆盖的 Scenario(与 spec 一一对应):
//   1. 6 接口 SHALL 存在且命名逐字对齐 canonical 10 流
//   2. 5 态状态机 SHALL 可达
//   3. 接口面 SHALL **NOT** 暴露 CHI 七态
//   4. `INV-SC1` ~ `INV-SC6` SHALL 各至少有 1 条**行为**断言
//   5. 折叠映射表 SHALL 呈**单向形态**
//   6. hit 路径 SHALL 零网络流量
//   7. 不可验证条款 SHALL 被降级登记且**不进**本 Requirement
//   8. 本 change SHALL **NOT** 引入协议实例声明
//   9. stub SHALL **NOT** 注册 23 ABI 且 SHALL **NOT** 进 `ModuleFactory`
//
// **判据纪律**(per 队列 #2 critic 7 轮提炼的六类「有牙」缺口):
//   ① 断言「某常量存在」永远不够 —— 必须断言「**输入改变时输出必须改变**」
//   ② 判据的**文档承诺**须与**实测覆盖**逐形态对齐
//   ③ **纯负向断言必须有正控**
//   ④ **夹具的判别属性必须自守**(不依赖布局 / 相邻关系)
//   ⑤ 守卫须与**它守护的那条命令**等价
//   ⑥ **误报与漏报同为缺陷**
//
// 红绿分离:「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为
//          交付物登记。本文件不含「结论可能为红」的判据(那些属 S1 / S3,见队列 #2)。
//
// 运行:ctest -R cep_stub_cache_core
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"
#include "tlm/cep/stub_cache_core.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using tlm::cep::AccessResult;
using tlm::cep::LineState;
using tlm::cep::Perm;
using tlm::cep::RequestOut;
using tlm::cep::SnoopResp;
using tlm::cep::SnoopType;
using tlm::cep::StubCacheCore;
using tlm::cep::WritebackOut;

namespace {

    constexpr const char* kStubHeader = "include/tlm/cep/stub_cache_core.hh";
    // ⚠️ **路径随归档迁移**(2026-10-25 队列 #3 归档):delta spec 从
    //   `openspec/changes/<name>/specs/…` 迁到 `openspec/changes/archive/<date>-<name>/specs/…`。
    //   ⇒ 断言须指向**归档后**的位置(否则守门因路径漂移而恒红 —— 判据⑤)。
    //   场景 7 同时扫归档件(delta 形态)与 main spec(合并后的契约形态)——
    //   两者是同一份内容,合并后 Scenario 数不变。
    constexpr const char* kStubSpec =
        "openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/specs/cep-stub-cache-core/"
        "spec.md";
    constexpr const char* kStubMainSpec = "openspec/specs/cep-stub-cache-core/spec.md";

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去掉 `//` 行注释与 `/* */` 块注释 —— **接口面**的静态扫描必须在去注释后做,
    /// 否则头文件里解释「折叠表」的散文会把被禁流名喂进来(假阳性),
    /// 而把禁用词写进注释里又能绕过(假阴性)。
    /// ⚠️ 实现已上提到 `test/cep_adr_parser.hh`(**共享**),与
    ///    `test_cep_contracts_crosscheck.cc` 的协议标识符负向断言用**同一份** ——
    ///    两份实现分叉会静默改变断言语义(per ADR-CEP-14 §2 决策 3 C9)。
    using cpptlm::cep::strip_comments;

    bool contains_word(const std::string& hay, const std::string& word) {
        return std::regex_search(hay, std::regex("\\b" + word + "\\b"));
    }

    /// 一个最小的 **mock ProtocolEngine**:只实现 net_side 的驱动,不知道 TG 存在。
    struct MockProtocolEngine {
        StubCacheCore* stub = nullptr;

        /// 完成一个 outstanding 请求(数据来源对 stub 不可分辨)
        void complete(uint64_t addr, Perm perm, uint64_t data = 0xCAFEull) {
            stub->completion(addr, stub->outstanding_txn(addr), data, perm, false);
        }
        /// 取一个已到期的 snoop 响应
        bool take_snoop_resp(SnoopResp& out) {
            return stub->snoop_resp(out);
        }
    };

} // namespace

// ═════════════════════════════════════════════════════════════════════
// Scenario 1:6 接口 SHALL 存在且命名逐字对齐 canonical 10 流
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 6 接口签名与命名(逐字对齐 canonical 10 流)",
          "[cep][stub-cache-core][interfaces]") {
    StubCacheCore stub;

    // net-side 5 条 CohIf RN 投影 —— 逐条**调用**以证明签名可编译(不是「名字存在」)
    SECTION("流 1 request(addr, perm) —— 发起") {
        const RequestOut r = stub.request(0x1000, Perm::READ);
        CHECK(r.accepted);
        CHECK(r.addr == 0x1000);
        CHECK(r.perm == Perm::READ);
        CHECK(r.txn_id != 0);
    }
    SECTION("流 2 completion(addr, txn_id, data, perm, error) —— 消费") {
        const RequestOut r = stub.request(0x2000, Perm::UNIQUE);
        const bool ok = stub.completion(0x2000, r.txn_id, 0xABCD, Perm::UNIQUE, false);
        CHECK(ok);
        CHECK(stub.state_of(0x2000) == LineState::E);
    }
    SECTION("流 3 snoop(addr, type, txn_id) —— 消费") {
        CHECK(stub.snoop(0x3000, SnoopType::INVALIDATE, 7));
        CHECK(stub.counters().net_snoops == 1);
    }
    SECTION("流 4 snoop_resp(out) —— 发起(延迟后可得)") {
        stub.snoop(0x3000, SnoopType::INVALIDATE, 7);
        SnoopResp resp;
        CHECK_FALSE(stub.snoop_resp(resp)); // 未到期
        stub.tick(stub.config().snoop_latency_cycles);
        REQUIRE(stub.snoop_resp(resp));
        CHECK(resp.txn_id == 7);
    }
    SECTION("流 5 writeback(addr, data, dirty) —— 发起") {
        const WritebackOut w = stub.writeback(0x4000, 0x1234, true);
        CHECK(w.valid);
        CHECK(w.addr == 0x4000);
        CHECK(w.dirty);
    }
    SECTION("core-side 原语 evict_line(addr)") {
        // 先建立一条脏行,再驱逐
        const RequestOut r = stub.request(0x5000, Perm::WRITEBACK);
        stub.completion(0x5000, r.txn_id, 0x55, Perm::WRITEBACK, false);
        const WritebackOut w = stub.evict_line(0x5000);
        CHECK(w.valid);
        CHECK_FALSE(stub.has_line(0x5000)); // 驱逐生效
    }

    SECTION("命名:接口面不得出现初版旧名(去注释后核对)") {
        const std::string code = strip_comments(read_repo_file(kStubHeader));
        REQUIRE_FALSE(code.empty());
        CHECK_FALSE(contains_word(code, "request_line"));
        CHECK_FALSE(contains_word(code, "snoop_query"));
        CHECK_FALSE(contains_word(code, "complete")); // 修订为 completion
    }
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 2:5 态状态机 SHALL 可达
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 5 态可达(NONE/I/S/E/M 各存在一条抵达路径)",
          "[cep][stub-cache-core][state-machine]") {
    CHECK(StubCacheCore::kStateCount == 5);

    // NONE:未缓存
    {
        StubCacheCore s;
        CHECK(s.state_of(0x10) == LineState::NONE);
    }
    // I:completion 带 error → I(且行存在)
    {
        StubCacheCore s;
        const auto r = s.request(0x10, Perm::READ);
        REQUIRE(s.completion(0x10, r.txn_id, 0, Perm::READ, /*error=*/true));
        CHECK(s.state_of(0x10) == LineState::I);
    }
    // S:READ 权限完成
    {
        StubCacheCore s;
        const auto r = s.request(0x10, Perm::READ);
        REQUIRE(s.completion(0x10, r.txn_id, 1, Perm::READ, false));
        CHECK(s.state_of(0x10) == LineState::S);
    }
    // E:UNIQUE 权限完成
    {
        StubCacheCore s;
        const auto r = s.request(0x10, Perm::UNIQUE);
        REQUIRE(s.completion(0x10, r.txn_id, 1, Perm::UNIQUE, false));
        CHECK(s.state_of(0x10) == LineState::E);
    }
    // M:WRITEBACK 权限完成
    {
        StubCacheCore s;
        const auto r = s.request(0x10, Perm::WRITEBACK);
        REQUIRE(s.completion(0x10, r.txn_id, 1, Perm::WRITEBACK, false));
        CHECK(s.state_of(0x10) == LineState::M);
    }

    // 5 态**去重**后恰好 5 个不同值(防「枚举有 6 个值」这类漂移)
    const std::vector<LineState> all = {LineState::NONE, LineState::I, LineState::S, LineState::E,
                                        LineState::M};
    auto uniq = all;
    std::sort(uniq.begin(), uniq.end());
    CHECK(std::adjacent_find(uniq.begin(), uniq.end()) == uniq.end());
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 3:接口面 SHALL NOT 暴露 CHI 七态
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 接口面不暴露 CHI RN-F 细节态", "[cep][stub-cache-core][state-machine]") {
    const std::string code = strip_comments(read_repo_file(kStubHeader));
    REQUIRE_FALSE(code.empty());
    // 4 个 RN-F 细节态名不得作为**标识符**出现(去注释后)
    for (const char* bad : {"UC", "SC", "UD", "SD"})
        CHECK_FALSE(contains_word(code, bad));

    // **正控**(判据③):同样机制必须能发现确实存在的标识符 ——
    //   否则「都没找到」可能只是扫描本身失效(无判别力)。
    CHECK(contains_word(code, "LineState"));
    CHECK(contains_word(code, "StubCacheCore"));
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 4:`INV-SC1` ~ `INV-SC6` 各至少有 1 条**行为**断言
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: INV-SC1 同一 addr 最多 1 个 outstanding request",
          "[cep][stub-cache-core][inv-sc]") {
    StubCacheCore stub;

    const auto first = stub.request(0x100, Perm::READ);
    REQUIRE(first.accepted);
    CHECK(stub.outstanding_count() == 1);

    // **输入改变 → 输出必须改变**:同 addr 第二次 request 必须被拒
    const auto dup = stub.request(0x100, Perm::READ);
    CHECK_FALSE(dup.accepted);
    CHECK(dup.txn_id == 0);
    CHECK(stub.outstanding_count() == 1); // 未增
    CHECK(stub.counters().mshr_rejects == 1);

    // 正控:不同 addr 必须被接受(证明拒绝是「同 addr」而非「一律拒绝」)
    const auto other = stub.request(0x200, Perm::READ);
    CHECK(other.accepted);
    CHECK(stub.outstanding_count() == 2);

    // MSHR 容量约束(第二重守卫)
    StubCacheCore tiny(StubCacheCore::Config{/*max_mshr=*/2, 2});
    CHECK(tiny.request(0x1, Perm::READ).accepted);
    CHECK(tiny.request(0x2, Perm::READ).accepted);
    CHECK_FALSE(tiny.request(0x3, Perm::READ).accepted); // 满
    // 正控:释放一个 slot 后必须又能接受
    tiny.completion(0x1, tiny.outstanding_txn(0x1), 0, Perm::READ, false);
    CHECK(tiny.request(0x3, Perm::READ).accepted);
}

TEST_CASE("CEP Stub: INV-SC2 evict 不选取 outstanding line", "[cep][stub-cache-core][inv-sc]") {
    StubCacheCore stub;
    // 建一条脏行
    const auto r = stub.request(0x300, Perm::WRITEBACK);
    REQUIRE(stub.completion(0x300, r.txn_id, 0xDEAD, Perm::WRITEBACK, false));
    REQUIRE(stub.state_of(0x300) == LineState::M);

    // 制造 outstanding:再发起一个请求占用 MSHR
    const auto inflight = stub.request(0x300, Perm::READ);
    REQUIRE(inflight.accepted);
    REQUIRE(stub.is_outstanding(0x300));

    // **行为**:evict 必须被拒 —— 且 line **保留**(状态不变)
    const WritebackOut w = stub.evict_line(0x300);
    CHECK_FALSE(w.valid);                        // 不写回
    CHECK(stub.has_line(0x300));                 // 行仍在
    CHECK(stub.state_of(0x300) == LineState::M); // 状态未被破坏
    CHECK(stub.counters().evict_skipped == 1);

    // **正控**(判据③):清掉 outstanding 后,同地址 evict **必须成功**
    stub.completion(0x300, inflight.txn_id, 0xDEAD, Perm::WRITEBACK, false);
    REQUIRE_FALSE(stub.is_outstanding(0x300));
    const WritebackOut w2 = stub.evict_line(0x300);
    CHECK(w2.valid);
    CHECK_FALSE(stub.has_line(0x300));
    CHECK(stub.counters().evict_skipped == 1); // 未增
}

TEST_CASE("CEP Stub: INV-SC3 snoop 响应在参数化固定延迟后返回", "[cep][stub-cache-core][inv-sc]") {
    // 延迟**参数化**:三个不同延迟的实例必须各自遵守自己的值(输入→输出)
    for (unsigned lat : {0u, 1u, 3u}) {
        StubCacheCore stub(StubCacheCore::Config{16, lat});
        stub.snoop(0x400, SnoopType::INVALIDATE, 11);

        SnoopResp resp;
        // **逐拍**核对:到期前的每一拍都**必须**不可得(不是只查一个采样点)
        for (unsigned t = 0; t < lat; ++t) {
            CHECK_FALSE(stub.snoop_resp(resp));
            CHECK(resp.txn_id == 0); // 未写入
            CHECK(stub.pending_snoop_resp_count() == 1);
            stub.tick(1);
        }
        REQUIRE(stub.snoop_resp(resp)); // 恰好到期
        CHECK(resp.txn_id == 11);
        CHECK(resp.addr == 0x400);
        CHECK(stub.pending_snoop_resp_count() == 0);
        // 取空后再取 → 必须为 false(不重复交付)
        CHECK_FALSE(stub.snoop_resp(resp));
    }
}

TEST_CASE("CEP Stub: INV-SC4 completion 与 request 严格配对(无 unsolicited)",
          "[cep][stub-cache-core][inv-sc]") {
    StubCacheCore stub;
    const uint64_t bad0 = stub.counters().unsolicited_completions;

    // **无配对**的 completion 必须被拒(且状态**不**变)
    CHECK_FALSE(stub.completion(0x500, /*txn_id=*/999, 0x1, Perm::READ, false));
    CHECK(stub.state_of(0x500) == LineState::NONE);
    CHECK(stub.counters().unsolicited_completions == bad0 + 1); // 增量断言
    CHECK(stub.counters().net_completions == 0);

    // txn_id 不匹配也必须被拒(不只是「有 outstanding」就算配对)
    const auto r = stub.request(0x500, Perm::READ);
    REQUIRE(r.accepted);
    const uint64_t ok0 = stub.counters().net_completions;
    CHECK_FALSE(stub.completion(0x500, r.txn_id + 100, 0x1, Perm::READ, false));
    CHECK(stub.is_outstanding(0x500));             // 请求仍在途
    CHECK(stub.counters().net_completions == ok0); // 未消费
    CHECK(stub.counters().unsolicited_completions == bad0 + 2);

    // **正控**(判据③):正确 txn_id 必须被接受
    CHECK(stub.completion(0x500, r.txn_id, 0x1, Perm::READ, false));
    CHECK(stub.counters().net_completions == ok0 + 1);
    CHECK_FALSE(stub.is_outstanding(0x500));

    // 同一 completion 重复投递(已消费)→ 必须被拒
    CHECK_FALSE(stub.completion(0x500, r.txn_id, 0x1, Perm::READ, false));
    CHECK(stub.counters().unsolicited_completions == bad0 + 3);
}

TEST_CASE("CEP Stub: INV-SC5 dirty writeback 带 data;clean 不带",
          "[cep][stub-cache-core][inv-sc]") {
    StubCacheCore stub;

    // **输入改变 → 输出必须改变**:dirty 参数翻转 ⇒ 载荷与携带标记同时翻转
    const WritebackOut dirty = stub.writeback(0x600, 0xF00D, true);
    CHECK(dirty.dirty);
    CHECK(dirty.carries_data);
    CHECK(dirty.data == 0xF00D); // dirty 带 data

    const WritebackOut clean = stub.writeback(0x600, 0xF00D, false);
    CHECK_FALSE(clean.dirty);
    CHECK_FALSE(clean.carries_data);
    CHECK(clean.data == 0); // clean **不**带 data(同一入参,载荷不同)

    // 恒等式在**驱逐路径**上同样成立(不是只在直接调用上成立)
    StubCacheCore s2;
    const auto rm = s2.request(0x700, Perm::WRITEBACK);
    REQUIRE(s2.completion(0x700, rm.txn_id, 0xBEEF, Perm::WRITEBACK, false));
    const WritebackOut ev_m = s2.evict_line(0x700);
    CHECK(ev_m.carries_data == ev_m.dirty);
    CHECK(ev_m.data == 0xBEEF);

    StubCacheCore s3;
    const auto rs = s3.request(0x701, Perm::READ);
    REQUIRE(s3.completion(0x701, rs.txn_id, 0xBEEF, Perm::READ, false));
    const WritebackOut ev_s = s3.evict_line(0x701);
    CHECK(ev_s.valid);
    CHECK(ev_s.carries_data == ev_s.dirty);
    CHECK_FALSE(ev_s.carries_data);
    CHECK(ev_s.data == 0); // S 态驱逐:clean 写回,不带 data
}

TEST_CASE("CEP Stub: INV-SC6 hit 路径不产生网络流量", "[cep][stub-cache-core][inv-sc]") {
    StubCacheCore stub;
    MockProtocolEngine pe{&stub};

    // 先 miss 一次把行填进来
    const AccessResult miss = stub.cpu_read(0x800);
    REQUIRE(miss.pending);
    CHECK(stub.counters().net_requests == 1);
    pe.complete(0x800, Perm::READ, 0x1234);

    // **行为断言**:hit 前后的 net-side 动作计数**必须完全相同**
    const auto before = stub.counters();
    const AccessResult hit = stub.cpu_read(0x800);
    CHECK(hit.hit);
    CHECK(hit.data == 0x1234);
    const auto after = stub.counters();
    CHECK(after.net_requests == before.net_requests);
    CHECK(after.net_writebacks == before.net_writebacks);
    CHECK(after.net_snoop_resps == before.net_snoop_resps);
    CHECK(after.net_snoops == before.net_snoops);
    CHECK(after.net_completions == before.net_completions);

    // **正控**(判据③):miss 路径**必须**产生 net-side request ——
    //   否则上面的「不增」可能只是「计数从不增加」(无判别力)。
    const AccessResult miss2 = stub.cpu_read(0x900);
    CHECK(miss2.pending);
    CHECK(stub.counters().net_requests == before.net_requests + 1);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 5:折叠映射表 SHALL 呈单向形态
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 折叠映射表单向形态(5 隐藏流名不得作接口标识符)",
          "[cep][stub-cache-core][folding]") {
    const std::string code = strip_comments(read_repo_file(kStubHeader));
    REQUIRE_FALSE(code.empty());

    // 5 条隐藏流(canonical 6/7/8/9/10)不得作为本类的接口标识符
    for (const char* hidden :
         {"forward_data", "back_invalidate", "address_resolve", "barrier", "snoop_completion_ack"})
        CHECK_FALSE(contains_word(code, hidden));

    // **正控**(判据③):5 条**投影**流名必须在场 ——
    //   否则「隐藏流都没找到」可能只是扫描失效。
    for (const char* shown : {"request", "completion", "snoop", "snoop_resp", "writeback"})
        CHECK(contains_word(code, shown));

    // 流 7 的**反向量**以普通 invalidate 表现(per 决策 3)
    StubCacheCore stub;
    const auto r = stub.request(0x1000, Perm::WRITEBACK);
    REQUIRE(stub.completion(0x1000, r.txn_id, 0x77, Perm::WRITEBACK, false));
    // 反向失效 = 普通 INVALIDATE snoop(无协议特异入口)
    CHECK(stub.snoop(0x1000, SnoopType::INVALIDATE, 5));
    CHECK(stub.state_of(0x1000) == LineState::I);
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 6:hit 路径 SHALL 零网络流量(见 INV-SC6 用例;此处补端到端形态)
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: hit 路径零网络流量(端到端:读→完成→重复读)",
          "[cep][stub-cache-core][folding]") {
    StubCacheCore stub;
    MockProtocolEngine pe{&stub};

    // miss → 完成 → 连续 10 次 hit
    REQUIRE(stub.cpu_read(0xA00).pending);
    pe.complete(0xA00, Perm::READ, 0x9999);
    const uint64_t base = stub.counters().net_requests;
    for (int i = 0; i < 10; ++i) {
        const AccessResult a = stub.cpu_read(0xA00);
        REQUIRE(a.hit);
        CHECK(a.data == 0x9999);
    }
    CHECK(stub.counters().net_requests == base); // 10 次 hit,零新增流量
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 7:不可验证条款 SHALL 被降级登记且不进本 Requirement
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 不可验证条款(#1/#4)降级登记且不进 spec", "[cep][stub-cache-core][scope]") {
    const std::string spec = read_repo_file(kStubSpec);
    REQUIRE_FALSE(spec.empty());
    const std::string main_spec = read_repo_file(kStubMainSpec);
    REQUIRE_FALSE(main_spec.empty());

    // 规格:spec 里这两个词**只允许**以「**排除**语境」出现(如「SHALL **不**出现 `barrier`」
    //   或「#1/#4 SHALL **不**出现在本 Requirement」)。
    //   ⇒ 真正的判据是:**没有**一条 `#### Scenario:` 把 #1/#4 立为**要求**。
    //   (若只断言「spec 不含这些词」→ 会误报 —— 排除语境本身就是合法用法,
    //    判据⑥:误报与漏报同为缺陷。)
    std::vector<std::string> titles;
    {
        std::istringstream is(spec);
        std::string line;
        while (std::getline(is, line))
            if (line.rfind("#### Scenario:", 0) == 0)
                titles.push_back(line);
    }
    // **正控**:Scenario 抽取机制必须真的抽到条目(否则下面的循环空转 = 无判别力)
    REQUIRE(titles.size() >= 9);

    for (const auto& tt : titles) {
        CHECK(tt.find("forward_data") == std::string::npos); // 不得立为 Scenario 主题
        CHECK(tt.find("barrier") == std::string::npos);
    }
    // 降级登记**必须在场**(不是「删掉了事」):#1 与 #4 都点名「不进本 Requirement」
    CHECK(spec.find("契约测试 #1") != std::string::npos);
    CHECK(spec.find("#4(") != std::string::npos); // spec 原文「与 #4(barrier 不可见)」
    CHECK(spec.find("**不**出现在本 Requirement") != std::string::npos);

    // **正控**:降级登记确实存在于 design.md(不是「删掉了事」)
    const std::string design =
        read_repo_file("openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/design.md");
    REQUIRE_FALSE(design.empty());
    CHECK(design.find("队列 #6") != std::string::npos); // 触发条件登记

    // **main spec 同样不得**把 #1/#4 立为要求(合并后的契约面是同一份内容)
    {
        std::vector<std::string> main_titles;
        {
            std::istringstream is(main_spec);
            std::string line;
            while (std::getline(is, line))
                if (line.rfind("#### Scenario:", 0) == 0)
                    main_titles.push_back(line);
        }
        REQUIRE(main_titles.size() >= 9); // 合并后 Scenario 数不变
        for (const auto& tt : main_titles) {
            CHECK(tt.find("forward_data") == std::string::npos);
            CHECK(tt.find("barrier") == std::string::npos);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 8:本 change SHALL NOT 引入协议实例声明
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 不引入 status != planned 的协议实例声明", "[cep][stub-cache-core][scope]") {
    namespace fs = std::filesystem;
    const fs::path root = CPPTLM_SOURCE_DIR;

    // 扫描本 change 的归档件目录,查找**声明文件**
    //   (协议实例声明的载体 = 含 "protocol_instance" 的 JSON)
    // ⚠️ **扫描集刻意不覆盖 `include/tlm/cep/`**——队列 #4 起,同目录下会出现
    //   **其它 change** 的协议实例声明(per 2026-10-25 队列 #4 Entry #62:
    //   `include/tlm/cep/protocol_instances.json` status=planned)。本断言守护的是
    //   **队列 #3 的 change 不引入**,scope 超出 change 的产物必然误报(判据⑥)。
    std::vector<std::string> decl_files;
    const std::vector<fs::path> roots = {
        root / "openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core",
    };
    for (const auto& r : roots) {
        if (!fs::exists(r))
            continue;
        for (const auto& e : fs::recursive_directory_iterator(r)) {
            if (!e.is_regular_file())
                continue;
            const std::string name = e.path().filename().string();
            // schema 文件是**结构定义**,不是实例声明 —— 排除
            if (name.find("protocol_instance") != std::string::npos &&
                name.find("schema") == std::string::npos)
                decl_files.push_back(e.path().string());
        }
    }

    // **行为断言**:本 change 产物中**不存在**任何协议实例声明文件
    //   ⇒ 也就不会出现 `status != "planned"` 的声明(per ADR-CEP-12 INV-CEP-12-IC3)。
    CHECK(decl_files.empty());

    // **更强的判据**:即便将来有人放了声明文件,其 `status` 也**必须**是 `planned`
    //   —— `status == "implemented"` 是 S1-2 / S1-3 的触发条件
    //   (per `ADR-CEP-12` `INV-CEP-12-IC3`);stub cache **不是**协议实例。
    for (const auto& f : decl_files) {
        std::string body = read_repo_file(f.substr(std::string(CPPTLM_SOURCE_DIR).size() + 1));
        INFO("协议实例声明文件: " << f);
        // 只允许 `planned`;出现任何其它 status 值即判红
        std::smatch m;
        const std::regex status_re("\"status\"\\s*:\\s*\"([^\"]+)\"");
        while (std::regex_search(body, m, status_re)) {
            CHECK(m[1].str() == "planned");
            body = m.suffix().str();
        }
    }

    // **正控**(判据③):扫描机制必须能发现**存在的**文件 ——
    //   否则「没找到」可能只是遍历失效。用同一遍历核对一个必然存在的文件。
    bool found_known = false;
    const fs::path cep_dir = root / "include/tlm/cep";
    if (fs::exists(cep_dir)) {
        for (const auto& e : fs::recursive_directory_iterator(cep_dir))
            if (e.is_regular_file() && e.path().filename() == "fabric.json")
                found_known = true;
    }
    CHECK(found_known);

    // **正控 ②**:status 抽取机制本身必须有效 ——
    //   对一段**已知含非 planned status** 的文本,该机制必须抽得到。
    {
        std::string probe = R"({"status": "implemented", "protocol_version": "x"})";
        // raw string 内**不**转义引号 —— 上面的正则用普通字符串,此处需与之匹配
        std::smatch m;
        const std::regex status_re("\"status\"\\s*:\\s*\"([^\"]+)\"");
        REQUIRE(std::regex_search(probe, m, status_re));
        CHECK(m[1].str() == "implemented"); // 抽得到,且**不是** planned
    }
}

// ═════════════════════════════════════════════════════════════════════
// Scenario 9:stub SHALL NOT 注册 23 ABI 且 SHALL NOT 进 ModuleFactory
// ═════════════════════════════════════════════════════════════════════
TEST_CASE("CEP Stub: 不注册 23 ABI / 不进 ModuleFactory", "[cep][stub-cache-core][scope]") {
    const std::string code = strip_comments(read_repo_file(kStubHeader));
    REQUIRE_FALSE(code.empty());

    // ① 不注册:头文件不得出现注册宏 / 工厂入口
    CHECK(code.find("REGISTER_CHSTREAM") == std::string::npos);
    CHECK(code.find("REGISTER_MODULE") == std::string::npos);
    CHECK(code.find("REGISTER_OBJECT") == std::string::npos);
    CHECK(code.find("registerObject") == std::string::npos);
    CHECK(code.find("registerModule") == std::string::npos);

    // ② 不引 ABI 头
    CHECK(code.find("abi/") == std::string::npos);
    CHECK(code.find("cpptlm_emulator") == std::string::npos);

    // **正控**(判据③):同样的机制必须能发现**确实存在**的宏 ——
    //   用 include/modules_cluster.hh(必然含 REGISTER_MODULE)核对扫描有效。
    const std::string reg = strip_comments(read_repo_file("include/modules_cluster.hh"));
    REQUIRE_FALSE(reg.empty());
    CHECK(reg.find("REGISTER_MODULE") != std::string::npos);

    // ③ 不进 ModuleFactory:全仓注册表不得含本类名
    CHECK(reg.find("StubCacheCore") == std::string::npos);
}
