// test/test_simmodule_exposed_port_wiring.cc
// SimModule **暴露端口**的真实数据通路径接入判据
//
// 关联 change:`cpptlm-simmodule-exposed-port-wiring`
// 关联决策:`docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`(Proposed)
//
// 覆盖 proposal AC-01 ~ AC-04 + AC-08(既有 SimModule 自身端口引用不受影响)
//
// 作者 CppTLM Team / 日期 2026-11-16
#include <catch2/catch_all.hpp>

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using json = nlohmann::json;
using namespace cpptlm;

namespace {

/// 测试专用容器：⛔ 不做任何事，只用 `SimModule` 基类的**内联 modules 能力**
/// （`SimModule::simulate_instantiate` 读 cfg["modules"] → 内部工厂装配）
class TestContainer : public SimModule {
public:
    TestContainer(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    std::string get_module_type() const override { return "TestContainer"; }
};

bool g_registered = false;
void ensure_registered() {
    if (!g_registered) {
        ModuleFactory::registerModule<TestContainer>("TestContainer");
        g_registered = true;
    }
}

std::filesystem::path repo_root() {
    // 仓根定位：configs/ 或 test/ 皆可
    std::filesystem::path p = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (std::filesystem::exists(p / "configs") && std::filesystem::exists(p / "test")) {
            return p;
        }
        p = p.parent_path();
    }
    return std::filesystem::current_path();
}

/// 单个 router 子模块（5 端口：NORTH/EAST/SOUTH/WEST/LOCAL=4）
json router_module(const char* name) {
    return json{{"name", name},
                {"type", "RouterTLM"},
                {"params", {{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 2}}}};
}

/// 容器 + 1 个 router 子模块 + 暴露 4 个 LOCAL 通道
json container_with_exposed(const char* cname, const char* rname) {
    // ⚠️ 模块条目必须同时含 `connections` 数组 —— `ModuleFactory::validateConfig` 要求
    //    顶层同时有 `modules` 与 `connections`（既有嵌套 config 均遵循此约定，
    //    见 configs/example_simmodule_nested_2level.json）。缺失 ⇒ 内部工厂校验失败
    //    且失败被 `SimModule::simulate_instantiate` **静默忽略** ⇒ 子模块不装配。
    json c{{"name", cname},
           {"type", "TestContainer"},
           {"modules", json::array({router_module(rname)})},
           {"connections", json::array()}};
    c["outputs"] = json::array({
        json{{"internal", std::string(rname) + ".req_out[4]"}, {"external", "p_req_out"}},
        json{{"internal", std::string(rname) + ".resp_out[4]"}, {"external", "p_resp_out"}},
    });
    c["inputs"] = json::array({
        json{{"internal", std::string(rname) + ".req_in[4]"}, {"external", "p_req_in"}},
        json{{"internal", std::string(rname) + ".resp_in[4]"}, {"external", "p_resp_in"}},
    });
    return c;
}

/// 取某模块 PortManager 上按镜像名取的端口（ChStream 端口在 Step 7 被 mirror 进来）
SimplePort* down_port(SimObject* m, const std::string& label) {
    if (!m || !m->hasPortManager()) return nullptr;
    return m->getPortManager().getDownstreamPort(label);
}
SimplePort* up_port(SimObject* m, const std::string& label) {
    if (!m || !m->hasPortManager()) return nullptr;
    return m->getPortManager().getUpstreamPort(label);
}

}  // namespace

