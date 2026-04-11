// include/modules/example_tlm_module.hh
// Example TLM Module demonstrating TLMModule base class usage

#ifndef EXAMPLE_TLM_MODULE_HH
#define EXAMPLE_TLM_MODULE_HH

#include "core/tlm_module.hh"

/**
 * @brief Example Crossbar using TLM lifecycle hooks
 * 
 * This crossbar simply logs transaction hops and forwards them.
 */
class ExampleCrossbar : public TLMModule {
public:
    ExampleCrossbar(const std::string& name, EventQueue* eq)
        : TLMModule(name, eq) {}

    void tick() override {
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            
            // Use TLM Hook to handle transaction
            TransactionInfo info = onTransactionHop(pkt);
            
            // In a real system, we would send pkt to output ports here based on info.action
            if (info.action == TransactionAction::PASSTHROUGH) {
                // Forward logic...
            }
            
            // Clean up (stub logic)
            PacketPool::get().release(pkt);
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        input_queue.push(pkt);
        return true;
    }

private:
    std::queue<Packet*> input_queue;
};

#endif
