// include/bundles/bundle_serialization.hh
// Bundle ↔ raw bytes 序列化（仿真环境专用）
// 功能描述：在仿真进程内将 Bundle 结构体序列化为裸字节流
//           用于 StreamAdapter 在 Packet payload 中传输 Bundle 数据
// 作者 CppTLM Team
// 日期 2026-04-12
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace bundles {

/**
 * @brief 将 Bundle 序列化为裸字节（仿真环境专用）
 * 
 * 设计约束：
 * - 仅在单一仿真进程内使用（生产者和消费者在同一内存空间）
 * - 跨进程/跨平台场景需改用位域序列化（future work）
 */
template <typename BundleT>
bool serialize_bundle(const BundleT &bundle, void *buf, size_t len) {
  if (len < sizeof(BundleT)) return false;
  std::memcpy(buf, &bundle, sizeof(BundleT));
  return true;
}

/**
 * @brief 从裸字节反序列化为 Bundle（仿真环境专用）
 */
template <typename BundleT>
bool deserialize_bundle(const void *data, size_t len, BundleT &out) {
  if (len < sizeof(BundleT)) return false;
  std::memcpy(&out, data, sizeof(BundleT));
  return true;
}

} // namespace bundles
