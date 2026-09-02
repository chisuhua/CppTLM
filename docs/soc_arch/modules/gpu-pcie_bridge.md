# gpu-pcie_bridge 微架构文档

> **类别**: gpu > pcie_bridge
> **状态**: 🟡 规划中（Phase 7 备选 dGPU）
> **Header**: (规划) `include/tlm/gpu/pcie_bridge_tlm.hh`
> **注册**: (规划) `REGISTER_CHSTREAM` 扩展 `ModuleFactory::registerObject<tlm::PCIBridgeTLM>("PCIBridgeTLM")`
> **蓝图来源**: gem5 `src/dev/amdgpu/amdgpu_device.py` + `src/dev/pci/pci_host.py`
> **首版 commit**: 🟡 蓝图（来自调研 §4 Phase 2 备选） · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4, §4 Phase 2 备选
> - 通用 GPU 概念: [gpu.common.md](./gpu.common.md)

---

## 1. 设计目标（规划）

`tlm::PCIBridgeTLM` 是 **Phase 7 备选（dGPU 形态）**的 **PCIe 桥接模块**——连接**两个独立的 coherence 域**（CPU 一套 + GPU 一套），模拟 gem5 `configs/example/gpufs/Disjoint_VIPER.py` 的双独立 NoC 拓扑。

**核心职能**：
- **跨域桥接**（CPU 域 ↔ GPU 域，via PCIe）
- **延迟模拟**（100-500 cycles，可配置）
- **BAR 路由**（BAR0 = VRAM / BAR2 = doorbell / BAR5 = MMIO — **三组**按 D5 决策）
- **DMA 路径**（CPU 域 ↔ GPU 域之间的批量数据传输）

**与 gem5 对位**: `gem5::AMDGPUDevice`（PCIe 设备）+ `gem5::PciHost`（PCI 宿主总线控制器）。

## 2. 架构概览（规划）

```
   CPU 域 (CoherenceDomain="cpu_domain")            GPU 域 (CoherenceDomain="gpu_domain")
   ──────────────────────────                    ──────────────────────────
         │                                              │
         ▼                                              ▼
   ┌──────────────────┐                         ┌──────────────────┐
   │ CrossbarTLM      │                         │ TCC_TLM           │
   │ (cpu side)       │                         │ (gpu side)        │
   └────────┬─────────┘                         └────────┬─────────┘
            │                                            │
            ▼                                            ▼
   ┌──────────────────────────────────────────────────────────────────┐
   │                     PCIBridgeTLM                                  │
   │                                                                  │
   │   ┌────────────────────┐        ┌────────────────────┐         │
   │   │ CPU 侧 PCIe port    │◄──────►│ GPU 侧 PCIe port   │         │
   │   │ - req/resp 通道     │  100-   │ - req/resp 通道     │         │
   │   │ - DMA channel       │  500    │ - DMA channel       │         │
   │   │ - BAR0/2/5 路由    │  cycles │ - BAR0/2/5 路由    │         │
   │   └────────────────────┘  delay  └────────────────────┘         │
   │                                                                  │
   │   ┌──────────────────────────────────────────────────┐         │
   │   │  BAR 路由表                                       │         │
   │   │   - BAR0 (VRAM)  → 转发到 GPU 域 memory        │         │
   │   │   - BAR2 (doorbell) → 转发到 GPU 域 HSAPP    │         │
   │   │   - BAR5 (MMIO)   → 转发到 GPU 域 PCC          │         │
   │   └──────────────────────────────────────────────────┘         │
   └──────────────────────────────────────────────────────────────────┘
            │                                            │
            ▼                                            ▼
   CPU 域 (DDR)                                  GPU 域 (HBM2)
```

## 3. 接口（规划）

