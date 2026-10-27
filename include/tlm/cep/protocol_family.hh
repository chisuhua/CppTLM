// include/tlm/cep/protocol_family.hh
// 协议**来源族**(ProtocolFamily)与其**数字编号**的唯一真源表
//
// 功能:为 `cpptlm-cep-cohif-carrier` Phase 3 的 per-bridge 元数据判据
//       (CC-05 ④「`protocol_id_` 与该协议实例声明的 `family` 一致」)提供
//       **可机器检查**的编号映射。
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 为什么需要这张表 ──────────────────────────────────────────────────
// `ADR-CEP-12` 的 13 属性里 `family` 是**来源族**(`industrial` / `ruby-slicc` /
// `academic`,**仅 3 值**),而 `06 号 §4.5` 的 per-bridge 静态元数据
// `protocol_id_` 是 `uint8_t` 的**协议标识**。
//
// ⇒ 两者若按「相等」对齐,则同为 `industrial` 的多个协议实例
//   (`moesi_gpu` / `mesif` / …)必然共享同一个 `protocol_id_` ⇒ 「一致」退化为
//   **恒真**,判据失去判别力。
//
// ⇒ **裁决(2026-10-28,用户 DRI)**:「一致」定义为 **编号对齐** ——
//   `protocol_id_` = **来源族的数字编号**;实例级唯一性由 `bridge_id_` 负责。
//   ⇒ `protocol_id_` 回答「这桥属于哪个来源族」,`bridge_id_` 回答「哪个具体桥」。
//   ⇒ 判据形态:注册/装配时读实例声明的 `family` ⇒ 断言
//      `bridge->protocol_id() == protocol_family_id(family)` 且 `!= 0`。
//
// ── 编号取值 ──────────────────────────────────────────────────────────
//   `0` **保留为「未分配」** ⇒ 与 `ProtocolBridgeEr::metadata_ok()` 的
//   「`protocol_id_` 非零」判据配套(0 必须能表达「没填」这一可测状态)。
//
// ── 与实例声明的一致性(测试层)──────────────────────────────────────
//   本头只定义**枚举 ↔ 编号 ↔ 字符串**的唯一真源;`protocol_instances.json`
//   里每个实例的 `family` 字符串是否都落在本枚举域内 —— 由
//   `test/test_cep_protocol_bridge_contracts.cc` 在**测试层**核对(per
//   `ADR-CEP-12` §4 的抓取点口径:实例表真源在 JSON,枚举真源在本头)。
#ifndef TLM_CEP_PROTOCOL_FAMILY_HH
#define TLM_CEP_PROTOCOL_FAMILY_HH

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tlm::cep {

/// 协议**来源族**(per `ADR-CEP-12` 的 13 属性之一 `family`)。
enum class ProtocolFamily : std::uint8_t {
    Industrial = 1,  ///< 工业协议族(per `07 号 §5.1` 口径)
    RubySlicc = 2,   ///< Ruby SLICC 族(per `07 号 §5.2` 口径)
    Academic = 3,    ///< 学术协议族(per `07 号 §5.3` 口径)
};

/// 「未分配」编号 —— `0` 保留(见文件头「编号取值」段)
inline constexpr std::uint8_t kUnassignedProtocolFamilyId = 0;

/// 来源族总数(per `ADR-CEP-12` 的 `family` 域 —— 恰好 3 值)
inline constexpr std::size_t kProtocolFamilyCount = 3;

/// 全部来源族(顺序 = 编号顺序)—— **唯一真源**
inline constexpr ProtocolFamily kProtocolFamilies[kProtocolFamilyCount] = {
    ProtocolFamily::Industrial,
    ProtocolFamily::RubySlicc,
    ProtocolFamily::Academic,
};

/// 来源族 → 数字编号
constexpr std::uint8_t protocol_family_id(ProtocolFamily f) {
    return static_cast<std::uint8_t>(f);
}

/// 数字编号 → 来源族(**越界返回 false** —— 不静默兜底)
constexpr bool protocol_family_from_id(std::uint8_t id, ProtocolFamily& out) {
    for (std::size_t i = 0; i < kProtocolFamilyCount; ++i) {
        if (protocol_family_id(kProtocolFamilies[i]) == id) {
            out = kProtocolFamilies[i];
            return true;
        }
    }
    return false;
}

/// 来源族 → 规范字符串(与 `protocol_instances.json` 的 `family` 取值**逐字一致**)
constexpr const char* protocol_family_name(ProtocolFamily f) {
    switch (f) {
        case ProtocolFamily::Industrial:
            return "industrial";
        case ProtocolFamily::RubySlicc:
            return "ruby-slicc";
        case ProtocolFamily::Academic:
            return "academic";
    }
    return "";  // 越界(防御性)
}

/// 规范字符串 → 来源族(**未知字符串返回 false** —— 不静默兜底到某个族)
constexpr bool protocol_family_from_name(std::string_view s, ProtocolFamily& out) {
    for (std::size_t i = 0; i < kProtocolFamilyCount; ++i) {
        if (s == protocol_family_name(kProtocolFamilies[i])) {
            out = kProtocolFamilies[i];
            return true;
        }
    }
    return false;
}

// ── 编译期锁 ─────────────────────────────────────────────────────────
// ① 枚举值与「编号 = 枚举值」的一致性(本文件的编号表**不是第二份数据**)
// ② 「未分配」编号 0 **不与**任何族冲突
// ③ 族集合大小 = 声明的 `kProtocolFamilyCount`
static_assert(protocol_family_id(ProtocolFamily::Industrial) == 1,
              "ProtocolFamily 编号漂移:Industrial");
static_assert(protocol_family_id(ProtocolFamily::RubySlicc) == 2,
              "ProtocolFamily 编号漂移:RubySlicc");
static_assert(protocol_family_id(ProtocolFamily::Academic) == 3,
              "ProtocolFamily 编号漂移:Academic");
static_assert(kUnassignedProtocolFamilyId == 0, "「未分配」编号必须为 0");
static_assert(kProtocolFamilyCount == 3, "来源族应为 3 值(per ADR-CEP-12 的 family 域)");

}  // namespace tlm::cep

#endif  // TLM_CEP_PROTOCOL_FAMILY_HH
