// src/core/port_compatibility.cc
// SPDX-License-Identifier: Apache-2.0
// Phase 3.2: 端口兼容性检查矩阵实现
// 作者: CppTLM 开发团队
// 创建日期: 2026-05-07

#include "core/port_compatibility.hh"
#include <algorithm>

namespace cpptlm {

// ============================================================================
// PortCompatibility 成员函数实现
// ============================================================================

bool PortCompatibility::is_role_compatible(PortRole src, PortRole dst) {
    return check_role_matrix(src, dst);
}

bool PortCompatibility::is_bundle_compatible(BundleType src, BundleType dst) {
    return check_bundle_matrix(src, dst);
}

bool PortCompatibility::is_width_compatible(unsigned src_width, unsigned dst_width) {
    // L3: 宽度不匹配只产生 WARNING，不阻止连接
    return src_width == dst_width;
}

bool PortCompatibility::is_compatible(const PortSpec& src, const PortSpec& dst) {
    // 综合检查（三层全部验证）
    if (!check_role_matrix(src.role, dst.role)) return false;
    if (!check_bundle_matrix(src.bundle, dst.bundle)) return false;
    // L3 宽度检查：警告但不拒绝
    return true;
}

std::string PortCompatibility::get_incompatibility_reason(const PortSpec& src, const PortSpec& dst) {
    std::string reason;

    if (!check_role_matrix(src.role, dst.role)) {
        reason += "Incompatible port roles: ";
        reason += role_name(src.role);
        reason += " -> ";
        reason += role_name(dst.role);
    }

    if (!check_bundle_matrix(src.bundle, dst.bundle)) {
        if (!reason.empty()) reason += "; ";
        reason += "Incompatible bundle types: ";
        reason += bundle_name(src.bundle);
        reason += " -> ";
        reason += bundle_name(dst.bundle);
    }

    if (src.width != dst.width) {
        if (!reason.empty()) reason += "; ";
        reason += "Width mismatch: ";
        reason += std::to_string(src.width);
        reason += " bits vs ";
        reason += std::to_string(dst.width);
        reason += " bits";
    }

    return reason;
}

const char* PortCompatibility::role_name(PortRole role) {
    switch (role) {
        case PortRole::INITIATOR: return "INITIATOR";
        case PortRole::TARGET: return "TARGET";
        case PortRole::BI_DIRECTIONAL: return "BI_DIRECTIONAL";
        case PortRole::NETWORK: return "NETWORK";
        case PortRole::PE: return "PE";
        default: return "UNKNOWN";
    }
}

const char* PortCompatibility::bundle_name(BundleType bundle) {
    switch (bundle) {
        case BundleType::CACHE_REQ: return "CACHE_REQ";
        case BundleType::CACHE_RESP: return "CACHE_RESP";
        case BundleType::NOC_FLIT: return "NOC_FLIT";
        case BundleType::GENERIC: return "GENERIC";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// L1: PortRole 兼容性矩阵
// ============================================================================

// 兼容性矩阵映射表
// true = 允许连接, false = 禁止连接
//
// | Src\Dst  | INITIATOR | TARGET | BI_DIR | NETWORK | PE   |
// |----------|:---------:|:------:|:------:|:-------:|:----:|
// | INITIATOR|    ❌     |   ✅   |   ✅   |    ❌    |  ❌  |
// | TARGET   |    ❌     |   ❌   |   ❌   |    ❌    |  ❌  |
// | BI_DIR   |    ✅     |   ✅   |   ✅   |    ✅    |  ❌  |
// | NETWORK  |    ❌     |   ❌   |   ✅   |    ✅    |  ✅  |
// | PE       |    ❌     |   ❌   |   ❌   |    ✅    |  ❌  |

bool PortCompatibility::check_role_matrix(PortRole src, PortRole dst) {
    // 使用静态二维数组实现兼容性矩阵
    static const bool role_matrix[5][5] = {
        // INITIATOR  TARGET  BI_DIR  NETWORK PE
        /* INITIATOR */ { false,  true,   true,   false,  false },
        /* TARGET    */ { false,  false,  false,  false,  false },
        /* BI_DIR    */ { true,   true,   true,   true,   false },
        /* NETWORK   */ { false,  false,  true,   true,   true  },
        /* PE        */ { false,  false,  false,  true,   false }
    };

    auto idx = [](PortRole role) -> size_t {
        switch (role) {
            case PortRole::INITIATOR: return 0;
            case PortRole::TARGET: return 1;
            case PortRole::BI_DIRECTIONAL: return 2;
            case PortRole::NETWORK: return 3;
            case PortRole::PE: return 4;
            default: return 0;
        }
    };

    return role_matrix[idx(src)][idx(dst)];
}

// ============================================================================
// L2: BundleType 兼容性矩阵
// ============================================================================

// Bundle 兼容性矩阵
// true = 兼容, false = 不兼容（需转换器）, ⚠️ = 警告但允许
//
// | Src\Dst     | CACHE_REQ | CACHE_RESP | NOC_FLIT | GENERIC |
// |------------|:---------:|:----------:|:--------:|:-------:|
// | CACHE_REQ  |    ✅     |     ❌      |    ❌    |    ⚠️   |
// | CACHE_RESP |    ❌     |     ✅      |    ❌    |    ⚠️   |
// | NOC_FLIT   |    ❌     |     ❌      |    ✅    |    ⚠️   |
// | GENERIC    |    ⚠️     |     ⚠️      |    ⚠️    |    ✅   |

bool PortCompatibility::check_bundle_matrix(BundleType src, BundleType dst) {
    static const bool bundle_matrix[4][4] = {
        // CACHE_REQ  CACHE_RESP  NOC_FLIT  GENERIC
        /* CACHE_REQ  */ { true,    false,    false,    true  },
        /* CACHE_RESP */ { false,   true,    false,    true  },
        /* NOC_FLIT   */ { false,   false,   true,     true  },
        /* GENERIC    */ { true,    true,    true,     true  }
    };

    auto idx = [](BundleType bundle) -> size_t {
        switch (bundle) {
            case BundleType::CACHE_REQ: return 0;
            case BundleType::CACHE_RESP: return 1;
            case BundleType::NOC_FLIT: return 2;
            case BundleType::GENERIC: return 3;
            default: return 0;
        }
    };

    return bundle_matrix[idx(src)][idx(dst)];
}

} // namespace cpptlm