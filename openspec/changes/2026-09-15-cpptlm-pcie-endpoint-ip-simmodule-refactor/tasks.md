# Tasks: cpptlm-pcie-endpoint-ip-simmodule-refactor — Phase 1+2

> **工期**: 2 工作日 (~17h 含缓冲,R4 决策 C 后从 20h 缩减) | **TDD 5 步结构** | **3 commit**（per AGENTS.md 零债务原则）
> **关键决策（Oracle 复评）**: R4 = C（取消外层 pf0/vfN 暴露）· Phase 1 PHY tick = a（composite->tick() Phase 1 无调用者）
> **Oracle 状态**: APPROVE_WITH_CONDITIONS（5 项条件已落入 design.md §0.2 INV-1..6）

---

## §0 边界与交叉引用

- 与 `openspec/changes/2027-02-09-cpptlm-pcie-endpoint-ip-json-config`（archive, 2027-02-09）边界 — 本 change 不动 JSON 消费语义；`on_config_loaded → simulate_instantiate` 是同语义接口切换
- 与 Phase 3 AxiAdapter 拆分（`cpptlm-pcie-endpoint-ip-axiadapter-split`，规划中）边界 — 本 change Phase 1 + Phase 2 不拆 AxiAdapter
- 与 `2026-09-10-cpptlm-stage-1-4-2-1`（Proposed v1.1，BAR window）边界 — 不重叠（BAR 窗口归对方）
- 与冻结 `src/tlm/gpu/pcie_endpoint_tlm.cc` 边界 — 该文件继续走静态注册表 fallback 路径，本 change 不引入对其的修改

> **VfPool 拆分 out of scope**：见 proposal.md §3 + design.md §0
> **LL↔PHY↔Mux Bundle 化 out of scope**：见 design.md INV-4
> **23 ABI header 零触碰**：见 design.md INV-5

---

## §1 Phase 1 — Composite 骨架 + 静态注册表 shim

### 任务 1.1 — `PcieLinkPhyMuxTLM` 头文件

- [x] **Write test**: `test/test_pcie_endpoint_ip_simmodule_refactor.cc::test_composite_construction_order`
  - ASSERT: 构造 `PcieLinkPhyMuxTLM` 后, `link()` / `phy()` / `mux()` 三个访问器均返回非 null 引用
  - ASSERT: `phy().link_layer() == &link()` （INV-1 成员指针绑定）
  - ASSERT: `phy().is_link_up() == true` （INV-1 link_up 立即置位）
  - ASSERT: `mux().link_layer() == &link()`
- [x] **Create**: `include/tlm/pcie/pcie_link_phy_mux_tlm.hh`
  - 类定义（per `design.md §1.1`）：`PcieLinkPhyMuxTLM : public ChStreamModuleBase`
  - 私有成员声明顺序：`PcieLinkLayer link_` → `PciePhyDigitalCtrl phy_` → `PcieBypassMux mux_`（构造顺序 = 成员声明顺序）
  - 17 端口 `req_in[17]` / `resp_out[17]`（复用 `bundles::PcieTlpBundle` 类型）
  - 静态注册表：`for_endpoint` / `attach_to_endpoint` / `detach_from_endpoint`
  - 访问器：`link() / phy() / mux() / get_adapter(i)`
- [x] **Verify**: `./build/bin/cpptlm_tests "[pcie-simmodule-refactor][composite]"` 全绿

### 任务 1.2 — `PcieLinkPhyMuxTLM` 实现（INV-1 落地 · Phase 1 tick 休眠）

> **Oracle 决策 a**:Phase 1 composite 自身实现完整 tick(`phy → link → 17 adapters`),但**无任何调用者**——EP::tick 走原路径(`for_endpoint(ll)->tick() + 17 adapters`),composite 仅承载配置存储 + 静态注册表 shim 目标。PHY tick 激活迁移到 Phase 2 任务 2.1 EP tick override 时发生。

- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_composite_tick_order_phase1`
  - ASSERT: **直接调用** `composite->tick()`(不经 EP),`phy_.tick()` 计数 +1 在 `link_.tick()` 计数 +1 之前（Oracle R3 顺序契约）
  - 验证方法:composite 头加 test-only `tick_counts_` 数组(不污染冻结的 3 子模块 .h)
- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_ep_tick_does_not_drive_phy_phase1`  ← **新增兼容性锁定测试**
  - ASSERT: 构造 EP + `link_layer.enabled=true` 配置后,调 `ep.tick()` N 次,PHY LTSSM 状态**未推进**(用公开 API `phy().state()` 返回 `LtState`,Oracle 复审条件 3:不得用 private `training_step_`/不存在的 `ltssm_state_`)
  - ASSERT: 对照组:外部 `composite->phy().tick()` 一次后 `phy().state()` 推进
  - 目的:锁定 Phase 1 "14+ 文件零修改" 承诺的 PHY 维度可执行证据;Phase 2 翻转预期时只需改本断言方向
- [x] **Create**: `src/tlm/pcie/pcie_link_phy_mux_tlm.cc`
  - 构造函数:`link_(eq), phy_(eq), mux_(&link_)` + `phy_.link_layer(&link_); phy_.set_link_up(true); mux_.set_phy_initialized(true);`(封装 `pcie_endpoint_ip.cc:129-141` 语义,INV-1 落地)
  - `tick()`: `phy_.tick() → link_.tick() → mux_` → 17 adapters tick(**Phase 1 无调用者**,Phase 2 任务 2.1 激活)
  - `init()`: 触发 link FC reset + mux Full mode
  - 静态注册表实现:`static std::unordered_map<std::string, std::unique_ptr<PcieLinkPhyMuxTLM>>& registry()`
  - header 注释: `// Phase 1 无调用者——EP::tick 不经 composite;Phase 2 task 2.1 EP tick override 时激活`
- [x] **Verify**:
  - `[pcie-simmodule-refactor][tick-order]` + `[pcie-simmodule-refactor][phy-dormant]` 全绿
  - `[pcie]` 全量回归 44498 assertions 不回归(test_aspm.cc 等 PHY tick 外部驱动测试不破)

### 任务 1.3 — EP `attach_composition` 改造 + 静态注册表 shim（INV-6 · composite 单一所有权）

> **composite 单一所有权（Phase 1）**:composite 由 `PcieLinkPhyMuxTLM::attach_to_endpoint(ep_name, eq)` 静态 API 创建并插入 `static unordered_map<name, unique_ptr<PcieLinkPhyMuxTLM>>`;EP **不持有** unique_ptr,仅持 raw observer 指针(从静态 API 返回);EP dtor 显式 `PcieLinkPhyMuxTLM::detach_from_endpoint(ep_name)` 清理静态注册表。**严禁** EP 与静态注册表双持 unique_ptr(防 double-delete)。

- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_static_registry_shim`
  - ASSERT: 构造 EP 后, `PcieLinkLayer::for_endpoint(ep_name)` 返回非 null（composite 路径）
  - ASSERT: `PciePhyDigitalCtrl::for_endpoint(ep_name)` 返回 `&composite->phy()`
  - ASSERT: `PcieBypassMux::for_endpoint(ep_name)` 返回 `&composite->mux()`
  - ASSERT: 14+ 既有测试文件 (`test_pcie_endpoint_ip_*.cc`) **零修改**通过
  - ASSERT: 同一 EP name 多次 `detach_from_endpoint` + `attach_to_endpoint` 后,`for_endpoint` 指向最新实例(无 stale 指针跨 TEST_CASE)
- [x] **Modify**: `include/tlm/pcie/pcie_endpoint_ip.hh`
  - 新增 `tlm::pcie::PcieLinkPhyMuxTLM* composite_ = nullptr;` 私有 raw observer 成员
  - **不**持 unique_ptr(单一所有权归静态注册表)
  - `link_layer() / phy() / bypass_mux()` 转发到 `composite_->link() / phy() / mux()`
  - 显式声明 dtor(用于 `detach_from_endpoint`);默认拷贝/移动 = delete
- [x] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - `attach_composition` 改造(per design §1.5 修订):
    - **composite 惰性构造**(Oracle 复审条件 2):仅当 `params.contains("link_layer") && link_layer.enabled=true` 时调 `PcieLinkPhyMuxTLM::attach_to_endpoint(getName(), event_queue)` 得 raw pointer 存入 `composite_`(**不** `make_unique`);`enabled=false` 或无 `link_layer` 块时不构造,保持 `for_endpoint→null` 现状语义
    - 7 类 JSON 消费语义保持不变,且**新增**完整字段映射（修复 P1-2/P1-3 + Oracle 复审条件 1）:
      - `link_layer.enabled` → 惰性构造开关(见上)
      - `link_layer.fc_initial_credit_p/np/cpl` → 既有 `update_fc` API
      - `link_layer.fc_token_bucket_capacity` → 新增 `set_fc_capacity`(**Oracle 复审条件 1**:`FcTokenBucket::capacity_` 仅构造期可设,`update_fc` 不覆盖)
      - `link_layer.retry_buffer_size` / `link_error_injection_enabled` → 新增 `set_retry_buffer_size` / `set_link_error_injection_enabled`(proposal §Impact 例外)
      - `link_layer.bypass_mode` → `mux_.apply_mode(...)`(原 `pcie_endpoint_ip.cc` 消费路径)
      - `phy_digital.preset_p/preset_np/preset_cpl/hot_plug_supported` → `phy_.set_config(...)`
      - `pm_cap_control` / `axi_adapter` / `sr_iov` / `transaction_layer` 保持原消费逻辑
  - **禁止** `static FILE* diag` 残留;test pass 后立即清理
- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_json_consumption_7keys`
  - ASSERT: 非默认 `fc_token_bucket_capacity`(如 512) 经 `set_fc_capacity` 生效并可从 LL 查询(Oracle 复审条件 1 验证)
  - ASSERT: 非默认 `retry_buffer_size` 生效
  - ASSERT: `link_layer.enabled=false` 时 `PcieLinkLayer::for_endpoint(ep_name) == nullptr`(现状语义保持)
  - ASSERT: 无 `link_layer` 块时 `for_endpoint(ep_name) == nullptr`
  - ASSERT: `bypass_mode` 经 `mux()` 查询确认 mode 生效
