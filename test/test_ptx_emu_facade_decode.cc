// test/test_ptx_emu_facade_decode.cc
// =============================================================================
// PtxEmuSubmoduleMVP facade — PTXIR decode 单元测试 (S1 / T-s1-3 §6.a)
// 功能: 验证 facade::decode_ptxir 接受合法 PTXIR 字节流返回 vector<StatementContext>,
//       拒绝 magic 错误和版本错误的字节流。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-3, Sisyphus)
// 标签 [ptx-emu-facade][decode]
// =============================================================================

#include "catch_amalgamated.hpp"

// PTX-EMU 头 — 测试显式 include (facade header 仅前向声明)
// 测试不属于 cpptlm_core 库, 不破坏编译防火墙。
#include "ptx_ir/ptxir_format.h"     // PTXIR_MAGIC / PTXIR_VERSION / PtxirHeader
#include "ptx_ir/ptxir_reader.h"     // PtxirReader
#include "ptx_ir/ptxir_writer.h"     // PtxirWriter
#include "ptx_ir/statement_context.h"

#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

using tlm::PtxEmuSubmoduleMVP;
using tlm::GPUConfig;
using tlm::StatementContext;

namespace {

/// 构造一个最小可解析的 PTXIR 字节流 (1 个 ret 指令)。
/// 使用 PtxirWriter (PTX-EMU 自带工具), 与 PTX-EMU 内部格式字节级一致。
std::vector<uint8_t> make_valid_ptxir_bytes() {
    std::vector<StatementContext> stmts;
    {
        // PC=0: ret (S_RET, VOID_INSTR)
        StatementContext stmt;
        stmt.type = S_RET;
        stmt.data = VoidInstr{};
        stmt.instructionText = "ret;";
        stmts.push_back(stmt);
    }

    std::ostringstream oss(std::ios::binary);
    PtxirWriter writer(oss);
    writer.write(stmts);

    const std::string s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

/// 构造一个 magic 错误的字节流 (前 4 字节改为 "XXXX" 而非 "PTXI")
std::vector<uint8_t> make_invalid_magic_bytes() {
    auto bytes = make_valid_ptxir_bytes();
    if (bytes.size() >= 4) {
        bytes[0] = 'X';
        bytes[1] = 'X';
        bytes[2] = 'X';
        bytes[3] = 'X';
    }
    return bytes;
}

/// 构造一个版本错误的字节流 (PTXIR 版本字段改为 99 — 已知不支持)
std::vector<uint8_t> make_invalid_version_bytes() {
    auto bytes = make_valid_ptxir_bytes();
    // PtxirHeader 布局: magic[4] + version[2] (LE) + flags[2] + ...
    if (bytes.size() >= 6) {
        bytes[4] = 99;  // version lo
        bytes[5] = 0;   // version hi
    }
    return bytes;
}

}  // namespace

TEST_CASE("facade_decode_ptxir_valid",
          "[ptx-emu-facade][decode]") {
    PtxEmuSubmoduleMVP facade;
    const GPUConfig default_cfg{};
    REQUIRE(facade.init("", default_cfg));

    auto bytes = make_valid_ptxir_bytes();
    REQUIRE_FALSE(bytes.empty());

    auto stmts = facade.decode_ptxir(bytes);
    REQUIRE_FALSE(stmts.empty());

    // 至少有 1 个 statement (ret), type 应为 S_RET
    bool found_ret = false;
    for (const auto& s : stmts) {
        if (s.type == S_RET) {
            found_ret = true;
            break;
        }
    }
    CHECK(found_ret);

    facade.shutdown();
}

TEST_CASE("facade_decode_ptxir_invalid_magic_throws",
          "[ptx-emu-facade][decode]") {
    PtxEmuSubmoduleMVP facade;
    const GPUConfig default_cfg{};
    REQUIRE(facade.init("", default_cfg));

    auto bytes = make_invalid_magic_bytes();
    REQUIRE_FALSE(bytes.empty());

    // PTX-EMU PtxirReader::read_header 抛出 std::runtime_error on bad magic
    CHECK_THROWS_AS(facade.decode_ptxir(bytes), std::runtime_error);

    facade.shutdown();
}

TEST_CASE("facade_decode_ptxir_invalid_version_throws",
          "[ptx-emu-facade][decode]") {
    PtxEmuSubmoduleMVP facade;
    const GPUConfig default_cfg{};
    REQUIRE(facade.init("", default_cfg));

    auto bytes = make_invalid_version_bytes();
    REQUIRE_FALSE(bytes.empty());

    // PtxirReader::read_header 抛出 std::runtime_error on unsupported version
    CHECK_THROWS_AS(facade.decode_ptxir(bytes), std::runtime_error);

    facade.shutdown();
}

TEST_CASE("facade_decode_ptxir_empty_input_throws",
          "[ptx-emu-facade][decode]") {
    PtxEmuSubmoduleMVP facade;
    const GPUConfig default_cfg{};
    REQUIRE(facade.init("", default_cfg));

    std::vector<uint8_t> empty_bytes;
    // 短于 sizeof(PtxirHeader)=24 的输入应触发读取错误
    CHECK_THROWS_AS(facade.decode_ptxir(empty_bytes), std::runtime_error);

    facade.shutdown();
}
