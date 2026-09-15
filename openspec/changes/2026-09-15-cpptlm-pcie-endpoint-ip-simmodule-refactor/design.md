# Design: cpptlm-pcie-endpoint-ip-simmodule-refactor — Phase 1+2

> **Status**: Proposed v1.0 (2026-09-15) — Oracle APPROVE_WITH_CONDITIONS（5 项条件已落入 §0.2 不变量）
> **关联 proposal**: `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/proposal.md`

---

## §0 边界与交叉引用

### 0.1 与既有 change 的所有权划分

**与 `2027-02-09-cpptlm-pcie-endpoint-ip-json-config`（archive, 2027-02-09）边界**：
- 既有 change 触碰：`attach_composition()` 内 4 类 JSON 消费扩展（`phy_digital` / `sr_iov` / `transaction_layer` / 未消费键 warning）
- 本 change 触碰：EP 基类切换 + composite 引入 + 静态注册表 shim
- **交集：无**——既有 change 落地 `attach_composition` 内部逻辑；本 change 用 composite 封装该逻辑，**接口签名/消费语义不变**

**与未来 `cpptlm-pcie-endpoint-ip-axiadapter-split`（规划中）边界**：
- 本 change Phase 1 + Phase 2 不拆 AxiAdapter
- AxiAdapter 拆分需先迁 EP::tick `cc:248-364` 的 AXI slave 业务逻辑进 child，独立 change 评估

**与 `2026-09-10-cpptlm-stage-1-4-2-1`（Proposed v1.1，BAR window）边界**：
- 本 change 不动 `bar_store_` / ResizableBar / INV-G 越界校验
- BAR 窗口归对方 change

### 0.2 不变量（强约束，落地各处）

| ID | 不变量 | 落地位置 |
|----|--------|----------|
| **INV-1** | `link_up(true)`（即 `phy->set_link_up(true)`）由 composite 构造后立即调用，EP 不再负责；封装 `pcie_endpoint_ip.cc:129` 原语义 | §1 composite 构造 |
| **INV-2** | `attach_composition` / `simulate_instantiate` 仅 composition-time 可调用（`configure_vectors` placement-new 销毁 pending IRQ，运行期调用会丢中断） | §3 Phase 2 EP 改造 |
| **INV-3** | EP `tick()` **必须 override 且显式定序**：AXI slave 处理 → composite 子模块 tick → 17 adapters tick；**禁止**依赖 SimModule 默认 unordered_map 迭代（`sim_module.hh:166`） | §3 Phase 2 EP 改造 |
| **INV-4** | LL↔PHY↔Mux 内部信号保持直接指针 + `std::function` sink（`pcie_link_layer_tlm.hh:164-169` `set_tlp_sink/set_dllp_sink` 模式）；**禁止** Bundle 化（rate switch / 10-step cleanup / INV-B ASPM exit 是 0-cycle 同步调用） | §1 composite + §6 Non-goals |
| **INV-5** | 23 ABI header 零触碰：`include/tlm/gpu/pcie_endpoint_tlm.h`（PcieEndpointTLM 4 端口冻结）+ `include/abi/cpptlm_emulator.h`（mmio_write 走 board shell `cpptlm_emulator.cc:245` 不经 EP 类布局） | 全程 |
| **INV-6** | 4 子模块静态注册表 API `attach_to_endpoint/for_endpoint/detach_from_endpoint` 行为保留；Phase 1 / Phase 2 内部委托实现可改，返回值语义不变（14+ 测试文件 + 冻结 PcieEndpointTLM 零修改） | §2 注册表 shim |

### 0.3 强耦合证据汇总（来自 Oracle R 输出 §2）

- **PHY→LL 同步调用**：`pcie_phy_digital_ctrl_tlm.hh:136-137` `link_layer_->trigger_rate_switch(rate_, to)` + `pcie_link_layer_tlm.hh:299-303` rate_switching_ 标志位 + wire busy 同步置位
- **PHY→Mux 同步调用**：`pcie_phy_digital_ctrl_tlm.hh:207-208` `mux_` 指针用于 Surprise Removal 清理
- **Mux→LL 10 步清理**：`pcie_bypass_mux.hh:132-137` `link_layer_->clear_retry_buffer()` / `reset_seq_counters()` / `reset_fc_buckets()` / `pause_link_layer()` / `resume_link_layer()`
- **EP::tick 当前不调 PHY**：观察 `pcie_endpoint_ip.cc:248-374` — PHY tick 由测试外部驱动；Phase 1 composite 须显式定义 PHY tick 顺序

### 0.4 风险矩阵（来自 Oracle R 输出 §6）

