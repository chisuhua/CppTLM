// test/test_cep_vc_mapping_contract.cc
// 传输层 `CATEGORY_*` → VC 桶映射契约的可执行回归(Phase 0,per change
// `cpptlm-cep-stub-cache-core` 的 delta capability `cep-vc-mapping-contract`)
//
// 契约化对象: **传输层** NoC(SocInternalNoc 的物理桶)与 flit 类别之间的映射关系。
//   ⚠️ 与 ArchForge `32 号 §6.0.2` 的 per-network VC 分配矩阵是**正交维度**
//      (后者归 `vc_binding`,是一致性协议层)—— 数值都 0-3 但语义不同。
//
// 覆盖的 Scenario(与 specs/cep-vc-mapping-contract/spec.md 一一对应):
//   1. 传输层 `CATEGORY_*` → VC 桶的映射 SHALL 保持值域连续(索引对齐)
//   2. 每个 VC 桶的**流量实况** SHALL 可断言
//   3. 零流量桶的「恒 0」SHALL 有**正控**
//   4. `vc_id` 与 `flit_category` 冲突时的归属规则 SHALL 显式
//   5. 传输层 SHALL **NOT** 仲裁 `vc_id` 与 `flit_category` 的冲突
//   6. `VC_DAT` 的语义描述 SHALL **NOT** 声称「统计 credit 流量」(由 lint 断言 ④⑥ 守护)
//   7. `CATEGORY_CREDIT` SHALL **NOT** 被表述为独占 VC(由 lint 断言 ② 守护)
//   8. 显式映射表**不属于**本 capability(由 lint 断言 ① 守护)
//
// **判据纪律**(per 队列 #2 critic 7 轮提炼):
//   · 断言针对**计数变化**而非「常量存在」—— 「输入改变时输出必须改变」;
//   · 纯负向断言(「恒 0」)**必须**配正控 —— 否则判据退化时仍然绿(无牙);
//   · 红绿分离:「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论登记为交付物。
//
// 作者: CppTLM Team / 日期: 2026-10-25
#include "catch_amalgamated.hpp"

#include "bundles/noc_bundles_tlm.hh"
#include "tlm/cluster/soc_internal_noc.hh"
#include "tlm/vram_noc_adapter.hh"

#include <cstdint>

using bundles::NoCFlitBundle;
using tlm::cluster::SocInternalNoc;
using tlm::cluster::VramNocAdapter;

// ── 编译期:值域连续性(索引对齐的前提)─────────────────────────────
// 静态求值断言:若 CATEGORY_* 的值域不再连续,`route_flit` 的
// `(cat < VC_COUNT) ? cat : VC_SNP` 守卫会把某类别**静默折叠**进 VC_SNP。
static_assert(NoCFlitBundle::CATEGORY_REQUEST == 0, "CATEGORY_REQUEST 必须为 0(索引对齐)");
static_assert(NoCFlitBundle::CATEGORY_RESPONSE == 1, "CATEGORY_RESPONSE 必须为 1(索引对齐)");
static_assert(NoCFlitBundle::CATEGORY_CREDIT == 2, "CATEGORY_CREDIT 必须为 2(索引对齐)");
static_assert(SocInternalNoc::VC_REQ == 0, "VC_REQ 必须为 0");
static_assert(SocInternalNoc::VC_RSP == 1, "VC_RSP 必须为 1");
static_assert(SocInternalNoc::VC_DAT == 2, "VC_DAT 必须为 2");
static_assert(SocInternalNoc::VC_SNP == 3, "VC_SNP 必须为 3");
static_assert(SocInternalNoc::VC_COUNT == 4, "VC_COUNT 必须为 4");

