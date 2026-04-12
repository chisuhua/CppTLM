// include/chstream_register.hh
// ChStream 模块注册宏：统一注册对象和 StreamAdapter
// 作者 CppTLM Team
// 日期 2026-04-13
#include "tlm/cache_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "modules.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"

#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
/*  ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); */  /* Phase 4 TODO */ \
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<MemoryTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("MemoryTLM");
/*  ChStreamAdapterFactory::get().registerAdapter<CrossbarTLM, */ \
/*      bundles::CacheReqBundle, bundles::CacheRespBundle>("CrossbarTLM"); */

#define REGISTER_ALL \
    REGISTER_OBJECT \
    REGISTER_CHSTREAM