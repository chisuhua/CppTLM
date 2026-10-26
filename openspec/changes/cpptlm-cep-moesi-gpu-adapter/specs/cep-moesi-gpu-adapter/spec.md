## ADDED Requirements

### Requirement: cep-moesi-gpu-adapter

CEP SHALL 交付 **`moesi_gpu` 协议实例**作为平台上的**首个 `implemented` adapter**:表驱动的 6 态 × 6 事件状态机 + 6→4 `perm` 投影 + 6→3 消息类映射 + 逐字镜像的实例声明 + 装配期校验实测。

> **形态 / 状态机表**:**header-only 不进 `ModuleFactory`**、不注册 23 ABI、不改 frozen header;6×6 `state_transition` 语义取自 ArchForge `09 号` §3 但**本仓从零实现** ⇒ 全表跨仓逐格镜像**不在**本 change 范围(design **D-ε**)。
> **边界**:**新增 invariant 数 = 0**(贡献 = 行为化既有条目 + 契约层加固);本 Stage 不可测条款 SHALL **不**进本 capability(依据见 design **D-ι** / **D-λ**)。

#### Scenario: 6 态 × 6 事件表驱动,且 `E+Write→M` 静默升级存在

- **WHEN** 读取 `MoesiGpuAdapter` 的状态转换表
- **THEN** SHALL 恰为 **6 态 × 6 事件**的表驱动形态(状态 `I/S/E/M/O/T`;事件 `Rd/Wr/Inv/InvAck/Fl/FlAck`)
- **AND** `E` + `Write` 的目标态 SHALL 为 `M`(即 `state_transition[2][1] == 3`,per ArchForge `32 号` §6.1 + `09 号` §3)
- **AND** 该转换 SHALL 被识别为**静默升级**(终态 `s1` 允许 store 命中且**无需外部通信**,per `ADR-CEP-13` 决策 3 的判据 (a))
- **AND** 状态 `M` 的 snoop 响应 SHALL 携带**数据值**(判据 (b) 的 `s2` 支),而状态 `E` 的 snoop 响应 SHALL **不**携带数据值(`s1` 支不成立)

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的转换表尺寸 + `E+Write→M` + snoop 响应语义断言

#### Scenario: 6→4 `perm` 投影完备,且瞬态 `T` **不泄漏**

- **WHEN** 读取 `MoesiGpuAdapter` 的 `perm` 投影表
- **THEN** SHALL 为 `I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK` / `O→WRITEBACK`(per ArchForge `32 号` §7.2 投影表)
- **AND** **瞬态态 `T` SHALL 无投影** —— 跨域接口 SHALL NOT 出现「等待 snoop 响应」的中间态(per `ADR-CEP-05` 的 `INV-CEP-05-IC1`:Stub 只暴露稳定状态)
- **AND** `O` 与 `M` 合并为 `WRITEBACK` 的语义损失 SHALL 被**显式登记**(「是否还有其他 sharer」信息由 Sharer Tracker 补偿,**不**依赖 `perm` 表达)
- **AND** **每个稳定态 SHALL 都有投影**(缺一即判红)

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的投影表断言 + `T` 无投影断言 + `O/M` 合并登记断言

#### Scenario: 6→3 消息类映射表**完备且非恒等**

- **WHEN** 读取 `MoesiGpuAdapter` 的消息类 → `CATEGORY_*` 映射表
- **THEN** SHALL 为该实例**实际使用**的每个 6 类消息类(`request` / `response` / `snoop` / `snoop_resp` / `atomic` / `query`)提供**恰好一行**映射
- **AND** 每个映射目标 SHALL ∈ 链路层 `CATEGORY_*` 的 **3 类**(per `ADR-CEP-12 §8.3` ② 分层纪律 —— `CATEGORY_*` **不**为承载 6 类词表而扩类)
- **AND** 映射 SHALL 为**显式**表(**非恒等**):至少一行的目标 ≠ 同名 `CATEGORY_*`
- **AND** 若某实例的映射**确实恒等**,则 SHALL **显式声明恒等并携带理由**(`identity_reason`)—— 不得静默放过

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的映射表完备性 + 目标值域 + 非恒等 + 恒等理由断言

