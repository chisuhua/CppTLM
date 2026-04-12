// include/core/chstream_adapter_factory.hh
// ChStreamAdapterFactory 单例工厂：模块类型字符串 → StreamAdapter 创建函数
// 功能描述：支持 A+B 混合模式（类型化注册 + 可选 JSON 配置参数）
// 作者 CppTLM Team
// 日期 2026-04-12
#ifndef CORE_CHSTREAM_ADAPTER_FACTORY_HH
#define CORE_CHSTREAM_ADAPTER_FACTORY_HH

#include "framework/stream_adapter.hh"
#include "core/sim_object.hh"
#include <functional>
#include <unordered_map>
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

    cpptlm::StreamAdapterBase* create(const std::string& type, SimObject* obj, const nlohmann::json* config = nullptr) {
        auto it = table_.find(type);
        return (it != table_.end()) ? it->second(obj, config) : nullptr;
    }

    bool knows(const std::string& type) const {
        return table_.count(type) > 0;
    }

private:
    ChStreamAdapterFactory() = default;
    std::unordered_map<std::string, FactoryFn> table_;
};

#endif // CORE_CHSTREAM_ADAPTER_FACTORY_HH