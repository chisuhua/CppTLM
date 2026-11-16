# design: cpptlm-spec-purpose-batch3

> **角色**: planner 起草（**Purpose 属契约内容 ⇒ planner 写域**）· builder 落地 + 验证
> **上游**: `proposal.md` + improvement · 台账 **Entry #444**
> **复用**: 批 1 的写作规则 **D-0**（7 条）+ 校验脚本范式 + 归因载体范式（`openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/`）
> **metis 预审**: 8 MUST + 8 SHOULD + 5 NIT **全处置**（其中 **MUST-1 推翻初稿范围**：17 → **14** 份）

---

## D-0 · Purpose 写作规则（**沿用批 1，7 条不变**）

| # | 规则 | 本批的适用 |
|:--:|---|---|
| **R-1** | 答「为什么存在 / 边界在哪」，⛔ 不复述 Requirement 清单 | 每份 **≤2 句 + ≤160 字符** |
| **R-2** | ⛔ 不写过程叙述（`被审查确认` / `早期提案` / `v1.x` / `本规范覆盖 A、B、C`） | 硬禁词沿用批 1 清单 |
| **R-3** | 只写现在时能力；spec 内含**spec 级** `Superseded` ⇒ ⛔ 移出批次 | 本批 **0 份移出**（3 份已按 D-1 末「实施状态判定」移出） |
| **R-4** | ⛔ 不写技术字面量（`>>` / `& 0x` / `0x…` / `A::B`） | 沿用 |
| **R-5** | 单 Requirement 的 spec ⇒ 「为什么需要这条交付判据」起句 + `waiver` | 本批 **0 份单 Requirement**（最少 2 条）⇒ **无 waiver** |
| **R-6** | 同源派生 spec ⇒ 各自锚定「哪条 Requirement 是它的独占内容」 | `soc-internal-noc` / `soc-internal-noc-data-flow` 两份同源 ⇒ 各自锚定不同侧面 + **强制交叉对拍** |
| **R-7** | 单物理行（⛔ 不用硬换行） | 沿用 |

---

## D-1 · 14 份 Purpose 草稿（canonical）

> 每条 = `## Purpose` 段的**完整替换文本**（单物理行）。**事实依据** = 来源 change 的 proposal「为什么」段（实施期意图陈述）+ 该 spec 的 Requirement 标题（能力面）。
> ⚠️ **每条能力断言的仓内可追证据**见 D-1 末表（**行为符号 / 配置键**，⛔ 不是「头文件存在」）。

