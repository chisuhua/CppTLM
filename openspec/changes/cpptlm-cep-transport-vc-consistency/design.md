# design —— `cpptlm-cep-transport-vc-consistency`

> 本 design 只落地**已签发决策**;⛔ 不新增架构取舍(凡新取舍须回 architect 追加 SU)。
> 上游裁决全文 = `.rddf/reviews/cep-c2-vc-consistency-decision.md` **§1~§12**;台账 **Entry #344**(+ ⑨ K-1/K-2)。

---

## D-1 · 判据落点:扩展 `evaluate_shared_mode_certificate`,⛔ 不新增 `ContractRule`

**载体**:`include/tlm/cep/collective_plane_contract.hh` 的 `evaluate_shared_mode_certificate`(V6-d 段,约 `:880-1010`)。

| # | 约束 | 依据 |
|:--:|---|---|
| ① | 两条新判据作为 **`CP-03` 行的子判据** ⇒ **不新增** `ContractRule` | `kContractRuleCount == 6` 是**被测试硬断言**的契约面(现有注释 `:876` 逐字写明) |
| ② | 失败 `detail` 以**精确前缀**表达 ⇒ 折进 `CP-03` 行 | per `ADR-CEP-01` §E.3.1 的**归因 id 纪律**(`:873-875`) |
| ③ | ⛔ **不抽** `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决） | per `T-C2-①`:共用消费者实测 **0** 个(`configs/cep_*.json` 共 9 份,8 份恰 1 个 `protocol_instance` + `configs/cep_chi_rnf_e2e_a.json` **0** 个)⇒ 抽 header 即为不存在的场景预留 |
| ④ | ⛔ **不得**以「重跑 `evaluate_channel_dependency_gate`」当证书 | per `ADR-CEP-01` **E.4**:边按 network 声明 ⇒ 诱导子图恒无环 ⇒ 该闸对 `vc_binding` 语义**结构性恒真** |

**新归因 tag**(per K-2,**共用** `CP-03-VC-CERTIFICATE` 前缀 + `detail` 子标记,⛔ 不新造顶层 id):

| 判据 | tag | 规约锚点 |
|---|---|---|
| (b) 排序假设逐 VC 一致 | `[CP-03-VC-CERTIFICATE/R1-b] ` | `ADR-CEP-12` 末尾 `## Status Update` |
| (C-1′-S) scope 完备性 | `[CP-03-VC-CERTIFICATE/R1-c] ` | `ADR-CEP-01` 末尾 `## Status Update` |
| (a) 类集合一致性(既有) | `[CP-03-VC-CERTIFICATE/C-1′] ` **不动** | `ADR-CEP-01` §E.3.1 |

> ⚠️ **tag 尾部空格**是既有约定(`:884-888` 四个常量均以 `] ` 结尾)⇒ 新 tag **必须**同样带尾随空格,否则精确相等匹配会漏。

**归因不冲突(AC-14 的机器依据)**:既有 fixture 用 `rfind("[CP-03-VC-CERTIFICATE]", 0) == 0` 与 `rfind("[CP-03-VC-CERTIFICATE/C-1′]", 0) == 0` —— 两者第 22 位均为 `]`,而新 tag 第 22 位是 `/` ⇒ **结构上不可能互命中** ⇒ 但仍须加一条回归钉住(既有 fixture 不覆盖新 tag 的反向)。

---

## D-2 · 快照形态:新增**一个**字段,⛔ 不加 `plane_binding`

**现状**(`VcBindingSnapshot`,`:628-639`):

| 字段 | 形态 | 消费者 |
|---|---|---|
| `used` | network → VC 集合 | `evaluate_vc_reservation`(`:783`) |
| `vc_count` | network → vc_count | `evaluate_vc_reservation`(`:753`) / C-2(`:949`) |
| `per_network_mappings` | network → **vector\<map\<class,vc\>\>** | C-1′(`:910` / `:923`);⚠️ **不含实例 ID** |
| `source_error` / `source_error_detail` | — | 调用方(`:1430`) |

**新增字段**:

