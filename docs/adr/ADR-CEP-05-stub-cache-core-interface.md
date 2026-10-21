# ADR-CEP-05: Stub Cache Core 接口(RN 侧投影 + 折叠映射表)

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 1:`06-inter-module-network.md` v0.4 §2.1 L1-L6 仿真层(正交,非 Stub Cache 真源)
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3(锚点 per `ADR-CEP-00A §3.2` — **§11.5 是"相关测试",非 P3-P6 演进路线,全文 grep "barrier" 0 匹配,per oracle 评审 B2-3 实证修订**)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口,草案)
> - 真源 3:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射,本批起草)
> - 真源 3:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单,本 ADR 折叠映射表逐字引用)
> - 真源 3:`ADR-CEP-04-sharer-tracker-abstraction-redesign.md`(lookup/snoop 接口 contract 联动)
> - 真源 3:`ADR-CEP-06-experiment-protocol.md`(实验输出,本批起草)
> - 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(仿真语义 + NullProtocolEngine,本批起草)
> - 真源 3:`ADR-X.1-transaction-id.md`(txn_id 命名空间,per oracle S4 修订)

---

## 1. 背景

### 1.1 关于"06 v0.4 §15.4 P3 Stub Cache 5 接口"伪引的修订(per oracle B4)

**经 oracle 评审实证**:`06-inter-module-network.md` v0.4 §15.4(行 1168-1222)P3 阶段范围是 "SnoopFilter directory-based + MesiGpuProtocol + MesiThreeLevelProtocol + translate_snoop 协议转换",**全文无 "Stub Cache" / "request_line"**;P3 阶段范围被 `ADR-CEP-04` 完整继承(per ADR-CEP-00 §3 supersede 范围映射表)。

**结论**:Stub Cache Core 接口集(`request` / `snoop` / `snoop_resp` / `writeback` / `completion` / `evict_line`)**由本 ADR 首次定义**(per oracle B4 修复方向 + oracle B2-10 修订:逐字对齐 `ADR-CEP-02 §2` canonical 10 流清单),作为 Stub Cache 仿真骨架的 canonical 契约。

### 1.2 三方案对比

| 维度 | (α) RN 侧投影 | (β) 独立接口 + 适配 | (γ) 全集直通 |
|---|---|---|---|
| 与阶段 1 ADR 一致性 | ✅ 完全吻合 | ✅ 兼容 | ❌ 违背 |
| 协议无关性 | ✅ 高 | ✅ 最高 | ❌ 无 |
| 词汇表数量 | 1.5(共享 + 少量内部) | 2(两份长期 drift) | 1(语义污染) |
| α 失效判据 | forward_data 不可分辨时失效 | — | — |

**否决 β**:5 接口与 CohIf RN 子集语义必然同构,独立命名只是换皮,换来永久性两份词汇对齐负担(放大 A13 风险)。独立演进价值只在接非 CohIf 协议层时兑现 — 假设性需求,YAGNI 不该现在付费。

**否决 γ**:forward_data 在 CHI 类协议里是"数据来自 peer cache 而非 HN"。若 stub 直接感知,stub 的 hit/miss 决策就开始依赖数据来源 — stub 不再是缓存模型,退化成半个协议引擎。**直接违背阶段 1 ADR "核心只感知 accept/retry/complete"**。

---

## 2. 决策

> **⚠️ v0.3 定位澄清(per 调研 F12)**:Stub Cache Core **不是缓存性能模型**,而是**客户端侧协议状态投影**(RN projection)。它只维护 **5** 个**稳定**权限状态(`NONE`/`I`/`S`/`E`/`M`)+ 有限的 outstanding 语义,用于驱动 Protocol Engine;**真实协议的瞬态状态全部住在 Protocol Engine 插件内**。
>
> **量级参照**:真实协议的非 stalling MSI cache controller 约 **18-20 状态 / 46-60 转换**(per ProtoGen ISCA 2018 实测);CEP Stub 的 **5** 个稳定态与之相差一个数量级 —— 这是**刻意的协议无关化简化**,不是建模缺陷。
>
> **对现有 6 条 `INV-SC*` 的影响:无** —— INV-SC1~INV-SC6 全部是**行为约束**(outstanding 上限 / evict 选择 / snoop 延迟 / 配对 / writeback 载荷 / hit 路径零流量),**没有一条假设 Stub 是真实缓存**。
>
> **INV-SC1 的刻意的保守性**(补充说明):"同一 addr 最多 1 个 outstanding request"比真实协议保守 —— 真实 non-stalling 协议中同一 block 可有多个并发事务,但 cache 侧并发度被限制为 "one outstanding transaction per block"(per ProtoGen 论文 §V-C)。CEP 取同一条保守线,是**有先例的选择**而非疏漏。