| 风险 ID | 描述 | 缓解 |
|--------|------|------|
| **R1 注册表兼容** | `for_endpoint` 被 src/tlm/gpu/pcie_endpoint_tlm.cc（冻结 EP）+ 14+ 测试文件依赖 | §2 shim 设计保证返回值语义不变；Phase 1 gate 14+ 文件零修改 |
| **R2 双注册残留** | `chstream_register.hh:82` object 注册若不删，module registry 虽优先命中（`module_factory.cc:266-269`），但 stale object 条目让 `getRegisteredObjectTypes()` 说谎 | §3 Phase 2 强制删 line 82；tasks.md 任务 2.1 显式 commit |
| **R3 tick 顺序漂移** | SimModule 默认 tick 遍历 unordered_map（`sim_module.hh:166`）非确定序；PHY tick 当前无 EP 侧调用者 | §3 INV-3 显式 override tick；tasks.md 任务 1.4 加顺序断言测试 |
| **R4 已规避（决策 C）** | 本 change **不引入**外层声明式 `pcie_ep.pf0`/`pcie_ep.vfN` 接线。composite 17 端口经 `getInternalOutputPort`/`getInternalInputPort`（`sim_module.hh:140-146`,命中 EP `internal_factory` + Step 7 mirror）程序化可达。Phase 8 实际靠 429327d HostBypass/RC `tick()` 程序化桥接工作,本 change 不破坏该路径。Phase 3 AxiAdapter 拆分有真实消费者时再统一暴露机制。 | tasks.md 任务 2.2 程序化可达性测试 + 任务 2.3 axislavein 桥接保留测试 |
| **R5 延迟不变量** | 保持 LL↔PHY↔Mux 直接指针 → 零新增 cycle；风险仅在后续"顺手" Bundle 化 | §6 Non-goals 显式列 |

---

## §1 Phase 1 — Composite 类 `PcieLinkPhyMuxTLM`

### 1.1 类定义

```cpp
// include/tlm/pcie/pcie_link_phy_mux_tlm.hh
#ifndef TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_HH
#define TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_HH

#include "core/chstream_module.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "framework/stream_adapter.hh"
#include "bundles/pcie_bundles_tlm.hh"
#include "bundles/pcie_dllp_bundles_tlm.hh"

#include <array>

namespace tlm::pcie {

class PcieLinkPhyMuxTLM : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_TLP_PORTS = 17;  // 1 PF + 16 VF

    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> req_in[NUM_TLP_PORTS];
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_TLP_PORTS];

    PcieLinkPhyMuxTLM(const std::string& name, EventQueue* eq);
    ~PcieLinkPhyMuxTLM() override = default;

    PcieLinkPhyMuxTLM(const PcieLinkPhyMuxTLM&) = delete;
    PcieLinkPhyMuxTLM& operator=(const PcieLinkPhyMuxTLM&) = delete;

    std::string get_module_type() const override { return "PcieLinkPhyMuxTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    unsigned num_ports() const override { return NUM_TLP_PORTS; }

    void init() override;
    void tick() override;
    void do_reset(const ResetConfig&) override;

    // 内部子模块访问器（composite → 子模块引用，供 EP 转发用）
    PcieLinkLayer& link() noexcept { return link_; }
    PciePhyDigitalCtrl& phy() noexcept { return phy_; }
    PcieBypassMux& mux() noexcept { return mux_; }
    const PcieLinkLayer& link() const noexcept { return link_; }
    const PciePhyDigitalCtrl& phy() const noexcept { return phy_; }
    const PcieBypassMux& mux() const noexcept { return mux_; }

    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const {
        return (idx < NUM_TLP_PORTS) ? adapters_[idx] : nullptr;
    }

private:
    // 成员声明顺序 = 构造顺序（INV-1: LL 先于 PHY/Mux）
    PcieLinkLayer link_;
    PciePhyDigitalCtrl phy_;
    PcieBypassMux mux_;

    cpptlm::StreamAdapterBase* adapters_[NUM_TLP_PORTS] = {nullptr};
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_HH
```

### 1.2 构造与初始化（INV-1 落地）

```cpp
// src/tlm/pcie/pcie_link_phy_mux_tlm.cc
PcieLinkPhyMuxTLM::PcieLinkPhyMuxTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq),
      link_(eq),
      phy_(eq),
      mux_(&link_) {           // 构造即持 link_ 指针
    // INV-1: link_up(true) 由 composite 内部调用
    phy_.link_layer(&link_);   // PHY 持 LL 指针
    phy_.set_link_up(true);    // link_up 立即置位
    mux_.set_phy_initialized(true);
}

void PcieLinkPhyMuxTLM::init() {
    ChStreamModuleBase::init();
    link_.reset_fc_buckets();  // 触发 LL 默认 FC 配置
    mux_.apply_mode(BypassMode::Full);  // 默认 Full mode
}
```

**关键决策**：原 `pcie_endpoint_ip.cc:125-141` 的 6 行组合逻辑（LL attach → PHY attach + `link_layer(ll)` + `set_link_up(true)` → Mux attach + `set_phy_initialized(phy != nullptr)`）**全部封装进 composite 构造函数**，EP 不再参与 INV-1 时序保证。

### 1.3 Tick 定序契约（Oracle R3 缓解 · **Phase 1 休眠**）

```cpp
void PcieLinkPhyMuxTLM::tick() {
    // INV-4: 内部保持直接指针，0-cycle 同步
    // PHY 先于 LL：PHY 的 LTSSM 状态机需驱动 LL 的速率切换前置检查
    phy_.tick();
    link_.tick();
    // mux 无显式 tick（状态机在 apply_mode 同步触发）

    // 17 TLP 端口 adapter 转发
    for (unsigned i = 0; i < NUM_TLP_PORTS; ++i) {
        if (adapters_[i]) {
            adapters_[i]->tick();
        }
    }
}
```

