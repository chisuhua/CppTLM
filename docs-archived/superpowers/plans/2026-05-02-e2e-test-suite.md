# End-to-End Test Suite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a comprehensive Catch2 end-to-end test suite that validates every registered module type through JSON config → instantiation → simulation → payload verification, covering all 12 production module types, 6 active config topologies, and the cpptlm_sim CLI binary.

**Architecture:** Single test file `test/test_e2e_simulation.cc` using the standard test pattern (static registration guard → EventQueue → ModuleFactory → instantiateAll → startAllTicks → eq.run → payload verify), organized by module domain with Catch2 tags for selective execution. A companion CI test script `scripts/ci_e2e_test.sh` validates the `cpptlm_sim` CLI binary end-to-end.

**Tech Stack:** Catch2 v3.5.0, nlohmann/json, C++17, CMake GLOB auto-discovery, Bash for CLI test script

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `test/test_e2e_simulation.cc` | **Create** | Main e2e test suite — 15 TEST_CASEs covering all 12 module types + 3 topology-level tests |
| `scripts/ci_e2e_test.sh` | **Create** | CI script to run cpptlm_sim against all valid config files, verify exit codes and output |
| `test/CMakeLists.txt` | **No change** | GLOB auto-discovers `test_*.cc` — no CMake modification needed |
| `configs/` | **No change** | All 6 active TLM configs already have proper `params` (fixed in prior commits) |

---

### Task 1: Initialize Test File with Shared Infrastructure

**Files:**
- Create: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Create the test file with includes and registration guard**

Write the file header, includes, and static registration guard:

```cpp
// test/test_e2e_simulation.cc
// 端到端仿真测试：覆盖所有已注册模块类型的JSON配置加载、实例化、仿真运行和payload验证
// 标签体系：[e2e][module-type][topology][sim]

#include <catch2/catch_all.hpp>
#include "modules.hh"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "framework/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
using namespace bundles;

// ============================================================================
// 共享基础设施
// ============================================================================

// 一次性注册所有模块类型（static guard 防止重复注册）
static void registerAllModules() {
    static bool registered = false;
    if (!registered) {
        REGISTER_OBJECT;
        REGISTER_CHSTREAM;
        REGISTER_MODULE;
        registered = true;
    }
}

// 从 configs/ 加载 JSON 配置文件
static json loadConfig(const std::string& relative_path) {
    std::string full_path = std::string(CPPTLM_SOURCE_DIR) + "/" + relative_path;
    std::ifstream f(full_path);
    REQUIRE(f.is_open());
    return json::parse(f);
}

// 标准仿真启动：EventQueue → 注册 → 工厂 → 实例化 → startAllTicks
static std::tuple<EventQueue, ModuleFactory>
setupSimulation(const json& config) {
    EventQueue eq;
    registerAllModules();
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(config));
    factory.startAllTicks();
    return {std::move(eq), std::move(factory)};
}
```

- [ ] **Step 2: Verify the file compiles (empty test)**

Add a placeholder test:

```cpp
TEST_CASE("E2E: 测试基础设施可用", "[e2e][infra]") {
    registerAllModules();
    EventQueue eq;
    REQUIRE(eq.getCurrentCycle() == 0);
    SUCCEED("Infrastructure initialized");
}
```

Build and run:

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][infra]"
```

Expected: 1 test PASS

- [ ] **Step 3: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e test infrastructure with registration guard"
```

---

### Task 2: Single-Port TLM Module Tests (CacheTLM, MemoryTLM, LinkTLM)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write CacheTLM → MemoryTLM direct connection test**

