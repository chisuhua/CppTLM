// test/test_module_registration.cc
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "core/module_factory.hh"

// 测试专用 Mock 模块
class TestModuleA : public SimObject {
public:
    explicit TestModuleA(const std::string& n, EventQueue* eq) : SimObject(n, eq) {
    }
    void tick() override {
    }
};

class TestModuleB : public SimObject {
public:
    explicit TestModuleB(const std::string& n, EventQueue* eq) : SimObject(n, eq) {
    }
    void tick() override {
    }
};

TEST_CASE("Module Registration and Instantiation Tests", "[module][factory]") {
    EventQueue eq;

    SECTION("Register and unregister single module type") {
        // 获取初始注册类型数量
        size_t initial_count = ModuleFactory::getRegisteredObjectTypes().size();

        // 注册
        ModuleFactory::registerObject<TestModuleA>("TestModuleA");

        // 验证注册的类型数量增加
        REQUIRE(ModuleFactory::getRegisteredObjectTypes().size() == initial_count + 1);

        // 验证注册的类型存在
        auto types = ModuleFactory::getRegisteredObjectTypes();
        REQUIRE(std::find(types.begin(), types.end(), "TestModuleA") != types.end());

        // 注销
        bool success = ModuleFactory::unregisterObject("TestModuleA");
        REQUIRE(success == true);

        // 验证注册的类型数量恢复
        REQUIRE(ModuleFactory::getRegisteredObjectTypes().size() == initial_count);

        // 再次注销应返回 false
        success = ModuleFactory::unregisterObject("TestModuleA");
        REQUIRE(success == false);
    }

    SECTION("Register multiple types and clear all") {
        // 获取初始注册类型数量
        size_t initial_count = ModuleFactory::getRegisteredObjectTypes().size();

        ModuleFactory::registerObject<TestModuleA>("TestModuleA");
        ModuleFactory::registerObject<TestModuleB>("TestModuleB");

        // 验证注册的类型数量增加
        REQUIRE(ModuleFactory::getRegisteredObjectTypes().size() == initial_count + 2);

        ModuleFactory::clearAllObjects();

        // 验证对象注册表已清空（但不清除模块注册表）
        REQUIRE(ModuleFactory::getRegisteredObjectTypes().size() == 0);
    }

    SECTION("Instantiate module after registration") {
        // 注册
        ModuleFactory::registerObject<TestModuleA>("TestModuleA");

        json config = R"({
            "modules": [
                { "name": "inst0", "type": "TestModuleA" }
            ],
            "connections": []
        })"_json;

        ModuleFactory factory(&eq);
        REQUIRE_NOTHROW(factory.instantiateAll(config));

        SimObject* obj = factory.getInstance("inst0");
        REQUIRE(obj != nullptr);
        REQUIRE(obj->getName() == "inst0");
    }

    SECTION("Test isolation - each section starts with current state") {
        // 获取当前注册类型数量（不假设为0）
        size_t current_count = ModuleFactory::getRegisteredObjectTypes().size();

        ModuleFactory::registerObject<TestModuleA>("TestModuleA");
        REQUIRE(ModuleFactory::getRegisteredObjectTypes().size() == current_count + 1);

        // 注销我们自己注册的
        ModuleFactory::unregisterObject("TestModuleA");
    }

    // 确保清理 - 只清理对象注册，不清理模块注册（模块类型如NICTLM/RouterTLM不能清除）
    ModuleFactory::clearAllObjects();
}