**Phase 1 休眠（Oracle 决策 a）**：原 EP::tick **不调** PHY（外部驱动如 `test_aspm.cc:67`）—— Phase 1 引入 composite 后，`composite->tick()` 实现完整定序（`phy → link → 17 adapters`）作为 Phase 2 契约，但 **Phase 1 期间无任何调用者**。EP::tick 继续走原路径（`for_endpoint(ll)->tick()` + EP 自身的 17 端口数组）。`composite->tick()` 在 Phase 2 任务 2.1 EP::tick override 激活时同步启用，彼时须**预算**外部 `phy->tick()` 测试驱动移除（tasks 2.1.5）。

**Phase 1 兼容性锁定**：`tasks.md 1.2` 新增 `test_ep_tick_does_not_drive_phy_phase1`，断言 EP::tick N 次不推进 PHY LTSSM 状态；Phase 2 翻转预期时仅改该断言方向。

测试方法：`composite` 头加 test-only `tick_counts_[3]` 数组（不污染冻结的 3 个子模块 .h；Phase 1 可观察 PHY/LL tick 顺序；Phase 2 释放该 test-only 钩子或加 `#ifdef CPPTLM_TESTING` 包裹）。

### 1.4 EP 改造（Phase 1：基类不动 · composite 单一所有权）

> **composite 单一所有权（Phase 1）**：composite 由 `PcieLinkPhyMuxTLM::attach_to_endpoint(ep_name, eq)` 静态 API 创建并插入 `static unordered_map<name, unique_ptr<PcieLinkPhyMuxTLM>>`；EP **不持有** unique_ptr，**仅持 raw observer 指针**（从静态 API 返回）。EP dtor 显式 `detach_from_endpoint(ep_name)` 清理静态注册表。严禁 EP 与静态注册表双持 unique_ptr（防 double-delete）。

```cpp
// include/tlm/pcie/pcie_endpoint_ip.hh — Phase 1 改动
// (基类仍 ChStreamModuleBase, 不动 REGISTER_CHSTREAM)

public:
    ~PcieEndpointIP() override;  // 显式声明 dtor(用于 detach_from_endpoint)
    PcieEndpointIP(const PcieEndpointIP&) = delete;
    PcieEndpointIP& operator=(const PcieEndpointIP&) = delete;

private:
    // 单一所有权归 PcieLinkPhyMuxTLM 静态注册表, EP 仅持 raw observer
    tlm::pcie::PcieLinkPhyMuxTLM* composite_ = nullptr;

    // 显式析构:从静态注册表移除 (避免 stale 指针跨 TEST_CASE)
    // (实现于 .cc, 见 §1.5)

// accessors 转发到 composite
PcieLinkLayer* link_layer() const noexcept {
    return composite_ ? &composite_->link() : nullptr;
}
PciePhyDigitalCtrl* phy() const noexcept {
    return composite_ ? &composite_->phy() : nullptr;
}
PcieBypassMux* bypass_mux() const noexcept {
    return composite_ ? &composite_->mux() : nullptr;
}
```

### 1.5 `attach_composition` 改造（Phase 1 · 完整 JSON 字段映射）

