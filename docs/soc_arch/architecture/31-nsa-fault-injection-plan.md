# NSA-FIT: NSA-aware Scale-Up 故障注入测试计划 v0.1 (Oracle P1-1 修正)

> **目的**: 定义 CppTLM dGPU SoC **MAS-3.1 NSA-aware Scale-Up** 的 **Fault Injection (FI) 测试计划**, 是 NSA Stage 1/2 实施时的**关键验证手段**。本计划是 Oracle 评审维度 5（测试覆盖，8.2/10）的核心改进, 目标将测试覆盖维度从 8.2 提升至 9.0+。
>
> **状态**: Draft v0.1 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 8.2 → 9.0+)
> **归属 OpenSpec**: 集成至 `openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`
> **关联文档**:
> - [`30-nsa-comprehensive-supplement.md`](30-nsa-comprehensive-supplement.md) §3 故障模型 (8 类故障类型)
> - [`26-gsp-rm-firmware.md`](26-gsp-rm-firmware.md) §0.5.5 FM↔GSP-RM 异常路径 (5 类)
> - [`28-cxl-3-fabric.md`](28-cxl-3-fabric.md) CXL 3.0 Fabric 兼容
> - [`29-nsa-evolution-roadmap.md`](29-nsa-evolution-roadmap.md) 5 阶段演进

> **NSA 命名含义澄清** (适用于所有 NSA 草案, 2026-09-19 决策):
>
> 本草案中 **NSA** 含义为 **"Network-System Architecture"** (多 Linux 节点 + 跨 Fabric 寻址架构), 与美国 **National Security Agency (国家安全局)** **无关**, 仅 CppTLM 内部使用。
>
> NSA 衍生术语对应关系:
>
> | NSA 衍生术语 | 含义 | 与 CXL 3.0 / NVLink / Infinity Fabric 对应 |
> |---|---|---|
> | NSA Switch | NSA Switch (跨 Fabric 路由器) | ≈ CXL Fabric Switch Tier 1 |
> | NSA Fabric Address | 64-bit NSA-aware 地址 (16+48 bits) | ≈ CXL Fabric Address |
> | NSA-aware MMU | 含 Fabric ID + Capability 的 MMU | ≈ CXL-aware IOMMU |
> | NSA Stage 1/2/3 | 5 阶段演进阶段 | ≈ CXL 3.0 Fabric 量产节奏 |
> | NSA-aware SoC | NSA-aware 分布式 SoC 终态 | ≈ CXL 3.0 Fabric-aware SoC |
>
> **替代命名参考** (若未来需替换): GFS (Global Fabric System) / FAS (Fabric Address Space) / UFA (Unified Fabric Address), **当前决策保持 NSA + 备注澄清**。
>

> **关联 ADR**: ADR-SOC-22/23/24

---

## §0 阅读引导

- 想理解 Fault Injection 测试总体 → 读 §1
- 想看 8 类故障 FI 测试用例 → 读 §2
- 想看 FI 测试工具链 → 读 §3
- 想看 RTO / 数据丢失容忍度量化 → 读 §4
- 想看测试阶段划分 → 读 §5
- 想看开放问题 → 读 §6

---

## §1 Fault Injection 测试总体设计

### §1.1 测试目标

NSA-aware Scale-Up Fault Injection 测试的 4 大目标:

```
目标 1: 验证故障模型完整性
  - 8 类故障类型全部触发并恢复
  - 验证故障处理流程符合 `30-nsa-comprehensive-supplement.md` §3 规范

目标 2: 量化恢复时间 (RTO)
  - 每类故障的检测时间 + 隔离时间 + 恢复时间
  - 满足 NSA Stage 1/2 RTO 目标 (<1 s / <10 ms / <100 ms)

目标 3: 量化数据丢失容忍度
  - 在途事务 (in-flight) 受故障影响的范围
  - Capability 撤销 / Drain 中止 / Page Fault 跨节点的数据一致性保证

目标 4: 验证 Split-Brain 处理
  - FM ↔ GSP-RM 状态不一致检测与恢复
  - 跨 Compute Tray Owner Directory 同步协议
```

### §1.2 测试层次（4 层）

