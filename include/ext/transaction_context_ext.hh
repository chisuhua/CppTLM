// include/ext/transaction_context_ext.hh
// Transaction Context Extension for CppTLM v2.0
#ifndef TRANSACTION_CONTEXT_EXT_HH
#define TRANSACTION_CONTEXT_EXT_HH

#include "tlm.h"
#include <vector>
#include <string>
#include <cstdint>

struct TraceEntry {
    std::string module;
    uint64_t timestamp;
    uint64_t latency;
    std::string event;
    
    TraceEntry() : timestamp(0), latency(0) {}
    TraceEntry(const std::string& m, uint64_t ts, uint64_t lat, const std::string& e)
        : module(m), timestamp(ts), latency(lat), event(e) {}
};

struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    uint64_t transaction_id = 0;
    uint64_t parent_id = 0;
    uint8_t  fragment_id = 0;
    uint8_t  fragment_total = 1;
    uint64_t create_timestamp = 0;
    std::string source_module;
    std::string type;
    uint8_t  priority = 0;
    std::vector<TraceEntry> trace_log;
    
    tlm_extension* clone() const override {
        return new TransactionContextExt(*this);
    }
    
    void copy_from(const tlm_extension_base& ext) override {
        const auto& other = static_cast<const TransactionContextExt&>(ext);
        transaction_id = other.transaction_id;
        parent_id = other.parent_id;
        fragment_id = other.fragment_id;
        fragment_total = other.fragment_total;
        create_timestamp = other.create_timestamp;
        source_module = other.source_module;
        type = other.type;
        priority = other.priority;
        trace_log = other.trace_log;
    }
    
    void add_trace(const std::string& module, uint64_t timestamp, uint64_t latency, const std::string& event) {
        trace_log.emplace_back(module, timestamp, latency, event);
    }
    
    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
    bool is_first_fragment() const { return fragment_id == 0; }
    bool is_last_fragment() const { return fragment_id == fragment_total - 1; }
    uint64_t get_group_key() const { return parent_id != 0 ? parent_id : transaction_id; }
    
    void reset() {
        transaction_id = 0;
        parent_id = 0;
        fragment_id = 0;
        fragment_total = 1;
        create_timestamp = 0;
        source_module.clear();
        type.clear();
        priority = 0;
        trace_log.clear();
    }
};

inline TransactionContextExt* get_transaction_context(tlm::tlm_generic_payload* p) {
    if (!p) return nullptr;
    TransactionContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

inline const TransactionContextExt* get_transaction_context(const tlm::tlm_generic_payload* p) {
    if (!p) return nullptr;
    const TransactionContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

inline void set_transaction_context(tlm::tlm_generic_payload* p, const TransactionContextExt& src) {
    if (!p) return;
    TransactionContextExt* ext = new TransactionContextExt(src);
    p->set_extension(ext);
}

inline TransactionContextExt* create_transaction_context(
    tlm::tlm_generic_payload* p,
    uint64_t tid,
    uint64_t pid = 0,
    uint8_t frag_id = 0,
    uint8_t frag_total = 1
) {
    if (!p) return nullptr;
    TransactionContextExt* ext = new TransactionContextExt();
    ext->transaction_id = tid;
    ext->parent_id = pid;
    ext->fragment_id = frag_id;
    ext->fragment_total = frag_total;
    p->set_extension(ext);
    return ext;
}

#endif