```cpp
/// network → **逐实例**的 `(instance_id, {vc(string) → ordering})`
/// ⚠️ **收集口径 = 该 network 上「有 `vc_binding`」的实例** ——「有」的定义 MUST **逐字复刻**
///   `per_network_mappings` 的 push 条件(`collective_plane_contract.hh:691-692` 的
///   `if (!mapping.empty())`)⇒ **该 network 的 `vc_binding` 至少含一个整数值**。
///   ⛔ **不是**「`vc_binding` 有该 network 键」—— 值全非整数的条目会被现有 loader 跳过,
///   两个口径分叉 ⇒ (a) 与 (b) 的实例集合不同 ⇒ L-1 空转 / AC-9 误报。
///   ⇒ **不以 `ordering_assumption` 为驱动** —— 否则「有 vc_binding 却缺 ordering_assumption」
///   的实例会被**静默排除**(per AC-9)。
///   ⇒ 某实例该 network 的 `ordering_assumption` 缺失时,其 entry 的 ordering 映射为**空**
///   (表示「未声明」,⛔ 不是「跳过该实例」)。
std::map<std::string,
         std::vector<std::pair<std::string, std::map<std::string, std::string>>>>
    per_network_orderings;
```

| # | 决策 | 理由 |
|:--:|---|---|
| ① | **只加这一个字段** | C-1′-S 需要的「实例 → networks」可由本字段**反查**得出(遍历各 network 的 vector,命中该 instance_id 即该 network 在其集合内)⇒ **不**再加第二个字段(避免两份真源) |
| ② | ⛔ **不加** `plane_binding` 进快照 | `decl.plane_binding` 已存在(`:181`)且已被 V6-c 消费(`:830`)⇒ 加进快照 = **双源死字段**(per metis P0-3) |
| ③ | 携带 `instance_id` | 缺它则红因无法指认「哪个实例」⇒ AC-4(删某实例某 VC)不可定位 |
| ④ | `ordering` 的 key 保持**字符串**形态 | 与 `include/tlm/cep/protocol_instances.json` 原始形态一致;数值化在**判据内**做(见 D-4)⇒ ⛔ 不在快照层做静默规范化(否则 AC-10 的「前导零 ⇒ FAIL」将无法检出) |

**⛔ 快照层不做的事**:不判任何一致性(判据在 `evaluate_shared_mode_certificate`);不吞异常(`ordering_assumption` 缺失 = 数据缺失,由判据判红,⛔ 不是快照层的 `source_error`)。

**loader 的缺失 / 畸形口径**(per metis P1-10 —— 逐条钉死,⛔ 不得静默):

| 情形 | 口径 |
|---|---|
| `ordering_assumption` **整体缺失** | 该 network 的 ordering 映射为**空** ⇒ 由 (b) 的完备性判红(归 `R1-b`)⇒ **不是** `source_error` |
| `ordering_assumption[network]` **非对象**(数组 / 字符串) | 按「**缺声明**」处理(同上一行)⇒ 红因文本须可区分「缺声明」与「值非法」 |
| 实例**缺 `id`** / `id` **非字符串** | ⛔ **不静默排除** ⇒ 置 `source_error`(配置错误)⇒ 否则 `instance_id` 无法定位 ⇒ 红因指认不到人 |
| `id` **重复** | 置 `source_error`(`vector` 会保留两条 ⇒ 报错无法指认) |
| VC key 数值**溢出** `unsigned` | `from_chars` 返回 `result_out_of_range` ⇒ 归「**越界 FAIL**」⇒ ⛔ 不得抛异常、⛔ 不得当作 0 |
| `vc_count` 该 network **未登记** | 由既有 C-2 判红并 return ⇒ (b) **不会**读到缺失的 `vc_count` |

---

## D-3 · (b) 判据形态:逐实例逐 VC **全等** + **每实例完备**

**作用域**(**条件化口径**,per `T-C2-⓪`):**仅** `certificate_scope` 内的 `(plane, network)` ⇒ `compute_noc` **不**被本判据触碰(它按 `ADR-CEP-01` §E.3.3 **不属 M-2**,允许不一致)。

**实例集合**(**AC-9 的硬约束**):**该 network 上所有有 `vc_binding` 的实例** —— 由 `per_network_orderings[network]` 直接给出(收集口径已按此对齐)⇒ ⛔ **不得**改以 `ordering_assumption` 为驱动。

**判定序列**(固定顺序,任一不过 ⇒ 立即 `return ContractVerdict::FAIL`):

