// include/framework/stream_adapter.hh
// StreamAdapter 桥梁：Port ↔ ch_stream 双向适配
// 功能描述：在模块的 ch_stream 端口和框架的 MasterPort/SlavePort 之间做双向翻译
// 作者 CppTLM Team
// 日期 2026-04-12
#ifndef FRAMEWORK_STREAM_ADAPTER_HH
#define FRAMEWORK_STREAM_ADAPTER_HH

#include "core/simple_port.hh"
#include "core/master_port.hh"
#include "core/slave_port.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "bundles/bundle_serialization.hh"
#include <cstdint>
#include <functional>
#include <memory>

namespace cpptlm {

/**
 * @brief StreamAdapter 基类（类型擦除）
 * 
 * 用于在 ModuleFactory 中统一管理不同类型的 StreamAdapter
 */
class StreamAdapterBase {
public:
    virtual ~StreamAdapterBase() = default;
    
    /**
     * @brief 每 tick 调用，处理双向数据流
     */
    virtual void tick() = 0;
    
    /**
     * @brief 绑定框架侧端口（由 ModuleFactory 调用）
     * @param req_out_port 请求输出 MasterPort（连接到下游模块的 req_in）
     * @param resp_in_port  响应输入 SlavePort（从上游模块接收 resp）
     * @param req_in_port   请求输入 SlavePort（从上游模块接收 req，可选）
     * @param resp_out_port 响应输出 MasterPort（连接到下游模块的 resp_in，可选）
     */
    virtual void bind_ports(
        MasterPort* req_out_port,
        SlavePort*  resp_in_port,
        MasterPort* resp_out_port = nullptr,
        SlavePort*  req_in_port = nullptr
    ) = 0;
    
    /**
     * @brief 获取请求方向适配器 tick
     * @param recv_fn 接收回调函数，接收 Packet 并填充 ch_stream
     */
    virtual void process_request_input(Packet* pkt) = 0;
    
    /**
     * @brief 处理响应方向输出
     * @return 待发送的 Packet（若无返回 nullptr）
     */
    virtual Packet* process_response_output() = 0;
};

/**
 * @brief 请求方向适配器：SlavePort 收到 Packet → 反序列化为 Bundle → 设置 ch_stream
 * 
 * 模板参数:
 *   BundleT - Bundle 类型（如 CacheReqBundle）
 */
template<typename BundleT>
class InputStreamAdapter {
private:
    BundleT stream_data_;       // ch_stream 等价物：存放反序列化后的 Bundle 数据
    bool stream_valid_ = false; // 等价于 ch_stream.valid
    bool stream_ready_ = true;  // 等价于 ch_stream.ready

public:
    /**
     * @brief 处理收到的 Packet
     * @param pkt 从 SlavePort 收到的 Packet
     * @return 是否成功处理
     */
    bool process(Packet* pkt) {
        if (!pkt || !pkt->payload) return false;
        
        stream_ready_ = true; // 默认 ready，可被消费者清除
        
        bool ok = bundles::deserialize_bundle(
            pkt->payload->get_data_ptr(),
            pkt->payload->get_data_length(),
            stream_data_
        );
        
        if (ok) {
            stream_valid_ = true;
        }
        return ok;
    }
    
    /**
     * @brief 模块读取 Bundle 数据
     */
    BundleT& data() { return stream_data_; }
    const BundleT& data() const { return stream_data_; }
    
    /**
     * @brief 等价于 ch_stream.valid（模块检查是否有数据可读）
     */
    bool valid() const { return stream_valid_; }
    
    /**
     * @brief 等价于 ch_stream.ready（模块设置 ready 信号）
     */
    bool ready() const { return stream_ready_; }
    void set_ready(bool r) { stream_ready_ = r; }
    void set_valid(bool v) { stream_valid_ = v; }
    void consume() { stream_valid_ = false; }

    void reset() {
        stream_valid_ = false;
        stream_ready_ = true;
    }
};

/**
 * @brief 输出方向适配器：ch_stream → Packet → MasterPort.send
 * 
 * 模板参数:
 *   BundleT - Bundle 类型（如 CacheRespBundle）
 */
template<typename BundleT>
class OutputStreamAdapter {
private:
    BundleT stream_data_;       // ch_stream 等价物：存放待序列化的 Bundle 数据
    bool stream_valid_ = false; // 等价于 ch_stream.valid

public:
    /**
     * @brief 模块写入 Bundle 数据并设置 valid
     */
    void write(const BundleT& data) {
        stream_data_ = data;
        stream_valid_ = true;
    }
    
