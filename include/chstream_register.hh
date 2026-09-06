// include/chstream_register.hh
// ChStream 模块注册宏：统一注册对象和 StreamAdapter + 双端口非对称适配器
// 作者 CppTLM Team
// 日期 2026-04-14 (更新 2026-06-19: 末尾 include modules_cluster.hh)
#include "modules.hh" // P2-T2.4: 提供 REGISTER_MODULE 宏 (参数化)
#include "tlm/arbiter_tlm.hh"
#include "tlm/cache_tlm.hh"
#include "tlm/coherent_xbar_tlm.hh"
#include "tlm/cpu_tlm.hh"
#include "tlm/crossbar_tlm.hh"
#include "tlm/gpu/gpu_mesh_noc_tlm.hh"
#include "tlm/gpu/gpu_tlm.hh"
#include "tlm/gpu/memory_cluster_tlm.hh"
#include "tlm/gpu/shared_memory_tlm.hh"
#include "tlm/link_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "tlm/nic_tlm.hh"
#include "tlm/router_tlm.hh"
#include "tlm/traffic_gen_tlm.hh"
// Task 12: kernel_launch_tlm.hh 已物理删除
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"
#include "tlm/gpu/vector_regfile_tlm.hh"
#include "tlm/gpu/wavefront_tlm.hh"
// Task 9: 删除 gpu_compute_unit_tlm.hh include (rename -> StreamingMultiprocessorTLM)
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/compute_bundles_tlm.hh"
#include "core/module_factory.hh"
#include "framework/chstream_adapter_factory.hh"
#include "rtl/hybrid_cache_wrapper.hh"
#include "tlm/gpu/command_processor_mvp.hh" // CommandProcessorTLM
#include "tlm/gpu/completion_ring_mvp.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"
#include "tlm/gpu/sdma_engine_tlm.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh" // SM 重构 Task 4: SM 顶层 + 12 子模块
#include "tlm/gpu/submit_queue_mvp.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

// NOTE: Legacy modules (REGISTER_OBJECT) are in modules.hh
// Usage: include modules.hh first, then chstream_register.hh for full registration

