// test_sdma_engine_d2h.cc
// SdmaEngineTLM: D2H 全流程测试 (SD-G3)
// Author: CppTLM Team
// Date: 2026-08-26
//
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/design.md §4,§7
//       spec.md Scenario "D2H transfer completes end to end"
//
// 测试策略（per tasks.md T-sd-3 注释）：
//   使用 fake board stub harness（直接构造 fake DmaTranslateCb 注入 +
//   直接内存地址读写模拟 IOMMU 翻译 + 模拟 VRAM 内存指针）。
//   **不依赖** cpptlm-dgpu-abi-export 的 backdoor ABI。

#include "bundles/dma_bundles_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/sim_object.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <cstdio>
#include <vector>

using namespace tlm::gpu;
using namespace bundles;

// fake host memory stub（per tasks.md T-sd-3 "fake board stub harness"）
static std::vector<uint8_t> g_host_mem_d2h(4096, 0);

static int fake_translate_cb_d2h(uint64_t iova, uint32_t size, uint64_t& phys) {
    if (iova + size > g_host_mem_d2h.size()) {
        return -EFAULT;
    }
    phys = iova;
    return 0;
}

TEST_CASE("SdmaEngine D2H: desc → mem_out MEM_READ + host_out MEM_WRITE → done_out",
          "[sdma][d2h]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_cb_d2h);

    // 构造 desc_in 输入（D2H: vram_offset=0x2000 → host_iova=300, size=8, tag=42）
    DmaDescriptor desc(DmaDescriptor::Dir::D2H,
                       /*host_iova=*/300,
                       /*vram_offset=*/0x2000,
                       /*size=*/8,
                       /*tag=*/42);
    PcieTlpBundle desc_pkt = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = desc_pkt;
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == false);

    // tick() 处理 D2H 描述符
    sdma.tick();

    // 验证：mem_out + host_out + done_out 都发出
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].valid() == true);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].valid() == true);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true);

    // 验证 mem_out: MEM_READ TLP 指向 VRAM (vram_offset=0x2000)
    const auto& mem_tlp = sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].data();
    REQUIRE(mem_tlp.kind.read() == PcieTlpBundle::MEM_READ);
    REQUIRE(mem_tlp.bar_index.read() == 1u);
    REQUIRE(mem_tlp.offset.read() == 0x2000u);
    REQUIRE(mem_tlp.size.read() == 8u);

    // 验证 host_out: MEM_WRITE TLP 指向 host_iova
    const auto& host_tlp = sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].data();
    REQUIRE(host_tlp.kind.read() == PcieTlpBundle::MEM_WRITE);
    REQUIRE(host_tlp.offset.read() == 300u);
    REQUIRE(host_tlp.size.read() == 8u);
    REQUIRE(host_tlp.trans_id.read() == 42u);

    // 验证 done_out: kind=DMA_DONE, status=0, tag=42
    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    REQUIRE(done_tlp.kind.read() == SdmaEngineTLM::KIND_DMA_DONE);
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_ok() == true);
    REQUIRE(done_cb.tag.read() == 42u);

    REQUIRE(sdma.completed_count() == 1u);
    REQUIRE(sdma.error_count() == 0u);
}

TEST_CASE("SdmaEngine D2H: completion implies upstream host write visibility",
          "[sdma][d2h][visibility]") {
    // per spec.md Scenario "D2H completion implies host write visibility"
    // MVP: 验证 done_out status=0 + tag alignment
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_cb_d2h);

    DmaDescriptor desc(DmaDescriptor::Dir::D2H,
                       /*host_iova=*/1000,
                       /*vram_offset=*/0x4000,
                       /*size=*/16,
                       /*tag=*/99);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);
    sdma.tick();

    // done_out 必须是 status=0 + tag=99
    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_ok() == true);
    REQUIRE(done_cb.tag.read() == 99u);

    // host_out 写事务 size 与请求一致
    const auto& host_tlp = sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].data();
    REQUIRE(host_tlp.size.read() == 16u);
}

TEST_CASE("SdmaEngine D2H: done_out status=0 implies host memory data written (R3-1)",
          "[sdma][d2h][visibility][data-path]") {
    // 强化 R3 "D2H host write visibility" 验证 — 真实数据搬运（per spec.md
    // Requirement sdma-completion-ordering Scenario "D2H 完成 implies upstream host writes"）
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_translate_cb(fake_translate_cb_d2h); // identity mapping

    // 注入 fake host memory + fake VRAM memory（per spec.md R3 backdoor 路径）
    std::vector<uint8_t> host_mem(4096, 0);
    std::vector<uint8_t> vram_mem(0x100000, 0);
    sdma.set_host_backdoor(host_mem.data(), host_mem.size());
    sdma.set_vram_backdoor(vram_mem.data(), vram_mem.size());

    // 预置 VRAM 数据：在 vram_offset=0x4000 处写入 16 字节 payload
    static constexpr uint8_t kPayload[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
                                             0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    std::memcpy(&vram_mem[0x4000], kPayload, sizeof(kPayload));

    // D2H desc: vram_offset=0x4000 → host_iova=1000, size=16, tag=99
    DmaDescriptor desc(DmaDescriptor::Dir::D2H,
                       /*host_iova=*/1000,
                       /*vram_offset=*/0x4000,
                       /*size=*/16,
                       /*tag=*/99);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    REQUIRE(sdma.has_host_backdoor() == true);
    REQUIRE(sdma.has_vram_backdoor() == true);

    sdma.tick();

    // done_out status=0 + tag=99
    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_ok() == true);
    REQUIRE(done_cb.tag.read() == 99u);

    // 关键断言：host_mem[host_iova..host_iova+size] 必须包含 VRAM 数据
    // (per spec.md R3 "D2H 完成 implies host writes visible at host side")
    REQUIRE(std::memcmp(&host_mem[1000], kPayload, sizeof(kPayload)) == 0);
}