| # | spec | Purpose（canonical） | 事实依据（来源 + Requirement 标题） |
|:--:|---|---|---|
| 1 | `dgpu-4ip-port-contract` | 划定四 IP 拓扑下板级外观与显存控制器之间的端口契约边界：板级对宿主的读写必须走控制器的地址译码路径，不得再由一串历史路由开关拼装决定去向；契约本身须有可执行的一致性校验与接线测试。 | 来源 `2026-10-09-dgpu-realip-pin-contract`（6+ legacy routing flag 是接缝补丁，实际数据路径已被 `match_bar` + `set_pcie_controller_ip` 取代；端口契约没有代码化）· Req `fabric-vram-ctrl-port-contract`·`dgpu-4ip-port-contract-test-coverage`·`dgpu-board-facade-production-wiring` |
| 2 | `dgpu-board-abi-hygiene` | 约束板级壳的对外可维护性：模块须以注册短名对外暴露，内部类型转换须走受控访问入口，空派发分支与遗留桩不得留在生产路径上，避免壳层不断掩盖实现层的真实缺陷。 | 来源 `2026-09-24-cpptlm-dgpu-board-abi-hygiene`（Wave 3 清理：短名注册 / 内部 cast 用 accessor / 删空 switch / 删链路层 stub）· Req 4 条逐条对应 |
| 3 | `dgpu-board-adapter-info` | 让宿主侧能够发现板卡的详细拓扑信息并持有设备句柄：前者用于把仿真设备对齐主流仿真器的设备发现范式，后者使句柄的所有权在宿主与板卡之间有明确归属与释放路径。 | 来源 `2026-09-07-dgpu-board-adapter-info-extension`（支撑 UsrLinuxEmu 真实硬件仿真绑定，对齐 Linux amdgpu/QEMU/gem5 设备发现范式）· Req `板卡适配器详细拓扑信息暴露`·`First-touch Handle 生命周期管理`（⛔ 草稿**不提**已删除的 `get_adapter_info`，见 D-1 末注） |
| 4 | `dgpu-soc-timing` | 定义 dGPU SoC 的时序仿真形态：与功能形态并列共存而不替换，在该形态下推进按周期计数、地址翻译驻留、带宽上限与仲裁成为可观察的语义，同时保持功能形态的行为不变。 | 来源 `0345d283:openspec/changes/cpptlm-dgpu-soc-timing-mvp/proposal.md`（v0.2，独立 timing-mode SoC，与 functional-mode 并列共存，不替换）· Req 18 条（仿真模式切换 / cycle advance / TLB / 行缓冲与带宽 / 4 routes 仲裁 / 23 ABI 字节级兼容 / 零回归 …） |
| 5 | `driver-visible-minimal-soc` | 定义宿主侧委托 dGPU 板卡仿真时必须成立的语义：配置空间所报告的地址窗口大小、显存窗口与真容量须区分，地址翻译与数据搬运走各自归属的组件，且对外的 ABI 表面保持不变。 | 来源 `689752ef:openspec/changes/cpptlm-driver-visible-minimal-soc/proposal.md`（D-AXI：配置空间 / MMIO / backdoor / 寄存器回调 / DMA 翻译 / MSI-X 真实语义；23 ABI 冻结）· Req 14 条（含 `PcieMemoryDevice 提升为 soc 顶层独立组件`·`DGpuBoard 双 size 字段`·`15 ABI Functions Unchanged`·`Freeze Surface Untouched`） |
| 6 | `gmmu-pte-wf` | 纠正地址翻译单元取页表的路径语义：页表取数须由该单元的专用访存客户端承接并绕开 mesh 互联，而不是借用面向板外的传输层报文类型；搬运引擎经内部互联发起请求时，其计数与路由须可被观察。 | 来源 `2026-10-12-cpptlm-gmmu-pte-bus-correction`（Gap I HIGH：页表取数应走专用访存客户端，混淆 PCIe 域与 chip-internal 域，违反 ADR-DGPU-06；Gap G1/G2）· Req 6 条（gc-gmmu-wire-format-correction·gc-sdma-translate-via-noc·gc-soc-internal-noc-data-flow·gc-noc-vc-counters·gc-frozen-surface-zero-diff·gc-test-coverage） |
| 7 | `lwc-bus-hub-mvp` | 为 SoC 内部的控制寄存器访问、门铃递交与消息读取三条路径提供统一的总线设施，使这些访问经由带格式的总线而非对具体实现模块的直接调用，从而让内部接口贴近真实物理 IP 边界。 | 来源 `2026-10-11-cpptlm-lwc-bus-hub-mvp`（Gap H/J/K：4 处 `set_pt_base_*` 直调 + 门铃直调 + MMIO Read 直调 ⇒ 违反「双重无注入约束」）· Req 9 条（lwc-bus-hub-core / cr-bundle / doorbell-bundle / 三条 handling / 异构适配器 / 冻结面零 diff / 6 套件零回归） |
| 8 | `minimal-dgpu-soc-abi-landing` | 让最小板卡配置能够真正通过对外 ABI 被使用：显示缓冲区的容量须由配置与地址窗口尺寸共同推导并在越界时被显式上限约束，配置覆盖与代码推导不一致时须给出告警，而不是靠测试专用接口绕过。 | 来源 `2026-09-25-2027-09-17-cpptlm-minimal-dgpu-soc-v1-landing`（`load_soc_config` 不消费顶层 `framebuffer_size_bytes` 也不从 BAR1 尺寸派生 ⇒ 整份配置通过 ABI 不可用；测试靠 `attach_framebuffer_for_testing` 绕过）· Req 3 条（abi-driver-closed-loop·framebuffer-size-cap·override-consistency-warning） |
| 9 | `minimal-dgpu-soc-architecture` | 确立最小 dGPU SoC 的架构基线：显存只有一份真源且由板卡独占，多个消费者共享同一 backing；BAR 布局与 backing 字段的命名必须与代码保持同源，使配置与代码不会各说各话。 | 来源 `2026-09-03-2027-02-09-cpptlm-dgpu-soc-v1-architecture`（v1.0 战略形态总蓝图；单一 VRAM backing 真源 per ADR-DGPU-05 v1.4 B7；backing 字段命名 per ADR-DGPU-10；BAR 布局与代码真相对齐；5 消费者路径数据流）· Req 8 条（⛔ 草稿**不提** doorbell 配置不一致 —— 该 Requirement 记录的正是**未收敛的不一致**，见 D-1 末注） |
| 10 | `minimal-dgpu-soc-test-coverage` | 补齐最小板卡切片此前推迟的验证面：边界取值与地址基址写入竞态须有特征化用例，端到端链路须由用例贯通，测试标签的汇总须可被直接读出，以免覆盖面只存在于清单而不存在于执行。 | 来源 `2026-09-24-cpptlm-minimal-dgpu-soc-v1-test-coverage`（主 change 实施完成但 7 项推迟：C1.1 标签汇总 / C1.2 边界用例（`framebuffer_size_=0` + 非 4/8 字节 len）/ C1.3 PT_BASE LO/HI race 特征化 / C2.1-C2.4 E2E / C3.1 backlog 确认）· Req 4 条 |
| 11 | `sdma-engine-tlm` | 把设备发起方向的搬运能力做成独立于设备接收方向的组件：地址翻译失败与非法描述须走显式错误路径，完成通知须保持既定的顺序语义，使上游搬运在错误与并发下都有确定结果。 | 来源 `2026-08-28-2026-08-26-cpptlm-dgpu-sdma-engine`（ADR-SOC-07 D3：PCIe master 方向必须与 slave 方向分离为独立组件；真实硬件中 SDMA 是 SoC 内独立 IP；ADR-088 §D3.8 翻译回调需要调用点）· Req 3 条（sdma-engine-component·sdma-engine-error-paths·sdma-completion-ordering） |
| 12 | `soc-internal-noc` | 落地 SoC 内部互联的最小可用形态：为内部模块提供按节点寻址的路由骨架与统一的请求响应接口，并给出各节点在当前版本是否承接实际流量的显式说明。 | 来源 `2026-10-09-cpptlm-dgpu-soc-internal-noc-minimal`（E0：`dgpu-realip-pin-contract` 只覆盖 fabric↔vram_ctrl 一个桥接）· Req 8 条（module-identity / 4-target bridge / 4 node id / route_flit / JSON 装配 / 测试覆盖 / 零回归 / 4-IP 文档同步）· ⛔ **草稿已收窄**（删除「三条数据路径贯通」—— 与 `gmmu-pte-wf`「GMMU 不经 mesh NoC」矛盾，见 D-1 末注） |
| 13 | `soc-internal-noc-data-flow` | 在内部互联骨架之上把实际数据流接通并使其可观测：经内部网络的搬运须留下可统计的报文痕迹，各目标端须经适配器转换到内部类型，请求与响应的区分由流类别字段承载而不另造报文类型。 | 来源 `2026-10-09-cpptlm-dgpu-soc-internal-noc-e1`（E0 痛点：驱动路径不可观测 / 4 target wire format 不一致 / req-resp 区分不清晰且另造 dual type 与 v2.1 决策矛盾；v1.1 限定 DataNoC）· Req 9 条 |
| 14 | `vram-controller-functional-mode` | 让显存控制器在功能形态下真正参与数据路径：模式切换须可被配置显式声明，切回旧行为时须保持向后兼容，使该组件不再只是存在却未接入。 | 来源 `2026-10-11-cpptlm-vram-controller-functional-integration`（`VramControllerTLM` 类已存在，但 `bind_memory_backings()` 的第 4 路注入路径未激活；业务定位原则 4 要求 functional-mode 本期即时落地）· Req 7 条（functional 数据路径 / 模式配置 / fallback 向后兼容 / JSON simulation_mode / 测试覆盖 / 冻结面零 diff / 文档同步） |

