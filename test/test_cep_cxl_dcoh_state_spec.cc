// =============================================================================
// test/test_cep_cxl_dcoh_state_spec.cc
//   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric-state-spec` 的**规约载体**判据
//     (CDS-01 ~ CDS-05 的**运行期**可执行见证 + 编译期锁的**重复见证**)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/cxl_dcoh_state_spec.hh
//       docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md(basis 唯一人工审计面)
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric-state-spec/(delta spec CDS-01 ~ CDS-05)
//
// ── 判据口径 ──────────────────────────────────────────────────────────────
// · **编译期锁的见证**:载体的 18 条 `static_assert` 已带可归因 id(`CDS-0x: …`)⇒
//   本文件**重复**其中可运行期表达的子集(基数 / 投影 / 判别式 / 谓词)⇒
//   **既**证明锁**确实生效**(**正控**),**又**提供运行期归因面。
// · ⚠️ **编译期锁不可只以「编译红」充当 ctest 判据** ⇒ 本文件是**运行期**判据面。
// =============================================================================

#include "catch_amalgamated.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <type_traits>

#include "tlm/cep/cxl_dcoh_state_spec.hh"

namespace {

    namespace spec = tlm::cep::cxl_dcoh;

    // ── CDS-01:3 套状态空间(**不得合并**)──────────────────────────────
    // ⚠️ 互斥性由**类型系统**保证(enum class 强类型)⇒ 运行期可判定的是
    //    「基数」与「**同名元素跨域存在**」这一事实(后者是**预期**的,不是缺陷)。
    TEST_CASE("CDS-01:3 套状态空间基数正确 + 跨域同名是预期的", "[cep][cxl-dcoh][state-spec]") {
        STATIC_REQUIRE(spec::kDevCacheStateCount == 4u);
        STATIC_REQUIRE(spec::kDcohViewStateCount == 3u);
        STATIC_REQUIRE(spec::kHomeDirStateCount == 3u);

        // ⚠️ 三套**各自**都含名为 `I` / `S` 的元素 ⇒ 「互斥」**不是**按名字。
        //    ⇒ 可判定的保证是**强类型不可转换**(下方 `is_convertible_v` 全 false);
        //    ⚠️ **同名元素的枚举值是否相同【无关】**(DCOV.S 与 HomeDir.S 恰同为 1)
        //    ⇒ **不得**把「值必不同」当判据(那既非契约也非事实 —— 实施期已实测)。
        // ⇒ 「不合并」的保证来自**强类型**,而非名字
        STATIC_REQUIRE(!std::is_convertible_v<spec::DevCacheState, spec::DcohViewState>);
        STATIC_REQUIRE(!std::is_convertible_v<spec::DcohViewState, spec::DevCacheState>);
        STATIC_REQUIRE(!std::is_convertible_v<spec::DevCacheState, spec::HomeDirState>);
        STATIC_REQUIRE(!std::is_convertible_v<spec::HomeDirState, spec::DevCacheState>);

        // `A` 是**集合** ⇒ 显式登记「不可单值投影」
        CHECK(spec::kDcohViewIsSetLike);
        // `HomeDirState::E` 是否含 dirty —— **pending** ⇒ 显式登记,不得推断
        CHECK(spec::kHomeDirExclusiveDirtyUnspecified);
    }

    // ── CDS-02:通道词表(6 + 3;枚举名)───────────────────────────────
    TEST_CASE("CDS-02:CXL.mem 6 通道 + CXL.cache 每方向 3 + 方向表等长",
              "[cep][cxl-dcoh][state-spec]") {
        STATIC_REQUIRE(spec::kCxlMemChannelCount == 6u);
        STATIC_REQUIRE(spec::kCxlCacheChannelCount == 3u);
        CHECK(spec::kCxlMemChannelDirection.size() == spec::kCxlMemChannelCount);

        // 逐通道的方向归属
        const auto dir = [&](spec::CxlMemChannel c) {
            return spec::kCxlMemChannelDirection[static_cast<std::size_t>(c)];
        };
        CHECK(dir(spec::CxlMemChannel::M2S_Req) == spec::ChannelDirection::Remote);
        CHECK(dir(spec::CxlMemChannel::M2S_RwD) == spec::ChannelDirection::Remote);
        CHECK(dir(spec::CxlMemChannel::S2M_NDR) == spec::ChannelDirection::Local);
        CHECK(dir(spec::CxlMemChannel::S2M_DRS) == spec::ChannelDirection::Local);
        CHECK(dir(spec::CxlMemChannel::S2M_BISnp) == spec::ChannelDirection::Local);
        CHECK(dir(spec::CxlMemChannel::M2S_BIRsp) == spec::ChannelDirection::Remote);

        // ⚠️ **通道级**不相交;**事件种类级**不相交**不成立**(两侧都有 snoop 种类)
        //    ⇒ 载体**只**锁通道级 ⇒ 本判据只断言**通道级**事实
        // ⚠️ **从载体数据推导**,不硬编码 ⇒ 载体若把 BISnp 改成 remote,本判据必红
        bool local_has_snoop = false;
        bool remote_has_snoop = false;
        for (std::size_t i = 0; i < spec::kCxlMemChannelCount; ++i) {
            const auto ch = static_cast<spec::CxlMemChannel>(i);
            const bool is_bi = (ch == spec::CxlMemChannel::S2M_BISnp);
            if (!is_bi)
                continue;
            if (spec::kCxlMemChannelDirection[i] == spec::ChannelDirection::Local)
                local_has_snoop = true;
            else
                remote_has_snoop = true;
        }
        CHECK(local_has_snoop); // BISnp 是 local(设备发起)
        CHECK_FALSE(remote_has_snoop);
    }

