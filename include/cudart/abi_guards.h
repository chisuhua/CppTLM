// include/cudart/abi_guards.h
// G-D4 ABI 静态断言集中托管 — HSK-6 §2.2 P0-1 硬门禁
//
// 集中托管 17 条 static_assert（Phase 2 物理删除 cpptlm_bridge.h + ptx_emu_driver.hh 前置）：
//   - 16 条 from cpptlm_bridge.h:243-306（PipelineId 6 + TcPrecision 6 + is_same_v 4）
//   - 1 条  from ptx_emu_driver.hh:27（sizeof(PtxEmuDriverApi) == 64 布局锁）
//
// HSK-6 关联：
//   - PTX-EMU HSK-6 §2.2 P0-1 硬门禁（commit 25e36f60）
//   - CppTLM HSK-6 ack 响应（commit 369cf71）
//   - 跨仓 commit 顺序 per ADR-035 §R5.1: PTX-EMU 1 → CppTLM 2 → UsrLinuxEmu 3 → CppTLM Phase 2 4 → TaskRunner 5
//
// 命名空间：保持全局 `abi_guards_g_d4`（与 cpptlm_bridge.h 原始定义一致，迁移动作最小化）。
// 头文件循环：abi_guards.h ↔ cpptlm_bridge.h 互 include，由各自 #ifndef guard 短路，
//             符合 C++ 头文件循环 include 的标准做法。
//
// Author: CppTLM Team (Sisyphus) / Date: 2026-08-18
// Refs:
//   - PTX-EMU HSK-6: https://github.com/chisuhua/PTX-EMU/blob/25e36f60/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md
//   - CppTLM HSK-6 ack response: docs/superpowers/specs/2026-08-18-hsk-6-response.md
//   - UsrLinuxEmu ADR-090 v2: https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md
#ifndef CPPTLM_CUDART_ABI_GUARDS_H
#define CPPTLM_CUDART_ABI_GUARDS_H

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "cudart/cpptlm_bridge.h"          // PtxEmuDriverApi (用于 sizeof 布局锁)
#include "cudart/pipeline_interface.h"      // PipelineId
#include "cudart/scoreboard_interface.h"    // IScoreboard
#include "cudart/tensor_core_interface.h"   // TcPrecision + ITensorCoreTiming

namespace abi_guards_g_d4 {

// === 16 条 from cpptlm_bridge.h:243-306 (G-D4 12 端点 + 4 签名级) ===

// PipelineId (6 端点) — HSK-4 byte-confirmed enum 值
static_assert(static_cast<uint32_t>(PipelineId::P0_INT_FP32) == 0,
              "G-D4 ABI drift: PipelineId::P0_INT_FP32 != 0 (expected 0)");
static_assert(static_cast<uint32_t>(PipelineId::V_SIMD) == 1,
              "G-D4 ABI drift: PipelineId::V_SIMD != 1 (expected 1)");
static_assert(static_cast<uint32_t>(PipelineId::P1_FP64) == 2,
              "G-D4 ABI drift: PipelineId::P1_FP64 != 2 (expected 2)");
static_assert(static_cast<uint32_t>(PipelineId::P2_SFU) == 3,
              "G-D4 ABI drift: PipelineId::P2_SFU != 3 (expected 3)");
static_assert(static_cast<uint32_t>(PipelineId::P3_LSU) == 4,
              "G-D4 ABI drift: PipelineId::P3_LSU != 4 (expected 4)");
static_assert(static_cast<uint32_t>(PipelineId::P4_TC) == 5,
              "G-D4 ABI drift: PipelineId::P4_TC != 5 (expected 5)");

// TcPrecision (6 端点) — HSK-4 byte-confirmed enum 值
static_assert(static_cast<uint32_t>(TcPrecision::FP4) == 0,
              "G-D4 ABI drift: TcPrecision::FP4 != 0 (expected 0)");
static_assert(static_cast<uint32_t>(TcPrecision::FP6) == 1,
              "G-D4 ABI drift: TcPrecision::FP6 != 1 (expected 1)");
static_assert(static_cast<uint32_t>(TcPrecision::FP8) == 2,
              "G-D4 ABI drift: TcPrecision::FP8 != 2 (expected 2)");
static_assert(static_cast<uint32_t>(TcPrecision::FP16) == 3,
              "G-D4 ABI drift: TcPrecision::FP16 != 3 (expected 3)");
static_assert(static_cast<uint32_t>(TcPrecision::BF16) == 4,
              "G-D4 ABI drift: TcPrecision::BF16 != 4 (expected 4)");
static_assert(static_cast<uint32_t>(TcPrecision::TF32) == 5,
              "G-D4 ABI drift: TcPrecision::TF32 != 5 (expected 5)");

// 签名级 ABI 验证 — 防 silent enum 替换 / 函数签名漂移

// IScoreboard::allocate(uint32_t, uint32_t) -> bool
static_assert(std::is_same_v<
              decltype(std::declval<IScoreboard&>().allocate(
                  uint32_t{}, uint32_t{})),
              bool>,
              "G-D4 ABI drift: IScoreboard::allocate signature drifted "
              "(expected bool(uint32_t, uint32_t))");

// IPipelineLatencyProvider::get_fractional_cycles_by_type(int, PipelineId) -> double
static_assert(std::is_same_v<
              decltype(std::declval<IPipelineLatencyProvider&>()
                           .get_fractional_cycles_by_type(
                               int{}, PipelineId::P0_INT_FP32)),
              double>,
              "G-D4 ABI drift: IPipelineLatencyProvider::"
              "get_fractional_cycles_by_type signature drifted "
              "(expected double(int, PipelineId) — int is the "
              "HKS-4 byte-confirmed type, NOT StatementType)");

// ITensorCoreTiming::get_latency(TcPrecision) -> uint32_t
static_assert(std::is_same_v<
              decltype(std::declval<ITensorCoreTiming&>().get_latency(
                  TcPrecision::FP16)),
              uint32_t>,
              "G-D4 ABI drift: ITensorCoreTiming::get_latency signature drifted "
              "(expected uint32_t(TcPrecision))");

// ITensorCoreTiming::get_latency_mnk(TcPrecision, M, N, K) -> uint32_t (含 default impl)
static_assert(std::is_same_v<
              decltype(std::declval<ITensorCoreTiming&>().get_latency_mnk(
                  TcPrecision::FP16, uint32_t{}, uint32_t{}, uint32_t{})),
              uint32_t>,
              "G-D4 ABI drift: ITensorCoreTiming::get_latency_mnk signature drifted "
              "(expected uint32_t(TcPrecision, uint32_t, uint32_t, uint32_t))");

// === 1 条 from ptx_emu_driver.hh:27 ===
// PtxEmuDriverApi 布局锁 — 与 PTX-EMU 真相源 (ccd34155:include/cudart/cpptlm_bridge.h) 同步
// 8 个函数指针 * 8 字节 = 64 字节 (LP64)
static_assert(sizeof(PtxEmuDriverApi) == 64,
              "PtxEmuDriverApi size mismatch — check vendor sync with PTX-EMU cpptlm_bridge.h");

}  // namespace abi_guards_g_d4

#endif  // CPPTLM_CUDART_ABI_GUARDS_H
