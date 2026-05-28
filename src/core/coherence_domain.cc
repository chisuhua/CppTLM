// src/core/coherence_domain.cc
// Phase 4.2: CoherenceDomain Implementation

#include "core/coherence_domain.hh"

CoherenceDomain::CoherenceDomain(const std::string& name, EventQueue* eq)
    : SimObject(name, eq) {}

bool CoherenceDomain::set_protocol(Protocol p) {
    protocol_ = p;
    return true;
}

bool CoherenceDomain::set_members(const std::vector<std::string>& members) {
    members_ = members;
    return true;
}

bool CoherenceDomain::set_snoop_fanout(int fanout) {
    snoop_fanout_ = fanout;
    return true;
}

bool CoherenceDomain::is_member(const std::string& id) const {
    return std::find(members_.begin(), members_.end(), id) != members_.end();
}

std::vector<std::string> CoherenceDomain::get_snoop_targets() const {
    return members_;
}

std::string CoherenceDomain::lookup_home_node(uint64_t addr) const {
    if (members_.empty()) {
        return "";
    }
    size_t index = (addr / 64) % members_.size();
    return members_[index];
}

void CoherenceDomain::register_bridge(const std::string& target_domain, const std::string& bridge_name) {
    bridge_map_[target_domain] = bridge_name;
}

bool CoherenceDomain::has_bridge_to(const std::string& target_domain) const {
    return bridge_map_.find(target_domain) != bridge_map_.end();
}

const std::string& CoherenceDomain::get_bridge(const std::string& target_domain) const {
    static const std::string empty;
    auto it = bridge_map_.find(target_domain);
    return it != bridge_map_.end() ? it->second : empty;
}