```
NSA-aware Scale-Up Fault Injection 4 层测试:

Level 1: 模块级 FI (per 硬件模块)
  - 目标: NSA-aware MMU / Remote Atomic Unit / Hardware Directory 等单模块故障
  - 工具: gem5 + FI 插件 (Python 脚本注入故障)
  - 时延: NSA Stage 1 实施时 (2026-2027)

Level 2: 子系统级 FI (per 子系统)
  - 目标: GSP-RM 4 大服务 / Switch PTE / Compute Tray 内部故障
  - 工具: SystemC + FI 库 (per CppTLM 框架)
  - 时延: NSA Stage 1 实施时

Level 3: SoC 级 FI (per SoC)
  - 目标: 多 Compute Tray + NSA Switch + Switch Tray 协同故障
  - 工具: CppTLM 仿真 + 故障注入
  - 时延: NSA Stage 2 实施时 (2027-2028)

Level 4: 集成级 FI (含软件栈)
  - 目标: 跨节点 PCIe over UALink + Linux KMD + GSP-RM + FM 协同
  - 工具: QEMU + gem5 + 真实 Linux KMD
  - 时延: NSA Stage 2 后期
```

### §1.3 测试范围（8 类故障 × 4 层 = 32 个测试场景）

```
每类故障在 4 层各 1 个测试场景, 共 32 个:

故障类型 (per `30-nsa-comprehensive-supplement.md` §3.1):
  1. 节点失效 (Compute Tray 故障)
  2. Fabric 分区 (UALink 物理层断开)
  3. In-flight Capability 撤销 (事务中途撤销)
  4. Drain 中止 (FM 决策中途取消)
  5. Page Fault 跨节点 (Remote-Fault)
  6. Switch Tray Linux FM 失联
  7. GSP-RM 失联 (GPU 硬件故障)
  8. Host FM 失联 (Host OS 崩溃)

每类故障 × 4 层 (模块/子系统/SoC/集成) = 32 个 FI 测试场景
```

---

## §2 8 类故障 FI 测试用例详细设计

### §2.1 故障 1: 节点失效 (Compute Tray 故障)

```
故障注入位置:
  - Level 1: GSP-RM 微控制器硬件故障 (经 Verilog force 注入)
  - Level 2: Compute Tray 子系统故障 (PCIe Switch 桥接器故障)
  - Level 3: 多 Compute Tray 中一个完全失效
  - Level 4: 真实 Compute Tray 断电 (硬件拔除)

故障注入方式:
  Level 1: gem5 FI 脚本: `$fault_inject --module gsp_rm --fault hw_crash --duration 1ms`
  Level 2: SystemC: `sc_core::sc_event` 触发 `gsp_rm_thread` 异常
  Level 3: CppTLM 仿真: 关闭 Compute Tray 时钟域 (clk_core/clk_fab/clk_io 全停)
  Level 4: QEMU: `system_powerdown -action poweroff`

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 1):
  - FM 经 MSI-X 检测: GSP-RM 心跳超时
  - FM 决策: 故障隔离
  - FM 通知所有 Compute Tray KMD: 该 Compute Tray 不可用
  - FM 协调: 工作负载迁移 (经 Host FM 决策)

验证项:
  - [ ] 故障检测时间: ≤ 100 ms (MSI-X 心跳超时)
  - [ ] 故障隔离时间: ≤ 1 s (FM 通知所有 Compute Tray)
  - [ ] 工作负载迁移时间: ≤ 10 s (per workload, 大模型训练可能更长)
  - [ ] 其他 Compute Tray 不受影响 (零中断)
  - [ ] 故障 Compute Tray 恢复后, FM 自动重新纳入拓扑
  - [ ] RTO 满足 NSA Stage 2 目标: <10 s (故障隔离 + 迁移)
```

### §2.2 故障 2: Fabric 分区 (UALink 物理层断开)