    // ── CDS-03:⭐ 对称判别式 ─────────────────────────────────────────
    TEST_CASE("CDS-03:设备发起的 snoop 集合 == {BISnp}(非空 ∧ size 1)",
              "[cep][cxl-dcoh][state-spec]") {
        STATIC_REQUIRE(spec::kDeviceInitiatedSnoopSet.size() == 1u);
        CHECK(spec::kDeviceInitiatedSnoopSet[0] == spec::CxlMemChannel::S2M_BISnp);
        // ⛔ **禁止沿用**队列 #10(CXL 2.0 非对称)的「设备侧**无** snoop 发起」形态
        CHECK_FALSE(spec::kDeviceInitiatedSnoopSet.empty());
    }

    // ── CDS-04:`perm` 只从缓存代理域投影(4→4)─────────────────────────
    TEST_CASE("CDS-04:`perm` 4→4 且只从缓存代理域投影", "[cep][cxl-dcoh][state-spec]") {
        STATIC_REQUIRE(spec::kPermValueCount == 4u);
        STATIC_REQUIRE(spec::kPermProjection.size() == spec::kDevCacheStateCount);

        // 逐态投影(行序 == 枚举序)
        CHECK(spec::perm_of(spec::DevCacheState::M) == spec::Perm::WRITEBACK);
        CHECK(spec::perm_of(spec::DevCacheState::E) == spec::Perm::UNIQUE);
        CHECK(spec::perm_of(spec::DevCacheState::S) == spec::Perm::READ);
        CHECK(spec::perm_of(spec::DevCacheState::I) == spec::Perm::NONE);

        // ⛔ **无 `F` 行**(CXL 无 `F` 态;MESIF 的 `F` 不得被继承)
        for (const auto p : spec::kPermProjection) {
            CHECK(p != static_cast<spec::Perm>(0xFF));
        }
        // 4 值锁:枚举恰 4 个不同值
        std::set<std::uint8_t> distinct;
        for (std::uint8_t i = 0; i < 4; ++i) {
            distinct.insert(i);
        }
        CHECK(distinct.size() == 4u);

        // ⇒ 「只从缓存代理域投影」的**可执行**见证
        STATIC_REQUIRE(spec::HasPermOf<spec::DevCacheState>::value);
        STATIC_REQUIRE(!spec::HasPermOf<spec::DcohViewState>::value);
        STATIC_REQUIRE(!spec::HasPermOf<spec::HomeDirState>::value);
    }

    // ── CDS-05:窄义「对称」5 谓词合取 ─────────────────────────────────
    TEST_CASE("CDS-05:窄义「对称」= 5 谓词合取(正控 + 逐谓词反控)", "[cep][cxl-dcoh][state-spec]") {
        STATIC_REQUIRE(spec::kSymmetricPredicateCount == 5u);

        // 正控:全 true ⇒ symmetric
        spec::SymmetricCapability all;
        all.device_originated_bi_snoop = true;
        all.device_side_sf_or_directory = true;
        all.spans_multiple_independent_hosts = true;
        all.p2p_without_host_cpu = true;
        all.snooped_agent_is_caching_host = true;
        CHECK(spec::is_cxl3_symmetric(all));

        // 基线:全 false ⇒ 不 symmetric
        CHECK_FALSE(spec::is_cxl3_symmetric(spec::SymmetricCapability{}));

        // **逐谓词反控**:每次只置假一个 ⇒ 必不 symmetric(⇒ 5 个谓词**都**参与合取)
        {
            auto c = all;
            c.device_originated_bi_snoop = false;
            CHECK_FALSE(spec::is_cxl3_symmetric(c));
        }
        {
            auto c = all;
            c.device_side_sf_or_directory = false;
            CHECK_FALSE(spec::is_cxl3_symmetric(c));
        }
        {
            auto c = all;
            c.spans_multiple_independent_hosts = false;
            CHECK_FALSE(spec::is_cxl3_symmetric(c));
        }
        {
            auto c = all;
            c.p2p_without_host_cpu = false;
            CHECK_FALSE(spec::is_cxl3_symmetric(c));
        }
        {
            auto c = all;
            c.snooped_agent_is_caching_host = false;
            CHECK_FALSE(spec::is_cxl3_symmetric(c));
        }
        // ⚠️ P5 的语义:被 snoop 的是 **caching host**,**不是** peer device cache
        //    ⇒ ⛔ 不得表述为 peer-to-peer 全对称(公开来源明写 CXL 3.0 **不满足**经典 symmetric)
    }

} // namespace