```cpp
TEST_CASE("E2E: CacheTLM → MemoryTLM 直接连接", "[e2e][cache][memory][single-port]") {
    json config = R"({
        "modules": [
            {"name": "cache", "type": "CacheTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cache", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    // 验证模块实例化
    REQUIRE(factory.getInstance("cache") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);
    REQUIRE(factory.getInstance("cache")->get_module_type() == "CacheTLM");
    REQUIRE(factory.getInstance("mem")->get_module_type() == "MemoryTLM");

    // 注入请求
    auto* cache = dynamic_cast<CacheTLM*>(factory.getInstance("cache"));
    CacheReqBundle req;
    req.transaction_id.write(1);
    req.address.write(0x1000);
    req.is_write.write(0);
    req.size.write(4);
    cache->req_in().data() = req;
    cache->req_in().set_valid(true);

    // 运行仿真（需要足够周期让 req → resp 完成）
    eq.run(20);

    // 验证 Cache 收到响应
    REQUIRE(cache->resp_out().valid() == true);
    auto resp = cache->resp_out().data();
    REQUIRE(resp.transaction_id.read() == 1);
    REQUIRE(resp.error_code.read() == 0);

    SUCCEED("CacheTLM→MemoryTLM round-trip successful");
}
```

- [ ] **Step 2: Write LinkTLM delay test (NoC domain, single port)**

```cpp
TEST_CASE("E2E: LinkTLM 延迟队列", "[e2e][link][noc][single-port]") {
    json config = R"({
        "modules": [
            {"name": "link", "type": "LinkTLM", "params": {"latency": 5}},
            {"name": "router_a", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 1}},
            {"name": "router_b", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "router_a.1", "dst": "router_b.0", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);
    REQUIRE(factory.getInstance("router_a") != nullptr);
    REQUIRE(factory.getInstance("router_b") != nullptr);

    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == 50);

    SUCCEED("LinkTLM + 2 RouterTLM topology instantiated and ran");
}
```

- [ ] **Step 3: Build and run the tests**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][single-port]"
```

Expected: 2 tests PASS

- [ ] **Step 4: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e single-port TLM tests (Cache, Memory, Link)"
```

---

### Task 3: Multi-Port TLM Module Tests (CrossbarTLM, ArbiterTLM2, ArbiterTLM4)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write CrossbarTLM 4-port routing test**

```cpp
TEST_CASE("E2E: CrossbarTLM 4端口路由", "[e2e][crossbar][multi-port]") {
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "cpu1", "type": "CPUTLM"},
            {"name": "xbar", "type": "CrossbarTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cpu0", "dst": "xbar.0", "latency": 1},
            {"src": "cpu1", "dst": "xbar.1", "latency": 1},
            {"src": "xbar.0", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    REQUIRE(factory.getInstance("xbar") != nullptr);
    REQUIRE(factory.getInstance("cpu0") != nullptr);
    REQUIRE(factory.getInstance("cpu1") != nullptr);
    REQUIRE(factory.getInstance("mem") != nullptr);

    // CrossbarTLM 应有 4 个端口
    auto* xbar = dynamic_cast<CrossbarTLM*>(factory.getInstance("xbar"));
    REQUIRE(xbar->num_ports() == 4);

    // CPUTLM 自主发送请求，运行足够周期
    eq.run(100);

    // 验证 cycle 推进
    REQUIRE(eq.getCurrentCycle() == 100);
    SUCCEED("CrossbarTLM multi-port topology ran without errors");
}
```

- [ ] **Step 2: Write ArbiterTLM2 2-requestor round-robin test**

```cpp
TEST_CASE("E2E: ArbiterTLM2 双请求者轮转仲裁", "[e2e][arbiter][multi-port]") {
    json config = R"({
        "modules": [
            {"name": "cpu0", "type": "CPUTLM"},
            {"name": "cpu1", "type": "CPUTLM"},
            {"name": "arb", "type": "ArbiterTLM2"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cpu0", "dst": "arb.0", "latency": 1},
            {"src": "cpu1", "dst": "arb.1", "latency": 1},
            {"src": "arb.0", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    auto* arb = dynamic_cast<ArbiterTLM<2>*>(factory.getInstance("arb"));
    REQUIRE(arb != nullptr);
    REQUIRE(arb->num_ports() == 2);

    eq.run(100);
    REQUIRE(eq.getCurrentCycle() == 100);
    SUCCEED("ArbiterTLM2 round-robin arbitration exercised");
}
```

- [ ] **Step 3: Write ArbiterTLM4 4-requestor test**

