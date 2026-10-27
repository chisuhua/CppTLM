// include/utils/port_contract_asserts.hh
// Compile-time port-contract static_asserts for bundle field widths.
// Per Oracle review C3 fix + Momus REJECT 修复:
//   - 不依赖 chstream::ChStreamInitiatorPort<Bundle> 模板(类型不存在, 实际是 cpptlm::ChStreamInitiatorPort 非模板)
//   - 不依赖 SFINAE on private members (C++17 access-check 不在 SFINAE 替换失败语境)
//   - 改为只检查 public bundle 字段宽度 (decltype(&BundleType::Member)::width)
// Private-member 删除验证 → CMake try_compile() 负向测试 (Task 9)
//
// ## Usage
//  In bundle header (e.g. dgpu_fabric_bundles_tlm.hh) or test:
//   BUNDLE_FIELD_WIDTH(bundles::Fabric2VramCtrlBundle, address, 64);
//   BUNDLE_FIELD_WIDTH(bundles::Fabric2VramCtrlBundle, chip_select, 8);
//
// Author: CppTLM Team / Date: 2026-10-08

#pragma once

#include <type_traits>
#include <cstddef>
#include "bundles/cpphdl_types.hh"  // bundles::ch_uint<W> with static width = W (added by A.2)

namespace utils {

// Field-width check via member-name lookup.
// If `bundle.field` doesn't exist, this is a hard error (NOT a SFINAE substitution failure):
// acceptable for widths — a missing field IS a bug.
// Note: `.*` on rvalue yields xvalue; decltype thus gives `T&&`. Static-member access
// via `T&&::width` works in C++17+, but we strip the reference explicitly for clarity
// and to avoid tool-chain-dependent acceptance.
//
// **枚举类型字段(2026-10-27 新增,per `cpptlm-cep-cohif-carrier` task 1.4 / CC-02 ③)**:
// 强类型枚举字段(如 `bundles::CohIfBundle::flow_id`)的宽度 = `sizeof(enum) * 8`
// —— 枚举**无成员**,取不到 `::width`。此前 `BUNDLE_FIELD_WIDTH` 对枚举字段是
// **硬编译错误** ⇒ 「宏覆盖全部字段」与「`flow_id` 强类型」二者不可兼得。
// 现按字段类型分派:枚举走 `sizeof * 8`,其余走 `::width`(**行为对既有
// `ch_uint<W>` 字段逐字不变** —— `ch_uint` 非枚举,仍走原路径)。
namespace detail {

    template <typename Field>
    inline constexpr unsigned bundle_field_width_bits() {
        if constexpr (std::is_enum_v<Field>)
            return static_cast<unsigned>(sizeof(Field) * 8);
        else
            return static_cast<unsigned>(Field::width);
    }

}  // namespace detail

template <typename Bundle, typename MemberPtr, std::size_t ExpectedWidth>
inline constexpr bool bundle_field_width_v =
    detail::bundle_field_width_bits<
        std::remove_reference_t<decltype(std::declval<Bundle>().*std::declval<MemberPtr>())>>() == ExpectedWidth;

// Convenience macro — caller preferred form
#define BUNDLE_FIELD_WIDTH(BundleType, Member, ExpectedWidth) \
    static_assert(::utils::bundle_field_width_v< \
                      BundleType, \
                      decltype(&BundleType::Member), \
                      ExpectedWidth>, \
                  "Bundle field width mismatch: " #BundleType "::" #Member)

// NOTE: PORT_IS_INITIATOR / PORT_IS_TARGET macro 已删除(原计划基于不存在的 chstream::ChStreamInitiatorPort<Bundle> 模板)。
//       GpuInternalFabric::vram_ctrl_out 与 VramControllerTLM::fabric_in 端口契约改用
//       set_vram_target setter-only 模式(per Momus REJECT 修复 + 原 Oracle AD-4 Phase 8 HB/RC 先例),
//       运行时 round-trip 测试在 Task 7 覆盖,放弃编译期端口类型断言。

}  // namespace utils