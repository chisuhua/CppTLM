// include/modules.hh
// 注册宏体系入口 (v2.1)
//
// 设计意图:
//   - REGISTER_OBJECT: 注册 SimObject 派生类（含 Legacy + ChStream 模块）
//   - REGISTER_MODULE: 注册 SimModule 派生类
//
// 约束:
//   - registerModule 含 static_assert(std::is_base_of_v<SimModule, T>)
//   - ChStreamModuleBase 继承 SimObject，必须用 registerObject
//   - 双注册表"不对称"是技术约束，不是设计缺陷
//
// 计划: BUILD_LEGACY_MODULES=OFF 时，REGISTER_OBJECT/REGISTER_MODULE
//   应退化为 no-op（详见 wave0-t04 §6.3，本任务仅文档化意图，不改宏体）
//
// 作者: CppTLM Team
// 日期: 2026-06-08

#include "modules/legacy/cpu_sim.hh"
#include "modules/legacy/cpu_cluster.hh"

// REGISTER_CHSTREAM moved to chstream_register.hh

// Legacy modules (CPUSim only remaining) - used by CpuCluster internal configs
#define REGISTER_OBJECT \
    ModuleFactory::registerObject<CPUSim>("CPUSim");

#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");