// include/core/tlm_module.hh
#ifndef TLM_MODULE_HH
#define TLM_MODULE_HH

#include "core/sim_object.hh"
#include "core/packet_pool.hh"
#include <map>
#include <atomic>

/**
 * @brief Base class for TLM Modules
 * Defines transaction lifecycle hooks and fragment reassembly
 */
class TLMModule : public SimObject {
protected:
    std::map<uint64_t, FragmentBuffer> fragment_buffers_;
    bool enable_fragment_reassembly_ = false;

public:
    TLMModule(const std::string& name, EventQueue* eq)
        : SimObject(name, eq) {}
    
    virtual ~TLMModule() = default;

    /** @brief Enable/Disable fragment reassembly buffering */
    void enableFragmentReassembly(bool e) { enable_fragment_reassembly_ = e; }

    // =========================================================================
    // Transaction Lifecycle Hooks
    // =========================================================================
    
    /** Called when this module initiates a transaction (Source) */
    virtual TransactionInfo onTransactionStart(Packet* pkt) { 
        TransactionInfo info; 
        if (pkt) info.transaction_id = pkt->get_transaction_id();
        return info; 
    }

    /** Called when a transaction hops through this module (Intermediate) */
    virtual TransactionInfo onTransactionHop(Packet* pkt) { 
        TransactionInfo info;
        info.action = TransactionAction::PASSTHROUGH;
        if (pkt) info.transaction_id = pkt->get_transaction_id();
        return info; 
    }

    /** Called when a transaction terminates at this module (Destination) */
    virtual TransactionInfo onTransactionEnd(Packet* pkt) { 
        TransactionInfo info;
        info.action = TransactionAction::TERMINATE;
        if (pkt) info.transaction_id = pkt->get_transaction_id();
        return info; 
    }

    /** Create sub-transaction (e.g. Cache Miss -> Memory) */
    virtual uint64_t createSubTransaction(Packet* parent, Packet* child) {
        static std::atomic<uint64_t> g_sub_tid{20000};
        uint64_t tid = g_sub_tid.fetch_add(1);
        if (child && parent) {
            child->set_transaction_id(tid);
            if (auto* ext = get_transaction_context(child->payload)) {
                ext->parent_id = parent->get_transaction_id();
            } else {
                create_transaction_context(child->payload, tid, parent->get_transaction_id(), 0, 1);
            }
        }
        return tid;
    }

    // =========================================================================
    // Fragment Reassembly
    // =========================================================================

    /**
     * Handle incoming packet. 
     * If fragmented and enabled, buffers it. Returns true if ready to process (complete).
     */
    virtual bool processWithFragmentation(Packet* pkt) {
        if (!pkt || !enable_fragment_reassembly_ || !pkt->is_fragmented()) {
            return true; // Pass through immediately
        }

        auto* ext = get_transaction_context(pkt->payload);
        if (!ext) return true;

        uint64_t group = ext->get_group_key();
        auto& buf = fragment_buffers_[group];
        buf.parent_id = group;
        buf.fragment_total = ext->fragment_total;
        buf.first_arrival_time = buf.first_arrival_time ? buf.first_arrival_time : getCurrentCycle();

        if (!buf.has_fragment(ext->fragment_id)) {
            buf.fragments[ext->fragment_id] = pkt;
        }

        if (buf.is_complete()) {
            onFragmentGroupComplete(buf);
            return false; // Group handled, don't process single packet further
        }
        
        return false; // Still waiting
    }

    virtual void onFragmentGroupComplete(FragmentBuffer& buf) {
        // Default: Release all. Derived classes should reassemble or forward.
        for (auto& [id, p] : buf.fragments) {
            // Logic for reassembly/fwd goes here
        }
    }

    // =========================================================================
    // Reset / Cleanup
    // =========================================================================
    void do_reset(const ResetConfig& config) override {
        // 1. Release all pending fragments
        for (auto& [id, buf] : fragment_buffers_) {
            for (auto& [fid, pkt] : buf.fragments) {
                if (pkt) PacketPool::get().release(pkt);
            }
        }
        fragment_buffers_.clear();

        // 2. Call Base
        SimObject::do_reset(config);
    }
};

#endif // TLM_MODULE_HH
