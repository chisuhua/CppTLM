# design: cpptlm-spec-purpose-batch3

> **角色**: planner 起草（**Purpose 属契约内容 ⇒ planner 写域**）· builder 落地 + 验证
> **上游**: `proposal.md` + improvement · 台账 **Entry #444**
> **复用**: 批 1 的写作规则 **D-0**（7 条）+ 校验脚本范式 + 归因载体范式（`openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/`）

---

## D-0 · Purpose 写作规则（**沿用批 1，7 条不变**）

| # | 规则 | 本批的适用 |
|:--:|---|---|
| **R-1** | 答「为什么存在 / 边界在哪」，⛔ 不复述 Requirement 清单 | 每份 **≤2 句 + ≤160 字符** |
| **R-2** | ⛔ 不写过程叙述（`被审查确认` / `早期提案` / `v1.x` / `本规范覆盖 A、B、C`） | 硬禁词沿用批 1 清单 |
| **R-3** | 只写现在时能力；spec 内含**spec 级** `Superseded` / `物理删除` ⇒ ⛔ 移出批次 | 本批 **0 份移出**（见 D-1 末的「风险预扫描结论」） |
| **R-4** | ⛔ 不写技术字面量（`>>` / `& 0x` / `0x…` / `A::B`） | 沿用 |
| **R-5** | 单 Requirement 的 spec ⇒ 「为什么需要这条交付判据」起句 + `waiver` | 本批 **0 份单 Requirement**（最少 2 条）⇒ **无 waiver** |
| **R-6** | 同源派生 spec ⇒ 各自锚定「哪条 Requirement 是它的独占内容」；无独占内容 ⇒ 登记残余 | `soc-internal-noc` / `soc-internal-noc-data-flow` 两份同源 ⇒ 各自锚定不同侧面（见 D-1 ⑮⑯） |
| **R-7** | 单物理行（⛔ 不用硬换行） | 沿用 |

---

## D-1 · 17 份 Purpose 草稿（canonical）

> 每条 = `## Purpose` 段的**完整替换文本**（单物理行）。**事实依据** = 来源 change 的 proposal「为什么」段（实施期意图陈述）+ 该 spec 的 Requirement 标题（能力面）。
> ⚠️ **草稿的每处能力断言都在 D-1 末尾的「可追性核对」里逐条列了仓内证据**（实现层文件路径 / 行号或文档段）。

