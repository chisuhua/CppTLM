// include/tlm/gpu/pcie_bar_router_mvp.hh
// PcieBarRouter: BAR0 MMIO 寄存器路由表（数据化）+ 门铃 strong-order 副作用
// 功能描述：参数化 BAR0 寄存器定义（JSON 声明）；门铃条目 side_effect 触发
//           250-700ns 区间延迟的 mmio_out 事务（沿用 s2 Doorbell 强序语义）
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §3,§5
//       ADR-SOC-07 D2 (no if-else 硬编码)
#ifndef CPPTLM_PCIE_BAR_ROUTER_MVP_H
#define CPPTLM_PCIE_BAR_ROUTER_MVP_H

#include "bundles/pcie_bundles_tlm.hh"
#include "tlm/gpu/doorbell_mvp.hh"
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief BAR Router（MVP）：数据化 BAR0 寄存器路由表
     *
     * 设计原则（per design.md §3 + spec.md Scenario "Doorbell register write is table-driven"）：
     *   - 寄存器定义数据化：JSON 声明 `{offset, name, access, side_effect}`，
     *     禁止 C++ `if (offset == 0x0014)` 硬编码路径
     *   - 访问类型 (Access): RO/RW/WO
     *   - 副作用类型 (SideEffect): NONE/DOORBELL
     *   - 门铃 side_effect：触发 Doorbell 强序写（250-700ns 区间），
     *     到期后由 PcieEndpointTLM 读取 emit_doorbell_out() 发出 mmio_out 事务
     *   - 未知偏移的 MMIO_WRITE 静默丢弃；MMIO_READ 返回 0xFFFFFFFF
     *
     * 强序写语义（per PCIe_上的保序write.md §4）：
     *   - 同一 stream_id 上的多次 ring 按调用顺序对 SQ 可见
     *   - tick() 推进内部周期计数器, 完成到期写入并更新可见 tail
     */
    class PcieBarRouter {
    public:
        // 寄存器访问类型
        enum class Access : uint8_t {
            RO = 0,  // Read-Only
            RW = 1,  // Read-Write
            WO = 2,  // Write-Only
        };

        // 副作用类型
        enum class SideEffect : uint8_t {
            NONE = 0,
            DOORBELL = 1,
        };

        // 寄存器定义
        struct RegisterEntry {
            uint32_t    offset;        // 4-byte aligned
            std::string name;         // 诊断名 (e.g.g., "GPU_REG_DOORBELL")
            Access      access;        // RO/RW/WO
            SideEffect  side_effect;   // NONE/DOORBELL
            uint32_t    value = 0;     // 当前寄存器值
            uint32_t    doorbell_stream_id = 0;  // 仅 DOORBELL 有效
        };

        // Doorbell 出向事务（mmio_out 端口传输）
        struct DoorbellOutEvent {
            uint32_t    stream_id;
            uint64_t    wdu_offset;     // 新 SQ tail
            uint32_t    trans_id;
            uint64_t    complete_cycle; // tick() 检测该 cycle >= now_ 时输出
        };

        PcieBarRouter();
        ~PcieBarRouter() = default;

        PcieBarRouter(const PcieBarRouter&) = delete;
        PcieBarRouter& operator=(const PcieBarRouter&) = delete;
        PcieBarRouter(PcieBarRouter&&) = delete;
        PcieBarRouter& operator=(PcieBarRouter&&) = delete;

        // 初始化（构造 regs_ 与 doorbell_）
        void init(uint64_t cycle_ns = 1);

        // 添加寄存器定义（JSON 注入或代码添加；offset 必须 4-byte 对齐）
        // side_effect=DOORBELL 时 doorbell_stream_id 决定投递到哪个 stream
        bool add_register(uint32_t offset, const std::string& name, Access access,
                          SideEffect side_effect, uint32_t doorbell_stream_id = 0);

        // MMIO_READ：返回寄存器值；未知偏移返回 0xFFFFFFFF
        uint32_t mmio_read(uint32_t offset) const;

        // MMIO_WRITE：写寄存器；doorbell side_effect 触发 Doorbell.ring()
        // 返回 true 表示成功；false 表示 offset 越界/未对齐/RO 写
        bool mmio_write(uint32_t offset, uint32_t value, uint32_t trans_id = 0);

        // tick()：推进 doorbell_ 周期；将到期门铃写入 pending_doorbell_out_
        void tick();

        // 检测并弹出下一个待输出的门铃事件（mmio_out 端口传输）
        // 返回 nullptr 表示无待输出；调用方消费后必须调 consume_doorbell_out()
        const DoorbellOutEvent* try_pop_doorbell_out();

        // 弹出已消费的 doorbell 事件（与 try_pop_doorbell_out 配对使用）
        void consume_doorbell_out();

        // 当前 in-flight doorbell 数量（用于测试断言）
        std::size_t pending_doorbell_count() const;

        // Doorbell 强序访问（用于 250-700ns 时序断言）
        bool doorbell_is_pending(uint32_t stream_id) const;
        uint64_t doorbell_sq_tail(uint32_t stream_id) const;
        uint64_t doorbell_now_cycles() const;

    private:
        std::unordered_map<uint32_t, RegisterEntry> regs_;
        std::deque<DoorbellOutEvent> pending_doorbell_out_;
        Doorbell doorbell_;
        uint64_t now_cycles_ = 0;

        // Helper：构造 doorbell out 事件
        void enqueue_doorbell_out(const RegisterEntry& entry, uint32_t value, uint32_t trans_id);
    };

} // namespace tlm::gpu

#endif // CPPTLM_PCIE_BAR_ROUTER_MVP_H