| 步 | 检查 | 失败红因(归 `R1-b`) |
|:--:|---|---|
| b-1 | 该 network 的 `per_network_orderings` **非空** | 无实例 ⇒ 无主体 ⇒ FAIL(⛔ 禁 vacuous PASS)—— ⚠️ **结构性不可达**(C-1′ `:923-926` 已先判同一情形并 return)⇒ 保留**仅作防御**,⛔ **不得**被读作一条独立判据 |
| b-2 | 每个 entry 的 ordering **key 形态**合法(见 D-4) | 「key 非规范十进制」 |
| b-3 | 每个 entry 的 ordering **key 集合 == {0..vc_count-1}** | 「VC 完备性不足」(缺 ⇒ AC-4)—— ⚠️ **必须先判越界再判缺失**:b-2 的形态校验之后**立即**判 `数值 >= vc_count` ⇒ 报「**越界**」(AC-10③);**然后**才判集合齐全 ⇒ 否则 `"3"` → `"4"` 的变异会误报成「完备性不足」⇒ 归因不稳 |
| b-4 | 不同 entry 之间,**同一 vc 的 ordering 值全等** | 「同 VC 排序假设不一致」(⇒ AC-3) |
| b-5 | ordering **值域**合法(`p2p_ordered` \| `unordered`) | 「值域非法」—— ⛔ **本仓既有闸已挡**(`scripts/test/cep_decl_alignment_check.sh` 用 `jsonschema` 全量校验 `include/tlm/cep/protocol_instance.schema.json`,其 `additionalProperties.enum` 已锁)⇒ **判据不重复实现值域**(禁双实现)⇒ **登记为已知残余**(见 D-8) |

**⛔ (b) 不做的事**:不判「该 VC 承载哪几类消息」(那是 (a) 的 C-1′ 职责);不判 `vc_binding` 与 `ordering_assumption` 的**键对称性**(既有 `scripts/test/cep_decl_alignment_check.sh` ③ 已判:键不对称 ⇒ 红);不引入「排序假设的正确性」判断 —— (b) 只证**跨实例一致**,⛔ 不证「逐类正确」(见 D-7 的 L-2)。

**⚠️ 已知表征局限 L-2(⛔ 不是缺陷)**:同一 VC 可同时承载多类消息(`ualink_fabric` 的 vc3 同时承载 `response` + `snoop_resp`)⇒ `ordering_assumption[vc]` 是**每 VC 一个值**,无法表达「`response` 需 ordered 而 `snoop_resp` 需 unordered」⇒ (b) 的绿**只**证明跨实例一致,⛔ 不得引申为「排序语义正确」。

---

## D-4 · VC key 的规范化策略(**AC-10 三类**)

`ordering_assumption[network]` 的 key 在 JSON 里是**字符串数字**(`"0"`),而 `vc_count` 是整数 ⇒ **显式转换**,⛔ 不得依赖隐式转换或 `operator[]` 的静默插入。

| # | 输入形态 | 策略 | 红因 |
|:--:|---|---|---|
| ① | **非数字**(`"a"` / `""` / `" 4"` / `"-1"`) | **FAIL** | 「key 不可解析为十进制整数」 |
| ② | **前导零**(`"04"`) | **FAIL**(规范化拒绝) | 「key 非规范十进制(前导零)」—— 理由:`"04"` 与 `"4"` 在 JSON 里是**不同 key**,若静默接受 ⇒ 与 `"4"` 语义分裂,且 `minProperties: 1` 的 schema **挡不住**(`propertyNames.pattern = ^[0-9]+$` 允许前导零) |
| ③ | **数值 >= `vc_count`** | **FAIL** | 「vc 越界」 |

**实现约束**:

