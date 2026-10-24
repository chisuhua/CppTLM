// test/test_realip_fabric_json_attach.cc
// T25A.1 pin: GpuInternalFabric JSON装配路径自动 attach inner CrossbarTLM.
//
// Per docs/superpowers/plans/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md
// §5 T25A.1 + §9.2 #8 AI-fail-point:
// `gpu_internal_fabric.hh:45-46` 注释说 PT-PR-C 会走 attach_composition 读
// JSON children, 但方法不存在。T22b 测试锁的是直接 C++ attach_crossbar,
// 走 JSON 路径 (ModuleFactory::instantiateAll) 时 fabric 仍保持 dormant
// crossbar()。T25A.1 必须实现 simulate_instantiate 覆写 或 attach_composition
// 方法, 让 JSON 路径也能自动 wire inner crossbar。
//
// 验收 (per handoff §5 T25A.1):
//   - 走 ModuleFactory JSON 路径 (dgpu_soc_v2_realip_p2.json) 装配测试绿
//   - GpuInternalFabric::simulate_instantiate 覆写后
//     fabric->crossbar() != nullptr (after instantiateAll + simulate_instantiate)
//
// TDD 5 步:
//   Step 1 RED:  写测试 (走 JSON 路径期望 crossbar() != nullptr), 跑 → fail
//   Step 2 GREEN: 改 JSON 加 inner CrossbarTLM child + 改 GpuInternalFabric 实现
//                 simulate_instantiate 覆写遍历 children 调 attach_crossbar
//   Step 3 REFACTOR: 加 idempotent 守卫 (T22b 直接 C++ attach 也应兼容)
//   Step 5 VERIFY 5 regression + [realip][p3] 全绿
//
// Tag: `[realip][p3]`

#include "chstream_register.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "tlm/cluster/gpu_internal_fabric.hh"

#include <fstream>
#include <nlohmann/json.hpp>

using tlm::cluster::GpuInternalFabric;
using json = nlohmann::json;

namespace {

    json load_realip_p2_config() {
        std::ifstream f("configs/dgpu_soc_v2_realip_p2.json");
        REQUIRE(f.is_open());
        json cfg;
        f >> cfg;
        return cfg;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// Step 1 (GREEN 路径, post-T25A.1):
// 走 ModuleFactory JSON 路径 (dgpu_soc_v2_realip_p2.json) instantiateAll +
// simulate_instantiate 后, GpuInternalFabric 内部 crossbar 应自动 wire 到
// internal CrossbarTLM child instance。
//
// 锁 MUST NOT: 仍可被 T22b 直接 attach_crossbar 覆盖 (idempotent / "if null
// only" 逻辑, 后续实现约束)。
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("P3 GpuInternalFabric: simulate_instantiate JSON 路径自动 attach inner crossbar",
          "[realip][p3]") {
    auto cfg = load_realip_p2_config();

    EventQueue eq;
    ModuleFactory factory(&eq);

    REQUIRE(factory.instantiateAll(cfg));

    auto* soc = factory.getInstance("soc");
    REQUIRE(soc != nullptr);
    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    REQUIRE(soc_sim != nullptr);

    auto* fabric = dynamic_cast<GpuInternalFabric*>(soc_sim->getInternalInstance("fabric"));
    REQUIRE(fabric != nullptr);

    // T25A.1 GREEN 验收: JSON 装配后 crossbar() 应为非 nullptr
    REQUIRE(fabric->crossbar() != nullptr);
    REQUIRE(fabric->crossbar()->get_module_type() == "CrossbarTLM");
}