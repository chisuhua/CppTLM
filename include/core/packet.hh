// packet.hh
// CppTLM v2.0 - Packet 类（扩展 transaction_id 和 error_code）
#ifndef PACKET_HH
#define PACKET_HH

#include "tlm.h"
#include "core/error_category.hh"
#include "ext/transaction_context_ext.hh"
#include <cstdint>
#include <unordered_map>

class PacketPool;

enum CmdType {
    CMD_INVALID = 0,
    CMD_READ    = 1,
    CMD_WRITE   = 2
};

enum PacketType {
    PKT_REQ,
    PKT_RESP,
    PKT_STREAM_DATA,
    PKT_CREDIT_RETURN
};

/**
 * @brief Packet - TLM 交易承载类
 * 
 * 扩展功能 (v2.0):
 * - transaction_id: 交易追踪 ID（与 TransactionContextExt 同步）
 * - error_code: 错误码（与 ErrorContextExt 同步）
 */
class Packet {
    friend class PacketPool;
public:
    tlm::tlm_generic_payload* payload;
    
    // ========== 流控相关 ==========
    uint64_t stream_id = 0;
    uint64_t seq_num = 0;
    CmdType cmd;
    PacketType type;

    // ========== 时间统计 ==========
    uint64_t src_cycle;
    uint64_t dst_cycle;

    // ========== 依赖关系 ==========
    Packet* original_req = nullptr;
    std::vector<Packet*> dependents;

    // ========== 路由信息 ==========
    std::vector<std::string> route_path;
    int hop_count = 0;
    uint8_t priority = 0;
    uint64_t flow_id = 0;
    int vc_id = 0;

    // ========== 基本判断方法 ==========
    bool isRequest() const { return type == PKT_REQ; }
    bool isResponse() const { return type == PKT_RESP; }
    bool isStream() const { return type == PKT_STREAM_DATA; }
    bool isCredit() const { return type == PKT_CREDIT_RETURN; }

    uint64_t getDelayCycles() const {
        return (dst_cycle >= src_cycle) ? (dst_cycle - src_cycle) : 0;
    }

    uint64_t getEnd2EndCycles() const {
        return original_req ? (dst_cycle - original_req->src_cycle) : getDelayCycles();
    }

    // ========== Transaction ID 方法（v2.0 新增） ==========
    
    /**
     * @brief 获取交易 ID（从 TransactionContextExt 或 stream_id）
     * @return 交易 ID
     */
    uint64_t get_transaction_id() const {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) return ext->transaction_id;
        }
        return stream_id;  // 回退到 stream_id
    }
    
    /**
     * @brief 设置交易 ID（同步更新 stream_id 和 TransactionContextExt）
     * @param tid 交易 ID
     */
    void set_transaction_id(uint64_t tid) {
        stream_id = tid;  // 同步更新 stream_id
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) {
                ext->transaction_id = tid;
            } else {
                // 自动创建 Extension
                create_transaction_context(payload, tid);
            }
        }
    }
    
    /**
     * @brief 获取父交易 ID
     * @return 父交易 ID（0 表示根交易）
     */
    uint64_t get_parent_id() const {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) return ext->parent_id;
        }
        return 0;
    }
    
    /**
     * @brief 设置分片信息
     * @param frag_id 分片 ID
     * @param frag_total 分片总数
     */
    void set_fragment_info(uint8_t frag_id, uint8_t frag_total) {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) {
                ext->fragment_id = frag_id;
                ext->fragment_total = frag_total;
            }
        }
    }
    
    /**
     * @brief 检查是否为分片包
     * @return true 如果是分片
     */
    bool is_fragmented() const {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) return ext->is_fragmented();
        }
        return false;
    }
    
    /**
     * @brief 获取分组键（用于分片重组）
     * @return 分组键
     */
    uint64_t get_group_key() const {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) return ext->get_group_key();
        }
        return stream_id;
    }
    
    /**
     * @brief 添加追踪日志
     * @param module 模块名称
     * @param timestamp 时间戳
     * @param latency 延迟
     * @param event 事件类型
     */
    void add_trace(const std::string& module, uint64_t timestamp, uint64_t latency, const std::string& event) {
        if (payload) {
            TransactionContextExt* ext = nullptr;
            payload->get_extension(ext);
            if (ext) ext->add_trace(module, timestamp, latency, event);
        }
    }

    // ========== Error Code 方法（v2.0 新增） ==========
    
    /**
     * @brief 获取错误码
     * @return 错误码
     */
    ErrorCode get_error_code() const {
        // 暂未实现 ErrorContextExt，返回 SUCCESS
        return ErrorCode::SUCCESS;
    }
    
    /**
     * @brief 设置错误码
     * @param code 错误码
     */
    void set_error_code(ErrorCode code) {
        // 暂未实现 ErrorContextExt，仅记录到 stream_id 高位
        // 后续在 Phase 4 实现完整的 ErrorContextExt
        (void)code;
    }
    
    /**
     * @brief 检查是否有错误
     * @return true 如果有错误
     */
    bool has_error() const {
        return get_error_code() != ErrorCode::SUCCESS;
    }

private:
    // 引用计数
    int ref_count = 0;

    // reset 方法
    void reset() {
        if (payload && !isCredit()) {
            delete payload;
        }
        payload = nullptr;
        stream_id = 0;
        seq_num = 0;
        cmd = CMD_INVALID;
        type = PKT_REQ;
        src_cycle = 0;
        dst_cycle = 0;
        original_req = nullptr;
        dependents.clear();
        route_path.clear();
        hop_count = 0;
        priority = 0;
        flow_id = 0;
        vc_id = 0;
        ref_count = 0;
    }

    // 私有构造函数
    Packet(tlm::tlm_generic_payload* p, uint64_t cycle, PacketType t)
        : payload(p), src_cycle(cycle), type(t), ref_count(0) {}

    ~Packet() {
        // 析构函数不负责删除 payload
    }

    friend class PacketPool;
};

#endif // PACKET_HH