| # | spec | Purpose（canonical） | 事实依据（来源 + Requirement 标题） |
|:--:|---|---|---|
| 1 | `dgpu-4ip-port-contract` | 划定四 IP 拓扑下板级外观与显存控制器之间的端口契约边界：板级对宿主的读写必须走控制器的地址译码路径，不得再由一串历史路由开关拼装决定去向；契约本身须有可执行的一致性校验与接线测试。 | 来源 `2026-10-09-dgpu-realip-pin-contract`（6+ legacy routing flag 是接缝补丁，实际数据路径已被 `match_bar` + `set_pcie_controller_ip` 取代；端口契约没有代码化）· Req `fabric-vram-ctrl-port-contract`·`dgpu-4ip-port-contract-test-coverage`·`dgpu-board-facade-production-wiring` |
| 2 | `dgpu-board-abi-hygiene` | 约束板级壳的对外可维护性：模块须以注册短名对外暴露，内部类型转换须走受控访问入口，空派发分支与遗留桩不得留在生产路径上，避免壳层不断掩盖实现层的真实缺陷。 | 来源 `2026-09-24-cpptlm-dgpu-board-abi-hygiene`（Wave 3 清理：短名注册 / 内部 cast 用 accessor / 删空 switch / 删链路层 stub）· Req 4 条逐条对应 |
| 3 | `dgpu-board-adapter-info` | 让宿主侧能够发现板卡的详细拓扑信息并持有一次触达句柄：前者用于把仿真设备对齐主流仿真器的设备发现范式，后者使一次触达的所有权在宿主与板卡之间有明确归属与释放路径。 | 来源 `2026-09-07-dgpu-board-adapter-info-extension`（支撑 UsrLinuxEmu 真实硬件仿真绑定，对齐 Linux amdgpu/QEMU/gem5 设备发现范式）· Req `板卡适配器详细拓扑信息暴露`·`First-touch Handle 生命周期管理` |
| 4 | `dgpu-board-mvp` | 把早期最小切片的计算单元包装为可由 JSON 拓扑驱动的板级组件，并使其挂入统一的数据流注册机制，从而让最小板卡能够独立跑通宿主的 IOCTL 路径而不必依赖整栈齐备。 | 来源 `2026-08-26-2026-08-21-cpptlm-v05-mvp-s2-dgpu-board`（DGpuBoardTLM 是 MVP 切片的宿主：包装计算单元为可 JSON 驱动的板卡，接入 UsrLinuxEmu 真实 IOCTL 路径）· Req `dgpu-board-8-component-wrapper`·`dgpu-board-chstream-registration` |
| 5 | `dgpu-soc-control-hub` | 收敛 dGPU SoC 的控制面：设备地址翻译、显存外观与控制寄存器访问各自归位到职责单一的模块上，使 SoC 拓扑的接线方式贴近真实板级设计，而不是把多种职责压进同一个外观层。 | 来源 `2026-10-19-cpptlm-dgpu-soc-control-hub`（当前拓扑违背真实 dGPU 物理 RTL 设计；`pcie_memory` 作为 BAR2 外观层违背物理 IP）· Req 5 条（control-hub 模块 / SDMA translate 端口 / 内存设备移除 / 完整 JSON 拓扑 / 物理边界延期登记） |
| 6 | `dgpu-soc-physical-ip-port-boundary` | 划清 SoC 内部互联与板级对外接口的类型边界：内部模块之间不得继续借用面向板外的传输层报文类型，跨边界的转换须有显式适配器承担，使端口的线格式与其物理归属一致。 | 来源 `2026-10-19-cpptlm-dgpu-soc-physical-ip-port-boundary`（旧版 MVP 临时方案把全部 SoC 内部 IP 端口用板外传输层类型；违反 ADR-DGPU-06 的 chip-internal vs board-level 分离）· Req 5 条 |
| 7 | `dgpu-soc-timing` | 定义 dGPU SoC 的时序仿真形态：与功能形态并列共存而不替换，在该形态下推进按周期计数、地址翻译驻留、带宽上限与仲裁成为可观察的语义，同时保持功能形态的行为不变。 | 来源 `docs/designs/dgpu-soc/timing-mode.md` v0.2（独立 timing-mode SoC，与 functional-mode 并列共存，不替换；T1-T8 实施完成）· Req 18 条（仿真模式切换 / cycle advance / TLB / 行缓冲与带宽 / 4 routes 仲裁 / 23 ABI 字节级兼容 / 零回归 …） |
| 8 | `driver-visible-minimal-soc` | 定义宿主侧委托 dGPU 板卡仿真时必须成立的语义：配置空间的条带大小、显存窗口与真容量须区分，地址翻译与数据搬运走各自归属的组件，且对外的 ABI 表面保持不变。 | 来源 `docs/pcie/driver-visible-minimal-soc.md` v1.8（D-AXI：配置空间 / MMIO / backdoor / 寄存器回调 / DMA 翻译 / MSI-X 真实语义；23 ABI 冻结）· Req 14 条（含 `PcieMemoryDevice 提升为 soc 顶层独立组件`·`DGpuBoard 双 size 字段`·`15 ABI Functions Unchanged`·`Freeze Surface Untouched`） |
| 9 | `gmmu-pte-wf` | 纠正地址翻译单元取页表的路径语义与线格式：页表取数必须走芯片内部专用的访存通道而非面向板外的传输层报文类型，并使搬运引擎经内部互联发起请求时其计数与路由可被观察。 | 来源 `2026-10-12-cpptlm-gmmu-pte-bus-correction`（Gap I HIGH：注释写板外传输层类型但配置走内部互联 ⇒ 混淆 PCIe 域与 chip-internal 域，违反 ADR-DGPU-06；Gap G1/G2）· Req 6 条（gc-gmmu-wire-format-correction·gc-sdma-translate-via-noc·gc-soc-internal-noc-data-flow·gc-noc-vc-counters·gc-frozen-surface-zero-diff·gc-test-coverage） |
| 10 | `lwc-bus-hub-mvp` | 为 SoC 内部的控制寄存器访问、门铃递交与消息读取三条路径提供统一的总线设施，使这些访问经由带格式的总线而非对具体实现模块的直接调用，从而让内部接口贴近真实物理 IP 边界。 | 来源 `2026-10-11-cpptlm-lwc-bus-hub-mvp`（Gap H/J/K：4 处 `set_pt_base_*` 直调 + 门铃直调 + MMIO Read 直调 ⇒ 违反「双重无注入约束」）· Req 9 条（lwc-bus-hub-core / cr-bundle / doorbell-bundle / 三条 handling / 异构适配器 / 冻结面零 diff / 6 套件零回归） |
| 11 | `minimal-dgpu-soc-abi-landing` | 让最小板卡配置能够真正通过对外 ABI 被使用：显示缓冲区的容量须由配置与条带尺寸共同推导并在越界时被显式上限约束，配置覆盖与代码推导不一致时须给出告警，而不是靠测试专用接口绕过。 | 来源 `2026-09-25-2027-09-17-cpptlm-minimal-dgpu-soc-v1-landing`（`load_soc_config` 不消费顶层 `framebuffer_size_bytes` 也不从 `bar_sizes[1]` 派生 ⇒ 整份配置通过 ABI 不可用；测试靠 `attach_framebuffer_for_testing` 绕过）· Req 3 条（abi-driver-closed-loop·framebuffer-size-cap·override-consistency-warning） |
| 12 | `minimal-dgpu-soc-architecture` | 确立最小 dGPU SoC 的架构基线：显存只有一份真源且由板卡独占，多个消费者共享同一 backing；条带布局与设备描述的字段命名必须与代码保持同源，使配置、代码与文档三者不会各说各话。 | 来源 `2026-09-03-2027-02-09-cpptlm-dgpu-soc-v1-architecture`（v1.0 战略形态总蓝图；单一 VRAM backing 真源 per ADR-DGPU-05 v1.4 B7；backing 字段命名 per ADR-DGPU-10；BAR 布局与代码真相对齐；GMMU 是 Functional Translation Service；5 消费者路径数据流；doorbell 配置 vs 代码 hardcoded 不一致）· Req 8 条 |
| 13 | `minimal-dgpu-soc-test-coverage` | 补齐最小板卡切片此前推迟的验证面：边界取值与地址基址写入竞态须有特征化用例，端到端链路须由用例贯通，测试标签的汇总须可被直接读出，以免覆盖面只存在于清单而不存在于执行。 | 来源 `2026-09-24-cpptlm-minimal-dgpu-soc-v1-test-coverage`（主 change 实施完成但 7 项推迟：C1.1 标签汇总 / C1.2 边界用例（`framebuffer_size_=0` + 非 4/8 字节 len）/ C1.3 PT_BASE LO/HI race 特征化 / C2.1-C2.4 E2E / C3.1 backlog 确认）· Req 4 条 |
| 14 | `sdma-engine-tlm` | 把设备发起方向的搬运能力做成独立于设备接收方向的组件：地址翻译失败与非法描述须走显式错误路径，完成通知须保持既定的顺序语义，使上游搬运在错误与并发下都有确定结果。 | 来源 `2026-08-28-2026-08-26-cpptlm-dgpu-sdma-engine`（ADR-SOC-07 D3：PCIe master 方向必须与 slave 方向分离为独立组件；真实硬件中 SDMA 是 SoC 内独立 IP；ADR-088 §D3.8 翻译回调需要调用点）· Req 3 条（sdma-engine-component·sdma-engine-error-paths·sdma-completion-ordering） |
| 15 | `soc-internal-noc` | 落地 SoC 内部互联的最小可用形态：为内部关键模块提供按节点寻址的路由骨架与统一的请求响应接口，使页表取数、显存搬运与条带译码这三条内部数据路径能够经同一内部网络贯通。 | 来源 `2026-10-09-cpptlm-dgpu-soc-internal-noc-minimal`（E0：`dgpu-realip-pin-contract` 只覆盖 fabric↔vram_ctrl 一个桥接；SoC 内部 4 个核心模块的端到端数据路径未落地）· Req 8 条（module-identity / 4-target bridge / 4 node id / route_flit / JSON 装配 / 测试覆盖 / 零回归 / 4-IP 文档同步） |
| 16 | `soc-internal-noc-data-flow` | 在内部互联骨架之上把实际数据流接通并使其可观测：经内部网络的搬运须留下可统计的报文痕迹，各目标端的线格式须统一到内部类型，请求与响应的区分须由流类别字段承担而非另造报文类型。 | 来源 `2026-10-09-cpptlm-dgpu-soc-internal-noc-e1`（E0 痛点：驱动路径不可观测 / 4 target wire format 不一致 / req-resp 区分不清晰且另造 dual type 与 v2.1 决策矛盾；v1.1 限定 DataNoC）· Req 9 条 |
| 17 | `vram-controller-functional-mode` | 让显存控制器在功能形态下真正参与数据路径：模式切换须可被配置显式声明，切回旧行为时须保持向后兼容，使该组件不再只是存在却未接入。 | 来源 `2026-10-11-cpptlm-vram-controller-functional-integration`（`VramControllerTLM` 类已存在，但 `bind_memory_backings()` 的第 4 路注入路径未激活；业务定位原则 4 要求 functional-mode 本期即时落地）· Req 7 条（functional 数据路径 / 模式配置 / fallback 向后兼容 / JSON simulation_mode / 测试覆盖 / 冻结面零 diff / 文档同步） |