// ============================================================
// 单端口模块注册（CacheTLM / MemoryTLM）
// 多端口模块注册（CrossbarTLM 4 端口）
// Initiator 模块注册（CPUTLM / TrafficGenTLM）
// 仲裁器（ArbiterTLM 模板特化）
// 路由器（RouterTLM 5 端口 Bidirectional）
// ============================================================
#define REGISTER_CHSTREAM                                                                           \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM");                                            \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM");                                          \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM");                                      \
    ModuleFactory::registerObject<cpptlm::tlm::CoherentXBarTLM>("CoherentXBarTLM");                 \
    ModuleFactory::registerObject<CPUTLM>("CPUTLM");                                                \
    ModuleFactory::registerObject<TrafficGenTLM>("TrafficGenTLM");                                  \
    ModuleFactory::registerObject<ArbiterTLM<2>>("ArbiterTLM2");                                    \
    ModuleFactory::registerObject<ArbiterTLM<4>>("ArbiterTLM4");                                    \
    ModuleFactory::registerObject<tlm::RouterTLM>("RouterTLM");                                     \
    ModuleFactory::registerObject<tlm::NICTLM>("NICTLM");                                           \
    ModuleFactory::registerObject<tlm::LinkTLM>("LinkTLM");                                         \
    ModuleFactory::registerObject<tlm::GPUTLM>("GPUTLM");                                           \
    ModuleFactory::registerObject<tlm::SharedMemoryTLM>("SharedMemoryTLM");                         \
    ModuleFactory::registerObject<tlm::MemoryClusterTLM>("MemoryClusterTLM");                       \
    ModuleFactory::registerObject<tlm::GpuMeshNoC>("GpuMeshNoC");                                   \
    /* Task 9: GpuComputeUnitTLM 删除, 由 StreamingMultiprocessorTLM 接管.                          \
       Task 10: WavefrontTLM/MinimalWarpSchedulerTLM/VectorRegFileTLM -> SM 内部子模块 (类标 \
       [[deprecated]], chstream_register 注销, Task 16 删除文件). Task 12: KernelLaunchTLM +                                                                         \
       CudaCoreAdapterMVP + PtxEmuSubmoduleMVP 已物理删除. */                                  \
    ModuleFactory::registerObject<tlm::StreamingMultiprocessorTLM>("StreamingMultiprocessorTLM");   \
    ModuleFactory::registerObject<tlm::sm::FetchUnitTLM>("FetchUnitTLM");                           \
    ModuleFactory::registerObject<tlm::sm::DecodeUnitTLM>("DecodeUnitTLM");                         \
    ModuleFactory::registerObject<tlm::sm::IssueUnitTLM>("IssueUnitTLM");                           \
    ModuleFactory::registerObject<tlm::sm::ScalarALU>("ScalarALU");                                 \
    ModuleFactory::registerObject<tlm::sm::VectorALU>("VectorALU");                                 \
    ModuleFactory::registerObject<tlm::sm::MatrixCore>("MatrixCore");                               \
    ModuleFactory::registerObject<tlm::sm::SIMTLane>("SIMTLane");                                   \
    ModuleFactory::registerObject<tlm::sm::LsuGlobal>("LsuGlobal");                                 \
    ModuleFactory::registerObject<tlm::sm::LsuLDS>("LsuLDS");                                       \
    ModuleFactory::registerObject<tlm::sm::RegFileUnit>("RegFileUnit");                             \
    ModuleFactory::registerObject<tlm::sm::WritebackUnit>("WritebackUnit");                         \
    ModuleFactory::registerObject<tlm::sm::HazardTracker>("HazardTracker");                         \
    ModuleFactory::registerObject<tlm::gpu::PcieEndpointTLM>("PcieEndpointTLM");                    \
    ModuleFactory::registerObject<tlm::pcie::PcieEndpointIP>("PcieEndpointIP");                     \
    ModuleFactory::registerObject<tlm::gpu::SdmaEngineTLM>("SdmaEngineTLM");                        \
    ModuleFactory::registerObject<tlm::gpu::SubmitQueueTLM>("SubmitQueueTLM");                      \
    ModuleFactory::registerObject<tlm::gpu::CompletionRingTLM>("CompletionRingTLM");                \
    ModuleFactory::registerObject<tlm::gpu::CommandProcessorTLM>("CommandProcessorTLM");            \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<CacheTLM, bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM");  \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<MemoryTLM, bundles::CacheReqBundle, bundles::CacheRespBundle>(             \
            "MemoryTLM");                                                                           \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<CrossbarTLM, bundles::CacheReqBundle, bundles::CacheRespBundle,   \
                                  4>("CrossbarTLM");                                                \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<cpptlm::tlm::CoherentXBarTLM, bundles::CacheReqBundle,            \
                                  bundles::CacheRespBundle, 4>("CoherentXBarTLM");                  \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<CPUTLM, bundles::CacheReqBundle, bundles::CacheRespBundle>("CPUTLM");      \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<TrafficGenTLM, bundles::CacheReqBundle, bundles::CacheRespBundle>(         \
            "TrafficGenTLM");                                                                       \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<ArbiterTLM<2>, bundles::CacheReqBundle,                           \
                                  bundles::CacheRespBundle, 2>("ArbiterTLM2");                      \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<ArbiterTLM<4>, bundles::CacheReqBundle,                           \
                                  bundles::CacheRespBundle, 4>("ArbiterTLM4");                      \
    ChStreamAdapterFactory::get()                                                                   \
        .registerBidirectionalPortAdapter<tlm::RouterTLM, bundles::NoCFlitBundle, 5>("RouterTLM");  \
    ChStreamAdapterFactory::get()                                                                   \
        .registerDualPortAdapter<tlm::NICTLM, bundles::CacheReqBundle, bundles::CacheRespBundle,    \
                                 bundles::NoCFlitBundle, bundles::NoCFlitBundle>("NICTLM");         \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<tlm::LinkTLM, bundles::NoCFlitBundle, bundles::NoCFlitBundle>("LinkTLM");  \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<tlm::GPUTLM, bundles::ComputeReqBundle, bundles::ComputeRespBundle>(       \
            "GPUTLM");                                                                              \
    ChStreamAdapterFactory::get()                                                                   \
        .registerAdapter<tlm::StreamingMultiprocessorTLM, bundles::ComputeReqBundle,                \
                         bundles::ComputeRespBundle>("StreamingMultiprocessorTLM");                 \
    /* HybridCacheWrapper 的 .cc 实现位于 src/rtl/，仅在 BUILD_RTL=ON 时编译。                      \
       宏体内不能直接用 ifdef 因续行问题，改用空宏守卫                          \
       HYBRID_CACHE_WRAPPER_REGISTER_RTL：BUILD_RTL=OFF 时展开为空注释，                   \
       调用方无感。BUILD_RTL 通过 target_compile_definitions 传递。 */                   \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<tlm::gpu::PcieEndpointTLM, bundles::PcieTlpBundle,                \
                                  bundles::PcieTlpBundle, 4>("PcieEndpointTLM");                    \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<tlm::pcie::PcieEndpointIP, bundles::PcieTlpBundle,                \
                                  bundles::PcieTlpBundle, 17>("PcieEndpointIP");                    \
    ChStreamAdapterFactory::get()                                                                   \
        .registerMultiPortAdapter<tlm::gpu::SdmaEngineTLM, bundles::PcieTlpBundle,                  \
                                  bundles::PcieTlpBundle, 5>("SdmaEngineTLM");                      \
    HYBRID_CACHE_WRAPPER_REGISTER_RTL