### 决策 1:(α) RN 侧投影(α')+ 命名复用 CohIf canonical 10 流清单

**Stub Cache Core 接口**(修订后,**逐字对齐 `ADR-CEP-02` §2 canonical 10 流清单**,per oracle B8 修订):

| 接口(修订) | 原命名 | 归属 | CohIf canonical 流对应 |
|------------|--------|------|---------------------------|
| `request(addr, perm)` | `request_line` | net-side | 流 1 `request` |
| `snoop(addr, type, txn_id)` | `snoop_query` | net-side | 流 3 `snoop` |
| `snoop_resp(addr, hit, state, dirty, data, txn_id)` | (合并 snoop 响应)| net-side | 流 4 `snoop_resp`(per oracle B2-10 修订:逐字对齐 canonical)|
| `writeback(addr, data, dirty)` | `writeback_data` | net-side | 流 5 `writeback` |
| `completion(txn_id, data, perm, error)` | `complete` | net-side | 流 2 `completion` |
| `evict_line(addr)`(内部策略触发)| `evict_line` | **core-side 原语**(非 CohIf 流投影)| 内部命令,net-side 表现为流 5 `writeback` + 流 2 `completion` |

**net-side 5 条 CohIf 投影 + core-side 1 条内部命令 = 6 接口总数**(修订:初版写 5 接口,实为 6,补 `snoop_resp` 显式列入 — per oracle B2-10 修订:逐字对齐 canonical 流名)。

**命名修订表**(避免 A13 风险):

| 原命名 | 修订 | 理由 |
|--------|------|------|
| `complete` | `completion` | 与 CohIf canonical 流名逐字一致(per ADR-CEP-02 §2 流 2)|
| `snoop_query` | `snoop` | query 后缀暗示独立响应接口;但响应是返回值,不是独立接口 |
| `request_line` | `request` | "line" 是实现细节,接口层不该泄漏 |
| `evict_line` | `evict_line`(保留)| 唯一 core-side 原语,非 CohIf 流投影 |

### 决策 2:三明治挂载形态(Stub 双端口)

```
Traffic Generator ──(cpu_side: read/write/evict)──> Stub Cache
                                                          │
                                    (net_side: request/snoop/`snoop_resp`/writeback/completion)
                                                          │
                                              ProtocolEngine ──> CohIf (canonical 10 流)
```

- **Stub 双端口不同语义**:
  - `cpu_side`:功能操作(读/写/驱逐),阻塞式
  - `net_side`:CohIf RN 投影 5 条
- **TG 只驱 cpu_side**,完全不知道 CohIf 存在
- **ProtocolEngine 只实现 net_side**,完全不知道 TG 存在
- **hit 路径零网络流量天然正确**

### 决策 3:折叠映射表(α 方案正确性的唯一审计点,per oracle B8 修订:逐字对齐 ADR-CEP-02 canonical 10 流)

**5 隐藏流 → stub 原语**(per `ADR-CEP-02` §2 canonical 10 流清单编号):

| CohIf canonical 流 | stub 表现 | 关键不变量 |
|-------------------|----------|-----------|
| 流 6 `forward_data` | 不可显式为接口,stub 收到流 2 `completion` 时数据可用即视为"forward 完成或 memory 完成" | **CEN-Ib6: forward_data → completion 不可分辨性** |
| 流 7 `back_invalidate` | 表现为普通 invalidate snoop(走流 3 `snoop` + 入参 invalidation)| stub 无协议特异行为 |
| 流 9 `barrier` | 对 stub 不可见 | barrier 穿插时 stub 的 MSHR 占用、snoop 响应、写回顺序均不变 |
| 流 8 `address_resolve` | 在 ProtocolEngine 内终结 | 不泄漏到 stub 任何接口 |
| 流 10 `snoop_completion_ack` | 由 ProtocolEngine 内部消化,与 stub `snoop_resp` 配对 | stub 不知道 snoop ack 单独流存在 |

**关键简化**(per oracle 验证建议 V1):forward_data 不可分辨性是 α 方案最关键简化 — stub 收到的 completion 无论从 memory 还是 peer 转发,stub 状态迁移与数据落位逐位一致。**契约测试 1 必须钉死**。

### 决策 4:bypass 模式作为 testbench 配置选项