#### Scenario: 实例声明逐字镜像 ArchForge,且 12 个非 `status` 键**零 diff**

- **WHEN** 读取 `include/tlm/cep/protocol_instances.json` 的 `moesi_gpu` 条目
- **THEN** SHALL 与 ArchForge `32 号` §6.1 的声明**逐字段相等**(除 `status` 外 —— `status` 是**本仓权威字段**,per design **D-ζ**)
- **AND** `protocol_version` SHALL 为 `moesi-gpu-1.0`(落在 `ADR-CEP-11 §3` 锁表内 ⇒ `INV-CEP-12-IC1` 行为化)
- **AND** `flow_support` SHALL 覆盖 canonical **10 流**全项(per `ADR-CEP-02 §2`)
- **AND** `perm` SHALL 为 **4 值**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`)—— **不含** `ATOMIC`(per `CEN-Ib11`)

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的声明字段断言 + `scripts/test/cep_decl_alignment_check.sh` ⑥ 项(排除 `status` 后仍逐字段严格)

#### Scenario: per-network 嵌套形态的装配期校验通过(IC2 三条件)

- **WHEN** 对 `moesi_gpu` 声明跑装配期校验(per `ADR-CEP-12 §8.3` `INV-CEP-12-IC2` 条件 ① ② ③)
- **THEN** 条件 ① SHALL 成立:`ordering_assumption[n]` 的每个 `vc_id` ∈ `vc_binding[n].values()`
- **AND** 条件 ② SHALL 成立:`vc_binding[n].values()` 中每个 `vc_id` < 该网络的 `vc_count`(2 个网络均 `vc_count = 4`)
- **AND** 条件 ③ SHALL 成立:`vc_binding[n]` 的每个消息类 ∈ 6 类规范词表
- **AND** 校验 SHALL 覆盖**两个网络**(`compute_noc` / `ualink_fabric`)—— 嵌套形态是 **per-network**,不是扁平
- **AND** 违反任一条件 SHALL 判 **FAIL**(不静默降级,per `ADR-CEP-11:134/106`)

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的 IC2 三条件断言 + `test_cep_moesi_gpu_adapter.cc` 的**反向 fixture**(越界 `vc_id` ⇒ 必红)

#### Scenario: `status` 翻转 ⇒ 计入 adapter 实例数(S1-2 **条件达成**)

- **WHEN** `moesi_gpu` 的 `status` 由 `planned` 翻转为 `implemented`
- **THEN** 计入 adapter 实例数的 SHALL 由 **0 → 1**(per `INV-CEP-12-IC3`:**仅 implemented 计入**)
- **AND** 翻转 SHALL **不**改动其余 12 键(⇒ ⑥ 镜像面零 diff)
- **AND** 若实施窗口内 ArchForge **未**同步翻转其侧 `status`,则本 Scenario SHALL 记为「**条件达成**」而非「已达成」,且**不得**宣称 S1-2 已闭合(per design **D-ζ** 的诚实登记)

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的计数断言 + 「条件达成」登记项的断言

#### Scenario: `mesif` SHALL **不在**本 change 的交付面内

- **WHEN** 检查本 change 的全部载体(`moesi_gpu_adapter.hh` / `adapter_schema_contracts.hh` / `perm_upgrades.json` / `protocol_instances.json`)
- **THEN** SHALL **不**含任何 `mesif` 的 adapter 实现、状态机、投影或消息类映射
- **AND** `mesif` 在 `protocol_instances.json` 的条目 SHALL 保持 **12 键逐字镜像** + `status = planned`(**零改动**)
- **AND** `mesif` 的排队与阻塞 SHALL 登记在 `obj §4.0` **队列 #8** 行(含 **R-1 `HierarchicalDirectory` 阻塞**)

> **锚点**:`test_cep_moesi_gpu_adapter.cc` 的「本载体不含 mesif」断言 + `scripts/test/cep_decl_alignment_check.sh` ⑥ 项的 `mesif` 镜像断言
