// test/test_param_loader.cc
// 模块参数「读取即声明」机制测试
// 作者 CppTLM Team / 日期 2026-11-17
//
// 决策源：docs/adr/ADR-X.17-module-param-read-declares.md（V-1 / V-2 / V-6 / D-3a）
// 判据源：openspec/changes/cpptlm-module-param-read-declares/proposal.md（AC-01~AC-12 + G-1/G-2）
#include "chstream_register.hh"
#include "catch_amalgamated.hpp"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/param_admission.hh"
#include "core/param_errors.hh"
#include "core/sim_module.hh"
#include "param_fixture_module.hh"
#include "tlm/router_tlm.hh"

#include <string>

namespace {

    /// 夹具注册（运行时，幂等；⛔ 不用静态注册宏以避免多 TU 的 ODR 面）
    void ensure_fixture_registered() {
        ModuleFactory::registerObject<ParamFixtureTLM>(ParamFixtureTLM::kType);
        cpptlm::register_param_admitted(ParamFixtureTLM::kType);
    }

    json one_module(const std::string& name, const std::string& type, const json& params) {
        return json{
            {"modules", json::array({json{{"name", name}, {"type", type}, {"params", params}}})},
            {"connections", json::array()}};
    }

    /// legacy 扁平形式：参数直接写在 module 条目上（无 params 子对象）
    json one_module_flat(const std::string& name, const std::string& type,
                         const json& flat_fields) {
        json mod = {{"name", name}, {"type", type}};
        for (auto it = flat_fields.begin(); it != flat_fields.end(); ++it)
            mod[it.key()] = it.value();
        return json{{"modules", json::array({mod})}, {"connections", json::array()}};
    }

    json router_params(unsigned x, unsigned y, unsigned mx, unsigned my) {
        return json{{"node_x", x}, {"node_y", y}, {"mesh_x", mx}, {"mesh_y", my}};
    }

} // namespace

// ============================================================================
// AC-01 / AC-05 / AC-11 — 类型与数值边界
// ============================================================================
TEST_CASE("AC-01: 夹具参数越界 / 类型错 ⇒ instantiateAll 失败", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    SECTION("越界 ⇒ false") {
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx", ParamFixtureTLM::kType, {{"gain", 0}})));
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx", ParamFixtureTLM::kType, {{"gain", 17}})));
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx", ParamFixtureTLM::kType, {{"offset", -65}})));
    }

    SECTION("类型错 ⇒ false") {
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx", ParamFixtureTLM::kType, {{"gain", "abc"}})));
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx", ParamFixtureTLM::kType, {{"offset", "x"}})));
    }

    SECTION("错误信息含 模块名 + 键名 + 期望范围") {
        ParamFixtureTLM mod("fx_name", &eq);
        try {
            mod.set_config(json{{"gain", 0}});
            FAIL("expected ParamValidationError");
        } catch (const cpptlm::ParamValidationError& e) {
            REQUIRE(e.module_name == "fx_name");
            REQUIRE(e.param_name == "gain");
            const std::string what = e.what();
            REQUIRE(what.find("range [1,16]") != std::string::npos);
            REQUIRE(what.find("got 0") != std::string::npos);
        }
    }
}

TEST_CASE("AC-05: 夹具边界成对（1/16 通过，0/17 失败）", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    REQUIRE(factory.instantiateAll(one_module("fx_lo", ParamFixtureTLM::kType, {{"gain", 1}})));
    REQUIRE(factory.instantiateAll(one_module("fx_hi", ParamFixtureTLM::kType, {{"gain", 16}})));
    REQUIRE_FALSE(
        factory.instantiateAll(one_module("fx_under", ParamFixtureTLM::kType, {{"gain", 0}})));
    REQUIRE_FALSE(
        factory.instantiateAll(one_module("fx_over", ParamFixtureTLM::kType, {{"gain", 17}})));
}

TEST_CASE("AC-11: 整型参数拒绝 8.0 / true / 1e20；无符号拒绝负值", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    SECTION("JSON double 8.0 ⇒ 失败（不得依赖隐式转换）") {
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx_d", ParamFixtureTLM::kType, {{"gain", 8.0}})));
    }
    SECTION("JSON bool true ⇒ 失败") {
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx_b", ParamFixtureTLM::kType, {{"gain", true}})));
    }
    SECTION("1e20 ⇒ 失败（超 storable range）") {
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx_e", ParamFixtureTLM::kType, {{"gain", 1e20}})));
    }
    SECTION("无符号参数负值 ⇒ 失败；同一值以整数给出 ⇒ 成功") {
        REQUIRE_FALSE(
            factory.instantiateAll(one_module("fx_n", ParamFixtureTLM::kType, {{"gain", -1}})));
        REQUIRE(factory.instantiateAll(one_module("fx_ok", ParamFixtureTLM::kType, {{"gain", 8}})));
    }
    SECTION("RouterTLM 的 buffer_size 同判据（8.0 / true / 1e20 拒绝）") {
        auto with_bs = [](const json& bs) {
            json p = router_params(0, 0, 2, 2);
            p["buffer_size"] = bs;
            return one_module("r", "RouterTLM", p);
        };
        REQUIRE_FALSE(factory.instantiateAll(with_bs(8.0)));
        REQUIRE_FALSE(factory.instantiateAll(with_bs(true)));
        REQUIRE_FALSE(factory.instantiateAll(with_bs(1e20)));
        REQUIRE(factory.instantiateAll(with_bs(8)));
    }
    SECTION("RouterTLM 的 credit_timeout 负值 ⇒ 失败") {
        json p = router_params(0, 0, 2, 2);
        p["credit_timeout"] = -1;
        REQUIRE_FALSE(factory.instantiateAll(one_module("r_ct", "RouterTLM", p)));
    }
}