- [x] **Modify**: `include/tlm/pcie/pcie_link_layer_tlm.hh`(proposal §Impact 例外)
  - **新增** public setter:`void set_retry_buffer_size(std::size_t);` / `void set_link_error_injection_enabled(bool);`
  - **不**修改现有 public/private 成员签名或布局
- [x] **Modify**: 4 个子模块静态注册表 `for_endpoint` 实现为 composite-first + fallback（per `design.md §2.3`）
  - `src/tlm/pcie/pcie_link_layer_tlm.cc::for_endpoint`
  - `src/tlm/pcie/pcie_phy_digital_ctrl_tlm.cc::for_endpoint`
  - `src/tlm/pcie/pcie_bypass_mux.cc::for_endpoint`
  - `src/tlm/pcie/pcie_axi_adapter_tlm.cc::for_endpoint`（仅 axi 自身注册表保留，无 composite 路径）
- [x] **Verify**:
  - `[pcie-simmodule-refactor][shim]` + `[pcie-simmodule-refactor][json-consumption]`(新增 7 类键逐字段断言)全绿
  - `[pcie]` 全量回归 44498 assertions
  - 14+ `test_pcie_endpoint_ip_*.cc` 文件**零修改**编译通过
- [x] **Commit (CppTLM)**: `feat(pcie): introduce PcieLinkPhyMuxTLM composite + static-registry shim (phase-1 dormant tick)`

### 任务 1.4 — Phase 1 全量回归 + Oracle gate

- [x] **Run**: `./test.sh --mode off --quick`（含 `[pcie]` Catch2 全量 + Python demo）
- [x] **Run**: `python3 examples/demo_pcie_full_e2e.py`
- [x] **Run**: `openspec validate 2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor --strict`（提案文件结构校验）
- [x] **Verify**:
  - 44498 assertions 全绿
  - 14+ 注册表依赖测试文件零修改
  - composite 在 binary 内: `strings build/bin/cpptlm_tests | grep -c "PcieLinkPhyMuxTLM"` ≥ 1
  - 诊断残留检查: `grep -rn "static FILE\* diag" include/ src/` 输出空

---

## §2 Phase 2 — EP 基类切换 + REGISTER_MODULE 迁移

### 任务 2.1 — EP 基类切换 + 双注册清理（Oracle R2 关键 · composite 所有权迁移）

