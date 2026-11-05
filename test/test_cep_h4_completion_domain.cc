// test/test_cep_h4_completion_domain.cc
// H4「完成域判别规则(requester-scoped vs group-scoped)足以同时容纳 coherence + collective」
//
// 正控:coherence 流 = requester-scoped(单点 ack)+ collective 流 = group-scoped(全组到达)
//       ⇒ 两类流在**同一实验内**共存 ⇒ **无死锁**
// 负控:coherence 流**错误标记为 group-scoped**(完成条件被绑定到 group rendezvous)
//       ⇒ **死锁发生**
//
// ⚠️ 负控是**装置合格的前提**(per 台账 Entry #222 ③):只做正控 ⇒ 本测试退化为 H6 换名。
//   ⛔ 负控不是「coherence 也 blocking」—— 那是「更多 blocking 更易死锁」的废话(h6 领域)。
//
// ⚠️ 口径限定(必须随任何引用重复):非完整仲裁器(无 credit 环流/多级仲裁/跨 VC 依赖图;
//   完整仲裁属 Stage 4+,per ADR-CEP-09)· `ADR-CEP-09` SU 2026-11-02 §④ 的
//   「`GrantAck` 可能被 `Grant` 饿死未建模」本测试**不覆盖** · 非协议级证明。
//
// 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #222)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "cep_adr_parser.hh"
#include "cep_completion_domain_model.hh"

namespace {

    using cep_cd::CompletionDomain;
    using cep_cd::CompletionDomainConfig;
    using cep_cd::CompletionDomainModel;

    /// 跑一个场景:注入 n 条 coherence + n 条 collective,推进 `cycles` 周期。
    /// `coh_domain` = coherence 流的完成域标签(正控 = RequesterScoped;负控 = GroupScoped)。
    struct RunResult {
        bool deadlocked = false;
        std::uint64_t stagnant_cycles = 0;
        std::uint64_t coherence_retired = 0;
        std::uint64_t collective_retired = 0;
        std::uint64_t coherence_stalls = 0;
        std::uint64_t collective_stalls = 0;
        std::uint64_t cycles = 0;
        bool domain_confusion = false;
    };

    RunResult run_scenario(CompletionDomain coh_domain, std::uint64_t n, std::uint64_t cycles) {
        CompletionDomainConfig cfg;
        cfg.group_size = 3;
        cfg.deadlock_threshold = 200;
        CompletionDomainModel m(cfg);

        for (std::uint64_t i = 0; i < n; ++i) {
            m.inject_coherence(coh_domain, 100 + i, 1);
            m.inject_collective(200 + i);
        }

        for (std::uint64_t c = 0; c < cycles; ++c)
            m.tick();

        RunResult r;
        r.deadlocked = m.deadlocked();
        r.stagnant_cycles = m.stagnant_cycles();
        r.coherence_retired = m.coherence_retired();
        r.collective_retired = m.collective_retired();
        r.coherence_stalls = m.coherence_stalls();
        r.collective_stalls = m.collective_stalls();
        r.cycles = m.cycle();
        r.domain_confusion = m.has_completion_domain_confusion();
        return r;
    }

} // namespace

TEST_CASE("H4 正控:coherence=requester-scoped + collective=group-scoped 同实验内**无死锁**",
          "[cep][h4][completion-domain]") {
    // ⚠️ 回归锁(per AC-9 评审 S2):`collective_requires_coherence_retirement` 默认 `true`
    // 是本装置「正控无死锁」结论的**行为前提**;若被人改成 `false`,正控仍会绿但语义已变
    // (collective 不再等待 coherence ⇒ 跨平面阻塞消失)⇒ 结论静默失效。故在此显式钉死默认值。
    REQUIRE(CompletionDomainConfig{}.collective_requires_coherence_retirement == true);

    // ✅ 判据正面:两类流共存 ⇒ 全部完成 ⇒ 无死锁
    const auto r = run_scenario(CompletionDomain::RequesterScoped, 8, 400);

    INFO("正控不得死锁(requester-scoped 的单点 ack 不受 group rendezvous 牵连)");
    REQUIRE_FALSE(r.deadlocked);
    INFO("正控的 coherence 请求应全部完成: " << r.coherence_retired);
    REQUIRE(r.coherence_retired == 8);
    INFO("正控的 collective 请求应全部完成: " << r.collective_retired);
    REQUIRE(r.collective_retired == 8);
    INFO("正控不应出现完成语义混淆");
    REQUIRE_FALSE(r.domain_confusion);
}

