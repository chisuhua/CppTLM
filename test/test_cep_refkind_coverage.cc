// test/test_cep_refkind_coverage.cc
// 判据:`cxl_dcoh` / `nvlink_sharp` 接入 `RefKind` 后的**载体覆盖**与映射一致性
//      (change `cpptlm-cep-dcoh-nvlink-refkind-migration`)。
//
// 覆盖:
//   RKC-01 cxl_dcoh 全部 basis 表逐格 `basis_ref_kind_consistent` 为真
//   RKC-02 nvlink_sharp 全部 basis 表逐格 `basis_ref_kind_consistent` 为真
//   RKC-03 两族**恒不出现** `spec_clause`(⇒ 仍不计入 B3b,per `07 号` SU · 四 D-1)
//   RKC-04 负控:`PublicDirect` 指向 `Derived` / `AdrContract` ⇒ 必红(判据有牙)
//   RKC-05 零追溯降级:`basis` 取值 / `pending` / `SourceId` / 许可表未被改动
//
// 作者: builder  日期: 2026-11-07
#include "catch_amalgamated.hpp"

#include <array>
#include <string>

#include "tlm/cep/cxl_dcoh_state_spec.hh"
#include "tlm/cep/nvlink_sharp_state_spec.hh"
#include "tlm/cep/source_registry.hh"

using namespace tlm::cep;

namespace {

/// cxl_dcoh 的全部 basis 表(逐表遍历 ⇒ 覆盖全族)。
template <std::size_t N>
bool all_cxl_dcoh_consistent(const std::array<cxl_dcoh::Basis, N>& t) {
    for (const auto& b : t) {
        if (!cxl_dcoh::basis_ref_kind_consistent(b)) {
            return false;
        }
    }
    return true;
}

template <std::size_t N>
bool all_nvlink_consistent(const std::array<nvlink_sharp::Basis, N>& t) {
    for (const auto& b : t) {
        if (!nvlink_sharp::basis_ref_kind_consistent(b)) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("RKC-01: cxl_dcoh 逐格 basis ↔ ref_kind 一致", "[cep][refkind][coverage]") {
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisDevCacheState));
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisDcohViewState));
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisHomeDirState));
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisCxlMemChannel));
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisCxlCacheChannel));
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisPermProjection));
    REQUIRE(all_cxl_dcoh_consistent(cxl_dcoh::kBasisSymmetricPredicate));
}

TEST_CASE("RKC-02: nvlink_sharp 逐格 basis ↔ ref_kind 一致", "[cep][refkind][coverage]") {
    REQUIRE(all_nvlink_consistent(nvlink_sharp::kBasisIngressDecision));
    REQUIRE(all_nvlink_consistent(nvlink_sharp::kBasisRegisterResult));
    REQUIRE(all_nvlink_consistent(nvlink_sharp::kBasisEpochState));
    REQUIRE(all_nvlink_consistent(nvlink_sharp::kBasisOrderingRule));
    REQUIRE(all_nvlink_consistent(nvlink_sharp::kBasisVcReservationState));
    REQUIRE(all_nvlink_consistent(nvlink_sharp::kBasisCollectiveScope));
}

