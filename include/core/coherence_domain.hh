#ifndef COHERENCE_DOMAIN_HH
#define COHERENCE_DOMAIN_HH

#include <string>
#include <unordered_map>
#include <vector>

namespace cpptlm {

class CoherenceDomain {
public:
    using DomainId = std::string;

private:
    DomainId domain_id_;
    std::vector<std::string> members_;
    std::unordered_map<std::string, std::string> bridge_map_;

public:
    explicit CoherenceDomain(const DomainId& id) : domain_id_(id) {}
    CoherenceDomain() : domain_id_("") {}
    CoherenceDomain(const CoherenceDomain& other)
        : domain_id_(other.domain_id_)
        , members_(other.members_)
        , bridge_map_(other.bridge_map_) {}
    CoherenceDomain& operator=(const CoherenceDomain& other) {
        if (this != &other) {
            domain_id_ = other.domain_id_;
            members_ = other.members_;
            bridge_map_ = other.bridge_map_;
        }
        return *this;
    }

    const DomainId& domain_id() const { return domain_id_; }

    void add_member(const std::string& member) {
        members_.push_back(member);
    }

    bool has_member(const std::string& member) const {
        for (const auto& m : members_) {
            if (m == member) return true;
        }
        return false;
    }

    void register_bridge(const std::string& target_domain, const std::string& bridge_name) {
        bridge_map_[target_domain] = bridge_name;
    }

    bool has_bridge_to(const std::string& target_domain) const {
        return bridge_map_.find(target_domain) != bridge_map_.end();
    }

    const std::string& get_bridge(const std::string& target_domain) const {
        static const std::string empty;
        auto it = bridge_map_.find(target_domain);
        return it != bridge_map_.end() ? it->second : empty;
    }
};

class DomainRegistry {
private:
    static std::unordered_map<std::string, CoherenceDomain>& get_domains() {
        static std::unordered_map<std::string, CoherenceDomain> domains;
        return domains;
    }

public:
    static void register_domain(const std::string& name, const CoherenceDomain& domain) {
        get_domains()[name] = domain;
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

}

#endif