// ============================================================
// RTL 模块注册宏：仅在 BUILD_RTL=ON 时展开为真实注册代码
// 当 BUILD_RTL 未定义或为 0 时，宏展开为 /* no-op */ 注释，调用方无感
// ============================================================
#ifdef BUILD_RTL
#define HYBRID_CACHE_WRAPPER_REGISTER_RTL                                                          \
    ModuleFactory::registerObject<cpptlm::rtl::HybridCacheWrapper>("HybridCacheWrapper");          \
    ChStreamAdapterFactory::get()                                                                  \
        .registerAdapter<cpptlm::rtl::HybridCacheWrapper, bundles::CacheReqBundle,                 \
                         bundles::CacheRespBundle>("HybridCacheWrapper");
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
#define REGISTER_CHSTREAM_DUAL(mod_type, pe_req_t, pe_resp_t, net_req_t, net_resp_t)               \
    ChStreamAdapterFactory::get()                                                                  \
        .registerDualPortAdapter<mod_type, pe_req_t, pe_resp_t, net_req_t, net_resp_t>(#mod_type);

// REGISTER_ALL = REGISTER_OBJECT; REGISTER_CHSTREAM
// - 当 BUILD_LEGACY_MODULES=OFF: REGISTER_OBJECT 退化为 no-op (空注释)
//   仍产生空语句 (;)，C++ 合法
// - 当 BUILD_LEGACY_MODULES=ON: 正常注册 Legacy + ChStream
// P2-T2.4 (2026-06-19): 不包含 REGISTER_MODULE (避免宏与模块注册耦合)
//   用户需显式 #include "modules_cluster.hh" (或在 REGISTER_ALL 之后) 触发 5 个 SimModule 集中注册
#define REGISTER_ALL                                                                               \
    REGISTER_OBJECT;                                                                               \
    REGISTER_CHSTREAM

// P2-T2.4: 集中注册 5 个 SimModule 派生类 (CpuCluster + ComputeCluster + TpcCluster + GpcCluster +
// GpuCluster) 依赖关系: modules.hh 必须在 modules_cluster.hh 之前被 include (提供 REGISTER_MODULE
// 宏) 此处 include 在 REGISTER_ALL 之后, 避免 REGISTER_CHSTREAM 注册的 inline 函数依赖未实例化模块
#include "modules_cluster.hh"