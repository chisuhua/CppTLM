// examples/demo_dgpu_soc_realip.cpp
// dGPU SoC RealIP Refactor Demo: 装载 dgpu_soc_v2_realip_p2.json + 实例化 4 IP
//
// 演示 Phase C 完成后的 4-IP topology 装配:
//   - DGpuSoc (顶层 SimModule container)
//   - PcieControllerIP (SimModule facade)
//   - GpuInternalFabric (SimModule, JSON 路径自动 attach inner CrossbarTLM)
//   - VramControllerTLM (ChStream, backing 注入)
//
// 验证每个 IP module 可经 soc.getInternalInstance() 拿到, 4 个 type 名称正确。
//
// 作者 CppTLM Team / 日期 2026-10-07
// 参考: openspec/changes/cpptlm-dgpu-realip-refactor T26 (Phase C)

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "tlm/cluster/gpu_internal_fabric.hh"
#include "tlm/gpu/dgpu_soc.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/vram_controller_tlm.hh"

#include <cstdio>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

using cpptlm::tlm::DGpuSoc;
using tlm::cluster::GpuInternalFabric;
using tlm::pcie::PcieControllerIP;
using json = nlohmann::json;

namespace {

    void print_header(const char* title) {
        printf("\n============================================================\n");
        printf("  %s\n", title);
        printf("============================================================\n\n");
    }

    bool require_ok(bool cond, const char* msg) {
        if (cond) {
            printf("  [OK]   %s\n", msg);
            return true;
        }
        printf("  [FAIL] %s\n", msg);
        return false;
    }

} // namespace