### D-1 末 · 风险预扫描结论（**16 份有标记 → 逐条核实 → 本批 0 份移出**）

| 预扫描命中 | 份数 | 核实结论 |
|---|:--:|---|
| `SUPERSEDED` / `Superseded` 命中 | 9 | **spec 级 superseded = 1 份**（`cpptlm-d1-p1-pipeline-scoreboard`，**不在本批**）；其余 8 份命中的是**行级历史引用**（`adr-revision-plan:101-102` 记 ADR-SOC-02/03 被取代；`gpgpu-precision-wave2:50` 记别处被取代）⇒ **本 spec 本身未 superseded** |
| `deprecated` 命中 | 6 | 全部是**单个 API / 端口**被标记废弃（`set_translate_cb` / `PcieTlpBundle` 端口 / `attach_timing` / `set_pt_base_*`）⇒ **spec 级有效** |
| `include/cudart/` 命中 | 2 | `sm-microarchitecture:131` 是**规范性表述**「SHALL 删除，目录 SHALL 为空」⇒ 该目录**确已**被 HSK-6 删除 ⇒ 表述**成立**（本批不含该份） |
| `docs/soc_arch/` 命中 | 1 | `v1-architecture` 引用 `docs/soc_arch/architecture/00-overview.md` ⇒ 该文件**已迁 ArchForge**（仓内仅 `docs/soc_arch/README.md`）⇒ 登记 **R-15**（悬空引用），**不写进 Purpose** |
| 单 Requirement（需 waiver） | 5 | 全部落在 D1/D2 域 ⇒ **本批 0 份** ⇒ **无 waiver** |