- ⛔ **不得**用 `std::stoul` 单独做形态校验(`stoul(" 4")` = 4、`stoul("-1")` 抛异常、`stoul("04")` = 4 且静默接受前导零)⇒ **先做字符串形态校验,再转换**。
- 形态校验:key **非空** ∧ **全为 ASCII 数字** ∧ (长度 == 1 ⇒ 首位可为 `'0'`;长度 > 1 ⇒ **首位不得为 `'0'**)。
- 转换:`std::from_chars`(C++17,<charconv>),⛔ 不用 `stoul`(异常 + 隐式接受)。
- 越界判定用**转换后的数值** vs `snap.vc_count[network]` ⇒ 数值比较,⛔ 不是字符串比较。

---

## D-5 · (C-1′-S) 判据形态:`(plane, network)` pair ⊆ `certificate_scope`

**作用域**:对 `decl.plane_binding` 中**每一个**实例(⛔ 不用 `per_network_mappings` 的实例集合 —— 那是「有 vc_binding 的实例」,而本判据问的是「**声明了 plane 绑定**的实例」)。

**判定序列**(固定顺序 —— 归因稳定性是硬要求,per metis 边界):

| 步 | 检查 | 处理 |
|:--:|---|---|
| s-1 | 该实例的 `plane_binding` 值为**空串**(loader 把显式 `null` 物化成空串,`:235`) | **跳过** —— 该实例**不启用** collective(与 V6-c `:831-832` 同型) |
| s-2 | 该 plane **已登记**于 plane 注册表(`find_collective_plane`) | 未登记 ⇒ **FAIL**,红因「plane 未登记」<br>⚠️ **诚实差异**:`V6-c`(`:830`)只在 `status == "unavailable"` 时判这一情形;M-2 模式(`shared_no_isolation`)下 V6-c 为 **SKIP** ⇒ 若本判据也跳过,「plane 未登记」在 M-2 下**无任何载体** ⇒ 本判据**自带**这半步(否则是覆盖洞) |
| s-3 | 该 plane **不是本地模型**(`is_local_model == false`) | **跳过** —— 本地模型不承载真实 fabric 集合流量(与 C-3 `:977` 同型语义)⇒ `nvlink_4 → LocalCollectivePlane` **不**触发本判据 |
| s-4 | 该实例**有** `vc_binding`(即在 `per_network_orderings` 的某个 network 下能查到该 instance_id) | 无 ⇒ **FAIL**,红因「实例无任何 VC 绑定 ⇒ M-2 无主体」(AC-11;对称 C-1′ 的「无实例 ⇒ FAIL」) |
| s-5 | 该实例 `vc_binding` 的**每个** network N ⇒ `(plane, N)` ∈ `certificate_scope` | 不在 ⇒ **FAIL**,红因「`(plane, network)` 对不在 `certificate_scope` 内」(⇒ **AC-5**) |

**⛔ 归因区分(AC-5 的硬要求)**:s-5 的红因**必须**是「`(plane, network)` 不在 `certificate_scope` 内」;⛔ **不是**「plane 未登记」(那是 s-2)⇒ 两者是**不同步**、不同红因文本 ⇒ 归因可区分。

**⛔ 方向不可颠倒**:本判据是「**声明 ⇒ scope 完备性**」方向(**声明了要在某 plane 上跑 ⇒ 该 plane 的相关 network 必须在 scope 内**),⛔ **不是**「scope 项 ⇒ 声明支持」方向 ⇒ 前者能检出「scope 漏登记」,后者不能。

**落点**:scope 完备性是**全局/声明级**性质(遍历 `decl.plane_binding`,⛔ 不是遍历 scope 项)⇒ 判据置于 V6-d 的 **scope 项循环之后、C-4 之前**(与同为声明级的 C-4 同级)。

> ⚠️ **⛔ 为什么不是「循环之前」**(per metis P0-1 的**实测**证据 —— 本条是**修订**,原 design 写「循环前」):
> 既有 C-3 的两条反向 fixture(`test/test_cep_ualink_inc.cc:501-520`)采用「**改写** `certificate_scope[0].plane`」的变异形态
> ⇒ 若 R1-c 在循环前,它会**先于** C-3 判红(C-1′-S 的 membership 因 plane 被改写而不成立)⇒ 既有断言
> `rfind("[CP-03-VC-CERTIFICATE/C-3]", 0) == 0` **必失败** ⇒ **既有 fixture 被抢归因**。
> ⇒ 「循环后」使 **既有 6 条反向 fixture 零改动**且归因优先级为 `C-1′ → C-2 → R1-b → C-3 → R1-c → C-4`。
> ✅ **代价登记**:R1-c 置于循环后 ⇒ 若 scope 漏登记**同时**循环内有别的失败,**先报循环内的失败**(first-failure-wins)⇒ scope 缺口**可能被遮蔽** ⇒ 见 **D-7 的 L-6**。

**空 `plane_binding` 的口径**:零键 ⇒ C-1′-S **无主体** ⇒ **vacuous PASS**(与 V6-c 的「零键 ⇒ SKIP」**语义不同** —— V6-c 是「不适用」,C-1′-S 是「无缺口可查」)⇒ ⛔ 不得把二者混为一谈 ⇒ 登记进诚实边界段(per D-7 **L-7**)。

---

## D-6 · V6-d 段的判定顺序(全局,归因稳定性的骨架)

| 序 | 判据 | 位置 | 归因 |
|:--:|---|---|---|
| 0 | `status` / `certificate_scope` 非空 等既有前置 | `:891-909` | `[CP-03-VC-CERTIFICATE]` |
| 1 | C-1′ 结构面(`collective_plane_vc_binding` 块) | 循环内 · 首 | `[CP-03-VC-CERTIFICATE/C-1′]` |
| 2 | C-1′ 语义面(逐类相等) | 循环内 | `[CP-03-VC-CERTIFICATE/C-1′]` |
| 3 | C-2 VC 覆盖完整 | 循环内 | `[CP-09-IC1]` |
| **4** | **(b) 排序假设逐 VC 一致 + 完备** | 循环内 · C-2 之后 | `[CP-03-VC-CERTIFICATE/R1-b]` |
| 5 | C-3 控制面与数据面互斥(声明级) | 循环内 · 末 | `[CP-03-VC-CERTIFICATE/C-3]` |
| **6** | **(C-1′-S) scope 完备性** | **循环后 · C-4 之前** | `[CP-03-VC-CERTIFICATE/R1-c]` |
| 7 | C-4 未检验项显式登记 | 循环后 · 末 | `[CP-03-VC-CERTIFICATE/C-4]` |

> ⚠️ **R1-c 放在循环后而非循环前** —— 决定性依据见 **D-5 的修订说明**(既有 C-3 fixture 的归因抢占)。
> ⚠️ **R1-b 放在 C-2 之后** —— (b) 的完备性基准 = `snap.vc_count[network]` ⇒ 需先由 C-2 保证该 network 已在 `include/tlm/cep/fabric.json` 登记。
> ⚠️ **R1-b 放在 C-1′ 之后** —— AC-1 的变异(改**消息类**)必须红在 C-1′;AC-3 的变异(改**ordering**)不触碰 `vc_binding` ⇒ C-1′ 仍绿 ⇒ 落到 R1-b ⇒ 两个归因互不遮蔽。

**为什么 (b) 在 C-2 之后**:(b) 的完备性基准 = `snap.vc_count[network]` ⇒ 需先由 C-2 保证该 network **已在 `include/tlm/cep/fabric.json` 登记**(`:950-953` 已判 ⇒ FAIL 并 return)⇒ 到 (b) 时 `vc_count` 必然存在且继承映射已全覆盖。

**为什么 (b) 在 C-1′ 之后**:AC-1 的变异(改**消息类**)⇒ 必须红在 **C-1′** ⇒ C-1′ 必先;而 AC-3 的变异(改**ordering**)不触碰 `vc_binding` ⇒ C-1′ 仍绿 ⇒ 落到 (b) 红 ⇒ 两个归因互不遮蔽。

**为什么 C-1′-S 在循环后**:见 **D-5 的修订说明** —— 「循环前」会抢既有 C-3 fixture 的归因(实测);「循环后」使既有 6 条反向 fixture **零改动**。代价(first-failure-wins ⇒ scope 缺口可能被遮蔽)登记为 **L-6**。

---

## D-7 · 判据的**边界**(⛔ 不得越界声称)

| # | 局限 | 表述纪律 |
|:--:|---|---|
| **L-1** | **单实例 network 上空转**:某 network 若只有 1 个有 `vc_binding` 的实例 ⇒ (a)(b) 的「全等」天然成立 ⇒ **恒绿、无鉴别力** | 当前 `ualink_fabric` 有 **3** 个实例(不空转)⇒ 登记为**表征局限**,⛔ 不得当作缺陷 |
| **L-2** | (b) 只证**跨实例一致**,不证**逐类正确**(见 D-3) | ⛔ 不得把 (b) 的绿引申为「排序语义正确」 |
| **L-3** | **`ordering_assumption` 值域非法**不在判据内(既有 schema 闸已挡,D-3 b-5) | 判据的 FAIL **只**覆盖形态 / 完备 / 一致三类;⛔ 不得声称「本判据验证了 ordering 取值合法」 |
| **L-4** | **`plane_binding` 自身漏登记**(实例实际在用 fabric 承载 plane,但 `plane_binding` 为 `null`)⇒ C-1′-S **漏判** | `plane_binding` 是**自声明数据**,无独立真源 ⇒ 不可判 ⇒ **登记**(per K-1,落 `docs/architecture/cep/fabric-transport.md` 诚实边界段) |
| **L-5** | **「不一致的 network 确实走了 M-1」不可判** | 本仓无 M-1 配置载体 ⇒ **登记**(per K-1,同上) |
| **L-6** | **R1-c 置于循环后 ⇒ scope 缺口可能被遮蔽**:若 scope 漏登记**同时**循环内有别的失败 ⇒ first-failure-wins ⇒ 先报循环内的失败,scope 缺口**不在同一份 detail 里** | ⛔ 不得声称「一次失败即报出全部问题」⇒ 登记 |
| **L-7** | **空 `plane_binding` ⇒ R1-c vacuous PASS**(零键 ⇒ 无主体) | 与 V6-c 的「零键 ⇒ SKIP」**语义不同**(前者 = 无缺口可查,后者 = 不适用)⇒ ⛔ 不得混为一谈 ⇒ 登记 |

> ⚠️ **L-4 / L-5 ⛔ 无机器守门**:`U-1` / `U-2` 有 C-4 的机器守门(「未被冒充成已满足」);L-4 / L-5 是**文档级登记**,⛔ 不得让读者以为它们也有机器守门 ⇒ **文档须显式写明该地位差异**(per AC-17)。

---

## D-8 · 文档同步与冻结面(3 处产物)

| # | 产物 | 落点 | 改动 |
|:--:|---|---|---|
| 1 | `docs/architecture/cep/fabric-transport.md` | R1 段(`:70-113`)+ R5 表(`:136-145`)+ 「诚实边界」段(`:106-112`) | R5 表「当前机器判定」列逐条更新(R1 不再写「(b) 无载体」/「`compute_noc` 无判定」);R1 段 scope 表述订正(**`compute_noc` 按裁决不属 M-2**,非「未覆盖」);**写明 `R1-a ≡ C-1′` 映射**;`:145` 的 **`R1-c` 悬空引用**处置(它**已由本 change 定义** ⇒ 改为「已由 `R1-c` 判据覆盖」);**L-4 / L-5 登记进诚实边界段** + 地位差异说明 |
| 2 | `docs/adr/ADR-CEP-01-fabric-collective-plane.md` | **末尾追加** `## Status Update` | C-1′-S 的规约锚点:`E.3.3` 的适用域由 `certificate_scope` 声明,新增 `R1-c` 校验其完备性 —— 对每个 `plane_binding` 指向 `is_local_model == false` 的 plane 的实例,其 `vc_binding` 中每个 `(plane, network)` 对必须在 `certificate_scope` 内。⛔ **正文不改** |
| 3 | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | **末尾追加** `## Status Update` | (b) 的规约锚点:决策 5 所称的「装配期校验」由 `R1-b` 承担;其作用域 = 声明 M-2 的 network(per `ADR-CEP-01 §E.3.3` 的条件化口径);⚠️ `INV-CEP-12-IC2` 只保证 key 可解析,**不承担**跨实例一致性。⛔ **正文不改** |

**冻结面五步链**(3 处产物,**同一次** `--update` 内重算):

```
① 台账 Entry 授权(本 change 立项 Entry)
② 产物 commit(3 个文件 + 源码 + 测试,一次 commit)
③ bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #XXX"
④ 清单 commit(docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json)
⑤ 无参复验:bash scripts/test/cep_freeze_check.sh   ⇒ rc=0
```

> ⚠️ **产物 commit 必须先于 `--update`**(C1 已验证的顺序,per 台账 Entry #342)。
> ⚠️ `artifact_count` **不新增**(3 处产物**已在**冻结集内 ⇒ 只改 hash)。
> ⚠️ 归档时若合并 delta ⇒ `openspec/specs/cep-collective-plane-contract/spec.md` **在**冻结集(`openspec/specs/cep-*/spec.md` glob)⇒ **另行触发**一次五步链。

---

## D-9 · 声明数据零改动(机器守门)

| # | 文件 | 纪律 |
|:--:|---|---|
| 1 | `include/tlm/cep/protocol_instances.json` | **零 diff** —— `tilelink_c` 的 `compute_noc` 映射差异是**合法协议差异**(per `U-3`)⛔ 不「修」 |
| 2 | `include/tlm/cep/collective_planes.json` | **零 diff** —— `certificate_scope` / `plane_binding` 一字不改 |
| 3 | `include/tlm/cep/fabric.json` | **零 diff** —— 只读取 `vc_count` |
| 4 | `include/tlm/cep/protocol_instance.schema.json` | **零 diff** —— 其 `:180` 的 description 是决策 5 的转录;澄清在 ADR SU 里做(per K-2) |

⇒ AC-13 以 `git diff --exit-code -- <上述 4 文件>` 机器守门。

---

## D-10 · 测试形态与「旧绿 / 新红」并排取证

**载体**:`test/test_cep_ualink_inc.cc`(既有 V6-d 四证书的载体面,`[cep][ualink-inc][certificate]`)。

**手法**:既有 fixture 的形态是「复制快照 / 声明 ⇒ 毒化 ⇒ 断言必红 + 精确前缀匹配」(`:469-536`)。本 change 的变异**沿用同一形态**:

| 变异 | 毒化对象 | 期望 |
|---|---|---|
| AC-1 | 快照的 `per_network_mappings["ualink_fabric"]` | FAIL,前缀 `[CP-03-VC-CERTIFICATE/C-1′]` |
| AC-3 | 快照的 `per_network_orderings["ualink_fabric"]` 某 entry 的某 vc 值 | FAIL,前缀 `[CP-03-VC-CERTIFICATE/R1-b]` |
| AC-4 | 同上,删某 entry 的某 vc | FAIL,前缀 `R1-b` |
| AC-5 | `decl.plane_binding` 插键(某 `compute_noc` 实例 → `FabricCollectivePlane`) | FAIL,前缀 `[CP-03-VC-CERTIFICATE/R1-c]`,且红因文本含「不在 `certificate_scope`」 |
| AC-10①②③ | `per_network_orderings` 的 key 形态 | FAIL,前缀 `R1-b`,三类各一条 |
| AC-11 | 令绑定 `FabricCollectivePlane` 的实例在 `per_network_orderings` 中**查不到** | FAIL,前缀 `R1-c` |
| AC-14 | — | 新 tag 不被既有两条 `rfind` 命中 + 既有 fixture 仍绿 |

**「旧绿 / 新红」并排取证(AC-3 / AC-12)**:

| AC | 旧实现为何必绿 | 取证方式 |
|---|---|---|
| AC-3 | 旧实现**完全不读** `ordering_assumption`(`grep -c ordering_assumption include/tlm/cep/collective_plane_contract.hh` = **0**)⇒ 同一份毒化**输入**在旧实现下 PASS | 见下方「版本无关探针」 |
| AC-12 | 旧实现**无 `R1-c` 判据** ⇒ AC-5 的同一变异在旧实现下 PASS(且旧实现连 `plane_binding` 都不读 ⇒ 不可能红) | 同上,并排 |

> ⚠️ **⛔ 探针必须「版本无关」**(per metis P0-2 的**实测**约束):旧 HEAD 的 `VcBindingSnapshot` **没有** `per_network_orderings` 字段
> ⇒ **不可能**把新结构体交给旧实现 ⇒ 「同一份毒化**快照**」在跨实现场景下**物理不可实现**。
> ⇒ 正确形态 = **同一份毒化输入文件**(影子 `include/tlm/cep/protocol_instances.json`)+ **同一份不引用新字段的探针源码**:
> **探针源码只允许**调用 `load_vc_binding_snapshot(影子 protocol_instances, 真实 fabric)` + `evaluate_shared_mode_certificate`
> ⇒ ⛔ **不得**引用 `per_network_orderings`(否则旧 header 编译不过 ⇒ 取证退化为同实现内自证)。
> ⇒ 旧/新分别编译(`g++ -std=c++17 -I<worktree>/include -I<worktree>/external/json/include probe.cc`),
> **命令 + 输入影子文件 + 两端输出**三者落 `.rddf/reviews/<name>-oldnew-evidence.md` ⇒ ⛔ 不得只写一句「旧实现必绿」。

> ⚠️ **AC-12 的「旧绿」有一个诚实边界**:旧实现读 `decl.plane_binding`?**不读** ⇒ 它既不因「plane 未登记」红、也不因「不在 scope」红 ⇒ 旧绿是**平凡**的。取证时**必须**如实写明这一点(⛔ 不得把它包装成「旧实现也检查了但更宽松」)。

**AC-15(真实 gate 路径可达)**:变异**必须**经由 `run_collective_plane_contract`(约 `:1447` 的调用链)触发,⛔ 不得只在测试里手搓 `decl` 直调 `evaluate_shared_mode_certificate` ⇒ ⇒ **两套都要**:① 直调层(逐条归因的精细断言)② 真实 gate 层(可达性 + 折进 `CP-03` 行)。

> ⚠️ **real gate 层的断言形态**(per metis P0-7 的**实测**约束):CP-03 行的 `detail` 是**拼接**的
> (`include/tlm/cep/collective_plane_contract.hh:1444-1453`:`d += " | " + cert`)⇒ 新前缀**不在位置 0**
> ⇒ ⛔ **不得**对 CP-03 行用 `rfind(prefix, 0) == 0`(那是**直调层**的写法)。
> ⇒ real gate 层必须:**遍历 `report.lines`** ⇒ 找到 `line.rule == ContractRule::VcReservation` 的行
> ⇒ 断言 `line.detail.find("[CP-03-VC-CERTIFICATE/R1-b] ") != npos`(用 `find`,⛔ 不用 `rfind(...,0)`)
> ⇒ 并断言 `report.lines.size() == kContractRuleCount`(**锁 6 条规则**,⛔ 未新增第 7 条)。
> ⇒ **影子文件纪律**:real gate 的变异必须通过 `paths.protocol_instances` / `paths.declaration` 指向**临时影子文件**
> ⇒ ⛔ **不得**修改仓内 JSON(那是「改声明数据变绿」);baseline 正控继续用仓内真实路径。

---

## D-11 · 风险与对策

| # | 风险 | 对策 |
|:--:|---|---|
| R-a | (b) 的实例集合若以 `ordering_assumption` 为驱动 ⇒ 缺 `ordering_assumption[network]` 的实例被**静默当子集排除** | AC-9 硬钉 + D-2 的收集口径 + D-3 的集合定义(三处同源) |
| R-b | `ordering_assumption` key 是字符串数字 ⇒ 与 `vc_count` 整数比较有类型陷阱 | AC-10 三类 + D-4 的「先形态校验再转换」 |
| R-c | tag 前缀误命中 | AC-14 回归 + D-1 的「尾部空格」约定 |
| R-d | 判据自证(测试里手搓数据 ⇒ 实现与 fixture 同源) | AC-15 的真实 gate 层 + 「旧绿/新红」并排(跨实现,非跨 fixture) |
| R-e | 冻结链 3 处产物 ⇒ hash 变更面更大 | 三处同一次 `--update` 重算 + 产物 commit 先于 `--update` |
| R-f | 快照字段加错 ⇒ (b) 恒绿(vacuous) | AC-2 基线 + **快照正控断言**(真实数据下 `per_network_orderings["ualink_fabric"].size() == 3` 且每条 key 集合 == `{0,1,2,3}`)+ AC-3/AC-4 双变异(值变 / 条目缺)⇒ 任一恒绿即实现错 |
| R-g | **归因抢占**:新判据先于既有判据触发 ⇒ 既有 6 条反向 fixture 的**归因前缀**被改写 ⇒ 既有测试失败或(更糟)**断言写错而仍绿** | AC-1 / AC-14 扩展为「**既有 6 条反向 fixture 的归因前缀逐一不变**」;R1-c 置于循环**后**(per **D-5** 的实测修订) |
| R-h | **(b) 的实例集合与 (a) 分叉** ⇒ 两维度各判一组不同实例 | D-2 ② 的口径**逐字复刻** `per_network_mappings` 的 push 条件 |

---

## 铁律

1. ⛔ **零 ADR 正文改动** —— 只在末尾追加 `## Status Update`(per `ADR-CEP-00` 的「ADR 编辑只允许两种形态」)。
2. ⛔ **零声明数据改动** —— 4 个 JSON/schema 文件零 diff(AC-13 机器守门)。
3. ⛔ **不新增 `ContractRule`** —— `kContractRuleCount == 6` 是硬断言。
4. ⛔ **不新造顶层归因 id** —— 只加 `detail` 子标记,共用 `CP-03-VC-CERTIFICATE` 前缀。
5. ⛔ **不借 `C-1′` 之名** —— 新 tag 用 `R1-b` / `R1-c`(per K-2 的选项 C 否决)。
6. ⛔ **不在装配闸另造第二实现** —— 判据只在 `include/tlm/cep/collective_plane_contract.hh`(per `T-C2-①` + BSS-01 先例)。
7. ⛔ **不得靠「文件消失」才红** —— 每条新判据的变异都是**数据变异**(内容改坏),不是文件删掉。
8. ⛔ **不得把 (b) 的绿引申为「排序语义正确」** —— (b) 只证跨实例一致(per L-2)。
9. ⛔ **不得宣称 L-4 / L-5 有机器守门** —— 它们是文档级登记。
10. ⛔ **产物 commit 必须先于 `--update`** —— 冻结链五步的硬顺序。
