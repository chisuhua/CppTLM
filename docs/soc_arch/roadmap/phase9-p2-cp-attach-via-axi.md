# phase9-p2-cp-attach-via-axi: CP 寄存器接入 + UE 双路径(Oracle REFINE + YES)

> **类别**: SoC Architecture > Roadmap · **阶段**: P2 · **优先级**: 🟠 跨仓核心议题
> **日期**: 2026-09-16 · **维护者**: Sisyphus · **跨仓**: ✅(CppTLM + UsrLinuxEmu)
> **关联 ADR**: [18-pcie-endpoint-entry.md](../../architecture/18-pcie-endpoint-entry.md) (双仓 SSOT) · [02-command-processor.md](../../architecture/02-command-processor.md) · [16-pcie-endpoint-architecture.md](../../architecture/16-pcie-endpoint-architecture.md)
> **关联 OpenSpec**: [archive/2026-09-10-2026-09-09-cpptlm-pcie-ep-foundation/](../../../openspec/changes/archive/2026-09-10-2026-09-09-cpptlm-pcie-ep-foundation/) (战略基础)
> **决策来源**:Oracle 三方会诊 (2026-09-16) — REFINE + YES

---

## 1. 目标

按 **Oracle REFINE + YES** 决策实施 CommandProcessor 寄存器接入:
- **不要**新建 AXI-Lite slave + crossbar
- **要**把 CP 寄存器映射进 EP BAR 窗口的子区域,经既有 AXI slave 路径转发
- **要**UE 端 ByPass / Real PCIe 双路径运行时配置并存
- **要**严格零 ABI 变更(`mmio_write/read(bar, offset)` 覆盖 CP 寄存器访问)

**关键先例**:`src/tlm/gpu/dgpu_board_shell.cc:305-330` 已实现 SDMA doorbell forwarding 模式(`mmio_write(bar, offset)` → `SdmaEngineTLM::mmio_write(bar=1, offset=kBar1DoorbellOffset)`)。CP 接入 = 照此模式**再写一条 forwarding 规则**,工作量被严重高估的可能性不存在。

**跨仓交付**:
- CppTLM 侧:CP 寄存器文件定义 + EP BAR 转发 + UE ABI 路径验证
- UE 侧:5.5.8 `cp_attach` 重启 + 后端选择机制

---

## 2. 任务清单

