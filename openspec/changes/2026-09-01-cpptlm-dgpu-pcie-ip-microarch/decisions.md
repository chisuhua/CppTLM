# cpptlm-dgpu-pcie-ip-microarch: 决策点文档 (Decisions)

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md) · [`roadmap.md`](./roadmap.md)
> **状态**: 📋 Decided — 2026-09-01
> **来源**: Oracle 咨询 `bg_661f4ee7` + Librarian 调研 `bg_07392e0c`

## 决策总览

| ID | 决策点 | 推荐选项 | 复杂度 | 状态 |
|---|---|---|---|:---:|
| Q1 | Gen5 128b/130b 编码抽象 | **C: 透明 + 延迟** | 🟢 低 | ✅ |
| Q2 | Flow Control 实现 | **B: Token Bucket** | 🟡 中 | ✅ |
| Q3 | PIPE 接口抽象 | **C: 4-signal 轻量** | 🟢 低 | ✅ |
| Q4 | 开源 PCIe PHY BFM | **B: 混合策略**(Xilinx fork + alexforencich wrap) | 🟡 中 | ✅ |
| Q5 | 商业验证策略 | **Cadence/Synopsys VIP reference**(不嵌入) | 🟢 低 | ✅ |
| Q6 | SR-IOV 端口架构 | **VF Pool 共享 StreamAdapter** | 🟡 中 | ✅ |
| Q7 | Bypass Mux 模式 | **Full / Bypass / Partial 3 态 + HostBypass 独立组件** | 🟡 中 | ✅ |
| Q8 | 23 ABI 兼容性策略(修订) | **A 修订: 23 C 符号冻结 + 类方法 noexcept/[[nodiscard]]**(对象错位已修正) | 🟢 低 | ✅ |
| Q9 | AXI4Mapper 范围 | **独立模块 + JSON 可选注入** | 🟡 中 | ✅ |
| **Q10** | **FLR**(Oracle 新增) | **简化 FLR: PF 全状态 + Vx 状态** | 🟡 中 | ✅ |
| **Q11** | **多 VC vs 单 VC**(Oracle 新增) | **单 VC0 + 保留 vc_id 字段** | 🟢 低 | ✅ |
| **Q12** | **Completion Timeout**(Oracle 新增) | **out-of-scope + TL completion 匹配** | 🟢 低 | ✅ |
| **Q13** | **AER / ECRC**(Oracle 新增) | **NOT 实现** | 🟢 低 | ✅ |
| **Q14** | **Surprise Removal**(Oracle 新增) | **drain(1µs) + abort + 回 Detect** | 🟡 中 | ✅ |
| **Q15** | **错误注入接口**(Oracle 新增) | **`link_error_injector_t` API** | 🟢 低 | ✅ |
| **Q16** | **ASPM / CLKREQ#**(Oracle 新增) | **NOT 实现** | 🟢 低 | ✅ |
| **Q17** | **下行 Rx 链路层**(Oracle 新增) | **双向链路层(DLLP/TLP 分流 + Rx ACK)** | 🟡 中 | ✅ |

---

## Q1: Gen5 128b/130b 编码抽象

### 选项对比

| 选项 | 描述 | 保真度 | 复杂度 | SoC 行为精度 |
|---|---|---|---|---|
| **A** | Full 128b/130b: 完整 bit-level 编码/解码 + 同步头 + 扰码 | ★★★★★ | 🔴 高(数千行 RTL 等价) | ★★★★★ |
| **B** | 半精度: skip CRC,保留 block alignment | ★★★☆☆ | 🟡 中 | ★★★★☆ |
| **C** | **透明 + 延迟**: 把 128b/130b 当不透明容器,只建模速率切换延迟 | ★★★★☆ | 🟢 低 | ★★★★★ |

### ✅ 决策: **选项 C**

### 理由

**关键修正**(Librarian `bg_07392e0c` 揭示):**Gen5 用 TLP + 128b/130b 编码,不是 FLIT mode**。FLIT 是 **PCIe 6.0** 特性(256B FLIT + PAM4 + FEC)。Oracle 初次咨询误判,已修订。

**SoC 仿真需求分析**:
- SoC 行为(VRAM 带宽 / 命令处理器饱和 / 中断延迟)取决于**何时到达 BAR1 aperture**,而非 bit-level 编码细节
- 128b/130b 编码在 PHY/MAC 层完成,**不向上传播**(bit 错误走链路重传,与 SoC 无关)
- 仿真只需建模"Gen5 速率(32 GT/s)下的链路延迟 ~32ns / 256-bit block"

**Spec 引用**:
- PCIe 5.0 Base Specification §4.2.2 "128b/130b Encoding and Scrambling"
- PCIe 5.0 §4.2.3 "Scrambling"
- 注意: FLIT 在 PCIe 6.0 §4.2.4 才引入

