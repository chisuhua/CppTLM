## ADDED Requirements

### Requirement: cep-adapter-schema-contracts

CEP SHALL 将 **adapter 侧的跨实例复用契约**固化成 **5 条可机器检查的判据**:`perm` 4 值锁不破 / `perm_upgrade` 引用完整性 + 权限单调 / 三态 + 反向 fixture / 6→3 映射表完备且非恒等 / adapter 投影自洽。

> **实现 / 声明源 / 边界**:`adapter_schema_contracts.hh`(test 与 CLI **共用**一份,**禁双实现**)+ `perm_upgrades.json` 与 schema(**诞生即不冻结**);**不扩** `protocol_instance.schema.json` / `protocol_instances.json` 的实例键(design **D-β**);交付**机制**,后续 adapter 复用并各自**填行**;不可测条款 SHALL **不**进判据清单(design **D-κ**)。

#### Scenario: AC-01 —— `perm` **4 值锁**不破(含跨 schema 值域绑定)

- **WHEN** 契约判据跑「4 值锁不破」检查
- **THEN** 子条件 ① SHALL 成立:`protocol_instance.schema.json` 的 `perm` 仍是 **4 值**枚举(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`)且**不含** `ATOMIC`
- **AND** 子条件 ② SHALL 成立:`perm_upgrades.schema.json` 的 `silent_upgrades[].from` / `.to` **必须**取自该 4 值集合 —— 跨 schema `$ref` 绑定 ⇒ **机制上**不可能表达第 5 值(**前提**:`perm_upgrades.schema.json` 已接入仓内既有 `jsonschema` 设施 —— `scripts/test/cep_decl_alignment_check.sh:98` `import jsonschema`;:122/:170 用 `jsonschema.Draft202012Validator`;跨文件 `$ref` 需 `referencing` registry 解析 —— 接入**前**该保证只是**声明性**)
- **AND** 子条件 ③ SHALL 成立:契约侧的**权限秩序**(`NONE < READ < UNIQUE < WRITEBACK`)与 schema 侧的**枚举顺序**一致
- **AND** **反向 fixture**:注入 `from: "ATOMIC"` 或 `to: "SHARED"` ⇒ 断言必红
- **AND** **反向 fixture**:把 `perm` 扩成 5 值 ⇒ 断言必红(4 值锁**不**因本 change 而松动)

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的 AC-01 三子条件断言 + 2 条反向 fixture

#### Scenario: AC-02 —— `perm_upgrade` 引用完整性 + 权限单调

- **WHEN** 契约判据核对静默升级声明源
- **THEN** 子条件 ① SHALL 成立:声明的实例 id **必须**存在于 `protocol_instances.json` 的 `instances[]` 中
- **AND** 子条件 ② SHALL 成立:该实例的 `perm` 数组 SHALL **同时包含** `from` 与 `to` 两个值
- **AND** 子条件 ③ SHALL 成立:`to` 的权限秩 SHALL ≥ `from` 的权限秩(静默升级**不得**降权限)
- **AND** **反向 fixture**:注入不存在的实例 id ⇒ 必红
- **AND** **反向 fixture**:实例的 `perm` 缺 `to` ⇒ 必红
- **AND** **反向 fixture**:`UNIQUE → READ`(降权限)⇒ 必红

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的 AC-02 三子条件断言 + 3 条反向 fixture

#### Scenario: AC-03 —— 三态显式化 + 反向 fixture(`mesif` 不在声明源 / 声明源畸形 ⇒ FAIL)

- **WHEN** 契约判据给出结论
- **THEN** SHALL 显式区分 **`[SKIP]`** / **`[PASS]`** / **`[FAIL]`** 三态(SKIP 与 PASS **可区分**,per 队列 #4 `cep-channel-dependency-gate` 先例)
- **AND** 每条判据 SHALL 输出**逐条可 grep** 的 verdict(格式 `[verdict] <rule-id> PASS|FAIL|SKIP (detail)`,per `ADR-CEP-09 §8.3` 同源格式)
- **AND** **不得**把「无可检查对象」装作 **PASS**
- **AND** 声明源缺失 / 畸形 SHALL 判 **FAIL**(配置错误)—— 与「无可检查对象 ⇒ SKIP」**严格区分**
- **AND** **反向 fixture**:`mesif`(无静默升级能力)SHALL **不**出现在静默升级声明源里;注入 `mesif` 条目 ⇒ 断言必红

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的三态可区分断言 + verdict 格式断言 + 声明源畸形断言 + `mesif` 注入反向 fixture

#### Scenario: AC-04 —— 6→3 映射表完备、值域受限且非恒等

- **WHEN** 契约判据核对 adapter 的消息类 → `CATEGORY_*` 映射表
- **THEN** 子条件 ① SHALL 成立:该实例**实际使用**的每个 6 类消息类**都有**恰好一行映射(无多、无少)
- **AND** 子条件 ② SHALL 成立:每个映射目标 ∈ 链路层 `CATEGORY_*` 的 **3 类**(`CATEGORY_*` **不**为承载 6 类词表而扩类,per `ADR-CEP-12 §8.3` ②)
- **AND** 子条件 ③ SHALL 成立:映射表**非恒等** —— 至少一行的目标 ≠ 同名 `CATEGORY_*`
- **AND** 子条件 ④ SHALL 成立:若映射**确实恒等**,则 SHALL 携带 `identity_reason`(恒等是**合法状态**,但必须**被登记**)
- **AND** **反向 fixture**:删一行 ⇒ 必红
- **AND** **反向 fixture**:目标写成第 4 类 ⇒ 必红
- **AND** **反向 fixture**:全表恒等且**无** `identity_reason` ⇒ 必红

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的 AC-04 四子条件断言 + 3 条反向 fixture

#### Scenario: AC-05 —— adapter 的投影与状态机自洽

- **WHEN** 契约判据核对 adapter 自身的投影表与状态转换表
- **THEN** 子条件 ① SHALL 成立:**每个稳定态**都有 4 值投影(缺一即红)
- **AND** 子条件 ② SHALL 成立:**瞬态态 `T` 无投影**(跨域接口不得出现中间态,per `INV-CEP-05-IC1`)
- **AND** 子条件 ③ SHALL 成立:`E + Write → M` 的转换存在(`state_transition[2][1] == 3`)
- **AND** 子条件 ④ SHALL 成立:该转换后 `perm` 从 `UNIQUE` 变 `WRITEBACK`(投影与转换**一致**)
- **AND** **反向 fixture**:让 `T` 拥有投影 ⇒ 必红
- **AND** **反向 fixture**:改掉 `state_transition[2][1]` ⇒ 必红

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的 AC-05 四子条件断言 + 2 条反向 fixture

#### Scenario: ⑥ 镜像闸的 `status` 收窄 SHALL 有**正控 + 反向 fixture**(收窄守卫不得无牙)

- **WHEN** 核对 `protocol_instances.json` 与 ArchForge 声明的逐字段镜像(⑥ 项,per `scripts/test/cep_decl_alignment_check.sh`)
- **THEN** **正控** SHALL 成立:两侧**仅** `status` 不同时,⑥ 项 SHALL **不**报红(per design **D-ζ**:`status` 是**本仓权威字段** —— 「本仓是否已实现」,非 ArchForge 镜像面)
- **AND** **反向 fixture** SHALL 成立:改 `status` 之外**全部 12 键**中的**任一**(`id` / `family` / `adapter` / `protocol_version` / `flow_support` / `perm` / `atomic` / `consistency_labels` / `ordering_assumption` / `vc_binding` / `sharer_tracker` / `combination_capabilities`)⇒ ⑥ 项 SHALL **必红**(穷举清单 = `protocol_instance.schema.json` 的 `required` 去掉 `status`;**不得**只挑其中几个 —— 漏键 ⇒ 该键的镜像漂移无 fixture 钉住)
- **AND** **反向 fixture** SHALL 成立:新增 / 删除任一实例 ⇒ ⑥ 项 SHALL **必红**
- **AND** 该收窄 SHALL 被**显式登记**进 `scripts/test/cep_decl_reverse_verify.sh` 的反向验证集(不得只存在于 spec 文本)—— 否则 builder 可静默漏做而归档仍绿

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的 ⑥ 收窄正控断言 + ≥2 条反向 fixture(改非 `status` 键 / 增删实例)+ `scripts/test/cep_decl_reverse_verify.sh` 的对应条目

#### Scenario: 判据实现 SHALL **单一**(禁双实现)+ 声明源 SHALL 被**至少一个**测试消费

- **WHEN** 检查判据实现的载体数
- **THEN** 判据实现 SHALL 只存在于 `adapter_schema_contracts.hh` **一份** —— 测试与离线 CLI **共用**该实现,**不得**各自复制一份
- **AND** `perm_upgrades.json` SHALL 被**至少一个**测试消费(⇒ 不产生死文件)
- **AND** `perm_upgrades.schema.json` 与 `perm_upgrades.json` SHALL **逐字段一致**
- **AND** **反向 fixture**:CLI 与测试的 verdict 输出 SHALL **逐条相同**(同一实现 ⇒ 同一结论)

> **锚点**:`test_cep_adapter_schema_contracts.cc` 的「声明源被消费」断言 + 「CLI 与测试 verdict 逐条相同」断言

#### Scenario: S1 有效期重评 —— 结论复核 + 新有效期登记(队列 #6 的 `perm` 形态演进落地触发)

- **WHEN** 队列 #6 的 `perm` 形态演进落地(`perm_upgrades.json` + 配套 schema 的 `perm_upgrade` 键 ⇒ `cep-poc-spike-verification` main spec 的 S1 有效期锚 `S1_VALIDITY_UNTIL=queue6-perm-schema-evolution` **届满**)
- **THEN** 重评结论 SHALL 为:**S1 仍为 FAIL**(结论**未翻转**)—— R-2 的发生点不变:写路径的 `perm` 仍在**请求侧**携带 `UNIQUE`,而期望投影是 `WRITEBACK` ⇒ `perm` 4 值投影**仍**不能表达静默 `E→M` 升级
- **AND** 重评 SHALL 记录**可发现性的改善**:「哪个协议实例有静默升级能力」现有**机器可读**的声明源(`perm_upgrades.json` + 跨 schema `$ref` 绑定)⇒ 消费端可据此修正请求侧 `perm`;但**修正机制本身不在本 change 的交付面**(Stub↔PE 腿归 `cpptlm-cep-stub-cache-chstream-surface`)
- **AND** 新有效期 SHALL 被显式标注:`S1_VALIDITY_UNTIL=stub-request-side-perm-extension`
- **AND** 新有效期的**届满条件 SHALL 机器可检测**:① `include/tlm/cep/stub_cache_core.hh` 出现请求侧 `perm_upgrade` 扩展;② `protocol_instance.schema.json` 的 `perm` 枚举 ≠ **4** 值(4 值锁被击穿)⇒ 任一命中即须**再次重评**(否则新有效期同样是纯纸面)
- **AND** 重评 SHALL 落在**契约层**(本 delta spec ⇒ 归档后并入 `openspec/specs/cep-adapter-schema-contracts/spec.md`)而**非**冻结的 `cep-poc-spike-verification` main spec —— 后者的 S1 Scenario **逐字仍然成立**(结论仍 FAIL / 机器登记仍 `S1_VERDICT=FAIL` + `S1_MISMATCH_STEPS=1` / spike 仍打印原锚)⇒ **零冻结面改动**

> **锚点**:`scripts/test/cep_docs_sync_check.sh` 断言 ⑧ 的「S1 有效期重评**已登记**」检查(新锚出现在本 delta spec 或归档后的 main spec)+ 2 条反向 fixture(新锚缺失 ⇒ 必红 / 新届满条件命中 ⇒ 必红)