- **保留为 testbench 配置选项**(不作为运行态开关)
- TG 可绕过 stub 直驱 CohIf,用于 ProtocolEngine 独立测试
- bypass 模式下 stub 仍正常工作(用于双路径对比验证)

---

## 3. 10 条契约测试清单(per oracle A7 §5 + oracle S8 修订:优先级统一)

**优先级统一**(per oracle S8 修订):本节"必做前 2 条"+ 决策汇总"优先 1+8"**冲突**,按本节为准 — "必做前 2 条"为 v0.1 首版强调;**v0.2 统一为"必做前 2 条 + 高优先级次轮 8"**,总优先级 1→2→8→others。

按"折叠表"组织,每条不暴露的流一个折叠断言:

### 3.1 必做(高优先级,前 2 条,per oracle S8 修订统一)

1. **`forward_data` → `completion` 不可分辨性**(对应流 6 → 流 2)
   - 断言:stub 收到的 completion 无论从 memory 还是 peer 转发,stub 状态迁移与数据落位逐位一致
   - 这是 α 方案最关键的简化,**必须钉死**
2. **写回与 snoop 竞态**(对应流 5 `writeback` × 流 3 `snoop`)
   - 断言:evict 进行中收到同地址 snoop 的行为契约(经典 bug 点)
   - 涉及 MSHR 状态机 + snoop 响应路径的并发安全性

### 3.2 高优先级(次轮 8 条)

3. `back_invalidate` → `snoop(invalidate)`(对应流 7 → 流 3)
4. `barrier` 对 stub 不可见(对应流 9)
5. `address_resolve` 在 ProtocolEngine 内终结(对应流 8)
6. **配对性**:stub 收到的每个 completion 必对应其发出的未完成 request(`txn_id` 在 ProtocolEngine,但 stub 侧"无 unsolicited completion"是 stub 依赖的契约 — per oracle S8 修订:统一为 ProtocolEngine + `txn_id` snake_case)
7. **accept/retry/complete 三态**:retry 后 adapter 负责重发,stub 不重复感知;complete 前数据可用
8. **snoop 响应完备性**:hit-clean / hit-dirty / miss 三态 + dirty 数据返回路径
9. **evict 与未完成 request 互斥**:MSHR 有 outstanding 的 line 不可被驱逐策略选中
10. **completion 乱序容忍**:CohIf 不保证顺序时 stub 行为正确

---

## 4. Stub Cache Core 内部状态机(**本 ADR 首次定义**,范围取舍参考 `06 §15.4 P3` 的 stub 精神 — **非内容继承**,per oracle B2-9 修订:初版 §1.1 已修订但 §4 §4.2 残留"继承"措辞)

### 4.1 缓存行状态(协议无关)

```
NONE → I → S → E → M(per MESI)
```

**禁止**:暴露 CHI UC/SC/UD/SD 七态,UC/SC/UD/SD 是 CHI RN-F 细节,放在 ProtocolEngine 适配器层。

### 4.2 MSHR / 写回缓冲 / 替换策略(**本 ADR 首次定义**,per oracle B2-9 修订)

- **保留**:64B cache line + tag hit/miss + MSHR outstanding 限制 + 写回缓冲 + snoop 查询 + 固定查找/响应延迟
- **不实现**:完整 SRAM 阵列 + 多路组相联细节 + 复杂预取器 + 精确功耗模型 + 完整流水线时序

### 4.3 接口一致性不变量(per oracle S4 修订:统一 txn_id 命名)

| 不变量 | 描述 |
|--------|------|
| **INV-SC1** | 同一 addr 在 stub 内部任何时刻最多 1 个 outstanding request |
| **INV-SC2** | evict 不允许选取 outstanding line |
| **INV-SC3** | snoop 响应必须在固定延迟内返回(参数化)|
| **INV-SC4** | completion 与 request 严格配对(无 unsolicited completion);`txn_id` 沿用 `ADR-X.1-transaction-id.md` snake_case 命名 |
| **INV-SC5** | dirty writeback 必须带 data;clean writeback 不带 |
| **INV-SC6** | hit 路径不产生网络流量(cpu_side 直接返回)|

**INV-CEP-05-IC1**(新增,2026-10-20 落位):**Stub Cache Core 只暴露稳定状态**;协议的**瞬态状态**(per ProtoGen 的 transient states,如 ArchForge 09 MOESI/GPU 的 `T`)与**协议私有状态**(token 等)一律住在 **Protocol Engine 插件内部**,不泄漏到 Stub 的 4 值 `perm` 枚举。
- **来源**:`ADR-CEP-17 §2 Decision 4`;一手依据 ProtoGen ISCA 2018 p.1(真实 non-stalling MSI 约 18-20 状态 / 46-60 转换,per p.12)
- **落位说明**:此前仅在 `ADR-CEP-17 §3.1` 声明,未落 owner ADR(per oracle 全面架构审查 N1)。本条为**补落**,非新增。