> **composite 单一所有权（Phase 2 迁移）**:composite 从 Phase 1 的 `static unordered_map<name, unique_ptr<PcieLinkPhyMuxTLM>>` 迁移到 EP 的 `internal_factory->object_instances`(via `internal_factory->instantiateAll(wrap)`)。Phase 2 后 EP **不持有** composite raw pointer,访问统一经 `internal_factory->getInstance(composite_name)`。**静态注册表 `PcieLinkPhyMuxTLM::attach_to_endpoint/detach_from_endpoint` 在 Phase 2 后弃用**(`[[deprecated]]` 标注,保留 API 兼容至 Phase 3 清理);`PcieLinkPhyMuxTLM::for_endpoint` 改为 wrapper,内部调 `PcieEndpointIP::find_composite(ep_name)`(扫描 EP 实例静态列表 + `internal_factory->getInstance`)。

- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_ep_inheritance_changed`
  - ASSERT: `PcieEndpointIP` 实例可通过 `dynamic_cast<SimModule*>` 转换成功
  - ASSERT: `PcieEndpointIP` 实例**不可**通过 `dynamic_cast<ChStreamModuleBase*>` 转换（基类已切换）
  - ASSERT: `ModuleFactory::getRegisteredModuleTypes()` 含 `"PcieEndpointIP"`
  - ASSERT: `ModuleFactory::getRegisteredObjectTypes()` **不**含 `"PcieEndpointIP"`（R2 缓解：双注册清理）
- [x] **Modify**: `include/tlm/pcie/pcie_endpoint_ip.hh`
  - 基类切换：`ChStreamModuleBase` → `SimModule`
  - 移除 17 端口数组（改由 composite 子模块暴露）
  - 移除 `set_stream_adapter` override（composite 接管）
  - **移除** `composite_` raw observer 成员(单一所有权归 internal_factory)
  - 添加 `simulate_instantiate(const json& cfg) override` 声明
  - 添加 `tick() override` 声明（INV-3,激活 composite tick + 迁移 PHY tick 驱动权,见任务 2.1.5）
  - 添加 `static std::vector<PcieEndpointIP*>& instances_()` 内部辅助(EP ctor/dtor 维护,供 `find_composite` 扫描)
  - 添加 `static PcieLinkPhyMuxTLM* find_composite(const std::string& ep_name)` 静态方法
- [x] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - 实现 `simulate_instantiate`(per design §3.3 修订版):
    - 构造 wrap JSON `{"modules": [{"name": ep_name+"_lpm", "type": "PcieLinkPhyMuxTLM", "params": cfg["params"]}]}`
    - `internal_factory->instantiateAll(wrap)` 创建 composite(Step 7 mirror 17 端口)
    - **不**再调 `addInputConfig/addOutputConfig`(R4 决策 C:外层 JSON 接线不在 scope;该 API 还在 `CPPTLM_TESTING` 包裹,生产编译失败)
    - `internal_factory->getInstance(composite_name)` → 缓存 `composite_` raw pointer(可选,仅供 `tick()` 显式调用;非必要可省略,`tick` override 中直接 `internal_factory->getAllInstances()`)
    - 调用 `attach_composition(cfg["params"])` 完成 JSON 消费(此时 composite 已存在,`composite_->link()` 等安全)
  - 实现 `tick() override`(per design §3.4 + 修订):
    ```cpp
    void PcieEndpointIP::tick() {
        // 1. AXI slave 处理(保留 cc:255-364 语义)
        // 2. composite 子模块 tick(包含 LL/PHY/Mux + 17 adapters)
        //    PHY tick 驱动权从 Phase 1 测试外部迁移到 EP 此处;
        //    对应 test_aspm.cc 等外部 phy->tick() 调用需移除(本任务 2.1.5)
        for (auto& [name, obj_ptr] : internal_factory->getAllInstances()) {
            if (obj_ptr) obj_ptr->tick();
        }
    }
    ```
  - 保留 `attach_composition(json)` 私有方法
- [x] **Modify**: `include/chstream_register.hh`
  - 删除 line 82: `ModuleFactory::registerObject<tlm::pcie::PcieEndpointIP>("PcieEndpointIP");`
  - 删除 line 130-131: `registerMultiPortAdapter<PcieEndpointIP, ..., 17>`
  - 新增: `ModuleFactory::registerObject<tlm::pcie::PcieLinkPhyMuxTLM>("PcieLinkPhyMuxTLM");`
  - 新增: `registerMultiPortAdapter<PcieLinkPhyMuxTLM, PcieTlpBundle, PcieTlpBundle, 17>("PcieLinkPhyMuxTLM");`
- [x] **Modify**: `include/modules_cluster.hh`
  - 新增: `#include "tlm/pcie/pcie_endpoint_ip.hh"` + `REGISTER_MODULE(PcieEndpointIP);`
