#include "tlm/traffic_gen_tlm.hh"
#include "core/event_queue.hh"
#include <iostream>

int main() {
    std::cout << "=== TrafficGenTLM Performance Stats Demo ===\n\n";
    
    EventQueue eq;
    TrafficGenTLM gen("traffic_gen", &eq);
    gen.set_mode(GenMode_TLM::RANDOM);
    gen.set_num_requests(10000);
    
    std::cout << "--- Generating 10000 requests ---\n";
    for (int i = 0; i < 50000; ++i) {
        gen.tick();
    }
    
    std::cout << "\n--- Stats Output (gem5 format) ---\n\n";
    gen.dumpStats(std::cout);
    
    std::cout << "\n--- Derived Metrics ---\n";
    auto& s = gen.stats();
    auto& issued = static_cast<tlm_stats::Scalar&>(*s.stats().at("requests_issued"));
    auto& reads = static_cast<tlm_stats::Scalar&>(*s.stats().at("reads"));
    auto& writes = static_cast<tlm_stats::Scalar&>(*s.stats().at("writes"));
    
    double read_pct = 100.0 * reads.value() / issued.value();
    double write_pct = 100.0 * writes.value() / issued.value();
    
    std::cout << "  read_ratio:   " << read_pct << "%\n";
    std::cout << "  write_ratio:  " << write_pct << "%\n";
    
    return 0;
}
