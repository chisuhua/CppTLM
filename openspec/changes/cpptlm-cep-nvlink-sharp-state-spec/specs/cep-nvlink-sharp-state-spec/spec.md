# cep-nvlink-sharp-state-spec Specification

## Purpose
定义 **NVLink SHARP** 在 CEP 平台上的**规约层可执行契约**:
**R1/R2 分类判据**(opcode 永不作分类器 · 唯一分类点 = fabric ingress)·
**R3/R4 区域互斥与 epoch 纪律**(注册三步原子 · epoch 未 commit 前**放行** · 注销失效化)·
**R5 可观测性**(divert 计数器 + fail-stop 断言)· 跨平面 **ordering A/B/C/D** ·
**集合 VC 预留状态**(Decision D 的 `unavailable` 四字段声明)· 逐格 `basis` 两轴标注。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #174 / #173 / #172 / #171** + **#169 / #170**
+ `ADR-CEP-01` 的 SU 2026-11-03(**Decision D**)· `ADR-CEP-02` / `ADR-CEP-03` / `ADR-CEP-04` / `ADR-CEP-12`)
⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改任何 ADR 正文 · 不改 `protocol_instance.schema.json` · 不改 `collective_planes.json` /
> `collective_planes.schema.json`(Decision D 已落)· 不改 `collective_plane_contract.hh` ·
> 不改 canonical 10 流清单 · **不建模**报文格式 / opcode 布局 / 数值精度 / 聚合树构建算法 ·
> **0 新增** ABI · **0 新增** invariant 编号数 · **不产出任何性能结论**
>
> **合规边界**:NVLink SHARP 正式文档受 NDA 约束(照 CXL 的 ECA §3 + AI AUP 同款处理)
> ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本能力的全部 basis 指向**公开非规范来源**的锚点摘要条目号
> ⇒ **9 项**细粒度语义登记 `pending`(7 项来自 Task 0 普查 + 2 项来自裁决 ③ / ④),走「**人类成员条款摘要**」流程。

---

## ADDED Requirements

### Requirement: cep-nvlink-sharp-state-spec

CEP SHALL 为 **NVLink SHARP**提供一份**可执行规约层契约**:
分类判据 · 区域/epoch 纪律 · 可观测性 · 跨平面 ordering · VC 预留状态 · 逐格 `basis` 两轴标注。

#### Scenario: NSS-01 —— R1/R2 分类判据 SHALL 钉住「opcode 永不作分类器」+「唯一分类点 = fabric ingress」

- **WHEN** 读取载体的分类判据
- **THEN** 分类 SHALL **只**发生在 fabric ingress(输入 = 目的地址 + 区域表 + epoch 状态)
- **AND** 源端 SHALL **SHALL NOT** 持有区域表的可见性,SHALL **SHALL NOT** 因「该地址是 SHARP 区」而改变 opcode
- **AND** **反向 fixture**:让源端按「是否 SHARP 区」改 opcode ⇒ 判据 SHALL 必红(归因 `CP-OPCODE-CLASSIFIER`(R1 分类器)与 `CP-05`(源端可见性))
- **AND** ⚠️ 编译期锁 SHALL 另配一条**运行期**判据(静态扫描源端侧不出现区域表字段)⇒ 不得只以「编译红」充当 ctest 判据

#### Scenario: NSS-02 —— R4 epoch 语义 SHALL **双向**成立

- **WHEN** 对**同一**地址分别求值「epoch 未 commit」与「epoch 已 commit」两种状态
- **THEN** 未 commit ⇒ SHALL **放行**(正常服务 coherence 路径);已 commit ⇒ SHALL **divert**(转 collective 平面)
- **AND** ⚠️ 「未 commit ⇒ **拒绝**」SHALL **SHALL NOT** 成立(与 `ADR-CEP-03 §3.1` R4 矛盾)⇒ **反向 fixture**:改成「未 commit 拒绝」⇒ SHALL 必红(归因 `CP-EPOCH-SEMANTICS`)
- **AND** 本条 SHALL 同时作为**正控**存在(两向各一例)⇒ 纯负向断言无对照 = 判据可能没生效

#### Scenario: NSS-03 —— R3 注册 SHALL 为**三步原子**

- **WHEN** 对一个地址区间执行 `register_region`
- **THEN** SHALL 依次完成 ① 登记 ② drain ③ 建立 epoch
- **AND** **drain 失败** ⇒ SHALL 返回失败且 **SHALL NOT** 建立 epoch、SHALL **SHALL NOT** 把该区间留在区域表内
- **AND** **反向 fixture**:drain 失败仍建 epoch ⇒ SHALL 必红(归因 `CP-DRAIN-FAIL`)

#### Scenario: NSS-04 —— R4 注销 SHALL 失效化并**回归 coherence 域**

