// include/framework/axi4_mapper.hh
// Axi4Mapper: AXI4 ↔ Bundle Mapper（独立模块，outstanding 跟踪 + OOO completion）
// 功能描述：在 Phase 5 Axi4Bundle 之上提供 outstanding 事务跟踪与 out-of-order
//           completion 调度。通过 rid 将乱序 rdata 关联回原事务（design.md §6.4）。
//           读写独立 ID 空间（awid/arid），容量上限 N，N+1 拒绝新发出。
//           独立于 PcieEndpointIP，可被 CrossbarTLM / CacheTLM 复用。
// 作者 CppTLM Team / 日期 2026-12-22
// 参考: openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/spec.md
//       openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §6.4
#ifndef FRAMEWORK_AXI4_MAPPER_HH
#define FRAMEWORK_AXI4_MAPPER_HH

#include "bundles/axi4_bundles_tlm.hh"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace cpptlm {

/**
 * @brief AXI4 ↔ Bundle Mapper（独立模块）
 *
 * 职责：
 *   - outstanding 跟踪：读写独立 ID 空间（awid / arid 各自计数）
 *   - 容量上限 N：outstanding 计数达到 N 时，第 N+1 个请求拒绝（N+1 拒绝，
 *     与 Q12 CompletionTracker 语义一致）
 *   - OOO completion：AXI4 通过 rid 把乱序 rdata 关联回原事务（核心机制），
 *     乱序完成不影响其他 outstanding（不匹配即不消耗）
 *   - 完成一个后释放槽位，可再注册
 *
 * 设计原则：
 *   - 独立于 PcieEndpointIP，无硬编码耦合，可被 CrossbarTLM / CacheTLM 复用
 *   - 只做跟踪/调度，不内嵌 AXI 通道数据路径（数据路径经 Phase 5 Axi4StreamAdapter）
 */
class Axi4Mapper {
public:
    /**
     * @brief 构造 Axi4Mapper
     * @param capacity outstanding 容量上限（读写各 N）
     */
    explicit Axi4Mapper(std::size_t capacity = 16);
    Axi4Mapper(const Axi4Mapper&) = delete;
    Axi4Mapper& operator=(const Axi4Mapper&) = delete;

    // ===================== 发出发出 =====================
    /**
     * @brief 登记一个写请求 outstanding（按 awid 计数）
     * @param req 写请求 Bundle（使用 awid/awaddr）
     * @return true 接受；false 表示容量满（N+1 拒绝）
     */
    bool issue_write(const bundles::Axi4Bundle& req);

    /**
     * @brief 登记一个读请求 outstanding（按 arid 计数，存储原事务供 OOO 关联）
     * @param req 读请求 Bundle（使用 arid/araddr）
     * @return true 接受；false 表示容量满（N+1 拒绝）
     */
    bool issue_read(const bundles::Axi4Bundle& req);

    // 容量查询：是否还可发出写/读
    bool can_issue_write() const;
    bool can_issue_read() const;

    // ===================== 计数 =====================
    std::size_t outstanding_wr() const;  // outstanding 写请求数
    std::size_t outstanding_rd() const;  // outstanding 读请求数
    std::size_t capacity() const;        // 容量上限

    // ===================== 完成 =====================
    /**
     * @brief 完成一个写事务（按 bid 释放 awid 槽位）
     * @param bid 写响应 ID（与 awid 关联）
     * @return true 匹配到 outstanding 写请求并释放；false 不匹配（不消耗）
     */
    bool complete_write(uint16_t bid);

    /**
     * @brief 完成一个读数据返回（按 rid 关联 rdata 回原事务）
     * @param rid   读响应 ID（与 arid 关联）
     * @param rdata 本次返回的读数据（写入关联事务）
     * @param rlast 是否末拍（rlast=true 时释放 outstanding 槽位）
     * @return true 匹配到 outstanding 读请求；false 不匹配（不消耗）
     */
    bool complete_read(uint16_t rid, uint64_t rdata = 0, bool rlast = true);

    // ===================== OOO 查询 =====================
    // 该 rid 是否已有乱序返回的 rdata 被关联（T-P6-3 测试断言）
    bool has_read_data(uint16_t rid) const;
    // 查询该 rid 已关联的 rdata
    uint64_t read_data(uint16_t rid) const;
    // 按 arid 查询原读事务（araddr 等）；非 outstanding 返回 nullptr
    const bundles::Axi4Bundle* pending_read(uint16_t arid) const;

    // 全量复位（清空所有 outstanding 与已关联数据）
    void reset();

private:
    std::size_t capacity_;
    // 写 outstanding ID（bid 匹配 awid 释放）
    std::deque<uint16_t> outstanding_wr_ids_;
    // 读 outstanding ID（rid 匹配 arid 释放）
    std::deque<uint16_t> outstanding_rd_ids_;
    // rid → 原读事务（OOO 关联：rdata 写回原事务，araddr 查询）
    std::unordered_map<uint16_t, bundles::Axi4Bundle> pending_reads_;
    // rid → 已关联的 rdata（OOO 完成数据暂存）
    std::unordered_map<uint16_t, uint64_t> read_data_;
};

} // namespace cpptlm

#endif // FRAMEWORK_AXI4_MAPPER_HH
