# Tasks — cpptlm-cep-cohif-carrier v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-10-27 转正;实施**已启动** —— Phase 0 前置已闭合 + **Phase 1 ~ Phase 4 已完成**(2026-10-27 / 2026-10-28,台账 Entry #84 ~ #87);Phase 5 ~ 7 未开始)· **队列**: `obj-cep-platform §4.0` #7(Stage 1 / 2028-Q2)
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
| 3 特化桥:`ProtocolBridge` 双层 | ✅ **已完成**(2026-10-28) | `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>` + `AbstractProtocolBridge`)+ `include/tlm/cep/cohif_noc_bridge.hh`(CohIf ↔ NoC 特化桥,**两层** pack/unpack)+ `include/tlm/cep/protocol_family.hh`;钩子扩展 `register_protocol_bridge(name, factory_lambda)` + `get_bridges()` 转 public + `ModuleFactory` **Step 7.5** 消费;**3 + 2 类反向 fixture 实测有牙** |
| 4 装配宏:`REGISTER_CHSTREAM_BIDIRECTIONAL` | ✅ **已完成**(2026-10-28) | `include/chstream_register.hh`(两件都做 + `static_assert(n > 1)`)+ **前置泛化** `cpptlm::BundleDirectionTraits`(解耦 `BidirectionalPortAdapter` 的 `NoCFlitBundle` 硬编码)+ CEP 侧特化 `include/tlm/cep/cohif_direction_traits.hh`;**反向 fixture 实测有牙**(N=1 编译红且原因精确命中断言) |
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

- [x] 3.1 `ProtocolBridgeEr`(类型擦除基类)+ `AbstractProtocolBridge`(identity 基线)(`include/framework/protocol_bridge.hh`)——
      **已完成**(2026-10-28):按 `06 号 §4.4` 原型落地。**比原型更严一处**:`metadata_ok()`
      要求 `protocol_id_` / `bridge_id_` **非零** —— 理由:`0` 需能表达「未分配」这一**可测状态**,
      否则「忘了分配 id」与「合法 id 0」不可分辨。**登记备查**。
- [x] 3.2 `ProtocolBridge<InB,OutB,Ordered>` 模板子类(`Ordered` 单 bool,per `06 号 §4.4`)——
      **已完成**:`template <typename InB, typename OutB, bool Ordered = false>`;
      `static_assert(is_trivially_copyable_v<InB/OutB>)`(端到端「逐字段恒等」依赖**字节等价**)。
      ⚠️ **踩过的坑**:依赖基类(`ProtocolBridge<FlitT,...>`)的**非静态**数据成员 `stats_`
      必须写 `this->stats_` —— 否则按 [temp.dep.expr] 在模板定义期名字查找不可见 ⇒
      编译报「`stats_` was not declared in this scope」(实测踩过)。
- [x] 3.3 `pack` / `unpack` 对 `CohIfBundle ↔ NoCFlitBundle` **双向可逆**(CC-03 ①)——
      **已完成**,但**分两层**(per 2026-10-28 DRI 裁决的 **B′ 方案**;论证见 `ADR-CEP-19`
      Status Update 2026-10-28(第二则)②):
      · **层 1 · 消息内容投影**(`pack`/`unpack`):**共有字段 4 个**
        (`transaction_id`/`address`/`data`/`size`)双向**无损**;协议语义字段按
        **字段三分类**显式登记 —— **共有 4** / **降维 1**(`flow_id` → `flit_category`,
        5 值→3 值**多对一 ⇒ 不可逆**) / **丢弃 7**(`perm`/`snoop_type`/`line_state`/`hit`/
        `dirty`/`error`/`atomic`);网络封装字段(`src_node`/`dst_node`/`vc_id`/`flit_type`/
        `flit_index`/`flit_count`/`hops`/`src_port`/`is_write`/`is_ok`/`error_code`)
        **既不写也不读** ⇒ per CC-03 ② 的职责边界。
      · **层 2 · 端到端完整语义**(`pack_with_packet`/`unpack_with_packet`,`06 号 §4.4` 原型
        **已有**这两个虚函数 ⇒ 零新增抽象):`Packet` payload 数据区承载 `CohIfBundle` 的
        **完整 12 字段字节** ⇒ **逐字段恒等**成立。
      ⚠️ **架构事实(本 change 的核心发现)**:`flow_id` 域 = **5 值**,`flit_category` 域 =
        **3 值**,队列 #6 的 6→3 映射表**刻意多对一** ⇒ **仅靠网络 flit 字段空间不可能唯一
        恢复 `flow_id`** ⇒ Bundle 级「逐字段 round-trip 恒等」**在数学上不可达** ⇒ 协议语义
        必须有自己的落脚点。⇒ **判据未被削弱反而被加强**(端到端层覆盖**跨真实网络**的往返)。