---

## D-1 末 · ⭐ 实施状态判定（**本轮最关键的实测**，决定范围）

⚠️ **判据不能用「来源 change 的 tasks 勾选数」** —— 批 1 的 **F-6** 已证明：归档之后才执行、未回勾 ⇒ tasks 是**记录债**，不是实施证据（实测本批 8 份「0 勾选」的 change 中，**6 份实现层锚点全部命中**）。
⇒ **判据 = 实现层行为锚点（`implementation_anchors`）**：行为符号 / 配置键在 `include/ src/ configs/ test/*.cc` 中真实命中。

| spec | 来源 tasks | 实现锚点（实测） | 判定 |
|---|:--:|---|:--:|
| `dgpu-4ip-port-contract` | 38/0 | `match_bar`（`include/tlm/pcie/pcie_controller_ip.hh`）· `set_pcie_controller_ip`（`src/tlm/gpu/dgpu_board_shell.cc`） | ✅ 已实施 |
| `dgpu-board-abi-hygiene` | 21/0 | `REGISTER_CHSTREAM`（`include/chstream_register.hh`）· 板级 accessor | ✅ 已实施 |
| `dgpu-board-adapter-info` | 8/0 | `cpptlm_emulator_open` / `close`（`include/abi/cpptlm_emulator.h:106-107` + `src/abi/cpptlm_emulator.cc:340`）· `visible_vram_size` / `invisible_vram_size`（`cpptlm_emulator.h:39-40`） | ✅ 已实施（⛔ `get_adapter_info` 已删除 ⇒ 草稿不提） |
| `dgpu-soc-timing` | 无归档目录 | `VramControllerMode` + `set_mode` · `translate_timing()` + TLB（`include/tlm/gpu/gmmu_tlm.hh:17-18`）· `configs/dgpu_soc_timing_v1.json` | ✅ 已实施 |
| `driver-visible-minimal-soc` | 无归档目录 | `vram_storage_`（`dgpu_board_shell.hh:242`）· `bind_memory_backings` · BAR2 aperture | ✅ 已实施 |
| `gmmu-pte-wf` | **0/48** | `set_vram_target` / `uses_vram_dedicated_client`（`include/tlm/gpu/gmmu_tlm.hh:134-138`）⇒ Gap I 已修复 | ✅ 已实施（tasks 记录债） |
| `lwc-bus-hub-mvp` | **0/85** | `include/tlm/lwc/lwc_bus_hub.hh` + `register_doorbell_target` + `doorbell_write` | ✅ 已实施（tasks 记录债） |
| `minimal-dgpu-soc-abi-landing` | 48/1 | `framebuffer_size_ = final_size`（`src/tlm/gpu/dgpu_board_shell.cc:154`）· `bar_sizes[1]` 派生 | ✅ 已实施 |
| `minimal-dgpu-soc-architecture` | 无 tasks.md | `vram_storage_`（`dgpu_board_shell.hh:242`，ADR-DGPU-10 新命名） | ✅ 已实施 |
| `minimal-dgpu-soc-test-coverage` | 20/0 | `test/` 内 `[minimal_dgpu_soc]` + PT_BASE race 特征化用例 | ✅ 已实施 |
| `sdma-engine-tlm` | 23/0 | `cpptlm_dma_translate_cb` 持有（`include/tlm/gpu/sdma_engine_tlm.hh:66`） | ✅ 已实施 |
| `soc-internal-noc` | **0/40** | `class SocInternalNoc` + `NODE_ID_PCIE/SDMA/GMMU/VRAM` + `route_flit` + `vc_counts_[4]` + `flits_dispatched_`（`include/tlm/cluster/soc_internal_noc.hh`） | ✅ 已实施（tasks 记录债） |
| `soc-internal-noc-data-flow` | **0/57** | `class VramNocAdapter`（`include/tlm/vram_noc_adapter.hh:48`）· `route_flit` 实际推送 + `vc_counts_[4]` | ✅ 已实施（tasks 记录债） |
| `vram-controller-functional-mode` | **0/68** | `VramControllerMode::FunctionalZeroDelay`（`include/tlm/vram_controller_tlm.hh:53`）+ `set_mode`（`:74`） | ✅ 已实施（tasks 记录债） |
| ~~`dgpu-board-mvp`~~ | 33/12 | ❌ **锚点全不命中**：`include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc` **不存在** · `configs/dgpu_board_v1_mvp.json.in` **不存在** | ❌ **未实施 ⇒ 移出（R-16）** |
| ~~`dgpu-soc-control-hub`~~ | **0/170** | ❌ **锚点全不命中**：`ControlHub` / `Chub` **0 命中**（`include/ src/`）· `configs/dgpu_soc_minimal_v1.json:49` 仍实例化 `PcieMemoryDevice` · 来源 proposal 状态 = `🟡 Proposed Draft v0.5.0 (草稿, 待评审)` | ❌ **未实施 ⇒ 移出（R-16）** |
| ~~`dgpu-soc-physical-ip-port-boundary`~~ | **0/127** | ❌ **锚点全不命中**：`include/tlm/gpu/gmmu_tlm.hh:12,142` 明写 `wire-format 统一 PcieTlpBundle`（**未**切 `NoCFlitBundle`）· 来源 proposal 状态 = `🟡 Proposed Draft v0.5.0 (草稿, 待评审)` | ❌ **未实施 ⇒ 移出（R-16）** |