```cpp
TEST_CASE("E2E: ArbiterTLM4 四请求者轮转仲裁", "[e2e][arbiter][multi-port]") {
    json config = R"({
        "modules": [
            {"name": "tg0", "type": "TrafficGenTLM"},
            {"name": "tg1", "type": "TrafficGenTLM"},
            {"name": "tg2", "type": "TrafficGenTLM"},
            {"name": "tg3", "type": "TrafficGenTLM"},
            {"name": "arb", "type": "ArbiterTLM4"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "tg0", "dst": "arb.0", "latency": 1},
            {"src": "tg1", "dst": "arb.1", "latency": 1},
            {"src": "tg2", "dst": "arb.2", "latency": 1},
            {"src": "tg3", "dst": "arb.3", "latency": 1},
            {"src": "arb.0", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);
    auto* arb = dynamic_cast<ArbiterTLM<4>*>(factory.getInstance("arb"));
    REQUIRE(arb != nullptr);
    REQUIRE(arb->num_ports() == 4);

    eq.run(200);
    REQUIRE(eq.getCurrentCycle() == 200);
    SUCCEED("ArbiterTLM4 topology ran without errors");
}
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][multi-port]"
```

Expected: 3 tests PASS

- [ ] **Step 5: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e multi-port TLM tests (Crossbar, Arbiter2, Arbiter4)"
```

---

### Task 4: Initiator Module Tests (CPUTLM, TrafficGenTLM)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write CPUTLM autonomous operation test**

```cpp
TEST_CASE("E2E: CPUTLM 自主发起请求", "[e2e][cpu][initiator]") {
    json config = R"({
        "modules": [
            {"name": "cpu", "type": "CPUTLM"},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "cpu", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    auto* cpu = dynamic_cast<CPUTLM*>(factory.getInstance("cpu"));
    REQUIRE(cpu != nullptr);

    // CPUTLM 每 ~10 cycles 自动发送请求，最多 4 个 in-flight
    eq.run(100);
    REQUIRE(eq.getCurrentCycle() == 100);
    SUCCEED("CPUTLM autonomous operation exercised");
}
```

- [ ] **Step 2: Write TrafficGenTLM sequential pattern test**

```cpp
TEST_CASE("E2E: TrafficGenTLM 顺序请求模式", "[e2e][traffic-gen][initiator]") {
    json config = R"({
        "modules": [
            {"name": "tg", "type": "TrafficGenTLM",
             "params": {"num_requests": 10, "gen_mode": "SEQUENTIAL"}},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "tg", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    auto* tg = dynamic_cast<TrafficGenTLM*>(factory.getInstance("tg"));
    REQUIRE(tg != nullptr);

    eq.run(200);
    REQUIRE(eq.getCurrentCycle() == 200);
    SUCCEED("TrafficGenTLM SEQUENTIAL mode exercised");
}
```

- [ ] **Step 3: Write TrafficGenTLM random pattern test**

```cpp
TEST_CASE("E2E: TrafficGenTLM 随机请求模式", "[e2e][traffic-gen][initiator]") {
    json config = R"({
        "modules": [
            {"name": "tg", "type": "TrafficGenTLM",
             "params": {"num_requests": 10, "gen_mode": "RANDOM"}},
            {"name": "mem", "type": "MemoryTLM"}
        ],
        "connections": [
            {"src": "tg", "dst": "mem", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);
    auto* tg = dynamic_cast<TrafficGenTLM*>(factory.getInstance("tg"));
    REQUIRE(tg != nullptr);

    eq.run(200);
    REQUIRE(eq.getCurrentCycle() == 200);
    SUCCEED("TrafficGenTLM RANDOM mode exercised");
}
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][initiator]"
```

Expected: 3 tests PASS

- [ ] **Step 5: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e initiator TLM tests (CPU, TrafficGen sequential+random)"
```

---

### Task 5: NoC Domain Tests (RouterTLM, NICTLM)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write NIC→Router→NIC minimal topology test**

```cpp
TEST_CASE("E2E: NICTLM → RouterTLM → NICTLM 最小NoC拓扑", "[e2e][nic][router][noc]") {
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM", "params": {"node_id": 0, "mesh_x": 2, "mesh_y": 1}},
            {"name": "router", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 1}},
            {"name": "ni1", "type": "NICTLM", "params": {"node_id": 1, "mesh_x": 2, "mesh_y": 1}}
        ],
        "connections": [
            {"src": "ni0.1", "dst": "router.4", "latency": 1},
            {"src": "router.4", "dst": "ni1.1", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    REQUIRE(factory.getInstance("ni0") != nullptr);
    REQUIRE(factory.getInstance("router") != nullptr);
    REQUIRE(factory.getInstance("ni1") != nullptr);

    auto* router = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("router"));
    REQUIRE(router != nullptr);
    REQUIRE(router->num_ports() == 5);  // N/E/S/W/Local
    REQUIRE(router->node_x() == 0);
    REQUIRE(router->node_y() == 0);
    REQUIRE(router->mesh_x() == 2);
    REQUIRE(router->mesh_y() == 1);

    // RouterTLM 有 6 阶段流水线，需要足够周期
    eq.run(200);
    REQUIRE(eq.getCurrentCycle() == 200);
    SUCCEED("NIC→Router→NIC topology instantiated and ran");
}
```

- [ ] **Step 2: Write RouterTLM XY routing correctness test**

```cpp
TEST_CASE("E2E: RouterTLM XY路由算法", "[e2e][router][routing][xy]") {
    json config = R"({
        "modules": [
            {"name": "r00", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 0, "mesh_x": 2, "mesh_y": 2}},
            {"name": "r01", "type": "RouterTLM", "params": {"node_x": 0, "node_y": 1, "mesh_x": 2, "mesh_y": 2}},
            {"name": "r10", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 0, "mesh_x": 2, "mesh_y": 2}},
            {"name": "r11", "type": "RouterTLM", "params": {"node_x": 1, "node_y": 1, "mesh_x": 2, "mesh_y": 2}}
        ],
        "connections": [
            {"src": "r00.2", "dst": "r01.0", "latency": 1},
            {"src": "r00.1", "dst": "r10.3", "latency": 1},
            {"src": "r01.1", "dst": "r11.3", "latency": 1},
            {"src": "r10.2", "dst": "r11.0", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    auto* r00 = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("r00"));
    REQUIRE(r00 != nullptr);
    REQUIRE(r00->node_id() == 0);  // (0,0) → node_id = y*mesh_x+x = 0*2+0 = 0

    auto* r11 = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("r11"));
    REQUIRE(r11 != nullptr);
    REQUIRE(r11->node_id() == 3);  // (1,1) → node_id = 1*2+1 = 3

    // XY 路由计算：(0,0) → (1,1) 应走 EAST(port 1) → SOUTH(port 2) 或 SOUTH → EAST
    unsigned route = r00->compute_xy_route(3);  // dst_node_id = 3
    // XY 路由：先走 X 方向到同列，再走 Y 方向
    REQUIRE((route == 1 || route == 2));  // EAST 或 SOUTH

    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == 50);
    SUCCEED("RouterTLM XY routing verified");
}
```

- [ ] **Step 3: Write NICTLM address mapping + flit packetize test**

```cpp
TEST_CASE("E2E: NICTLM 地址映射与Flit分包", "[e2e][nic][packetize]") {
    json config = R"({
        "modules": [
            {"name": "ni", "type": "NICTLM", "params": {
                "node_id": 5,
                "mesh_x": 4,
                "mesh_y": 4,
                "address_regions": [
                    {"base": 268435456, "size": 67108864, "target_node": 7, "target_type": "MEMORY_CTRL"},
                    {"base": 0, "size": 268435456, "target_node": 3, "target_type": "MEMORY_CTRL"}
                ]
            }}
        ],
        "connections": []
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    auto* ni = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni"));
    REQUIRE(ni != nullptr);
    REQUIRE(ni->node_id() == 5);
    REQUIRE(ni->mesh_x() == 4);
    REQUIRE(ni->mesh_y() == 4);

    // 验证地址映射
    REQUIRE(ni->lookup_node(0x10000000) == 7);   // 0x1000_0000 在 region 0 (256MB-320MB)
    REQUIRE(ni->lookup_node(0x00001000) == 3);   // 0x0000_1000 在 region 1 (0-256MB)

    eq.run(10);
    REQUIRE(eq.getCurrentCycle() == 10);
    SUCCEED("NICTLM address regions and flit packetization configured");
}
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][noc]"
```

Expected: 3 tests PASS

- [ ] **Step 5: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e NoC domain tests (Router, NIC, XY routing, flit)"
```

---

### Task 6: Topology-Level E2E Tests (Load External Config Files)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write mesh_2x2.json load + run test**

```cpp
TEST_CASE("E2E: mesh_2x2拓扑 完整加载与仿真", "[e2e][topology][mesh][external-config]") {
    auto config = loadConfig("configs/mesh_2x2.json");
    auto [eq, factory] = setupSimulation(config);

    // 验证所有模块已实例化
    REQUIRE(factory.getInstance("router_0_0") != nullptr);
    REQUIRE(factory.getInstance("router_0_1") != nullptr);
    REQUIRE(factory.getInstance("router_1_0") != nullptr);
    REQUIRE(factory.getInstance("router_1_1") != nullptr);
    REQUIRE(factory.getInstance("ni_0_0") != nullptr);
    REQUIRE(factory.getInstance("ni_0_1") != nullptr);
    REQUIRE(factory.getInstance("ni_1_0") != nullptr);
    REQUIRE(factory.getInstance("ni_1_1") != nullptr);
    REQUIRE(factory.getInstance("proc_0_0") != nullptr);
    REQUIRE(factory.getInstance("proc_0_1") != nullptr);
    REQUIRE(factory.getInstance("proc_1_0") != nullptr);
    REQUIRE(factory.getInstance("proc_1_1") != nullptr);

    // 验证 Router params
    auto* r00 = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("router_0_0"));
    REQUIRE(r00->node_x() == 0);
    REQUIRE(r00->node_y() == 0);
    REQUIRE(r00->mesh_x() == 2);
    REQUIRE(r00->mesh_y() == 2);

    // 验证 NIC params
    auto* ni00 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni_0_0"));
    REQUIRE(ni00->node_id() == 0);

    // 运行仿真
    eq.run(100);
    REQUIRE(eq.getCurrentCycle() == 100);
    SUCCEED("mesh_2x2.json topology fully loaded and simulated");
}
```

- [ ] **Step 2: Write mesh_4x4.json large topology load + run test**

```cpp
TEST_CASE("E2E: mesh_4x4大规模拓扑 完整加载与仿真", "[e2e][topology][mesh][large][external-config]") {
    auto config = loadConfig("configs/mesh_4x4.json");
    auto [eq, factory] = setupSimulation(config);

    // 验证所有 16 个 Router 已实例化
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            char name[32];
            snprintf(name, sizeof(name), "router_%d_%d", x, y);
            auto* r = dynamic_cast<tlm::RouterTLM*>(factory.getInstance(name));
            REQUIRE(r != nullptr);
            REQUIRE(r->node_x() == (unsigned)x);
            REQUIRE(r->node_y() == (unsigned)y);
            REQUIRE(r->mesh_x() == 4);
            REQUIRE(r->mesh_y() == 4);
        }
    }

    // 验证所有 16 个 NIC 已实例化
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            char name[32];
            snprintf(name, sizeof(name), "ni_%d_%d", x, y);
            auto* ni = dynamic_cast<tlm::NICTLM*>(factory.getInstance(name));
            REQUIRE(ni != nullptr);
            REQUIRE(ni->node_id() == (unsigned)(y * 4 + x));
        }
    }

    eq.run(100);
    REQUIRE(eq.getCurrentCycle() == 100);
    SUCCEED("mesh_4x4.json large topology (48 modules) fully loaded and simulated");
}
```

- [ ] **Step 3: Write hierarchical_2x2.json load + run test**

```cpp
TEST_CASE("E2E: hierarchical_2x2分层拓扑 完整加载与仿真", "[e2e][topology][hierarchical][external-config]") {
    auto config = loadConfig("configs/hierarchical_2x2.json");
    auto [eq, factory] = setupSimulation(config);

    REQUIRE(factory.getInstance("root") != nullptr);
    REQUIRE(factory.getInstance("l1_n0") != nullptr);
    REQUIRE(factory.getInstance("l1_n1") != nullptr);

    auto* root = dynamic_cast<tlm::RouterTLM*>(factory.getInstance("root"));
    REQUIRE(root != nullptr);
    REQUIRE(root->node_x() == 0);
    REQUIRE(root->node_y() == 0);

    eq.run(100);
    REQUIRE(eq.getCurrentCycle() == 100);
    SUCCEED("hierarchical_2x2.json topology loaded and simulated");
}
```

- [ ] **Step 4: Write all 5 TLM configs batch load test**

```cpp
TEST_CASE("E2E: 批量加载所有TLM配置文件", "[e2e][config][batch][external-config]") {
    std::vector<std::string> config_files = {
        "configs/crossbar_test.json",
        "configs/cache_chstream_test.json",
        "configs/cpu_tlm_test.json",
        "configs/traffic_gen_tlm_test.json",
        "configs/arbiter_tlm_test.json",
        "configs/test/nic_router_nic.json"
    };

    for (const auto& path : config_files) {
        INFO("Loading config: " << path);
        auto config = loadConfig(path);

        EventQueue eq;
        registerAllModules();
        ModuleFactory factory(&eq);

        REQUIRE(factory.instantiateAll(config));
        factory.startAllTicks();
        eq.run(10);

        REQUIRE(eq.getCurrentCycle() == 10);
    }

    SUCCEED("All TLM config files loaded and simulated successfully");
}
```

- [ ] **Step 5: Build and run**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][topology]"
./build/bin/cpptlm_tests "[e2e][config][batch]"
```