// ============================================================================
// AC-02 — 行为判别（两个不同合法值 ⇒ 可观测指标不同）
// ============================================================================
TEST_CASE("AC-02: 夹具两个参数各自驱动可观测行为", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;

    ParamFixtureTLM a("a", &eq);
    ParamFixtureTLM b("b", &eq);
    a.set_config(json{{"gain", 2}, {"offset", 0}});
    b.set_config(json{{"gain", 5}, {"offset", 0}});
    REQUIRE(a.compute(10) != b.compute(10)); // gain 影响输出

    ParamFixtureTLM c("c", &eq);
    c.set_config(json{{"gain", 2}, {"offset", 7}});
    REQUIRE(a.compute(10) != c.compute(10)); // offset 影响输出
}

// ============================================================================
// AC-06 — 无 params 键的实例仍获得默认值
// ============================================================================
TEST_CASE("AC-06: 无 params 键的夹具实例 ⇒ 声明默认值落到成员", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    // module entry 完全无 params（夹具成员初值 99/999 故意非法 ⇒ 本断言有判别力）
    json cfg =
        json{{"modules", json::array({json{{"name", "fx_np"}, {"type", ParamFixtureTLM::kType}}})},
             {"connections", json::array()}};
    REQUIRE(factory.instantiateAll(cfg));

    auto* fx = factory.getInstance<ParamFixtureTLM>("fx_np");
    REQUIRE(fx != nullptr);
    CHECK(fx->gain() == 2u);
    CHECK(fx->offset() == 0);
}

// ============================================================================
// AC-07 — 未知键在真实模块上生效
// ============================================================================
TEST_CASE("AC-07: RouterTLM 拼错参数名 ⇒ 装配失败且报出该键名", "[param][loader]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    json p = router_params(0, 0, 2, 2);
    p["buffer_sizes"] = 16; // 拼错（正确为 buffer_size）
    // ⚠️ 本用例**不**调用 ensure_fixture_registered() ⇒ 进程内无夹具参与 ⇒
    //    「夹具手写 throw」这条蒙混路径被封，失败只可能来自基类的未知键检查。
    REQUIRE_FALSE(factory.instantiateAll(one_module("r_typo", "RouterTLM", p)));

    // 错误文本含该键名（直接对实例施压，绕过 instantiateAll 的 catch）
    tlm::RouterTLM r("r_typo_direct", &eq);
    try {
        r.set_config(p);
        FAIL("expected ParamValidationError for unknown key");
    } catch (const cpptlm::ParamValidationError& e) {
        CHECK(e.param_name == "buffer_sizes");
        CHECK(std::string(e.what()).find("unknown parameter") != std::string::npos);
    }
}

// ============================================================================
// AC-10 — 默认值不写回 JSON
// ============================================================================
TEST_CASE("AC-10: 声明的默认值不写回配置 JSON", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    SECTION("夹具：未提供 gain ⇒ params 中不出现 gain") {
        json cfg = one_module("fx_nw", ParamFixtureTLM::kType, json::object());
        REQUIRE(factory.instantiateAll(cfg));
        const json& kept = cfg["modules"][0]["params"];
        CHECK_FALSE(kept.contains("gain"));
        CHECK_FALSE(kept.contains("offset"));
        auto* fx = factory.getInstance<ParamFixtureTLM>("fx_nw");
        REQUIRE(fx != nullptr);
        CHECK(fx->gain() == 2u); // 默认值确实生效（只落成员）
    }

    SECTION("RouterTLM：未提供 buffer_size ⇒ params 中不出现 buffer_size") {
        json cfg = one_module("r_nw", "RouterTLM", router_params(0, 0, 2, 2));
        REQUIRE(factory.instantiateAll(cfg));
        CHECK_FALSE(cfg["modules"][0]["params"].contains("buffer_size"));
    }
}

