// include/modules.hh
// 注册宏体系入口 (v2.1)
//
// 设计意图:
//   - REGISTER_OBJECT: 注册 SimObject 派生类（包含 Legacy CPUSim 与 ChStreamModuleBase 子树）
//   - REGISTER_MODULE: 注册 SimModule 派生类（CpuCluster）
//
// 约束:
//   - registerModule 含 static_assert(std::is_base_of_v<SimModule, T>)
//   - ChStreamModuleBase 继承 SimObject，必须用 registerObject
//   - 双注册表"不对称"是技术约束，不是设计缺陷
//
// 构建守卫（v2.1）:
//   - BUILD_LEGACY_MODULES=ON  → 展开为 CPUSim/CpuCluster 注册代码
//   - BUILD_LEGACY_MODULES=OFF → 退化为 no-op（默认，推荐）
//   - ChStream 模块始终通过 REGISTER_CHSTREAM 注册，不受此开关影响
//
// 迁移路径:
//   - 新代码统一使用 REGISTER_CHSTREAM（include/chstream_register.hh）
//   - Legacy 模块计划在 v2.2+ 删除 include/modules/legacy/ 归档目录
//
// 作者: CppTLM Team
// 日期: 2026-06-08

#ifdef BUILD_LEGACY_MODULES
#include "modules/legacy/cpu_sim.hh"
#include "modules/legacy/cpu_cluster.hh"

// Legacy modules (CPUSim only remaining) - used by CpuCluster internal configs
#define REGISTER_OBJECT \
    ModuleFactory::registerObject<CPUSim>("CPUSim");

#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");
#else
#define REGISTER_OBJECT  /* no-op: legacy modules disabled */
#define REGISTER_MODULE  /* no-op: legacy modules disabled */
#endif

// REGISTER_CHSTREAM moved to chstream_register.hh