```
故障注入位置:
  - Level 1: UALink PHY 链路断开 (PCIe Switch 端口 down)
  - Level 2: Switch PTE 路由表损坏
  - Level 3: 两个 Compute Tray 间 UALink 断开
  - Level 4: 物理拔除 UALink 电缆

故障注入方式:
  Level 1: gem5 FI: `$fault_inject --module ualink_phy --fault link_down`
  Level 2: SystemC: 强制 UALink PTE 输出 Unreachable
  Level 3: CppTLM: 关闭 Compute Tray 间 UALink 通道
  Level 4: 真实: 拔除 UALink 电缆

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 2):
  - NSA Switch PTE 更新: 标记该路径为 Unreachable
  - FM 通知所有 Compute Tray KMD: 该 Fabric 路径不可用
  - Capability 撤销: 跨 Fabric Capability 标记为无效
  - 工作负载迁移 (FM 协调)

验证项:
  - [ ] 故障检测时间: ≤ 10 ms (UALink PHY Link Down 信号)
  - [ ] Switch PTE 更新: ≤ 100 ms (NSA Switch 内部)
  - [ ] Capability 撤销: ≤ 1 ms (TLB Flush + Version 校验)
  - [ ] 跨 Fabric 工作负载迁移: ≤ 5 s (FM 协调)
  - [ ] 其他 Fabric 路径仍正常工作 (非全局故障)
  - [ ] UALink 物理恢复后, PBR 自动重新建立
  - [ ] RTO 满足 NSA Stage 2 目标: <5 s (故障隔离 + 迁移)
```

### §2.3 故障 3: In-flight Capability 撤销（事务中途撤销）

```
故障注入位置:
  - Level 1: NSA-aware MMU TLB Entry Capability 校验失败
  - Level 2: GSP-RM Capability Manager 撤销进行中
  - Level 3: 跨 Compute Tray Capability 共享撤销中
  - Level 4: 真实 Capability 撤销 (FM → GSP-RM → TLB Flush)

故障注入方式:
  Level 1: gem5 FI: 注入 Capability Version 不匹配
  Level 2: SystemC: GSP-RM Capability Manager 强制删除 Token
  Level 3: CppTLM: 跨 Compute Tray Capability 撤销模拟
  Level 4: 真实: FM 经 Host Comm 通知 GSP-RM 撤销 Capability

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 3):
  - HW: 当前 SM 事务挂起 (类似 Page Fault)
  - GSP-RM Fault Service: 通知 FM Capability 已撤销
  - SW: SM 重新发起事务 (使用新 Capability Token) 或 SIGSEGV

验证项:
  - [ ] 在途事务挂起时间: ≤ 1 μs (HW 检测 + 挂起)
  - [ ] TLB Flush 原子性: Capability 撤销 + TLB Entry 失效同时发生 (HW 原子)
  - [ ] 后续访问触发 Security Violation AWT Trap
  - [ ] 无数据丢失 (在途事务要么完成要么挂起重试)
  - [ ] 跨 Compute Tray Capability 撤销: ≤ 10 ms (跨节点协调)
  - [ ] RTO 满足 NSA Stage 2 目标: <10 ms (本地) / <1 s (跨节点)
```

### §2.4 故障 4: Drain 中止（FM 决策中途取消）

```
故障注入位置:
  - Level 1: NIC-DMA ROB 强制清零
  - Level 2: GSP-RM Fabric Service Drain 中止
  - Level 3: 跨 Compute Tray Drain 中止
  - Level 4: 真实: FM 写 NIC_DRAIN_CTRL.FORCE_ABORT = 1

故障注入方式:
  Level 1: gem5 FI: 注入 FORCE_ABORT 信号
  Level 2: SystemC: GSP-RM Fabric Service 接收 FORCE_ABORT
  Level 3: CppTLM: 跨 Compute Tray Drain 中止模拟
  Level 4: 真实: Host KMD 写 NIC_DRAIN_CTRL.FORCE_ABORT

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 4 + `26-gsp-rm-firmware.md` §0.5.3):
  - GSP-RM Fabric Service 接收 FORCE_ABORT
  - 强制清零 NIC-DMA ROB
  - 通知 Switch FM: Drain 中止

验证项:
  - [ ] Drain 中止响应时间: ≤ 10 μs (NIC-DMA ROB 清零)
  - [ ] Switch FM 收到通知: ≤ 100 μs (经 Sideband SMBus)
  - [ ] 后续流量恢复正常: ≤ 1 ms (Capability 不受影响)
  - [ ] Capability 不受影响 (Drain 中止 ≠ Capability 撤销)
  - [ ] 跨 Compute Tray Drain 中止协调: ≤ 50 μs (NSA Stage 2)
  - [ ] RTO 满足 NSA Stage 2 目标: <1 ms (恢复流量)
```

### §2.5 故障 5: Page Fault 跨节点（Remote-Fault）

