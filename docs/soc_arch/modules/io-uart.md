# io-uart 微架构文档

> **类别**: io > uart · **状态**: 🟡 规划中 + 📋 v1.0 dGPU SoC 战略补充
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/uart8250_tlm.hh`
> **蓝图来源**: gem5 `src/dev/uart8250.hh`（经典 16550 兼容 UART）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-pio.md](./io-pio.md) | [io-terminal.md](./io-terminal.md)

---

## 1. 设计目标（蓝图）

`tlm::Uart8250TLM` 是 CppTLM v2.2+ 规划的 **经典 UART 16550 兼容串口**——8-bit 字符流，典型用于系统调试输出。**与 gem5 对位**: `gem5::Uart8250`（~200 行，11 个寄存器 + TX/RX FIFO）。

**核心特征**：
- **11 个寄存器**（THR/RBR/IER/IIR/FCR/LCR/MCR/LSR/MSR/SCR/DLL）
- **TX/RX 16 字节 FIFO**
- **可配波特率**（默认 115200）
- **IRQ 中断**（v0 简化：可选）
- **与 TerminalTLM 协同**（输出到同一 line_buffer_）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                  Uart8250TLM 继承 PioDeviceTLM              │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  11 寄存器 (单字节宽度)                            │     │
│  │    - 0x00: THR (TX) / RBR (RX) / DLL (divisor L)  │     │
│  │    - 0x01: IER / DLM (divisor H)                 │     │
│  │    - 0x02: IIR / FCR                              │     │
│  │    - 0x03: LCR                                    │     │
│  │    - 0x04: MCR                                    │     │
│  │    - 0x05: LSR                                    │     │
│  │    - 0x06: MSR                                    │     │
│  │    - 0x07: SCR                                    │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  TX/RX FIFO (16 字节各)                            │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  output_target_ (stdout / log file / Python pipe)   │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 端口表

继承 PioDeviceTLM 的 2 端口（req_in + resp_out）。

## 3. 接口（规划）

```cpp
namespace tlm {
class Uart8250TLM : public PioDeviceTLM {
public:
    static constexpr uint8_t NUM_REGS = 8;
    static constexpr uint8_t FIFO_SIZE = 16;
    static constexpr uint32_t DEFAULT_BAUD_RATE = 115200;
    static constexpr uint64_t DEFAULT_BASE_ADDR = 0x3F8;  // COM1

    explicit Uart8250TLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "Uart8250TLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_baud_rate(uint32_t baud) { baud_rate_ = baud; }
    void set_log_file(const std::string& path) { log_file_path_ = path; }

    // === PioDevice 抽象实现 ===
    uint64_t read(uint64_t offset, unsigned size_bytes) override;
    void write(uint64_t offset, uint64_t data, unsigned size_bytes) override;

    // === 字符输入（外部注入 stdin） ===
    void inject_input_char(char c);

    // === ChStream 桥接 ===
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void tx_char(char c);
    void update_dlab_state();

    uint32_t baud_rate_;
    bool dlab_;  // Divisor Latch Access Bit (LCR bit 7)
    uint16_t divisor_;
    uint8_t regs_[NUM_REGS];
    std::array<char, FIFO_SIZE> tx_fifo_;
    uint8_t tx_fifo_head_, tx_fifo_tail_, tx_fifo_count_;
    std::array<char, FIFO_SIZE> rx_fifo_;
    uint8_t rx_fifo_head_, rx_fifo_tail_, rx_fifo_count_;

    std::ofstream log_file_;
    std::string log_file_path_;
    std::string line_buffer_;
    uint32_t line_length_;