Expected: 4 tests PASS

- [ ] **Step 6: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e topology and batch config load tests (mesh_2x2, mesh_4x4, hierarchical, all TLM)"
```

---

### Task 7: Negative Test Cases (Validation Failures)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write unknown module type rejection test**

```cpp
TEST_CASE("E2E: 未注册模块类型应被拒绝", "[e2e][negative][validation]") {
    json config = R"({
        "modules": [
            {"name": "bad", "type": "NonExistentType"}
        ],
        "connections": []
    })"_json;

    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE_FALSE(factory.instantiateAll(config));
    SUCCEED("Unknown type correctly rejected");
}
```

- [ ] **Step 2: Write missing required params rejection test**

```cpp
TEST_CASE("E2E: RouterTLM缺少params应被拒绝", "[e2e][negative][validation][params]") {
    json config = R"({
        "modules": [
            {"name": "r0", "type": "RouterTLM"}
        ],
        "connections": []
    })"_json;

    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE_FALSE(factory.instantiateAll(config));
    SUCCEED("Missing params for RouterTLM correctly rejected");
}

TEST_CASE("E2E: NICTLM缺少params应被拒绝", "[e2e][negative][validation][params]") {
    json config = R"({
        "modules": [
            {"name": "ni0", "type": "NICTLM"}
        ],
        "connections": []
    })"_json;

    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE_FALSE(factory.instantiateAll(config));
    SUCCEED("Missing params for NICTLM correctly rejected");
}
```

- [ ] **Step 3: Write missing required fields rejection test**

```cpp
TEST_CASE("E2E: 配置文件缺少modules字段应被拒绝", "[e2e][negative][validation][schema]") {
    json config = R"({
        "name": "test",
        "connections": []
    })"_json;

    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE_FALSE(factory.instantiateAll(config));
    SUCCEED("Missing 'modules' field correctly rejected");
}