- [x] 3.4 职责边界:网络层语义只在 `NoCFlitBundle` 侧(CC-03 ②);同一 `ProtocolBridgeEr*`
      可被多具体桥共享(CC-03 ③)——
      **已完成**:① 职责边界由判据「网络封装字段不被桥改写」机器核对(预置 11 个网络字段
      → pack 后逐一比对);② 共享 = `ChStreamModuleBase::attach_protocol_bridge` 把**同一个**
      桥指针挂到**多个** PE(测试里 `pe0`/`pe1` 挂同一 `ProtocolBridgeEr*` ⇒ 断言同一指针)。
      ⚠️ **挂载点选型**:`ProtocolEngineBase<N>` 的 `N` 是**模板参数** ⇒ ModuleFactory 无法
      `dynamic_cast` 到具体类型;独立 mixin 与 `SimObject` **无继承关系** ⇒ `dynamic_cast`
      **编译不过**,让 mixin 继承 `SimObject` 则引入**菱形继承** ⇒ 更糟 ⇒ 最终用
      `ChStreamModuleBase` 的**可选**虚函数 `attach_protocol_bridge`(默认返回 false ⇒ **零波及**)。
- [x] 3.5 `register_protocol_bridge(name, factory_lambda)` 钩子扩展(`include/core/module_factory.hh:313-315`,死钩子 → 活钩子)——
      **已完成**:签名扩为 `(bridge_name, ProtocolBridgeFactory)`;注册表存 **name → factory**;
      **旧单参签名已移除**(全仓零调用点 ⇒ 无兼容负担;保留「只给名字不给工厂」会制造
      「注册了却造不出桥」—— 正是 CC-05 反向 fixture「只注册不消费 ⇒ 必红」要抓的形态)。
      **不**引入独立 factory 抽象类(per `06 号 §11.2` P3 子项的显式偏离)。
- [x] 3.6 `get_bridges()` 可读 + 被 ModuleFactory **消费**(注册与消费成对,CC-05 ①②)——
      **已完成**:`get_bridges()` 从 `private:` 转 **public**;消费路径 = `ModuleFactory::instantiateAll`
      的 **Step 7.5**(per `06 号 §4.5` 的「per-bridge 配置时机 = ModuleFactory Step 7c(构建期)」)
      读 **既有** `config["cep"]["bridges"]` 段 ⇒ **零新增** JSON 顶层段 → 建桥 → 校验元数据 →
      注入目标模块。
      ⚠️ **「只注册不消费」不判装配失败**(多拓扑共用进程级注册表时某个拓扑不引用全部桥是正常的
      ⇒ 强制失败会误伤)⇒ 以**死桥清单** `unconsumed_bridges()` 暴露,由判据测试断言其非空。
      ⚠️ **踩过的坑**:Step 7.5 必须查**局部** `object_instances` —— member `instances` 在
      `instantiateAll` 末尾才 `= std::move(object_instances)`(Step 7.5 之前为空 ⇒ 报「模块不存在」)。
