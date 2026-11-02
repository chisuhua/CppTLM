// =============================================================================
// scripts/test/cep_cxl_dcoh_state_spec_probe.cc
//   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric-state-spec` 的**反向验证探针**
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: scripts/test/cep_cxl_dcoh_state_spec_reverse_verify.sh(驱动本探针)
//       include/tlm/cep/cxl_dcoh_state_spec.hh(被毒化的对象)
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric-state-spec/(delta spec CDS-01 ~ CDS-07)
//
// ── 用途 ──────────────────────────────────────────────────────────────────
// 本探针在**影子目录**(被毒化的载体副本)下编译并运行 ⇒ 验证判据的**鉴别力**:
//   · **干净载体 ⇒ 必须通过**(基线正控)
//   · **毒化载体 ⇒ 必须失败**,且失败原因 SHALL 命中**期望的判据 id**
//
// ⚠️ **两类失败形态都要支持**(本 change 的判据是**双形态**):
//   ① **编译期红** ⇒ 载体的 `static_assert` 触发 ⇒ 归因 = 编译日志里的 `CDS-0x:`
//   ② **运行期红** ⇒ 探针的 `PROBE:FAIL <id>` 输出
//
// 用法:probe        (退出码 0 = 全部谓词成立;1 = 有谓词不成立)
// =============================================================================

#include "tlm/cep/cxl_dcoh_state_spec.hh"

#include <cstdio>
#include <string>

namespace {

using namespace tlm::cep::cxl_dcoh;

int g_fail = 0;

void expect(bool cond, const char* id, const char* what) {
    if (!cond) {
        std::printf("PROBE:FAIL %s —— %s\n", id, what);
        ++g_fail;
    }
}

}  // namespace

int main() {
    // ── 基线正控:5 谓词全 true ⇒ symmetric ───────────────────────────────
    SymmetricCapability all;
    all.device_originated_bi_snoop = true;
    all.device_side_sf_or_directory = true;
    all.spans_multiple_independent_hosts = true;
    all.p2p_without_host_cpu = true;
    all.snooped_agent_is_caching_host = true;
    expect(is_cxl3_symmetric(all), "CDS-05", "5 谓词全 true 时 SHALL 判为 symmetric");

    // ── 基线正控:全 false ⇒ 不 symmetric ────────────────────────────────
    expect(!is_cxl3_symmetric(SymmetricCapability{}), "CDS-05",
           "全 false 时 SHALL **不**判为 symmetric");

    // ── CDS-05 逐谓词反控:每次只置假一个 ⇒ 必不 symmetric ────────────────
    //    ⇒ 这 5 条同时证明「5 个谓词**都**参与合取」
    {
        auto c = all;
        c.device_originated_bi_snoop = false;
        expect(!is_cxl3_symmetric(c), "CDS-05", "P1 置假 ⇒ SHALL 不 symmetric");
    }
    {
        auto c = all;
        c.device_side_sf_or_directory = false;
        expect(!is_cxl3_symmetric(c), "CDS-05", "P2 置假 ⇒ SHALL 不 symmetric");
    }
    {
        auto c = all;
        c.spans_multiple_independent_hosts = false;
        expect(!is_cxl3_symmetric(c), "CDS-05", "P3 置假 ⇒ SHALL 不 symmetric");
    }
    {
        auto c = all;
        c.p2p_without_host_cpu = false;
        expect(!is_cxl3_symmetric(c), "CDS-05", "P4 置假 ⇒ SHALL 不 symmetric");
    }
    {
        auto c = all;
        c.snooped_agent_is_caching_host = false;
        expect(!is_cxl3_symmetric(c), "CDS-05", "P5 置假 ⇒ SHALL 不 symmetric");
    }

    // ── CDS-03 基线正控:设备发起的 snoop 集合 == {BISnp} ───────────────────
    expect(kDeviceInitiatedSnoopSet.size() == 1u, "CDS-03", "设备发起 snoop 集合 SHALL 恰 1 类");
    expect(kDeviceInitiatedSnoopSet[0] == CxlMemChannel::S2M_BISnp, "CDS-03",
           "设备发起 snoop SHALL 是 BISnp");

    // ── CDS-04 基线正控:`perm` 只从缓存代理域投影 ──────────────────────────
    expect(perm_of(DevCacheState::M) == Perm::WRITEBACK, "CDS-04", "M ⇒ WRITEBACK");
    expect(perm_of(DevCacheState::E) == Perm::UNIQUE, "CDS-04", "E ⇒ UNIQUE");
    expect(perm_of(DevCacheState::S) == Perm::READ, "CDS-04", "S ⇒ READ");
    expect(perm_of(DevCacheState::I) == Perm::NONE, "CDS-04", "I ⇒ NONE");

    // ── CDS-06 基线正控:逐格 basis 全格有效 ───────────────────────────────
    expect(all_basis_valid(kBasisDevCacheState), "CDS-06", "DevCacheState 逐格 basis SHALL 全格有效");
    expect(all_basis_valid(kBasisDcohViewState), "CDS-06", "DCOH 视图逐格 basis SHALL 全格有效");
    expect(all_basis_valid(kBasisHomeDirState), "CDS-06", "home 目录逐格 basis SHALL 全格有效");
    expect(all_basis_valid(kBasisCxlMemChannel), "CDS-06", "CXL.mem 通道 basis SHALL 全格有效");
    expect(all_basis_valid(kBasisCxlCacheChannel), "CDS-06", "CXL.cache 通道 basis SHALL 全格有效");
    expect(all_basis_valid(kBasisPermProjection), "CDS-06", "`perm` 投影 basis SHALL 全格有效");
    expect(all_basis_valid(kBasisSymmetricPredicate), "CDS-06", "「对称」谓词 basis SHALL 全格有效");

    if (g_fail > 0) {
        std::printf("PROBE:FAIL 共 %d 项谓词不成立\n", g_fail);
        return 1;
    }
    std::printf("PROBE:OK 全部谓词成立\n");
    return 0;
}