TEST_CASE("E2E: 配置文件缺少connections字段应被拒绝", "[e2e][negative][validation][schema]") {
    json config = R"({
        "modules": [
            {"name": "cpu", "type": "CPUSim"}
        ]
    })"_json;

    registerAllModules();
    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE_FALSE(factory.instantiateAll(config));
    SUCCEED("Missing 'connections' field correctly rejected");
}
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][negative]"
```

Expected: 5 tests PASS

- [ ] **Step 5: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e negative validation tests (unknown type, missing params, schema errors)"
```

---

### Task 8: Legacy Module Test (CPUSim)

**Files:**
- Append to: `test/test_e2e_simulation.cc`

- [ ] **Step 1: Write CPUSim legacy module test**

```cpp
TEST_CASE("E2E: CPUSim 遗留模块加载与仿真", "[e2e][legacy][cpu-sim]") {
    json config = R"({
        "modules": [
            {"name": "proc0", "type": "CPUSim"},
            {"name": "proc1", "type": "CPUSim"}
        ],
        "connections": [
            {"src": "proc0", "dst": "proc1", "latency": 1}
        ]
    })"_json;

    auto [eq, factory] = setupSimulation(config);

    REQUIRE(factory.getInstance("proc0") != nullptr);
    REQUIRE(factory.getInstance("proc1") != nullptr);
    REQUIRE(factory.getInstance("proc0")->get_module_type() == "CPUSim");

    // CPUSim 可产生 downstream 请求
    eq.run(50);
    REQUIRE(eq.getCurrentCycle() == 50);
    SUCCEED("CPUSim legacy module instantiated and ran");
}
```