- [x] **Modify**: `include/tlm/pcie/pcie_link_phy_mux_tlm.hh`
  - `attach_to_endpoint` / `detach_from_endpoint` 加 `[[deprecated("Phase 2 composite 迁 internal_factory;Phase 3 清理")]]`
  - `for_endpoint` 实现改为调 `PcieEndpointIP::find_composite(ep_name)`
- [x] **Modify**: 4 个子模块 `for_endpoint` 实现更新(`.cc` only, `.hh` 布局不变):
  - composite-first 路径调 `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` (Phase 2 后走 internal_factory 扫描)
  - fallback 路径保持(为 `PcieEndpointTLM` legacy 兼容)
- [x] **Verify**:
  - `[pcie-simmodule-refactor][inheritance]` 全绿
  - `ModuleFactory::getRegisteredObjectTypes()` 不含 PcieEndpointIP（双注册清理验证）
  - `[pcie-simmodule-refactor][composite-ownership-single]` 全绿(同一 ep_name 多次创建/销毁,composite 唯一)
  - 44498 assertions 全绿
- [x] **Commit (CppTLM)**: `refactor(pcie): switch PcieEndpointIP base from ChStreamModuleBase to SimModule`

#### 任务 2.1.5 — Phase 2 PHY tick 迁移(Oracle 决策 a 落地)

> **Phase 2 行为变更**:composite->tick() 激活,EP::tick override 显式调用,PHY tick 驱动权从测试外部迁移至 EP。**`test_aspm.cc` / 其他 `phy->tick()` 外部驱动测试必须修改**(破坏 Phase 1 "零修改" 承诺,但 Phase 2 已开此预算)。

- [x] **Audit**: `grep -rn "phy.*->tick()\|phy.*\.tick()" test/ | grep -v simmodule_refactor`
  - 列出所有外部驱动 PHY tick 的测试 fixture(预期:test_aspm.cc / test_link_training*.cc / test_pcie_endpoint_ip_*.cc 等)
- [x] **Modify**: 命中的测试文件,移除 EP 创建后的外部 `phy->tick()` 循环驱动;改为单次 `setup_phy_state()` 推 PHY 至目标 LTSSM state(若 LTSSM 推进依赖 tick,改为 `composite->phy().tick()` 或 `composite->tick()` 局部驱动)
  - **允许**:测试 fixture 内部 `composite_->tick()` 调用(经 `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` 取 composite)
  - **禁止**:重新引入对 EP::tick 内部细节的依赖
- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_ep_tick_drives_phy_phase2`
  - ASSERT: Phase 2 后 `ep.tick()` 推进 PHY LTSSM 状态(原 Phase 1 测试断言方向**翻转**)
- [x] **Verify**: `[pcie-simmodule-refactor][phy-migration]` + `[pcie][aspm]` + `[pcie][link-training]` 全绿
- [x] **Commit (CppTLM, 合并入 2.1 主 commit)**: `test(pcie): migrate PHY tick drivers from external to EP::tick (phase-2)`

### 任务 2.2 — composite 17 端口内部可达性验证（R4 决策 C 落地）

> **Oracle 决策 C 修订**:本 change **不引入**外层 JSON `pcie_ep.pf0` 接线;composite 17 端口仅供程序化测试 / Phase 3 AxiAdapter split 内部访问,**不**承诺 `findInternalPath("pf0")` 返回非空(因无 `addOutputConfig` 注册;R4 决策 C)。

- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_composite_internal_reachability_17`
  - ASSERT: `ep->getInternalOutputPort("pcie_ep_lpm.resp_out[0]")` 返回非 null MasterPort(`internal_factory` 内 Step 7 mirror 生效,`sim_module.hh:140-146`)
  - ASSERT: `ep->getInternalOutputPort("pcie_ep_lpm.resp_out[16]")` 非 null(vf15 对应 port 16)
  - ASSERT: `ep->getInternalInputPort("pcie_ep_lpm.req_in[0]")` 返回非 null SlavePort
  - ASSERT: `ep->getInternalInputPort("pcie_ep_lpm.req_in[16]")` 非 null
  - ASSERT: 17 端口**全部**可达(0..16 循环断言,无 off-by-one)
