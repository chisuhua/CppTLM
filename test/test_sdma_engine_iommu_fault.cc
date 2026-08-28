// test_sdma_engine_iommu_fault.cc
// SdmaEngineTLM: IOMMU translation fault + 错误路径测试 (SD-G4)
// Author: CppTLM Team
// Date: 2026-08-26
//
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/design.md §6,§7
//       spec.md Scenarios "IOMMU translation fault"
//                          "Invalid descriptor rejected"
//
// 测试策略（per tasks.md T-sd-3 注释）：
//   使用 fake board stub harness — translate callback 返回非 0 模拟 IOMMU fault。

#include "bundles/dma_bundles_tlm.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/sim_object.hh"
#include "tlm/gpu/dma_descriptor_mvp.hh"
#include "tlm/gpu/sdma_engine_tlm.hh"

#include <cstdio>
#include <string>

using namespace tlm::gpu;
using namespace bundles;

TEST_CASE("SdmaEngine IOMMU fault: translate_cb returns non-zero → CompleterAbort + error_cb",
          "[sdma][error][iommu]") {
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // 注入 fake translate callback：返回非 0 模拟 IOMMU 翻译失败
    int translate_call_count = 0;
    sdma.set_translate_cb(
        [&translate_call_count](uint64_t /*iova*/, uint32_t /*size*/, uint64_t& /*phys*/) -> int {
            translate_call_count++;
            return -EFAULT; // 模拟 IOMMU translation fault
        });

    // 注入 error callback（捕获上报）
    int received_err_code = 0;
    std::string received_err_msg;
    sdma.set_error_cb([&received_err_code, &received_err_msg](int err, const std::string& msg) {
        received_err_code = err;
        received_err_msg = msg;
    });

    // 构造 desc_in（H2D: host_iova=100 → vram_offset=0x1000, size=4, tag=1）
    DmaDescriptor desc(DmaDescriptor::Dir::H2D,
                       /*host_iova=*/100,
                       /*vram_offset=*/0x1000,
                       /*size=*/4,
                       /*tag=*/1);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == false);

    // tick() 处理：translate_cb 失败 → 模拟 PCIe RequesterCompleterAbort
    sdma.tick();

    // per spec.md Scenario "IOMMU translation fault":
    //   no VRAM write MUST occur (mem_out 不发出)
    //   error completion MUST be emitted on done_out
    //   board error channel MUST be signaled
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].valid() == false); // 无 VRAM 写
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].valid() ==
            false); // 无 host upstream（即使失败也无事务）
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true); // 错误完成发出

    // 验证 done_out: kind=DMA_DONE, status=-EIO
    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_error() == true);
    REQUIRE(static_cast<int32_t>(done_cb.status.read()) == -EIO);
    REQUIRE(done_cb.tag.read() == 1u);

    // 验证 error_cb 被调用
    REQUIRE(translate_call_count == 1);
    REQUIRE(received_err_code == -EIO);
    REQUIRE(received_err_msg.find("translation fault") != std::string::npos);

    // 统计：error_count++
    REQUIRE(sdma.completed_count() == 0u);
    REQUIRE(sdma.error_count() == 1u);
}

TEST_CASE("SdmaEngine Invalid: size == 0 → rejected with -EINVAL", "[sdma][error][invalid]") {
    // per spec.md Scenario "Invalid descriptor rejected" (size == 0)
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();

    // 注意：translate_cb 不应被调用（size==0 在 cb 之前就拒绝）
    int translate_call_count = 0;
    sdma.set_translate_cb(
        [&translate_call_count](uint64_t /*iova*/, uint32_t /*size*/, uint64_t& /*phys*/) -> int {
            translate_call_count++;
            return 0;
        });

    // desc with size=0
    DmaDescriptor desc(DmaDescriptor::Dir::H2D,
                       /*host_iova=*/100,
                       /*vram_offset=*/0x1000,
                       /*size=*/0, // 非法
                       /*tag=*/1);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    sdma.tick();

    // 拒绝：无 host/mem 事务，done_out 错误完成
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true);
    REQUIRE(translate_call_count == 0); // cb 未触发

    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_error() == true);
    REQUIRE(static_cast<int32_t>(done_cb.status.read()) == -EINVAL);
}

TEST_CASE("SdmaEngine Invalid: vram_offset out-of-range → rejected with -EINVAL",
          "[sdma][error][oob]") {
    // per spec.md Scenario "Invalid descriptor rejected" (out-of-range vram_offset)
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    sdma.set_vram_size_bytes(0x100000); // 1 MB VRAM
    sdma.set_translate_cb([](uint64_t, uint32_t, uint64_t&) -> int { return 0; });

    // desc with vram_offset + size 越界
    DmaDescriptor desc(DmaDescriptor::Dir::H2D,
                       /*host_iova=*/100,
                       /*vram_offset=*/0x100000, // 刚好越界
                       /*size=*/4,
                       /*tag=*/2);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    sdma.tick();

    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true);

    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_error() == true);
    REQUIRE(static_cast<int32_t>(done_cb.status.read()) == -EINVAL);
}

TEST_CASE("SdmaEngine Missing translate_cb: error path still works", "[sdma][error][no-cb]") {
    // 不注入 translate_cb（未注册）→ 应触发 -EIO 错误路径
    EventQueue eq;
    SdmaEngineTLM sdma("sdma", &eq);
    sdma.init();
    // 注意：未调用 sdma.set_translate_cb()

    int received_err_code = 0;
    sdma.set_error_cb(
        [&received_err_code](int err, const std::string&) { received_err_code = err; });

    DmaDescriptor desc(DmaDescriptor::Dir::H2D,
                       /*host_iova=*/100,
                       /*vram_offset=*/0x1000,
                       /*size=*/4,
                       /*tag=*/3);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].data() = SdmaEngineTLM::to_pcie_tlp_descriptor(desc);
    sdma.req_in[SdmaEngineTLM::PORT_DESC_IN].set_valid(true);

    sdma.tick();

    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_HOST_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_MEM_OUT].valid() == false);
    REQUIRE(sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].valid() == true);

    const auto& done_tlp = sdma.resp_out[SdmaEngineTLM::PORT_DONE_OUT].data();
    CompletionBundle done_cb = SdmaEngineTLM::from_pcie_tlp_completion(done_tlp);
    REQUIRE(done_cb.is_error() == true);
    REQUIRE(static_cast<int32_t>(done_cb.status.read()) == -EIO);
    REQUIRE(received_err_code == -EIO);
}