// test/test_sm_streaming_multiprocessor_port.cc
// Test: SM 顶层 4 端口访问器 (GPUTLM 范式, per Task 1.1 v2 P0-1 修订)
//
// 验证 StreamingMultiprocessorTLM 暴露 4 个 GPUTLM 范式端口访问器:
//   - req_out() (OutputStreamAdapter<ComputeReqBundle>, master 方向)
//   - resp_in() (InputStreamAdapter<ComputeRespBundle>, slave 方向)
//   - req_in() (InputStreamAdapter<ComputeReqBundle>, 真实成员)
//   - resp_out() (OutputStreamAdapter<ComputeRespBundle>, 真实成员)
//
// StreamAdapter::tick() 契约要求 ModuleT 提供 req_in() + resp_out() 访问器
// (per include/framework/stream_adapter.hh:186 注释)
//
// 作者 CppTLM Team / 2027-02-09 (Task 18 实施)
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "framework/chstream_adapter_factory.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;

TEST_CASE("StreamingMultiprocessorTLM has 4 port accessors (GPUTLM pattern)",
          "[sm-port][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    // 4 访问器存在性 (per StreamAdapter 契约, framework/stream_adapter.hh:186)
    REQUIRE_NOTHROW(sm.req_out());  // OutputStreamAdapter<ComputeReqBundle>
    REQUIRE_NOTHROW(sm.resp_in());  // InputStreamAdapter<ComputeRespBundle>
    REQUIRE_NOTHROW(sm.req_in());   // InputStreamAdapter<ComputeReqBundle> (static dummy)
    REQUIRE_NOTHROW(sm.resp_out()); // OutputStreamAdapter<ComputeRespBundle> (static dummy)

    // P1 修复 (Oracle §6 引用恒等性): req_out/resp_in 是真实成员,
    // 每次调用应返回同一引用 (避免 dummy 互换错误)
    REQUIRE(&sm.req_out() == &sm.req_out());
    REQUIRE(&sm.resp_in() == &sm.resp_in());
}

TEST_CASE("StreamingMultiprocessorTLM get/set_scalar_reg (Task 1.3 依赖)",
          "[sm-port][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    // Oracle P1-7 Task 1.3 依赖: get/set_scalar_reg 提前声明
    REQUIRE_NOTHROW(sm.set_scalar_reg(0, 0xCAFE));
    REQUIRE(sm.get_scalar_reg(0) == 0xCAFE);
    REQUIRE(sm.get_scalar_reg(999) == 0); // 未设置返回 0
    sm.set_scalar_reg(0, 0xDEADBEEF);
    REQUIRE(sm.get_scalar_reg(0) == 0xDEADBEEF);
}

// Task 1.2 TDD 失败测试 (Oracle 预审 P1 修复):
// 解开 SM StreamAdapter 注册前, ChStreamAdapterFactory 必须知道 "StreamingMultiprocessorTLM".
// 当前 chstream_register.hh line 112-116 仍被 Task 17 closeout 注释屏蔽 (per Task 18 P1-1),
// 故此 REQUIRE 必然 FAIL. Task 1.2 Step 3 注册后 PASS.
TEST_CASE("ChStreamAdapterFactory knows StreamingMultiprocessorTLM (Task 1.2 pre-req)",
          "[sm-port][sm-microarch][chstream]") {
    // Task 1.2: 删除 chstream_register.hh line 112-116 Task 17 closeout 注释,
    // 在 line 110 GPUTLM 注册后追加 StreamingMultiprocessorTLM registerAdapter.
    REQUIRE(ChStreamAdapterFactory::get().knows("StreamingMultiprocessorTLM"));
    // sanity: GPUTLM 早已注册 (per Task 17 之前), 同 Bundle 类型对照
    REQUIRE(ChStreamAdapterFactory::get().knows("GPUTLM"));
}
