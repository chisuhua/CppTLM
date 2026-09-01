// include/tlm/pcie/pcie_endpoint_ip.hh
// PcieEndpointIP ABI 版本锁定头（v1 → v2 边界声明）
// 功能描述：Phase 1 链层 + FC Token Bucket 落地后，冻结 PcieEndpointIP 的编译期
//           ABI 版本意图。**仅声明意图，非实际编译期/二进制保护**（per design §9.1）。
//           - CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2 = 链路层 + PHY 数字 + SR-IOV 能力
//           - v1 消费者（仅事务层）可继续 include，不获新功能，不被 #error 阻断
// 作者 CppTLM Team / 日期 2026-09-08
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §9.1
//       2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/proposal.md (P1-G6)
// ⚠️ 不修改 include/abi/cpptlm_emulator.h（23 ABI 冻结边界，AD-088）
#ifndef CPPTLM_PCIE_ENDPOINT_ABI_VERSION
#define CPPTLM_PCIE_ENDPOINT_ABI_VERSION 2  // v2 = 链路层 + PHY 数字 + SR-IOV
#endif

// ⚠️ 修订（Oracle Top-1）：删除原 #error 硬阻断（避免破坏 v1 消费者编译）
// 旧 v1 = 仅事务层（2026-08-26 archive）；v1 消费者可继续 include，仅不获新功能
// #if CPPTLM_PCIE_ENDPOINT_ABI_VERSION != 2
// #error "ABI version mismatch"
// #endif