TEST_CASE("RKC-03: 两族恒不出现 spec_clause ⇒ 仍不计入 B3b", "[cep][refkind][coverage]") {
    // cxl_dcoh:公开来源(S2/S4/S5/S6/S7)⇒ public_nonnormative;Derived ⇒ repo_derived
    cxl_dcoh::Basis pub{};
    pub.source = cxl_dcoh::SourceId::S5;
    REQUIRE(cxl_dcoh::ref_kind_of(pub) == RefKind::PublicNonnormative);
    cxl_dcoh::Basis der{};
    der.source = cxl_dcoh::SourceId::Derived;
    REQUIRE(cxl_dcoh::ref_kind_of(der) == RefKind::RepoDerived);
    // nvlink:公开来源(S1/S2/S3)⇒ public_nonnormative;AdrContract / Derived ⇒ repo_derived
    nvlink_sharp::Basis npub{};
    npub.source = nvlink_sharp::SourceId::S1;
    REQUIRE(nvlink_sharp::ref_kind_of(npub) == RefKind::PublicNonnormative);
    nvlink_sharp::Basis nadr{};
    nadr.source = nvlink_sharp::SourceId::AdrContract;
    REQUIRE(nvlink_sharp::ref_kind_of(nadr) == RefKind::RepoDerived);
    // ⛔ 无任何来源映射到 spec_clause(CXL 规范正文禁读 / NVLink 无条款级公开规范)
    REQUIRE(cxl_dcoh::ref_kind_of(pub) != RefKind::SpecClause);
    REQUIRE(cxl_dcoh::ref_kind_of(der) != RefKind::SpecClause);
    REQUIRE(nvlink_sharp::ref_kind_of(npub) != RefKind::SpecClause);
    REQUIRE(nvlink_sharp::ref_kind_of(nadr) != RefKind::SpecClause);
}

TEST_CASE("RKC-04: 负控 —— PublicDirect 指向 Derived / AdrContract 必红", "[cep][refkind][coverage]") {
    // cxl_dcoh:PublicDirect 却引 `Derived` ⇒ 必红
    cxl_dcoh::Basis bad{};
    bad.kind = cxl_dcoh::EvidenceKind::PublicDirect;
    bad.source = cxl_dcoh::SourceId::Derived;
    REQUIRE_FALSE(cxl_dcoh::basis_ref_kind_consistent(bad));
    // 同一格若改为公开来源 ⇒ 一致(判据有鉴别力)
    cxl_dcoh::Basis good{};
    good.kind = cxl_dcoh::EvidenceKind::PublicDirect;
    good.source = cxl_dcoh::SourceId::S5;
    REQUIRE(cxl_dcoh::basis_ref_kind_consistent(good));

    // nvlink:PublicDirect 却引 `AdrContract`(仓内 ADR 锚点,非公开来源)⇒ 必红
    nvlink_sharp::Basis nbad{};
    nbad.kind = nvlink_sharp::EvidenceKind::PublicDirect;
    nbad.source = nvlink_sharp::SourceId::AdrContract;
    REQUIRE_FALSE(nvlink_sharp::basis_ref_kind_consistent(nbad));
    nvlink_sharp::Basis ngood{};
    ngood.kind = nvlink_sharp::EvidenceKind::PublicDirect;
    ngood.source = nvlink_sharp::SourceId::S2;
    REQUIRE(nvlink_sharp::basis_ref_kind_consistent(ngood));
}

TEST_CASE("RKC-05: 零追溯降级 —— 许可表与来源枚举未被改动", "[cep][refkind][coverage]") {
    // cxl_dcoh:6 个来源,S5(CC-BY 4.0)许可已确认;S4(许可 absent)未确认
    REQUIRE(cxl_dcoh::kSourceCount == 6u);
    REQUIRE(cxl_dcoh::kSourceLicenseConfirmed[static_cast<std::size_t>(cxl_dcoh::SourceId::S5)]);
    REQUIRE_FALSE(cxl_dcoh::kSourceLicenseConfirmed[static_cast<std::size_t>(cxl_dcoh::SourceId::S4)]);
    // nvlink:5 个来源;S1/S2/S3 再分发许可**未**确认 ⇒ 以其为依据的格必须 pending
    REQUIRE(nvlink_sharp::kSourceCount == 5u);
    REQUIRE_FALSE(nvlink_sharp::kSourceLicenseConfirmed[static_cast<std::size_t>(nvlink_sharp::SourceId::S1)]);
    REQUIRE_FALSE(nvlink_sharp::kSourceLicenseConfirmed[static_cast<std::size_t>(nvlink_sharp::SourceId::S2)]);
    REQUIRE_FALSE(nvlink_sharp::kSourceLicenseConfirmed[static_cast<std::size_t>(nvlink_sharp::SourceId::S3)]);
}
