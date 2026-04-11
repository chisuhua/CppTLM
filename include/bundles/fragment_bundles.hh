#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"
#include <cstddef>
#include <cstring>

using namespace ch::core;

namespace cpptlm {
namespace bundles {

struct FragmentBundle : public bundle_base<FragmentBundle> {
  using Self = FragmentBundle;
  ch_uint<64> transaction_id;
  ch_uint<8> fragment_id;
  ch_uint<8> fragment_total;
  ch_uint<64> data;
  ch_bool is_last;

  FragmentBundle() = default;
  explicit FragmentBundle(const std::string &prefix) {
    this->set_name_prefix(prefix);
  }

  CH_BUNDLE_FIELDS_T(transaction_id, fragment_id, fragment_total, data, is_last)

  void as_master_direction() {
    this->make_output(transaction_id, fragment_id, fragment_total, data, is_last);
  }

  void as_slave_direction() {
    this->make_input(transaction_id, fragment_id, fragment_total, data, is_last);
  }
};

class BundleSerializer {
public:
  template <typename BundleT>
  static size_t serialized_size() {
    return sizeof(BundleT);
  }

  template <typename BundleT>
  static bool serialize(const BundleT &bundle, void *buf, size_t len) {
    if (len < sizeof(BundleT)) {
      return false;
    }
    std::memcpy(buf, &bundle, sizeof(BundleT));
    return true;
  }

  template <typename BundleT>
  static bool deserialize(const void *data, size_t len, BundleT &out) {
    if (len < sizeof(BundleT)) {
      return false;
    }
    std::memcpy(&out, data, sizeof(BundleT));
    return true;
  }
};

} // namespace bundles
} // namespace cpptlm
