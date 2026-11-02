// =============================================================================
// test/test_cep_cxl_dcoh_tracker.cc
//   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric` 的 **tracker 行为判据**
//     (CDX-04 行为逐字段符合 traits · CDX-05 `LOCAL_HINT` 语义代价 · 可观测差异)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/cxl_dcoh_tracker.hh · include/tlm/cep/sharer_tracker.hh
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric/(delta spec CDX-04 / CDX-05)
//       docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md
//
// ── 判据口径 ────────────────────────────────────────────────────────────────
// · **CDX-04 要求「数据级断言,非仅『常量存在』」** ⇒ 本文件对 **6 个 caps / behavior
//   字段**逐个**行为**核对(不是抄 traits)。
// · ⚠️ **可观测差异铁律**(per `ADR-CEP-04 §13`「克隆体不可能」的判据精神):
//   每个行为断言都**配正控** —— 同一断言在「不满足该 caps 的对照实现」上**必红**
//   ⇒ 否则「断言恒绿」= 判据可能没生效。
//   ⇒ 对照实现 = `SparseSnoopFilter`(同 CONSERVATIVE 但 `SYSTEM` scope / `GLOBAL` authority)
//     与 `FullMapDirectory`(同 HOME 聚合但 `EXACT` / `GLOBAL`)。
// =============================================================================

#include "catch_amalgamated.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "tlm/cep/cxl_dcoh_tracker.hh"
#include "tlm/cep/sharer_tracker.hh"
#include "tlm/cep/tilelink_tree_manager.hh"

namespace {

    namespace ad = tlm::cep;

    /// 本 tracker 覆盖的区域(测试用固定区间)
    /// ⚠️ 区间 = `[0x1000, 0x101000)` ⇒ 覆盖下方**全部**用例地址
    ///    (`0x1xxx` ~ `0x6xxx`)⇒ 「区域内」与「区域外」两组断言互不串味。
    constexpr ad::DcohSymmetric::LocalRegion kRegion{0x1000, 0x100000};

    /// 区域外的地址(用于验证 `scope=LOCAL_MEMORY_REGION` 的可观测差异)
    constexpr std::uint64_t kOutsideAddr = 0x90000000;