// ============================================================================
// AC-12 — legacy 扁平路径豁免未知键，但不吞必填
// ============================================================================
TEST_CASE("AC-12: legacy 扁平形式的未知键豁免与必填保留", "[param][loader]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    SECTION("扁平形式（无 params，顶层 node_x…）⇒ 装配成功") {
        json cfg =
            one_module_flat("r_flat", "RouterTLM",
                            json{{"node_x", 1}, {"node_y", 2}, {"mesh_x", 4}, {"mesh_y", 4}});
        REQUIRE(factory.instantiateAll(cfg));
        auto* r = factory.getInstance<tlm::RouterTLM>("r_flat");
        REQUIRE(r != nullptr);
        CHECK(r->node_x() == 1u);
        CHECK(r->mesh_x() == 4u);
    }

    SECTION("扁平形式缺 node_x ⇒ 仍失败（豁免不得吞必填）") {
        json cfg = one_module_flat("r_flat_bad", "RouterTLM",
                                   json{{"node_y", 2}, {"mesh_x", 4}, {"mesh_y", 4}});
        REQUIRE_FALSE(factory.instantiateAll(cfg));
    }
}

// ============================================================================
// G-1 / G-2 — 回归护栏
// ============================================================================
TEST_CASE("G-1: 二次加载幂等（成员值不变）", "[param][loader]") {
    ensure_fixture_registered();
    EventQueue eq;

    ParamFixtureTLM fx("fx_idem", &eq);
    const json cfg = json{{"gain", 5}, {"offset", -3}};
    fx.set_config(cfg);
    const unsigned g1 = fx.gain();
    const int o1 = fx.offset();
    fx.set_config(cfg);
    CHECK(fx.gain() == g1);
    CHECK(fx.offset() == o1);
}

TEST_CASE("G-2: 嵌套 RouterTLM 缺 node_x ⇒ 内部实例不被创建（既存行为）", "[param][loader]") {
    EventQueue eq;
    REGISTER_CHSTREAM;
    ModuleFactory factory(&eq);

    json inner = json{{"name", "inner_r"},
                      {"type", "RouterTLM"},
                      {"params", json{{"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}}}};
    json outer = json{{"name", "outer_c"},
                      {"type", "CpuCluster"},
                      {"params", json::object()},
                      {"modules", json::array({inner})}};
    json cfg = json{{"modules", json::array({outer})}, {"connections", json::array()}};

    // ⚠️ **语义已变更**（change `cpptlm-module-param-migrate-batch2` T0.2 / design D-4）：
    //    本用例原先记录「内层装配失败**不会**让外层返回 false」（旧行为 —— 返回值被丢弃，
    //    登记为已知残留 R-5）。T0.2 之后**内层失败必须传播** ⇒ `instantiateAll` 返回 false。
    //    ⛔ 断言不得再依赖旧语义（否则等于把「失败被静默吞掉」钉成契约）。
    REQUIRE_FALSE(factory.instantiateAll(cfg));

    // 失败时**实例表不提交**（`instances = std::move(object_instances)` 在函数末尾，
    // Step 4.5 的 early-return 到不了那里）⇒ 零半构造残留 —— 与 R-5 的 AC-02 同语义。
    CHECK(factory.getInstance<SimModule>("outer_c") == nullptr);
    CHECK(factory.getInstance("inner_r") == nullptr);
}

// ============================================================================
// AC-09② — credit 同源：同值二次加载不刷新已消耗的 credit
// ============================================================================
namespace {

    /// 向 EAST 口注入：裸 router（无 adapter）⇒ credit 永不归还 ⇒ 转发数封顶于 credit 预算
    std::size_t inject_to_east_budget(tlm::RouterTLM& r, std::size_t inject_count) {
        std::size_t injected = 0;
        std::size_t drained = 0;
        std::uint64_t txid = 700000;
        for (unsigned cyc = 0; cyc < 4000 && drained < inject_count + 64; ++cyc) {
            if (injected < inject_count) {
                bundles::NoCFlitBundle f;
                f.transaction_id.write(txid++);
                f.src_node.write(0);
                f.dst_node.write(1); // ⇒ EAST
                f.vc_id.write(0);
                f.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
                f.flit_index.write(0);
                f.flit_count.write(1);
                f.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);
                r.req_in()[0].data() = f;
                r.req_in()[0].set_valid(true);
                ++injected;
            }
            r.tick();
            if (r.resp_out()[1].valid()) {
                ++drained;
                r.resp_out()[1].clear_valid();
            }
        }
        return drained;
    }

} // namespace

TEST_CASE("AC-09②: 同值二次加载不刷新已消耗的 credit", "[param][loader]") {
    EventQueue eq;
    tlm::RouterTLM r("r_reload", &eq, 0, 0, 2, 2);

    json p = {{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}, {"buffer_size", 4}};
    r.set_config(p);
    REQUIRE(r.buffer_depth() == 4u);

    CHECK(inject_to_east_budget(r, 64) == 4u); // 首次：credit 预算 == 深度

    r.set_config(p); // 二次加载**同值**
    REQUIRE(r.buffer_depth() == 4u);
    // 若二次加载无差别重算 downstream_credits_ ⇒ 已耗尽的 credit 会被刷新 ⇒ 又能转发 4 个
    CHECK(inject_to_east_budget(r, 64) == 0u);
}
