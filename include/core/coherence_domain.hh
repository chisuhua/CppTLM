// include/core/coherence_domain.hh
//
// ⚠️ [[deprecated]] —— per ADR-CEP-07A(IC1 / 决策 1 产物 1 / 决策 2 Stage 0 ①)
//    本文件**废弃**:它把 `enum class Protocol { MESI, MOESI }` 写进了通用层,
//    属**协议泄漏**(per ADR-CEP-07A 决策 3 通用禁令 / ADR-CEP-14 C9 负向断言)。
//    CEP 侧改用 **Stub Cache Core**(per ADR-CEP-05),二者职责不同(per ADR-CEP-15 决策 1)。
//    文件**保留**以免破坏既有引用(per ADR-CEP-07A 决策 4 显式排除);
//    实体删除待 Stage 4+ 视引用数而定(per ADR-CEP-07A 决策 2)。
//    原 Phase 4.2/4.3 TDD Red stub 出身说明保留于下。
//
// Phase 4.2: CoherenceDomain C++ Module with Phase 4.3 Extensions
// TDD Red Phase: Stub for compilation

#ifndef COHERENCE_DOMAIN_HH
#define COHERENCE_DOMAIN_HH

#include "sim_object.hh"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

// ⚠️ 协议标识符泄漏点(per ADR-CEP-07A 决策 3 通用禁令)—— 保留仅为不破坏既有引用;
//    新代码**不得**使用(per INV-CEP-07A-IC1:不得被新代码引用)。
enum class Protocol {
    MESI,
    MOESI
};

[[deprecated("per ADR-CEP-07A IC1")]] class CoherenceDomain : public SimObject {
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

[[deprecated("per ADR-CEP-07A IC1")]] class DomainRegistry {
private:
    static std::unordered_map<std::string, std::shared_ptr<CoherenceDomain>>& get_domains() {
        static std::unordered_map<std::string, std::shared_ptr<CoherenceDomain>> domains;
        return domains;
    }

public:
    static void register_domain(const std::string& name, std::shared_ptr<CoherenceDomain> domain) {
        get_domains()[name] = std::move(domain);
    }

    static bool domain_exists(const std::string& name) {
        return get_domains().find(name) != get_domains().end();
    }

    static std::shared_ptr<CoherenceDomain> get_domain(const std::string& name) {
        auto it = get_domains().find(name);
        return it != get_domains().end() ? it->second : nullptr;
    }

    static void clear() {
        get_domains().clear();
    }
};

#endif // COHERENCE_DOMAIN_HH