// ---------------------------------------------------------------------------
// AC-01: exposed 连接建立**真实 PortPair**（两侧 isBound() == true）
// ---------------------------------------------------------------------------
TEST_CASE("EXP-1 (AC-01): exposed 端点与顶层 ChStream 端点建立真实 PortPair",
          "[simmodule][exposed]") {
    ensure_registered();
    json cfg{
        {"modules", json::array({container_with_exposed("box", "r0"),
                                 json{{"name", "peer"}, {"type", "RouterTLM"},
                                      {"params", {{"node_x", 1}, {"node_y", 0},
                                                  {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections",
         json::array({
             // peer 的 req_out[1] → 容器内部 r0.req_in[4]
             json{{"src", "peer.1"}, {"dst", "box.p_req_in"}},
             // 容器内部 r0.req_out[4] → peer 的 req_in[1]
             json{{"src", "box.p_req_out"}, {"dst", "peer.1"}},
         })},
    };

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* box = dynamic_cast<SimModule*>(factory.getInstance("box"));
    auto* peer = factory.getInstance("peer");
    REQUIRE(box != nullptr);
    REQUIRE(peer != nullptr);

    // 容器侧：内部 router 的 req_in[4] / req_out[4] 均已绑定
    auto* internal_in = box->getInternalInputPort("r0.req_in[4]");
    auto* internal_out = box->getInternalOutputPort("r0.req_out[4]");
    REQUIRE(internal_in != nullptr);
    REQUIRE(internal_out != nullptr);
    REQUIRE(internal_in->isBound());
    REQUIRE(internal_out->isBound());

    // 对端侧：peer 的 req_out[1] / req_in[1] 均已绑定
    auto* peer_out = down_port(peer, "req_out[1]");
    auto* peer_in = up_port(peer, "req_in[1]");
    REQUIRE(peer_out != nullptr);
    REQUIRE(peer_in != nullptr);
    REQUIRE(peer_out->isBound());
    REQUIRE(peer_in->isBound());
}

// ---------------------------------------------------------------------------
// AC-02: 方向按**内部端口后缀**推导（4 组）+ CpuCluster 型 req_out 反例
// ---------------------------------------------------------------------------
TEST_CASE("EXP-2 (AC-02): 方向按内部端口后缀推导 —— 4 组配对逐项成立",
          "[simmodule][exposed]") {
    ensure_registered();
    json cfg{
        {"modules", json::array({container_with_exposed("box", "r0"),
                                 json{{"name", "peer"}, {"type", "RouterTLM"},
                                      {"params", {{"node_x", 1}, {"node_y", 0},
                                                  {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections",
         json::array({
             json{{"src", "peer.1"}, {"dst", "box.p_req_in"}},     // req_in  ↔ peer.req_out
             json{{"src", "peer.1"}, {"dst", "box.p_resp_in"}},    // resp_in ↔ peer.resp_out
             json{{"src", "box.p_req_out"}, {"dst", "peer.1"}},    // req_out ↔ peer.req_in
             json{{"src", "box.p_resp_out"}, {"dst", "peer.1"}},   // resp_out ↔ peer.resp_in
         })},
    };

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));

    auto* box = dynamic_cast<SimModule*>(factory.getInstance("box"));
    auto* peer = factory.getInstance("peer");
    REQUIRE(box != nullptr);
    REQUIRE(peer != nullptr);

    // peer 的 4 个通道都应被绑定（各绑一次）
    REQUIRE(down_port(peer, "req_out[1]")->isBound());
    REQUIRE(down_port(peer, "resp_out[1]")->isBound());
    REQUIRE(up_port(peer, "req_in[1]")->isBound());
    REQUIRE(up_port(peer, "resp_in[1]")->isBound());

    // 容器侧 4 个通道也都绑定
    REQUIRE(box->getInternalInputPort("r0.req_in[4]")->isBound());
    REQUIRE(box->getInternalInputPort("r0.resp_in[4]")->isBound());
    REQUIRE(box->getInternalOutputPort("r0.req_out[4]")->isBound());
    REQUIRE(box->getInternalOutputPort("r0.resp_out[4]")->isBound());
}

// ---------------------------------------------------------------------------
// AC-03: 端口已绑定 ⇒ **报错**（⛔ 不得静默覆盖既有 PortPair）
// ---------------------------------------------------------------------------
TEST_CASE("EXP-3 (AC-03): 同一 exposed 端口二次连接 ⇒ 装配期报错（不静默覆盖）",
          "[simmodule][exposed]") {
    ensure_registered();
    json cfg{
        {"modules", json::array({container_with_exposed("box", "r0"),
                                 json{{"name", "peer"}, {"type", "RouterTLM"},
                                      {"params", {{"node_x", 1}, {"node_y", 0},
                                                  {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections",
         json::array({
             json{{"src", "peer.1"}, {"dst", "box.p_req_in"}},
             // 第二次绑定**同一个**内部端口 req_in[4] ⇒ 必须报错
             json{{"src", "peer.2"}, {"dst", "box.p_req_in"}},
         })},
    };

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

// ---------------------------------------------------------------------------
// AC-04: 五类失败 ⇒ 硬错误（装配失败），不得静默丢连接
// ---------------------------------------------------------------------------
TEST_CASE("EXP-4 (AC-04a): 声明过 exposed 的容器上引用**未声明**的标签 ⇒ 报错",
          "[simmodule][exposed]") {
    ensure_registered();
    json cfg{
        {"modules", json::array({container_with_exposed("box", "r0"),
                                 json{{"name", "peer"}, {"type", "RouterTLM"},
                                      {"params", {{"node_x", 1}, {"node_y", 0},
                                                  {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections", json::array({json{{"src", "peer.1"}, {"dst", "box.p_not_declared"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

TEST_CASE("EXP-5 (AC-04b): 内部端口不可解析（含不存在的内部模块）⇒ 报错",
          "[simmodule][exposed]") {
    ensure_registered();
    json box{{"name", "box"},
             {"type", "TestContainer"},
             {"modules", json::array({router_module("r0")})},
             {"connections", json::array()}};
    box["outputs"] = json::array({json{{"internal", "rX.req_out[4]"}, {"external", "p_bad"}}});
    json cfg{
        {"modules",
         json::array({box, json{{"name", "peer"}, {"type", "RouterTLM"},
                                {"params", {{"node_x", 1}, {"node_y", 0},
                                            {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections", json::array({json{{"src", "box.p_bad"}, {"dst", "peer.1"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

TEST_CASE("EXP-6 (AC-04b'): 嵌套容器暴露 ⇒ 报错（当前不支持）", "[simmodule][exposed]") {
    ensure_registered();
    // 内层容器 inner 持有 router r0；外层容器 outer 暴露 inner 的端口 ⇒ 路径含 2 个点
    json inner{{"name", "inner"},
               {"type", "TestContainer"},
               {"modules", json::array({router_module("r0")})},
               {"connections", json::array()}};
    json outer{{"name", "outer"},
               {"type", "TestContainer"},
               {"modules", json::array({inner})},
               {"connections", json::array()}};
    outer["outputs"] = json::array({json{{"internal", "inner.r0.req_out[4]"}, {"external", "p_nested"}}});
    json cfg{
        {"modules",
         json::array({outer, json{{"name", "peer"}, {"type", "RouterTLM"},
                                  {"params", {{"node_x", 1}, {"node_y", 0},
                                              {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections", json::array({json{{"src", "outer.p_nested"}, {"dst", "peer.1"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

TEST_CASE("EXP-7 (AC-04c): 对端索引越界 ⇒ 报错", "[simmodule][exposed]") {
    ensure_registered();
    json cfg{
        {"modules", json::array({container_with_exposed("box", "r0"),
                                 json{{"name", "peer"}, {"type", "RouterTLM"},
                                      {"params", {{"node_x", 1}, {"node_y", 0},
                                                  {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections", json::array({json{{"src", "peer.99"}, {"dst", "box.p_req_in"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

TEST_CASE("EXP-8 (AC-04d): 内部端口后缀不属四类 ⇒ 报错", "[simmodule][exposed]") {
    ensure_registered();
    json box{{"name", "box"},
             {"type", "TestContainer"},
             {"modules", json::array({router_module("r0")})},
             {"connections", json::array()}};
    box["outputs"] = json::array({json{{"internal", "r0.custom_out"}, {"external", "p_custom"}}});
    json cfg{
        {"modules",
         json::array({box, json{{"name", "peer"}, {"type", "RouterTLM"},
                                {"params", {{"node_x", 1}, {"node_y", 0},
                                            {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        {"connections", json::array({json{{"src", "box.p_custom"}, {"dst", "peer.1"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

TEST_CASE("EXP-9 (AC-04e): 连接**两端都是** exposed ⇒ 报错（当前仅支持一端 exposed）",
          "[simmodule][exposed]") {
    ensure_registered();
    json cfg{
        {"modules", json::array({container_with_exposed("boxA", "r0"),
                                 container_with_exposed("boxB", "r1")})},
        {"connections", json::array({json{{"src", "boxA.p_req_out"}, {"dst", "boxB.p_req_in"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_FALSE(factory.instantiateAll(cfg));
}

// ---------------------------------------------------------------------------
// AC-08: 未声明 exposed 的 SimModule，其**自身普通端口**引用行为零变化
// ---------------------------------------------------------------------------
TEST_CASE("EXP-10 (AC-08): 未声明 exposed 的 SimModule 自身端口引用不受影响",
          "[simmodule][exposed]") {
    ensure_registered();
    // box **不声明** outputs/inputs ⇒ 其自身 PortManager 上的普通端口引用走既有语义
    json box{{"name", "box"},
             {"type", "TestContainer"},
             {"modules", json::array({router_module("r0")})},
             {"connections", json::array()}};
    json cfg{
        {"modules",
         json::array({box, json{{"name", "peer"}, {"type", "RouterTLM"},
                                {"params", {{"node_x", 1}, {"node_y", 0},
                                            {"mesh_x", 2}, {"mesh_y", 2}}}}})},
        // `box.r0.req_out[4]` 指向容器**内部** router 的端口（未声明 exposed 的容器）
        {"connections", json::array({json{{"src", "peer.1"}, {"dst", "box.r0.req_in[4]"}}})},
    };
    EventQueue eq;
    ModuleFactory factory(&eq);
    // ⛔ 不报错（该引用不是 exposed 端点）—— 既有语义下这条连接本就不被接通
    REQUIRE(factory.instantiateAll(cfg));
    auto* box_ptr = dynamic_cast<SimModule*>(factory.getInstance("box"));
    REQUIRE(box_ptr != nullptr);
    auto* internal_in = box_ptr->getInternalInputPort("r0.req_in[4]");
    REQUIRE(internal_in != nullptr);
    REQUIRE_FALSE(internal_in->isBound());
}