| ID | 任务 | 来源 | 成本 | 阻塞 |
|----|------|------|------|------|
| **P2-1** | CP 寄存器映射设计 spec(register map + doorbell 语义) | 设计任务 | 1 周 | 无 |
| **P2-9** | OpenSpec change 提案 + Oracle 评审(在 P2-1 之后,P2-2 之前) | 跨仓 change | 0.5 周 | **P2-1** |
| **P2-2** | EP 侧 BAR decode 扩展:CP 区域转发 | `dgpu_board_shell.cc` 模式 | 1 周 | P2-1 + P2-9 |
| **P2-3** | `command_processor_mvp` 新增 reg file + doorbell→wake() 触发 | `command_processor_mvp.hh/cc` | 1 周 | P2-1 |
| **P2-4** | Ring buffer 读取路径接入(fetch_out ↔ MemoryCluster / VramReadFn) | `command-processor.md` §2.1(FSM) + §4.2(fetch via mem_read_vram GPU_VA) | 1 周 | P2-3 |
| **P2-4b** | `dgpu_board_v1.json` CP 接线修正(known minor #6:`CommandProcessorTLM` 引用但 `dgpu_board_shell` 无接线) | `configs/dgpu_board_v1.json` + `dgpu_board_shell.cc` | 0.5 周 | P2-3 |
| **P2-5** | UE 端 ByPass 路径验证(`HostBypassTLM` 已具 API) | 跨仓验证 | 0.5 周 | P2-2 + P2-4b |
| **P2-6** | UE 端 Real PCIe 路径验证(RC + EP 桥接) | 跨仓验证 | 0.5 周 | P2-5 |
| **P2-7** | UE 双路径运行时 config 选路 | UE 仓 | 1 周 | P2-5/6 |
| **P2-8** | UE 5.5.8 `cp_attach` 真实化 + 集成测试 | UE 仓活跃 change | 2 周 | P2-7 |

### P2-1:CP 寄存器映射设计 Spec

**必备内容**:
- 寄存器地址分配(BAR0 子区域,per NVIDIA/AMD 业界模式)
  - `0x1000 + stream_id * 0x100` → doorbell(per-stream)
  - `0x2000` → CP 状态寄存器
  - `0x2004` → CP 控制寄存器
  - 未来:WPTR/RPTR 寄存器(若不放 VRAM)
- doorbell 写语义(per `docs/research/PCIe/PCIe_上的保序write.md`):强序写,250-700ns latency
- 触发路径:写 doorbell → CP::wake() → 从 IDLE 进 FETCH

**设计原则**:
- 对齐 AMD HQD(Hardware Queue Descriptor)模式:ring buffer base/tail 在 VRAM,doorbell 只触发唤醒
- 不引入新 ABI 函数
- 不增加 register file 复杂度(对齐 NVIDIA GPFIFO + Pushbuffer 简洁模式)

### P2-2:EP 侧 BAR Decode 扩展

**位置**:`src/tlm/gpu/dgpu_board_shell.cc:305-330`(SDMA doorbell forwarding 旁)
**实现**:
```cpp
// 伪代码 - 照 SDMA 模式
if (bar == 0 && offset >= CP_DOORBELL_BASE && offset < CP_DOORBELL_END) {
    auto* cp = get_command_processor();
    if (cp) {
        const uint8_t stream_id = (offset - CP_DOORBELL_BASE) >> 8;
        cp->doorbell_ring(stream_id, static_cast<uint32_t>(data));
        return 0;  // 成功
    }
}
```

**关键纪律**:
- **additive + flag-gated**:仅当 CP attach 时激活,默认行为(bar_store_)不变
- **Phase 5/6/7 测试零破坏**:bar_store_ 仍是 fallback,未命中地址回落

### P2-3:CP 新增 Reg File

**位置**:`include/tlm/gpu/command_processor_mvp.hh`
**新增**:
- `std::unordered_map<uint32_t, uint32_t> regs_;`(或 `std::array<uint32_t, N>`)
- `void doorbell_ring(uint8_t stream_id, uint32_t value);`(替代 `wake()` 或并行)
- `uint32_t read_reg(uint32_t offset) const;`
- `void write_reg(uint32_t offset, uint32_t value);`

**改动量**:~30 行 + 测试 ~50 行

### P2-4:Ring Buffer 读取路径接入

**当前状态**:`command_processor_mvp.cc:66-79` 通过 `vram_read_cb_` 回调读取
**目标**:让 `fetch_out[1]` 端口实际连到 `MemoryCluster`(经 ChStream 桥)
**注意**:MemoryCluster 已在 5-state FSM 中通过 `fetch_out` 端口抽象,实际接线是 DGpuSoc 配置层职责

**改动量**:DGpuSoc JSON 配置 + 少量 ChStream 桥接线

### P2-5 / P2-6:UE 端双路径验证

**ByPass 路径**(P2-5):
- UE 测试 `test_dgpu_p2p_ue_standalone` 已通过 `HostBypassTLM::bar_write` 验证
- 仅需验证**新增的 CP 寄存器地址**能正确触发 EP 转发 → CP doorbell

**Real PCIe 路径**(P2-6):
- UE 测试 `test_dgpu_power_mgmt_ue_standalone` 已通过 RC 枚举 + MMIO 验证
- 验证**新增 CP 寄存器**经过完整 PCIe 协议栈(RC → EP AXI slave → BAR decode → CP)

### P2-7:UE 双路径运行时 Config 选路

**位置**:UsrLinuxEmu 仓活跃 change `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`
**机制**:
- JSON 配置 `"backend": "bypass"` 或 `"backend": "real_pcie"` 选择 host-side 模块实例
- 同二进制,不引入编译宏
- 共享同一 EP 断言,降低测试矩阵

### P2-8:UE 5.5.8 cp_attach 真实化

**当前状态**:UsrLinuxEmu 活跃 change `2026-09-15-5-5-8-stage-1-2-cp-attach-and-ret-zero` 进行中
**目标**:从 `cp_attach 假设错位` 修复,真正调用 `mmio_write(bar=0, offset=CP_DOORBELL_BASE+stream_id*0x100)`

---

## 3. 依赖关系

```
[UE 端 7-fix 收尾] (per UE 仓加固 commit,非 CppTLM 文件) ─┐
P0 全部完成 ──────────────────────────────────────────┐
                                                       │
                                                       ▼
P2-1 (Spec) ──> P2-9 (OpenSpec 提案+评审) ──> P2-2 (EP 转发) ──┬──> P2-5 (UE ByPass 验证)
              │                                          │           │
              │                                          │           └──> P2-6 (UE Real PCIe 验证)
              └──> P2-3 (CP reg) ──> P2-4 (Ring 读取)        │
                                   └──> P2-4b (JSON 接线) ──┘
                                                       │
                                                       ▼
                                  P2-7 (UE 双路径 config) ──> P2-8 (UE cp_attach 真实化)
```

**关键依赖**:
- P2-2 / P2-3 必须在 P2-1 之后(spec 冻结)
- **P2-9(OpenSpec 提案+评审)在 P2-1 之后、P2-2 之前** — 严格遵循"先提案后实施"纪律,避免 UE Wave 6 stage-3 先行冲突
- P2-5 / P2-6 必须有 P2-2 完成的 EP 转发路径才能验证
- P2-8 (UE 真实化)依赖 P2-7 的 config 选路
- **UE 端 stage-3 change MUST 先等 P2-9 (Oracle 评审 PASS) 才能启动实施**(见 §6.4)

---

## 4. 完成标准(DoD)

- [ ] P2-1:`docs/soc_arch/modules/command-processor.md` 附录:CP 寄存器映射 spec 章节已加(含 BAR0 子区域、doorbell 语义、WPTR/RPTR 决策)
- [ ] **P2-9:OpenSpec change 提案 + Oracle 评审 PASS**(在 P2-2 实施前)
- [ ] P2-2:`dgpu_board_shell.cc` 新增 CP forwarding 规则(伪代码见 §P2-2)
- [ ] P2-3:`command_processor_mvp` reg file 实现 + `test_command_processor_regfile.cc` 30+ assertions PASS
- [ ] P2-4:`fetch_out[1]` ↔ MemoryCluster 接线,DGpuSoc JSON 配置已加
- [ ] **P2-4b:`dgpu_board_v1.json` CP 接线修正完成**(known minor #6)
- [ ] P2-5:UE ByPass 路径触发 CP doorbell,E2E 测试 PASS
- [ ] P2-6:UE Real PCIe 路径触发 CP doorbell,完整 PCIe 协议栈验证 PASS
- [ ] P2-7:UE config 双选路机制 + nightly/按需测试 PASS
- [ ] P2-8:UE 5.5.8 cp_attach 真实化,提交已合入 main
- [ ] OpenSpec change 归档至 `openspec/changes/archive/`

**注**:P2-2/P2-5/P2-6 DoD 引用的 UE 测试名(`test_dgpu_p2p_ue_standalone` / `test_dgpu_power_mgmt_ue_standalone`)需在 UE 仓核验存在性 — 当前为引用待验证项,实施前先 grep UE 仓确认。

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| **"假 AXI" 风险**:转发用 direct call 绕过事务语义 | 中 | 高 | 走 `Axi4StreamAdapter::slave_req` 真实路径,仅最后一跳是 reg write;测试断言事务层(burst/rid) |
| **CP 寄存器语义模糊**:边做边发明寄存器 | 中 | 高 | **强制**先写 register map spec(P2-1 提前),对齐 AMD HQD 专利 + `02-command-processor.md` |
| **双路径测试矩阵膨胀** | 中 | 中 | ByPass 作为默认回归,Real PCIe 作为 nightly/按需;共享同一 EP 断言 |
| **7-fix 假成功复发** | 中 | 高 | `pcie-ep-foundation` spec.md 13 ADDED Requirements 作为 SSOT,验证不再断言 `ret!=ENOSYS` 而是数据正确性 |
| **跨仓协调漂移**(CppTLM 5+4 vs UE 5.5.8) | 中 | 高 | 以 `18-pcie-endpoint-entry.md` 为双仓 SSOT,每周同步点;CP reg 接口冻结后两端同时实施 |
| **强制新建 AXI-Lite slave/crossbar** | 低 | 高 | **hard 纪律**:任何 PR 引入新 AXI 端口/crossbar 需 Oracle 评审拒绝,除非 P2-1 spec 明确升级 |

---

## 6. 跨仓协调事项

### 6.1 SSOT 与同步

- **CppTLM 侧 SSOT**:`docs/soc_arch/architecture/18-pcie-endpoint-entry.md`(双仓实施入口)
- **UE 侧 SSOT**:UsrLinuxEmu 活跃 change `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`
- **同步节奏**:W34 提案 / W36 评审 / W38 实施,每周一次同步会

### 6.2 接口冻结契约

- **23 ABI 签名不变**(`18-pcie-endpoint-entry.md §7.3`)
- **5 端口 wire-format 冻结**(HAL append-only)
- **CP 寄存器地址分配** 一旦 P2-1 评审通过,双仓共同冻结
- **双仓镜像文档**:CppTLM 侧 CP 寄存器 spec 写完后,**UE 端必须同步创建 `docs/02_architecture/cp-register-map.md`** 作为镜像(W34 前 stub,内容随 P2-1 演进)。commit 哈希双仓必须一致 — 防止 spec 版本漂移(参考 18-pcie-endpoint-entry.md v0.9 vs UE 端 v0.2.4 不同步的教训)。

### 6.3 Escalation 触发

升级到 **P2-hardened** 模式的触发条件(任一):
- UE 双路径任一出现功能缺失,需引入 crossbar 才能解决
- CP 真实化需要 NoC 级一致性(超出现有 AXI 桥接能力)
- CppTLM + UE 双仓对 BAR 窗口分配有冲突

升级方案:引入独立 AXI-Lite slave + 简单 crossbar,严格按 P2-1 spec 重新评审。

### 6.4 UE 端 stage-3 change 硬约束

UsrLinuxEmu 仓活跃 change `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`(Wave 6 stage-3)的 task definitions 必须遵守以下硬约束:

- **stage-3 MUST NOT 在 P2-9 (Oracle 评审 PASS) 之前启动实施** — 添加 gate: `wait for CppTLM phase9-p2-cp-attach-via-axi.md P2-9 archive`
- stage-3 任一 task 若涉及 CP 寄存器访问,必须在 commit message 中引用 `phase9-p2-cp-attach-via-axi.md P2-1 spec` 的 commit hash
- 若 stage-3 已存在 task 定义与 P2-1 spec 冲突,优先修改 stage-3 task definitions 而非 P2-1 spec(防止双向漂移)

具体落地位置:UsrLinuxEmu `docs/02_architecture/pcie-endpoint-entry.md §12`(跨仓实施 SSOT)需新增 "CppTLM P2 dependency gate" 小节。

---

## 7. 反思与未来改进(本阶段结束后回顾用)

- **实施顺利的话**:Phase 9 收尾(双仓 5.5.8 集成 E2E PASS,完成 ADR-SOC-08 全部前置测试)
- **遇到阻塞的话**:Escalation 到 P2-hardened,或回退到 P0/P1 修补

**未来可选扩展**(Phase 10+):
- 引入 SoC 级 crossbar + NoC(对齐 NVIDIA NvSwitch / AMD XGMI)
- 多 host 共享 CP(VM 分区)
- 用户态 doorbell(避免 kernel MMIO)

---

**下次 review**: W34 末(P2-1 spec 完成,准备 OpenSpec 提案)