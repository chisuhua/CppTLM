// include/chstream_register.hh
// ChStream 模块注册宏：统一注册对象和 StreamAdapter + 双端口非对称适配器
// 作者 CppTLM Team
// 日期 2026-04-14
#include "tlm/cache_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "modules.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"

// ============================================================
// 单端口模块注册（CacheTLM / MemoryTLM）
// 多端口模块注册（CrossbarTLM 4 端口）
// ============================================================
#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<MemoryTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("MemoryTLM"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<CrossbarTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("CrossbarTLM");

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