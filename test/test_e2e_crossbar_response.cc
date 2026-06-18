// test/test_e2e_crossbar_response.cc
// CrossbarTLM 端到端响应回路验证（P0-5 bug 修复 RED 测试）
// 标签体系：[e2e][crossbar][response]
// 作者 CppTLM Team / 日期 2026-06-12
//
// 目的：锁定 module_factory.cc:586-600 多端口分支 bug
//   - CrossbarTLM 走 WARN 路径而非 bind_port_pair()，
//   - 响应路径未被正确绑定，导致 resp_out[dst] 永远发不出，
//   - MemoryTLM 收不到请求、永远不响应，
//   - CPU resp_in_ 永远空，last_response_transaction_id() == 0。
// 修复后（PR2 Task 8 用 isMultiPort 统一处理）这两个测试应转为 GREEN。

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/cpu_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "tlm/memory_tlm.hh"
#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static void registerAllModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        REGISTER_MODULE(CpuCluster);
        registered = true;
    }
}

// ============================================================================
// TEST_CASE 1：单 CPU → Crossbar → Memory → Crossbar → CPU 完整响应回路
// 预期：CPU 应至少收到一个响应，且 last_response_transaction_id() > 0
// 当前（P0-5 未修）：响应被静默丢弃，last_response_transaction_id() == 0 → RED
// ============================================================================

TEST_CASE("E2E: CPU→Crossbar→Mem→Crossbar→CPU 完整响应回路",
          "[e2e][crossbar][response][round-trip]") {
    registerAllModules();

    // 1 CPU + 1 4端口 CrossbarTLM + 1 MemoryTLM
    // CPU 写入 xbar.0；xbar 4 端口全部路由到 mem（地址位选路 0x1000/0x2000/0x3000）
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem",  "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cpu0", "dst": "xbar.0", "latency": 1},
            {"src": "xbar.0", "dst": "mem", "latency": 1},
            {"src": "xbar.1", "dst": "mem", "latency": 1},
            {"src": "xbar.2", "dst": "mem", "latency": 1},
            {"src": "xbar.3", "dst": "mem", "latency": 1}
        ]
    })"_json;

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(config));
    factory.startAllTicks();

    auto* cpu0 = dynamic_cast<CPUTLM*>(factory.getInstance("cpu0"));
    REQUIRE(cpu0 != nullptr);

    // 运行足够周期让 CPU 发出请求、Crossbar 路由、Memory 响应、回路返回
    eq.run(200);

    // 关键断言：CPU 必须收到响应（P0-5 修复前 last_response_transaction_id_ == 0）
    INFO("cpu0->last_response_transaction_id() = " << cpu0->last_response_transaction_id());
    REQUIRE(cpu0->last_response_transaction_id() > 0);
}

// ============================================================================
// TEST_CASE 2：4 CPU 同时打 Crossbar 不同端口，响应不丢失
// 预期：每个 CPU 都应收到响应，transaction_id 与发出的请求对齐
// 当前（P0-5 未修）：所有响应丢失 → 全部 RED
// ============================================================================

TEST_CASE("E2E: CrossbarTLM 4 端口并发响应不丢失", "[e2e][crossbar][response][concurrent]") {
    registerAllModules();

    // 4 CPU → 各自占用 xbar 一个端口 → 4 Memory（隔离地址段避免 Crossbar 冲突）
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "cpu1", "type": "CPUTLM"},
            {"name": "cpu2", "type": "CPUTLM"},
            {"name": "cpu3", "type": "CPUTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem0", "type": "MemoryTLM"},
            {"name": "mem1", "type": "MemoryTLM"},
            {"name": "mem2", "type": "MemoryTLM"},
            {"name": "mem3", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cpu0", "dst": "xbar.0", "latency": 1},
            {"src": "cpu1", "dst": "xbar.1", "latency": 1},
            {"src": "cpu2", "dst": "xbar.2", "latency": 1},
            {"src": "cpu3", "dst": "xbar.3", "latency": 1},
            {"src": "xbar.0", "dst": "mem0", "latency": 1},
            {"src": "xbar.1", "dst": "mem1", "latency": 1},
            {"src": "xbar.2", "dst": "mem2", "latency": 1},
            {"src": "xbar.3", "dst": "mem3", "latency": 1}
        ]
    })"_json;

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(config));
    factory.startAllTicks();

    auto* cpu0 = dynamic_cast<CPUTLM*>(factory.getInstance("cpu0"));
    auto* cpu1 = dynamic_cast<CPUTLM*>(factory.getInstance("cpu1"));
    auto* cpu2 = dynamic_cast<CPUTLM*>(factory.getInstance("cpu2"));
    auto* cpu3 = dynamic_cast<CPUTLM*>(factory.getInstance("cpu3"));
    REQUIRE(cpu0 != nullptr);
    REQUIRE(cpu1 != nullptr);
    REQUIRE(cpu2 != nullptr);
    REQUIRE(cpu3 != nullptr);

    // 运行足够周期让 4 个 CPU 都发出请求、4 个 Memory 都响应、4 条回路都返回
    eq.run(300);

    // 关键断言：每个 CPU 都必须收到响应（P0-5 未修时 4 个全部 == 0）
    INFO("cpu0 last_response_transaction_id = " << cpu0->last_response_transaction_id());
    INFO("cpu1 last_response_transaction_id = " << cpu1->last_response_transaction_id());
    INFO("cpu2 last_response_transaction_id = " << cpu2->last_response_transaction_id());
    INFO("cpu3 last_response_transaction_id = " << cpu3->last_response_transaction_id());
    REQUIRE(cpu0->last_response_transaction_id() > 0);
    REQUIRE(cpu1->last_response_transaction_id() > 0);
    REQUIRE(cpu2->last_response_transaction_id() > 0);
    REQUIRE(cpu3->last_response_transaction_id() > 0);
}