- [ ] **Step 2: Build and run**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[e2e][legacy]"
```

Expected: 1 test PASS

- [ ] **Step 3: Commit**

```bash
git add test/test_e2e_simulation.cc
git commit -m "test: add e2e legacy module test (CPUSim)"
```

---

### Task 9: cpptlm_sim CLI End-to-End Test Script

**Files:**
- Create: `scripts/ci_e2e_test.sh`

- [ ] **Step 1: Create the CI test script**

```bash
#!/bin/bash
# ci_e2e_test.sh — cpptlm_sim CLI 端到端验证脚本
# 验证所有有效 TLM 配置文件可通过 cpptlm_sim 成功运行

set -euo pipefail

SIM_BIN="${CPPTLM_SIM:-./build/bin/cpptlm_sim}"
PROJECT_DIR="${CPPTLM_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$PROJECT_DIR"

echo "=========================================="
echo "cpptlm_sim CLI End-to-End Test Suite"
echo "Binary: $SIM_BIN"
echo "=========================================="

PASS=0
FAIL=0
FAILED_TESTS=()

run_sim_test() {
    local name="$1"
    local config="$2"
    local cycles="${3:-100}"

    echo -n "[TEST] $name ... "
    if timeout 30 "$SIM_BIN" "$config" --cycles "$cycles" > /dev/null 2>&1; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL"
        ((FAIL++))
        FAILED_TESTS+=("$name ($config)")
    fi
}

