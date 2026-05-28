#include <catch2/catch_all.hpp>
#include "core/module_factory.hh"
#include "core/coherence_domain.hh"

TEST_CASE("validate_domain_boundary passes for same domain", "[domain][boundary]") {
    cpptlm::CoherenceDomain domain("cpu_domain");
    domain.add_member("cpu0");
    domain.add_member("l2_cache");
    cpptlm::DomainRegistry::register_domain("cpu_domain", domain);

    bool result = ModuleFactory::validate_domain_boundary("cpu0", "l2_cache", "cpu_domain");
    REQUIRE(result == true);

    cpptlm::DomainRegistry::clear();
}

TEST_CASE("validate_domain_boundary rejects cross-domain without bridge", "[domain][boundary]") {
    cpptlm::CoherenceDomain domain("cpu_domain");
    domain.add_member("cpu0");
    cpptlm::DomainRegistry::register_domain("cpu_domain", domain);

    bool result = ModuleFactory::validate_domain_boundary("cpu0", "gpu0", "cpu_domain");
    REQUIRE(result == false);

    cpptlm::DomainRegistry::clear();
}

TEST_CASE("validate_domain_boundary passes cross-domain with bridge", "[domain][boundary]") {
    cpptlm::CoherenceDomain domain("cpu_domain");
    domain.add_member("cpu0");
    domain.register_bridge("gpu_domain", "bridge_cpu_gpu");
    cpptlm::DomainRegistry::register_domain("cpu_domain", domain);

    bool result = ModuleFactory::validate_domain_boundary("cpu0", "gpu0", "cpu_domain", "bridge_cpu_gpu");
    REQUIRE(result == true);

    cpptlm::DomainRegistry::clear();
}

TEST_CASE("validate_domain_boundary logs error for invalid config", "[domain][boundary]") {
    bool result = ModuleFactory::validate_domain_boundary("", "gpu0", "cpu_domain");
    REQUIRE(result == false);
}