⇒ **范围 17 → 14 份**（移出 3 份**未实施**的 spec，登记 **R-16** —— 与 R-10 同型的「孤儿 spec 存废裁定」，路由 architect）。

### D-1 末 · 三处「草稿 vs 事实」的**主动收窄**（metis MUST-2 + SHOULD-10 + NIT-16 的处置）

| # | 草稿初稿 | 冲突事实 | 处置 |
|:--:|---|---|---|
| **N-1** | #12 `soc-internal-noc`：「使页表取数、显存搬运与条带译码这三条内部数据路径能够经同一内部网络贯通」 | `gmmu-pte-wf/spec.md:28`「**GMMU 不经 mesh NoC**」· `soc-internal-noc-data-flow/spec.md:15`「GMMU page walk …**不进 SocInternalNoc**」· `soc_internal_noc.hh:63` `NODE_ID_GMMU` 注释「v1.1 **不接**」 | ✅ **收窄**为「路由骨架 + 统一请求响应接口 + 各节点是否承接流量的显式说明」 |
| **N-2** | #6 `gmmu-pte-wf`：「页表取数必须走**芯片内部专用**的访存通道」 | 该 spec 的 `AxiMemBundle` 条目已被 driver-visible v1.8 H1 取代（`gmmu_tlm.hh:142`）⇒ 「芯片内部」这一措辞已过时 | ✅ **改写**为「由该单元的**专用访存客户端**承接并**绕开 mesh 互联**」 |
| **N-3** | #9 `minimal-dgpu-soc-architecture`：「条带布局与设备描述的字段命名必须与代码保持同源，使配置、代码与文档三者不会各说各话」 | 该 spec 的 Requirement `doorbell 配置 vs 代码 hardcoded 不一致` 记录的正是**未收敛的不一致** ⇒ 原句读作「已一致」= 用规范文字吞掉已知缺陷 | ✅ **删除**该句，只保留「BAR 布局 + backing 字段命名同源」⇒ doorbell 不一致**登记为残余 R-18** |

