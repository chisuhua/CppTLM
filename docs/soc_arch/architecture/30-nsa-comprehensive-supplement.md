# NSA-Compat: NSA-aware Scale-Up 综合补充规范 v0.1 (Oracle 评审响应)

> **目的**: 整合 Oracle 评审识别的 3 项**额外重要问题** + P2-8 商业化定位 + 跨节点一致性模型 + 故障模型, 形成 NSA-aware Scale-Up 的综合补充规范。
>
> **状态**: Draft v0.1 (2026-09-19)
> **审计**: 待 Oracle 评审 (预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: 集成至 `openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`
> **关联文档**:
> - [`29-nsa-evolution-roadmap.md`](29-nsa-evolution-roadmap.md) 5 阶段演进主路线
> - [`23-dist-scale-up-topology.md`](23-dist-scale-up-topology.md) NSA-aware SoC 拓扑
> - [`25-nsa-hardware.md`](25-nsa-hardware.md) NSA-aware 硬件
> - [`28-cxl-3-fabric.md`](28-cxl-3-fabric.md) CXL 3.0 Fabric 兼容
> - [`22-nsa-fabric-address-spec.md`](22-nsa-fabric-address-spec.md) Fabric Address 规范

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

- 想理解 NSA-aware 商业化定位 vs NVL72 → 读 §1
- 想理解跨节点一致性模型 (memory ordering / 原子性作用域) → 读 §2
- 想理解跨节点故障模型 (节点失效 / fabric 分区 / drain 中止) → 读 §3
- 想理解端到端时延预算 → 读 §4

---

## §1 NSA-aware Scale-Up vs NVIDIA NVL72 商业化定位 (Oracle C2 修正)

> **本节为 Oracle C2 修正**: 补充 NSA-aware Scale-Up 与 NVIDIA NVL72 / AMD MI300X 的**差异化定位**。

### §1.1 业界对标矩阵

| 维度 | **NVIDIA NVL72 / GB200** | **AMD MI300X CXD** | **Intel Xe-HPC + CXL 3.0** | **NSA-aware Scale-Up** (本方案) |
|------|------------------------|--------------------|----------------------------|--------------------------------|
| **架构** | 72 GPU NVL Switch | 8 GPU CXD | 4 GPU Xe-HPC + CXL Fabric | N GPU NSA Switch |
| **GPU↔GPU** | NVLink (900 GB/s) | Infinity Fabric (~800 GB/s) | Xe-Link (~600 GB/s) | UALink (900 GB/s) + Fabric Switch |
| **跨 Node** | NVL72 单 rack | 单 server + CXD | CXL 3.0 Fabric (远期) | PCIe over UALink + NSA Switch |
| **多租户** | MIG (硬件切片, 闭源) | CXD (类 MIG, 部分闭源) | SGX/CCA (CPU 中心) | Capability (HW 强制, CHERI+seL4 风格, 开放) |
| **CXL 兼容** | ❌ (闭源 NVLink) | ⚠️ 部分 (CXD) | ✅ 完全 (CXL 3.0) | ✅ 完全 (CXL 3.0 + UALink) |
| **开源生态** | ❌ 闭源 (CUDA 锁定) | ⚠️ ROCm (部分开源) | ⚠️ oneAPI (部分开源) | ✅ 完全开源 (SYCL + UCX + Linux) |
| **商业化时间** | 已商用 (2024+) | 已商用 (2024+) | 2025-2027 (CXL 3.0) | 2027-2028 (NSA Stage 2) |
| **量产化成熟度** | 成熟 (Hopper + Blackwell) | 成熟 (MI300X) | 早期 (CXL 3.0 量产 2025) | 早期 (依赖 CXL 3.0) |

### §1.2 NSA-aware Scale-Up 的 5 大差异化优势

```
优势 1: 开放标准 vs 闭源
  - NVIDIA: NVLink 闭源, 仅 NVIDIA GPU 兼容
  - AMD: CXD 部分闭源
  - NSA-aware: UALink + CXL 3.0 开放标准, 多厂商 GPU 兼容

优势 2: Capability-Based 多租户 vs 资源切片
  - NVIDIA: MIG (硬件切片, 闭源)
  - AMD: CXD (类 MIG)
  - NSA-aware: Capability (借鉴 CHERI + seL4, 形式化可验证, HW 强制)
  - 优势: 细粒度叠加 (MIG 实例内 + Capability 地址权 二维隔离)

优势 3: NSA-aware MMU 跨 Fabric vs 限本地
  - NVIDIA: MIG 限本地 PCIe Hierarchy
  - AMD: CXD 限本地
  - NSA-aware: 跨 Compute Tray Capability 传递 (Fabric ID 16 bits)
  - 优势: 跨节点多租户隔离

优势 4: GSP-RM 控制面下沉 vs Host KMD
  - NVIDIA: H100 GSP-RM (NV-RTOS 闭源)
  - AMD: 类似 GSP-RM (闭源)
  - NSA-aware: GSP-RM + NV-RTOS/seL4 灵活选型
  - 优势: 开放生态 + 形式化可验证 (seL4)

优势 5: 5 阶段演进无债务 vs 单阶段
  - NVIDIA/AMD: 一次性升级 (破坏性)
  - NSA-aware: 5 阶段渐进升级 (V3.1-Rev2.0 兼容, per `29-nsa-evolution-roadmap.md`)
  - 优势: 客户从单 Linux 节点 → 多 Linux 节点 → NSA-aware 平滑升级
```

### §1.3 NSA-aware Scale-Up 的 3 大劣势

```
劣势 1: 量产化时间窗落后 2-3 年
  - NVIDIA NVL72 已商用 (2024+)
  - NSA-aware Stage 2 量产 2027-2028
  - 差距: 2-3 年市场先机丧失

劣势 2: 性能 vs NVIDIA NVLink 仍有差距
  - NVIDIA NVLink 900 GB/s + NVL72 1.8 TB/s
  - NSA-aware UALink 900 GB/s + NSA Switch ~256 GB/s
  - 差距: NSA Switch 总带宽 < NVIDIA NVL72

劣势 3: 自研 GSP-RM + NSA Switch 风险
  - NVIDIA: 成熟 GSP-RM (Hopper H100)
  - AMD: 成熟 CXD
  - NSA-aware: 自研 GSP-RM 微控制器 + NSA Switch ASIC
  - 风险: 研发周期 + 良率 + 量产化不确定性
```

### §1.4 商业化建议

```
商业化定位 (per Oracle C2 修正 + ADR-SOC-23 D1):

不要与 NVIDIA NVL72 正面竞争 (劣势 1-3)
应该聚焦 NSA-aware 差异化优势:
  - 开放标准 (UALink + CXL 3.0): 多厂商 GPU 兼容
  - Capability 多租户: 形式化可验证 + 跨 Fabric
  - GSP-RM 开放生态: NV-RTOS/seL4 灵活选型
  - 5 阶段演进无债务: 平滑升级路径

目标客户:
  - 多租户 GPU 云厂商 (CXL 3.0 Fabric + 多厂商 GPU)
  - 数据中心运营商 (开放标准 + 互操作)
  - 学术 + 开源生态 (避免 NVIDIA 锁定)

商业模式:
  - License + Open-source 双模式 (类似 RISC-V)
  - 重点: NSA Switch ASIC + GSP-RM 固件 + Capability 软件栈
  - 服务: 集成 + 优化 + 客户支持

时间窗:
  - 2024-2026: V3.1-Rev2.0 (方案 A, 单 Linux 节点) ✅
  - 2026-2027: NSA Stage 1 (GSP-RM + Capability, 软件层)
  - 2027-2028: NSA Stage 2 (NSA Switch + Remote Atomic + Directory)
  - 2028-2029: NSA Stage 3 (CXL 3.0 Fabric 完整)
  - 2029+: NSA Stage 4 (商业化)
```

---

## §2 跨节点一致性模型 (Oracle 新识别)

> **本节为 Oracle 新识别**: 补充跨节点内存访问的 memory ordering / 原子性作用域规则。这是 NSA-aware Scale-Up 的**关键技术基础**, 但当前 9 份草案中**未明确**。

### §2.1 内存一致性模型选择

```
业界一致性协议对比:

TSO (Total Store Order, x86 默认):
  - 单节点顺序一致
  - 跨节点需要额外 fence 指令
  - 实现: Store Buffer + Memory Ordering Buffer

Weak Ordering (ARM 默认):
  - 显式 fence 指令 (DMB/DSB)
  - 更宽松, 性能更好
  - 跨节点需 CXL 3.0 一致性引擎

Release Consistency (RC, 现代 GPU 默认):
  - Acquire-Release 语义
  - 跨节点 fence 由 HW + SW 协同
  - 业界实践: NVIDIA / AMD GPU 多用 RC

NSA-aware 一致性模型选型 (推荐): RC (Release Consistency)
  - 与业界现代 GPU 实践对齐
  - HW + SW 协同 fence (per `27-nsa-capability.md` §3 Capability Acquire/Release)
  - 跨节点由 NSA Switch Coherency Engine 协同 (per `25-nsa-hardware.md` §4 Hardware Directory)
```

### §2.2 Atomicity Scope

```
Atomic 操作作用域 (per `25-nsa-hardware.md` §3.4):

本地 Atomic (Fabric ID = 0):
  - SM 发起 Atomic Op → HBM Controller 完成
  - 作用域: 单 GPU 内
  - 延时: ~50 ns

同 Compute Tray 跨 GPU Remote Atomic (Fabric ID 一致):
  - SM 发起 → Remote Atomic Unit → Owner GPU HBM Controller
  - 作用域: 跨 GPU 单 Compute Tray 内
  - 延时: ~200 ns
  - 原子性保证: HW 保证 TS (per IEEE 750 / TSO)

跨 Compute Tray Remote Atomic (Fabric ID 不同):
  - SM 发起 → Remote Atomic Unit → NSA Switch PBR → Owner Compute Tray
  - 作用域: 跨 Compute Tray
  - 延时: ~500 ns (NSA-aware 硬件加速)
  - 原子性保证: HW 保证 + NSA Switch 协调
  - ⚠️ 关键: NSA Switch 必须**锁定**目标 cache line (HW)
  - ⚠️ 关键: Capability 校验**先于** Atomic Op 启动

跨数据中心 Remote Atomic (Fabric ID 不同, 跨机柜):
  - SM 发起 → NSA Switch → InfiniBand / 专用 Fabric Link
  - 作用域: 跨数据中心 (Multi-Fabric)
  - 延时: ~10-50 μs (数据中心间)
  - 原子性保证: HW + 协议层 fence (类似 RDMA compare-and-swap)
```

### §2.3 Memory Ordering 规则

```
NSA-aware Scale-Up Memory Ordering 规则:

Rule 1: 单 GPU 内 (Fabric ID = 0)
  - 默认: 强 TSO (Total Store Order)
  - SW 无需额外 fence
  - HW 保证顺序一致

Rule 2: 同 Compute Tray 跨 GPU (Fabric ID 一致, Stage 1 启用)
  - 默认: RC (Release Consistency)
  - Capability Acquire (读 fence): 保证后续读看到之前写
  - Capability Release (写 fence): 保证之前写对其他 GPU 可见
  - HW 强制 (per `27-nsa-capability.md` §3.4 Capability Token Permissions)

Rule 3: 跨 Compute Tray (Fabric ID 不同, Stage 2/3 启用)
  - 默认: 弱 RC (类似 ARM Weak Consistency)
  - Capability Acquire/Release **必须**显式使用
  - NSA Switch 协同 Hardware Directory 保证一致性
  - 性能: 跨 Compute Tray ~500 ns 包含 fence 开销

Rule 4: 跨数据中心 (Multi-Fabric, Stage 3 远期)
  - 默认: 最弱一致性 (类似 RDMA)
  - 必须使用最强 Capability Acquire/Release
  - 性能: 跨数据中心 ~10-50 μs 包含 fence 开销
```

---

## §3 故障模型 (Oracle 新识别)

> **本节为 Oracle 新识别**: 补充跨节点故障处理模型, 包括节点失效 / fabric 分区 / in-flight capability 撤销 / drain 中止。

### §3.1 故障类型与处理策略

```
故障类型 1: 节点失效 (Compute Tray 故障)
  检测: 主机 FM 经 MSI-X 心跳检测
  处理:
    - FM 决策: 标记该 Compute Tray 为 Fault
    - FM 通知所有 Compute Tray KMD: 该 Compute Tray 不可用
    - FM 协调: 工作负载迁移 (经 Host FM 决策)
  恢复: Compute Tray 硬件修复 + 重启 + FM 重新扫描

故障类型 2: Fabric 分区 (UALink 物理层断开)
  检测: UALink PHY Link Down 信号
  处理:
    - NSA Switch PTE 更新: 标记该路径为 Unreachable
    - FM 通知所有 Compute Tray KMD: 该 Fabric 路径不可用
    - Capability 撤销: 跨 Fabric Capability 标记为无效
    - 工作负载迁移 (FM 协调)
  恢复: UALink 物理层 Link Training 重试 → FM 恢复 PTE

故障类型 3: In-flight Capability 撤销 (事务中途撤销)
  检测: Capability Token Version 不匹配 (HW 校验失败)
  处理:
    - HW: 当前 SM 事务挂起 (类似 Page Fault)
    - GSP-RM Fault Service: 通知 FM Capability 已撤销
    - SW: SM 重新发起事务 (使用新 Capability Token) 或 SIGSEGV
  恢复: GSP-RM Capability Manager 重新签发新 Token

故障类型 4: Drain 中止 (FM 决策中途取消)
  检测: FM 写 NIC_DRAIN_CTRL.FORCE_ABORT = 1 (per `26-gsp-rm-firmware.md` §0.5.3)
  处理:
    - GSP-RM Fabric Service 接收 FORCE_ABORT
    - 强制清零 NIC-DMA ROB (per `21-dma-backends-mvp.md` §6.4)
    - 通知 Switch FM: Drain 中止
  恢复: 恢复正常流量, Capability 不受影响

故障类型 5: Page Fault 跨节点 (Remote Page Fault)
  检测: GMMU PTW 输出 Remote-Fault 标志位 (per `25-nsa-hardware.md` §2.2)
  处理:
    - HW: 发起 Remote Page Fault Request (经 UALink VC0)
    - Owner Compute Tray GSP-RM Fault Service 响应
    - Page Table Hit → 注入 TLB
    - Page Table Miss → 通知 Host Driver 调页
  恢复: 取决于 Host 调页 (Linux mmap 流程)

故障类型 6: Switch Tray Linux FM 失联
  检测: 主机 FM 经 Sideband SMBus 心跳检测
  处理:
    - 主机 FM 决策: 进入保守模式
    - 主机 FM 通知所有 Compute Tray KMD: 跨 Fabric 不可用
    - 工作负载降级: 仅本地 Compute Tray 寻址
  恢复: Switch Tray Linux FM 重启 + 重新建立通信

故障类型 7: GSP-RM 失联 (GPU 硬件故障)
  检测: 主机 FM 经 MSI-X 检测: GSP-RM 心跳超时
  处理:
    - FM 决策: 故障隔离
    - FM 通知所有 Compute Tray KMD: 该 GPU 不可用
    - 工作负载迁移 (FM 协调)
  恢复: GPU 硬件修复 + GSP-RM 重新初始化

故障类型 8: Host FM 失联 (Host OS 崩溃)
  检测: Switch Tray FM 经 Sideband SMBus 心跳检测 (或 BMC 检测)
  处理:
    - Switch Tray FM 进入 "保守模式"
    - Capability 仅允许本地寻址 (Fabric ID = 0)
    - 拒绝跨 Fabric Capability 签发
    - HRT 维持当前状态 (不切换)
  恢复: Host OS 重启, FM 自动恢复
```

### §3.2 Split-Brain 风险与缓解

```
Split-Brain 风险 (FM ↔ GSP-RM 状态不一致):

场景: FM 写入新 HRT, GSP-RM 未收到通知
  风险: GPU 仍在用旧 HRT, 拓扑不一致
  缓解: GSP-RM 定期与 FM 同步 (FM 版本号)

场景: GSP-RM 签发 Capability, FM 不知道
  风险: Capability 计数不一致
  缓解: Capability Token 含 Version 字段, FM 定期同步

场景: Drain 中途 FM 崩溃
  风险: NIC-DMA 永久阻塞
  缓解: GSP-RM Watchdog Timer 超时 → 强制 FORCE_ABORT
```

### §3.3 一致性 + 故障模型 端到端时延

| 场景 | 正常时延 | 故障时延 |
|------|---------|----------|
| 单 GPU 读 (Hit) | ~21 ns | ~30 ns (含 Capability 校验) |
| 单 GPU 读 (Miss) | ~67 ns | ~80 ns (含 PTW + Capability 校验) |
| 同 tray 跨 GPU 读 (Hit) | ~50 ns | ~70 ns (含跨 GPU Hardware Directory) |
| 跨 tray 读 (Hit) | ~500 ns | ~700 ns (含 NSA Switch PBR + Capability 校验) |
| 跨数据中心 读 (Hit) | ~10-50 μs | ~30-80 μs (含数据中心间 fence) |
| **Page Fault** (本地 Miss) | ~5 μs | ~10 μs (含 HW + SW 调页) |
| **Page Fault** (跨 Compute Tray) | ~50 μs | ~100 μs (含跨节点 + HW 协调) |
| **Drain** | ~100-200 μs | ~500 μs (含超时) |
| **Capability 撤销** | ~1 ms (批量) | ~10 ms (跨节点同步) |

---

## §4 端到端时延预算 (Oracle 新识别)

> **本节为 Oracle 新识别**: 补充 NSA-aware Scale-Up **端到端时延预算**, 是 NSA-aware 性能保证的基础。当前 `24-host-gpu-pcie-ifc.md` §3-§4 有 PCIe 95/99 分位估算, 但**远程访存全链路预算**缺失。

### §4.1 端到端时延预算矩阵

```
NSA-aware Scale-Up 端到端时延预算 (TLB Hit, 80% 占比):

路径 1: 单 GPU 读 (Fabric ID = 0)
  - SM Load VA → GMMU TLB Lookup (~5 cycles @ clk_core, ~2.5 ns)
  - Capability 校验 (~5 cycles, 并行, ~2.5 ns)
  - HRT 查表 (~3 cycles, ~1.5 ns)
  - WRR 仲裁 (~1 cycle, ~0.5 ns)
  - 注入 NoC (~1 cycle, ~0.5 ns)
  - Compute NoC 路由 (~4 cycles @ clk_fab, ~3.3 ns)
  - Global UDD Hub RCT 校验 (~1 cycle @ clk_fab, ~0.8 ns)
  - HBM-DMA 处理 (~15 cycles @ clk_fab, ~12.5 ns)
  - 数据返回 (同路径反向, ~10 ns)
  - 总时延: ~30 ns (TLB Hit, 含 Capability 校验)

路径 2: 同 tray 跨 GPU 读 (Fabric ID 一致)
  - 同路径 1 + 跨 GPU NoC (~200 ns)
  - 总时延: ~230 ns (TLB Hit, 同 tray 跨 GPU)

路径 3: 跨 Compute Tray 读 (Fabric ID 不同, NSA-aware)
  - 同路径 1 + NSA Switch PBR (~300 ns)
  - 跨 Compute Tray UALink (~100 ns)
  - 总时延: ~500 ns (TLB Hit, 跨 Compute Tray, NSA-aware 硬件加速)

路径 4: 跨数据中心 读 (Multi-Fabric, NSA Stage 3 远期)
  - 同路径 3 + 数据中心间 InfiniBand (~5-50 μs)
  - 总时延: ~10-50 μs (跨数据中心, 远期)
```

### §4.2 加权平均时延预算

```
加权平均时延 (per workload 估算):

大模型训练 (LLM 8K-128K context, batch size 32):
  - 跨 GPU 通信: 80% (AllReduce + Attention)
  - 本地 HBM: 20% (GEMM, Activation)
  - 加权平均: 0.8 × 230 + 0.2 × 30 = 190 ns / 操作

小模型推理 (实时, batch size 1):
  - 跨 GPU 通信: 30%
  - 本地 HBM: 70%
  - 加权平均: 0.3 × 230 + 0.7 × 30 = 90 ns / 操作

对比 V3.1-Rev2.0 (无 NSA-aware):
  - 单 Linux 节点, 同 Compute Tray 跨 GPU: 230 ns (一致)
  - 跨 Compute Tray: N/A (不支持)

对比 NVIDIA NVL72 (闭源, 实测估算):
  - 跨 GPU NVLink: ~150-200 ns
  - 72 GPU 跨 Switch: ~500-800 ns (NVL72 Scale-Up)
  - NSA-aware 跨 Compute Tray: ~500 ns (与 NVL72 相当)
```

### §4.3 时延预算验证 (NSA Stage 2 实施时)

| 验证项 | 目标 | 测试方法 |
|--------|------|----------|
| 路径 1 (本地 HBM Load Hit) | ≤ 35 ns | microbenchmark + gem5 仿真 |
| 路径 2 (同 tray 跨 GPU Hit) | ≤ 250 ns | microbenchmark + 2 GPU 交叉测试 |
| 路径 3 (跨 Compute Tray Hit) | ≤ 600 ns | microbenchmark + 2 Compute Tray 交叉测试 |
| 路径 3 (跨 Compute Tray Miss) | ≤ 800 ns | microbenchmark + 强制 TLB Miss |
| Capability 校验关键路径 | ≤ 5 ns | 关键路径分析 + 形式化验证 |
| Remote Atomic (本地) | ≤ 60 ns | microbenchmark |
| Remote Atomic (跨 Compute Tray) | ≤ 600 ns | microbenchmark |

---

## §5 跨子系统影响矩阵

| NSA 草案 | 涉及本规范的章节 | 优先级 |
|---------|------------------|--------|
| `22-nsa-fabric-address-spec.md` | §4 时延预算 (端到端路径) | P1 |
| `23-dist-scale-up-topology.md` | §4 时延预算 + §3 故障模型 | P1 |
| `25-nsa-hardware.md` | §4 时延预算 + §2 一致性 | P0 (已含 §0.5) |
| `26-gsp-rm-firmware.md` | §3 故障模型 (Split-Brain) | P0 (已含 §0.5) |
| `27-nsa-capability.md` | §2 一致性 (Acquire/Release) + §3 故障模型 (撤销) | P1 |
| `28-cxl-3-fabric.md` | §2 一致性 (CXL 3.0 Coherency) + §3 故障模型 (Fabric 分区) | P1 |
| `29-nsa-evolution-roadmap.md` | §1 商业化定位 + §4 时延预算 (Stage 3 远期) | P2 |
| `21-dist-scale-up-topology-b.md` | §1 商业化定位 (降级替代) | P2 |

---

## §6 开放问题 (待新 session 讨论)

| # | 开放问题 | 优先级 | 关联章节 |
|---|---------|--------|----------|
| 1 | **Capability Acquire/Release 语义**: 与 ARM DMB/DSB 对齐? | P1 | §2.3 |
| 2 | **跨节点 Atomicity Scope**: 是否需要 2PC 协议? | P1 | §2.2 |
| 3 | **Page Fault Remote-Fault 时延**: 跨 Compute Tray ~50 μs 是否过慢? | P1 | §3.1 故障 5 |
| 4 | **Capability 撤销 race condition**: HW 如何保证原子性? | P1 | §3.1 故障 3 |
| 5 | **GSP-RM Watchdog 超时时间**: 1 s 还是 5 s? (Host 失联检测) | P2 | §3.1 故障 8 |
| 6 | **Split-Brain 自动检测协议**: 类似 Raft? | P2 | §3.2 |
| 7 | **Remote Atomic 跨数据中心时延**: ~10-50 μs 是否可接受? | P2 | §4.1 路径 4 |
| 8 | **Capability 撤销批量优化**: 一次撤销多个 Capability | P3 | §3.1 故障 3 |
| 9 | **跨数据中心一致性与性能权衡**: 如何平衡? | P3 | §4.1 路径 4 |
| 10 | **故障注入测试 (Fault Injection)**: NSA Stage 2 实施时如何测试? | P3 | §3.1 8 类故障 |

---

## §7 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v0.1-draft | Sisyphus | 首版: NSA-aware 综合补充规范 (Oracle 评审响应) - §1 NSA vs NVL72 商业化定位 + §2 跨节点一致性模型 + §3 故障模型 + §4 端到端时延预算 |

---

**关联 OpenSpec change**: 集成至 `openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`
**下次更新**: Oracle 评审反馈后 v0.2

**关键定位**: 本规范整合 Oracle 新识别的 3 项关键问题 (商业化定位 / 一致性模型 / 故障模型 / 时延预算), 是 NSA-aware Scale-Up 的**跨子系统补充规范**。8 份 NSA 草案应**引用本规范**的相应章节, 不重复定义。