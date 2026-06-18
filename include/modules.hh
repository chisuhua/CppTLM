// include/modules.hh
// 注册宏体系入口 (v2.2)
//
// 设计意图:
//   - REGISTER_OBJECT: 注册 SimObject 派生类（Legacy CPUSim 已退场, ChStreamModuleBase 子树改走 REGISTER_CHSTREAM）
//   - REGISTER_MODULE: 注册 SimModule 派生类（CpuCluster, 无条件注册）
//
// 约束:
//   - registerModule 含 static_assert(std::is_base_of_v<SimModule, T>)
//   - ChStreamModuleBase 继承 SimObject, 必须用 registerObject (走 REGISTER_CHSTREAM)
//   - 双注册表"不对称"是技术约束, 不是设计缺陷
//
// v2.2 变更 (json-nested-simmodule Stage 1):
//   - 移除 BUILD_LEGACY_MODULES 编译守卫 (无条件注册)
//   - REGISTER_OBJECT 改为 no-op 注释 (CPUSim 退场, 由 CPUTLM/REGISTER_CHSTREAM 替代)
//   - REGISTER_MODULE 无条件注册 CpuCluster (新位置 include/tlm/cluster/cpu_cluster.hh)
//
// 作者: CppTLM Team
// 日期: 2026-06-18

#include "tlm/cluster/cpu_cluster.hh"

// v2.2: CPUSim 已由 CPUTLM (include/tlm/cpu_tlm.hh + REGISTER_CHSTREAM) 替代
// 新代码统一使用 REGISTER_CHSTREAM (include/chstream_register.hh)
#define REGISTER_OBJECT  /* no-op: CPUSim removed in v2.2; use REGISTER_CHSTREAM for CPUTLM */

// v2.2: CpuCluster 从 include/modules/legacy/ 移至 include/tlm/cluster/, 无条件注册
#define REGISTER_MODULE \
    ModuleFactory::registerModule<CpuCluster>("CpuCluster");

// REGISTER_CHSTREAM moved to chstream_register.hh