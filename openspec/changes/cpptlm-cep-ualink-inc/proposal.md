# cpptlm-cep-ualink-inc — UALink INC 实现层(adapter + fabric collective plane + 共享模式证书判据)

> **状态**: 🟡 **Proposed**(2026-11-04)· **队列**: `obj-cep-platform §4.0` **#14** 第 1 个 change(**实现层**)
> **配套**: [`design.md`](design.md) · [`tasks.md`](tasks.md) · `specs/cep-ualink-inc/spec.md` · `specs/cep-collective-plane-contract/spec.md` · `specs/cep-collective-plane-base/spec.md`
> **口径**: 台账 **Entry #203**(规约层归档)· **#194**(improvement 草稿)· **#193**(三则 ADR SU + 冻结面授权)
> **前置**: 规约层 `cpptlm-cep-ualink-inc-state-spec` **已完成归档**(Entry #203)⇒ `openspec/specs/cep-ualink-inc-state-spec/spec.md` 已入库
> **审计面**: `docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md`(basis 唯一人工审计面)
> **估时**: **3 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 change = `cpptlm-cep-ualink-inc-state-spec`(**规约层**,已归档)+ 第 1 个 change = **本 change**(**实现层**)

---

## Why

### ① M-2(共享 VC + 证书)至今**没有任何可执行判据面**

`ADR-CEP-01` SU#4(Entry #193)裁决 **VC 路径 = D**(M-2:集合流量**复用** coherence 已占用的 VC + **四条可机器判定的证书** C-1′ / C-2 / C-3 / C-4)。
规约层(Entry #203)已把四条证书的**规约面**(逐边依据 + 参照域 + `IC1` 鉴别力边界 + U-1 / U-2 登记)固化 ⇒
但**判据面**(`V6-d`)尚未存在 ⇒ `evaluate_vc_reservation` 对 `status != "unavailable"` **一律 FAIL**
(per `ADR-CEP-01` E.7 负面 ⑥)⇒ **M-2 今日仍不可用**。

### ② 平台至今**没有承载真实 fabric 集合流量的 plane**

`CollectivePlaneIf` 的两个现役实现:`NullCollectivePlane`(全原语「不支持」)与 `LocalCollectivePlane`
(数据路径 **显式声明未实现** ⇒ `Status::DataPathNotImplemented`)⇒ 二者**均**是**本地模型**
(`kCollectivePlaneRegistry` 的 `is_local_model = true`)⇒ 平台**答不了**「集合流量真的走 fabric 时会怎样」。

### ③ `ADR-CEP-01` E.3.2 的三条障碍**已清零**,载体形态现在可落地

集合平面**不是协议实例**、**不声明**自己的 VC 绑定、`protocol_instances.json` **零新声明组**
⇒ 实现层可以落地一个**独立的** `FabricCollectivePlane` + 一个**零副本**的 `UalinkAdapter`,
且**零 M1 冻结件的结构性改动**。

### ④ ⚠️ 一处**范围收窄**(用户裁决 2026-11-04)—— `ualink_4` 实例声明**不在本 change**

| 依据 | 内容 |
|---|---|
| `ADR-CEP-04` SU 2026-11-03 §③ 残余 | 「`ualink_4` 的实际取值**尚未裁决** …… ⛔ **不得**由 architect 或 builder **预先钉死**」 |
| 锚点摘要 §3 `N-8` / `N-9` | `flow_support.snoop` / `snoop_resp` 与 `perm` 子集 = **pending**(公开来源零涉及;规范正文受 Evaluation Copy Agreement 约束)⇒「**不得**据此推断」 |
| `protocol_instance.schema.json` | `flow_support` **10 个 boolean 全必填** + `perm` **minItems 1**、值域 4 值,**无 pending / null 机制** ⇒ 声明 `ualink_4` **必然**给出具体值 = **推理填空** |

⇒ **裁决**:本 change **不声明** `ualink_4`;实例声明 + 计数链(实例 7 → 8 · `no_upgrade_instances` 3 → 4 ·
`implemented` 7 → 8)与 07 号 §11 计数**全部移出**,登记为 backlog 项(触发条件 = 用户**自行**取得 Evaluation Copy
并**明确**授权 + 走**新**的 Task 0 轮次 ⇒ 新 change)。
⇒ **本 change 的其余交付面全部不依赖 `ualink_4`**。

---

## What Changes

| 面 | 交付 |
|---|---|
| **载体 ①** | `include/tlm/cep/ualink_adapter.hh`(新 · header-only)—— `UalinkAdapter`:**零自有表副本**(⛔ 不含任何载体成员字面量)+ **无 `transition_*`**(INC 原语转移语义 pending ⇒ 暴露即等于自造转移表)+ 全部判定**委托**规约载体 `ualink_inc_state_spec.hh` |
| **载体 ②** | `include/tlm/cep/fabric_collective_plane.hh`(新 · header-only)—— `FabricCollectivePlane`:**REDUCE / BROADCAST / MULTICAST 数据路径真可达**(⛔ **不得** `DataPathNotImplemented` 却宣称 INC 已实现)+ **C-3 控制面与数据面互斥**(group 有 op in-flight ⇒ 拒绝 `group_register_region` / `group_destroy`) |
| **载体 ③ 修改** | `include/tlm/cep/collective_plane.hh` —— +1 个 `Status` 取值(`ControlPlaneConflict`,C-3 的可观测拒绝通道)+ `kCollectivePlaneRegistry` 登记 `FabricCollectivePlane`(`is_local_model = false` + `enforces_control_plane_exclusion = true`) |
| **判据面** | `include/tlm/cep/collective_plane_contract.hh` —— `evaluate_vc_reservation` 的 `status` **值域放宽为 2 值**(`unavailable` \| `shared_no_isolation`)+ `certificate_scope` 校验 + **新增 `evaluate_shared_mode_certificate`**(`V6-d`:C-1′ / C-2 / C-3 / C-4,**折进 `CP-03` 行**,⛔ **不新增** `ContractRule`) |
| **声明面** | `include/tlm/cep/collective_planes.json` + `collective_planes.schema.json` —— `status` 第二取值 + `certificate_scope`(证书适用的 `(plane, network)` 清单,per `ADR-CEP-01` E.6) |
| **判据(测试)** | 正向判据(每条**负向断言配正控**)+ **N 组反向 fixture**(毒化 ⇒ **必红**且归因 id **精确相等**命中)+ 基线正控 + **归因 id 元测试**(双向覆盖)+ `ctest` 注册 |
| **文档** | 台账登记 · `scripts/README.md` 守门表 + 反向 fixture 登记 · 07 号 §5.1.1 UALink 行 + §11 计数行(**冻结面**,两步授权) |
| **零改动** | `ADR-CEP-01` / `02` / `03` / `04` / `09` / `11` / `12` **正文** · `protocol_instance*.json` · `fabric.json` · `protocol_instance.schema.json` · `fabric.schema.json` · canonical **10 流**清单 · `ReduceOp` 枚举 · `CollectivePlaneIf` 的 **7 方法**面 · **0 新增** ABI · **0 新增** `INV-` 编号 |

---

## Capabilities

| 能力 | 内容 | 载体 |
|---|---|---|
| **`cep-ualink-inc`**(新) | UALink INC 的**实现层**契约:`UalinkAdapter`(零副本 · 委托规约载体)· `FabricCollectivePlane`(数据路径真可达 + C-3 互斥)· **M-2 共享模式证书**的判据面 | 本 change |
| **`cep-collective-plane-contract`**(delta) | **V6-d** 共享模式证书 + `status` 第二取值(`shared_no_isolation`)+ `V6-c` 的**适用条件**改写 | 本 change |
| **`cep-collective-plane-base`**(delta) | 「已注册且 epoch 生效的区域上,数据原语 SHALL 显式声明未实现」**收窄到 `LocalCollectivePlane`**(⛔ 否则与 `FabricCollectivePlane` 的真可达数据路径**矛盾**)+ 新增 `FabricCollectivePlane` 的行为场景 | 本 change |

---

## Impact

| 面 | 影响 |
|---|---|
| **本仓实现** | 新增 **2 个头** + 1 份测试 + 1 份驱动脚本;修改 `collective_plane.hh` / `collective_plane_contract.hh` / `collective_planes.json` / `collective_planes.schema.json` |
| **冻结面** | **3 份**(⚠️ improvement 原估 **1 份**,实际 **3 份**):`openspec/specs/cep-collective-plane-contract/spec.md` · `openspec/specs/cep-collective-plane-base/spec.md` · `docs/architecture/07-coherence-exploration-platform.md` ⇒ 归档时走**两步授权**(`--update --authorized-by`) |
| **跨仓** | **零** —— `fabric.json` / `fabric.schema.json` / `protocol_instance.schema.json` 全部**零 diff** ⇒ `cep_decl_alignment_check.sh` ⑥ 的 ArchForge 镜像闸**不触发** |
| **计数链** | **本 change 不动**(实例 7 → 8 等**全部移出**,见「Why ④」)⇒ 07 号 §11 的 adapter 计数行**数值不变**(仅状态描述更新) |
| **平台能力** | 首个**数据路径真可达**的 collective plane(REDUCE / BROADCAST / MULTICAST 真执行)+ M-2 证书**首次可执行** |
| **残余风险 ①** | **U-1**(「VC 隔离确实避免协议级死锁」)在 M-2 下**永远无法检验** ⇒ 登记为未检验 ⇒ 该假设**不得**再被当作已满足的前提 |
| **残余风险 ②** | **U-2**(同 VC 内 coherence ↔ collective 的相互阻塞)**不被 VC 级依赖图表达** ⇒ 只能靠 C-3(控制面互斥)+ 显式的顺序声明 ⇒ 已登记 |
| **残余风险 ③** | `ualink_4` 未声明 ⇒ `plane_binding` 无 UALink 实例 ⇒ **实例级装配不可达**(仅 plane 级 + 声明级可判)⇒ 登记为 backlog |
| **可逆性** | 中:删 2 头 + 撤 `status` 第二取值 + 撤 `V6-d` + 撤 registry 登记 + 恢复 `Status` 枚举 ⇒ 回到 D.4 原文本(M-1 语义未变) |

---

## Out of Scope

- **`ualink_4` 实例声明**与随之而来的**计数链**(实例 7 → 8 · `no_upgrade_instances` 3 → 4 · `implemented` 7 → 8)
  —— 取值待 `N-8` / `N-9` 转正(见「Why ④」)
- 硬件 **opcode / 指令编码** · 真实 **带宽 / 延迟**结论 · UALink 的**一致性状态机**(它没有)
- **扩 `vc_count`**(路径 A)—— 4 VC 是硬件事实 ⇒ 本 change 零改动
- 真实 UALink **线格式 / 物理拓扑** · 新 canonical 流 · **新** reduce 算子族(唯一真源 = 既有 `ReduceOp` 恰 6 值)
- `N-1` ~ `N-7` 的**转正**(需规范正文条款级定位 ⇒ 需 Evaluation Copy 授权)
- **不**改 `ADR-CEP-01` / `02` / `03` / `04` / `09` / `11` / `12` 的**正文**(只消费已签的 SU)

---

## Acceptance

- [ ] **`UalinkAdapter` 零自有表副本**:⛔ 不含规约载体的任何**成员字面量 / 表名**;全部判定**委托** `ualink_inc_state_spec.hh`
- [ ] **`UalinkAdapter` 无 `transition_*`**:⛔ 不暴露任何 INC 原语转移面(INC 转移语义 pending ⇒ 暴露即等于自造转移表)
- [ ] **`FabricCollectivePlane` 数据路径真可达**:`REDUCE` / `BROADCAST` / `MULTICAST` 在**已注册且 epoch 生效**的区域上**真执行** ⇒ ⛔ **不得**返回 `DataPathNotImplemented`
- [ ] **C-3 控制面与数据面互斥**:group 有 op **in-flight** 时,`group_register_region` 与 `group_destroy` SHALL **被拒绝**且拒绝**可观测**(独立状态取值)
- [ ] **`status` 值域 = 2 值**:`unavailable` \| `shared_no_isolation`;⛔ 其它取值判 **FAIL**
- [ ] **V6-d 共享模式证书**:C-1′ / C-2 / C-3 / C-4 四条**各自**可执行且归因 **精确相等**命中
      (`CP-03-VC-CERTIFICATE/C-1′` · `CP-09-IC1` · `CP-03-VC-CERTIFICATE/C-3` · `CP-03-VC-CERTIFICATE/C-4`)
- [ ] ⛔ **不**把「重跑 `IC1`」当证书(实测**恒真**)⇒ 判据**必须**落在 C-1′ / C-2 / C-3 / C-4
- [ ] **未检验项 U-1 / U-2** 显式登记且**不**含「已验证 / 已满足」字样
- [ ] **反向 fixture**:每条必红 + 归因 id **精确相等**命中 + **基线正控**(干净载体必绿)+ **归因 id 元测试**(双向覆盖)
- [ ] **冻结面闭环**:① 台账 Entry(逐件列名)→ ② commit → ③ `--update --authorized-by` → commit → ④ `--update` → commit(**3 份**)
- [ ] **收口**:`ctest` 全绿 + 门禁 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0 + KPI 回填