    /**
     * @brief 等价于 ch_stream.valid
     */
    bool valid() const { return stream_valid_; }
    
    /**
     * @brief 模块清除 valid 信号
     */
    void clear_valid() { stream_valid_ = false; }
    
    /**
     * @brief 将 Bundle 序列化为 Packet 并发送
     * @param port 目标 MasterPort
     * @param type Packet 类型（PKT_REQ 或 PKT_RESP）
     * @return 是否成功发送
     */
    bool send(MasterPort* port, PacketType type = PKT_RESP, uint64_t stream_id = 0) {
        if (!stream_valid_ || !port) return false;
        
        Packet* pkt = PacketPool::get().acquire();
        pkt->type = type;
        pkt->stream_id = stream_id;
        
        bool ok = bundles::serialize_bundle(
            stream_data_,
            pkt->payload->get_data_ptr(),
            pkt->payload->get_data_length()
        );
        
        if (ok) {
            port->send(pkt);
            stream_valid_ = false; // 消费后清除 valid
            return true;
        }
        
        // 序列化失败，释放 Packet
        PacketPool::get().release(pkt);
        return false;
    }
    
    /**
     * @brief 直接访问 Bundle 数据（模块可原地修改）
     */
    BundleT& data() { return stream_data_; }
    const BundleT& data() const { return stream_data_; }
    
    /**
     * @brief 重置状态
     */
    void reset() {
        stream_valid_ = false;
    }
};

/**
 * @brief StreamAdapter 模板实现（绑定模块侧和框架侧）
 * 
 * 模板参数:
 *   ModuleT     - 模块类型（如 CacheTLM），必须提供 req_in() 和 resp_out() 访问器
 *   ReqBundleT  - 请求 Bundle 类型（如 bundles::CacheReqBundle）
 *   RespBundleT - 响应 Bundle 类型（如 bundles::CacheRespBundle）
 * 
 * 职责：
 * - tick() 周期中执行双向数据搬运
 * - 将框架侧 Port 数据桥接到模块侧 InputStreamAdapter/OutputStreamAdapter
 */
template<typename ModuleT, typename ReqBundleT, typename RespBundleT>
class StreamAdapter : public StreamAdapterBase {
private:
    ModuleT* module_;                  // 指向所属模块
    MasterPort* req_out_port_;         // 框架侧：请求输出
    SlavePort*  resp_in_port_;         // 框架侧：响应输入
    MasterPort* resp_out_port_ = nullptr; // 可选：响应输出
    SlavePort*  req_in_port_ = nullptr;   // 可选：请求输入

public:
    explicit StreamAdapter(ModuleT* mod)
        : module_(mod), req_out_port_(nullptr), resp_in_port_(nullptr) {}

    void bind_ports(
        MasterPort* req_out,
        SlavePort*  resp_in,
        MasterPort* resp_out = nullptr,
        SlavePort*  req_in = nullptr
    ) override {
        req_out_port_ = req_out;
        resp_in_port_ = resp_in;
        resp_out_port_ = resp_out;
        req_in_port_ = req_in;
    }

    void tick() override {
        // 输出方向：模块有响应数据 → 序列化 → 通过 MasterPort 发送
        if (module_->resp_out().valid()) {
            MasterPort* out_port = resp_out_port_ ? resp_out_port_ : req_out_port_;
            if (out_port) {
                module_->resp_out().send(out_port, PKT_RESP);
            }
        }
    }

    void process_request_input(Packet* pkt) override {
        if (!pkt || !pkt->payload) return;

        // Packet → Bundle 反序列化
        auto& req_in = module_->req_in();
        if (!req_in.valid()) {
            req_in.process(pkt);
        }
    }

    Packet* process_response_output() override {
        // 响应输出由 tick() 中的 send() 直接处理
        return nullptr;
    }

    ModuleT* module() const { return module_; }
};

} // namespace cpptlm

#endif // FRAMEWORK_STREAM_ADAPTER_HH