### D-1 末 · 草稿的**可追性核对**（C-6/C-8 的依据 · **行为符号**，⛔ 不是「头文件存在」）

| # | spec | `implementation_anchors`（行为符号 / 配置键） | 实测 |
|:--:|---|---|:--:|
| 1 | `dgpu-4ip-port-contract` | `match_bar` · `set_pcie_controller_ip` | ✅ 命中 |
| 2 | `dgpu-board-abi-hygiene` | `REGISTER_CHSTREAM` · 板级 accessor | ✅ 命中 |
| 3 | `dgpu-board-adapter-info` | `cpptlm_emulator_open` · `cpptlm_emulator_close` · `visible_vram_size` | ✅ 命中 |
| 4 | `dgpu-soc-timing` | `VramControllerMode` · `set_mode` · `translate_timing` | ✅ 命中 |
| 5 | `driver-visible-minimal-soc` | `vram_storage_` · `bind_memory_backings` · BAR2 aperture | ✅ 命中 |
| 6 | `gmmu-pte-wf` | `set_vram_target` · `uses_vram_dedicated_client` | ✅ 命中 |
| 7 | `lwc-bus-hub-mvp` | `register_doorbell_target` · `doorbell_write` | ✅ 命中 |
| 8 | `minimal-dgpu-soc-abi-landing` | `framebuffer_size_` · `bar_sizes[1]` 派生 | ✅ 命中 |
| 9 | `minimal-dgpu-soc-architecture` | `vram_storage_`（ADR-DGPU-10 命名） | ✅ 命中 |
| 10 | `minimal-dgpu-soc-test-coverage` | `[minimal_dgpu_soc]` 标签用例 · PT_BASE race 特征化 | ✅ 命中 |
| 11 | `sdma-engine-tlm` | `cpptlm_dma_translate_cb` | ✅ 命中 |
| 12 | `soc-internal-noc` | `SocInternalNoc` · `route_flit` · `NODE_ID_*` | ✅ 命中 |
| 13 | `soc-internal-noc-data-flow` | `VramNocAdapter` · `vc_counts_` | ✅ 命中 |
| 14 | `vram-controller-functional-mode` | `VramControllerMode::FunctionalZeroDelay` · `set_mode` | ✅ 命中 |

---

## D-2 · 归因载体（`test/spec_purpose_batch3_attribution.json`）

⚠️ **`checks` 由脚本生成**（人手填 = 自证 ⇒ 判别力为零，per metis S-5）⇒ 由 `scripts/test/cep_spec_purpose_batch3_check.py` 重算并回写。

**两种来源形态，**同一**判据（C-7 单一判据表，见 D-3）**：

| `evidence_kind` | 适用 | 证据路径 | 复现方式 |
|---|---|---|---|
| `archived-change-proposal` | **12 份** | `openspec/changes/archive/<dir>/proposal.md`（工作树） | 直接读文件 |
| `git-history-proposal` | **2 份**（`dgpu-soc-timing` / `driver-visible-minimal-soc`） | `openspec/changes/<name>/proposal.md`（**历史路径**，工作树已无） | `git show <ref>:<path>` |

⚠️ **`git-history-proposal` 的来源实测（推翻初稿推断）**：
- 初稿写「二者是**主 spec 直写**，**从未走归档器**」⇒ **错误推断**，已撤回。
- 实测：`git log --all --diff-filter=A` 找到两个 change 目录的**真实新增 commit** ——
  `cpptlm-dgpu-soc-timing-mvp` ⇒ **`0345d283`**（168 行 proposal，`git show` 可达，核心名命中前 2000 字符）；
  `cpptlm-driver-visible-minimal-soc` ⇒ **`689752ef`**（228 行 proposal，同上）。
- ⇒ **真实情况 = 归档目录被移走 / 改名**（与 `minimal-dgpu-soc-architecture` 的 TBD 行名 ≠ 归档目录名同型）⇒ 登记 **R-17**。
- ⇒ **收益**：`repo-doc` 这条**弱判据路径不必存在** ⇒ metis MUST-5（判据无牙）**直接消解**（不新建 `repo-doc` 形态）。