- [x] 3.7 per-bridge 元数据齐备:`protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明的 `family`
      一致(CC-05 ④)——
      **已完成**,**口径经 2026-10-28 DRI 裁决**:**`protocol_id_` = 协议来源族的数字编号**
      (`family` ∈ {`industrial`,`ruby-slicc`,`academic`},编号唯一真源 =
      `include/tlm/cep/protocol_family.hh`);**实例级唯一性由 `bridge_id_` 负责**。
      ⚠️ 判据原措辞「与 `family` 一致」若按「相等」读则**恒真**(多个 industrial 实例同族)
      ⇒ 按**编号对齐**落地 ⇒ 判据可机器检查且**未被削弱**。
      判据实现抽成 **public 静态函数**(可独立单测,反向 fixture 无需跑完整装配):
      `ModuleFactory::validate_bridge_metadata(...)` / `ModuleFactory::check_bridge_id_collision(...)`。
      ⚠️ **附带**:`DeclaredInstance` 加 `family` 字段(`channel_dependency_graph.hh`),
      避免 ModuleFactory 侧重复解析协议实例 JSON。
- [x] 3.8 **反向 fixture**:只注册不消费 ⇒ 必红;重复 `bridge_id_` ⇒ 必红;`protocol_id_` 与 `family` 不符 ⇒ 必红——
      **已完成**,3 条**实测有牙**且**因正确原因**红:
      ① 只注册不消费 ⇒ `unconsumed_bridges()` 含该桥 ⇒ 红;
      ② 重复 `bridge_id_` ⇒ `check_bridge_id_collision` 红,且错误信息**点名**冲突的两个桥名;
      ③ `protocol_id_` 与族编号不符 ⇒ `validate_bridge_metadata` 红,错误信息给出两边的编号。
- [x] 3.9 **反向 fixture**:篡改任一字段映射(如 `perm` → `flit_category`)⇒ round-trip 必红——
      **已完成**,2 类毒化**实测有牙**且**因正确原因**红:
      ① `perm` → `flit_category` ⇒ **字段映射探针**红(报「`perm` 泄漏」+「`flow_id` 跨类别
      未改变网络 flit」—— 篡改确实破坏了 `flow_id` 的降维映射);
      ② `address` ↔ `data` 对调 ⇒ **内容层 round-trip** 红(报「`address`/`data` 不相等」)。
      ⚠️ **判据设计的关键**:「丢弃清单 ↔ 实际行为」用**行为式**判定(逐个变更字段 → 看网络 flit
      是否跟着变),**不是**清单比对 ⇒ 篡改能被抓到。
      ⚠️ **判据抓到的真实不一致(已修正)**:初版把 `flow_id` 列为「丢弃」,但它**确实**影响
      `flit_category` ⇒ 行为式判据**假红** ⇒ 修正为**字段三分类**(共有 4 / 降维 1 / 丢弃 7)。
      ⚠️ **Phase 3 的判据是运行期语义** ⇒ 毒化副本必须**编得过**但**判红**(若编译失败就是
      「因错误的原因红」)⇒ 反向验证脚本对每个毒化**同时**断言「编译成功」+「运行期判红」。

### Phase 4 —— 装配宏(判据 CC-04 ③)

- [x] 4.1 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)`(`include/chstream_register.hh`)——
      **已完成**(2026-10-28):参数化宏,用法
      `REGISTER_CHSTREAM_BIDIRECTIONAL(MyPE, bundles::CohIfBundle, 5);`(在**函数体内**,
      与 `REGISTER_CHSTREAM` 同风格)。
      ⚠️ **与既有宏的差异**:`REGISTER_CHSTREAM` 是**批量**宏(一次注册全部 ChStream 类型);
      本宏是**单类型参数化**宏(per-instance 注册)。
- [x] 4.2 宏**同时**做 `registerObject` **与** `registerBidirectionalPortAdapter`(缺 `registerObject`
      ⇒ ModuleFactory 无法实例化该类型)——
      **已完成**,宏体**两件都做**。
      ⚠️ **不**照抄 `REGISTER_CHSTREAM_DUAL` 的**半截实现** —— 那个宏**只**注册 adapter、
      **不含** `registerObject`,且全仓零调用点 ⇒ 不能作为形态基准。
      ⇒ **运行期判据**:注册后 `ChStreamAdapterFactory::get().isMultiPort(type)` 为 true、
      `getPortCount(type) == N`,且 `ModuleFactory::instantiateAll` 能用该类型实例化 JSON 模块
      (实测 4 case / 15 assertion 全绿)。
- [x] 4.3 宏内 `static_assert(n > 1)`(防 `N=1` 落 `bind_ports` 空 shim 静默不绑定)——
      **已完成**:宏体第一条语句即 `static_assert((n) > 1, ...)`。
      ⚠️ **判据 CC-04 ③ 的机器可检形态**:N=1 落 ModuleFactory 的 single 分支调 `bind_ports`
      —— 而 `bidirectional_port_adapter.hh` 的 `bind_ports` 是**空操作 shim** ⇒ **静默不绑定**
      (不报错、不绑定)⇒ 编译期锁是唯一防线。