```cpp
// src/tlm/pcie/pcie_endpoint_ip.cc — Phase 1
PcieEndpointIP::~PcieEndpointIP() {
    // 显式清理静态注册表(防 stale 指针跨 TEST_CASE / 多 EP 共存)
    if (composite_) {
        tlm::pcie::PcieLinkPhyMuxTLM::detach_from_endpoint(getName());
        composite_ = nullptr;
    }
}

void PcieEndpointIP::on_config_loaded() {
    const auto& params = get_config();
    attach_composition(params);
}

void PcieEndpointIP::attach_composition(const json& params) {
    config_warnings_.clear();

    // Phase 1: composite 惰性构造（Oracle 复审条件 2:仅 link_layer.enabled=true 时,
    // 与现状 attach 语义逐点对齐,保持 for_endpoint→null 语义）
    if (!composite_ && params.contains("link_layer") &&
        params["link_layer"].value("enabled", true)) {
        composite_ = tlm::pcie::PcieLinkPhyMuxTLM::attach_to_endpoint(
            getName(), event_queue);
        // composite_ 是 raw pointer,所有权在静态注册表内
    }

    // 1. AXI adapter（保持原 attach_to_endpoint shim）
    if (params.contains("axi_adapter")) {
        auto* ax = PcieAxiAdapter::attach_to_endpoint(getName(), event_queue);
        if (ax) {
            ax->set_endpoint(this);
            const auto& axi_json = params["axi_adapter"];
            ax->set_mapper_injected(axi_json.value("axi4_mapper_inject", false));
        }
    } else {
        PcieAxiAdapter::detach_from_endpoint(getName());
    }

    // 2. link_layer 配置（composite 惰性构造:仅 link_layer.enabled=true 时,与现状 attach 语义逐点对齐)
    //    完整字段映射(修复 P1-2/P1-3 + Oracle 复审条件 1/2):
    //    - enabled: 现状 cc:112-115 在 enabled=false 时 detach 三者 → for_endpoint 返回 null;
    //      composite 惰性构造保持该 null 语义(仅 enabled=true 时 attach_to_endpoint)
    //    - fc_token_bucket_capacity: 新增 setter set_fc_capacity (Oracle 复审条件 1:
    //      FcTokenBucket::capacity_ 仅构造期可设,update_fc 不覆盖 capacity)
    //    - fc_initial_credit_{p,np,cpl}: 既有 update_fc API
    //    - retry_buffer_size: 新增 setter set_retry_buffer_size (proposal §Impact 例外)
    //    - link_error_injection_enabled: 新增 setter set_link_error_injection_enabled
    //      (无害增量:现状 EP 不从 link_layer.* 消费该键,struct 注释指向 link_error_injection.enabled 块)
    //    - bypass_mode: composite->mux().apply_mode(...) (原 pcie_endpoint_ip.cc 消费路径)
    if (params.contains("link_layer")) {
        const auto& ll_json = params["link_layer"];
        const bool enabled = ll_json.value("enabled", true);
        if (!enabled || !composite_) {
            // Oracle 复审条件 2: 惰性构造,保持 for_endpoint→null 现状语义
            // (enabled=false 或无 link_layer 块时不构造 composite)
            if (!enabled && composite_) {
                // R-B 修补:重配置 enabled=false 时必须 detach 静态注册表
                // 否则 composite-first shim 仍返回非 null,违反 for_endpoint→null 现状
                tlm::pcie::PcieLinkPhyMuxTLM::detach_from_endpoint(getName());
                composite_ = nullptr;
            }
        } else {
            // FC tokens (既有 API)
            composite_->link().update_fc(
                FcTokenBucket::Type::P,
                ll_json.value("fc_initial_credit_p", 256u));
            composite_->link().update_fc(
                FcTokenBucket::Type::NP,
                ll_json.value("fc_initial_credit_np", 256u));
            composite_->link().update_fc(
                FcTokenBucket::Type::Cpl,
                ll_json.value("fc_initial_credit_cpl", 256u));
            // FC capacity (新增 setter, Oracle 复审条件 1)
            composite_->link().set_fc_capacity(
                ll_json.value("fc_token_bucket_capacity", 256u));
            // retry buffer size (新增 setter, proposal §Impact 例外)
            composite_->link().set_retry_buffer_size(
                ll_json.value("retry_buffer_size", 4096u));
            // error injection flag (新增 setter, 无害增量)
            composite_->link().set_link_error_injection_enabled(
                ll_json.value("link_error_injection_enabled", false));
        }
        // bypass_mode 顶层 link_layer.bypass_mode (原 cc 路径)
        // R-A 修补:仅 composite_ 非空时消费(enabled=true + composite 已构造)
        if (composite_ && ll_json.contains("bypass_mode")) {
            const std::string mode_str = ll_json["bypass_mode"].get<std::string>();
            BypassMode mode = BypassMode::Full;  // 默认
            if (mode_str == "Bypass") mode = BypassMode::Bypass;
            else if (mode_str == "Partial") mode = BypassMode::Partial;
            // 注: 原 cc:118-122 还支持顶层 params["bypass_mode"], 同步处理
            composite_->mux().apply_mode(mode);
        }
    }

    // 3. phy_digital（composite 内 phy_）
    if (params.contains("phy_digital") && composite_) {
        const auto& pj = params["phy_digital"];
        auto cfg = composite_->phy().config();
        if (pj.contains("preset_p")) cfg.preset_P = pj.value("preset_p", cfg.preset_P);
        if (pj.contains("preset_np")) cfg.preset_NP = pj.value("preset_np", cfg.preset_NP);
        if (pj.contains("preset_cpl")) cfg.preset_Cpl = pj.value("preset_cpl", cfg.preset_Cpl);
        if (pj.contains("hot_plug_supported")) cfg.hot_plug_supported = pj.value("hot_plug_supported", cfg.hot_plug_supported);
        composite_->phy().set_config(cfg);
    }

    // 4. sr_iov / transaction_layer（pool_ 仍为 EP 值成员，保持原逻辑）
    // ... 原 attach_composition cc:170-207 逻辑不变

    // 5. 顶层 bypass_mode (兼容原 cc 路径, 也可放 link_layer 下)
    if (params.contains("bypass_mode") && composite_) {
        const std::string mode_str = params["bypass_mode"].get<std::string>();
        BypassMode mode = BypassMode::Full;
        if (mode_str == "Bypass") mode = BypassMode::Bypass;
        else if (mode_str == "Partial") mode = BypassMode::Partial;
        composite_->mux().apply_mode(mode);
    }

    warn_unconsumed(params, { "axi_adapter", "link_layer", "phy_digital", "sr_iov",
                             "transaction_layer", "pm_cap_control", "bypass_mode" });
}
```

**关键**：Phase 1 `attach_composition` 经 `PcieLinkPhyMuxTLM::attach_to_endpoint` 静态 API 获取 composite raw pointer（单一所有权归静态注册表），所有 4 子模块配置改为通过 composite 成员访问。**保留** 4 个静态注册表 `attach_to_endpoint` 调用作 shim（向后兼容），但实际访问通过 composite。

**JSON 消费语义保证**：7 类键（`pm_cap_control` / `axi_adapter` / `link_layer` / `phy_digital` / `sr_iov` / `transaction_layer` / 顶层 `bypass_mode`）逐字段映射；其中 `retry_buffer_size` 与 `link_error_injection_enabled` 需 `pcie_link_layer_tlm.hh` 新增 public setter（proposal §Impact 例外允许，仅 API 扩展不修改布局）。

