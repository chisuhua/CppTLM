// include/core/chstream_adapter_factory.hh
// ChStreamAdapterFactory 单例工厂：模块类型字符串 → StreamAdapter 创建函数
// 功能描述：支持 A+B 混合模式（类型化注册 + 可选 JSON 配置参数）
// 作者 CppTLM Team
// 日期 2026-04-13
#ifndef CORE_CHSTREAM_ADAPTER_FACTORY_HH
#define CORE_CHSTREAM_ADAPTER_FACTORY_HH

#include "framework/stream_adapter.hh"
#include "framework/multi_port_stream_adapter.hh"
#include "framework/dual_port_stream_adapter.hh"
#include "framework/bidirectional_port_adapter.hh"
#include "core/sim_object.hh"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <nlohmann/json.hpp>

class ChStreamAdapterFactory {
public:
    using FactoryFn = std::function<cpptlm::StreamAdapterBase*(SimObject*, const nlohmann::json*)>;

    static ChStreamAdapterFactory& get() {
        static ChStreamAdapterFactory instance;
        return instance;
    }

    void registerFactory(const std::string& type, FactoryFn fn) {
        table_[type] = std::move(fn);
    }

    template<typename ModuleT, typename ReqBundleT, typename RespBundleT>
    void registerAdapter(const std::string& type) {
        table_[type] = [](SimObject* obj, const nlohmann::json*) {
            auto* mod = static_cast<ModuleT*>(obj);
            return new cpptlm::StreamAdapter<ModuleT, ReqBundleT, RespBundleT>(mod);
        };
    }

    template<typename ModuleT, typename ReqBundleT, typename RespBundleT, std::size_t N>
    void registerMultiPortAdapter(const std::string& type) {
        table_[type] = [](SimObject* obj, const nlohmann::json*) {
            auto* mod = static_cast<ModuleT*>(obj);
            return new cpptlm::MultiPortStreamAdapter<ModuleT, ReqBundleT, RespBundleT, N>(mod);
        };
        port_count_[type] = N;
    }

    cpptlm::StreamAdapterBase* create(const std::string& type, SimObject* obj, const nlohmann::json* config = nullptr) {
        auto it = table_.find(type);
        return (it != table_.end()) ? it->second(obj, config) : nullptr;
    }

    bool knows(const std::string& type) const {
        return table_.count(type) > 0;
    }

    bool isMultiPort(const std::string& type) const {
        return port_count_.count(type) > 0 && port_count_.at(type) > 1;
    }

    bool isDualPort(const std::string& type) const {
        return dual_port_types_.count(type) > 0;
    }

    unsigned getPortCount(const std::string& type) const {
        auto it = port_count_.find(type);
        return it != port_count_.end() ? it->second : 1;
    }

    /**
     * @brief 注册双端口非对称适配器
     *
     * @param type 模块类型字符串（如 "NICTLM"）
     *
     * 双端口模块有 2 组独立端口（PE 侧 + Network 侧），
     * 每组使用不同的 Bundle 类型。ModuleFactory 在 Step 7
     * 会为这类模块创建 2 组 ChStreamPort。
     */
    template<typename ModuleT,
             typename PE_ReqBundleT, typename PE_RespBundleT,
             typename Net_ReqBundleT, typename Net_RespBundleT>
    void registerDualPortAdapter(const std::string& type) {
        table_[type] = [](SimObject* obj, const nlohmann::json*) {
            auto* mod = static_cast<ModuleT*>(obj);
            return new cpptlm::DualPortStreamAdapter<ModuleT, PE_ReqBundleT, PE_RespBundleT,
                                                   Net_ReqBundleT, Net_RespBundleT>(mod);
        };
        dual_port_types_.insert(type);
        port_count_[type] = 2;
    }

    /**
     * @brief 注册双向端口适配器（RouterTLM 等使用）
     *
     * @param type 模块类型字符串（如 "RouterTLM"）
     * @param N 端口数量
     *
     * 双向端口模块有 N 个端口，每端口同时支持 req_in 和 resp_out。
     */
    template<typename ModuleT, typename BundleT, std::size_t N>
    void registerBidirectionalPortAdapter(const std::string& type) {
        table_[type] = [](SimObject* obj, const nlohmann::json*) {
            auto* mod = static_cast<ModuleT*>(obj);
            return new cpptlm::BidirectionalPortAdapter<ModuleT, BundleT, N>(mod);
        };
        port_count_[type] = N;
    }

private:
    ChStreamAdapterFactory() = default;
    std::unordered_map<std::string, FactoryFn> table_;
    std::unordered_map<std::string, unsigned> port_count_;
    std::unordered_set<std::string> dual_port_types_;  // 双端口非对称类型集合
};

#endif // CORE_CHSTREAM_ADAPTER_FACTORY_HH