int main() {
    print_header("CppTLM dGPU SoC RealIP Refactor Demo: 4-IP Topology Assembly");

    // 触发 REGISTER_CHSTREAM (per chstream_register.hh) 让 VramControllerTLM 等
    // ChStream modules 在 ModuleFactory object_registry 注册, 否则
    // factory.instantiateAll 在 dgpu_soc_v2_realip_p2.json 找不到 VramControllerTLM。
    // (per src/abi/cpptlm_emulator.cc:31-40: libcpptlm_emulator.so 静态初始化触发,
    // 但 demo binary 没强制 load cpptlm_emulator.so, 显式 REGISTER 触发 static init)
    REGISTER_CHSTREAM;

    // ── Step 1: 加载 configs/dgpu_soc_v2_realip_p2.json (T23B 重建) ──
    printf("[Init] Loading configs/dgpu_soc_v2_realip_p2.json\n");
    std::ifstream f("configs/dgpu_soc_v2_realip_p2.json");
    if (!f.is_open()) {
        fprintf(stderr, "  [FAIL] cannot open configs/dgpu_soc_v2_realip_p2.json\n");
        return 1;
    }
    json board_cfg;
    f >> board_cfg;
    printf("  [OK]   JSON loaded: name=%s, version=%s, top-level modules=%zu\n",
           board_cfg.value("name", std::string{"?"}).c_str(),
           board_cfg.value("version", std::string{"?"}).c_str(), board_cfg["modules"].size());

    // 打印 soc 节点 modules 数组 (验证 JSON 加载正确)
    for (auto& m : board_cfg["modules"]) {
        const std::string m_name = m.value("name", std::string{});
        if (m_name == "soc") {
            printf("  [OK]   soc.modules has %zu children\n", m["modules"].size());
        }
    }

    // ── Step 2: 实例化 DGpuSoc + 4 IP children ──
    printf("\n[Init] Instantiate DGpuSoc nested topology\n");
    EventQueue eq;
    ModuleFactory factory(&eq);
    if (!factory.instantiateAll(board_cfg)) {
        fprintf(stderr, "  [FAIL] factory.instantiateAll failed\n");
        return 1;
    }

    auto* soc = factory.getInstance("soc");
    if (!require_ok(soc != nullptr, "DGpuSoc 'soc' instantiated"))
        return 1;
    if (!require_ok(soc->get_module_type() == "DGpuSoc", "soc module_type == DGpuSoc"))
        return 1;

    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    if (!require_ok(soc_sim != nullptr, "DGpuSoc cast to SimModule"))
        return 1;

    // ── Step 3: 验证 4 IP children getInternalInstance 可达 ──
    printf("\n[Init] Verify 4 IP children accessible via getInternalInstance\n");

    auto* pcie_ctrl = soc_sim->getInternalInstance("pcie_ctrl");
    if (!require_ok(pcie_ctrl != nullptr, "pcie_ctrl (PcieControllerIP) accessible"))
        return 1;
    if (!require_ok(pcie_ctrl->get_module_type() == "PcieControllerIP",
                    "pcie_ctrl module_type == PcieControllerIP"))
        return 1;

    auto* fabric = soc_sim->getInternalInstance("fabric");
    if (!require_ok(fabric != nullptr, "fabric (GpuInternalFabric) accessible"))
        return 1;
    if (!require_ok(fabric->get_module_type() == "GpuInternalFabric",
                    "fabric module_type == GpuInternalFabric"))
        return 1;

    auto* vram_ctrl = soc_sim->getInternalInstance("vram_ctrl");
    if (!require_ok(vram_ctrl != nullptr, "vram_ctrl (VramControllerTLM) accessible"))
        return 1;
    if (!require_ok(vram_ctrl->get_module_type() == "VramControllerTLM",
                    "vram_ctrl module_type == VramControllerTLM"))
        return 1;

    // ── Step 4: 验证 T25A.1 — GpuInternalFabric crossbar JSON 路径自动 attach ──
    auto* fabric_sim = dynamic_cast<GpuInternalFabric*>(fabric);
    if (!require_ok(fabric_sim != nullptr, "fabric cast to GpuInternalFabric"))
        return 1;
    if (!require_ok(fabric_sim->crossbar() != nullptr,
                    "fabric.crossbar() != nullptr (T25A.1 auto-attach)"))
        return 1;
    if (!require_ok(fabric_sim->crossbar()->get_module_type() == "CrossbarTLM",
                    "fabric.crossbar() module_type == CrossbarTLM"))
        return 1;

    // ── Step 5: 验证 T23C — VramControllerTLM backing 注入 (framebuffer_ptr_ 派生) ──
    auto* vram = dynamic_cast<VramControllerTLM*>(vram_ctrl);
    if (!require_ok(vram != nullptr, "vram_ctrl cast to VramControllerTLM"))
        return 1;
    // backing 注入由 DGpuBoard::init() 触发 bind_memory_backings, 此 demo 仅展示
    // 实例化后 binding null (init() 未调); production 路径 (DGpuBoard::init) 注入
    // framebuffer_ptr_ → vram_ctrl backing (per handoff §5 T23C)。
    if (!require_ok(!vram->has_backing(),
                    "vram_ctrl.has_backing() == false (pre-DGpuBoard::init, 演示期望)"))
        return 1;
    if (!require_ok(vram->host_addr(0) == nullptr,
                    "vram_ctrl.host_addr(0) == nullptr (ENODEV semantics)"))
        return 1;

    // ── Step 6: 验证 T25B — PcieControllerIP 5 BAR decode stimuli ──
    printf("\n[Init] Verify PcieControllerIP 5 BAR decode stimuli (T25B facade)\n");
    auto* ctrl = dynamic_cast<PcieControllerIP*>(pcie_ctrl);
    if (!require_ok(ctrl != nullptr, "pcie_ctrl cast to PcieControllerIP"))
        return 1;

    // 激活 B1 dormant gate: ctrl 必须 attach PcieEndpointIP 才能查 bar_size(1)
    // 触发 Bar1FbFastPath 分支 (per PcieControllerIP::match_bar line 38).
    tlm::pcie::PcieEndpointIP ep("demo_t26_ep", &eq);
    nlohmann::json ep_cfg;
    ep_cfg["bar_sizes"] =
        nlohmann::json::array({0ULL, 16777216ULL, 8589934592ULL, 0ULL, 0ULL, 0ULL});
    ep.set_config(ep_cfg);
    ctrl->attach_endpoint(&ep);
    if (!require_ok(ep.bar_size(1) == 16777216ULL, "EP bar_size(1) = 16MB"))
        return 1;

    using BT = PcieControllerIP::BarTarget;
    BT target;
    uint64_t sub_offset;
    if (!require_ok(ctrl->match_bar(0, 0x123ULL, target, sub_offset) && target == BT::Bar0Mmio,
                    "stim 1: bar=0 → Bar0Mmio"))
        return 1;
    if (!require_ok(ctrl->match_bar(1, 0x5000ULL, target, sub_offset) &&
                        target == BT::Bar1FbFastPath,
                    "stim 2: bar=1 offset<16MB → Bar1FbFastPath"))
        return 1;
    if (!require_ok(!ctrl->match_bar(1, 16777216ULL, target, sub_offset) && target == BT::Unknown,
                    "stim 3: bar=1 boundary → Unknown (B2d guard)"))
        return 1;
    if (!require_ok(ctrl->match_bar(2, 0x1000ULL, target, sub_offset) &&
                        target == BT::Bar2VramAperture,
                    "stim 4: bar=2 → Bar2VramAperture"))
        return 1;
    if (!require_ok(!ctrl->match_bar(3, 0ULL, target, sub_offset) && target == BT::Unknown,
                    "stim 5: bar=3 upper dword → Unknown"))
        return 1;

    print_header("Demo PASSED — 4-IP Topology Assembly Verified");
    printf("  All 4 IP modules instantiated from dgpu_soc_v2_realip_p2.json\n");
    printf("  GpuInternalFabric crossbar auto-attached via JSON child path (T25A.1)\n");
    printf("  PcieControllerIP facade 5 BAR decode stimuli all PASS (T25B)\n");
    printf("  VramControllerTLM backing null (DGpuBoard::init() production 路径注入, T23C)\n\n");
    return 0;
}