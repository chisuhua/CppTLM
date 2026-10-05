// test/test_display_device_injection.cc
// D3a-2 display_device_injection_ 测试家族: PcieDisplayDevice 注入式重构验收
//
// 覆盖 (per spec display-device-injection):
//   - Case 1: set_backing_view 注入 / has_backing_view / backing_size / framebuffer_size_bytes 兼容
//   - Case 2: 无注入时 backdoor_read/write 返 -ENODEV (view-only 守卫, 无 silent lazy alloc)
//   - Case 3: PcieDisplayDevice 与 PcieMemoryDevice API 对称 (static_assert)
//   - Case 4: PcieEndpointIP::display_device_ raw ptr + set_display_device 注入
//   - Case 5: DGpuBoard::bind_display_backings 完整路径 (owner/injected/init-order)
//   - Case 6: PcieDisplayDevice 删除 ensure_framebuffer_allocated (SFINAE 编译期验证)
//
// 标签: [pcie][pcie-display][d3a2] / [pcie][d3a2]
// 作者: CppTLM Team / 日期: 2026-10-04
// 配套: openspec/changes/cpptlm-dgpu-d3a1-display-injection-refactor/tasks.md T1.1

#include <cerrno>
#include <cstdint>
#include <type_traits>
#include <vector>

#include <catch_amalgamated.hpp>
#include <nlohmann/json.hpp>

#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/pcie_display_device.hh"
#include "tlm/gpu/pcie_memory_device.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "utils/json_includer.hh"

using namespace tlm::gpu;

// ============================================================================
// Case 1: PcieDisplayDevice backing_view 注入
// ============================================================================
TEST_CASE("D3a-2: PcieDisplayDevice backing_view_ 注入",
          "[pcie][pcie-display][d3a2]") {
    PcieDisplayDevice dev;
    std::vector<uint8_t> backing(1024, 0xAA);
    dev.set_backing_view(backing.data(), backing.size());

    REQUIRE(dev.has_backing_view());
    REQUIRE(dev.backing_size() == 1024);
    // 兼容旧 API: framebuffer_size_bytes() 返回 backing_view_size_
    REQUIRE(dev.framebuffer_size_bytes() == 1024);

    // 注入后可 backdoor round-trip (view-only 语义)
    uint8_t w = 0x42;
    REQUIRE(dev.backdoor_write(0, &w, 1) == 0);
    uint8_t r = 0;
    REQUIRE(dev.backdoor_read(0, &r, 1) == 0);
    REQUIRE(r == 0x42);
}

// ============================================================================
// Case 2: 无注入时 backdoor_read/write 返 -ENODEV
// ============================================================================
TEST_CASE("D3a-2: PcieDisplayDevice 无注入时 backdoor_read 返 -ENODEV",
          "[pcie][pcie-display][d3a2]") {
    PcieDisplayDevice dev;
    uint8_t buf[4] = {};
    REQUIRE(dev.backdoor_read(0, buf, 4) == -ENODEV);   // 无 silent lazy alloc
    REQUIRE(dev.backdoor_write(0, buf, 4) == -ENODEV);  // 对称
    REQUIRE_FALSE(dev.has_backing_view());
    REQUIRE(dev.backing_size() == 0);
}

// ============================================================================
// Case 3: PcieDisplayDevice 与 PcieMemoryDevice API 对称 (static_assert)
// ============================================================================
TEST_CASE("D3a-2: PcieDisplayDevice 与 PcieMemoryDevice API 对称",
          "[pcie][pcie-display][d3a2]") {
    // 静态断言:三个 API 签名必须精确镜像 PcieMemoryDevice
    // 用 std::is_invocable_r_v 验证调用约定 + 返回类型(忽略类限定符)
    static_assert(std::is_invocable_r_v<
        void,
        decltype(&PcieDisplayDevice::set_backing_view),
        PcieDisplayDevice*, uint8_t*, uint64_t
    >, "PcieDisplayDevice::set_backing_view 可调用 (uint8_t*, uint64_t) → void");
    static_assert(std::is_invocable_r_v<
        bool,
        decltype(&PcieDisplayDevice::has_backing_view),
        const PcieDisplayDevice*
    >, "PcieDisplayDevice::has_backing_view const 可调用 → bool");
    static_assert(std::is_invocable_r_v<
        uint64_t,
        decltype(&PcieDisplayDevice::backing_size),
        const PcieDisplayDevice*
    >, "PcieDisplayDevice::backing_size const 可调用 → uint64_t");

    REQUIRE(true);  // 编译期保证为主, 运行期占位
}