```cpp
namespace tlm {

// BAR 类型（按 D5 决策分三组）
enum class PCIeBarType {
    VRAM,        // BAR0: 设备内存映射（巨型 frame buffer）
    DOORBELL,    // BAR2: host → GPU 唤醒信号 mmio
    MMIO         // BAR5: 设备控制寄存器（PCIe capability 等）
};

// BAR 路由条目
struct PCIeBar {
    uint64_t base_addr;
    uint64_t size;
    PCIeBarType type;
    uint32_t target_node;  // 转发到哪个 GPU 节点
};

class PCIBridgeTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_PCIE_LATENCY = 200;  // cycles
    static constexpr uint32_t MAX_BARS = 16;

    explicit PCIBridgeTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "PCIBridgeTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_pcie_latency(uint32_t cyc) { pcie_latency_ = cyc; }
    void add_bar(const PCIeBar& bar) { bars_.push_back(bar); }

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

    // === BAR 路由 ===
    PCIeBarType lookup_bar_type(uint64_t addr) const;
    uint32_t lookup_target_node(uint64_t addr) const;

private:
    void handle_cpu_to_gpu_request(const ComputeReqBundle& req);
    void handle_gpu_to_cpu_response(const ComputeRespBundle& resp);

    // PCIe ports
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>   cpu_req_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle> cpu_resp_out_;
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>   gpu_req_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle> gpu_resp_out_;

    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    uint32_t pcie_latency_ = DEFAULT_PCIE_LATENCY;
    std::vector<PCIeBar> bars_;

    // 延迟模拟 (PCIe 是高延迟设备)
    struct PendingTransaction {
        uint64_t arrival_cycle;
        uint64_t target_node;
        bool is_write;
    };
    std::queue<PendingTransaction> pending_txns_;

    // 统计
    tlm_stats::Scalar cpu_to_gpu_requests_;
    tlm_stats::Scalar gpu_to_cpu_responses_;
    tlm_stats::Distribution pcie_latency_distribution_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 3 阶段

```cpp
void PCIBridgeTLM::tick() {
    // 1. 响应消费 (双向)
    if (cpu_resp_out_.ready()) {  // CPU 侧响应
        // 转发到 GPU 侧
    }
    if (gpu_resp_out_.ready()) {  // GPU 侧响应
        // 转发到 CPU 侧
    }

    // 2. PCIe 延迟模拟 (FIFO 出队)
    while (!pending_txns_.empty() &&
           pending_txns_.front().arrival_cycle <= getCurrentCycle()) {
        auto tx = pending_txns_.front();
        pending_txns_.pop();
        // ... 实际转发到目标域 ...
        pcie_latency_distribution_.sample(getCurrentCycle() - tx.arrival_cycle + pcie_latency_);
    }

    // 3. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 BAR 路由

```cpp
PCIeBarType PCIBridgeTLM::lookup_bar_type(uint64_t addr) const {
    for (const auto& bar : bars_) {
        if (addr >= bar.base_addr && addr < bar.base_addr + bar.size) {
            return bar.type;
        }
    }
    return PCIeBarType::MMIO;  // 默认
}
```

### 4.3 关键设计取舍

- **100-500 cycle 延迟**（PCIe 是高延迟设备）
- **三组 BAR**（D5 决策：VRAM/Doorbell/MMIO）
- **跨域 CoherenceDomain 桥接**——v0 仅字符串映射（与 CoherenceDomain API 一致）
- **DMA 通道**——v0 简化（不模拟）
- **PCIe atomic requestor**——v0 不启用（gem5 `PXCAPDevCtrl2=0x40` 不模拟）
- **MSI 中断**——v0 不模拟（`device.device_ih` 不实现）

## 5. Bundle 字段使用（规划）

**ComputeReqBundle / ComputeRespBundle**（与 GPU 域其他模块一致）：

| 字段 | PCIBridgeTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——跨域 ID 关联 |
| `address` | **关键**——BAR 路由决策 |
| `is_write` | 透传 |
| `data` | 透传 |
| `size` | 透传 |
| `kernel_id` | 透传（GPU 内核标识） |
| 其他 | 透传 |

## 6. 蓝图对齐

- gem5 `src/dev/amdgpu/amdgpu_device.py`（AMDGPUDevice PCIe 设备）
- gem5 `src/dev/pci/pci_host.py`（PciHost 宿主总线）
- gem5 `src/dev/amdgpu/viper_shader.py:124-165`（`_setup_device`，含 BAR0/2/5 设置）
- gem5 `configs/example/gpufs/Disjoint_VIPER.py`（Disjoint Network 配置）
- 调研 §2.4 蓝图来源 + §4 Phase 2 备选

## 7. 实施路径

### 7.1 Phase 7 备选 dGPU 步骤

> **触发条件**: Phase 7.A-F 全部完成 + APU 形态满足 + 评估 ROI 后启动

1. 新建 `include/tlm/gpu/pcie_bridge_tlm.hh`（~250 行）
2. 修改 `include/chstream_register.hh`：
   - 加 `#include "tlm/gpu/pcie_bridge_tlm.hh"`
   - 加 `ModuleFactory::registerObject<tlm::PCIBridgeTLM>("PCIBridgeTLM");`
   - 加 `ChStreamAdapterFactory::registerAdapter<tlm::PCIBridgeTLM, ComputeReqBundle, ComputeRespBundle>("PCIBridgeTLM");`（每个 PCIe port 独立注册）
3. 写 PCIe 延迟模拟（FIFO 出队）
4. 写 BAR 路由表
5. 跨域 CoherenceDomain 桥接真实实现（与 Phase 7.C `CoherenceDomain::register_bridge` 集成）
6. 加 Catch2 测试：`test/test_pcie_bridge.cc`
7. 新增 `configs/dgpu_demo.json`（dGPU 端到端配置）
8. 更新 `AGENTS.md` + `docs/ONBOARDING.md`（PCIBridgeTLM 注册条目）
9. 更新 `docs/soc_arch/modules/README.md`（新增 `gpu-pcie_bridge.md` 链接）
10. 更新 `roadmap.md` Phase 7 备选章节

### 7.2 验收标准

- [ ] 编译通过（Release + Debug）
- [ ] `cpptlm_tests "[gpu]"` 全部通过
- [ ] `cpptlm --config configs/dgpu_demo.json` 端到端可执行
- [ ] `docs_sync_check.sh --strict` 通过
- [ ] 零 TODO/FIXME/XXX in new files
- [ ] PCIe 延迟 = `pcie_latency_distribution_.mean() >= pcie_latency_`

### 7.3 估计工作量

- 设计: 1-2 周
- 实施: 1-2 周
- 测试: 0.5-1 周
- 文档: 0.5 周
- **总计: 3-4 周**（调研 §4 Phase 2 备选 4-6 周估算范围内）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **Phase 7 APU-first 路径可能不需要 dGPU**——ROI 低，v0 永不实施 | 中 | 低 | 蓝图先行；Phase 7.F 后评估 |
| R2 | **跨域 CoherenceDomain 桥接复杂度**——Phase 7.C 必须先完成 | 中 | 高 | Phase 7.C 优先级（最高风险，roadmap §Phase 7.C） |
| R3 | **PCIe atomic requestor 行为**——v0 简化（不模拟） | 低 | 低 | 与 gem5 v25.1 KVM/Atomic 行为一致 |
| R4 | **BAR 大小冲突**——同一 BAR 范围多 bar 配置 | 中 | 中 | v0 加错检测；on_config_loaded 验证 |
| R5 | **DMA 通道**——v0 不模拟，真实 dGPU 工作负载依赖 | 中 | 中 | Phase 7.D+ 补 DMA 通道 |
| R6 | **MSI 中断**——v0 不模拟 | 低 | 低 | Phase 7.F+ 补 |
| R7 | **PCIe 延迟硬编码** | 低 | 低 | on_config_loaded 暴露 `pcie_latency` |
| R8 | **DISJOINT_VIPER 拓扑**——v0 仅 1 个 PCIeBridge，多 GPU 节点需多个 | 中 | 中 | Phase 7.D+ 数组化（`num_gpu_nodes`） |

## 9. 设计决策点

### D1 PCIe port 注册方式

- **Q**: 4 个 port（cpu_req_in/cpu_resp_out/gpu_req_in/gpu_resp_out）注册 1 个还是 2 个 adapter？
- **状态**: 留待 Phase 7 备选设计时确定
- **建议**: 2 个 adapter（cpu side + gpu side 独立），类似 NICTLM 模式
- **依赖**: `ChStreamAdapterFactory::registerAdapter` 单端口限制

### D2 BAR 路由与 CoherenceDomain 关系

- **Q**: BAR 路由由 PCIeBridgeTLM 内部维护，还是查询 CoherenceDomain？
- **状态**: 留待 Phase 7 备选设计时确定
- **建议**: 内部维护（v0 简化），Phase 7.C 完整实现时查询
- **依赖**: Phase 7.C CoherenceDomain 真实实现

### D3 dGPU 端 cache 处理

- **Q**: dGPU 是否有 L1/L2 cache（TCC）？v0 模拟哪一级？
- **状态**: 留待 Phase 7 备选设计时确定
- **建议**: TCC（GPU L2）必须有；TCP（GPU L1）由 ComputeUnitTLM 内部处理
- **依赖**: Phase 7.D TCC_TLM

### D4 PCIe 与 coherency 关系

- **Q**: PCIe 跨域请求是否参与 CPU coherence 协议？
- **状态**: 留待 Phase 7 备选设计时确定
- **建议**: 参与（CPU 域 MOESI 域通过 PCIe 与 GPU 域 VIPER 域桥接）
- **依赖**: gem5 `MOESI_AMD_Base` ↔ `GPU_VIPER` 桥接

### D5 MSIX 中断支持

- **Q**: v0 PCIeBridge 是否模拟 MSIX 中断？
- **状态**: 留待 Phase 7 备选设计时确定
- **建议**: 不模拟（Phase 7.F+ 渐进增强）
- **依赖**: 与 gem5 `AMDGPUDevice.device.device_ih` 对位

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自调研 §4 Phase 2 备选）
- **2026-06-11**: B3 批次设计 — 提取 D1-D5 + 蓝图对齐 + 三组 BAR 决策
- **Phase 7 备选 dGPU (触发条件)**: Phase 7.A-F 全部完成 + ROI 评估通过后启动
- **未来 v2.2+**: MSIX 中断 + DMA 通道 + 真实 PCIe atomic