// ── 1) 传输层 `CATEGORY_*` → VC 桶的映射 SHALL 保持值域连续(索引对齐) ──
TEST_CASE("CEP VC 映射: CATEGORY_* 值域连续 → 索引对齐不折叠任何类别",
          "[cep][vc-mapping-contract][mapping]") {
    // 值域连续 ⇒ 每个 CATEGORY_* 都落在**语义正确**的桶:
    //   CATEGORY_REQUEST → VC_REQ / CATEGORY_RESPONSE → VC_RSP / CATEGORY_CREDIT → VC_DAT
    // 判据形态:**注入不同类别 → 不同桶的计数改变**(不只断言常量存在)
    SocInternalNoc noc("noc", nullptr);

    NoCFlitBundle req{};
    req.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    req.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    noc.route_flit(req);
    CHECK(noc.vc_req_count() == 1);
    CHECK(noc.vc_rsp_count() == 0);
    CHECK(noc.vc_dat_count() == 0);

    NoCFlitBundle resp{};
    resp.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    resp.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    noc.route_flit(resp);
    CHECK(noc.vc_req_count() == 1); // 不变
    CHECK(noc.vc_rsp_count() == 1); // 变了
    CHECK(noc.vc_dat_count() == 0); // 不变
}

// ── 2) 每个 VC 桶的**流量实况** SHALL 可断言 ───────────────────────
TEST_CASE("CEP VC 映射: 三桶计数随 flit_category 改变(输入→输出映射可观测)",
          "[cep][vc-mapping-contract][mapping][traffic]") {
    SocInternalNoc noc("noc", nullptr);

    // baseline:全 0
    CHECK(noc.vc_req_count() == 0);
    CHECK(noc.vc_rsp_count() == 0);
    CHECK(noc.vc_dat_count() == 0);
    CHECK(noc.vc_snp_count() == 0);

    NoCFlitBundle f{};
    f.dst_node.write(SocInternalNoc::NODE_ID_VRAM);

    f.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    noc.route_flit(f);
    CHECK(noc.vc_req_count() == 1);

    f.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    noc.route_flit(f);
    CHECK(noc.vc_rsp_count() == 1);

    f.flit_category.write(NoCFlitBundle::CATEGORY_CREDIT);
    noc.route_flit(f);
    CHECK(noc.vc_dat_count() == 1);

    // 累计:各桶互不串扰
    CHECK(noc.vc_req_count() == 1);
    CHECK(noc.vc_rsp_count() == 1);
    CHECK(noc.vc_dat_count() == 1);
    // VC_SNP 是**预留**桶 —— 3 个传输层类别都落不到它
    CHECK(noc.vc_snp_count() == 0);
}

// ── 3) 零流量桶的「恒 0」SHALL 有**正控** ──────────────────────────
TEST_CASE("CEP VC 映射: 零流量桶「恒 0」配正控(注入 CREDIT → 计数 0→1)",
          "[cep][vc-mapping-contract][mapping][positive-control]") {
    // 负向部分:无注入时恒 0
    SocInternalNoc noc("noc", nullptr);
    CHECK(noc.vc_dat_count() == 0);
    CHECK(noc.vc_snp_count() == 0);

    // ⚠️ **正控**(判据③:纯负向断言必须有正控 —— 否则判据退化时仍然绿 = 无牙):
    //   注入一个 CATEGORY_CREDIT flit → vc_dat_count() **必须**从 0 变为 1。
    //   这证明「恒 0」是**数据驱动**的结论,而不是恒真断言。
    NoCFlitBundle credit{};
    credit.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    credit.flit_category.write(NoCFlitBundle::CATEGORY_CREDIT);
    noc.route_flit(credit);

    CHECK(noc.vc_dat_count() == 1); // 0 → 1(正控生效)

    // 正控后:VC_SNP 仍恒 0(预留桶确实无类别可落入)
    CHECK(noc.vc_snp_count() == 0);
}