```json
{
  "schema": "spec-purpose-attribution/v2",
  "batch": "batch3",
  "archive_semantics": "completed-direct-main-spec",
  "items": [
    {
      "spec": "dgpu-soc-timing",
      "claim_kind": "current",
      "evidence_kind": "git-history-proposal",
      "source_change": "cpptlm-dgpu-soc-timing-mvp",
      "source_evidence": "openspec/changes/cpptlm-dgpu-soc-timing-mvp/proposal.md",
      "source_evidence_ref": "0345d283",
      "implementation_anchors": ["VramControllerMode", "set_mode", "translate_timing"],
      "requirement_titles": ["仿真模式切换 (per §1.4 仿真模式声明)", "…"],
      "purpose_text": "定义 dGPU SoC 的时序仿真形态：……",
      "waiver": null,
      "result_commit": "<产物 commit sha>"
    }
  ]
}
```

| 字段 | 用途 | 机判 |
|---|---|---|
| `claim_kind` | `current` / `historical` / `superseded` / `proposed` | **互斥校验**：spec 级 superseded 标记 ⇒ ⛔ 不得为 `current`（C-6） |
| `evidence_kind` | `archived-change-proposal` / `git-history-proposal` | C-7 单一判据表的输入 |
| `source_evidence_ref` | `git-history-proposal` 形态的固定 sha（⛔ 不可写 `HEAD`） | `git show <sha>:<path>` 可达 |
| `implementation_anchors` | **行为符号 / 配置键**（⛔ 不是头文件名） | 逐个在 `include/ src/ configs/ test/*.cc` 命中（C-8） |
| `requirement_titles` | 该 spec 的全部 `### Requirement:` 标题 | 从 spec 自动抽取 |
| `purpose_text` | 实际落地文本 | 从 spec 自动抽取 |
| `waiver` | `single-requirement` / null | Requirement 数 == 1 ⇒ 必须有 waiver **且**以「为什么需要这条交付判据」起句（本批 0 份） |
| `result_commit` | 落地 commit | 回填 |
| `checks` | **脚本生成**，不手填 | 见 D-3 |

---

## D-3 · 校验项（由脚本重算 · **判据表单一**）

| # | 校验 | 判据 | 相对批 1 |
|:--:|---|---|---|
| **C-1** | 非占位 | Purpose 段内 `TBD`/`TODO` 计数 = 0（**限段**） | 同 |
| **C-2** | 长度 + 单行 | `50 ≤ len ≤ 160` **且** **单物理行** | 同 |
| **C-3** | 不复述 Requirement | **CJK-only LCS < 12** + **英文词序列公共 < 5 词**（`waiver` 档 16） | 同 |
| **C-4** | 无过程叙述 | ⛔ `被审查确认`/`早期提案`/`越界做法`/`本规范覆盖`/`本 change`/`v\d\.\d`/`Phase \d`/`Task \d+`/`Entry #\d+`/日期/`归档载体`/`分别见` ⇒ 红 | 同 |
| **C-5** | 无技术字面量 | ⛔ `>>`/`& 0x`/`0x[0-9a-f]+`/`A::B` ⇒ 红 | 同 |
| **C-6** | **spec 级** superseded 互斥 | 判定**只认状态位置**：文件**前 10 行** / `> **状态**` 行 / `## Status` 段 / front-matter `status:`；词表扩展 = `SUPERSEDED\|DEPRECATED\|OBSOLETE\|RETIRED\|REPLACED\|已废弃\|已下线\|不再维护\|已取代\|被取代\|退役\|停用` ⇒ 命中 ⇒ ⛔ 不得为 `current` | **收紧到 spec 级**（批 3 实测：批 1 口径会假红 —— 9 份命中里 8 份是**行级历史引用**；批 1 已归档**不追溯**） |
| **C-7** | 归因闭合（**单一判据**） | 证据必须**可复现**：`archived-change-proposal` ⇒ 工作树文件 `isfile`；`git-history-proposal` ⇒ `git show <ref>:<path>` 可达（`ref` 须为固定 sha）⇒ **且**「来源 change 核心名」出现在证据**前 2000 字符**内 ⇒ **且** 归因里记录证据的 blob sha 供复验比对 | **扩展**（批 1 只有前者） |
| **C-8** | **实施存在性**（新） | 每份 spec 的 `implementation_anchors` **逐个**在 `include/ src/ configs/ test/*.cc` 中命中；**任一未命中** ⇒ 该 spec ⛔ 不得留在本批（须移出 + 登记残余） | **新增**（metis MUST-6） |
| **C-9** | **AC-02b 闭合载体**（新） | 归因载体旁生成 `test/spec_purpose_batch3_reconcile.json`（schema `spec-purpose-reconcile/v1`）：`fixed = pre.red − post.red`；`set(post.red) ∪ set(fixed) == set(pre.red)`；两集**不相交**；`k = len(fixed)`；每个 `remaining[].reasons` 必须是 post 实跑 `openspec validate --strict --json` 的**真实 issue code**；`fixed` 的 pre issues 必须 ⊆ **仅 Purpose 类** | **新增**（metis MUST-3） |