# --------------------------------------------------
# 单端口 TLM 配置
# --------------------------------------------------
run_sim_test "CacheTLM→MemoryTLM"          "configs/cache_chstream_test.json" 100
run_sim_test "CPUTLM→Cache→Memory"         "configs/cpu_tlm_test.json"        200

# --------------------------------------------------
# 多端口 TLM 配置
# --------------------------------------------------
run_sim_test "CrossbarTLM 4端口"           "configs/crossbar_test.json"       100
run_sim_test "ArbiterTLM2 双请求者"        "configs/arbiter_tlm_test.json"    100

# --------------------------------------------------
# 发起者 TLM 配置
# --------------------------------------------------
run_sim_test "TrafficGenTLM 双生成器"      "configs/traffic_gen_tlm_test.json" 200

# --------------------------------------------------
# NoC 拓扑配置
# --------------------------------------------------
run_sim_test "NIC→Router→NIC 最小拓扑"     "configs/test/nic_router_nic.json" 200
run_sim_test "mesh_2x2 拓扑"               "configs/mesh_2x2.json"            200
run_sim_test "mesh_4x4 大规模拓扑"         "configs/mesh_4x4.json"            100
run_sim_test "hierarchical_2x2 分层拓扑"   "configs/hierarchical_2x2.json"    100

# --------------------------------------------------
# 冒烟测试
# --------------------------------------------------
run_sim_test "帮助信息" "--help" 0

# 特殊处理：--help 不用 config 参数
smoke_help() {
    echo -n "[TEST] --help 输出验证 ... "
    if "$SIM_BIN" --help 2>&1 | grep -q "Usage:"; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL"
        ((FAIL++))
        FAILED_TESTS+=("--help output verification")
    fi
}
smoke_help

echo ""
echo "=========================================="
echo "Results: $PASS passed, $FAIL failed"
echo "=========================================="

if [ "$FAIL" -gt 0 ]; then
    echo "FAILED TESTS:"
    for t in "${FAILED_TESTS[@]}"; do
        echo "  - $t"
    done
    exit 1
fi