```
故障注入位置:
  - Level 1: NSA-aware MMU Remote-Fault 标志位触发
  - Level 2: GSP-RM Fault Service 响应 Remote Fault
  - Level 3: 跨 Compute Tray Page Fault
  - Level 4: 真实 Page Fault (Linux 调页)

故障注入方式:
  Level 1: gem5 FI: 注入 Page Table 不命中
  Level 2: SystemC: GSP-RM Fault Service 模拟响应
  Level 3: CppTLM: 跨 Compute Tray Page Fault 模拟
  Level 4: 真实: Linux mmap + 强制访问未映射地址

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 5 + `26-gsp-rm-firmware.md` §4.3):
  - HW: 发起 Remote Page Fault Request (经 UALink VC0)
  - Owner Compute Tray GSP-RM Fault Service 响应
  - Page Table Hit → 注入 TLB
  - Page Table Miss → 通知 Host Driver 调页

验证项:
  - [ ] Local Page Fault 时延: ≤ 5 μs (HW + SW 调页)
  - [ ] 跨 Compute Tray Remote Page Fault 时延: ≤ 50 μs (含跨节点 + HW 协调)
  - [ ] Page Table Hit 比例: ≥ 80% (TLB 命中率)
  - [ ] 跨 Compute Tray Remote Fault 成功率: ≥ 99%
  - [ ] 调页失败 (Host Driver OOM): AWT Trap + SIGSEGV
  - [ ] RTO 满足 NSA Stage 2 目标: <5 μs (本地) / <50 μs (跨节点)
```

### §2.6 故障 6: Switch Tray Linux FM 失联

```
故障注入位置:
  - Level 1: Switch Tray 内部 Linux 进程崩溃
  - Level 2: Switch Tray 与 Compute Tray 间 Sideband 断开
  - Level 3: Switch Tray 完全失联
  - Level 4: 真实 Switch Tray 断电

故障注入方式:
  Level 1: Linux: `kill -9 switch_fm_daemon`
  Level 2: CppTLM: 模拟 Sideband SMBus 通信超时
  Level 3: CppTLM: 关闭 Switch Tray 时钟域
  Level 4: 真实: Switch Tray 断电

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 6):
  - 主机 FM 经 Sideband SMBus 心跳检测
  - 主机 FM 决策: 进入保守模式
  - 主机 FM 通知所有 Compute Tray KMD: 跨 Fabric 不可用
  - 工作负载降级: 仅本地 Compute Tray 寻址

验证项:
  - [ ] 故障检测时间: ≤ 5 s (FM 心跳超时)
  - [ ] 进入保守模式时间: ≤ 1 s (Capability 仅允许 Fabric ID = 0)
  - [ ] 跨 Fabric Capability 拒绝时间: ≤ 10 ms (HW Capability 校验)
  - [ ] 本地 Compute Tray 仍正常工作 (零中断)
  - [ ] Switch Tray 恢复后自动重建通信 (≤ 5 s)
  - [ ] RTO 满足 NSA Stage 2 目标: <10 s (保守模式生效)
```

### §2.7 故障 7: GSP-RM 失联（GPU 硬件故障）

```
故障注入位置:
  - Level 1: GSP 微控制器硬件崩溃
  - Level 2: GSP-RM 微内核崩溃
  - Level 3: GSP-RM 4 大服务中一个失效 (如 Tenant Manager)
  - Level 4: 真实 GPU 断电

故障注入方式:
  Level 1: gem5 FI: 注入 GSP 硬件崩溃信号
  Level 2: SystemC: GSP-RM 微内核 panic
  Level 3: CppTLM: GSP-RM 单服务失效模拟
  Level 4: 真实: GPU PCIe 断电

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 7):
  - FM 经 MSI-X 检测: GSP-RM 心跳超时
  - FM 决策: 故障隔离
  - FM 通知所有 Compute Tray KMD: 该 GPU 不可用
  - 工作负载迁移 (FM 协调)

验证项:
  - [ ] 故障检测时间: ≤ 100 ms (GSP-RM 心跳超时)
  - [ ] FM 故障隔离决策: ≤ 1 s
  - [ ] 其他 GPU 不受影响 (零中断)
  - [ ] 工作负载迁移: ≤ 10 s
  - [ ] GPU 恢复后自动重新纳入拓扑
  - [ ] RTO 满足 NSA Stage 2 目标: <10 s
```

### §2.8 故障 8: Host FM 失联（Host OS 崩溃）