---

## §2 静态注册表 shim 设计（INV-6 落地 · composite 单一所有权）

### 2.1 shim 实现原则

- 4 子模块的静态 `attach_to_endpoint(name, eq)` 行为保持：返回指向子模块的指针
- **Phase 1 单一所有权**：`PcieLinkPhyMuxTLM::attach_to_endpoint(ep_name, eq)` 创建 `unique_ptr<PcieLinkPhyMuxTLM>` 插入 `static unordered_map<name, unique_ptr<PcieLinkPhyMuxTLM>>`,返回 raw pointer;EP 持 raw observer,不持 unique_ptr(防 double-delete)
- **Phase 2 单一所有权迁移**：composite 改为由 EP 的 `internal_factory` 持有;`PcieLinkPhyMuxTLM::for_endpoint` 改为 wrapper 调 `PcieEndpointIP::find_composite(ep_name)`;`attach_to_endpoint`/`detach_from_endpoint` 加 `[[deprecated]]`,保留 API 至 Phase 3 清理
- 测试 `PcieLinkLayer::for_endpoint(name)`、`phy()`、`bypass_mux()` 等调用语义不变

### 2.2 EP name → composite 映射（Phase 1 静态 + Phase 2 internal_factory）

```cpp
// include/tlm/pcie/pcie_link_phy_mux_tlm.hh — static API
class PcieLinkPhyMuxTLM {
public:
    static PcieLinkPhyMuxTLM* for_endpoint(const std::string& ep_name) noexcept;
    static PcieLinkPhyMuxTLM* attach_to_endpoint(const std::string& ep_name, EventQueue* eq);
    static void detach_from_endpoint(const std::string& ep_name) noexcept;

private:
    // Phase 1 单一所有权(EP 不持 unique_ptr, 仅 raw observer)
    static std::unordered_map<std::string, std::unique_ptr<PcieLinkPhyMuxTLM>>& registry();
};

// include/tlm/pcie/pcie_endpoint_ip.hh — Phase 2 静态辅助
class PcieEndpointIP : public SimModule {
private:
    static std::vector<PcieEndpointIP*>& instances_();  // ctor/dtor 维护
    static PcieLinkPhyMuxTLM* find_composite(const std::string& ep_name) noexcept;
};
```

**Phase 1**: `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` 直接查静态注册表 → 返回 raw pointer。EP 通过 `attach_to_endpoint` 取得 raw pointer 存 `composite_` raw observer。

**Phase 2**: `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` → `PcieEndpointIP::find_composite(ep_name)` → 扫描 `instances_()` 列表 → 找到匹配的 EP → `ep->internal_factory->getInstance(ep_name + "_lpm")`。`PcieEndpointIP` ctor/dtor 维护 `instances_()` 列表（自动 register/unregister）。

### 2.3 子模块静态注册表的"双查找"（composite-first + legacy-fallback）

`PcieLinkLayer::for_endpoint(ep_name)` 在 Phase 1 + Phase 2 维持双查找：
- **composite-first**：若 `PcieLinkPhyMuxTLM::for_endpoint(ep_name) != nullptr`,返回 `&composite->link()`(Phase 1) 或 `&composite->link_`(Phase 2 由 internal_factory 持有)
- **legacy fallback**：回落到原 `attach_to_endpoint` 注册表（保持冻结 PcieEndpointTLM 兼容；`PcieEndpointTLM` 不在 EP 实例列表中,`find_composite` 自然 miss）

```cpp
// src/tlm/pcie/pcie_link_layer_tlm.cc — for_endpoint 改造(Phase 1 + Phase 2 一致)
PcieLinkLayer* PcieLinkLayer::for_endpoint(const std::string& ep_name) noexcept {
    if (auto* lpm = PcieLinkPhyMuxTLM::for_endpoint(ep_name)) {
        return &lpm->link();
    }
    auto& reg = link_registry();
    auto it = reg.find(ep_name);
    return (it != reg.end()) ? it->second.get() : nullptr;
}
```

### 2.4 兼容性矩阵

| 调用方 | 当前 | Phase 1 | Phase 2 | 行为变化 |
|--------|------|---------|---------|---------|
| `src/tlm/gpu/pcie_endpoint_tlm.cc` 冻结 EP `for_endpoint(name)` | 查独立注册表 | composite-first → fallback | 同 Phase 1(走 fallback) | ✅ 无变化 |
| `test/test_pcie_endpoint_ip_*.cc` 14+ 文件 | 同上 | 同上 | 同上 | ✅ 无变化 |
| `test_pcie_endpoint_ip_full_e2e.cc` | 同上 | 同上 | 同上 | ✅ 无变化 |
| EP `attach_composition` 内部 | 调 4 次 `attach_to_endpoint` | 构造 composite via `attach_to_endpoint` raw pointer + 保留其他子模块 `attach_to_endpoint` 调用为 shim | `simulate_instantiate` 经 `internal_factory->instantiateAll(wrap)` 创建 composite,不再用 `attach_to_endpoint` | Phase 1 行为零变化,Phase 2 路径切换 |

**Phase 1 + Phase 2 子模块静态注册表保留**——是为了 Phase 3 删除时不会触发 14+ 测试文件级联修改 + PcieEndpointTLM 兼容。

