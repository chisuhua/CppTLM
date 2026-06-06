// include/rtl/fragment_mapper.hh
// FragmentMapper: TLM Packet ↔ RTL Beat 映射层（保留轻量级 POD）
// 功能描述：薄映射函数，以 TransactionContextExt 为真值源
//           复用 ADR-X.6 TransactionContext + ADR-X.13 多 extension API
// 作者 CppTLM Team
// 日期 2026-06-06
#ifndef RTL_FRAGMENT_MAPPER_HH
#define RTL_FRAGMENT_MAPPER_HH

#include "ext/transaction_context_ext.hh"
#include "core/packet.hh"
#include <cstdint>
#include <cstring>

namespace cpptlm {
namespace rtl {

// RTL-side beat 表示（轻量级 POD，无 CppHDL 依赖）
// 注：v4 重构后 Bundle 自带 fragment 字段，beat 仅作为 FragmentMapper
//     的中间数据结构，用于在 TLM 端提取后由 Wrapper 写入 CppHDL Bundle
struct CacheReqBeatRTL {
    uint64_t tid;
    uint64_t parent_id;
    uint8_t  fragment_id;
    uint8_t  fragment_total;
    uint64_t addr;
    uint64_t data;
    uint8_t  strb;
    bool first;
    bool last;

    CacheReqBeatRTL()
        : tid(0), parent_id(0), fragment_id(0), fragment_total(1)
        , addr(0), data(0), strb(0xFF)
        , first(true), last(true) {}
};

struct CacheRespBeatRTL {
    uint64_t tid;
    uint64_t parent_id;
    uint8_t  fragment_id;
    uint8_t  fragment_total;
    uint64_t data;
    bool     hit;
    uint8_t  error_code;
    bool     first;
    bool     last;

    CacheRespBeatRTL()
        : tid(0), parent_id(0), fragment_id(0), fragment_total(1)
        , data(0), hit(false), error_code(0)
        , first(true), last(true) {}
};

class FragmentMapper {
public:
    static CacheReqBeatRTL serialize_req(Packet* pkt) {
        CacheReqBeatRTL beat;
        if (!pkt || !pkt->payload) return beat;

        const TransactionContextExt* ext = get_transaction_context(pkt->payload);

        if (ext) {
            beat.tid            = ext->transaction_id;
            beat.parent_id      = ext->parent_id;
            beat.fragment_id    = ext->fragment_id;
            beat.fragment_total = ext->fragment_total;
            beat.first          = ext->is_first_fragment();
            beat.last           = ext->is_last_fragment();
        } else {
            beat.tid            = pkt->stream_id;
            beat.parent_id      = 0;
            beat.fragment_id    = 0;
            beat.fragment_total = 1;
            beat.first          = true;
            beat.last           = true;
        }

        beat.addr = pkt->payload->get_address();
        if (pkt->payload->get_data_length() >= sizeof(uint64_t)) {
            std::memcpy(&beat.data, pkt->payload->get_data_ptr(), sizeof(uint64_t));
        } else if (pkt->payload->get_data_length() > 0) {
            std::memcpy(&beat.data, pkt->payload->get_data_ptr(),
                        pkt->payload->get_data_length());
        }
        beat.strb = 0xFF;

        return beat;
    }

    static CacheReqBeatRTL serialize_beat_at(Packet* pkt, uint8_t beat_index) {
        CacheReqBeatRTL beat = serialize_req(pkt);
        beat.fragment_id = beat_index;
        beat.first = (beat_index == 0);
        beat.last  = (beat_index + 1 >= beat.fragment_total);
        return beat;
    }

    static void write_resp(Packet* resp_pkt, const CacheRespBeatRTL& beat) {
        if (!resp_pkt || !resp_pkt->payload) return;

        resp_pkt->payload->template release_extension<TransactionContextExt>();

        auto* ext = new TransactionContextExt();
        ext->transaction_id = beat.tid;
        ext->parent_id      = beat.parent_id;
        ext->fragment_id    = beat.fragment_id;
        ext->fragment_total = beat.fragment_total;
        resp_pkt->payload->template set_extension<TransactionContextExt>(ext);

        resp_pkt->set_transaction_id(beat.tid);

        if (resp_pkt->payload->get_data_length() >= sizeof(uint64_t)) {
            std::memcpy(resp_pkt->payload->get_data_ptr(), &beat.data, sizeof(uint64_t));
        }
    }

    static uint8_t beats_remaining(const CacheReqBeatRTL& beat) {
        if (beat.fragment_total == 0) return 0;
        if (beat.fragment_id >= beat.fragment_total) return 0;
        return static_cast<uint8_t>(beat.fragment_total - beat.fragment_id - 1);
    }

    static uint64_t group_key(const CacheReqBeatRTL& beat) {
        return beat.parent_id != 0 ? beat.parent_id : beat.tid;
    }

    static bool is_single_beat(const CacheReqBeatRTL& beat) {
        return beat.fragment_total <= 1;
    }
};

} // namespace rtl
} // namespace cpptlm

#endif // RTL_FRAGMENT_MAPPER_HH