**实现要点**:
```cpp
// 用 PcieLinkLayer 内部 Latency 建模,不解析 130b
class PcieLinkLayer {
    static constexpr sc_time GEN5_BLOCK_LATENCY{32, SC_NS};  // 128b/130b 块延迟
    static constexpr sc_time GEN4_BLOCK_LATENCY{63, SC_NS};
    static constexpr sc_time GEN3_BLOCK_LATENCY{125, SC_NS};
    // ...
};
```

### 实施时机

**Phase 2**:`cpptlm-dgpu-pcie-130b-encoding`(2 周,简化为延迟建模)

---

## Q2: Flow Control 实现保真度

### 选项对比

| 选项 | 描述 | 保真度 | 复杂度 | SR-IOV 兼容 |
|---|---|---|---|---|
| **A** | Full 6-header credit: P/NP/Cpl × Hdr/Data = 6 个计数器,UpdateFC DLLP 周期级追踪 | ★★★★★ | 🔴 高 | 🔴 16VF × 6 = 96 计数器 |
| **B** | **Token Bucket**: P/NP/Cpl 各一个桶 + weight + refill rate | ★★★★☆ | 🟡 中 | 🟢 每 VF 一个桶即可 |
| **C** | Skip FC: 假设无限 credit | ★☆☆☆☆ | 🟢 低 | ❌ 反压失真 |

### ✅ 决策: **选项 B**

### 理由

**SoC 仿真关注 backpressure 行为,非 credit 精确值**。

Token Bucket 抽象捕捉关键行为:
- EP 接收能力上限 → 上游 RC 减慢(backpressure)
- EP 内部拥塞 → 下游 SoC 模块延迟
- 不同事务类型权重(P > NP > Cpl)

**vs 方案 A 的关键差异**:
- 方案 A 区分 Hdr credit vs Data credit 的精确边界(用于协议验证)
- SoC 仿真不关心"1 TLP 消耗 1 Hdr + 4 Data" 的具体细节
- 方案 A 在 SR-IOV 场景 96 个计数器开销爆炸

**Spec 引用**:
- PCIe 5.0 Base Specification §3.4 "Flow Control"
- §3.4.1 "Flow Control Initialization"
- §3.4.2 "Flow Control Updates"