---

## §3 Phase 2 — EP 基类切换 + REGISTER_MODULE 迁移

### 3.1 基类切换（Oracle R2 关键缓解）

```cpp
// include/tlm/pcie/pcie_endpoint_ip.hh
// Phase 2: 基类切换
#include "core/sim_module.hh"
#include "core/module_factory.hh"
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"  // composite 类型前向

class PcieEndpointIP : public SimModule {  // 改：ChStreamModuleBase → SimModule
public:
    static constexpr unsigned NUM_PORTS = 17;
    // ... 17 TLP 端口由 composite 子模块持有(EP 自身无端口数组, R4 决策 C)

    explicit PcieEndpointIP(const std::string& n, EventQueue* eq);
    ~PcieEndpointIP() override;

    std::string get_module_type() const override { return "PcieEndpointIP"; }

    // Phase 2 替换 on_config_loaded → simulate_instantiate
    void simulate_instantiate(const json& cfg) override;
    void tick() override;  // INV-3 显式定序 (Phase 2 激活 composite tick)

    void incorporate_parent(SimModule* parent) override;  // late-binding

    // 静态辅助 (Phase 2 single-ownership 迁移需要)
    static std::vector<PcieEndpointIP*>& instances_() noexcept;
    static tlm::pcie::PcieLinkPhyMuxTLM* find_composite(const std::string& ep_name) noexcept;

private:
    // 移除 Phase 1 的 composite_ raw observer (所有权归 internal_factory)
};
```

### 3.2 REGISTER_MODULE 迁移 + 双注册清理

```cpp
// include/chstream_register.hh — 删除 line 82
// ModuleFactory::registerObject<tlm::pcie::PcieEndpointIP>("PcieEndpointIP");  // ❌ 删除
// ❌ 删除 line 130-131 registerMultiPortAdapter<PcieEndpointIP, ..., 17>

// 新增：composite 注册
ModuleFactory::registerObject<tlm::pcie::PcieLinkPhyMuxTLM>("PcieLinkPhyMuxTLM");
ChStreamAdapterFactory::get()
    .registerMultiPortAdapter<tlm::pcie::PcieLinkPhyMuxTLM, 
                              bundles::PcieTlpBundle, 
                              bundles::PcieTlpBundle, 
                              17>("PcieLinkPhyMuxTLM");
```

```cpp
// include/modules_cluster.hh — 新增 PcieEndpointIP 注册
#include "tlm/pcie/pcie_endpoint_ip.hh"
REGISTER_MODULE(PcieEndpointIP);
```

### 3.3 `simulate_instantiate` 实现（INV-2 落地 · R4 决策 C）

> **R4 决策 C 修订**：本 change **不引入**外层 JSON `pcie_ep.pf0` 接线。composite 17 端口经 `getInternalOutputPort`/`getInternalInputPort` 程序化可达（`sim_module.hh:140-146` 命中 EP `internal_factory` + Step 7 mirror）。`addInputConfig`/`addOutputConfig` 在 `sim_module.hh` 仅 `#ifdef CPPTLM_TESTING` 暴露（line 193-196），生产构建编译失败，**禁止**在 `simulate_instantiate` 调用。

```cpp
// src/tlm/pcie/pcie_endpoint_ip.cc
void PcieEndpointIP::simulate_instantiate(const json& cfg) {
    if (internal_factory && !internal_factory->getAllInstances().empty()) {
        return;  // 幂等守卫
    }

    nlohmann::json wrap;
    wrap["modules"] = nlohmann::json::array();
    wrap["connections"] = nlohmann::json::array();

    // 构造 composite 子模块（持有 17 TLP 端口 + LL/PHY/Mux）
    //    composite 由 internal_factory 持有(单一所有权 Phase 2)
    nlohmann::json composite_cfg;
    composite_cfg["name"] = getName() + "_lpm";
    composite_cfg["type"] = "PcieLinkPhyMuxTLM";
    composite_cfg["params"] = cfg.value("params", nlohmann::json::object());
    wrap["modules"].push_back(composite_cfg);

    // 递归 instantiate:composite 在 internal_factory 中构造 + Step 7 mirror 17 端口
    internal_factory->instantiateAll(wrap);

    // 调原 attach_composition 完成 JSON 消费语义保持(此时 composite 已存在)
    if (cfg.contains("params")) {
        attach_composition(cfg["params"]);
    }

    // 注册 EP 实例到静态 instances_ 列表(供 PcieLinkPhyMuxTLM::for_endpoint 扫描)
    instances_().push_back(this);
}
```

**EP ctor/dtor 维护**：
```cpp
PcieEndpointIP::PcieEndpointIP(const std::string& n, EventQueue* eq)
    : SimModule(n, eq) {
    instances_().push_back(this);  // ctor 注册
}

PcieEndpointIP::~PcieEndpointIP() {
    auto& v = instances_();
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
    // internal_factory 由基类 SimModule dtor 清理(composite 随 internal_factory 析构)
}
```

### 3.4 EP `tick()` override（INV-3 严格落地 · Phase 2 行为变更）

