// include/tlm/pcie/pcie_tlp_codec.hh
// PcieTlpCodec: wire-format TLP 编解码器 (3DW/4DW header + CRC-32/LCRC/DLLP-CRC16)
// 功能描述：PCIe TLP wire-format 编码/解码工具类
//           - CRC-32 (poly 0x04C11DB7, init 0xFFFFFFFF, reflected, XOR-out 0xFFFFFFFF)
//           - LCRC-32 (cover header+payload+ECRC) + ECRC-32 (cover header+payload, TD=1)
//           - DLLP CRC-16 (poly 0x100B, reflected)
//           - TLP 编码: MRd / MWr / CplD (3DW header, TD=0)
//           - TLP 解码: 任意 3DW TLP → DecodedTlp 结构
//           - Malformed TLP 判别: 格式校验
// 作者 CppTLM Team / 日期 2026-09-17
// 参考: PCIe Base Spec §2.2 (TLP header), §2.7 (CRC)
//       openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/wire-format/spec.md
// C++23 迁移 (2026-09-22): decode() 返回类型 std::optional → std::expected,
//       显式区分解码失败原因 (per Phase 3 C++23 采纳: std::expected 错误传播).
#ifndef TLM_PCIE_PCIE_TLP_CODEC_HH
#define TLM_PCIE_PCIE_TLP_CODEC_HH

#include <cstdint>
#include <expected>
#include <vector>

namespace cpptlm::pcie {

/**
 * @brief PCIe TLP wire-format 编解码器
 *
 * 所有方法均为静态。不持有状态，可安全并发使用。
 * 当前支持 3DW header (32-bit address) + TD=0 (no ECRC) TLP。
 */
class PcieTlpCodec {
public:
    /// TLP 解码错误原因 (std::expected error type)
    enum class DecodeError {
        TooShort,       ///< 输入长度 < 最小 TLP (3DW header 12B + LCRC 4B = 16B)
        Unsupported4Dw, ///< 4DW header (64-bit address) 尚未支持
        IncompleteData, ///< 长度不足以容纳 header + payload + LCRC
    };
    /// TLP 解码结果结构体
    struct DecodedTlp {
        uint8_t fmt = 0;       // Fmt[2:0]
        uint8_t type = 0;      // Type[4:0]
        uint8_t tc = 0;        // Traffic Class
        uint8_t td = 0;        // TLP Digest (1 = ECRC present)
        uint8_t ep = 0;        // Poisoned
        uint8_t attr = 0;      // Attributes
        uint16_t length = 0;   // Length in DW (0 = 1024 DW)
        uint16_t requester_id = 0;  // Requester ID / Completer ID
        uint8_t tag = 0;       // Tag
        uint32_t address = 0;  // 32-bit address (for Memory TLPs)
        uint16_t byte_count = 0;   // Byte Count (for Completion TLPs)
        uint16_t completer_id = 0; // Completer ID (for Completion TLPs)
        uint8_t status = 0;    // Completion Status
        std::vector<uint8_t> payload;  // Data payload
        uint32_t lcrc = 0;     // Last 4 bytes = LCRC
        bool lcrc_valid = false;     // LCRC matches TLP content
        bool ecrc_valid = false;     // ECRC present and valid (only if TD=1)
    };

    /// TLP Kind 枚举 (解码后分类)
    enum class TlpKind {
        MRd,        // Memory Read
        MWr,        // Memory Write
        CplD,       // Completion with Data
        Cpl,        // Completion without Data
        CfgRd0,     // Configuration Read Type 0
        CfgWr0,     // Configuration Write Type 0
        CfgRd1,     // Configuration Read Type 1
        CfgWr1,     // Configuration Write Type 1
        Msg,        // Message
        MsgD,       // Message with Data
        Unknown     // Unrecognized Fmt/Type combination
    };

    // ========== CRC 计算 ==========

    /** CRC-32 (PCIe spec §2.7). poly=0x04C11DB7, reflected, init=0xFFFFFFFF, xor-out=0xFFFFFFFF */
    static uint32_t crc32_pcie(const uint8_t* data, std::size_t len);

    /** LCRC-32: header+payload+ECRC 的 CRC-32 (同 crc32_pcie) */
    static uint32_t compute_lcrc(const uint8_t* data, std::size_t len);

    /** ECRC-32: header+payload 的 CRC-32 (同 crc32_pcie), 仅当 TD=1 时使用 */
    static uint32_t compute_ecrc(const uint8_t* data, std::size_t len);

    /** DLLP CRC-16: poly=0x100B, reflected, init=0xFFFF */
    static uint16_t compute_dllp_crc16(const uint8_t* data, std::size_t len);

    // ========== TLP 编码 (生成 wire-format bytes) ==========

    /** 编码 Memory Write TLP (3DW header, 32-bit address, TD=0) */
    static std::vector<uint8_t> encode_mwr(uint16_t requester_id, uint8_t tag,
                                            uint32_t address,
                                            const uint8_t* data, std::size_t len);

    /** 编码 Memory Read TLP (3DW header, 32-bit address, TD=0) */
    static std::vector<uint8_t> encode_mrd(uint16_t requester_id, uint8_t tag,
                                            uint32_t address, std::size_t len_dw);

    /** 编码 Completion with Data TLP (3DW header, TD=0) */
    static std::vector<uint8_t> encode_cpld(uint16_t completer_id, uint16_t requester_id,
                                             uint8_t tag, uint16_t byte_count,
                                             const uint8_t* data, std::size_t len);

    // ========== TLP 解码 ==========

    /** 解码 wire-format TLP → DecodedTlp. 返回 unexpected{DecodeError} 当格式错误/长度不足. */
    static std::expected<DecodedTlp, DecodeError>
    decode(const uint8_t* tlp_bytes, std::size_t len);

    /** 判定 TLP 是否 malformed (length 不匹配 / CRC 校验失败 / 保留字段非零等) */
    static bool is_malformed(const DecodedTlp& tlp);

    /** 从 DecodedTlp 推导 TLP Kind */
    static TlpKind classify(const DecodedTlp& tlp);

    // ========== CRC self-test 工具 ==========

    /** 验证 CRC 实现是否符合规范测试向量 */
    static bool crc_self_test();
};

} // namespace cpptlm::pcie

#endif // TLM_PCIE_PCIE_TLP_CODEC_HH