// include/chstream_register.hh
// ChStream 模块注册宏：统一注册对象和 StreamAdapter + 双端口非对称适配器
// 作者 CppTLM Team
// 日期 2026-04-14
#include "tlm/cache_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "tlm/cpu_tlm.hh"
#include "tlm/traffic_gen_tlm.hh"
#include "tlm/arbiter_tlm.hh"
#include "core/module_factory.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"

// NOTE: Legacy modules (REGISTER_OBJECT) are in modules.hh
// Usage: include modules.hh first, then chstream_register.hh for full registration

// ============================================================
// 单端口模块注册（CacheTLM / MemoryTLM）
// 多端口模块注册（CrossbarTLM 4 端口）
// Initiator 模块注册（CPUTLM / TrafficGenTLM）
// 仲裁器（ArbiterTLM 模板特化）
// ============================================================
#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); \
    ModuleFactory::registerObject<CPUTLM>("CPUTLM"); \
    ModuleFactory::registerObject<TrafficGenTLM>("TrafficGenTLM"); \
    ModuleFactory::registerObject<ArbiterTLM<2>>("ArbiterTLM2"); \
    ModuleFactory::registerObject<ArbiterTLM<4>>("ArbiterTLM4"); \
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<MemoryTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("MemoryTLM"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<CrossbarTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("CrossbarTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<CPUTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CPUTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<TrafficGenTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("TrafficGenTLM"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<ArbiterTLM<2>, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 2>("ArbiterTLM2"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<ArbiterTLM<4>, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("ArbiterTLM4");

// ============================================================
// 双端口非对称模块注册宏（NICTLM 等）
//
// 用法: REGISTER_CHSTREAM_DUAL(NICTLM, \
//     pe_req_bundle, pe_resp_bundle, \
//     net_req_bundle, net_resp_bundle)
// ============================================================
#define REGISTER_CHSTREAM_DUAL(mod_type, pe_req_t, pe_resp_t, net_req_t, net_resp_t) \
    ChStreamAdapterFactory::get().registerDualPortAdapter<mod_type, \
        pe_req_t, pe_resp_t, net_req_t, net_resp_t>(#mod_type);

#define REGISTER_ALL \
    REGISTER_OBJECT \
    REGISTER_CHSTREAM