    /// 读仓内文件(相对 `CPPTLM_SOURCE_DIR`)
    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 读 capability 的 main spec(**双态**:change 目录 → main specs → 归档件)
    /// ⇒ 归档前后都必须绿(照 `test_cep_sharer_tracker_base.cc` 同款纪律)
    std::string read_capability_spec(const std::string& capability) {
        namespace fs = std::filesystem;
        const std::vector<std::string> direct{
            "openspec/specs/" + capability + "/spec.md",
        };
        for (const auto& rel : direct) {
            const std::string text = read_repo_file(rel);
            if (!text.empty())
                return text;
        }
        const fs::path arch = fs::path(CPPTLM_SOURCE_DIR) / "openspec/changes/archive";
        std::error_code ec;
        if (fs::is_directory(arch, ec)) {
            for (const auto& e : fs::directory_iterator(arch, ec)) {
                if (!e.is_directory())
                    continue;
                const fs::path sp = e.path() / "specs" / capability / "/spec.md";
                std::error_code ec2;
                if (fs::exists(sp, ec2)) {
                    std::ifstream in(sp);
                    std::ostringstream ss;
                    ss << in.rdbuf();
                    return ss.str();
                }
            }
        }
        return {};
    }

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// CDX-04 —— caps / behavior 逐字段(数据级)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-04:DcohSymmetric 的 caps/behavior 逐字段等于 kTrackerTraits[2]",
          "[cep][cxl-dcoh][tracker]") {
    const auto& traits = ad::kTrackerTraits[2];
    REQUIRE(traits.impl == ad::TrackerImpl::DcohSymmetric);
    CHECK(std::string(traits.name) == "DcohSymmetric");

    ad::DcohSymmetric t(kRegion);

    // ── caps 4 字段 ──
    CHECK(t.caps.scope == ad::Scope::LOCAL_MEMORY_REGION);
    CHECK(t.caps.completeness == ad::Completeness::CONSERVATIVE);
    CHECK(t.caps.authority == ad::Authority::LOCAL_HINT);
    CHECK(t.caps.aggregation_at == ad::AggregationAt::HOME);

    // ── behavior 2 字段 ──
    CHECK(t.behavior.hazard_under_contention == ad::LookupHazard::STALL);
    CHECK(t.behavior.broadcast_fallback == true);

    // ⚠️ **存储唯一**:caps / behavior 存在**基类** ⇒ 本类不另存副本
    //    ⇒ `impl.caps` 写法仍可用(继承而来)⇒ 与既有 3 个行为类同构。
    static_assert(std::is_base_of_v<ad::SharerTrackerIf, ad::DcohSymmetric>,
                  "DcohSymmetric 必须是 SharerTrackerIf 的派生类");
    CHECK(t.kImpl == ad::TrackerImpl::DcohSymmetric);
    CHECK(ad::DcohSymmetric::kHandleSpace == ad::SnoopMechanics::kHandleSpace);
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-04① —— `scope = LOCAL_MEMORY_REGION` 的**可观测**行为
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-04①:scope=LOCAL_MEMORY_REGION ⇒ 区域外地址不由本 tracker 回答",
          "[cep][cxl-dcoh][tracker]") {
    ad::DcohSymmetric t(kRegion);

    // ① 区域外 ⇒ `BROADCAST` + `hazard=NONE` + view 无效
    ad::LookupResult res = ad::LookupResult::ANSWER_HERE;
    ad::LocalView view{};
    ad::LookupHazard hazard = ad::LookupHazard::CHAIN;
    t.lookup(kOutsideAddr, ad::Intent::want_read, res, view, hazard);
    CHECK(res == ad::LookupResult::BROADCAST);
    CHECK(hazard == ad::LookupHazard::NONE);
    CHECK_FALSE(view.valid);

    // ② 区域外 ⇒ `set_sharer` 是**显式 no-op**(不留可见条目)
    t.set_sharer(kOutsideAddr, ad::kNodeIdSdma, ad::SnoopState::M);
    CHECK(t.possible_sharer_count(kOutsideAddr) == 0u);
    CHECK_FALSE(t.view_of(kOutsideAddr).valid);

    // ③ 区域内 ⇒ 同样调用**有效** ⇒ 这是 scope 生效的**正控**
    t.set_sharer(0x1010, ad::kNodeIdSdma, ad::SnoopState::M);
    CHECK(t.possible_sharer_count(0x1010) == 1u);
    CHECK(t.view_of(0x1010).valid);

    // ④ 区域边界:`base` 与 `base+size-1` **在**区域内,`base+size` **不在**
    CHECK(kRegion.contains(kRegion.base));
    CHECK(kRegion.contains(kRegion.base + kRegion.size - 1));
    CHECK_FALSE(kRegion.contains(kRegion.base + kRegion.size));

    // ⑤ **正控**:`scope=SYSTEM` 的对照实现(`FullMapDirectory`)对**同一**区域外地址
    //    **能**给出本地结论 ⇒ 二者**可判别** ⇒ 本断言有鉴别力(非恒绿)。
    ad::FullMapDirectory fmd;
    fmd.set_sharer(kOutsideAddr, ad::kNodeIdSdma, ad::SnoopState::M);
    ad::LookupResult fres = ad::LookupResult::ANSWER_HERE;
    ad::LocalView fview{};
    ad::LookupHazard fhazard = ad::LookupHazard::CHAIN;
    fmd.lookup(kOutsideAddr, ad::Intent::want_read, fres, fview, fhazard);
    CHECK(fres != ad::LookupResult::BROADCAST); // SYSTEM scope ⇒ 不因「区域外」而广播
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-04② —— `completeness = CONSERVATIVE` 的**可观测**行为(宁多探不漏探)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-04②:CONSERVATIVE ⇒ 保守近似,绝不声称精确(永不 ANSWER_HERE)",
          "[cep][cxl-dcoh][tracker]") {
    ad::DcohSymmetric t(kRegion);

    // ① 本区域**无**该地址 ⇒ **不能**证明无人持有 ⇒ 广播(不是 ANSWER_HERE)
    ad::LookupResult res = ad::LookupResult::ANSWER_HERE;
    ad::LocalView view{};
    ad::LookupHazard hazard = ad::LookupHazard::CHAIN;
    t.lookup(0x2000, ad::Intent::want_read, res, view, hazard);
    CHECK(res == ad::LookupResult::BROADCAST);
    CHECK(hazard == ad::LookupHazard::NONE);

    // ② **多个**可能 sharer ⇒ 单点转发会漏探 ⇒ 广播(保守)
    t.set_sharer(0x2010, ad::kNodeIdSdma, ad::SnoopState::M);
    t.set_sharer(0x2010, ad::kNodeIdVram, ad::SnoopState::M);
    res = ad::LookupResult::ANSWER_HERE;
    t.lookup(0x2010, ad::Intent::want_read, res, view, hazard);
    CHECK(res == ad::LookupResult::BROADCAST);
    CHECK(view.valid);
    CHECK(ad::DcohSymmetric::bit_count(view.sharers) == 2u);

    // ③ **唯一**可能 sharer 且脏 ⇒ 单点转发**安全**(位集是上界 ⇒ 不会有别的 sharer)
    t.set_sharer(0x2020, ad::kNodeIdSdma, ad::SnoopState::M);
    res = ad::LookupResult::ANSWER_HERE;
    t.lookup(0x2020, ad::Intent::want_read, res, view, hazard);
    CHECK(res == ad::LookupResult::FORWARD_TO);
    CHECK(view.forward_to == ad::kNodeIdSdma);

    // ④ **唯一**可能 sharer 但**不脏** ⇒ 不能靠「脏数据在谁那里」定位 ⇒ 广播
    t.set_sharer(0x2030, ad::kNodeIdSdma, ad::SnoopState::E);
    res = ad::LookupResult::ANSWER_HERE;
    t.lookup(0x2030, ad::Intent::want_read, res, view, hazard);
    CHECK(res == ad::LookupResult::BROADCAST);

    // ⑤ **正控**:`EXACT` 的对照实现(`FullMapDirectory`)对「无该地址」能答
    //    `ANSWER_HERE` ⇒ 二者**可判别** ⇒ CONSERVATIVE 断言有鉴别力。
    ad::FullMapDirectory fmd;
    ad::LookupResult fres = ad::LookupResult::BROADCAST;
    ad::LocalView fview{};
    ad::LookupHazard fh = ad::LookupHazard::CHAIN;
    fmd.lookup(0x2000, ad::Intent::want_read, fres, fview, fh);
    CHECK(fres == ad::LookupResult::ANSWER_HERE);
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-04③ —— `authority = LOCAL_HINT` 的**可观测**行为
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-04③:authority=LOCAL_HINT ⇒ 答案只是 hint,不可当权威", "[cep][cxl-dcoh][tracker]") {
    ad::DcohSymmetric t(kRegion);

    // ① 本 tracker **永不**给 `ANSWER_HERE`(hint 不足以支撑「本地权威回答」)
    t.set_sharer(0x3010, ad::kNodeIdSdma, ad::SnoopState::M);
    ad::LookupResult res = ad::LookupResult::ANSWER_HERE;
    ad::LocalView view{};
    ad::LookupHazard hazard = ad::LookupHazard::CHAIN;
    t.lookup(0x3010, ad::Intent::want_read, res, view, hazard);
    CHECK(res != ad::LookupResult::ANSWER_HERE);

    // ② hint 载体 = `forward_to`(语义**重载**)⇒ 其值**必须**与 `result` 联读
    //    ⇒ `result == FORWARD_TO` 时 `forward_to` 才是有意义的 hint 目标
    CHECK(res == ad::LookupResult::FORWARD_TO);
    CHECK(view.forward_to == ad::kNodeIdSdma);
    //    ⇒ 若 `result` 非 `FORWARD_TO`,则 `forward_to` **不承载**任何权威含义
    t.lookup(0x3010, ad::Intent::want_write, res, view, hazard);
    CHECK(res == ad::LookupResult::BROADCAST);

    // ③ `LocalView` **零改动**(契约面)⇒ 本 tracker 只用既有 4 字段
    //    ⇒ 用**偏移量 + 尺寸**锁死布局(加字段 ⇒ 偏移或尺寸变 ⇒ 必红)
    static_assert(offsetof(ad::LocalView, sharers) == 0, "LocalView.sharers 偏移 SHALL 不变");
    static_assert(offsetof(ad::LocalView, dirty) == 8, "LocalView.dirty 偏移 SHALL 不变");
    static_assert(offsetof(ad::LocalView, forward_to) == 16,
                  "LocalView.forward_to 偏移 SHALL 不变");
    static_assert(offsetof(ad::LocalView, valid) == 20, "LocalView.valid 偏移 SHALL 不变");
    static_assert(sizeof(ad::LocalView) == 24, "LocalView 尺寸 SHALL 不变(不得加字段)");
    CHECK(view.valid); // 保守视图仍**有效**(=「本区域的已知上界」),但**不是**权威答案

    // ④ **正控**:`GLOBAL` authority 的对照实现(`SparseSnoopFilter` 同 CONSERVATIVE)
    //    在「唯一可能 sharer」时**同样**给 FORWARD_TO ⇒ 本条的**鉴别力**来自
    //    「本 tracker 永不 ANSWER_HERE」这一**独立**可观测差异,已由 ① 覆盖。
    ad::SparseSnoopFilter ssf;
    ssf.set_sharer(0x3010, ad::kNodeIdSdma, ad::SnoopState::M);
    ad::LookupResult sres = ad::LookupResult::ANSWER_HERE;
    ad::LocalView sview{};
    ad::LookupHazard sh = ad::LookupHazard::CHAIN;
    ssf.lookup(0x3010, ad::Intent::want_read, sres, sview, sh);
    CHECK(sres == ad::LookupResult::FORWARD_TO);
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-04④ —— `aggregation_at = HOME` + `hazard = STALL` 的**可观测**行为
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-04④:aggregation_at=HOME ⇒ 无 ESCALATE_UP;hazard=STALL ⇒ 同址并发返 STALL",
          "[cep][cxl-dcoh][tracker]") {
    ad::DcohSymmetric t(kRegion);

    // ① 聚合点恒为 HOME ⇒ 本 tracker **不**产生 `ESCALATE_UP`(那是 `TREE_PARENT` 的语义)
    t.set_sharer(0x4010, ad::kNodeIdSdma, ad::SnoopState::M);
    ad::LookupResult res = ad::LookupResult::ANSWER_HERE;
    ad::LocalView view{};
    ad::LookupHazard hazard = ad::LookupHazard::CHAIN;
    t.lookup(0x4010, ad::Intent::want_read, res, view, hazard);
    CHECK(res != ad::LookupResult::ESCALATE_UP);

    // ② 同址 snoop 在飞 ⇒ `STALL`(+ view 无意义)
    ad::snoop_handle_t handle = 0;
    t.snoop_begin(0x4010, ad::responder_mask_of(ad::kNodeIdSdma), handle);
    REQUIRE(handle != 0);
    res = ad::LookupResult::ANSWER_HERE;
    t.lookup(0x4010, ad::Intent::want_read, res, view, hazard);
    CHECK(hazard == ad::LookupHazard::STALL);
    CHECK_FALSE(view.valid);

    // ③ **正控**:`CHAIN` 的对照实现(`TreeManager`)在同址在飞时给 `CHAIN`
    //    ⇒ 二者**可判别** ⇒ STALL 断言有鉴别力。
    tlm::cep::TreeManager tm(ad::kNodeIdVram);
    ad::snoop_handle_t th = 0;
    tm.snoop_begin(0x4010, ad::responder_mask_of(ad::kNodeIdSdma), th);
    REQUIRE(th != 0);
    ad::LookupResult tres = ad::LookupResult::ANSWER_HERE;
    ad::LocalView tview{};
    ad::LookupHazard thazard = ad::LookupHazard::NONE;
    tm.lookup(0x4010, ad::Intent::want_read, tres, tview, thazard);
    CHECK(thazard == ad::LookupHazard::CHAIN);
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-04⑤ —— CONSERVATIVE 的**唯一收缩来源** = 观测
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-04⑤:CONSERVATIVE ⇒ 只增不减;收缩仅来自观测", "[cep][cxl-dcoh][tracker]") {
    ad::DcohSymmetric t(kRegion);

    // ① 装入 ⇒ 2 个可能 sharer
    t.set_sharer(0x5010, ad::kNodeIdSdma, ad::SnoopState::M);
    t.set_sharer(0x5010, ad::kNodeIdVram, ad::SnoopState::M);
    CHECK(t.possible_sharer_count(0x5010) == 2u);

    // ② 装入**不**收缩(只增不减)⇒ 重装同一节点仍是 2
    t.set_sharer(0x5010, ad::kNodeIdSdma, ad::SnoopState::E);
    CHECK(t.possible_sharer_count(0x5010) == 2u);
    CHECK(ad::DcohSymmetric::bit_count(t.view_of(0x5010).dirty) == 1u); // 脏性仍保守

    // ③ **观测**到该节点转为 `I` ⇒ 合法收缩
    t.observe_snoop(0x5010, ad::kNodeIdSdma, ad::SnoopState::I);
    CHECK(t.possible_sharer_count(0x5010) == 1u);

    // ④ 全部收缩 ⇒ 条目释放(`view_of` 变无效)
    t.observe_snoop(0x5010, ad::kNodeIdVram, ad::SnoopState::I);
    CHECK(t.possible_sharer_count(0x5010) == 0u);
    CHECK_FALSE(t.view_of(0x5010).valid);
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-05 —— `LOCAL_HINT` 的**语义代价**可寻址
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-05:LOCAL_HINT 的语义代价(双重语义)已显式登记且可寻址", "[cep][cxl-dcoh][tracker]") {
    // ① 语义代价的**代码侧载体**:本 tracker 的 `forward_to` 承载的是 **hint 目标**,
    //    而**不是**权威转发点 ⇒ 实现层**不**新增字段(`LocalView` 零改动)。
    ad::DcohSymmetric t(kRegion);
    t.set_sharer(0x6010, ad::kNodeIdSdma, ad::SnoopState::M);
    ad::LookupResult res = ad::LookupResult::ANSWER_HERE;
    ad::LocalView view{};
    ad::LookupHazard hazard = ad::LookupHazard::CHAIN;
    t.lookup(0x6010, ad::Intent::want_read, res, view, hazard);
    CHECK(res == ad::LookupResult::FORWARD_TO);
    CHECK(view.forward_to == ad::kNodeIdSdma);
    //    ⇒ 判读 `forward_to` 前**必须**先读 `result`(双重语义)
    CHECK(res != ad::LookupResult::BROADCAST);

    // ③ 语义代价的**登记侧**(Phase 3.5 的冻结面追加段)—— 须**可寻址**
    //    ⇒ 否则该义务是**纸面约定**(没人能在契约面找到它)
    const std::string spec = read_capability_spec("cep-sharer-tracker-base");
    REQUIRE_FALSE(spec.empty());
    CHECK(spec.find("LOCAL_HINT") != std::string::npos);
    CHECK(spec.find("语义代价") != std::string::npos);
    CHECK(spec.find("语义重载") != std::string::npos);
    CHECK(spec.find("Entry #165") != std::string::npos);
    //    ⇒ 冻结面 spec 的「其余 2 种」计数 SHALL 已随本实现落地而收缩
    CHECK(spec.find("其余 **1** 种") != std::string::npos);

    // ② 语义代价的**实现侧文档义务**:头文件**必须**写明「双重语义」+ 判读纪律
    //    ⇒ 消费方只读头文件即知「`forward_to` 前须先读 `result`」
    //    (⚠️ 台账 / main spec 的**登记**属 Phase 3.5 的**冻结面追加段**
    //     ⇒ 那侧的「可寻址」断言落 Phase 3 的实例声明用例,不在此重复)
    const std::string hdr = read_repo_file("include/tlm/cep/cxl_dcoh_tracker.hh");
    REQUIRE_FALSE(hdr.empty());
    CHECK(hdr.find("LOCAL_HINT") != std::string::npos);
    CHECK(hdr.find("CDX-05") != std::string::npos);
    CHECK(hdr.find("forward_to") != std::string::npos);
    CHECK(hdr.find("\u5224\u8bfb") != std::string::npos);             // 「判读」
    CHECK(hdr.find("\u8bed\u4e49\u91cd\u8f7d") != std::string::npos); // 「语义重载」
}