```cpp
// src/tlm/pcie/pcie_endpoint_ip.cc
void PcieEndpointIP::tick() {
    // 1. AXI slave 处理（保留 cc:255-364 语义）
    if (auto* ax = PcieAxiAdapter::for_endpoint(getName())) {
        cpptlm::Axi4StreamAdapter& axi = ax->axi();
        if (axi.slave_req_valid()) {
            // ... 原处理逻辑（AXI slave config/BAR 分发）
        }
    }

    // 2. composite 子模块 tick（包含 LL/PHY/Mux + 17 adapters）
    //    [Phase 2 行为变更]: PHY tick 驱动权从测试外部迁移到 EP 此处
    //    对应 test_aspm.cc 等外部 phy->tick() 调用必须修改 (tasks 2.1.5)
    for (auto& [name, obj_ptr] : internal_factory->getAllInstances()) {
        if (obj_ptr) {
            obj_ptr->tick();  // composite::tick 内部已定序 phy → link → adapters
        }
    }
}
```

**显式定序保证**：不依赖 SimModule 默认 `unordered_map` 迭代顺序（Oracle R3）。

**Phase 2 行为变更审计**：tasks 2.1.5 `grep -rn "phy.*->tick()\|phy.*\.tick()" test/ | grep -v simmodule_refactor` 列出所有外部 PHY tick 驱动点，逐个移除（用 fixture `setup_phy_state()` 替代）。

### 3.5 外层 JSON 声明式接线（Oracle R4 决策 C 不承诺）

> **R4 决策 C**:本 change **不实现**外层 JSON `connections: [{ "src": "host_bypass.tlp_out[0]", "dst": "pcie_ep.pf0" }]` 路径。原因：composite 住在 EP `internal_factory`,**不在外层 `object_instances`**;`connection_resolver.cc:47-98` 对外层 `ep.<label>` → 内部 `<composite>.<port>` 的两层下钻**无递归 `getInternalOutputPort` 解析支持**(只查外层 object_instances),连接会被静默丢弃。

**Phase 8 数据路径**(不受影响):实际靠 429327d `HostBypassTLM/RC::tick()` 程序化桥接工作(直接调 `PcieAxiAdapter::for_endpoint(ep_name)->axi()`),`examples/dgpu_soc_with_pcie_ip.json` 的 `pcie_ep.axi_slave_in`/`pcie_ep.axi_master_out` 接线虽在 Phase 2 后被 connection_resolver 静默丢弃(EP 入 `module_instances`, `findInternalPath("axi_slave_in")==""` 无 fallthrough),但程序化桥接路径完整。`test_axislavein_bridge_path_intact`(tasks 2.3)锁定此不变性。

**未来扩展**(Phase 3 AxiAdapter split):若需外层声明式 TLP 接线,需扩展 `connection_resolver.cc` 加 `SimModule::getInternalOutputPort` 递归解析——属独立 change。

---

## §5 静态注册表 API 兼容矩阵

| API | Phase 1 | Phase 2 | Phase 3+ |
|-----|---------|---------|---------|
| `PcieLinkLayer::for_endpoint(ep_name)` | ✅ shim（composite-first → legacy fallback） | 同 Phase 1（composite 改为 internal_factory 路径,API 一致） | 评估 |
| `PciePhyDigitalCtrl::for_endpoint(ep_name)` | ✅ shim | 同 Phase 1 | 评估 |
| `PcieBypassMux::for_endpoint(ep_name)` | ✅ shim | 同 Phase 1 | 评估 |
| `PcieAxiAdapter::for_endpoint(ep_name)` | ✅ shim（仅 axi 自身注册表） | 同 Phase 1 | Phase 3 拆分为 ChStream child 后可能废弃 |
| `PcieAxiAdapter::attach_to_endpoint(ep_name, eq)` | ✅ shim | 同 Phase 1 | 同上 |
| `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` | ✅ 直接查静态注册表 | ✅ wrapper→`PcieEndpointIP::find_composite(ep_name)`(扫描 EP 实例 + `internal_factory->getInstance`) | 评估(Phase 3 全清理) |
| `PcieLinkPhyMuxTLM::attach_to_endpoint(ep_name, eq)` | ✅ 创建 + 插注册表 | `[[deprecated]]` 仅 Phase 1 用 | 删除 |
| `PcieLinkPhyMuxTLM::detach_from_endpoint(ep_name)` | ✅ 注册表清理 | `[[deprecated]]` 仅 Phase 1 用 | 删除 |

**冻结 PcieEndpointTLM（`src/tlm/gpu/pcie_endpoint_tlm.cc`）继续走 legacy fallback 路径**，与本 change 的 composite 隔离；`PcieEndpointTLM` 不在 EP 实例列表中（不注册到 `instances_()`），`find_composite` 自然 miss → fallback。

---

## §6 Non-goals（Oracle INV-4 显式落地 · R4 决策 C）