- **WHEN** 对已注册区间执行 `destroy`
- **THEN** 该区间 SHALL **退出**区域表,且其中 stale copy SHALL 失效化
- **AND** 此后对同一地址的 coherence 请求 SHALL **正常服务**(不再 divert)
- **AND** **反向 fixture**:注销后仍 divert ⇒ SHALL 必红(归因 `NSS-04-DESTROY-LEAK`)

#### Scenario: NSS-05 —— R5 可观测性 SHALL 落地

- **WHEN** 读取载体的可观测性面
- **THEN** SHALL 有 divert **计数器**(按 group / 区域可寻址)
- **AND** SHALL 有 **fail-stop 断言**:**epoch commit 后仍有 coherence atomic 命中已注册区域** ⇒ SHALL 报错(非静默)
- **AND** **反向 fixture**:该情形**不**报错 ⇒ SHALL 必红(归因 `CP-DIVERT-STOP`)

#### Scenario: NSS-06 —— 跨平面 ordering A/B/C/D SHALL **可判定**

- **WHEN** 读取载体的 ordering 规则集
- **THEN** SHALL 恰含 **4** 类:A(同址 coherent atomic → reduce)· B(同址 reduce → coherent atomic)· C(同 group 内 reduce ↔ reduce)· D(SHARP barrier → reduce)
- **AND** 每类 SHALL 有**可判定**的谓词,而非散文描述
- **AND** **反向 fixture**:把某类判反 ⇒ SHALL 必红(归因 `NSS-06-ORDERING-MIS`)

#### Scenario: NSS-07 —— 集合 VC SHALL 登记为**「状态」**而非「表」

- **WHEN** 读取载体的 VC 预留面
- **THEN** 载体 SHALL **SHALL NOT** 表达任何**具体 `vc_id`**、SHALL **SHALL NOT** 表达「隔离 VC 可用」(⇒ 由**不可表达性**保证,而非由断言保证)
- **AND** ⚠️ 载体 SHALL **NOT** 复制 Decision D 的**四字段内容**(那是**拓扑级**事实,权威在 `collective_planes.json` + `CP-03`;复制会把拓扑变化变成两处同改)
- **AND** 该形态 SHALL 与 `ADR-CEP-01` SU 2026-11-03 的 **Decision D** 一致(判定**负向**而非复制)
- **AND** **反向 fixture**:填入具体 VC 编号 ⇒ SHALL 必红(归因 `CP-03`)
- **AND** ⚠️ 「不可表达」型判据 SHALL 另配一条**运行期**判据(载体解析出的 scope / status 恒为预期值)⇒ 不得只以「编译红」充当 ctest 判据

#### Scenario: NSS-08 —— 能力域 SHALL 钉为**节点内**

- **WHEN** 读取载体声明的 collective 能力域
- **THEN** SHALL 为 **intra-node**(节点内)—— 依据:NVLink SHARP(`NVLS`)用于**节点内**归约,跨节点归约属 CollNet 通路
- **AND** 载体 SHALL **SHALL NOT** 声明跨 fabric 的通用归约能力
- **AND** ⚠️ 本条约束**能力域**(拓扑域),**不**约束**流量落点** ⇒ 落点约束见 `ADR-CEP-01` SU 2026-11-03(#2) G.1(「不得占用承载 coherence 的 VC 与传输通道」)—— 二者是**两条不同**的约束,**不**共用归因 id
- **AND** **反向 fixture**:把能力域改为跨 fabric ⇒ SHALL 必红(归因 `NSS-08-SCOPE-OVERREACH`)

#### Scenario: NSS-09 —— 逐格 `basis` SHALL 带两轴 + **9 项 `pending`** SHALL 逐条登记

- **WHEN** 读取载体的每个格的 `basis`
- **THEN** 每格 SHALL 同时带 **证据强度轴**(`kind` ∈ `public-direct` / `public-analogical` / `self-derived`)与**钉版强度轴**(`pending` ∈ `true` / `false`)
- **AND** 每格 SHALL 带 `ref` ⇒ 其值 SHALL 指向**锚点摘要的条目号**(`A-n` / `N-n`),SHALL **SHALL NOT** 指向规范原文
- **AND** **9 项** `pending` SHALL **恰好**覆盖:reduce 算子穷举集 · 逐操作支持矩阵 · epoch 状态机细节 · VC 编号 · 聚合树构建算法 · 报文格式/opcode 布局 · 数值/精度语义 · **NVLink 实例的 `perm` 具体子集** · **NVLink atomic 的协议侧 completion 配对**
- **AND** 这 9 项 SHALL **SHALL NOT** 被推理填空 ⇒ 载体中**不得**出现其**具体取值**的断言
- **AND** **反向 fixture**:去掉某格的 `pending` 标记 ⇒ SHALL 必红(归因 `NSS-09-BASIS-MISSING`)

> **锚点**:`test/test_cep_nvlink_sharp_state_spec.cc` 的编译期断言 + 运行期兜底判据 + 逐格 basis 核对 + **9 组反向 fixture**(每组带**可归因 id**)