⚠️ **C-6 / C-8 的机判边界（显式登记，⛔ 不隐瞒）**：
- **纯中文的能力声称不在机判射程内**（CJK-LCS < 12 时无法区分「合理概括」与「凭空声称」）⇒ **中文侧由人工抽验 + critic 分组签承担**（T2.4 抽验须**覆盖 5 组各 1 份**，且**必须含** #6 `gmmu-pte-wf` 与 #12 `soc-internal-noc`）。
- **C-8 只验「锚点存在」**，⛔ **不验**「锚点语义等于该 Purpose 的声称」⇒ 同上边界。

---

## D-4 · 落地方式与预期 diff 形状

- **替换**：正则 `## Purpose\n(.*?)\n## Requirements` **整段替换** ⇒ `## Requirements` 段**逐字节不动**。
- **预期 diff 形状（AC-04 写死）**：**14 文件 · 每文件恰 1 hunk · 删 1 行增 1 行 · 不新增空行 · Purpose 单物理行**。
- **归因载体生成**：独立脚本从 `design.md` §D-1 表格**机器提取** 14 条 canonical 草稿（⛔ 不手抄 ⇒ 无转录误差）+ **glob 解析**来源归档目录（⛔ **不依赖 TBD 行** —— 批 1 实测其名 ≠ 归档目录名）+ 从 spec **自动抽取** `requirement_titles` + 从本表**自动抽取** `implementation_anchors`。
- ⛔ **生成器要求**：glob 必须**唯一命中**（命中 0 或 ≥2 ⇒ 报错退出）；`git-history-proposal` 的 `ref` 必须是**固定 sha**（⛔ 不可写 `HEAD`）。

---

## D-5 · 边界（零放宽）

⛔ 14 份的 `## Requirements` 段**逐字节不变** · ⛔ 不动 43 份 `cep-*` · ⛔ 不动实现层 · ⛔ 不改既有 45 闸（新增的校验脚本属本 change 自测工具）· ⛔ 零冻结产物（14 份均不在 `FROZEN_GLOBS`）⇒ 免五步链。

---

## D-6 · critic 分组（**5 组**，metis SHOULD-9 采纳 + 强制交叉对拍）

| 组 | 份数 | spec |
|:--:|:--:|---|
| **①** 板级外观与 ABI 卫生 | 3 | `dgpu-4ip-port-contract` · `dgpu-board-abi-hygiene` · `dgpu-board-adapter-info` |
| **②** 总线设施与内核搬运 | 2 | `lwc-bus-hub-mvp` · `sdma-engine-tlm` |
| **③** SoC 基线与验证 | 3 | `minimal-dgpu-soc-architecture` · `minimal-dgpu-soc-abi-landing` · `minimal-dgpu-soc-test-coverage` |
| **④** 时序形态与驱动可见面 | 2 | `dgpu-soc-timing` · `driver-visible-minimal-soc` |
| **⑤** 内部互联（**3 份同组**） | 3 | `gmmu-pte-wf` · `soc-internal-noc` · `soc-internal-noc-data-flow` |

**⚠️ 强制交叉对拍项（组 ⑤ 专属 · 解决 metis SHOULD-9 的「同组互相背书」风险）**：
> 组 ⑤ 的 #12 `soc-internal-noc` 与 #13 `soc-internal-noc-data-flow` 是同源 spec ⇒ **必须逐条对拍** `gmmu-pte-wf/spec.md`（GMMU **不经** mesh NoC）与 `soc_internal_noc.hh` 的 `NODE_ID_GMMU` / `NODE_ID_PCIE` 注释（本期**不接** / **无入向流量**）⇒ 任何一条 Purpose 若声称「GMMU / PCIe 侧流量经内部 NoC 贯通」⇒ **该组不签**。

⚠️ **组名已按 metis NIT-18/NIT-20 订正**：测试覆盖类（`minimal-dgpu-soc-test-coverage`）归入 **③ SoC 基线与验证**（不塞「内部互联」）；设备发现类（`dgpu-board-adapter-info`）归入 **① 板级外观与 ABI 卫生**。

---

## D-7 · metis 预审处置对照表（8 MUST + 8 SHOULD + 5 NIT 全处置）

