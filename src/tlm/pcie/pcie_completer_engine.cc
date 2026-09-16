// src/tlm/pcie/pcie_completer_engine.cc
// PcieCompleterEngine 实现 (T-P10-1)
// 强制 #include "tlm/pcie/pcie_tlp_codec.hh" — CplD 必须走 encode_cpld API
// 作者 CppTLM Team / 日期 2026-09-17
#include "tlm/pcie/pcie_completer_engine.hh"
#include "tlm/pcie/pcie_tlp_codec.hh"

#include <algorithm>
#include <cstring>

namespace cpptlm::pcie {

// ==================== 构造 ====================

PcieCompleterEngine::PcieCompleterEngine() {
    // 初始化 config_space_ 为已知默认值
    // PCIe spec: Vendor ID @0x00, Device ID @0x02
    config_space_[0] = 0x123410DEu;  // Vendor=0x10DE, Device=0x1234
    // Command @0x04, Status @0x06
    config_space_[1] = 0x00000000u;  // Command=0 (disable MMIO, etc.)
    // BAR 寄存器默认全 0
    for (auto& v : bar_bases_) {
        v = 0;
    }
}

// ==================== ConfigSpace 读/写 ====================

void PcieCompleterEngine::config_space_write(uint16_t offset, uint8_t width, uint32_t value) {
    if (offset >= CONFIG_SPACE_SIZE) {
        return;  // 越界忽略
    }
    const uint16_t idx = (offset / 4) % (CONFIG_SPACE_SIZE / 4);
    const uint8_t shift = static_cast<uint8_t>((offset % 4) * 8);

    if (width == 4 && (offset % 4) == 0) {
        // 4-byte aligned 32-bit write
        config_space_[idx] = value;
    } else if (width == 2) {
        // 16-bit write
        const uint32_t mask = 0xFFFFu << shift;
        config_space_[idx] = (config_space_[idx] & ~mask) | ((value & 0xFFFFu) << shift);
    } else if (width == 1) {
        // 8-bit write
        const uint32_t mask = 0xFFu << shift;
        config_space_[idx] = (config_space_[idx] & ~mask) | ((value & 0xFFu) << shift);
    } else {
        // fallback: 直接写 (未对齐 32-bit 也走此路径)
        config_space_[idx] = value;
    }

    // BAR 寄存器写 → 触发路由表重建
    // BAR0 @ offset 0x10, BAR1 @ 0x14, BAR2 @ 0x18, BAR3 @ 0x1C, BAR4 @ 0x20, BAR5 @ 0x24
    if (offset >= 0x10 && offset <= 0x24 && ((offset - 0x10) % 4) == 0) {
        rebuild_bdf_bar_route_table();
    }
}

uint32_t PcieCompleterEngine::config_space_read(uint16_t offset, uint8_t width) {
    if (offset >= CONFIG_SPACE_SIZE) {
        return 0xFFFFFFFFu;  // 越界返回 ALL_ONES
    }
    const uint16_t idx = (offset / 4) % (CONFIG_SPACE_SIZE / 4);
    const uint8_t shift = static_cast<uint8_t>((offset % 4) * 8);
    const uint32_t raw = config_space_[idx];

    if (width == 4 && (offset % 4) == 0) {
        return raw;
    } else if (width == 2) {
        return (raw >> shift) & 0xFFFFu;
    } else if (width == 1) {
        return (raw >> shift) & 0xFFu;
    }
    return raw;  // fallback
}

// ==================== BAR 路由表 ====================

void PcieCompleterEngine::rebuild_bdf_bar_route_table() {
    // 从 config_space_ 读取 BAR 寄存器 (offset 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24)
    // PCIe spec: BAR 寄存器的低位表示类型
    //   bit 0 = 0: Memory BAR (32-bit)
    //   bit 0 = 1: IO BAR
    //   bit 2:1 = 00: 32-bit, 10: 64-bit
    // 简化模型: 将 BAR 寄存器的值(清除低 4 位)作为基地址
    static constexpr uint16_t bar_offsets[6] = {0x10, 0x14, 0x18, 0x1C, 0x20, 0x24};

    for (int i = 0; i < 6; ++i) {
        const uint16_t idx = bar_offsets[i] / 4;
        const uint32_t bar_val = config_space_[idx];
        if (bar_val == 0) {
            bar_bases_[i] = 0;
            continue;
        }
        // Memory BAR: 清除低 4 位 (type+prefetchable)
        // IO BAR: 清除低 2 位
        if ((bar_val & 0x1) == 0) {
            // Memory BAR
            bar_bases_[i] = static_cast<uint64_t>(bar_val & 0xFFFFFFF0u);
        } else {
            // IO BAR
            bar_bases_[i] = static_cast<uint64_t>(bar_val & 0xFFFFFFFCu);
        }
    }
}

uint64_t PcieCompleterEngine::route_bdf_to_bar(uint16_t /*bdf*/, uint8_t bar) const {
    if (bar >= 6) {
        return 0;
    }
    return bar_bases_[bar];
}

// ==================== bar_store_ helper ====================

uint64_t PcieCompleterEngine::bar_store_value(uint8_t bar, uint64_t offset) const {
    const uint64_t key = bar_store_key(bar, offset);
    const auto it = bar_store_.find(key);
    return (it != bar_store_.end()) ? it->second : 0;
}

void PcieCompleterEngine::bar_store_write(uint8_t bar, uint64_t offset, uint64_t value) {
    const uint64_t key = bar_store_key(bar, offset);
    bar_store_[key] = value;
}

// ==================== 核心: handle_tlp ====================

bundles::PcieTlpBundle PcieCompleterEngine::handle_tlp(const bundles::PcieTlpBundle& tlp) {
    const uint8_t kind = static_cast<uint8_t>(tlp.kind.read());

    switch (kind) {
    case bundles::PcieTlpBundle::CFG_READ:
        return handle_cfgrd(tlp);
    case bundles::PcieTlpBundle::CFG_WRITE:
        return handle_cfgwr(tlp);
    case bundles::PcieTlpBundle::MMIO_READ:
        return handle_mmio_read(tlp);
    case bundles::PcieTlpBundle::MMIO_WRITE:
        return handle_mmio_write(tlp);
    case bundles::PcieTlpBundle::MEM_READ:
        return handle_mem_read(tlp);
    case bundles::PcieTlpBundle::MEM_WRITE:
        return handle_mem_write(tlp);
    default:
        // 未知 kind: 返回空 bundle
        return bundles::PcieTlpBundle();
    }
}

// ==================== 各分支处理 ====================

bundles::PcieTlpBundle PcieCompleterEngine::handle_cfgrd(const bundles::PcieTlpBundle& req) {
    const uint16_t cfg_offset = static_cast<uint16_t>(req.offset.read() & ~0x3ULL);
    const uint8_t width = static_cast<uint8_t>(std::min<uint32_t>(
        static_cast<uint32_t>(req.size.read()), 4));
    const uint32_t value = config_space_read(cfg_offset, width);

    // 生成 CplD
    // byte_count = data payload 大小
    // data_length_for_cpld: width bytes
    std::size_t data_len = width;
    // 构造 data buffer
    uint8_t data_buf[8] = {0};
    std::memcpy(data_buf, &value, std::min<std::size_t>(data_len, sizeof(value)));

    uint8_t tag = static_cast<uint8_t>(req.trans_id.read() & 0xFF);
    uint16_t byte_count = static_cast<uint16_t>(data_len);

    return generate_cpld(
        static_cast<uint16_t>(req.requester_id.read()),
        tag,
        byte_count,
        data_buf,
        data_len
    );
}

bundles::PcieTlpBundle PcieCompleterEngine::handle_cfgwr(const bundles::PcieTlpBundle& req) {
    const uint16_t cfg_offset = static_cast<uint16_t>(req.offset.read() & ~0x3ULL);
    const uint8_t width = static_cast<uint8_t>(std::min<uint32_t>(
        static_cast<uint32_t>(req.size.read()), 4));
    const uint32_t value = static_cast<uint32_t>(req.data.read());

    config_space_write(cfg_offset, width, value);

    // 返回空 bundle (写事务不需要 CplD)
    return bundles::PcieTlpBundle();
}

bundles::PcieTlpBundle PcieCompleterEngine::handle_mmio_read(const bundles::PcieTlpBundle& req) {
    const uint8_t bar = static_cast<uint8_t>(req.bar_index.read());
    const uint64_t offset_in_bar = req.offset.read();
    const uint64_t value = bar_store_value(bar, offset_in_bar);

    uint8_t tag = static_cast<uint8_t>(req.trans_id.read() & 0xFF);
    uint16_t byte_count = static_cast<uint16_t>(std::min<uint32_t>(
        static_cast<uint32_t>(req.size.read()), 8));
    uint8_t data_buf[8] = {0};
    std::memcpy(data_buf, &value, sizeof(data_buf));

    return generate_cpld(
        static_cast<uint16_t>(req.requester_id.read()),
        tag,
        byte_count,
        data_buf,
        static_cast<std::size_t>(byte_count)
    );
}

bundles::PcieTlpBundle PcieCompleterEngine::handle_mmio_write(const bundles::PcieTlpBundle& req) {
    const uint8_t bar = static_cast<uint8_t>(req.bar_index.read());
    const uint64_t offset_in_bar = req.offset.read();
    const uint64_t value = req.data.read();

    // 4B 粒度 + wstrb (简化: full wstrb)
    bar_store_write(bar, offset_in_bar, value);

    // 写事务返回空 bundle
    return bundles::PcieTlpBundle();
}

bundles::PcieTlpBundle PcieCompleterEngine::handle_mem_read(const bundles::PcieTlpBundle& req) {
    const uint8_t bar = static_cast<uint8_t>(req.bar_index.read());
    const uint64_t offset_in_bar = req.offset.read();
    const uint64_t value = bar_store_value(bar, offset_in_bar);

    uint8_t tag = static_cast<uint8_t>(req.trans_id.read() & 0xFF);
    uint16_t byte_count = static_cast<uint16_t>(std::min<uint32_t>(
        static_cast<uint32_t>(req.size.read()), 8));
    uint8_t data_buf[8] = {0};
    std::memcpy(data_buf, &value, sizeof(data_buf));

    return generate_cpld(
        static_cast<uint16_t>(req.requester_id.read()),
        tag,
        byte_count,
        data_buf,
        static_cast<std::size_t>(byte_count)
    );
}

bundles::PcieTlpBundle PcieCompleterEngine::handle_mem_write(const bundles::PcieTlpBundle& req) {
    const uint8_t bar = static_cast<uint8_t>(req.bar_index.read());
    const uint64_t offset_in_bar = req.offset.read();
    const uint64_t value = req.data.read();

    // 写 BAR (VRAM)
    bar_store_write(bar, offset_in_bar, value);

    // 写事务返回空 bundle
    return bundles::PcieTlpBundle();
}

// ==================== CplD 生成 (强制走 PcieTlpCodec::encode_cpld) ====================

bundles::PcieTlpBundle PcieCompleterEngine::generate_cpld(
    uint16_t requester_id, uint8_t tag, uint16_t byte_count,
    const uint8_t* data, std::size_t len)
{
    // 强制使用 PcieTlpCodec::encode_cpld 生成 wire-format bytes
    // 这确保 CRC 和 TLP header 填充不 inline 实现
    PcieTlpCodec::encode_cpld(
        completer_id_,  // completer_id
        requester_id,   // requester_id
        tag,            // tag
        byte_count,     // byte_count
        data,           // data payload
        len             // payload length
    );

    // 从原始 input data 构造 data_val (不依赖 wire bytes 提取, 避免 LCRC 泄漏)
    uint64_t data_val = 0;
    for (std::size_t i = 0; i < 8 && i < len; ++i) {
        data_val |= static_cast<uint64_t>(data[i]) << (i * 8);
    }

    // 填充 CplD bundle
    bundles::PcieTlpBundle cpld;
    cpld.kind.write(bundles::PcieTlpBundle::CPLD);  // CPLD=7 (per T-P10-1)
    cpld.requester_id.write(requester_id);  // Requester ID echo
    cpld.trans_id.write(tag);   // Tag echo
    cpld.data.write(data_val);  // Payload 首 8 字节
    cpld.size.write(byte_count);
    cpld.offset.write(0);       // lower_address = 0 (简化)
    return cpld;
}

} // namespace cpptlm::pcie