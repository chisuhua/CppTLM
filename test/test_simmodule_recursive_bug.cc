// test/test_simmodule_recursive_bug.cc
// D.4 + D.5 修复验证 + 3 层 JSON E2E 测试
// 修复前：findInternalPath 不递归，3 层 JSON exposed port 路径解析失败
// 修复后：递归到子 SimModule
// 作者: Sisyphus / 日期: 2026-06-19
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.1
#include "core/sim_module.hh"
#include "modules_cluster.hh"  // placeholder, P2 后存在
#include "core/module_factory.hh"
#include <catch2/catch_all.hpp>

TEST_CASE("D.4 findInternalPath recurses nested SimModule", "[simmodule][regression]") {
    // 验证 findInternalPath 递归到子 SimModule
    // 3 层 outer → mid → inner
    // outer.outputs: internal="mid.mid_cpu0_to_bus" (跨层引用)
    // mid.outputs:    internal="inner.inner_cpu0_to_bus"
    // inner.outputs:  internal="cpu0.req_out"
    // 期望 outer.findInternalPath("mid_cpu0_to_bus") 返回 "mid.inner.cpu0.req_out"

    EventQueue eq;
    auto* outer = new CpuCluster("outer", &eq);
    auto* mid = new CpuCluster("mid", &eq);
    auto* inner = new CpuCluster("inner", &eq);
    auto* cu0 = new CpuCluster("cu0_proxy", &eq);  // 占位，实际是 CPUTLM
    cu0->setName("cu0");

    // 模拟 build hierarchy
    inner->addInternalInstance(cu0);
    mid->addInternalInstance(inner);
    outer->addInternalInstance(mid);

    // 注册 outputs
    inner->addOutputConfig("cpu0.req_out", "inner_cpu0_to_bus");
    mid->addOutputConfig("inner.inner_cpu0_to_bus", "mid_cpu0_to_bus");
    outer->addOutputConfig("mid.mid_cpu0_to_bus", "outer_cpu0_to_bus");

    REQUIRE(outer->findInternalPath("outer_cpu0_to_bus") == "mid.mid_cpu0_to_bus");
    REQUIRE(outer->findInternalPath("mid_cpu0_to_bus") == "mid.inner.inner_cpu0_to_bus");
    REQUIRE(outer->findInternalPath("inner_cpu0_to_bus") == "mid.inner.cpu0.req_out");

    // 仅 delete outer: outer->internal_factory 销毁时递归 delete mid/inner/cu0
    // (ModuleFactory::~ModuleFactory 迭代 instances 调用 delete)
    delete outer;
}