    // 统计
    tlm_stats::Scalar chars_transmitted_;
    tlm_stats::Scalar chars_received_;
    tlm_stats::Scalar pio_writes_;
    tlm_stats::Scalar pio_reads_;
    tlm_stats::Scalar fifo_overruns_;
};
}
```

## 4. 行为流程

```cpp
uint64_t Uart8250TLM::read(uint64_t offset, unsigned size_bytes) {
    uint8_t value = 0;
    uint8_t reg = offset % NUM_REGS;

    if (dlab_ && reg == 0) reg = 8;  // DLL
    if (dlab_ && reg == 1) reg = 9;  // DLM

    switch (reg) {
        case 0:  // RBR
            if (rx_fifo_count_ > 0) {
                value = static_cast<uint8_t>(rx_fifo_[rx_fifo_head_]);
                rx_fifo_head_ = (rx_fifo_head_ + 1) % FIFO_SIZE;
                --rx_fifo_count_;
            }
            break;
        case 5:  // LSR (Line Status Register)
            value = 0x60;  // TX empty + TX holding empty
            if (rx_fifo_count_ > 0) value |= 0x01;  // data ready
            break;
        default:
            value = regs_[reg];
            break;
    }
    ++pio_reads_;
    return value;
}

void Uart8250TLM::write(uint64_t offset, uint64_t data, unsigned size_bytes) {
    uint8_t value = static_cast<uint8_t>(data);
    uint8_t reg = offset % NUM_REGS;

    if (dlab_ && reg == 0) { divisor_ = (divisor_ & 0xFF00) | value; return; }
    if (dlab_ && reg == 1) { divisor_ = (divisor_ & 0x00FF) | (value << 8); return; }

    switch (reg) {
        case 0:  // THR
            tx_char(static_cast<char>(value));
            break;
        case 3:  // LCR
            dlab_ = (value & 0x80) != 0;
            break;
        default:
            regs_[reg] = value;
            break;
    }
    ++pio_writes_;
}

void Uart8250TLM::tx_char(char c) {
    if (tx_fifo_count_ >= FIFO_SIZE) {
        ++fifo_overruns_;
        return;
    }
    tx_fifo_[tx_fifo_tail_] = c;
    tx_fifo_tail_ = (tx_fifo_tail_ + 1) % FIFO_SIZE;
    ++tx_fifo_count_;

    line_buffer_ += c;
    if (c == '\n' || line_buffer_.size() >= line_length_) {
        if (log_file_.is_open()) {
            log_file_ << line_buffer_;
            log_file_.flush();
        }
        std::cout << line_buffer_;
        line_buffer_.clear();
    }
    ++chars_transmitted_;
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/uart8250.hh` Uart8250 | `tlm::Uart8250TLM` | 同语义 |
| `Uart8250::THR` | `reg 0 (write)` | 同语义 |
| `Uart8250::RBR` | `reg 0 (read)` | 同语义 |
| `Uart8250::LCR` | `reg 3` | 同语义（含 DLAB） |
| `Uart8250::LSR` | `reg 5` | 同语义 |
| `Uart8250::intr_enable` | `reg 1` (IER) | v0 简化（IRQ 留 Phase 7+） |
| `Uart8250::FCR` | `reg 2` (FCR) | v0 简化（固定 FIFO） |

## 6. 实施路径

### Phase 7+ 步骤

1. 新建 `include/tlm/io/uart8250_tlm.hh`（~250 行）
2. 继承 PioDeviceTLM
3. 实现 11 寄存器 + DLB + FIFO
4. 实现 TX 输出到 log file / stdout
5. 加 Catch2 测试

**估计工作量**: 1-2 周

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **FIFO 真实行为缺失**——v0 简化 | 中 | 中 | Phase 7+ 真实 16 字节 FIFO |
| R2 | **IRQ 中断缺失**——v0 无 | 中 | 中 | Phase 7+ 加 interrupt port |
| R3 | **波特率不影响仿真**——v0 仅作记录 | 中 | 低 | 文档明确 |

## 8. 决策点

### D1 默认 base_addr
- **Q**: 默认 base_addr 多少？
- **建议**: 0x3F8 (COM1) 与 TerminalTLM 一致
- **依赖**: 平台约定

### D2 FIFO 行为
- **Q**: v0 简化 FIFO 还是真实 16 字节？
- **建议**: v0 真实 16 字节（实现简单）
- **依赖**: 仿真精度

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7+ (未来)**: 基础版实施