1. **LL↔PHY↔Mux 内部信号 Bundle 化**：rate switch / 10-step cleanup / INV-B ASPM exit 是 0-cycle 同步调用（`pcie_phy_digital_ctrl_tlm.hh:136-137` `link_layer_->trigger_rate_switch`）；Bundle 化必然引入 ≥1 cycle 延迟，违反 INV-B
2. **VfPool 拆分为独立 ChStream child**：Oracle R 明确驳回；`pool_` 是 EP 值成员 + EP::tick 数据路径每拍访问（`pcie_endpoint_ip.cc:280,296,336,340`），拆分为模块只会让所有热路径调用穿过跨模块指针，零收益纯成本
3. **PcieEndpointTLM（4 端口冻结 legacy）任何修改**：23 ABI 冻结头
4. **23 ABI header `include/abi/cpptlm_emulator.h` 任何修改**：mmio_write ABI 走 board shell，不经 EP 类布局
5. **JSON `params` 消费语义变更**：7 类键（`pm_cap_control` / `axi_adapter` / `link_layer` / `phy_digital` / `sr_iov` / `transaction_layer` / 顶层 `bypass_mode`）保持原 `attach_composition` 消费逻辑不变
6. **AxiAdapter 拆分为 ChStream child**：Phase 3 独立 change；本 change Phase 1 + Phase 2 不触及
7. **第 18 个及以上 TLP 端口**：17 端口是 Phase 4 SR-IOV 冻结约束，扩展需另行 spec
8. **Axi4Mapper 内部重构**：Phase 6 已 ship，本 change 不动
9. **外层 JSON `pcie_ep.pf0`/`pcie_ep.vfN` 声明式接线**（R4 决策 C）：composite 17 端口仅程序化可达（`getInternalOutputPort`）；外层 connection_resolver.cc 不支持两层下钻,扩展属 Phase 3 独立 change
10. **`addInputConfig`/`addOutputConfig` 调用**：该 API 在 `sim_module.hh` 仅 `#ifdef CPPTLM_TESTING` 暴露,生产编译失败;本 change 不使用,Phase 3+ 若需外层 JSON 暴露则须先解除 `CPPTLM_TESTING` 包裹或扩 `parsePortConfigs`
11. **Phase 1 EP::tick 驱动 PHY**：Oracle 决策 a,保持测试外部驱动;Phase 2 任务 2.1.5 审计并迁移(已开预算,违反 Phase 1 "零修改"承诺但 Phase 2 已接受)

---

---

## §7 文档同步清单

| 文件 | 改动 |
|------|------|
| `docs/architecture/14-pcie-ip-microarchitecture.md` | §8-§10 更新:composite 类设计图 + composite 17 端口内部可达性（程序化 API,R4 决策 C）+ tick 顺序契约（Phase 1 休眠 + Phase 2 激活）;新增"已知限制"段(R4 决策 C 不支持外层 pf0/vfN 接线) |
| `AGENTS.md` STRUCTURE 节 | `include/tlm/pcie/pcie_link_phy_mux_tlm.hh` 加入 PCIe EP 微架构表格 |
| `include/AGENTS.md` 注册宏体系表 | `PcieEndpointIP` 从 `REGISTER_CHSTREAM` 迁移到 `REGISTER_MODULE` 标注 |
| `openspec/specs/pcie-ip-microarch/spec.md` | MODIFIED：新增 `pcie-endpoint-ip-composition` Requirement 段 |

---

## §8 总览图

```
                  ┌──────────────────────────────────────────┐
   外层 JSON      │  ModuleFactory::instantiateAll          │
   connections:  │   Step 0-1: validate / extends          │
    {             │   Step 2:  registry lookup              │
      "src":      │     "PcieEndpointIP" 命中 module registry│
        "(无外层 │   Step 4.5: simulate_instantiate(ep)    │
        TLP 接   │     ↓                                   │
        线,R4=C)"│   ┌────────────────────────────────────┐ │
      "dst":      │   │ PcieEndpointIP : SimModule         │ │
        "..."     │   │   simulate_instantiate(cfg):       │ │
    }             │   │     1. 构造 wrap JSON              │ │
                  │   │     2. instantiateAll(wrap) 内部   │ │
                  │   │     3. attach_composition JSON消费 │ │
                  │   │     4. instances_().push_back(this)│ │
                  │   └────────────────────────────────────┘ │
                  │     ↓                                    │
                  │   ┌────────────────────────────────────┐ │
                  │   │ internal_factory (ModuleFactory) │ │
                  │   │   child: PcieLinkPhyMuxTLM        │ │
                  │   │     (17-port MultiPortAdapter)    │ │
                  │   │     ├─ PcieLinkLayer link_       │ │
                  │   │     ├─ PciePhyDigitalCtrl phy_   │ │
                  │   │     │    └─ link_layer(&link_)   │ │
                  │   │     └─ PcieBypassMux mux_       │ │
                  │   │          └─ link_layer(&link_)   │ │
                  │   │   tick(): phy→link→adapters      │ │
                  │   │     (Phase 1 休眠;Phase 2 激活) │ │
                  │   └────────────────────────────────────┘ │
                  │     ↓                                    │
                  │   Step 5-7: ConnectionResolver           │
                  │     (本 change 不声明外层 TLP 接线,    │
                  │      R4 决策 C;composite 17 端口经     │
                  │      getInternalOutputPort 程序化可达)  │
                  │   Step 9: incorporate_parent hook        │
                  └──────────────────────────────────────────┘
                                   ↓
                   PcieEndpointIP::tick() override (INV-3, Phase 2):
                     1. AXI slave 处理 (for_endpoint)
                     2. composite->tick() (phy→link→adapters)
                     [Phase 2 行为变更:PHY tick 驱动权迁移,见 tasks 2.1.5]
                   ```
