# Bundle 示例

> **目录**: `include/bundles/`  
> **原则**: 一份定义，TLM/RTL 共用

---

## cache_bundles.hh

```cpp
// include/bundles/cache_bundles.hh
#ifndef CACHE_BUNDLES_HH
#define CACHE_BUNDLES_HH

#include "ch.hpp"
#include "chlib/stream.h"

// ========== Cache 请求 Bundle ==========
struct CacheReqBundle : ch::bundle_base<CacheReqBundle> {
    ch_uint<64> transaction_id;
    ch_uint<32> address;
    ch_bool is_write;
    ch_uint<32> data;
    
    CH_BUNDLE_FIELDS_T(transaction_id, address, is_write, data)
};

// ========== Cache 响应 Bundle ==========
struct CacheRespBundle : ch::bundle_base<CacheRespBundle> {
    ch_uint<64> transaction_id;
    ch_uint<32> data;
    ch_bool is_hit;
    ch_uint<8> latency_cycles;
    
    CH_BUNDLE_FIELDS_T(transaction_id, data, is_hit, latency_cycles)
};

#endif // CACHE_BUNDLES_HH
```

---

## noc_bundles.hh

```cpp
// include/bundles/noc_bundles.hh
#ifndef NOC_BUNDLES_HH
#define NOC_BUNDLES_HH

#include "ch.hpp"
#include "chlib/stream.h"

// ========== NoC 请求 Bundle ==========
struct NoCReqBundle : ch::bundle_base<NoCReqBundle> {
    ch_uint<64> transaction_id;
    ch_uint<32> src_id;
    ch_uint<32> dst_id;
    ch_uint<64> address;
    ch_uint<512> payload;
    ch_uint<8> packet_type;  // 0=READ, 1=WRITE, 2=RESPONSE
    
    CH_BUNDLE_FIELDS_T(transaction_id, src_id, dst_id, address, payload, packet_type)
};

// ========== NoC 响应 Bundle ==========
struct NoCRespBundle : ch::bundle_base<NoCRespBundle> {
    ch_uint<64> transaction_id;
    ch_uint<32> src_id;
    ch_uint<512> payload;
    ch_uint<8> status;  // 0=OK, 1=ERROR
    
    CH_BUNDLE_FIELDS_T(transaction_id, src_id, payload, status)
};

#endif // NOC_BUNDLES_HH
```

---

## fragment_bundles.hh

```cpp
// include/bundles/fragment_bundles.hh
#ifndef FRAGMENT_BUNDLES_HH
#define FRAGMENT_BUNDLES_HH

#include "ch.hpp"
#include "chlib/stream.h"

// ========== Fragment 分片 Bundle ==========
template<typename PayloadT>
struct FragmentBundle : ch::bundle_base<FragmentBundle<PayloadT>> {
    ch_uint<64> transaction_id;
    ch_uint<8> fragment_id;
    ch_uint<8> fragment_total;
    ch_bool is_first;
    ch_bool is_last;
    PayloadT data_beat;
    
    CH_BUNDLE_FIELDS_T(transaction_id, fragment_id, fragment_total, is_first, is_last, data_beat)
};

#endif // FRAGMENT_BUNDLES_HH
```

---

## axi4_bundles.hh (示例)

```cpp
// include/bundles/axi4_bundles.hh
#ifndef AXI4_BUNDLES_HH
#define AXI4_BUNDLES_HH

#include "ch.hpp"
#include "chlib/stream.h"

// ========== AXI4 写地址 Bundle ==========
struct AXI4WriteAddrBundle : ch::bundle_base<AXI4WriteAddrBundle> {
    ch_uint<64> addr;
    ch_uint<8>  burst_len;
    ch_uint<3>  burst_size;
    ch_uint<2>  burst_type;
    ch_uint<4>  id;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(addr, burst_len, burst_size, burst_type, id, valid)
};

// ========== AXI4 写数据 Bundle ==========
struct AXI4WriteDataBundle : ch::bundle_base<AXI4WriteDataBundle> {
    ch_uint<64> data;
    ch_uint<8>  strb;
    ch_bool     last;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(data, strb, last, valid)
};

// ========== AXI4 写响应 Bundle ==========
struct AXI4WriteRespBundle : ch::bundle_base<AXI4WriteRespBundle> {
    ch_uint<2>  bresp;
    ch_uint<4>  bid;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(bresp, bid, valid)
};

// ========== AXI4 读地址 Bundle ==========
struct AXI4ReadAddrBundle : ch::bundle_base<AXI4ReadAddrBundle> {
    ch_uint<64> addr;
    ch_uint<8>  burst_len;
    ch_uint<3>  burst_size;
    ch_uint<2>  burst_type;
    ch_uint<4>  id;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(addr, burst_len, burst_size, burst_type, id, valid)
};

// ========== AXI4 读数据 Bundle ==========
struct AXI4ReadDataBundle : ch::bundle_base<AXI4ReadDataBundle> {
    ch_uint<64> data;
    ch_uint<2>  rresp;
    ch_uint<4>  id;
    ch_bool     last;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(data, rresp, id, last, valid)
};

#endif // AXI4_BUNDLES_HH
```

---

## chi_bundles.hh (示例)

```cpp
// include/bundles/chi_bundles.hh
#ifndef CHI_BUNDLES_HH
#define CHI_BUNDLES_HH

#include "ch.hpp"
#include "chlib/stream.h"

// ========== CHI 数据包 Bundle ==========
struct CHIDataPacketBundle : ch::bundle_base<CHIDataPacketBundle> {
    ch_uint<8>  glob_id;
    ch_uint<5>  return_n;
    ch_uint<3>  snoop_n;
    ch_uint<64> addr;
    ch_uint<512> data;
    ch_uint<64> tag;
    ch_uint<16> trust_id;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(glob_id, return_n, snoop_n, addr, data, tag, trust_id, valid)
};

// ========== CHI 响应包 Bundle ==========
struct CHIRespPacketBundle : ch::bundle_base<CHIRespPacketBundle> {
    ch_uint<8>  glob_id;
    ch_uint<5>  return_n;
    ch_uint<64> tag;
    ch_uint<2>  resp;
    ch_bool     valid;
    
    CH_BUNDLE_FIELDS_T(glob_id, return_n, tag, resp, valid)
};

#endif // CHI_BUNDLES_HH
```

---

## 示例说明

- **Bundle 定义**：TLM/RTL 共用同一份，确保接口一致性
- **泛型支持**：`FragmentBundle<PayloadT>` 支持不同 Payload 类型
- **位宽定义**：使用 `ch_uint<N>` 定义位宽，与 CppHDL 兼容
