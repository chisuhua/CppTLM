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