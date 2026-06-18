// include/modules.hh
// 注册宏体系入口 (v2.2)
//
// 设计意图:
//   - REGISTER_OBJECT: 注册 SimObject 派生类（Legacy CPUSim 已退场, ChStreamModuleBase 子树改走 REGISTER_CHSTREAM）
//   - REGISTER_MODULE(T): 注册 SimModule 派生类（参数化, 支持 CpuCluster + 4 GPU SimModule）
//
// 约束:
//   - registerModule 含 static_assert(std::is_base_of_v<SimModule, T>)
//   - ChStreamModuleBase 继承 SimObject, 必须用 registerObject (走 REGISTER_CHSTREAM)
//   - 双注册表"不对称"是技术约束, 不是设计缺陷
//
// v2.2 变更 (json-nested-simmodule Stage 1):
//   - 移除 BUILD_LEGACY_MODULES 编译守卫 (无条件注册)
//   - REGISTER_OBJECT 改为 no-op 注释 (CPUSim 退场, 由 CPUTLM/REGISTER_CHSTREAM 替代)
//   - REGISTER_MODULE 改为参数化宏, 由 modules_cluster.hh 集中注册 5 个 SimModule 派生类
//
// v2.2 变更 (Phase 2 P2-T2.4, 2026-06-19):
//   - REGISTER_MODULE 由无参 (硬编码 CpuCluster) 改为参数化宏
//   - 集中注册点迁至 include/modules_cluster.hh, 自动 include 在末尾
//   - 用户使用 REGISTER_MODULE(MySimModule) 即可注册任何 SimModule 派生类
//
// 作者: CppTLM Team
// 日期: 2026-06-18 (更新 2026-06-19)

#include "tlm/cluster/cpu_cluster.hh"

// v2.2: CPUSim 已由 CPUTLM (include/tlm/cpu_tlm.hh + REGISTER_CHSTREAM) 替代
// 新代码统一使用 REGISTER_CHSTREAM (include/chstream_register.hh)
#define REGISTER_OBJECT  /* no-op: CPUSim removed in v2.2; use REGISTER_CHSTREAM for CPUTLM */

// v2.2 (P2-T2.4): 参数化宏, 支持任意 SimModule 派生类
// 集中注册点见 include/modules_cluster.hh
// 返回 bool 类型 (始终 true), 可在全局作用域作为变量初始化器使用:
//   const bool _ = (REGISTER_MODULE(MyClass), true);
#define REGISTER_MODULE(T) \
    ModuleFactory::registerModule<T>(#T), true

// REGISTER_CHSTREAM moved to chstream_register.hh

// v2.2 (P2-T2.4): 集中注册 5 个 SimModule 派生类 (CpuCluster + 4 GPU 端核心类)
// 用户的 REGISTER_ALL 自动触发此 include, 无需手动引用
// 注意: 不在末尾 #include "modules_cluster.hh" 以避免循环 include
//   (modules_cluster.hh 自身依赖 REGISTER_MODULE 宏)
// 用户使用方式: #include "modules.hh" 后再 #include "modules_cluster.hh"