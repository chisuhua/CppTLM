// include/core/chstream_module.hh
// ChStreamModule 基类：ch_stream 模块的统一类型标识
// 功能描述：提供 ChStreamModuleBase 抽象接口，使 ModuleFactory 能通过 dynamic_cast 识别 ch_stream 模块
// 作者 CppTLM Team / 日期 2026-04-12
#ifndef CORE_CHSTREAM_MODULE_HH
#define CORE_CHSTREAM_MODULE_HH

#include "sim_object.hh"

class StreamAdapterBase;
namespace cpptlm { class StreamAdapterBase; }

/**
 * @brief ChStream 模块基类
 * 
 * 所有使用 ch_stream 内部通信的新模块从此类派生，而非直接使用 SimObject。
 * ModuleFactory 通过 dynamic_cast<ChStreamModuleBase*> 识别需要 StreamAdapter 的模块。
 * 
 * 与 SimObject 的关系：
 *   SimObject — 所有仿真模块基类（包括 Legacy Port 回调模块）
 *   └── ChStreamModuleBase — ch_stream 内部模块额外接口
 *       └── CacheTLM / CrossbarTLM / MemoryTLM / NICTLM
 */
class ChStreamModuleBase : public SimObject {
public:
    ChStreamModuleBase(const std::string& n, EventQueue* eq) 
        : SimObject(n, eq) {}
    
    virtual ~ChStreamModuleBase() = default;

    /**
     * @brief 注入 StreamAdapter 实例
     * 由 ModuleFactory 在 instantiateAll 阶段调用
     * @param adapter 类型擦除的 StreamAdapter 指针
     */
    virtual void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) = 0;
};

#endif // CORE_CHSTREAM_MODULE_HH