echo "All E2E tests passed!"
exit 0
```

- [ ] **Step 2: Make executable and run**

```bash
chmod +x scripts/ci_e2e_test.sh
./scripts/ci_e2e_test.sh
```

Expected: all tests PASS (0 failures)

- [ ] **Step 3: Commit**

```bash
git add scripts/ci_e2e_test.sh
git commit -m "test: add cpptlm_sim CLI end-to-end test script"
```

---

### Task 10: Final Integration Verification

**Files:**
- No new files

- [ ] **Step 1: Full test suite run**

```bash
cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[e2e]"
```

Expected: All 15 e2e Catch2 tests PASS

- [ ] **Step 2: CLI test run**

```bash
./scripts/ci_e2e_test.sh
```

Expected: All CLI tests PASS

- [ ] **Step 3: Full regression run (non-e2e tests still pass)**

```bash
./build/bin/cpptlm_tests ~"[e2e]" --verbosity quiet 2>&1 | tail -5
```

Expected: Same pass/fail count as before (0 new regressions). Known 12 pre-existing failures should remain unchanged.

- [ ] **Step 4: Commit final state**

```bash
git add test/test_e2e_simulation.cc scripts/ci_e2e_test.sh
git diff --cached --stat
git commit -m "test: complete e2e test suite (15 Catch2 + 11 CLI tests)"
```

---

## Test Coverage Matrix

| Module Type | Task | Tag Filter | Config Source | Verifies |
|---|---|---|---|---|
| CacheTLM | Task 2 | `[e2e][cache]` | Inline JSON | req→resp round-trip |
| MemoryTLM | Task 2 | `[e2e][memory]` | Inline JSON | resp generation |
| LinkTLM | Task 2 | `[e2e][link]` | Inline JSON | delay queue + NoC connection |
| CrossbarTLM | Task 3 | `[e2e][crossbar]` | Inline JSON | 4-port routing |
| ArbiterTLM2 | Task 3 | `[e2e][arbiter]` | Inline JSON | 2-port round-robin |
| ArbiterTLM4 | Task 3 | `[e2e][arbiter]` | Inline JSON | 4-port round-robin |
| CPUTLM | Task 4 | `[e2e][cpu]` | Inline JSON | autonomous request gen |
| TrafficGenTLM | Task 4 | `[e2e][traffic-gen]` | Inline JSON | SEQUENTIAL + RANDOM modes |
| RouterTLM | Task 5 | `[e2e][router]` | Inline JSON | XY routing, node_id, 5 ports |
| NICTLM | Task 5 | `[e2e][nic]` | Inline JSON | addr regions, packetize |
| CPUSim | Task 8 | `[e2e][legacy]` | Inline JSON | PortManager-based |
| mesh_2x2 | Task 6 | `[e2e][topology]` | External JSON | 12 modules, all params |
| mesh_4x4 | Task 6 | `[e2e][topology]` | External JSON | 48 modules, all params |
| hierarchical_2x2 | Task 6 | `[e2e][topology]` | External JSON | 7 modules, hierarchical |
| All TLM configs | Task 6 | `[e2e][config][batch]` | External JSON | 6 files batch load |
| Unknown type | Task 7 | `[e2e][negative]` | Inline JSON | rejection |
| Missing params | Task 7 | `[e2e][negative]` | Inline JSON | rejection |
| Schema errors | Task 7 | `[e2e][negative]` | Inline JSON | rejection |
| cpptlm_sim CLI | Task 9 | N/A (bash) | External JSON | 11 config files × 100-200 cycles |

## Convenience Test Filters

```bash
# 运行所有 e2e 测试
./build/bin/cpptlm_tests "[e2e]"

# 仅运行单端口模块测试
./build/bin/cpptlm_tests "[e2e][single-port]"

# 仅运行多端口模块测试
./build/bin/cpptlm_tests "[e2e][multi-port]"

# 仅运行 NoC 域测试
./build/bin/cpptlm_tests "[e2e][noc]"

# 仅运行外部配置文件加载测试
./build/bin/cpptlm_tests "[e2e][external-config]"

# 仅运行负向测试
./build/bin/cpptlm_tests "[e2e][negative]"

# 运行所有 e2e + CLI
./build/bin/cpptlm_tests "[e2e]" && ./scripts/ci_e2e_test.sh
```
