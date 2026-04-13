#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

namespace cpptlm {
namespace bundles {

using ch_uint8  = ch::core::ch_uint<8>;
using ch_uint32 = ch::core::ch_uint<32>;
using ch_uint64 = ch::core::ch_uint<64>;
using ch_bool   = ch::core::ch_bool;

struct NoCReqBundle : public ch::core::bundle_base<NoCReqBundle> {
  using Self = NoCReqBundle;
  ch_uint64 transaction_id;
  ch_uint32 src_id;
  ch_uint32 dst_id;
  ch_uint64 address;
  ch_uint64 data;
  ch_uint8  size;
  ch_bool   is_write;

  NoCReqBundle() = default;
  explicit NoCReqBundle(const std::string &prefix) {
    this->set_name_prefix(prefix);
  }

  CH_BUNDLE_FIELDS_T(transaction_id, src_id, dst_id, address, data, size, is_write)

  void as_master_direction() {
    this->make_output(transaction_id, src_id, dst_id, address, data, size, is_write);
  }

  void as_slave_direction() {
    this->make_input(transaction_id, src_id, dst_id, address, data, size, is_write);
  }
};

struct NoCRespBundle : public ch::core::bundle_base<NoCRespBundle> {
  using Self = NoCRespBundle;
  ch_uint64 transaction_id;
  ch_uint64 data;
  ch_bool   is_ok;
  ch_uint8  error_code;

  NoCRespBundle() = default;
  explicit NoCRespBundle(const std::string &prefix) {
    this->set_name_prefix(prefix);
  }

  CH_BUNDLE_FIELDS_T(transaction_id, data, is_ok, error_code)

  void as_master_direction() {
    this->make_input(transaction_id, data, is_ok, error_code);
  }

  void as_slave_direction() {
    this->make_output(transaction_id, data, is_ok, error_code);
  }
};

} // namespace bundles
} // namespace cpptlm