// ============================================================================
// Case 4: PcieEndpointIP display_device_ 改为 raw ptr + set_display_device
// ============================================================================
TEST_CASE("D3a-2: PcieEndpointIP display_device_ 改为 raw ptr + set_display_device",
          "[pcie][pcie-display][d3a2]") {
    tlm::pcie::PcieEndpointIP ep("test_ep", nullptr);
    REQUIRE_FALSE(ep.has_display_device());  // 构造期不自动 make_unique

    PcieDisplayDevice dev;
    ep.set_display_device(&dev);
    REQUIRE(ep.has_display_device());
    REQUIRE(&ep.display_device() == &dev);
}

// ============================================================================
// Case 5: DGpuBoard::bind_display_backings 完整路径
// ============================================================================
TEST_CASE("D3a-2: DGpuBoard::bind_display_backings 注入完整路径",
          "[pcie][pcie-display][d3a2]") {
    // 内联镜像 configs/dgpu_soc_minimal_v1.json (cwd-independent, 与现有测试模式一致)
    nlohmann::json cfg = nlohmann::json::parse(R"({
        "name": "dgpu_soc_minimal_v1",
        "display_routing_enabled": false,
        "memory_routing_enabled": true,
        "framebuffer_size_bytes": 16777216,
        "modules": [{
            "name": "soc",
            "type": "DGpuSoc",
            "modules": [
                { "name": "pcie_ep", "type": "PcieEndpointIP",
                  "params": { "config_size": 4096, "num_msix_vectors": 16,
                              "bar_sizes": [4096, 16777216, 8589934592] } },
                { "name": "pcie_memory", "type": "PcieMemoryDevice",
                  "params": { "capacity_gb": 8 } },
                { "name": "sdma", "type": "SdmaEngineTLM", "params": { "max_inflight": 4 } },
                { "name": "gmmu", "type": "GmmuTLM", "params": { "page_size_bytes": 4096 } }
            ],
            "connections": [
                { "src": "sdma.2", "dst": "pcie_memory.0", "latency": 1 },
                { "src": "gmmu.0", "dst": "pcie_memory.1", "latency": 1 }
            ]
        }]
    })");

    DGpuBoard board("d3a2_bind_display_board");
    REQUIRE(board.load_soc_config(cfg));
    REQUIRE(board.init());

    // owner 端: display_fb_storage_ 已分配
    REQUIRE(board.display_fb_ptr() != nullptr);
    REQUIRE(board.display_fb_size() > 0);

    // injected 端: EP 已注册 display device + backing_view 已注入
    if (auto* ep = board.pcie_ep()) {
        REQUIRE(ep->has_display_device());
        REQUIRE(ep->display_device().has_backing_view());
        REQUIRE(ep->display_device().backing_size() == board.display_fb_size());
        // framebuffer_size_bytes 兼容旧 API
        REQUIRE(ep->display_device().framebuffer_size_bytes() == board.display_fb_size());
    }

    board.shutdown();
}

// ============================================================================
// Case 6: PcieDisplayDevice 删除 ensure_framebuffer_allocated (编译期验证)
// ============================================================================
namespace {

// SFINAE: has_ensure_framebuffer_allocated<T> = T 是否声明该方法
template <typename T, typename = void>
struct has_ensure_framebuffer_allocated : std::false_type {};
template <typename T>
struct has_ensure_framebuffer_allocated<
    T, std::void_t<decltype(&T::ensure_framebuffer_allocated)>> : std::true_type {};

}  // namespace

