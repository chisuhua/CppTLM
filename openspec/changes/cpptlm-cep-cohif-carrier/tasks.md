# Tasks — cpptlm-cep-cohif-carrier v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-10-27 转正;实施**已启动** —— Phase 0 前置已闭合 + **Phase 1 / Phase 2 已完成**(2026-10-27,台账 Entry #84 / #85);Phase 3 ~ 7 未开始)· **队列**: `obj-cep-platform §4.0` #7(Stage 1 / 2028-Q2)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-cohif-carrier/spec.md`](specs/cep-cohif-carrier/spec.md) · [`specs/cep-protocol-bridge-contracts/spec.md`](specs/cep-protocol-bridge-contracts/spec.md)
> **上游决策**: `ADR-CEP-19`(✅ v1.0 已签发)· 架构重审 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3)
> **估时**: 6 周(3 项交付 + 1 宏 + 1 钩子扩展)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;转正后并发 **1/3**,per Entry #81)

---

## 执行进度(待 builder 阶段回填)

| Phase | 状态 | 说明 |
|-------|------|------|
| 0 前置 | ✅ **已闭合**(2026-10-27) | `tlm::cep::ContractVerdict` 同名冲突消解(独立 fix commit `7f91abd9`;本 change 启动前的前置,不属本 change 交付面) |
| 1 载体面:`CohIfBundle` | ✅ **已完成**(2026-10-27) | `include/bundles/cep_coherence_bundles_tlm.hh` —— `bundles::CohIfBundle`(12 字段)+ 强类型 `CohIfFlowId`(域恰好 5 值)+ 12 条 `BUNDLE_FIELD_WIDTH` 锁;**4 类反向 fixture 实测有牙**(2 静态扫描红 / 2 编译红) |
| 2 端口面:`ProtocolEngine` 基类 | ✅ **已完成**(2026-10-27) | `include/tlm/cep/protocol_engine_base.hh` —— `ProtocolEngineBase<N>`(派生 `ChStreamModuleBase` + `Input/OutputStreamAdapter<CohIfBundle>` 端口成员 + 类型擦除 `ProtocolBridgeEr*` + 10 个纯虚 `on_*`)+ `NullProtocolEngine<N>`(**10 流覆盖矩阵** + 两条编译期锁);**2 类反向 fixture 实测有牙** |
| 3 特化桥:`ProtocolBridge` 双层 | ⬜ 未开始 | `include/framework/protocol_bridge.hh` + 钩子扩展 |
| 4 装配宏:`REGISTER_CHSTREAM_BIDIRECTIONAL` | ⬜ 未开始 | `include/chstream_register.hh` |
| 5 JSON 装配连通性 | ⬜ 未开始 | `configs/` 侧示例 + E2E |
| 6 测试(CC-01~CC-06 + 反向 fixture) | ⬜ 未开始 | 2 个测试文件 |
| 7 闸与真源同步 | ⬜ 未开始 | `cep_decl_reverse_verify.sh` 条目 + `06 号 §3.2` 归类表 |

---

## 逐任务清单(TDD 5-step:判据 → 实现 → 夹具 → 验证 → 登记)

### Phase 0 —— 前置(已闭合,2026-10-27)

> **为什么单列 Phase 0**:本 change 是 **CohIf 契约面** change —— task 6.3 要求**引用**队列 #6 的
> `adapter_schema_contracts.hh`,而 canonical 10 流的校验面在 `collective_plane_contract.hh`
> ⇒ **两个契约头必须在同一 TU 共存**。此前它们在 `tlm::cep` 命名空间下**同名定义了
> `ContractVerdict` / `ContractVerdictLine` / `ContractReport` / `contract_verdict_name`**
> 且**底层值序相反** ⇒ 同时 include 即编译失败(已实测复现 `multiple definition`)。
> 该缺陷在本 change 启动**前**已由独立 fix commit 消解(不属于本 change 的交付面)。

