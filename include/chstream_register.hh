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
#include "tlm/router_tlm.hh"
#include "tlm/nic_tlm.hh"
#include "tlm/link_tlm.hh"
#include "rtl/hybrid_cache_wrapper.hh"
#include "core/module_factory.hh"
#include "framework/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"

// NOTE: Legacy modules (REGISTER_OBJECT) are in modules.hh
// Usage: include modules.hh first, then chstream_register.hh for full registration

// ============================================================
// 单端口模块注册（CacheTLM / MemoryTLM）
// 多端口模块注册（CrossbarTLM 4 端口）
// Initiator 模块注册（CPUTLM / TrafficGenTLM）
// 仲裁器（ArbiterTLM 模板特化）
// 路由器（RouterTLM 5 端口 Bidirectional）
// ============================================================
#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); \
    ModuleFactory::registerObject<CPUTLM>("CPUTLM"); \
    ModuleFactory::registerObject<TrafficGenTLM>("TrafficGenTLM"); \
    ModuleFactory::registerObject<ArbiterTLM<2>>("ArbiterTLM2"); \
    ModuleFactory::registerObject<ArbiterTLM<4>>("ArbiterTLM4"); \
    ModuleFactory::registerObject<tlm::RouterTLM>("RouterTLM"); \
    ModuleFactory::registerObject<tlm::NICTLM>("NICTLM"); \
    ModuleFactory::registerObject<tlm::LinkTLM>("LinkTLM"); \
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
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("ArbiterTLM4"); \
    ChStreamAdapterFactory::get().registerBidirectionalPortAdapter<tlm::RouterTLM, \
        bundles::NoCFlitBundle, 5>("RouterTLM"); \
    ChStreamAdapterFactory::get().registerDualPortAdapter<tlm::NICTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, \
        bundles::NoCFlitBundle, bundles::NoCFlitBundle>("NICTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<tlm::LinkTLM, \
        bundles::NoCFlitBundle, bundles::NoCFlitBundle>("LinkTLM"); \
    /* HybridCacheWrapper 的 .cc 实现位于 src/rtl/，仅在 BUILD_RTL=ON 时编译。
       宏体内不能直接用 ifdef 因续行问题，改用空宏守卫
       HYBRID_CACHE_WRAPPER_REGISTER_RTL：BUILD_RTL=OFF 时展开为空注释，
       调用方无感。BUILD_RTL 通过 target_compile_definitions 传递。 */ \
    HYBRID_CACHE_WRAPPER_REGISTER_RTL

// ============================================================
// RTL 模块注册宏：仅在 BUILD_RTL=ON 时展开为真实注册代码
// 当 BUILD_RTL 未定义或为 0 时，宏展开为 /* no-op */ 注释，调用方无感
// ============================================================
#ifdef BUILD_RTL
#define HYBRID_CACHE_WRAPPER_REGISTER_RTL \
    ModuleFactory::registerObject<cpptlm::rtl::HybridCacheWrapper>("HybridCacheWrapper"); \
    ChStreamAdapterFactory::get().registerAdapter<cpptlm::rtl::HybridCacheWrapper, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("HybridCacheWrapper");
#else
#define HYBRID_CACHE_WRAPPER_REGISTER_RTL /* no-op when BUILD_RTL=OFF */
#endif

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

// REGISTER_ALL = REGISTER_OBJECT; REGISTER_CHSTREAM
// - 当 BUILD_LEGACY_MODULES=OFF: REGISTER_OBJECT 退化为 no-op (空注释)
//   仍产生空语句 (;)，C++ 合法
// - 当 BUILD_LEGACY_MODULES=ON: 正常注册 Legacy + ChStream
#define REGISTER_ALL REGISTER_OBJECT; REGISTER_CHSTREAM