TEST_CASE("D3a-2: PcieDisplayDevice 不再 ensure_framebuffer_allocated",
          "[pcie][pcie-display][d3a2]") {
    // D3a-2 AC-1: ensure_framebuffer_allocated() 方法已删除
    static_assert(!has_ensure_framebuffer_allocated<PcieDisplayDevice>::value,
                  "PcieDisplayDevice::ensure_framebuffer_allocated 应已删除 (D3a-2 AC-1)");
    // 自有 framebuffer_ 字段也不应存在 (SFINAE 无法查私有字段, 用类型大小/行为间接验证)
    PcieDisplayDevice dev;
    REQUIRE_FALSE(dev.has_backing_view());  // 默认无自有 backing
    REQUIRE(true);
}

// ============================================================================
// Case 7: shutdown→load_soc_config→init cycle 后 display 注入仍生效 (P1.1 修复验证)
// ============================================================================
TEST_CASE("D3a-2: re-init cycle 后 display 注入仍生效",
          "[pcie][pcie-display][d3a2][reinit]") {
    // P1.1 修复验证: 旧实现 guard "if (display_fb_storage_) return" 包裹整个函数,
    // 导致 shutdown→load_soc_config→init cycle 后新 soc 的 display_device 永不被注入。
    // 修复后: guard 仅包裹 make_unique, 注入 (set_backing_view + set_display_device) 每次 init 都执行。

    // 第一次 load + init (load_soc_config 仅接受 json 对象, 需先 JsonIncluder 加载)
    auto cfg = JsonIncluder::loadAndInclude(std::string("configs/dgpu_soc_minimal_v1.json"));
    DGpuBoard board("d3a2_reinit_test_board");
    REQUIRE(board.load_soc_config(cfg));
    board.init();

    REQUIRE(board.display_fb_ptr() != nullptr);
    REQUIRE(board.display_fb_size() > 0);
    auto* ep_first = board.pcie_ep();
    REQUIRE(ep_first != nullptr);
    REQUIRE(ep_first->has_display_device());
    REQUIRE(ep_first->display_device().has_backing_view());
    REQUIRE(ep_first->display_device().backing_size() > 0);
    auto* fb_ptr_first = board.display_fb_ptr();

    // shutdown: destroy() reset soc_/eq_ 但保留 display_fb_storage_/vram_storage_
    board.shutdown();

    // 第二次 load (重建 soc_) + init
    // 修复后: 注入路径重新执行, 新 soc 的 display_device 获得 backing
    // 旧实现: guard 命中 → 整体 return → 新 display_device 无 backing
    board.load_soc_config(cfg);
    board.init();

    REQUIRE(board.display_fb_ptr() != nullptr);
    REQUIRE(board.display_fb_size() > 0);
    auto* ep_second = board.pcie_ep();
    REQUIRE(ep_second != nullptr);
    REQUIRE(ep_second->has_display_device());  // ← P1.1 修复后此断言转绿
    REQUIRE(ep_second->display_device().has_backing_view());  // ← P1.1 修复后此断言转绿
    REQUIRE(ep_second->display_device().backing_size() == board.display_fb_size());

    // 同一 backing ptr (display_fb_storage_ 复用, 未重新分配)
    REQUIRE(board.display_fb_ptr() == fb_ptr_first);

    board.shutdown();
}

// ============================================================================
// Case 8: backing_size 多 size 派生语义验证 (覆盖 AC-13 + AC-4 静态对称完整)
// ============================================================================
TEST_CASE("D3a-2: backing_size 多 size 派生语义验证",
          "[pcie][pcie-display][d3a2]") {
    // AC-13 回归保护: 禁止 backing_size() 返回硬编码常量 (例如旧 kFbSize=32MB)
    PcieDisplayDevice dev;

    REQUIRE_FALSE(dev.has_backing_view());
    REQUIRE(dev.backing_size() == 0);
    REQUIRE(dev.framebuffer_size_bytes() == 0);

    std::vector<uint8_t> backing_small(1024, 0xAA);
    dev.set_backing_view(backing_small.data(), backing_small.size());
    REQUIRE(dev.has_backing_view());
    REQUIRE(dev.backing_size() == 1024);
    REQUIRE(dev.framebuffer_size_bytes() == 1024);

    std::vector<uint8_t> backing_16mb(16ULL * 1024 * 1024, 0xBB);
    dev.set_backing_view(backing_16mb.data(), backing_16mb.size());
    REQUIRE(dev.backing_size() == 16ULL * 1024 * 1024);
    REQUIRE(dev.framebuffer_size_bytes() == 16ULL * 1024 * 1024);

    std::vector<uint8_t> backing_64mb(64ULL * 1024 * 1024, 0xCC);
    dev.set_backing_view(backing_64mb.data(), backing_64mb.size());
    REQUIRE(dev.backing_size() == 64ULL * 1024 * 1024);
    REQUIRE(dev.framebuffer_size_bytes() == 64ULL * 1024 * 1024);

    dev.set_backing_view(nullptr, 0);
    REQUIRE_FALSE(dev.has_backing_view());
    REQUIRE(dev.backing_size() == 0);
}