- [x] 0.1 **`tlm::cep::ContractVerdict` 同名冲突消解** —— 改名 adapter 侧为
      `AdapterContractVerdict` / `AdapterContractVerdictLine` / `AdapterContractReport` /
      `adapter_contract_verdict_name`(消费面更小:3 个 TU vs collective 侧 4 个);
      **语义与行为零变化**(枚举仅按名访问,无 `static_cast` / 下标依赖值序);
      **符号名从未进入 `openspec/specs/` 或 ADR 契约面** ⇒ 无 delta spec 变更。
      **已完成**:commit `7f91abd9`(2026-10-27;台账 Entry #83)

### Phase 1 —— 载体面(`CohIfBundle`,判据 CC-01 / CC-02)

- [x] 1.1 `bundles::CohIfBundle` 定义(`include/bundles/cep_coherence_bundles_tlm.hh`)——
      **已完成**(2026-10-27):`bundles::CohIfBundle : public bundle_base`,**12 字段**;
      载体形态 per design `D-α`(单一类型)+ `D-δ`(路径 `include/bundles/cep_coherence_bundles_tlm.hh`)。
- [x] 1.2 `flow_id` 域 **恰好 5 值**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`)——
      域外流名**类型不可表达**(CC-01 ③)——
      **已完成**:`enum class CohIfFlowId : std::uint8_t`(**强类型**,非裸 `ch_uint<8>`)——
      裸整数会让域外 5 条流在**类型层**仍可表达 ⇒ 「对 stub 不可见」退化为**运行期纪律**
      (与 `design.md D-α` 的裁决相反)。**代价与处置**:枚举无成员 ⇒ `BUNDLE_FIELD_WIDTH`
      取不到 `::width` ⇒ `include/utils/port_contract_asserts.hh` 的宽度 trait 增枚举分支
      (**纯增量**,对既有 `ch_uint<W>` 字段行为逐字不变)⇒ CC-02 ③「覆盖**全部**字段」得以成立。
- [x] 1.3 字段集**不含**网络层字段(`src_node` / `dst_node` / `flit_category` / `vc_id`)(CC-01 ②)——
      **已完成**:字段集 = `flow_id` / `transaction_id` / `address` / `data` / `size` / `perm` /
      `snoop_type` / `line_state` / `hit` / `dirty` / `error` / `atomic`;
      **逐值对应** `tlm::cep::Perm` / `SnoopType` / `LineState`(本头位于最底层,**不**反向依赖
      `include/tlm/cep/` ⇒ 值域对应由 Phase 6 的测试层做编译期交叉断言)。
- [x] 1.4 `BUNDLE_FIELD_WIDTH` 覆盖**全部**字段 + `static_assert(sizeof(...) == 期望)` +
      `is_trivially_copyable`(CC-02 ①②③)——
      **已完成**:`BUNDLE_FIELD_WIDTH` × **12**(头文件末尾,含 `flow_id` 8 bit);
      `is_trivially_copyable` 静态断言写在**头文件**内(随类型走,不依赖测试 TU);
      `sizeof` 断言当前落在 `scripts/test/cep_cohif_compile_probe.cc`(= 96 字节),
      Phase 6 的 test TU 将**同款镜像**(CC-02 ① 原文要求「test TU 内」)。
- [x] 1.5 **反向 fixture**:新增第 6 个 `flow_id` ⇒ 静态扫描用例必红——
      **已完成**:毒化副本生成器 `poison_cohif_bundle(..., CohIfPoison::FlowCount6)`
      + 静态扫描器 `scan_cohif_bundle()`(共享于 `test/cep_cohif_contract.hh`);
      **实测报红**(见 Phase 1 的反向验证脚本输出)。
- [x] 1.6 **反向 fixture**:加入任一网络层字段 ⇒ 静态扫描用例必红——
      **已完成**:毒化副本生成器 `CohIfPoison::NetworkField`(注入 `ch_uint<32> src_node;`);
      **实测报红**。
- [x] 1.7 **反向 fixture**:字段宽度 +1 的**毒化副本**走 `try_compile` ⇒ 编译必失败;加虚函数 ⇒
      `is_trivially_copyable` 断言必失败——
      **已完成**:毒化副本生成器 `CohIfPoison::WidthPlusOne` / `CohIfPoison::VirtualFunction`
      + `scripts/test/cep_cohif_compile_probe.cc`(含 `sizeof` / `is_trivially_copyable` 断言);
      **实测编译失败**。
      ⚠️ 扫描器 / 毒化生成器的**共享实现**放在 `test/cep_cohif_contract.hh`(Phase 6 的
      Catch2 用例与 Phase 1 的反向验证脚本**共用同一份** ⇒ 避免两份判据漂移)。
- [x] 1.8 `06 号 §3.2` 归类表同步(第三类 = 跨模块共享的业务 Bundle;真源 1,**非**冻结集)——
      **已完成**:`06 号` §3.2 归类表增补 `cep_coherence_bundles_tlm.hh::CohIfBundle` 行
      (类别 = **跨模块共享的业务**)+ 计数 17 → **18**;§3.3「唯一实例」由「规划实例(当前零实例)」
      改为「**已落地(1 个)**」;§1.3 第 3 项「不引入新 Bundle 类型」加**例外**说明;
      `01 号` §1.2 计数同步(18 个 .hh / 27 个 Bundle 类型);`07 号` 走第四则 Status Update
      (含 §5.4 第三类同步 —— 第一则附录 ③ 声明的「已到期动作」)。

### Phase 2 —— 端口面(`ProtocolEngine` 基类)

- [x] 2.1 `ProtocolEngineBase` 派生 `ChStreamModuleBase`(`include/tlm/cep/protocol_engine_base.hh`)——
      **已完成**(2026-10-27):`template <std::size_t N> class ProtocolEngineBase : public ChStreamModuleBase`。
      **端口数 N 取编译期模板参数** —— 理由:`ChStreamAdapterFactory::registerBidirectionalPortAdapter<
      ModuleT, BundleT, N>` 的 `N` 是模板参数(`include/framework/chstream_adapter_factory.hh:103`),
      而 adapter 内部按 `m->req_in()[i]` 索引 ⇒ N 必须编译期确定,否则「adapter 的 N」与
      「模块的运行时端口数」会出现**两个真源**且可能静默分歧。
      附带 `static_assert(N > 1)`(N=1 会落 `bind_ports` **空操作 shim** ⇒ 静默不绑定)。
- [x] 2.2 `Input/OutputStreamAdapter<CohIfBundle>` 端口成员(照 `router_tlm.hh:139-141` 范式)——
      **已完成**:`std::array<ReqAdapter, N> req_in_` / `std::array<RespAdapter, N> resp_out_`
      + 4 个访问器(含 const 重载);`ReqAdapter`/`RespAdapter` 均为
      `cpptlm::Input/OutputStreamAdapter<bundles::CohIfBundle>`(**1 个 Bundle 类型 ⇒
      N 个端口共用同一对适配器类型**,per design `D-ε`)。
- [x] 2.3 持 `ProtocolBridgeEr*`(类型擦除基类指针,不引入协议语义基类)——
      **已完成**:`cpptlm::ProtocolBridgeEr* bridge_`(**前向声明** + **非拥有**;
      完整定义在 `include/framework/protocol_bridge.hh`,归 tasks 3.1);
      `set_bridge()` / `bridge()`。⚠️ **前向声明即可**的原因:非拥有 ⇒ 无需完整类型即可析构
      ⇒ Phase 2 **不**依赖 Phase 3 的产物(生命周期由 ModuleFactory / 装配链负责,
      per tasks 3.6「注册与消费成对」)。
- [x] 2.4 端口方向语义登记:`req_in[i]` = ingress / `resp_out[i]` = egress;**流语义由
      `flow_id` 决定**(design `D-ε`)——
      **已完成**:端口方向语义写在头文件「端口方向语义」段;`dispatch_cohif()` 按
      `flow_id` 分派 ⇒ **流语义由 `flow_id` 决定,不由端口方向决定**。
- [x] 2.5 **`NullProtocolEngine`** —— `ProtocolEngineBase` 的 **ADR 强制默认实现**
      (per `ADR-CEP-07 §3` `INV-CEP-07-IC2`:`NullProtocolEngine` 是 ProtocolEngine
      **默认实现**,须有 **10 流覆盖矩阵**;逐流行为 per `07 号 §6.4`:
      流 **1/3/4/5/6/7/10** = no-op · 流 **2** = immediate-complete ·
      流 **8** = immediate-resolve · 流 **9** = immediate-complete)——
      落 `include/tlm/cep/protocol_engine_base.hh`。
      ⚠️ **本项在 2026-10-27 之前无队列 owner**(全仓零载体),owner 自本任务起 = **队列 #7**
      —— **已完成**(2026-10-27):
      · `ProtocolEngineBase` 的 **10 个 `on_*` 纯虚方法**(per `ADR-CEP-02` §2 的流表
        + `ADR-CEP-07` 决策 4 草图注释「必须全 10 流」)⇒ 「10 流覆盖」由**类型系统**
        保证,而非运行期纪律
      · `kNullFlowMatrix`(C 数组 + 显式 size)+ 两条编译期锁:
        `null_flow_matrix_is_complete()`(各流恰好一次且总数 10)/
        `null_flow_matrix_behaviors_are_expected()`(逐流行为 vs `07 号 §6.4` 口径)
      · 可观测状态:每流计数 + `no_op`/`immediate_complete`/`immediate_resolve` 分桶计数
        (只记「调用次数」的计数器**无法区分** no-op 与 immediate-complete ⇒ 不算有判别力)
      · `on_address_resolve` 的 Null 解析口径:无真实目录 ⇒ `home_node = node_id`
        (请求者自身)+ `coherent = false`(**不做任何一致性声称**)
      · **⚠️ 落点偏离登记**:`ADR-CEP-07` 决策 4 的草图路径是
        `include/tlm/cep/null_protocol_engine.hh`,本实现落 `protocol_engine_base.hh`
        (per 本 task 的「落 …」句)⇒ **路径偏离已登记**(见 `ADR-CEP-07` Status Update)
- [x] 2.6 **反向 fixture**:把 `NullProtocolEngine` 的 10 流覆盖矩阵改成**缺 1 流**
      ⇒ 覆盖矩阵断言必红(per 判据纪律「纯负向断言必须有正控」)——
      **已完成**:毒化副本生成器 `poison_protocol_engine_base(..., MatrixDropOne)`
      (删掉 barrier 项 ⇒ 矩阵 size 9)⇒ **覆盖完整性** `static_assert` 触发。
      **实测**:编译失败,且错误信息**精确命中**预期的完整性断言(非「因错误的原因红」)。
- [x] 2.7 **反向 fixture**:把某流的 `immediate-complete` 语义改成 no-op
      ⇒ 该流的行为断言必红——
      **已完成**:毒化副本生成器 `BehaviorDegrade`(barrier 的 ImmediateComplete → NoOp)
      ⇒ **逐流行为** `static_assert` 触发。**实测**:编译失败,精确命中预期断言。
      ⚠️ **矩阵用 C 数组 + 显式 size**(非 `std::array<T,10>`)—— 否则「少一个元素」
      会因**初始化器个数不足**而编不过(理由不对 ⇒ 判据被「错误的原因」满足)。

### Phase 3 —— 特化桥(判据 CC-03 / CC-05)

- [ ] 3.1 `ProtocolBridgeEr`(类型擦除基类)+ `AbstractProtocolBridge`(identity 基线)(`include/framework/protocol_bridge.hh`)
- [ ] 3.2 `ProtocolBridge<InB, OutB, Ordered>` 模板子类(`Ordered` 单 bool,per `06 号 §4.4`)
- [ ] 3.3 `pack` / `unpack` 对 `CohIfBundle ↔ NoCFlitBundle` **双向可逆**(CC-03 ①)
- [ ] 3.4 职责边界:网络层语义只在 `NoCFlitBundle` 侧(CC-03 ②);同一 `ProtocolBridgeEr*` 可被多具体桥共享(CC-03 ③)
- [ ] 3.5 `register_protocol_bridge(name, factory_lambda)` 钩子扩展(`include/core/module_factory.hh:313-315`,死钩子 → 活钩子)
- [ ] 3.6 `get_bridges()` 可读 + 被 ModuleFactory **消费**(注册与消费成对,CC-05 ①②)
- [ ] 3.7 per-bridge 元数据齐备:`protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明 `family` 一致(CC-05 ④)
- [ ] 3.8 **反向 fixture**:只注册不消费 ⇒ 必红;重复 `bridge_id_` ⇒ 必红;`protocol_id_` 与 `family` 不符 ⇒ 必红
- [ ] 3.9 **反向 fixture**:篡改任一字段映射(如 `perm` → `flit_category`)⇒ round-trip 必红

### Phase 4 —— 装配宏(判据 CC-04 ③)

- [ ] 4.1 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)`(`include/chstream_register.hh`)
- [ ] 4.2 宏**同时**做 `registerObject` **与** `registerBidirectionalPortAdapter`(缺 `registerObject` ⇒ ModuleFactory 无法实例化)
- [ ] 4.3 宏内 `static_assert(n > 1)`(防 `N=1` 落 `bind_ports` 空 shim 静默不绑定)
- [ ] 4.4 **反向 fixture**:`N = 1` ⇒ 编译必失败

### Phase 5 —— JSON 装配连通性(判据 CC-04 ①②)

- [ ] 5.1 走既有 JSON 端口注册链(`chstream_adapter_factory` → `module_factory.cc` `is_multi` 分支),**不**做 programmatic 端口注入
- [ ] 5.2 示例配置:`configs/` 下新增最小 PE 拓扑(N ≥ 2 端口)
- [ ] 5.3 端到端:一次 CohIf 事务**穿过** adapter 抵达对端(非仅绑定成功)
- [ ] 5.4 **反向 fixture**:断开某端口绑定 ⇒ 端到端断言必红

### Phase 6 —— 测试

- [ ] 6.1 `test/test_cep_cohif_carrier.cc`(CC-01 / CC-02 / CC-04)
- [ ] 6.2 `test/test_cep_protocol_bridge_contracts.cc`(CC-03 / CC-05 / CC-06)
- [ ] 6.3 CC-06 **引用**队列 #6 的 AC-04 实现(`adapter_schema_contracts.hh`),**不重立**第二份判据

### Phase 7 —— 闸与真源同步

- [ ] 7.1 `scripts/test/cep_decl_reverse_verify.sh` 登记本 change 的反向 fixture 条目
- [ ] 7.2 `test/test_cep_canonical_invariants.cc` **零 diff**(新增 invariant 数 = 0)
- [ ] 7.3 守门 7 项 + `cep_gate_selftest` + `ctest` 全绿

---

## 归档前必须完成(复核)

| # | 项 | 状态 |
|---|---|---|
| 1 | 判据增补**已追认**(CC-05 ④ / CC-06 ②) | ✅ 转正前置已落地(`ADR-CEP-19` Status Update,Entry #78) |
| 2 | 每条新判据有**反向 fixture** | 🟡 Phase 1 **已完成**(4 类 fixture 实测有牙)· Phase 2 **已完成**(2 类 fixture 实测有牙);Phase 3/4/5 待补 |
| 3 | 编译期锁**不**以「编译红」充当 ctest 判据(`try_compile` 毒化副本) | 🟡 Phase 1.7 **已完成**(毒化副本 + 编译探针断言「编译确实失败」);Phase 4.4 待补 |
| 4 | 新增 invariant 数 = **0** | ⬜ Phase 7.2 |
| 5 | 冻结面**零改动**(本 change 3 个新头文件 + 1 宏 + 1 钩子均不在 M1 冻结集) | ⬜ 待实施后复核 |
| 6 | `StubCacheCore` **零触碰**(design `D-ζ`,显式延期) | ⬜ 待实施后复核 |

---

## 已知边界(诚实登记,不掩盖)

- **Stub↔PE 腿不在本 change**:`StubCacheCore` 的 ChStream 端口面延期到 `cpptlm-cep-stub-cache-chstream-surface`(design `D-ζ` 的四条互斥理由);本 change 只闭 **PE→网络** 段 + PE 输入侧类型契约。
- **6×6 状态机不做跨仓逐格镜像**:除 `state_transition[2][1] == 3` 外漂移无人发现 —— 缺口已在 `ADR-CEP-19` 决策层登记,本 change 不重复登记。
- **判据定义真源**:转正前真源在附属层调研文档(`docs/research/...review-2026-10-26.md §6.4`);本 change 的 delta spec 是**追认后**的契约层承载 ⇒ 归档并入 `openspec/specs/` 后即自足。
