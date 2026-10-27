// include/core/chstream_module.hh
// ChStreamModule 基类：ch_stream 模块的统一类型标识
// 功能描述：提供 ChStreamModuleBase 抽象接口，使 ModuleFactory 能通过 dynamic_cast 识别 ch_stream 模块
// 作者 CppTLM Team / 日期 2026-04-12
#ifndef CORE_CHSTREAM_MODULE_HH
#define CORE_CHSTREAM_MODULE_HH

#include "sim_object.hh"
#include "metrics/stats.hh"

class StreamAdapterBase;
namespace cpptlm { class StreamAdapterBase; }
namespace cpptlm { class ProtocolBridgeEr; }

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
     * @brief 注入 StreamAdapter 实例（单端口模块用）
     * 由 ModuleFactory 在 instantiateAll 阶段调用
     * @param adapter 类型擦除的 StreamAdapter 指针
     */
    virtual void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) = 0;

    /**
     * @brief 注入 StreamAdapter 实例（多端口模块用）
     * 由 ModuleFactory 在 instantiateAll 阶段调用
     * @param adapters StreamAdapter 指针数组（CrossbarTLM 等使用）
     */
    virtual void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        // 默认实现：仅处理第 0 个端口（向后兼容单端口模块）
        if (adapters) {
            set_stream_adapter(adapters[0]);
        }
    }

    /**
     * @brief 返回模块端口数量（默认 1，多端口模块重写）
     */
    virtual unsigned num_ports() const { return 1; }

    /**
     * @brief 返回模块的 StatGroup 指针（供 ModuleFactory 自动注册用）
     * @return StatGroup 指针，无统计则返回 nullptr（默认实现）
     */
    virtual tlm_stats::StatGroup* get_stats_group() { return nullptr; }

    /**
     * @brief 返回模块的层次路径（用于 StatsManager 注册）
     * @return 路径字符串，默认返回模块名
     */
    virtual std::string get_stats_path() const { return getName(); }

    /**
     * @brief 推进一个仿真 cycle (Phase 4 T5, per design.md §6.2 P2-R10)
     * DGpuBoard::tick 集中推进 timing-mode 模块 (TInv-3: 模块不可自行 ++cycle)。
     * 默认空实现 — 仅 timing-mode 模块 (MemoryTLM/VramControllerTLM) 覆盖。
     */
    virtual void advance_cycle() noexcept {}

    /**
     * @brief 挂载协议桥（可选能力点，per `cpptlm-cep-cohif-carrier` Phase 3 / CC-05）
     *
     * 由 ModuleFactory 在装配期（Step 7.5）调用，把 `ProtocolBridgeEr*` 注入
     * 支持桥的模块（当前仅 `tlm::cep::ProtocolEngineBase`）。
     *
     * @param bridge 类型擦除的协议桥指针（**非拥有** —— 生命周期由 ModuleFactory 负责）
     * @return true = 已挂载；false = **本模块不支持桥挂载**（默认实现）
     *
     * ⚠️ **默认实现返回 false ⇒ 零波及**：所有既有 ChStream 派生类无需改动。
     * ⚠️ **为什么不用 `dynamic_cast` 到独立 mixin**：`ProtocolBridgeHolder` 与
     *    `SimObject` 无继承关系 ⇒ 该 cast 编译不过；让 mixin 继承 `SimObject`
     *    则引入菱形继承 ⇒ 更糟。详见 `include/framework/protocol_bridge.hh` 末段说明。
     */
    virtual bool attach_protocol_bridge(cpptlm::ProtocolBridgeEr* /*bridge*/) { return false; }
};

#endif // CORE_CHSTREAM_MODULE_HH