- [x] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc`
  - **不**新增 `addInputConfig / addOutputConfig` 调用(R4 决策 C;且该 API 在 `sim_module.hh` 仅 `#ifdef CPPTLM_TESTING` 暴露)
  - **不**修改 `examples/dgpu_soc_with_pcie_ip.json`(R4 决策 C:Phase 2 不引入外层 pf0/vfN 暴露)
- [x] **Verify**:
  - `[pcie-simmodule-refactor][composite-reachability]` 全绿
  - 17 端口程序化可达性全绿
- [x] **Commit (CppTLM)**: `feat(pcie): composite 17-port internal reachability via getInternalOutputPort (no outer JSON wiring)`

### 任务 2.3 — E2E 全链路验证（Oracle R4 决策 C 闭环 · axi_slave_in 桥接加固）

> **Oracle 审查 C5 风险**(决策 C 后未消除):Phase 8 JSON 现有 `connections` 引用 `pcie_ep.axi_slave_in` / `pcie_ep.axi_master_out`(非 17 端口暴露范围)。Phase 2 后 EP 入 `module_instances`,`findInternalPath("axi_slave_in")==""` → 连接静默丢弃。Phase 8 实际靠 429327d `HostBypassTLM/RC::tick()` 程序化桥接,若桥接充分 E2E 仍能过;若不充分则 E2E 失败。**本任务必须显式断言桥接路径**。

- [x] **Run**: `python3 examples/demo_pcie_full_e2e.py`
  - 验证: AXI slave request 经 HostBypass `tick()` 程序化转发 → `PcieAxiAdapter::for_endpoint(ep_name)->axi()` 路径(非 JSON connection)
  - 验证: completion / cfg write / BAR read 经 EP::tick 内 PcieAxiAdapter 路径,无 ChStream PortPair 依赖
  - **不**验证:`pcie_ep.pf0` ↔ `pcie_ep_lpm.resp_out[0]` 路径(R4 决策 C 取消外层接线)
- [x] **Write test**: `test_pcie_endpoint_ip_simmodule_refactor.cc::test_axislavein_bridge_path_intact`
  - ASSERT: Phase 2 后,`pcie_ep.axi_slave_in` / `axi_master_out` 连接在 module_factory 连接解析中产生 `[WARN] port not found` 警告是**预期**(R4 决策 C 已知)
  - ASSERT: 但 EP 仍能处理经 HostBypass::tick 转发的 AXI 请求(走 PcieAxiAdapter 路径)
  - **目的**:把 C5 风险锁定为"已知 + 已测",避免未来 agent 误以为是回归
- [x] **Run**: `ctest --test-dir build --output-on-failure -j4`（CMake 注册的测试集）
- [x] **Run**: `./build/bin/cpptlm_tests "[pcie]"`（全量 Catch2 44498 assertions）
- [x] **Run**: `./build/bin/cpptlm_tests "[pcie-simmodule-refactor]"`（新增 6+ TEST_CASE）
- [x] **Verify**:
  - E2E PASS
  - 44498 assertions 全绿
  - 6+ 新增 TEST_CASE 全绿

### 任务 2.4 — Phase 2 Oracle gate

- [x] **Run**: `openspec validate 2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor --strict`
- [x] **Run**: `git diff --stat`（与 Impact 章节列出的修改/新增文件清单对比）
- [x] **Verify**:
  - 仅触及 §Impact 列出文件（per `proposal.md §Impact`）
  - 未触碰 23 ABI headers（`pcie_endpoint_tlm.h` / `cpptlm_emulator.h`）
  - 未触碰 `bar_store_` / ResizableBar / INV-G 路径
- [x] **Submit Oracle 复评证据**（per `proposal.md §Oracle 复评` 7 项清单）

---

## §3 文档同步 + Oracle 复评

### 任务 3.1 — 架构文档同步

- [x] **Modify**: `docs/architecture/14-pcie-ip-microarchitecture.md`
  - §8-§10 更新:composite 类设计图 + composite 17 端口内部可达性（程序化 API,非外层 JSON）+ tick 顺序契约
  - 新增章节:`§14.2 PcieLinkPhyMuxTLM Composite Architecture`(描述本 change 引入 + R4 决策 C)
  - 标注 Phase 8 M2 审计: composite 不破坏既有 Phase 8 M2 修复;Phase 8 数据路径靠 429327d 程序化桥接,本 change 不依赖外层 TLP 接线
  - 新增"已知限制"段:`pcie_ep.pf0` 外层 JSON 接线 R4 决策 C 不支持,Phase 3 AxiAdapter split 时统一设计