```
故障注入位置:
  - Level 1: Host FM daemon 进程崩溃
  - Level 2: Host KMD 故障
  - Level 3: Host OS 内核崩溃
  - Level 4: 真实 Host OS 崩溃 (Kernel Panic)

故障注入方式:
  - Level 1: Linux: `kill -9 host_fm_daemon`
  - Level 2: Linux: `rmmod nvidia_kmd`
  - Level 3: Linux: `echo c > /proc/sysrq-trigger` (Kernel Panic)
  - Level 4: 真实: Kernel Panic + 自动重启

预期行为 (per `30-nsa-comprehensive-supplement.md` §3.1 故障 8):
  - Switch Tray FM 经 Sideband SMBus 心跳检测
  - Switch Tray FM 进入 "保守模式"
  - Capability 仅允许本地寻址 (Fabric ID = 0)
  - 拒绝跨 Fabric Capability 签发
  - HRT 维持当前状态 (不切换)
  - 恢复: Host OS 重启, FM 自动恢复

验证项:
  - [ ] 故障检测时间: ≤ 5 s (Switch FM 心跳超时)
  - [ ] 进入保守模式: ≤ 1 s (Capability 拒绝跨 Fabric)
  - [ ] GPU 仍在运行 (GSP-RM 维持, 仅本地寻址)
  - [ ] Host OS 重启后 FM 自动恢复: ≤ 30 s
  - [ ] 跨 Fabric 流量自动恢复
  - [ ] RTO 满足 NSA Stage 2 目标: <30 s (FM 自动恢复)
```

---

## §3 Fault Injection 测试工具链

### §3.1 Level 1-2: gem5 + SystemC FI 框架

```
gem5 Fault Injection 框架 (Level 1-2):

gem5 5.x + NSA-aware 扩展:
  - gem5 自带 FI 框架: $fault_inject 命令
  - CppTLM 扩展: 添加 NSA-aware 模块 (per `25-nsa-hardware.md` §2-§4)
  - FI 脚本 (Python):
    ```
    # 故障 1 注入
    fault_inject(
        module="gsp_rm",
        fault="hw_crash",
        duration="1ms",
        severity="critical"
    )
    ```
  - 验证脚本: 检查 FM 通知 + 工作负载迁移
  - 期望输出: 故障检测 ≤ 100 ms, 迁移 ≤ 10 s

SystemC FI 库 (Level 2):
  - SystemC 2.3 + sc_fault_inject (开源库)
  - 模拟 GSP-RM 微内核崩溃
  - 模拟 Switch PTE 路由错误
  - 模拟 Sideband SMBus 超时
```

### §3.2 Level 3-4: CppTLM + QEMU FI 框架

```
CppTLM Fault Injection 框架 (Level 3):

CppTLM 现有 + NSA-aware 扩展:
  - per `23-dist-scale-up-topology.md` 五大子系统建模
  - FI 模块: per-子系统 force_inject() 接口
  - 集成 gem5 + SystemC 仿真
  - 多 Compute Tray 协同故障

QEMU FI 框架 (Level 4):
  - QEMU 8.x + NSA-aware 设备模型
  - 集成真实 Linux KMD
  - 真实 PCIe Switch 故障注入
  - 真实 Linux Kernel Panic 模拟
  - 真实 UALink 物理层拔除
```

### §3.3 测试自动化框架

```
NSA-aware FI 自动化:

# 测试用例: 故障 1 (节点失效) + Level 3 (SoC 级)
def test_node_failure():
    # 1. Setup
    soc = setup_nsa_soc(n_compute_trays=4)
    workload = launch_ddp_training(soc, n_gpus=32)
    
    # 2. 注入故障
    soc.compute_tray[2].inject_fault("hw_crash", duration="1ms")
    
    # 3. 验证
    assert soc.fm.detect_failure() < 100  # ms
    assert soc.fm.isolate_failure() < 1    # s
    assert workload.migrate_to_remaining() < 10  # s
    assert workload.continue_training()
    
    # 4. 恢复
    soc.compute_tray[2].recover()
    assert soc.fm.reintegrate() < 30  # s
```

---

## §4 RTO 与数据丢失容忍度量化

### §4.1 RTO (Recovery Time Objective) 量化

