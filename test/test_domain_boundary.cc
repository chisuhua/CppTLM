#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/coherence_domain.hh"
#include "core/event_queue.hh"

TEST_CASE("validate_domain_boundary passes for same domain", "[domain][boundary]") {
    EventQueue eq;
    CoherenceDomain domain("cpu_domain", &eq);
    domain.set_members({"cpu0", "l2_cache"});
    DomainRegistry::register_domain("cpu_domain", &domain);

    bool result = ModuleFactory::validate_domain_boundary("cpu0", "l2_cache", "cpu_domain");
    REQUIRE(result == true);

    DomainRegistry::clear();
}

TEST_CASE("validate_domain_boundary rejects cross-domain without bridge", "[domain][boundary]") {
    EventQueue eq;
    CoherenceDomain domain("cpu_domain", &eq);
    domain.set_members({"cpu0"});
    DomainRegistry::register_domain("cpu_domain", &domain);

    bool result = ModuleFactory::validate_domain_boundary("cpu0", "gpu0", "cpu_domain");
    REQUIRE(result == false);

    DomainRegistry::clear();
}

TEST_CASE("validate_domain_boundary passes cross-domain with bridge", "[domain][boundary]") {
    EventQueue eq;
    CoherenceDomain domain("cpu_domain", &eq);
    domain.set_members({"cpu0"});
    domain.register_bridge("gpu_domain", "bridge_cpu_gpu");
    DomainRegistry::register_domain("cpu_domain", &domain);

    bool result = ModuleFactory::validate_domain_boundary("cpu0", "gpu0", "cpu_domain", "bridge_cpu_gpu");
    REQUIRE(result == true);

    DomainRegistry::clear();
}

TEST_CASE("validate_domain_boundary logs error for invalid config", "[domain][boundary]") {
    bool result = ModuleFactory::validate_domain_boundary("", "gpu0", "cpu_domain");
    REQUIRE(result == false);
}