// include/chstream_register.hh
// ChStream 模块注册宏：统一注册对象和 StreamAdapter
// 作者 CppTLM Team
// 日期 2026-04-12
#include "tlm/cache_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "modules.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"

#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<MemoryTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("MemoryTLM");

#define REGISTER_ALL \
    REGISTER_OBJECT \
    REGISTER_CHSTREAM