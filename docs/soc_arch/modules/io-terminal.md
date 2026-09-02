# io-terminal 微架构文档

> **类别**: io > terminal · **状态**: 🟡 规划中 + 📋 v1.0 dGPU SoC 战略补充
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/terminal_tlm.hh`
> **蓝图来源**: gem5 `src/dev/terminal.hh`（字符 I/O 终端设备）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-pio.md](./io-pio.md) | [io-uart.md](./io-uart.md) | [io-disk.md](./io-disk.md)

---

## 1. 设计目标（蓝图）

`tlm::TerminalTLM` 是 CppTLM v2.2+ 规划的 **字符 I/O 终端**——提供 stdout/stdin 流式 I/O 仿真。**与 gem5 对位**: `gem5::Terminal`（~150 行，stdout/stderr 输出 + 输入缓冲）。

**核心特征**：
- **字符流 I/O**（stdout/stderr 输出 + stdin 输入）
- **缓冲行**（典型 80 字符/行）
- **可选 PIO 接口**（CPU 通过 load/store 写入字符）
- **可选 DMA 接口**（大块传输）
- **历史回放**（v0 留接口）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                  TerminalTLM 简化                            │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  PIO 接口 (可选)                                  │     │
│  │    - DATA register (write 字符)                    │     │
│  │    - STATUS register (read 状态)                   │     │
│  │    - COMMAND register (启动/停止)                  │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  line_buffer_ (典型 80 字符)                      │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  output_target_                                   │     │
│  │    - stdout (cout)                                │     │
│  │    - log file (可选)                              │     │
│  │    - pipe (Python dashboard)                       │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `pio_req_in_` | `InputStreamAdapter<PioReqBundle>` | 接收 PIO 写入（字符） |
| `pio_resp_out_` | `OutputStreamAdapter<PioRespBundle>` | 发送 PIO 读取（状态） |

### 2.2 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  TerminalTLM 内部                           │
│                                                             │
│  配置:                                                      │
│    - line_length_: 80 (默认)                                │
│    - line_buffer_: string (当前行)                          │
│    - output_target_: STDOUT / LOG_FILE / PIPE              │
│    - log_file_path_: 可选                                   │
│                                                             │
│  寄存器:                                                    │
│    - DATA_REG: 8-bit (写字符)                               │
│    - STATUS_REG: 8-bit (TX_READY / RX_READY)                │
│    - COMMAND_REG: 8-bit (CLEAR / FLUSH)                    │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class TerminalTLM : public PioDeviceTLM {
public:
    static constexpr uint8_t DATA_REG = 0x00;
    static constexpr uint8_t STATUS_REG = 0x04;
    static constexpr uint8_t COMMAND_REG = 0x08;

    static constexpr uint8_t STATUS_TX_READY = 0x01;
    static constexpr uint8_t STATUS_RX_READY = 0x02;

    static constexpr uint8_t CMD_CLEAR = 0x01;
    static constexpr uint8_t CMD_FLUSH = 0x02;

    static constexpr uint64_t DEFAULT_BASE_ADDR = 0x3F8;  // COM1 (经典)

    explicit TerminalTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "TerminalTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_line_length(uint32_t n) { line_length_ = n; }
    void set_log_file(const std::string& path) { log_file_path_ = path; }
    void enable_python_pipe(bool en) { python_pipe_enabled_ = en; }

    // === PioDevice 抽象实现 ===
    uint64_t read(uint64_t offset, unsigned size_bytes) override;
    void write(uint64_t offset, uint64_t data, unsigned size_bytes) override;

    // === ChStream 桥接 ===
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void output_char(char c);
    void output_line();
    void open_log_file();
    void write_to_python_pipe(const std::string& line);

    uint32_t line_length_;
    std::string line_buffer_;
    std::ofstream log_file_;
    std::string log_file_path_;
    bool python_pipe_enabled_;
    int python_pipe_fd_;

    uint8_t status_reg_;
    char rx_buffer_;  // 单字符 RX（v0 简化）

    // 统计
    tlm_stats::Scalar chars_written_;
    tlm_stats::Scalar lines_output_;
    tlm_stats::Scalar pio_writes_;
    tlm_stats::Scalar pio_reads_;
};
}
```

## 4. 行为流程

```cpp
void TerminalTLM::write(uint64_t offset, uint64_t data, unsigned size_bytes) {
    uint8_t value = static_cast<uint8_t>(data);

    switch (offset) {
        case DATA_REG:
            output_char(static_cast<char>(value));
            status_reg_ |= STATUS_TX_READY;
            break;

        case COMMAND_REG:
            if (value & CMD_CLEAR) line_buffer_.clear();
            if (value & CMD_FLUSH) output_line();
            break;
    }
    ++pio_writes_;
}

uint64_t TerminalTLM::read(uint64_t offset, unsigned size_bytes) {
    uint8_t value = 0;
    switch (offset) {
        case STATUS_REG:
            value = status_reg_;
            break;
        case DATA_REG:
            if (rx_buffer_ != 0) {
                value = static_cast<uint8_t>(rx_buffer_);
                rx_buffer_ = 0;
                status_reg_ &= ~STATUS_RX_READY;
            }
            break;
    }
    ++pio_reads_;
    return value;
}

void TerminalTLM::output_char(char c) {
    if (c == '\n') {
        output_line();
    } else {
        line_buffer_ += c;
        if (line_buffer_.size() >= line_length_) {
            output_line();
        }
    }
}

void TerminalTLM::output_line() {
    if (line_buffer_.empty()) return;

    if (log_file_.is_open()) {
        log_file_ << line_buffer_ << "\n";
        log_file_.flush();
    }
    if (python_pipe_enabled_ && python_pipe_fd_ >= 0) {
        write_to_python_pipe(line_buffer_);
    }
    std::cout << line_buffer_ << std::endl;

    line_buffer_.clear();
    ++lines_output_;
    ++chars_written_;
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/terminal.hh` Terminal | `tlm::TerminalTLM` | 同语义 |
| `Terminal::dataReg` | `DATA_REG` | 同语义 |
| `Terminal::statusReg` | `STATUS_REG` | 同语义 |
| `Terminal::txBuffer` | `line_buffer_` | 同语义 |
| `Terminal::output() ` | `output_line` | 同语义 |

## 6. 实施路径

### Phase 7+ 步骤

1. 新建 `include/tlm/io/terminal_tlm.hh`（~200 行）
2. 继承 PioDeviceTLM
3. 实现 3 个寄存器 (DATA / STATUS / COMMAND)
4. 实现 log file + Python pipe 输出
5. 加 Catch2 测试

**估计工作量**: 1 周

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **Python pipe 同步**——pipe 满阻塞仿真 | 中 | 中 | 非阻塞写（O_NONBLOCK） |
| R2 | **多 Terminal 地址冲突** | 中 | 中 | 启动时校验 |
| R3 | **行缓冲内存膨胀**——大行不刷新 | 中 | 中 | 强制 80 字符换行 |

## 8. 决策点

### D1 默认 base_addr
- **Q**: 默认 base_addr 多少？
- **建议**: 0x3F8 (COM1，x86 经典)
- **依赖**: 平台约定

### D2 Python pipe 启用
- **Q**: 默认启用 Python pipe？
- **建议**: 否（v0 简化，按需启用）
- **依赖**: dashboard 需求

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7+ (未来)**: 基础版实施