**实现要点** (Oracle 修订 #2, 2026-09-01):
```cpp
// ⚠️ 关键修正: PCIe FC credit **只能由 UpdateFC DLLP 增加**,不存在自动 refill
// 自动 refill 会让 credit 永远不耗尽 → FC 反压永不触发 → backpressure 建模作废(本 change 核心目标)
class FcTokenBucket {
public:
    enum class Type { Posted, NonPosted, Completion };

    // 发送前检查:credit 不足返回 false(调用方必须等待 UpdateFC)
    [[nodiscard]] bool can_send(Type t) const noexcept;

    // 发送后扣减(只在 can_send() 为 true 后调用)
    void consume(Type t) noexcept;

    // UpdateFC DLLP 到达时由 Link Layer 调用(唯一补充路径)
    void update(Type t, uint32_t credit) noexcept;

private:
    uint32_t tokens_p_, tokens_np_, tokens_cpl_;
    uint32_t weight_p_ = 1, weight_np_ = 1, weight_cpl_ = 1;
    uint32_t capacity_ = 256;
    // credit 单调非减,无 refill_rate(由 InitFC1/InitFC2 设上限)
    // 无 sc_event_queue;事件通知由 link_layer_ 调度 EventQueue* 注入
};

// SR-IOV 场景: 每 VF 一个桶(per-VF 隔离)
class FcEngine {
    EventQueue* eq_;  // 注入 CppTLM 事件队列(非 SystemC sc_event_queue)
    std::unordered_map<VfId, FcTokenBucket> per_vf_buckets_;
    // PF0 = bucket index 0, VF0 = 1, ..., VF15 = 16(单 VC0,per Q11 决策)
};
```

**死锁防护** (Oracle R3 修订):
- ✅ InitFC1/InitFC2 顺序保证双向 credit 初始化
- ✅ EP 必须先发 InitFC1 给 RC 才能接受对方的 TLP
- ✅ 死锁防护测试: credit 耗尽 → 发送阻塞 → UpdateFC 到达 → 恢复(非"自动 refill 恢复")

### 实施时机

**Phase 1**:`cpptlm-dgpu-pcie-link-layer-and-fc`(基础 backpressure,所有后续阶段的依赖)

---

## Q3: PIPE 接口抽象级别

### 选项对比

| 选项 | 描述 | 保真度 | 复杂度 |
|---|---|---|---|
| **A** | Full PIPE 5.x: 50+ 信号完整时序(TxData/TxDataValid/TxStartBlock/TxSyncHeader/PhyStatus/RxValid/RxData/RxStatus[2:0]/...) | ★★★★★ | 🔴 高 |
| **B** | PIPE 子集: 数字通道状态 + 关键 strobe,抽象模拟 | ★★★☆☆ | 🟡 中 |
| **C** | **4-signal 轻量**: `(rate, lanes_active, elec_idle, training_state)` | ★★★★☆ | 🟢 低 |

### ✅ 决策: **选项 C**

### 理由

**PIPE 5.1 SerDes 架构变化**(Librarian 揭示):
- 传统 PIPE: PHY 包含 PCS (8b/10b, elastic buffer, polarity)
- **PIPE 5.1 SerDes**: PHY 仅保留 Serializer/Deserializer + CDR,**MAC 承担全部 PCS 功能**
- Legacy handshake 信号(`txdeemph`, `txmargin`, `rxfneqeval`)在 PIPE 5.1 已**废弃**,改用 MBI(Message Bus Interface)register 访问

SoC 仿真 4 个信号足够:
- `rate`: 带宽计算(⚠️ Oracle 修订 #5: `enum class Rate { GEN1=2, GEN2=5, GEN3=8, GEN4=16, GEN5=32 }` GT/s per-lane,正确单位是 GT/s 而非 MT/s;原 `25/50/100/200/400` 为 10× 错误的单位混淆)
- `lanes_active`: 通道宽度(1/2/4/8/16)
- `elec_idle`: 电气空闲状态(影响 FC 信用可用性)
- `training_state`: LTSSM 当前状态(11 状态 + Gen3+ 均衡子状态,**非 Gen5 扩展** — Oracle 修订 #8)

**Spec 引用**:
- PHY Interface for the PCI Express (PIPE) 5.1.1 Specification
- Synopsys PIPE 5.1.1 博客:`https://www.synopsys.com/blogs/chip-design/pipe-5-1-1-pcie-usb-sata.html`
- Cadence PIPE SerDes 博客:`https://community.cadence.com/cadence_blogs_8/b/fv/posts/pipe-serdes-architecture-for-pcie-gen-5-and-beyond`

**实现要点** (Oracle 修订 #5/#8):
```cpp
struct PciePipeSignal {
    // ⚠️ 单位修正: GT/s per-lane per-direction(不是 MB/s,不是 MT/s)
    // PCIe spec: Gen1=2.5, Gen2=5, Gen3=8, Gen4=16, Gen5=32 GT/s
    // 速率枚举仅指示 Gen 档位,精确值 ×10(Gen1=2 表示 2.5 GT/s)
    enum class Rate : uint8_t { GEN1=2, GEN2=5, GEN3=8, GEN4=16, GEN5=32 };
    enum class ElecIdle : uint8_t { IDLE, ACTIVE };

    // ⚠️ LTSSM 修订: 11 主状态中 "Hot-Plug" 不是 LTSSM 状态(属平台机制)
    // Hot_Reset 是 LTSSM 子状态(Recovery 内);L2/3_Ready 是 L2 子状态
    enum class TrainingState : uint8_t {
        DETECT, POLLING, CONFIG, L0, L0S, L1, L2,
        RECOVERY, DISABLED, LOOPBACK, HOT_RESET  // 11 个主状态(不含 HOT_PLUG)
    };

    Rate rate_;                 // 当前链路速率(Gen 档位枚举)
    uint8_t lanes_active_;      // 活跃通道数(1/2/4/8/16)
    ElecIdle elec_idle_;        // 电气空闲(影响 FC 信用可用性)
    TrainingState training_;    // LTSSM 主状态
    sc_time timestamp_;         // PHY 事件时间戳
};
```

### 实施时机

**Phase 3**:`cpptlm-dgpu-pcie-phy-digital-ctrl`(LTSSM + PIPE 4-signal 端)

---

## Q4: 开源 PCIe PHY BFM / IP 候选

### 选项对比

| 选项 | 候选 | License | Gen | 推荐策略 |
|---|---|---|---|---|
| **A** | 单 wrap `alexforencich/verilog-pcie` | MIT | Gen3/4 | ⚠️ 仅 wrap 复杂(Verilog → C++ TLM) |
| **B** | **混合策略**: Xilinx pcie-model fork + alexforencich wrap | Xilinx example code + MIT | Gen4 → Gen5 扩展 | ✅ **最优** |
| **C** | 重实现自研 PIPE BFM | N/A | Gen5 | ⚠️ 工作量大 |
| **D** | chili-chips-ba/openCologne-PCIE | BSD-3 | Gen1/2 | ⚠️ Gen 太旧 |

### ✅ 决策: **选项 B**(混合策略)

### 理由

**Xilinx pcie-model**(`https://github.com/Xilinx/pcie-model`):
- TLM 2.0 native,与 CppTLM StreamAdapter 模式**直接对应**
- 已有 6 个 BAR TLM initiator sockets + DMA target socket
- 结构示例:
  ```
  左侧 (PCIe 协议侧):
    TLM target socket  ← 接收 TLP
    TLM initiator socket → 发送 TLP
  右侧 (用户逻辑侧):
    bar0~5_init_socket → 6 个 BAR TLM initiator
    dma_tgt_socket ← DMA TLM target
    signals_irq → MSI-X 中断
  ```
- ✅ 作为 `PcieEndpointIP` 的 Link Layer + Transaction Layer **参考实现**
- 升级路径: Xilinx pcie-model 是 Gen4 → 我们扩展到 Gen5

**alexforencich/verilog-pcie**(`https://github.com/alexforencich/verilog-pcie`):
- MIT license, 1621 stars, **最成熟**的开源 PCIe
- cocotb testbenches + AXI Bridge + PIPE 接口
- ✅ Phase 7 通过 TLM2VFIO bridge 接入,**获得真实 DMA 行为 + cocotb 验证**
- 备选: wyvernSemi/pcievhost C model(纯 C,便于 wrap)

**chili-chips-ba/openCologne-PCIE**:
- 唯一开源 EP 软核 + VIP,但仅 Gen1/2,**仅做参考**

**实施路径**:
- **Phase 1-3**: 内部实现 PcieLinkLayer + PciePHY Digital Ctrl,参考 Xilinx pcie-model 的 TLM 接口设计
- **Phase 7 (可选)**: 用 TLM2VFIO bridge 接 alexforencich/verilog-pcie 做"真实 PHY"对比验证
- **不开新依赖**: CppTLM 不强制依赖上述 BFM;用户自选

### 实施时机

**Phase 1 + Phase 7**: 参考 Xilinx pcie-model(立即);wrap alexforencich(可选,Phase 7)

---

## Q5: 商业 dGPU PCIe EP 验证策略

### 选项对比

| 选项 | 描述 | 工作量 | 商业依赖 |
|---|---|---|---|
| **A** | 嵌入 Cadence/Synopsys VIP | 🔴 高 | ❌ license 不嵌入 |
| **B** | **reference 验证策略**(不嵌入) | 🟢 低 | ✅ 用户自选 |
| **C** | 自建全套 UVM/SystemVerilog 验证 | 🔴 极高 | ✅ 无 |

### ✅ 决策: **选项 B**

### 理由

商业 VIP license **不可嵌入 CppTLM**;但其验证策略可参考。

**行业关键发现**(Librarian 调研):

| 验证策略 | 来源 | 关键要点 |
|---|---|---|
| UVM VIP as passive monitor | Cadence DVCon Taiwan 2025 | PCIe Gen6 verification, FLIT format error + credit exhaustion debug |
| Virtual RC for software dev | Avery Design / Siemens VirtuaLAB | QEMU + SystemVerilog PCIe VIP co-simulation, **75% faster first linkup** |
| TLM 2.0 PCIe controller | Xilinx pcie-model / Synopsys Virtualizer VDKs | TLM blocking transport (`b_transport`), instruction-accurate |
| 学术 reference | arXiv:2505.15590 (2025-05) | Physical PCI Device → SystemC-TLM via VFIO,**480× speedup** |
| PCIe verification effort growth | WINTECHCON 2024 | "90-fold increase from PCIe 5.0 to PCIe 6.0" |

**CppTLM 集成策略**:
- 内部验证: Catch2 unit tests + cocotb Python tests + 端到端 E2E
- 参考验证: 文档说明如何用 Cadence VIP / Synopsys VIP / Xilinx pcie-model / QEMU vPCI 做 reference verification
- **不嵌入**商业 VIP,避免 license 问题

### 实施时机

**全程**: 在每个 phase 文档中提供 VIP reference verification 用例;不嵌入 CppTLM

---

## Q6: SR-IOV 端口架构

### 选项对比

| 选项 | 描述 | 端口数 | 复杂度 |
|---|---|---|---|
| **A** | 每 VF 独立 StreamAdapter: 16 VF × 4 ports = 64 ports | 64 | 🔴 端口爆炸 |
| **B** | **VF Pool**: 共享 1 个 MultiPortStreamAdapter,内部 stream_id 路由 | 17 (PF0 + 16 VF) | 🟡 中 |
| **C** | 单 StreamAdapter + 内部 mux | 4 | 🟡 中(但易错位) |

### ✅ 决策: **选项 B**(VF Pool)

### 理由

**端口爆炸问题**:
- 现有 `MultiPortStreamAdapter<ModuleT, ReqBundleT, RespBundleT, N>` 的 N 是编译期模板参数
- 64 ports 意味着 N=64 的模板实例,编译时间爆炸 + StreamAdapter 工厂复杂度爆炸
- `set_stream_adapter(adapters[64])` 链路连接配置不可维护

**VF Pool 设计**:
- 共享 1 个 MultiPortStreamAdapter(17 ports)
- 每个 port 对应一个 PF/VF:`port[0]=PF0, port[1]=VF0, ..., port[16]=VF15`
- 内部通过 `stream_id` 区分请求/响应
- per-VF 状态独立(Config Space / MSI-X / BAR / FC / Retry / Seq#)

**vs 方案 C 的优势**:
- 方案 C 单 adapter 内部 mux 在 16 VF 并发时易发生请求/响应错位
- VF Pool 每个 port 有独立请求/响应队列,**避免错位**

### 实施时机

**Phase 4**:`cpptlm-dgpu-pcie-sriov`(4 周,扩展现有 4 端口为 17 端口 VF Pool)

---

## Q7: Bypass Mux 三态架构

### 选项对比

| 选项 | 描述 | 复杂度 | 场景覆盖 |
|---|---|---|---|
| **A** | **3 态 + Host Bypass 独立**: Full / Bypass / Partial + 独立 HostBypassTLM | 🟡 中 | ✅ 全部场景 |
| **B** | 仅 2 态(Full / Bypass) | 🟢 低 | ⚠️ 缺 FC 反压场景 |
| **C** | 单 bypass mux + 配置开关 | 🟢 低 | ⚠️ 缺快速软件 bring-up |

### ✅ 决策: **选项 A**

### 理由

**两个正交概念**:

1. **Host Bypass** = Host 如何连接到 SoC(整体跳过 PCIe RC BFM + PCIe EP 数字逻辑)
2. **EP 内部 Bypass Mux** = PCIe EP 内部是否仿真 PHY 数字 / 链路层

两者互补,不能合并:
- Host Bypass 决定 SoC 接受 Host 数据的**方式**(快速 AXI vs PCIe TLP)
- EP Bypass Mux 决定 EP 内部数字逻辑的**仿真精度**

**模式矩阵**:

| 场景 | Host Bypass | EP Bypass Mux | 说明 |
|---|---|---|---|
| 完整 PCIe 链路训练 | ❌ | Full | 真实 PCIe 物理 |
| 性能分析 / 带宽 | ❌ | Full | 链路延迟精确 |
| 软件 bring-up(driver) | ✅ | Bypass | 跳过一切 |
| FC 反压 / VRAM 饱和 | ❌ | Partial | 保留 FC |
| 单元测试 / 集成测试 | ✅ | Bypass | 快速 setup |

**关键约束**(防状态泄漏,Oracle 修订 C6, 2026-09-01 — 同步 design §7 的 10 步完备版):
```cpp
class BypassMux {
    enum class DrainPolicy { GRACEFUL_DRAIN, IMMEDIATE_ABORT };
    DrainPolicy drain_policy_ = DrainPolicy::GRACEFUL_DRAIN;

    void apply_mode(BypassMode new_mode) {
        // 0. 通知对端(RC BFM / HostBypass)准备切换
        notify_peer_mode_change(new_mode);

        // 1. 暂停 DLLP/TLP 传输
        link_layer_.pause();

        // 2. 处理 in-flight TLP/AXI 事务(graceful drain 或 immediate abort)
        if (drain_policy_ == DrainPolicy::GRACEFUL_DRAIN)
            wait_for_in_flight_completion(sc_time(1, SC_US));
        else
            abort_in_flight_tlps();

        // 3. 清理 Retry Buffer(累积确认语义,清到 ACK seq 而非全清)
        link_layer_.retry_buf_.clear_to(seq_num_last_acked_);

        // 4. 重置 seq# 计数器(避免对端序列号失步)
        seq_num_tx_ = 0; seq_num_rx_expected_ = 0;

        // 5. 重置 FC Token Bucket(所有 VF/VC,无 refill — per Q2)
        fc_engine_.reset_all_buckets();

        // 6. Partial 模式守卫(§1 未初始化不能 flush)
        if (new_mode == BypassMode::PARTIAL && !phy_digital_ctrl_.is_initialized())
            throw std::logic_error("Partial mode requires PHY Digital Ctrl initialized");
        if (new_mode == BypassMode::FULL || new_mode == BypassMode::PARTIAL)
            phy_digital_ctrl_.flush_pipe_state();

        // 7. MSI-X pending 状态清理
        msix_table_.clear_all_pending();

        // 8. 提交新模式 + 通知对端
        mode_ = new_mode;
        notify_peer_mode_complete(new_mode);

        // 9. 恢复传输
        link_layer_.resume();
    }
};
```

### 实施时机

**Phase 3**(Bypass Mux 基础)+ **Phase 7**(HostBypassTLM 独立组件 + RootComplexTLM 镜像)

---

## Q8: 23 ABI 兼容性策略(Oracle 重大修订 #1, 2026-09-01)

### 选项对比

| 选项 | 描述 | 兼容性保证 |
|---|---|---|
| **A** | 加 `CPPTLM_PCIE_ENDPOINT_ABI_VERSION` 宏 + `noexcept` + `[[nodiscard]]` | 🟢 强(对源兼容);🟡 中(对二进制 ABI 无直接作用) |
| **B** | 仅加 noexcept | 🟡 中 |
| **C** | 不加任何保护 | 🔴 弱 |

### ✅ 决策: **选项 A**(修订版)

### 理由(Oracle 关键修正)

**23 ABI 的真实构成**(per `include/abi/cpptlm_emulator.h` 逐行核实):

23 ABI = **19 个 `extern "C"` 前向函数 + 4 个 callback typedef**

| 类别 | 数量 | 符号 |
|---|---|---|
| **前向函数** | 19 | `cpptlm_emulator_get_version` / `get_device_count` / `get_device_info` / `create` / `create_by_id` / `destroy` / `mmio_write` / `mmio_read` / `pcie_config_write` / `pcie_config_read` / `backdoor_read` / `backdoor_write` / `msix_init` / `msix_update_pending` / `msix_clear_pending` / `lookup_register` / `register_callbacks` / `register_backdoor_cb` / `register_dma_translate_cb` |
| **回调 typedef** | 4 | `cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t`(per `include/abi/cpptlm_emulator.h` Oracle C1 修订) |
| **总计** | **23** | (per ADR-088 §D5) |

**🔴 原设计错误(Oracle Top-1)**:
- `CPPTLM_PCIE_ENDPOINT_ABI_VERSION` 宏 + `noexcept` + `[[nodiscard]` 保护的是 `PcieEndpointTLM`/`PcieEndpointIP` 的 **C++ 类方法**
- 但 23 ABI 是 **`cpptlm_emulator.h` 的 C 符号**,与 C++ 类方法**完全不同对象**
- `noexcept` 不改变 C 符号 mangled name;`[[nodiscard]]` 不进 ABI;版本宏只影响 C++ 源码编译期,对 dlopen `libcpptlm_emulator.so` **零影响**
- 原版 `#if CPPTLM_PCIE_ENDPOINT_ABI_VERSION != 2 #error` 会**主动拒绝** v1 消费者编译,与 "向后兼容" 宣称矛盾

**具体措施(修订版)**:
1. **23 ABI 边界保护**(真正有效的部分)
   - 本 change **NOT modify** `include/abi/cpptlm_emulator.h`
   - 23 符号修改需走 ADR 流程(per project convention)
   - 任何修改 `cpptlm_emulator.h` 的 PR 必须新增 `cpptlm_emulator_abi_diff.md` 说明
2. **类方法纪律**(编译期源兼容,非二进制 ABI)
   - 新增 public 方法标 `noexcept`(若适用)
   - 查询方法标 `[[nodiscard]]`
   - 不修改 `PcieEndpointTLM` 现有类的成员布局(freeze for downstream v1 兼容)
3. **JSON params 子集兼容**(不删除旧字段,只扩展)
4. **CI 检查**:
   - `abi-compliance-checker` 对比 `cpptlm_emulator.so` v1 vs v2(对 23 C 符号)
   - C++ 类布局: `bloaty` 对比 `cpptlm_core.a` 符号表

### 实施时机

**Phase 1**(修订): ✅ 加 ABI 边界纪律;❌ **不**在旧 `PcieEndpointTLM` 加 `[[deprecated]]`(推迟到 PcieEndpointIP 可用,per Oracle Top-9)
**全程**: 每个 phase 提交前跑 ABI 检查

---

## Q9: AXI4Mapper 范围

### 选项对比

| 选项 | 描述 | 耦合 | 复杂度 |
|---|---|---|---|
| **A** | **独立模块 + JSON 可选注入** | 🟢 解耦 | 🟡 中 |
| **B** | 内嵌在 PcieEndpointIP | 🔴 紧耦合 | 🔴 高 |
| **C** | 不实现(用现成 Xilinx mapper) | 🟢 解耦 | 🟢 低(但失去控制) |

### ✅ 决策: **选项 A**

### 理由

**与 PcieEndpointIP 解耦的关键性**:
- AXI4 有 Outstanding transaction、Out-of-order、Outstanding 等复杂特性
- 若 AXI4Mapper 内嵌,`PcieEndpointIP` 复杂度爆炸
- 独立模块可在其他场景复用(CacheTLM/CrossbarTLM 都可受益)

**JSON 可选注入**:
```json
{
  "axi_adapter": {
    "axi4_mapper_inject": true,    // ← 配置开关
    "ports": {
      "axi_master_out": { "mapper": "axi4_default" }
    }
  }
}
```

**与 design.md §6 的关系**:
- `AxiStreamAdapter` 内部按 JSON 配置决定是否注入 AXI4Mapper
- 不注入时:`AxiStreamAdapter` 直接暴露 AXI 信号
- 注入时:`AxiStreamAdapter` ↔ `AXI4Mapper` ↔ AXI 信号

### 实施时机

**Phase 5**(`AxiStreamAdapter`)+ **Phase 6**(`Axi4Mapper`,独立模块)

---

## Q10-Q17: Oracle 评审补充决策(2026-09-01)

> 以下 8 项决策是 Oracle 评审 `bg_455593fe` 揭示的**原文档遗漏**,均已完成决议并落到对应 Phase。

### Q10: FLR(Function Level Reset)

**陈述**: PcieEndpointIP **SHALL** 实现简化 FLR — PF 全状态复位,VF 仅对应 VF 状态复位(per-VF Config Space + MSI-X + BAR + FC token bucket + Retry buffer + seq#)。

**理由**: PCIe SR-IOV spec **强制** PF 支持 FLR(VF 可选但 GPGPU 驱动常用);原 §8 未提,Phase 4 gate 会漏测。

**实施**: Phase 4,新增 `test_pcie_sriov_flr.cc`。

---

### Q11: 多 VC vs 单 VC

**陈述**: PcieEndpointIP **SHALL** 仅实现单 VC0(`vc_id` 字段保留但默认 0)。

**理由**: 多 VC 需 VC arbitration(Strict Priority / RR / WRR),增加复杂度超 SoC 仿真需求;单 VC 满足当前 dGPU SoC 拓扑。

**实施**: `PcieDllpBundle.vc_id` 字段保留(per 决策 Q2 字段宽度 4 bits),实际值固定 0;Phase 1 验证。

---

### Q12: Completion Timeout / Error Handling

**陈述**: PcieEndpointIP **SHALL NOT** 实现完整 Completion Timeout 错误模型(out-of-scope,UsrLinuxEmu 容错吸收);但事务层 **SHALL** 实现 **completion 匹配**(trans_id 关联)+ 溢出丢弃/上报策略。

**理由**: SR-IOV VF 隔离需要 completion tracking;否则 NP 请求与 Cpl 失联。完整超时/重传逻辑在 UsrLinuxEmu 端。

**实施**: Phase 4,新增 `test_pcie_sriov_completion_tracking.cc`。

---

### Q13: AER / ECRC

**陈述**: PcieEndpointIP **SHALL NOT** 实现 AER(Advanced Error Reporting)或 ECRC(End-to-end CRC)。

**理由**: SoC 仿真不消费这些能力;实现需复杂错误上报路径与 config space capability 扩展,远超仿真价值。

**实施**: 不实现;记录为正式 out-of-scope 决策。

---

### Q14: Surprise Removal 行为

**陈述**: 当 PRSNT# 检测到 surprise removal 时,PcieEndpointIP **SHALL** 执行 `drain_in_flight_with_timeout(1µs)` → `abort_remaining()` → `clear_msix_pending()` → 链路回 Detect 状态。

**理由**: PCIe 机箱规范定义 surprise removal 语义(对比 safe removal 的有序 shutdown);EP 端需仿真"热拔出时正在进行的 TLP 如何处置",否则热插拔 SM 不完整。

**实施**: Phase 3 扩展 Hot-Plug SM,新增 `test_pcie_phy_digital_hotplug_surprise_removal.cc`。

---

### Q15: 链路错误注入机制(Phase 1 必需)

**陈述**: P1 Link Layer **SHALL** 暴露 `link_error_injector_t` API,支持注入:
- `inject_nak(seq_num)` — 模拟接收 NAK DLLP
- `inject_dllp_loss()` — 模拟 DLLP 丢包
- `inject_tlp_loss(seq_num)` — 模拟 TLP 丢包

**理由**: P1-G4 "ACK/NAK 重传场景" 需要模拟丢包;无注入接口则 TDD 无法写测试。原文档未设计接口。

**实施**: Phase 1 T-P1-5 内新增,新建 `test_pcie_link_layer_error_injector.cc`。

---

### Q16: ASPM / L0s/L1 细节 / CLKREQ#

**陈述**: PcieEndpointIP **SHALL** 仅建模 L0s / L1 / L2 状态切换对**链路可用性**的影响(`elec_idle` 信号),**SHALL NOT** 实现 ASPM(Active State Power Management)协商或 CLKREQ# 信号。

**理由**: 避免 P3 范围膨胀;CLKREQ# 是平台/电源管理细节,SoC 仿真价值低。

**实施**: Phase 3 LTSSM 状态转换支持 L0s/L1/L2,但不实现进入/退出协商细节。

---

### Q17: 下行(host→EP)链路层覆盖范围

**陈述**: Phase 1 Link Layer **SHALL** 实现**双向**链路层:
- **下行 Rx**(host→EP TLP): §2 Link Layer 接收方向解析 DLLP/TLP 分流,DLLP 解析 ACK/NAK/InitFC/UpdateFC/NOP;TLP 解析后送 §3 事务层
- **下行 ACK 生成**: EP 收到 host TLP 后生成 ACK DLLP 发回 host(累积确认)
- **下行 Retry**: EP 收方向 retry buffer(per-vc/per-vf)
- **上行 Tx**(EP→host): §2 Tx Path 排序 + Tx retry(已规划)

**理由**: 原 §4 数据流图只画 slave_in → Tx Path,方向混淆,实现 half-duplex 风险;不决策则 P1 仅实现半边链路。

**实施**: Phase 1 T-P1-4 / T-P1-5 覆盖双向;新建 `test_pcie_link_layer_downstream_rx.cc`。

---

---

## 决策矩阵(总览)

| 决策点 | 推荐 | 理由摘要 | Phase |
|---|---|---|---|
| **Q1 编码** | C 透明+延迟 | SoC 不需要 bit-level;Gen5 ≠ FLIT | P2 |
| **Q2 FC** | B Token Bucket | backpressure 关键,credit 精确性冗余(修订:仅 UpdateFC 补充) | P1 |
| **Q3 PIPE** | C 4-signal | PIPE 5.1 SerDes 简化,TLM 不需 50-signal(修订:GT/s 枚举) | P3 |
| **Q4 BFM** | B 混合 | Xilinx pcie-model 直接对应;alexforencich wrap | P1+P7 |
| **Q5 验证** | B reference | 不嵌入商业 VIP | 全程 |
| **Q6 SR-IOV** | B VF Pool | 避免 64 端口爆炸(修订:理由非编译爆炸,是接入成本) | P4 |
| **Q7 Bypass** | A 3态+HostBypass | 两正交概念,场景全覆盖(修订:apply_mode 状态清理完备) | P3+P7 |
| **Q8 ABI** | A 宏+noexcept+nd | ADR-088 §D5 强制(修订:对象明确为 23 C 符号) | P1+全程 |
| **Q9 AXI4Mapper** | A 独立+可选 | 解耦,可复用(修订:OOO 需 `bid`/`rid` 字段) | P5+P6 |
| **Q10 FLR** | 简化 FLR(PF+Vx 状态) | SR-IOV spec 强制 | P4 |
| **Q11 多 VC** | 单 VC0 + 保留字段 | 避免 arbitration 复杂度 | P1+全程 |
| **Q12 Completion Timeout** | out-of-scope + TL completion 匹配 | SR-IOV VF 隔离 | P4 |
| **Q13 AER/ECRC** | NOT 实现 | SoC 仿真不消费 | out-of-scope |
| **Q14 Surprise Removal** | drain(1µs) + abort + 回 Detect | PCIe 机箱规范 | P3 |
| **Q15 错误注入接口** | link_error_injector_t API | P1-G4 TDD 必需 | P1 |
| **Q16 ASPM/CLKREQ#** | NOT 实现 | P3 范围控制 | P3 |
| **Q17 下行 Rx 链路层** | 双向(DLLP/TLP 分流 + Rx ACK) | 避免 half-duplex | P1 |

---

## 决策修订记录

| 日期 | 决策 | 旧值 | 新值 | 修订原因 |
|---|---|---|---|---|
| 2026-09-01 | Q1 | (待定: FLIT 模式) | C 透明 + 128b/130b 延迟 | **Librarian 揭示 FLIT 是 Gen6 而非 Gen5**;Oracle 初次咨询误判 |
| 2026-09-01 | Q4 | chips4all-pcie-phy / corescore-pcie | Xilinx pcie-model + alexforencich | Librarian 揭示上述候选为 Chisel/Scala,不适合 C++ wrap |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: ✅ Decided — 等待 OpenSpec 评审后,Phase 1 子 change 启动实施
