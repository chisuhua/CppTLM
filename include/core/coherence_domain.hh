// include/core/coherence_domain.hh
// Phase 4.2: CoherenceDomain C++ Module with Phase 4.3 Extensions
// TDD Red Phase: Stub for compilation

#ifndef COHERENCE_DOMAIN_HH
#define COHERENCE_DOMAIN_HH

#include "sim_object.hh"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

enum class Protocol {
    MESI,
    MOESI
};

class CoherenceDomain : public SimObject {
private:
    Protocol protocol_ = Protocol::MESI;
    std::vector<std::string> members_;
    int snoop_fanout_ = 1;
    std::unordered_map<std::string, std::string> bridge_map_;

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

    void register_bridge(const std::string& target_domain, const std::string& bridge_name);
    bool has_bridge_to(const std::string& target_domain) const;
    const std::string& get_bridge(const std::string& target_domain) const;
};

class DomainRegistry {
private:
    static std::unordered_map<std::string, CoherenceDomain>& get_domains() {
        static std::unordered_map<std::string, CoherenceDomain> domains;
        return domains;
    }

public:
    static void register_domain(const std::string& name, CoherenceDomain* domain) {
        get_domains()[name] = *domain;
    }

    static bool domain_exists(const std::string& name) {
        return get_domains().find(name) != get_domains().end();
    }

    static CoherenceDomain* get_domain(const std::string& name) {
        auto it = get_domains().find(name);
        return it != get_domains().end() ? &it->second : nullptr;
    }

    static void clear() {
        get_domains().clear();
    }
};

#endif // COHERENCE_DOMAIN_HH