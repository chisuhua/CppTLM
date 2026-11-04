# design.md — UALink INC 实现层(adapter + fabric collective plane + M-2 证书判据)

> **change**: `cpptlm-cep-ualink-inc`(队列 #14 **实现层**)· **配套**: `proposal.md` · `tasks.md` ·
> `specs/cep-ualink-inc/spec.md` · `specs/cep-collective-plane-contract/spec.md` · `specs/cep-collective-plane-base/spec.md`
> **口径**: `ADR-CEP-01` SU#4(**E.3** 四条证书 / **E.4** 重跑 DAG 恒真 / **E.6** 取代文本 / **E.7** 代价)·
> `ADR-CEP-03 §2`(7 方法面)· `ADR-CEP-04` SU 2026-11-03 · `ADR-CEP-11 §3`(版本锁)·
> 台账 Entry **#203**(规约层归档)· **#193**(三则 ADR SU + 冻结面授权)· **#194**(improvement)

---

## §1 范围与已锁决策

| # | 决策 | 依据 |
|---|---|---|
| 1 | **VC 路径 = M-2**(共享 VC + 四条证书) | `ADR-CEP-01` SU#4 E.6(用户裁决 2026-11-04) |
| 2 | ⛔ **不声明** `ualink_4` 实例 | `ADR-CEP-04` SU §③ 残余 + 锚点摘要 `N-8`/`N-9` + schema 无 pending 机制(用户裁决 2026-11-04) |
| 3 | 集合平面**不是协议实例** ⇒ **零新声明组** | `ADR-CEP-01` E.3.2 |
| 4 | 载体 = **继承**(集合平面**不声明**自己的 VC 绑定) | `ADR-CEP-01` E.3.1 C-1′ |
| 5 | ⛔ **不得**以「重跑 `evaluate_channel_dependency_gate`」当证书 | `ADR-CEP-01` E.4(构造性论证:诱导子图恒无环 ⇒ 恒真) |
| 6 | 四条证书**共用** `CP-03-VC-CERTIFICATE`,靠 `detail` **子标记**区分;C-2 复用 `CP-09-IC1` | `ADR-CEP-01` E.3.1 归因 id 纪律 |
| 7 | ⛔ **不新增** `ContractRule`(`kContractRuleCount == 6` 是硬断言)⇒ V6-d **折进 `CP-03` 行** | `ADR-CEP-01` E.3.1 + 既有契约面 |
| 8 | ⛔ **不得**复用 `[CP-03-VC-PRECONDITION]` 前缀(那是 V6-c 的精确匹配前缀) | `ADR-CEP-01` E.3.1 |

---

## §2 载体设计

### 2.1 `UalinkAdapter`(`include/tlm/cep/ualink_adapter.hh`,新)

**定位**:**零自有表副本**的静态门面 —— 全部判定**委托**规约载体 `ualink_inc_state_spec.hh`(已归档,Entry #203)。

| 面 | 内容 |
|---|---|
| **类型别名** | `Obligation` / `BasisKind` / `CertificateId` / `CertificateSpec` / `UntestedId` / `UntestedItem` —— **类型名**允许(不含 `::`) |
| **标量常量** | `kAdapterName = "UalinkAdapter"` · `kProtocolVersion = "ualink-2.0"`(per `ADR-CEP-11 §3` / 锚点 `C-5`)· 载体侧常量的**委托**副本 |
| **委托判定** | `certificate_specs()` · `m2_usable_on_network(bool)` · `untested_items()` · `untested_registry_clean()` |
| **⛔ 禁项 ①** | 本文件**去注释后**文本 SHALL **不**含规约载体的**成员字面量 / 表名**(`Obligation::` / `BasisKind::` / `CertificateId::` / `CertificateSpec` / `UntestedId::` / `UntestedItem` / `kUntestedItems` / `kCertificateSpecs` / `kSourceLicenseConfirmed` / `all_basis_valid` / `PendingSemanticItem`)⇒ **文本扫描**判据 |
| **⛔ 禁项 ②** | 本文件 SHALL **不**含任何 `transition_` 标识符 ⇒ INC 原语的转移语义是 **pending**(`N-1`)⇒ 暴露该面即等于**自造转移表** |
| **⛔ 禁项 ③** | SHALL **不**声明 `kInstanceId`(本 change **不**声明 `ualink_4` ⇒ 无实例可指) |

> ⚠️ **类型别名 vs 成员字面量**:别名形如 `using Obligation = ualink_inc::Obligation;` —— **含** `ualink_inc::`
> ⇒ 与队列 #13 的禁项扫描口径一致(`NvlinkAdapter` 的 `using IngressDecision = nvlink_sharp::IngressDecision;`
> 是**允许**的,因为禁项清单列的是**成员**字面量(带 `::` 的**成员**访问),而别名只出现**类型**路径)。

### 2.2 `FabricCollectivePlane`(`include/tlm/cep/fabric_collective_plane.hh`,新)

**定位**:平台**第一个承载真实 fabric 集合流量**的 collective plane —— 数据路径**真可达**
(⛔ **不得** `DataPathNotImplemented` 却宣称 INC 已实现,per 本 change 的 proposal「Why ②」)。

#### 2.2.1 数据路径模型(**平台侧**语义,非 UALink 侧 ⇒ 不依赖 `N-1` ~ `N-7`)

| 原语 | 模型 | 产出 |
|---|---|---|
| `REDUCE` | 每成员**贡献**一次载荷;**收齐**后按 `reduce_op` 在 `dtype` 上归约 | `CollectiveEvt::Result`(归约值)+ `Completion` |
| `BROADCAST` | **首个**贡献者的载荷为广播值;收齐后向**全体成员**广播 | `Result`(逐成员同值)+ `Completion` |
| `MULTICAST` | 参与集 = `multicast_mask` 选中的成员;收齐后回送 | `Result` + `Completion` |
| `BARRIER` | 走 `collective_barrier`(per-phase 计数,沿用 `LocalCollectivePlane` 语义) | `Completion` |

- **收齐**的定义 = 参与集**全部**成员各贡献一次 ⇒ 未收齐时 `send` 只**累积**、不产出事件。
- ⛔ 归约算子**唯一真源** = 既有 `ReduceOp`(恰 6 值)· 数据类型 = 既有 `DataType`(恰 5 值)⇒ **不新增**算子族。
- ⚠️ **本模型是平台抽象**(per `ADR-CEP-03 §2`),**不是** UALink 的 INC 语义 ⇒ `N-1`(四原语规范定义)仍 **pending**,
  转正后**只**影响 UALink 侧映射,不影响本 plane 的既有语义。

#### 2.2.2 C-3 控制面与数据面互斥

| 项 | 规定 |
|---|---|
| **in-flight 的定义** | 某 group 自**首个** `send` 起、至该 op **收齐**(产出结果)前 |
| **拒绝面** | `group_register_region`(R3 drain)与 `group_destroy`(R4 回归)—— 二者是**控制面**操作 |
| **可观测性** | 新增 `Status::ControlPlaneConflict`(`collective_plane.hh` 的第 **12** 个取值,追加在末尾 ⇒ 既有数值**不变**)⇒ `group_register_region` 的返回值 + `group_destroy` 后的 `last_status()` **均可观测** |
| **登记** | `kCollectivePlaneRegistry` 新增 `{"FabricCollectivePlane", /*is_local_model=*/false, /*enforces_control_plane_exclusion=*/true}` ⇒ ① V6-c 在 `status=unavailable` 下会因「非本地模型」判红 ② V6-d 的 C-3 声明级判定**有单源** |

#### 2.2.3 epoch 与区域

沿用 `LocalCollectivePlane` 的**两阶段**语义:`group_register_region` ⇒ `epoch_valid=false` ⇒
`complete_drain(gid, range)` ⇒ `epoch_valid=true` ⇒ 数据路径方可执行(per-region 粒度)。
⛔ **未注册区域**上的 `send` ⇒ `Status::GroupRegisterError` 且**不投递任何事件**(per 冻结 spec 的既有场景)。

### 2.3 `collective_plane_contract.hh`(修改)

| # | 改动 | 依据 |
|---|---|---|
| 1 | `VcBindingSnapshot` **扩展**:`per_network_mappings`(network → 逐实例的 `class → vc_id` 映射列表)—— C-1′ 需要**逐类相等**的判定,现有 `used` 集合**不足以**判定 | E.3.1 C-1′ |
| 2 | `evaluate_vc_reservation`:`status` **值域放宽为 2 值**(`unavailable` \| `shared_no_isolation`);`shared_no_isolation` 时校验 `certificate_scope`(非空 + 每项 network 在 `fabric.json` 登记) | E.6(取代 D.4 的 V6-a)+ E.7 负面 ⑥ |
| 3 | **新增** `evaluate_shared_mode_certificate(decl, snap, untested, detail)` —— **V6-d**:逐 `(plane, network)` 判 **C-1′ / C-2 / C-3 / C-4**;结论**折进 `CP-03` 行**的 `detail`,前缀**精确**为 `[CP-03-VC-CERTIFICATE/C-1′]` / `[CP-09-IC1]` / `[CP-03-VC-CERTIFICATE/C-3]` / `[CP-03-VC-CERTIFICATE/C-4]` | E.3.1 + E.6(V6-d 行) |
| 4 | **新增** `struct UntestedRegistry`(C-4 的输入)+ `default_untested_registry()`(由规约载体 `ualink_inc_state_spec.hh` **提供**,⛔ 不复制 U-1 / U-2 表) | E.3.4 + 零副本纪律 |

> ⚠️ **依赖方向**:契约头**消费**规约载体(include)⇒ **不复制** U-1 / U-2 ⇒ 本仓当前**唯一** M-2 使用方
> (队列 #14)⇒ 该 include 成立;若将来出现第二个 M-2 使用方,须把 `UntestedRegistry` 提升为**入参**形态。

#### 2.3.1 四条证书的**可判形式**(逐条)

| 证书 | 判定输入 | PASS 条件 | FAIL 前缀(精确) |
|---|---|---|---|
| **C-1′** | `snap.per_network_mappings[net]` + `snap.instance_ids` | ① 集合平面**未**作为实例出现在 `protocol_instances.json`(⇒ 它没有自己的 VC 绑定)② 该网上**有绑定的**既有实例**逐类相等**(类集合相同 ∧ 每类 vc 相同)③ 该网**至少有一个**有绑定的实例(⇒ 继承**有主体**,禁 vacuous PASS) | `[CP-03-VC-CERTIFICATE/C-1′]` |
| **C-2** | `snap.per_network_mappings[net]` + `snap.vc_count[net]` | 继承映射的 **vc 镜像** ⊇ `{0 .. vc_count-1}` | `[CP-09-IC1]` |
| **C-3** | `kCollectivePlaneRegistry`(单源) | 该 plane **已登记** ∧ `is_local_model == false` ∧ `enforces_control_plane_exclusion == true` | `[CP-03-VC-CERTIFICATE/C-3]` |
| **C-4** | `UntestedRegistry` | **非空** ∧ 每项 `ref` 非空 ∧ 每项 `statement` **不含**「已验证 / 已满足」 | `[CP-03-VC-CERTIFICATE/C-4]` |

> ⚠️ **C-3 的两级判定**:声明级(= plane **已登记**且声明承担 C-3)在上表;**运行级**(in-flight ⇒ 拒绝控制面)
> 由 `FabricCollectivePlane` 的**调用序列 fixture** 判定 ⇒ 两者**同归因**(`CP-03-VC-CERTIFICATE/C-3`)。

### 2.4 声明面(`collective_planes.json` + `collective_planes.schema.json`)

| # | 改动 |
|---|---|
| 1 | `vc_reservation_declaration.status`:`unavailable` ⇒ **`shared_no_isolation`**(M-2)· `reason` / `owner` / `revisit_trigger` 同步改写(⛔ 四字段**仍须齐全**) |
| 2 | **新增** `certificate_scope`(数组):`[{plane, network}]` —— 证书适用的 `(plane, network)` 清单(per E.6「声明源 SHALL 记录」)⇒ **声明最小集仍为四组**(它是 `vc_reservation_declaration` 组内的**字段**,不是新组) |
| 3 | schema:`status` 的 `enum` ⇒ 2 值;新增 `certificate_scope` 属性 + `allOf/if-then`(`status == shared_no_isolation` ⇒ `certificate_scope` **必填且 minItems ≥ 1**) |

---

## §3 判据面

### 3.1 正向判据(每条**负向断言配正控**)

| # | 判据 | 载体 |
|---|---|---|
| P-1 | `UalinkAdapter` **零自有表副本**(禁项 ① 文本扫描) | `test_cep_ualink_inc.cc` |
| P-2 | `UalinkAdapter` **无 `transition_*`**(禁项 ② 文本扫描) | 同上 |
| P-3 | `FabricCollectivePlane` 数据路径**真可达**(REDUCE / BROADCAST / MULTICAST 真执行 ⇒ 产出 `Result` + `Completion`) | 同上 |
| P-4 | **C-3**:in-flight ⇒ `group_register_region` / `group_destroy` 被拒绝且**可观测** | 同上 |
| P-5 | `status` **值域 = 2 值**;其它取值 ⇒ FAIL | `test_cep_collective_plane_contract` 侧 |
| P-6 | **V6-d** 四条证书**各自** PASS(且 detail 前缀**精确**) | 同上 |
| P-7 | **U-1 / U-2** 登记完整且**不含**「已验证 / 已满足」 | 同上 |
| P-8 | **`ualink_4` 未声明**(本 change 收窄的可执行形态) | 同上 |

### 3.2 反向 fixture(**11 组 / 11 条**,归因键 1:1)

| 组 | 归因 id | 毒化 | 预期 |
|---|---|---|---|
| **F-1** | `UIA-01` | 在 adapter 中加一行**载体成员字面量** | 文本扫描红 |
| **F-2** | `UIA-02` | 在 adapter 中加 `transition_read_reduce()` | 文本扫描红 |
| **F-3** | `UIA-03` | 把 plane 的 `send` 执行分支改成返回 `DataPathNotImplemented` | 运行期红 |
| **F-4** | `UIA-04` | 去掉 plane 的 in-flight 检查 | 运行期红(in-flight 时 register 成功) |
| **F-5** | `UIA-05` | 声明源 `status` 改成 `"shared"` | 判据红 |
| **F-6** | `UIA-06` | 清空 `certificate_scope` | 判据红 |
| **F-7** | `UIA-07` | 在 `protocol_instances.json` 加一条 `ualink_4` | 判据红 |
| **F-8** | `CP-03-VC-CERTIFICATE/C-1′` | 影子 `protocol_instances.json`:令 `mesif` 的 `ualink_fabric` 映射与 `moesi_gpu` **不一致** | 判据红(**精确**前缀) |
| **F-9** | `CP-09-IC1` | 影子 `fabric.json`:`ualink_fabric.vc_count` 4 → 5 | 判据红(**精确**前缀) |
| **F-10** | `CP-03-VC-CERTIFICATE/C-3` | 影子 `collective_plane.hh`:把 `FabricCollectivePlane` 的 `enforces_control_plane_exclusion` 改 `false` | 判据红(**精确**前缀) |
| **F-11** | `CP-03-VC-CERTIFICATE/C-4` | 影子 `ualink_inc_state_spec.hh`:把 U-1 的 `statement` 改成含「已验证」 | 判据红(**精确**前缀) |

> ⚠️ **红形态双分支**(照队列 #13 / #14 规约层先例):影子副本**编译红** ⇒ 归因取编译日志;
> 影子副本**编译绿** ⇒ 跑判据 ⇒ 归因取 verdict detail 的**精确前缀**;两者都绿 ⇒ 判「**无鉴别力**」⇒ 该组失效。
> ⚠️ **F-8 / F-9 / F-11 的红形态 = 运行期**(数据 / 影子头 ⇒ 编译绿 ⇒ 走判据分支)。

---

## §4 归因 id 登记(**唯一**登记处 = 本 design §4)

| id | 归属 | 义务 | 载体 |
|---|---|---|---|
| `UIA-01` | **本层新建** | `UalinkAdapter` 零自有表副本 | `ualink_adapter.hh` 禁项 ① |
| `UIA-02` | **本层新建** | `UalinkAdapter` 无 `transition_*` | `ualink_adapter.hh` 禁项 ② |
| `UIA-03` | **本层新建** | `FabricCollectivePlane` 数据路径真可达 | `fabric_collective_plane.hh` |
| `UIA-04` | **本层新建** | C-3 控制面与数据面互斥(运行级) | `fabric_collective_plane.hh` |
| `UIA-05` | **本层新建** | `status` 值域 = 2 值 | `collective_plane_contract.hh` |
| `UIA-06` | **本层新建** | `certificate_scope` 登记(非空 + network 合法) | `collective_plane_contract.hh` |
| `UIA-07` | **本层新建** | `ualink_4` **不**声明(收窄的可执行形态) | `test_cep_ualink_inc.cc` |
| `CP-03-VC-CERTIFICATE/C-1′` | **ADR-CEP-01 E.3.1 消费** | C-1′ 槽位语义继承 | `evaluate_shared_mode_certificate` |
| `CP-09-IC1` | **ADR-CEP-01 E.3.1 消费** | C-2 VC 覆盖完整 | 同上 |
| `CP-03-VC-CERTIFICATE/C-3` | **ADR-CEP-01 E.3.1 消费** | C-3 控制面互斥(声明级) | 同上 |
| `CP-03-VC-CERTIFICATE/C-4` | **ADR-CEP-01 E.3.1 消费** | C-4 未检验项登记 | 同上 |

⇒ **11 组 / 11 条** fixture ↔ **11** 个归因键 **1:1**(组 = 归因键 · 条 = 毒化变体)。
⇒ 本层**新建** id = **7**(`UIA-01` ~ `UIA-07`)· 另 **4** 个为 ADR 既有 id 的**消费**(⛔ 不新造)。

---

## §5 冻结面(**3 份**,⚠️ improvement 原估 1 份)

| # | 冻结件 | 变更原因 | 授权形态 |
|---|---|---|---|
| 1 | `openspec/specs/cep-collective-plane-contract/spec.md` | **V6-d** 新增 + `status` 2 值 + `V6-c` 适用条件改写 | 归档时 delta 合并 ⇒ 走**两步授权** |
| 2 | `openspec/specs/cep-collective-plane-base/spec.md` | 「已注册且 epoch 生效的区域上,数据原语 SHALL 显式声明未实现」**收窄到 `LocalCollectivePlane`** ⇒ 否则与 `FabricCollectivePlane` 的真可达数据路径**直接矛盾** | 同上 |
| 3 | `docs/architecture/07-coherence-exploration-platform.md` | §5.1.1 UALink 行状态 + §11 的 Collective Plane 行(+ adapter 计数行**数值不变**) | 手工编辑 ⇒ 走**两步授权** |

⇒ 两步授权流程:① 台账 Entry(**逐件列名**)→ ② commit → ③ `cep_freeze_check.sh --update --authorized-by "<Entry #>"` → commit
→ ④ `cep_freeze_check.sh --update` → commit。

---

## §6 风险与代价登记

| # | 风险 / 代价 | 处置 |
|---|---|---|
| 1 | **U-1**(「VC 隔离确实避免协议级死锁」)在 M-2 下**永远无法检验** | 登记为未检验(C-4)⇒ 该假设**不得**再被当作已满足的前提 |
| 2 | **U-2**(同 VC 内 coherence ↔ collective 的相互阻塞)**不被 VC 级依赖图表达** | 只能靠 C-3(控制面互斥)+ 显式顺序声明 ⇒ 已登记 |
| 3 | **`ualink_4` 未声明** ⇒ `plane_binding` 无 UALink 实例 ⇒ **实例级装配不可达** | 登记为 backlog(触发条件 = Evaluation Copy 授权 + 新 Task 0 轮次) |
| 4 | `certificate_scope` 登记的是**声明面**的适用范围,当前**无实例**消费 | `reason` 字段**显式写明** ⇒ 不隐藏 |
| 5 | `Status` 枚举 **+1 值** | 追加在末尾 ⇒ 既有数值**不变**;`status_name()` 同步 |
| 6 | `kCollectivePlaneRegistry` 条目 **+1 行** 且结构 **+1 字段** | 非冻结面;`find_collective_plane()` 逻辑不变 |
| 7 | 契约头 **include** 规约载体(UALink 专用) | 本仓当前**唯一** M-2 使用方 ⇒ 成立;将来第二个使用方 ⇒ 提升为入参 |