TEST_CASE("H4 负控:coherence 被**错误标记为 group-scoped** ⇒ **死锁发生**",
          "[cep][h4][completion-domain]") {
    // ⚠️ 这是**装置合格的前提**(per Entry #222 ③):只做正控 ⇒ 本测试退化为 H6 换名。
    //   负控证明「完成纪律混用 ⇒ 静默死锁」(per ADR-CEP-01 §6.2 代价 3)。
    const auto r = run_scenario(CompletionDomain::GroupScoped, 8, 400);

    INFO("负控必须死锁:否则装置没建模「完成语义混淆 → 死锁」机制");
    REQUIRE(r.deadlocked);
    INFO("负控应出现完成语义混淆");
    REQUIRE(r.domain_confusion);
    INFO("负控下 coherence 请求不得全部完成: " << r.coherence_retired);
    REQUIRE(r.coherence_retired < 8);
}

TEST_CASE("H4 负控的死锁**不是带宽竞争**(与 H6 的共享 VC 饿死区分)",
          "[cep][h4][completion-domain]") {
    // ⚠️ 关键区分:负控的成因是**完成语义混淆**,不是「更多 blocking 更易死锁」。
    //   ⇒ 判据:在**隔离 VC**下(带宽充足)负控仍死锁 ⇒ 排除带宽竞争解释。
    //   ⇒ 且 collective 侧(group-scoped,语义正确)**不受影响** ⇒ 死锁定位在 coherence 侧。
    const auto r = run_scenario(CompletionDomain::GroupScoped, 8, 400);

    // ⚠️ **循环等待的两端都不动** —— collective 的**到达**依赖 coherence 退休,
    //   而 coherence 的**完成**又依赖 collective 到达 ⇒ 负控下**两端都完成不了**。
    //   ⇒ 这是死锁的**直接证据**,不是「只有 coherence 卡住」。
    INFO("负控下 collective 也完成不了(循环等待): " << r.collective_retired);
    REQUIRE(r.collective_retired == 0);
    INFO("负控下 coherence 同样完成不了: " << r.coherence_retired);
    REQUIRE(r.coherence_retired == 0);
    // ⚠️ 关键区分:**隔离 VC**(带宽充足)下仍死锁 ⇒ 排除「带宽竞争」解释
    //   (H6 的共享 VC 饿死是带宽问题;本例是完成语义问题)
    INFO("隔离 VC 下 collective_stalls 应为 0(非带宽竞争): " << r.collective_stalls);
    REQUIRE(r.collective_stalls == 0);
}

TEST_CASE("H4 装置**合格性判定**:负控必须死锁(per ADR-CEP-06 SU#3 §②B(i))",
          "[cep][h4][completion-domain]") {
    // ⚠️ 这是**产出层硬约束**的判定逻辑(per ADR-CEP-06 SU#3 §②B(i)):
    //   「负控红不了 ⇒ 装置不合格 ⇒ ⛔ 不产出 run、不登记结论」
    // ⇒ 判定逻辑必须**可机器测** —— ⛔ 不可只活在 emit 的控制流里
    //   (那会变成「纯承诺 = 测试给缺口盖章」,per C1 根因教训)。
    INFO("负控死锁 ⇒ 装置合格");
    REQUIRE(cep_cd::device_qualified(true));
    INFO("负控未死锁 ⇒ 装置不合格 ⇒ emit 必须拒绝产出");
    REQUIRE_FALSE(cep_cd::device_qualified(false));
}

TEST_CASE("H4 装置口径限定随结论重复(per Entry #222 ③)", "[cep][h4][completion-domain]") {
    // ⚠️ 口径限定必须**可机器查到**(不靠散文记忆)—— 校验 ADR 侧的三条登记义务仍在案。
    const auto adr09 =
        cpptlm::cep::read_file((cpptlm::cep::repo_root() / "docs" / "adr" /
                                "ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md")
                                   .string());
    REQUIRE_FALSE(adr09.empty());
    // 「GrantAck 可能被 Grant 饿死未建模」的登记义务须仍在 ADR 内(本装置不覆盖它)
    REQUIRE(adr09.find("GrantAck") != std::string::npos);
    // ADR-CEP-01 §3.2 的两套完成纪律 + §6.2 代价 3 的「分类错误 = 静默死锁」须在案
    const auto adr01 = cpptlm::cep::read_file(
        (cpptlm::cep::repo_root() / "docs" / "adr" / "ADR-CEP-01-fabric-collective-plane.md")
            .string());
    REQUIRE_FALSE(adr01.empty());
    REQUIRE(adr01.find("group-scoped") != std::string::npos);
    REQUIRE(adr01.find("静默死锁") != std::string::npos);
}
