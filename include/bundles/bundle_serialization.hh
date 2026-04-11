#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace cpptlm {
namespace bundles {

template <typename BundleT>
bool serialize_bundle(const BundleT &bundle, void *buf, size_t len) {
  static_assert(std::is_standard_layout_v<BundleT>,
                "BundleT must be standard layout for serialization");
  if (len < sizeof(BundleT)) {
    return false;
  }
  std::memcpy(buf, &bundle, sizeof(BundleT));
  return true;
}

template <typename BundleT>
bool deserialize_bundle(const void *data, size_t len, BundleT &out) {
  static_assert(std::is_standard_layout_v<BundleT>,
                "BundleT must be standard layout for deserialization");
  if (len < sizeof(BundleT)) {
    return false;
  }
  std::memcpy(&out, data, sizeof(BundleT));
  return true;
}

} // namespace bundles
} // namespace cpptlm
