// include/core/port_compatibility.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.2: 端口兼容性检查矩阵
// 作者: CppTLM 开发团队
// 创建日期: 2026-05-07

#pragma once

#include "port_types.hh"
#include <string>

namespace cpptlm {

// 端口兼容性检查类
// L1: 方向检查 (PortRole)
// L2: Bundle 匹配检查 (BundleType)
// L3: 数据宽度检查 (width)
class PortCompatibility {
public:
    // L1: 方向检查
    static bool is_role_compatible(PortRole src, PortRole dst);

    // L2: Bundle 匹配检查
    static bool is_bundle_compatible(BundleType src, BundleType dst);

    // L3: 宽度检查（WARNING 而非 ERROR）
    static bool is_width_compatible(unsigned src_width, unsigned dst_width);

    // 综合检查（三层全部检查）
    static bool is_compatible(const PortSpec& src, const PortSpec& dst);

    // 获取不兼容原因（用于错误/警告消息）
    static std::string get_incompatibility_reason(const PortSpec& src, const PortSpec& dst);

    // 获取角色名称字符串
    static const char* role_name(PortRole role);

    // 获取 Bundle 类型名称字符串
    static const char* bundle_name(BundleType bundle);

private:
    // L1 兼容性矩阵查询
    static bool check_role_matrix(PortRole src, PortRole dst);

    // L2 Bundle 兼容性矩阵查询
    static bool check_bundle_matrix(BundleType src, BundleType dst);
};

// ============================================================================
// 兼容性矩阵定义
// ============================================================================

// L1: PortRole 兼容性矩阵
// true = 允许连接, false = 禁止连接
//
// | Src\Dst  | INITIATOR | TARGET | BI_DIR | NETWORK | PE   |
// |----------|:---------:|:------:|:------:|:-------:|:----:|
// | INITIATOR|    ❌     |   ✅   |   ✅   |    ❌    |  ❌  |
// | TARGET   |    ❌     |   ❌   |   ❌   |    ❌    |  ❌  |
// | BI_DIR   |    ✅     |   ✅   |   ✅   |    ✅    |  ❌  |
// | NETWORK  |    ❌     |   ❌   |   ✅   |    ✅    |  ✅  |
// | PE       |    ❌     |   ❌   |   ❌   |    ✅    |  ❌  |

// L2: BundleType 兼容性矩阵
// true = 兼容, false = 不兼容（需转换器）
//
// | Src\Dst     | CACHE_REQ | CACHE_RESP | NOC_FLIT | GENERIC |
// |------------|:---------:|:----------:|:--------:|:-------:|
// | CACHE_REQ  |    ✅     |     ❌      |    ❌    |    ⚠️   |
// | CACHE_RESP |    ❌     |     ✅      |    ❌    |    ⚠️   |
// | NOC_FLIT   |    ❌     |     ❌      |    ✅    |    ⚠️   |
// | GENERIC    |    ⚠️     |     ⚠️      |    ⚠️    |    ✅   |

// L3: Width 兼容性
// 位宽不一致产生 WARNING 但不阻止连接

} // namespace cpptlm