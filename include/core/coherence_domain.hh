// include/core/coherence_domain.hh
// Phase 4.2: CoherenceDomain C++ Module
// TDD Red Phase: Stub for compilation

#ifndef COHERENCE_DOMAIN_HH
#define COHERENCE_DOMAIN_HH

#include "sim_object.hh"
#include <vector>
#include <string>
#include <memory>

enum class Protocol {
    MESI,
    MOESI
};

class CoherenceDomain : public SimObject {
private:
    Protocol protocol_ = Protocol::MESI;
    std::vector<std::string> members_;
    int snoop_fanout_ = 1;

public:
    CoherenceDomain(const std::string& name, EventQueue* eq);
    ~CoherenceDomain() override = default;

    bool set_protocol(Protocol p);
    bool set_members(const std::vector<std::string>& members);
    bool set_snoop_fanout(int fanout);

    bool is_member(const std::string& id) const;
    std::vector<std::string> get_snoop_targets() const;
    std::string lookup_home_node(uint64_t addr) const;
    void tick() override {}
};

#endif // COHERENCE_DOMAIN_HH