### D-1 末 · 草稿的**可追性核对**（C-6 的依据 · 逐条）

| # | spec | 草稿中的能力断言 | 仓内可追证据 |
|:--:|---|---|---|
| 1 | `dgpu-4ip-port-contract` | 控制器地址译码路径 / 不用历史路由开关 | `include/tlm/pcie/pcie_controller_ip.hh`（`match_bar`）· `src/tlm/gpu/dgpu_board_shell.cc`（`set_pcie_controller_ip`） |
| 2 | `dgpu-board-abi-hygiene` | 注册短名 / 受控访问入口 | `include/chstream_register.hh`（`REGISTER_CHSTREAM`）· `include/tlm/gpu/dgpu_board_shell.hh` |
| 3 | `dgpu-board-adapter-info` | 拓扑信息 / 一次触达句柄 | `include/tlm/gpu/dgpu_board_shell.hh` · `include/abi/cpptlm_emulator.h`（设备信息结构） |
| 4 | `dgpu-board-mvp` | JSON 驱动板卡 / 数据流注册 | `include/modules_cluster.hh`（`REGISTER_MODULE`）· `include/chstream_register.hh` |
| 5 | `dgpu-soc-control-hub` | 控制面职责单一化 / 显存外观 | `include/tlm/gpu/dgpu_board_shell.hh` · `include/tlm/gpu/pcie_memory_device.hh` |
| 6 | `dgpu-soc-physical-ip-port-boundary` | 内部 vs 板外类型边界 | `include/bundles/dgpu_fabric_bundles_tlm.hh` · `include/bundles/pcie_tlp_wire_bundle.hh` |
| 7 | `dgpu-soc-timing` | 时序形态并列共存 / 周期计数 | `include/tlm/vram_controller_tlm.hh` · `include/tlm/gpu/gmmu_tlm.hh` · `include/tlm/gpu/sdma_engine_tlm.hh` · `configs/dgpu_soc_timing_v1.json` |
| 8 | `driver-visible-minimal-soc` | 条带大小 vs 显存窗口 / 翻译与搬运各自归属 / ABI 不变 | `include/tlm/gpu/dgpu_board_shell.hh`（双 size 字段 + `bind_memory_backings`）· `include/tlm/gpu/pcie_memory_device.hh` · `include/tlm/gpu/gmmu_tlm.hh` · `include/abi/cpptlm_emulator.h` |
| 9 | `gmmu-pte-wf` | 页表取数走内部访存通道 | `include/tlm/gpu/gmmu_tlm.hh` · `include/tlm/cluster/soc_internal_noc.hh` |
| 10 | `lwc-bus-hub-mvp` | 统一总线设施 / 三条路径 | `include/tlm/lwc/lwc_bus_hub.hh` · `include/bundles/cr_bundles_tlm.hh` · `include/bundles/doorbell_bundles_tlm.hh` |
| 11 | `minimal-dgpu-soc-abi-landing` | 显示缓冲容量推导 + 上限 + 告警 | `src/tlm/gpu/dgpu_board_shell.cc`（`load_soc_config` / `framebuffer_size_`）· `include/tlm/gpu/pcie_display_device.hh` |
| 12 | `minimal-dgpu-soc-architecture` | 单一显存真源 / 字段命名同源 | `docs/adr/ADR-DGPU-05-vram-storage-ownership.md` · `docs/adr/ADR-DGPU-10-*.md` · `src/tlm/gpu/dgpu_board_shell.cc` |
| 13 | `minimal-dgpu-soc-test-coverage` | 边界用例 / 竞态特征化 / E2E / 标签汇总 | `test/test_minimal_dgpu_soc*.cc`（按 `[minimal_dgpu_soc]` 标签可 grep） |
| 14 | `sdma-engine-tlm` | 搬运独立组件 / 错误路径 / 完成顺序 | `include/tlm/gpu/sdma_engine_tlm.hh` · `include/bundles/dma_bundles_tlm.hh` |
| 15 | `soc-internal-noc` | 按节点寻址路由骨架 / 请求响应接口 | `include/tlm/cluster/soc_internal_noc.hh` · `include/modules_cluster.hh` |
| 16 | `soc-internal-noc-data-flow` | 报文痕迹可统计 / 线格式统一 / 流类别区分 | `include/tlm/cluster/soc_internal_noc.hh` · `include/bundles/noc_bundles_tlm.hh` |
| 17 | `vram-controller-functional-mode` | 模式可配置 / 向后兼容 fallback | `include/tlm/vram_controller_tlm.hh` · `src/tlm/gpu/dgpu_board_shell.cc`（`bind_memory_backings` 第 4 路） |