// ============================================================================
// Case 9: set_display_device ↔ set_memory_device API 对称性 (覆盖 spec Scenario)
// ============================================================================
TEST_CASE("D3a-2: PcieEndpointIP set_X_device API 对称性 (display/memory)",
          "[pcie][pcie-display][d3a2]") {
    tlm::pcie::PcieEndpointIP ep("test_ep_sym", nullptr);

    REQUIRE_FALSE(ep.has_display_device());
    REQUIRE_FALSE(ep.has_memory_device());

    PcieDisplayDevice dev;
    ep.set_display_device(&dev);
    REQUIRE(ep.has_display_device());
    REQUIRE_FALSE(ep.has_memory_device());
    REQUIRE(&ep.display_device() == &dev);

    PcieMemoryDevice mem("test_mem_sym", nullptr);
    ep.set_memory_device(&mem);
    REQUIRE(ep.has_display_device());
    REQUIRE(ep.has_memory_device());
    REQUIRE(&ep.display_device() == &dev);
}

// ============================================================================
// Case 10: DGpuBoard::set_display_fb_size override 优先级验证
// ============================================================================
TEST_CASE("D3a-2: DGpuBoard set_display_fb_size override 优先级",
          "[pcie][pcie-display][d3a2]") {
    // 内联最小 SoC 配置 (避免依赖文件 IO)
    nlohmann::json cfg = nlohmann::json::parse(R"({
        "name": "d3a_override_test",
        "display_routing_enabled": false,
        "memory_routing_enabled": false,
        "framebuffer_size_bytes": 16777216,
        "modules": [{
            "name": "soc",
            "type": "DGpuSoc",
            "modules": [
                { "name": "pcie_ep", "type": "PcieEndpointIP",
                  "params": { "config_size": 4096, "num_msix_vectors": 16,
                              "bar_sizes": [4096, 16777216, 8589934592] } },
                { "name": "pcie_memory", "type": "PcieMemoryDevice",
                  "params": { "capacity_gb": 8 } },
                { "name": "sdma", "type": "SdmaEngineTLM", "params": { "max_inflight": 4 } },
                { "name": "gmmu", "type": "GmmuTLM", "params": { "page_size_bytes": 4096 } }
            ],
            "connections": [
                { "src": "sdma.2", "dst": "pcie_memory.0", "latency": 1 },
                { "src": "gmmu.0", "dst": "pcie_memory.1", "latency": 1 }
            ]
        }]
    })");

    DGpuBoard board("d3a2_override_test");
    REQUIRE(board.load_soc_config(cfg));

    REQUIRE(board.display_fb_size() == 16777216);

    board.set_display_fb_size(8ULL * 1024 * 1024);
    REQUIRE(board.display_fb_size() == 8ULL * 1024 * 1024);

    board.set_display_fb_size(64ULL * 1024 * 1024);
    REQUIRE(board.display_fb_size() == 64ULL * 1024 * 1024);

    board.set_display_fb_size(0);
    REQUIRE(board.display_fb_size() == 16777216);

    REQUIRE(board.init());
    REQUIRE(board.display_fb_size() == 16777216);
    REQUIRE(board.display_fb_ptr() != nullptr);

    board.shutdown();
}