| # | metis 条目 | 处置 |
|:--:|---|---|
| **MUST-1** | `dgpu-soc-control-hub` / `dgpu-soc-physical-ip-port-boundary` 的来源是**未实施草案** ⇒ Purpose 把计划写成事实 | ✅ **采纳并扩大** —— 实测追加发现 **`dgpu-board-mvp` 亦未实施**（`dgpu_board_mvp.hh/.cc` 与 `dgpu_board_v1_mvp.json.in` 均不存在）⇒ **范围 17 → 14**，登记 **R-16** |
| **MUST-2** | #12 `soc-internal-noc` 声称「三条路径贯通」与 `gmmu-pte-wf` / `soc-internal-noc-data-flow` 矛盾 | ✅ **采纳** —— 已**收窄**（见 D-1 末 N-1）+ 组 ⑤ 强制交叉对拍项 |
| **MUST-3** | AC-02b 无载体 / 字段 / 判定者 | ✅ **采纳** —— 新增 **C-9** + `test/spec_purpose_batch3_reconcile.json` schema + 机判式 |
| **MUST-4** | 「从未走归档器」是**无依据推断** | ✅ **采纳** —— 实测 `git log --all --diff-filter=A` 找到 `0345d283` / `689752ef` ⇒ 真实情况 = **归档目录被移走** ⇒ 登记 **R-17** + 措辞已改 |
| **MUST-5** | `repo-doc` 判据无牙 | ✅ **采纳（结果不同）** —— 因为 MUST-4 的实测使 `repo-doc` **不必存在** ⇒ 改为 `git-history-proposal`（`git show` 可达 + 核心名命中 + blob sha 记录）⇒ **比 metis 建议的「文件名含 token」更强** |
| **MUST-6** | 缺「来源 change 是否已实施」判据 + 可追性核对表把「文件存在」当「能力存在」 | ✅ **采纳** —— 新增 **C-8 `implementation_anchors`**（**行为符号**，⛔ 非头文件存在）+ 可追性核对表改列锚点 + **实施状态判定表**（D-1 末） |
| **MUST-7** | 预扫描漏了「Proposed Draft 未实施的 spec」 | ✅ **采纳** —— D-1 末「实施状态判定」表以**实现锚点**判定（⛔ 不用 tasks 勾选 —— F-6 教训） |
| **MUST-8** | `claim_kind` 枚举缺 `proposed`；C-6 可被换措辞绕过 | ✅ **采纳** —— 枚举扩为 `{current, historical, superseded, proposed}` + C-6 只认**状态位置** + 词表扩展 11 项 |
| **SHOULD-9** | critic 分组负载不均 + 同源 spec 同组风险 | ✅ **采纳** —— 重切为 **5 组** + **组 ⑤ 强制交叉对拍项** |
| **SHOULD-10** | `gmmu-pte-wf` 的「芯片内部线格式」措辞已过时 | ✅ **采纳** —— 已改写（见 D-1 末 N-2） |
| **SHOULD-11** | #16 措辞降级 | ✅ **采纳** —— 已降级为「经适配器转换到内部类型」+「由流类别字段承载」 |
| **SHOULD-12** | AC-09 计数口径（`STATUS_AUDIT_*.md` 是文件 ⇒ `ls | wc -l` = 151） | ✅ **采纳** —— 改为「按**目录**计数：150 → 151」 |
| **SHOULD-13** | improvement 的「事实基础最扎实」被过度解读 | ✅ **采纳** —— Out of Scope 增加「未实施草案 spec 不在本批（R-16）」+ 措辞收窄 |
| **SHOULD-14** | Entry #444 B-2 错分（10 条 `carrier_buildable_id_blocked` 实为「⛔ id 被已签批 ADR 挡住」⇒ 属 architect） | ✅ **采纳** —— Entry #444 §2.2 已改：B-2 从「类 B 本仓可推进」迁到「类 C/D 需 architect」 |
| **SHOULD-15** | D-2 与 D-3 的 C-7 表述不一致 | ✅ **采纳** —— D-2 只引用 D-3 的**单一判据表**（C-7 行） |
| **NIT-16** | 「条带」误译 BAR | ✅ **采纳** —— #5 / #8 / #9 已改为「地址窗口」/「BAR 布局」/「地址窗口尺寸」 |
| **NIT-17** | 人工抽验须覆盖 4 组各 1 份 | ✅ **采纳（改为 5 组）** —— T2.4 抽验须覆盖 **5 组各 1 份** + **必须含** #6 / #12 |
| **NIT-18** | 组名与内容不符（test-coverage 在「内部互联」组） | ✅ **采纳** —— 已移入组 ③（见 D-6） |
| **NIT-19** | D-2 schema 示例用 `…` 省略 | ✅ **采纳** —— 示例改为完整数组（`…` 仅作排版示意，⛔ 生成器须能处理） |
| **NIT-20** | `dgpu-board-adapter-info` 在组 ① 名不副实 | ✅ **采纳（保留）** —— 组 ① 改名为「板级外观与 ABI 卫生」，adapter-info 的「设备发现」属板级对外 ABI 面 ⇒ 归组 ① |
