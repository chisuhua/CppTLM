# design —— `ualink_4` 实例级装配

## D-1 三层证据(判据的形状)

| 层 | 证据 | 缺失 ⇒ |
|---|---|---|
| ① 声明层 | `protocol_instances.json` 的 `instances[]` 有该 id + `family` 可解析 | `[IAG-02]` / `[IAG-03]` |
| ② 拓扑层 | 配置 `cep.bridges[].protocol_instance` 命中 + 该 bridge 的 `modules` **确实在拓扑中** + `bridge_id` 唯一 | `[IAG-06]` / `[IAG-07]` |
| ③ 平面层 | `collective_planes.json` 的 `plane_binding[instance]` **非空** 且 plane 名**已登记** | `[IAG-04]` / `[IAG-05]` |

⚠️ **判据只对「配置中挂载了的实例」提要求** ⇒ 不会把「未挂载的其它实例」误判红(`nvlink_4` 有 plane 绑定但无拓扑 ⇒ 不在本闸职责内,登记为**独立遗留**)。
⚠️ **零 bridge ⇒ `[IAG-01]`** ⇒ 「拓扑里一个协议实例都没挂」也判红(装配的**下限**)。

## D-2 为什么 `ualink_4` 绑 `FabricCollectivePlane` 而非 `LocalCollectivePlane`

- `FabricCollectivePlane` = **承载真实 fabric 集合流量**(`is_local_model = false`)⇒ 数据路径真可达;
- ⚠️ 代价:M-1 变体(`status = unavailable`)下 V6-c 规则「不得存在真实 fabric 集合流量载体」⇒ **判红** —— 这不是 bug,而是**规则有牙**;
  ⇒ 故 M-1 **人造夹具**必须先移除该载体(测试已改),并**新增反向断言**(M-1 + 真实载体 ⇒ FAIL)保留其鉴别力。

## D-3 承载模块的选择(为何用 `CohIfProtocolEngine`)

`UalinkAdapter` 只有静态能力谓词(证书 / 未检验项 / M-2 判定),**无 flow 分派**、**不是** `ChStreamModuleBase` ⇒ 不能承载 bridge。`CohIfProtocolEngine`(`CohIfNullProtocolEngine`)是**已注册**的 `ProtocolEngineBase` 派生 ⇒ 可被 `attach_protocol_bridge` 绑定 ⇒ 用它承载「拓扑挂载」这一层。

⚠️ **诚实边界**:`CohIfNullProtocolEngine` 是 **Null(零发射)**实现 ⇒ 本 change 闭合的是「**装配**」,**不是**「流语义」;`ualink_4` 的 `request`/`completion` 需要**专用引擎** ⇒ 登记为**后续 change**(⛔ 本 change 不假装已实现)。

## D-4 冻结面三处的分工

- `07 号` §5.1.1 UALink 行:「⚠️ 声明 ≠ 装配:实例级装配仍待 backlog」⇒ 改为「✅ 装配已闭合(但流语义未闭环)」;
- `ADR-CEP-12` SU:记录装配闸与三层证据(append-only);
- `ADR-CEP-04` SU:记录 `ualink_4` 的 plane 绑定与拓扑挂载(append-only)。