---

## 5. Traffic Generator 接入

```systemverilog
interface StubCacheCpuSideIf;
  // TG 驱动接口
  task cpu_read(addr, size);
  task cpu_write(addr, data, size);
  task cpu_evict(addr);  // 内部命令,触发 stub eviction 策略
endinterface
```

**TG 配置**:
- 合成流量:随机读/写、snoop storm、混合读写比例
- 真实流量:从 workload trace replay
- 参数化:命中率、MSHR 容量、响应延迟

---

## 6. 折叠映射表的实施审计(per oracle N5 修订:统一 ProtocolEngine 术语)

**ProtocolEngine 实施检查清单**(per oracle N5:`adapter` / `ProtocolEngine` 统一为 `ProtocolEngine`):

1. ✅ forward_data 处理在 ProtocolEngine 内完成(adapter 内部 — **per N5 修订**统一称 "ProtocolEngine")
2. ✅ back_invalidate 映射为普通 invalidate snoop
3. ✅ barrier 在 ProtocolEngine 内消化,无 stub 可见信号
4. ✅ address_resolve 在 ProtocolEngine 内终结,无 stub 可见信号
5. ✅ 配对性 check:ProtocolEngine 维护 txn_id ↔ stub request 映射表
6. ✅ retry 重发逻辑在 ProtocolEngine 内,stub 不感知重复

**第二协议接入时退化点**:ProtocolEngine 实现不严 → 折叠表任一条破裂 → α 方案退化为 γ(违背"核心只感知 accept/retry/complete")。**折叠表是审计红线**。

---

## 7. α 失效判据(per oracle A7)

如果未来某个协议要求 stub 区分"数据来自 peer vs memory":
- 统计 sharing 行为
- cache-to-cache 优化决策

→ forward_data 不可分辨性(契约测试 1)破裂 → 重新评估 (β) 独立接口方案

**当前不触发该判据**:CHI / CXL / TileLink / NVLink / UALink 都不需要 stub 区分数据来源(数据来源对 stub 透明)。

### α 失效判据 2(E 态静默升级,2026-10-20 新增,per oracle 全面架构审查 N7)

**方向**:本判据覆盖 α 方案的**另一侧** —— 不是「协议信息泄漏给 stub」(判据 1),而是「**stub 可见信息不足以支撑协议侧的正确判断**」。

**判据**:若组合协议需要区分「**静默** E→M 升级」与「**显式**请求升级」,则 Stub 的 4 值 `perm` 表达力不足 → 重新评估 (β) 独立接口方案,或引入 `perm` 的「可静默升级」标志位。

- **一手依据**:`INV-CEP-05-IC1`(瞬态态住 Protocol Engine 插件内)+ 调研 F5(E 态静默升级是分层组合破坏全局 SWMR 的具名机制;一手:HieraGen §V-D)
- **为何必须补**:α 方案的信息边界原本是**单向**的(只防「协议 → stub」泄漏),缺「stub → 协议」不足这一侧的判据。补上后 α 方案才是**双向可证伪**的
- **当前不触发**:§5.1.1 / §5.1.2 的 14 个 adapter 实例均无「静默 E→M」需求(工业协议族为显式请求升级);该判据在 `ADR-CEP-08` 的组合检查落地后才可能被触发

---

## 8. 抓取点

**高 impact / 低 risk**:
- 命名复用 CohIf canonical 10 流清单(一轮改名消除 A13 风险,逐字对齐 `ADR-CEP-02` §2)

**高 impact / 中 risk**:
- 折叠映射表作为独立规范文档(放进 `ADR-CEP-02` Status Update 或新 ADR-I0x)— **这是 ProtocolEngine 正确性的唯一审计点**

**Medium**:
- 接口分组显式化(net-side 5 + core-side 1)
- 6 条 Stub Cache 不变量(§4.3)

**Low / optional**:
- TG ↔ stub ↔ CohIf 三明治挂载 + bypass 测试模式

---

## 9. 与 CppTLM 现有 CacheTLM 对齐(待 A13 决议 — per oracle S1 修订)

**当前状态**(per oracle S1):A1 归口已在 `ADR-CEP-00` §5 拍板;但 A13 与现有 CacheTLM 对齐**仍待决议**(不依赖 A1)。