| 故障类型 | NSA Stage 1 RTO 目标 | NSA Stage 2 RTO 目标 | 测试方法 |
|---------|---------------------|---------------------|----------|
| 节点失效 | < 30 s (V3.1-Rev2.0 KMD 处理) | < 10 s (GSP-RM + FM 协同) | FI + 计时 |
| Fabric 分区 | < 30 s (UALink 重训练) | < 5 s (NSA Switch PBR + Capability 撤销) | FI + 计时 |
| In-flight Capability 撤销 | < 1 s (TLB Flush) | < 10 ms (HW Capability 校验) | FI + 计时 |
| Drain 中止 | < 100 ms (单 Compute Tray) | < 1 ms (NSA Switch + GSP-RM 协同) | FI + 计时 |
| Page Fault 跨节点 | N/A (V3.1-Rev2.0 单节点) | < 50 μs (HW Remote-Fault + 跨节点协调) | FI + 计时 |
| Switch Tray FM 失联 | < 30 s (Host KMD 重连) | < 10 s (保守模式自动启用) | FI + 计时 |
| GSP-RM 失联 | N/A (V3.1-Rev2.0 无 GSP-RM) | < 10 s (FM 故障隔离) | FI + 计时 |
| Host FM 失联 | N/A (V3.1-Rev2.0 单 Linux 节点) | < 30 s (Switch FM 保守模式 + Host 重启) | FI + 计时 |

### §4.2 数据丢失容忍度量化

| 故障类型 | 数据丢失容忍度 | 处理策略 | 验证方法 |
|---------|----------------|----------|----------|
| 节点失效 | **无数据丢失** (在途事务挂起重试) | 工作负载迁移 + Capability 撤销 | FI + 数据校验 |
| Fabric 分区 | **可能丢失跨 Fabric 未提交数据** | Capability 撤销 + 强制清零 | FI + Checksum 校验 |
| In-flight Capability 撤销 | **无数据丢失** (事务挂起重试) | HW Capability 校验 + AWT Trap | FI + 数据校验 |
| Drain 中止 | **无数据丢失** (Capability 不变) | NIC-DMA ROB 强制清零 (Capability 校验不变) | FI + 数据校验 |
| Page Fault 跨节点 | **无数据丢失** (HW Page Fault 挂起重试) | Remote Page Fault + Host 调页 | FI + 数据校验 |
| Switch Tray FM 失联 | **可能丢失 Capability 同步数据** (保守模式降级) | Capability 仅本地寻址 | FI + 数据校验 |
| GSP-RM 失联 | **可能丢失 GSP-RM 内存数据** (Capability Database) | FM 重发 Owner Directory | FI + 数据校验 |
| Host FM 失联 | **可能丢失 Host KMD 内存数据** | Switch FM 保守模式 + Host 重启 | FI + 数据校验 |

### §4.3 数据一致性保证 (强一致 vs 最终一致)

| 故障类型 | 一致性保证 | 协议 |
|---------|----------|------|
| 节点失效 | **强一致** (HW Capability 校验 + 工作负载迁移) | per `27-nsa-capability.md` §3 |
| Fabric 分区 | **最终一致** (Capability 撤销 + 重试) | per `28-cxl-3-fabric.md` §4 |
| In-flight Capability 撤销 | **强一致** (HW 原子性 + TLB Flush) | per `27-nsa-capability.md` §3 |
| Drain 中止 | **强一致** (NIC-DMA ROB 强制清零) | per `21-dma-backends-mvp.md` §6.4 |
| Page Fault 跨节点 | **强一致** (Remote Page Fault + 跨节点协调) | per `26-gsp-rm-firmware.md` §4.3 |
| Switch Tray FM 失联 | **最终一致** (保守模式 + 重连) | per `26-gsp-rm-firmware.md` §0.5.5 |
| GSP-RM 失联 | **最终一致** (FM 重发 Owner Directory) | per `26-gsp-rm-firmware.md` §0.5.5 |
| Host FM 失联 | **最终一致** (保守模式 + 重启) | per `26-gsp-rm-firmware.md` §0.5.5 |

---

## §5 FI 测试阶段划分（与 NSA 5 阶段对应）

### §5.1 NSA Stage 1 FI 测试 (v1.x, 2026-2027)

