#pragma once

#include "ch.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

using namespace ch::core;

namespace cpptlm {
namespace bundles {

struct CacheReqBundle : public bundle_base<CacheReqBundle> {
  using Self = CacheReqBundle;
  ch_uint<64> transaction_id;
  ch_uint<64> address;
  ch_uint<8> size;
  ch_bool is_write;
  ch_uint<64> data;

  CacheReqBundle() = default;
  explicit CacheReqBundle(const std::string &prefix) {
    this->set_name_prefix(prefix);
  }

  CH_BUNDLE_FIELDS_T(transaction_id, address, size, is_write, data)

  void as_master_direction() {
    this->make_output(transaction_id, address, size, is_write, data);
  }

  void as_slave_direction() {
    this->make_input(transaction_id, address, size, is_write, data);
  }
};

struct CacheRespBundle : public bundle_base<CacheRespBundle> {
  using Self = CacheRespBundle;
  ch_uint<64> transaction_id;
  ch_uint<64> data;
  ch_bool is_hit;
  ch_uint<8> error_code;

  CacheRespBundle() = default;
  explicit CacheRespBundle(const std::string &prefix) {
    this->set_name_prefix(prefix);
  }

  CH_BUNDLE_FIELDS_T(transaction_id, data, is_hit, error_code)

  void as_master_direction() {
    this->make_input(transaction_id, data, is_hit, error_code);
  }

  void as_slave_direction() {
    this->make_output(transaction_id, data, is_hit, error_code);
  }
};

} // namespace bundles
} // namespace cpptlm