---

## D-2 · 归因载体（`test/spec_purpose_batch3_attribution.json`）

⚠️ **`checks` 四布尔由脚本生成**（人手填 = 自证 ⇒ 判别力为零，per metis S-5）⇒ 由 `scripts/test/cep_spec_purpose_batch3_check.py` 重算并回写。

**⚠️ 与批 1 的差异（判据扩展，非放宽）**：本批有 **2 份 spec 的来源 change 无归档目录**（`dgpu-soc-timing` / `driver-visible-minimal-soc` —— 二者是**主 spec 直写**，从未走归档器）⇒ 归因载体新增 **`evidence_kind`** 字段：

| `evidence_kind` | 适用 | C-7 判据 |
|---|---|---|
| `archived-change-proposal` | 15 份 | 证据文件 `isfile` **且**「来源 change 核心名」出现在证据文件前 2000 字符内 |
| `repo-doc` | 2 份（`dgpu-soc-timing` / `driver-visible-minimal-soc`） | 证据文件 `isfile` **且** 证据文件名含该 spec 的**主名 token**（`timing-mode` / `driver-visible-minimal-soc`）⇒ 仍是「可追」，⛔ 非自证 |

**⚠️ 另一处差异（批 1 的实测教训）**：批 1 证明 **spec 的 TBD 行声明的 change 名 ≠ 归档目录名**（`minimal-dgpu-soc-architecture` 的 TBD 写 `cpptlm-minimal-dgpu-soc-v1-architecture`，实际归档目录 = `2026-09-03-2027-02-09-cpptlm-dgpu-soc-v1-architecture`）⇒ **⛔ 不可用 TBD 行做 glob 键** ⇒ 本批的 glob 键**逐份实测后写死**在归因载体生成脚本里，并**要求唯一命中**（命中 0 或 ≥2 ⇒ 生成器报错退出）。