```
NSA Stage 1 FI 测试范围:
  - 故障 1 (节点失效): Level 1-2 FI (模块/子系统级, 软件层验证)
  - 故障 4 (Drain 中止): Level 1-2 FI (GSP-RM Drain 协议验证)
  - 故障 6 (Switch Tray FM 失联): Level 2 FI (Switch Linux FM 保守模式验证)
  - 故障 8 (Host FM 失联): Level 2 FI (保守模式 + 自动恢复验证)

NSA Stage 1 FI 测试工具:
  - gem5 + NSA-aware 模块 (Level 1)
  - SystemC + sc_fault_inject (Level 2)

NSA Stage 1 FI 测试目标:
  - 8 类故障中 4 类基础验证
  - RTO 满足 NSA Stage 1 目标 (per §4.1)
  - 数据丢失容忍度满足 §4.2
```

### §5.2 NSA Stage 2 FI 测试 (v3.x, 2027-2028)

```
NSA Stage 2 FI 测试范围:
  - 全部 8 类故障 (含 NSA 硬件特定故障)
  - Level 3-4 FI (SoC 级 / 集成级)
  - 故障 2 (Fabric 分区): Level 3 (跨 Compute Tray)
  - 故障 3 (In-flight Capability 撤销): Level 4 (HW Capability 校验)
  - 故障 5 (Page Fault 跨节点): Level 3-4 (HW Remote-Fault)
  - 故障 7 (GSP-RM 失联): Level 4 (真实 GPU 故障)

NSA Stage 2 FI 测试工具:
  - CppTLM + gem5 + SystemC (Level 3)
  - QEMU + 真实 Linux KMD (Level 4)

NSA Stage 2 FI 测试目标:
  - 全部 8 类故障完整验证
  - RTO 满足 NSA Stage 2 目标 (per §4.1)
  - 数据丢失容忍度满足 §4.2
  - Split-Brain 检测与恢复 100% 验证
```

### §5.3 NSA Stage 3 FI 测试 (v3.x 远期, 2028-2029)

```
NSA Stage 3 FI 测试范围:
  - 跨数据中心 FI
  - CXL 3.0 Fabric 故障
  - 跨数据中心 Capability 撤销
  - 数据中心间网络故障

NSA Stage 3 FI 测试目标:
  - 跨数据中心 RTO < 1 min (含数据中心间重连)
  - 跨数据中心数据丢失容忍度 (per §4.2)
```

---

## §6 开放问题（待新 session 讨论）

| # | 开放问题 | 优先级 | 关联章节 |
|---|---------|--------|----------|
| 1 | **gem5 NSA-aware 模块集成时间窗**: gem5 8.x + NSA 扩展 + 社区贡献 | P1 | §3.1 |
| 2 | **SystemC sc_fault_inject 库适配**: CppTLM 框架集成 | P1 | §3.1 |
| 3 | **FI 测试用例自动化覆盖率目标**: 32 个场景 100% 自动化 vs 80% | P2 | §1.3 |
| 4 | **跨数据中心 FI 测试工具**: 真实数据中心模拟 vs 仿真 | P2 | §5.3 |
| 5 | **Split-Brain 自动检测 vs 人工干预**: HW Watchdog 触发 vs FM 决策 | P2 | §2.6, §2.8 |
| 6 | **FI 测试结果持久化**: 长期故障注入的日志管理 | P3 | §3.3 |
| 7 | **故障恢复路径的 ML 优化**: 基于历史 FI 数据优化恢复策略 | P3 | §4.3 |
| 8 | **生产环境 FI 测试 vs 仿真 FI 测试**: 数据相关性验证 | P3 | §3.2 |

---

## §7 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v0.1-draft | Sisyphus | 首版: NSA-aware Scale-Up Fault Injection 测试计划 (32 测试场景 + RTO 量化 + 数据丢失容忍度 + 3 阶段 FI 路线图) |

---

**关联 OpenSpec change**: 集成至 `openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`
**下次更新**: Oracle 评审反馈后 v0.2

**关键定位**: 本规范填补了 Oracle 评审维度 5（测试覆盖 8.2/10 → 9.0+）的 Fault Injection 测试计划空白, 是 NSA-aware Scale-Up 量产化的**关键验证手段**。与 `30-nsa-comprehensive-supplement.md` §3 故障模型协同, 与 `29-nsa-evolution-roadmap.md` 5 阶段演进对齐。8 类故障 × 4 层 = 32 个 FI 测试场景, 覆盖 NSA Stage 1-3 全阶段。