**当前默认**:沿用 06 §15.4 P3 stub 精神(per oracle B2-9 修订:06 §15.4 P3 实际是 SnoopFilter,**非 Stub Cache 雏形**);A13 拍板后展开。

**若 A13 拍板复用现有 CacheTLM**(`include/tlm/cache_tlm.hh:35`):
- 本 ADR §2 Decision 1 命名表(逐字对齐 ADR-CEP-02 canonical 10 流)需反向同步
- 6 接口命名可能需调整以兼容现有 API
- 需要 adapter 层兼容

**A13 议题**:留待第 3 回合议题 A13 展开(per AGENTS.md 真源原则,不阻塞本 ADR 签发)。

---

## 10. 关联 ADR / OpenSpec

- 关联:`06-inter-module-network.md` v0.4 §15.4 P3 范围(非 Stub Cache 真源,被 supersede by CEP,per oracle B4)
- 关联:`ADR-CEP-01-fabric-collective-plane.md`(旁路平面)
- 关联:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单,本 ADR 折叠映射表逐字对齐)
- 关联:`ADR-CEP-00-platform-charter-and-governance.md`(归口,草案)
- 关联:`ADR-X.1-transaction-id.md`(txn_id 命名空间)
- 后续 OpenSpec(架构师不写,builder 阶段):
  - `cpptlm-cep-stub-cache-core-base`(P1 实施 change,1 KPI slot,doc-only + 测试)
  - `cpptlm-cep-stub-cache-core-impl`(P2 实施 change,1 KPI slot)

---

## 11. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| Stub 接口策略 | α' = (α) RN 投影 + 命名复用 CohIf canonical 10 流 | ✅ 已拍板 |
| 命名修订 | request_line→request / snoop_query→snoop / complete→completion | ✅ 已拍板 |
| 接口总数 | 6(net-side 5 + core-side 1)| ✅ 修订(per B4)|
| 接口分组 | net-side 5 + core-side 1(evict_line 标注 core-side)| ✅ 已拍板 |
| 三明治挂载 | cpu_side / net_side 双端口 | ✅ 已拍板 |
| 折叠映射表 | 5 隐藏流 → stub 原语(独立规范文档),逐字对齐 ADR-CEP-02 canonical 10 流 | ✅ 已拍板 |
| 10 条契约测试 | §3 清单(必做前 2 条 + 高优先级次轮 8,per S8 修订)| ✅ 已拍板 |
| bypass 模式 | 保留为 testbench 配置选项 | ✅ 已拍板 |
| 6 条 Stub Cache 不变量 | §4.3 INV-SC1 ~ INV-SC6(txn_id 命名 per S4 统一)| ✅ 已拍板 |
| α 失效判据 | forward_data 不可分辨性(契约测试 1) | ✅ 已拍板 |
| 与 CacheTLM 对齐 | 待 A13 议题(暂默认新接口名) | 🟡 A13 未拍板 |
| ProtocolEngine 术语 | §6 全篇统一(per N5 修订)| ✅ 已拍板 |

---

## 12. v0.2 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| §1.1 新增 | oracle B4 | 明确"06 v0.4 §15.4 P3 Stub Cache"是**伪引**,Stub Cache 接口由本 ADR 首次定义 |
| §2 Decision 1 表格 | oracle B8 + v0.4 B2-10 | 6 接口命名逐字对齐 ADR-CEP-02 canonical 10 流清单;接口总数 5 → 6(补 `snoop_resp` 显式)|
| §2 Decision 1 命名表 | oracle A7 | 沿用命名修订(`complete → completion` 等) |
| §3.1 修订 | oracle S8 | "必做前 2 条" + 决策汇总"优先 1+8" 冲突统一为"必做前 2 条 + 高优先级次轮 8" |
| §4.3 INV-SC4 | oracle S4 | `txn_id` 命名沿用 `ADR-X.1-transaction-id.md`,不独立定义 |
| §6 折叠表审计 | oracle N5 | `adapter` / `ProtocolEngine` 术语统一为 `ProtocolEngine` |
| §9 修订 | oracle S1 | A13 状态改为"待 A13 拍板"(非"待 A1 归口决议后展开");"沿用 06 §15.4 P3 5 接口" 改为"未来 A13 拍板后展开" |
| §10 关联 ADR | oracle B8 | 加 ADR-CEP-02 canonical 10 流清单引用 |

---

**版本**: v0.2 草案
**最后更新**: 2026-10-20
**批准状态**: 🟡 待 Oracle v0.5 二次复审
**维护者**: CppTLM build agent(architect mode)