- [x] **Modify**: `AGENTS.md` STRUCTURE 节
  - `include/tlm/pcie/pcie_link_phy_mux_tlm.hh` 加入 PCIe EP 微架构表格
  - 标注: `PcieEndpointIP` 从 `REGISTER_CHSTREAM` 迁移到 `REGISTER_MODULE`
- [x] **Modify**: `include/AGENTS.md` 注册宏体系表
  - `PcieEndpointIP` 标注: `REGISTER_MODULE(PcieEndpointIP)`（不再走 `REGISTER_CHSTREAM`）
- [x] **Run**: `./scripts/test/docs_sync_check.sh --strict`
- [x] **Verify**: 脚本 PASS

### 任务 3.2 — Phase 1/2 Oracle 复评提交

- [x] **Compile evidence**（per `proposal.md §Oracle 复评` 7 项清单）:
  1. `git diff --stat` 输出
  2. `./build/bin/cpptlm_tests "[pcie]"` 全绿
  3. `./build/bin/cpptlm_tests "[pcie-simmodule-refactor]"` 6+ TEST_CASE 全绿
  4. `python3 examples/demo_pcie_full_e2e.py` PASS
  5. `openspec validate --strict` PASS
  6. `strings build/bin/cpptlm_tests | grep -c "PcieLinkPhyMuxTLM"` ≥ 1
  7. `grep -rn "static FILE\* diag" include/ src/` 输出空
- [x] **Submit**: Oracle 复评 evidence package
- [x] **Await**: Oracle 复评 PASS 后 archive

---

## §4 总计

| 指标 | 值 |
|------|-----|
| Commit 数 | 3（§1 composite + §2 base switch + §2 composite-port reachability） |
| LOC Δ（预估） | +550 / -120（composite 头+实现 ~250 LOC；EP .hh/cc 重构 ~150 LOC；测试 ~250 LOC；文档 ~150 LOC） |
| TEST_CASE | 7+（composite 构造 + tick 顺序 + Phase 1 PHY 休眠锁定 + shim + JSON 消费 7 字段 + inheritance + composite 内部可达性 17 端口 + axislavein 桥接路径 + E2E） |
| Oracle 复评 | 1 次（§3.2 提交 evidence 后） |
| Archive | 通过后 `openspec archive 2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor` |

## §5 复评证据清单（提交 Oracle 时附）

1. **git diff --stat 范围审查** — 仅触及:
   - 修改: `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/tlm/pcie/pcie_endpoint_ip.cc` + `include/chstream_register.hh` + `include/modules_cluster.hh` + `include/tlm/pcie/pcie_link_layer_tlm.hh`(proposal §Impact 例外:仅新增 public setter) + 4 个子模块 `for_endpoint` 改造(.cc only)
   - 新增: `include/tlm/pcie/pcie_link_phy_mux_tlm.hh` + `src/tlm/pcie/pcie_link_phy_mux_tlm.cc` + `test/test_pcie_endpoint_ip_simmodule_refactor.cc`
   - **不修改** `examples/dgpu_soc_with_pcie_ip.json`(R4 决策 C)
   - 不触及: 23 ABI headers / `PcieEndpointTLM` legacy / `bar_store_` / ResizableBar / 4 子模块 .h 布局(LL .hh 仅新增 setter,其他不变)
2. **`[pcie]` Catch2 全量回归** — 44498 assertions 全绿
3. **`[pcie-simmodule-refactor]` 新增测试** — 7+ TEST_CASE 全绿（含 tick 顺序断言 + Phase 1 PHY 休眠锁定 + composite 17 端口内部可达性 + axislavein 桥接路径保留）
4. **`python3 examples/demo_pcie_full_e2e.py`** — PASS
5. **`openspec validate --strict`** — PASS
6. **`strings build/bin/cpptlm_tests | grep -c "PcieLinkPhyMuxTLM"`** — ≥ 1（composite 在 binary 内）
7. **`grep -rn "static FILE\* diag" include/ src/`** — 输出空（无诊断残留）
8. **`ModuleFactory::getRegisteredObjectTypes()`** — 不含 `"PcieEndpointIP"`（Oracle R2 双注册清理验证）
9. **`ModuleFactory::getRegisteredModuleTypes()`** — 含 `"PcieEndpointIP"`
10. **`docs_sync_check.sh --strict`** — PASS
