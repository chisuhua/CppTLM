// include/tlm/gpu/pcie_config_space_mvp.hh
// PcieConfigSpace: SOC 片内 PCIe Endpoint Config Space 模型（MVP）
// 功能描述：参数化 256B / 4KB Config Space + capability chain 链表
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/design.md §3
//       ADR-SOC-07 D2
#ifndef CPPTLM_PCIE_CONFIG_SPACE_MVP_H
#define CPPTLM_PCIE_CONFIG_SPACE_MVP_H

#include <cstdint>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief SOC PCIe Endpoint Config Space（MVP）
     *
     * 设计原则（per design.md §3 + spec.md "Capability chain walk"）：
     *   - 参数化 config_size：256（PCIe 1.x 兼容）或 4096（PCIe 2.0+ extended）
     *   - capability chain 链表由 JSON 声明（id + offset + next）
     *   - 越界 read 返回 0xFFFFFFFF（PCIe spec 标准 ALL_ONES）
     *   - write 越界静默忽略（PCIe spec：posted/completion 行为简化）
     *   - vendor_id/device_id/revision 默认值对齐 s2 DGpuBar
     *
     * 用途：作为 PcieEndpointTLM 的内部子块，被 PcieEndpointTLM::tick()
     *       在 CFG_READ/CFG_WRITE 路径调用。
     */
    class PcieConfigSpace {
    public:
        // Config Space 大小（字节）
        static constexpr std::size_t CFG_SIZE_256B = 256;
        static constexpr std::size_t CFG_SIZE_4KB  = 4096;

        // 默认 vendor/device/revision（对齐 s2 DGpuBar）
        static constexpr uint16_t DEFAULT_VENDOR_ID  = 0x10DE;
        static constexpr uint16_t DEFAULT_DEVICE_ID  = 0x1234;
        static constexpr uint8_t  DEFAULT_REVISION   = 0x01;
        static constexpr uint8_t  DEFAULT_HEADER_TYPE = 0x00;  // Type 0

        // capability 链表节点
        struct Capability {
            uint8_t  id;       // capability ID (per PCI-SIG)
            uint8_t  offset;   // config space offset（4-byte aligned）
            uint8_t  next;     // next capability offset (0x00 = end)
            uint16_t control;  // capability-specific control word (offset+2)
        };

        PcieConfigSpace(std::size_t config_size = CFG_SIZE_4KB);
        ~PcieConfigSpace() = default;

        PcieConfigSpace(const PcieConfigSpace&) = delete;
        PcieConfigSpace& operator=(const PcieConfigSpace&) = delete;
        PcieConfigSpace(PcieConfigSpace&&) = delete;
        PcieConfigSpace& operator=(PcieConfigSpace&&) = delete;

        // 初始化：填充 vendor/device/revision/header
        void init();

        // 添加 capability（JSON 注入或代码添加；offset 必须 4-byte 对齐）
        // 失败原因：offset 越界 / next 越界 / 与已有 capability 重叠
        bool add_capability(uint8_t id, uint8_t offset, uint8_t next, uint16_t control = 0);

        // Read/Write（4-byte 对齐 offset；越界 read 返回 0xFFFFFFFF）
        uint32_t read(uint16_t offset) const;
        void write(uint16_t offset, uint32_t value);

        // Capability chain walk：返回 chain 中 capability 数量（per spec.md Scenario）
        std::size_t capability_count() const { return capabilities_.size(); }

        // Get capability by index（用于测试验证 chain 顺序）
        const Capability* get_capability(std::size_t index) const;

        // Get config_size
        std::size_t config_size() const { return config_size_; }

    private:
        std::size_t config_size_;
        std::vector<uint32_t> regs_;       // 4-byte aligned register array
        std::vector<Capability> capabilities_;

        // 内部 helper：检查 offset 是否 4-byte aligned
        static bool is_aligned(uint16_t offset) { return (offset & 0x3) == 0; }
    };

} // namespace tlm::gpu

#endif // CPPTLM_PCIE_CONFIG_SPACE_MVP_H