- [x] 4.4 **反向 fixture**:`N = 1` ⇒ 编译必失败——
      **已完成**,**实测有牙**且**因正确原因**红:
      N=1 ⇒ 编译失败,且错误信息**精确命中**宏内的 `static_assert`(脚本检查错误文本
      「端口数 N 必须 > 1」)⇒ 若失败原因是别的(例如类型不匹配),脚本判红
      (防「因错误的原因红」);N=2 ⇒ 编译成功(**正控**)。
      ⇒ 已注册 ctest `cep_chstream_bidirectional_macro_reverse_verify`(3/3)。

- [x] 4.5 **前置的必要泛化**(`BidirectionalPortAdapter` 与 Bundle 解耦)——
      **已完成**(本项不在原 task 表内,是实施中发现的**阻塞前提**,登记备查):
      该 adapter 原本**硬编码** `NoCFlitBundle` 语义(读 `flit_category` 决定 `PKT_REQ`/`PKT_RESP`;
      credit 用 `NoCFlitBundle` 构造)⇒ 承载 `CohIfBundle` **编译不过**
      (`CohIfBundle` 无 `flit_category` / `vc_id` 字段)⇒ **不泛化则 4.1~4.4 无法用
      CohIfBundle 验证**。
      ⇒ 抽成 `cpptlm::BundleDirectionTraits<BundleT>`(方向判定 + credit 载体):
      **框架层不认识任何具体 Bundle**;各 Bundle 在**自己的层**提供特化
      (CEP 侧:`include/tlm/cep/cohif_direction_traits.hh`)。
      ⚠️ **未特化的 BundleT ⇒ 编译期报错**(不静默用错方向)。
      ⚠️ **`NoCFlitBundle` 特化逐行为等价**于泛化前的硬编码实现 ⇒ **全量回归 0 变化**。
      ⚠️ **层次纪律**:特化放 CEP 侧(`tlm/cep` → `framework` ✓),**不放** `include/bundles/`
      (bundles 是最底层,**不得**反向依赖 `core/` / `framework/`)。
      ⚠️ 附带:`ProtocolEngineBase` 加 `virtual void receive_credit(unsigned, unsigned) {}`
      (**默认空实现 ⇒ 零波及**)—— `BidirectionalPortAdapter::tick()` 收 credit 时会调
      `module_->receive_credit(port, vc)` ⇒ 任何用该 adapter 的模块**必须**提供此方法。

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
| 2 | 每条新判据有**反向 fixture** | 🟡 Phase 1 **已完成**(4 类)· Phase 2 **已完成**(2 类)· Phase 3 **已完成**(3 + 2 类)· Phase 4 **已完成**(1 类,实测有牙);Phase 5 待补 |
| 3 | 编译期锁**不**以「编译红」充当 ctest 判据(`try_compile` 毒化副本) | 🟡 Phase 1.7 **已完成**(毒化副本 + 编译探针断言「编译确实失败」);Phase 4.4 待补 |
| 4 | 新增 invariant 数 = **0** | ⬜ Phase 7.2 |
| 5 | 冻结面**零改动**(本 change 3 个新头文件 + 1 宏 + 1 钩子均不在 M1 冻结集) | ⬜ 待实施后复核 |
| 6 | `StubCacheCore` **零触碰**(design `D-ζ`,显式延期) | ⬜ 待实施后复核 |

---

## 已知边界(诚实登记,不掩盖)

- **Stub↔PE 腿不在本 change**:`StubCacheCore` 的 ChStream 端口面延期到 `cpptlm-cep-stub-cache-chstream-surface`(design `D-ζ` 的四条互斥理由);本 change 只闭 **PE→网络** 段 + PE 输入侧类型契约。
- **6×6 状态机不做跨仓逐格镜像**:除 `state_transition[2][1] == 3` 外漂移无人发现 —— 缺口已在 `ADR-CEP-19` 决策层登记,本 change 不重复登记。
- **判据定义真源**:转正前真源在附属层调研文档(`docs/research/...review-2026-10-26.md §6.4`);本 change 的 delta spec 是**追认后**的契约层承载 ⇒ 归档并入 `openspec/specs/` 后即自足。