```json
{
  "schema": "spec-purpose-attribution/v2",
  "batch": "batch3",
  "archive_semantics": "completed-direct-main-spec",
  "items": [
    {
      "spec": "dgpu-soc-timing",
      "claim_kind": "current",
      "evidence_kind": "repo-doc",
      "source_change": null,
      "source_evidence": "docs/designs/dgpu-soc/timing-mode.md",
      "requirement_titles": ["仿真模式切换 (per §1.4 仿真模式声明)", "…"],
      "purpose_text": "定义 dGPU SoC 的时序仿真形态：……",
      "waiver": null,
      "result_commit": "<产物 commit sha>"
    }
  ]
}
```

---

## D-3 · 校验项（由脚本重算 · **判据与批 1 同源，两处扩展见下**）

| # | 校验 | 判据 | 与批 1 的关系 |
|:--:|---|---|---|
| **C-1** | 非占位 | Purpose 段内 `TBD`/`TODO` 计数 = 0（**限段**） | 同 |
| **C-2** | 长度 + 单行 | `50 ≤ len ≤ 160` **且** **单物理行** | 同 |
| **C-3** | 不复述 Requirement | **CJK-only LCS < 12** + **英文词序列公共 < 5 词**（`waiver` 档 16） | 同（批 1 实测订正后的形式） |
| **C-4** | 无过程叙述 | ⛔ 含 `被审查确认`/`早期提案`/`越界做法`/`本规范覆盖`/`本 change`/`v\d\.\d`/`Phase \d`/`Task \d+`/`Entry #\d+`/日期/`归档载体`/`分别见` ⇒ 红 | 同 |
| **C-5** | 无技术字面量 | ⛔ 含 `>>`/`& 0x`/`0x[0-9a-f]+`/`A::B` ⇒ 红 | 同 |
| **C-6** | 不声称不存在的能力 | ① `claim_kind` 与 spec 内 **spec 级** `Superseded`/`物理删除` **互斥** ② Purpose 中的**英文标识符** ⊆ Requirement 标题 token ∪ 仓内 `include/` `src/` `configs/` 可 grep 符号 ∪ 领域缩写白名单 | 同（⚠️ **纯中文能力声称仍在机判射程外** ⇒ 人工抽验 + critic 分组签兜住） |
| **C-7** | 归因闭合 | `source_evidence` `isfile` == True **且** 按 `evidence_kind` 命中（`archived-change-proposal` ⇒ 核心名命中；`repo-doc` ⇒ 文件名含 spec 主名 token） | **扩展**（批 1 只有前者） |

⚠️ **C-6 的 spec 级 superseded 判据订正（批 3 实施前发现）**：批 1 的 C-6 判据是「spec 全文含 `Superseded|物理删除` ⇒ 不得为 `current`」⇒ **本批实测该口径会假红**：`adr-revision-plan` 的 `:101-102` 记的是**别的 ADR 被取代**、`gpgpu-precision-wave2:50` 记的是**别处被取代** ⇒ spec 本身有效。
⇒ **订正为 spec 级判据**：⛔ `Status: SUPERSEDED` / `Status: SUPERSEDED (…)` 形式的**状态行**、或 `**Superseded** by <change>` 形式的**本 spec 状态更新** ⇒ 才算 spec 级。**本批 17 份实测 0 份命中**（`cpptlm-d1-p1-pipeline-scoreboard` 命中但不在本批）。
⚠️ 该订正**只放宽到 spec 级**，⛔ **不放宽到「spec 存在任何 superseded 字样」** ⇒ 批 1 的 16 份判据结论不变（批 1 已归档，不追溯）。

---

## D-4 · 落地方式与预期 diff 形状

- **替换**：正则 `## Purpose\n(.*?)\n## Requirements` **整段替换** ⇒ `## Requirements` 段**逐字节不动**。
- **预期 diff 形状（AC-04 写死）**：**17 文件 · 每文件恰 1 hunk · 删 1 行增 1 行 · 不新增空行 · Purpose 单物理行**。
- **归因载体生成**：独立脚本从 `design.md` §D-1 表格**机器提取** 17 条 canonical 草稿（⛔ 不手抄 ⇒ 无转录误差）+ **glob 解析**来源归档目录（⛔ 不依赖 TBD 行）+ 从 spec **自动抽取** `requirement_titles`。

---

## D-5 · 边界（零放宽）

⛔ 17 份的 `## Requirements` 段**逐字节不变** · ⛔ 不动 43 份 `cep-*` · ⛔ 不动实现层 · ⛔ 不改既有 45 闸（新增的校验脚本属本 change 自测工具）· ⛔ 零冻结产物（17 份均不在 `FROZEN_GLOBS`）⇒ 免五步链。