// ── 4) `vc_id` 与 `flit_category` 冲突时的归属规则 SHALL 显式 ──────
TEST_CASE("CEP VC 映射: 冲突输入(vc_id≠flit_category)计数归 flit_category",
          "[cep][vc-mapping-contract][mapping][conflict]") {
    SocInternalNoc noc("noc", nullptr);

    // 冲突输入:`vc_id = VC_DAT` 但 `flit_category = CATEGORY_REQUEST`
    NoCFlitBundle conflict{};
    conflict.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    conflict.vc_id.write(SocInternalNoc::VC_DAT);
    conflict.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    noc.route_flit(conflict);

    // **计数归属** = flit_category → 计入 VC_REQ,**不**计入 VC_DAT
    CHECK(noc.vc_req_count() == 1);
    CHECK(noc.vc_dat_count() == 0);

    // **编码归属** = vc_id → chip_select 用 vc_id 的值参与计算
    //   (per VramNocAdapter::emit_noc_request: (vc_id & 0x0F) << 4 | (src_node & 0x0F))
    //   SDMA (src=1) + VC_DAT (2) → (2 << 4) | 1 = 0x21
    VramNocAdapter adapter;
    NoCFlitBundle enc{};
    enc.src_node.write(SocInternalNoc::NODE_ID_SDMA);
    enc.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    enc.vc_id.write(SocInternalNoc::VC_DAT);
    enc.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    auto encoded = adapter.emit_noc_request(enc);
    CHECK(encoded.chip_select.read() == 0x21); // 编码取 vc_id(=2),不是 flit_category(=0)

    // 对照:vc_id=VC_REQ 时编码取 0 → chip_select = (0 << 4) | 1 = 0x01
    adapter.reset();
    NoCFlitBundle enc2{};
    enc2.src_node.write(SocInternalNoc::NODE_ID_SDMA);
    enc2.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    enc2.vc_id.write(SocInternalNoc::VC_REQ);
    enc2.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    auto encoded2 = adapter.emit_noc_request(enc2);
    CHECK(encoded2.chip_select.read() == 0x01);
}

// ── 5) 传输层 SHALL **NOT** 仲裁 `vc_id` 与 `flit_category` 的冲突 ──
TEST_CASE("CEP VC 映射: 传输层不仲裁冲突输入(不报错/不纠正/不丢弃)",
          "[cep][vc-mapping-contract][mapping][conflict][no-arbitration]") {
    SocInternalNoc noc("noc", nullptr);

    // 记录 target 收到的 flit —— 用于断言「**不丢弃**」
    int received = 0;
    NoCFlitBundle seen{};
    noc.set_vram_accept_fn([&](const NoCFlitBundle& f) {
        ++received;
        seen = f;
    });

    NoCFlitBundle conflict{};
    conflict.dst_node.write(SocInternalNoc::NODE_ID_VRAM);
    conflict.vc_id.write(SocInternalNoc::VC_DAT);
    conflict.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    const uint8_t orig_vc = conflict.vc_id.read();
    const uint8_t orig_cat = conflict.flit_category.read();

    // **不报错**:route_flit 是 noexcept —— 任何异常都会 terminate;
    //          用例能跑到下一行即证明未报错。
    noc.route_flit(conflict);

    // **不丢弃**:flit 被推送到 target(不是被静默丢弃)
    CHECK(received == 1);
    CHECK(noc.flits_dispatched() == 1);
    CHECK(noc.unknown_dst_count() == 0);

    // **不纠正**:输入 flit 的两个字段未被改写(计数侧按 flit_category,编码侧按 vc_id)
    CHECK(conflict.vc_id.read() == orig_vc);
    CHECK(conflict.flit_category.read() == orig_cat);
    // target 收到的也是原始值 —— 传输层没有做任何「修正」
    CHECK(seen.vc_id.read() == orig_vc);
    CHECK(seen.flit_category.read() == orig_cat);

    // 计数侧仍归 flit_category(与场景 4 一致)
    CHECK(noc.vc_req_count() == 1);
    CHECK(noc.vc_dat_count() == 0);
}
