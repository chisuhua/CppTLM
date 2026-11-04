# design — cpptlm-cep-ualink-inc-state-spec(规约层)

> **口径**:本 design 是**规约层**的实施指导(how);**当前态**与**架构意图**在 `docs/designs/` 与
> `docs/adr/ADR-CEP-0{1,3,4,9,11}`(真源 3)·**不重推语义**,只把裁决**机器化**。
> **唯一真源链**:`ADR-CEP-01` SU#4(路径 D + 四条证书)→ `ADR-CEP-03`(原语集 / 跨平面契约)
> → `ADR-CEP-04` SU(tracker 选型)→ 本层载体 → 判据。

---

## §1 交付面与载体形态决定

### §1.1 复用的既有裁决(**不重开**)

| # | 裁决 | 登记处 |
|---|---|---|
| ① | **VC 路径 = M-2**(共享 VC + 四条证书);M-1(隔离)在本拓扑**不可用**(VC 饱和) | `ADR-CEP-01` SU#4 的 E.2 |
| ② | **原语最小集 = 恰 4**;`AllReduce` = REDUCE + BROADCAST 的**复合**,⛔ **不是**第 5 原语 | `ADR-CEP-01:94` / `ADR-CEP-03:75` |
| ③ | **reduce 算子 = 复用既有 `ReduceOp`(恰 6 值)**;载体**不**定义算子族 | `ADR-CEP-03` SU(#2) F.1-b |
| ④ | **partial failure** ⇒ `CollectiveEvt::Fault`(不得静默) | `ADR-CEP-01` §7 V5 |
| ⑤ | **tracker 选型 = 候选集(7 值)+ 逐实例理由登记**;⛔ **无** `flow_support` 派生规则 | `ADR-CEP-04` SU 2026-11-03 |
| ⑥ | 集合平面**继承**该 network 既有实例的 VC 空间(**不声明**自己的绑定) | `ADR-CEP-01` SU#4 的 E.3.2 |

### §1.2 本层**新**定的 4 项形态决定

| # | 决定项 | 结论 | 依据 |
|---|---|---|---|
| **N-1** | 载体的**自足性** | **只 include 标准库**;⛔ 不引用 `collective_plane.hh` / `sharer_tracker.hh` 等平台头 | 队列 #13 规约层载体的先例(自足 ⇒ 不受平台表变动影响 ⇒ basis 不会因实现漂移而失效) |
| **N-2** | 「不新增原语」如何**可判定** | 载体**自持** INC 四原语 id + 它们到 `REDUCE`/`BROADCAST`/`MULTICAST` 的**映射**(按**名字**);**跨载体一致性**由**元测试**断言(载体名集 **⊆** `tlm::cep::CollectiveOp` 的取值集 ⇒ 覆盖;**⊊** ⇒ 非副本)⇒ 载体仍自足,判据仍可执行 | 载体自足 + 判据可执行**两者兼得**的唯一形态(在载体里 include 平台头会破自足;只写散文会不可判) |
| **N-3** | **9 项 pending** 的载体形态 | 每项 pending 是一个**具名条目**,带 `ref`(= 锚点摘要的 `N-n` id)+ **禁推声明**;载体**不得**含任何 `pending = false` 的「由公开来源支撑」格(公开来源撑不住 ⇒ 全部 `self-derived` 或 `pending`) | 锚点摘要 §3 · 队列 #13 规约层的「无 `ref` 的 pending 即红」先例 |
| **N-4** | **证书规约面**的位置 | 载体**内**给出四条证书的**逐边依据 + 参照域**;⛔ **不**在载体里实现判据(判据属 `cep-collective-plane-contract`,由**实现层**的 delta 承载)⇒ 本层只提供**规约面** | `ADR-CEP-01` SU#4 的 E.3.1(判据面在契约头)· 队列 #13 的「载体 ≠ 判据」分层 |

---

## §2 载体形态(`include/tlm/cep/ualink_inc_state_spec.hh`)

### §2.1 ⭐ 自足性(禁项清单)

本文件的**去注释后**文本 SHALL **不**含:

| 类别 | 禁项 |
|---|---|
| **平台标识** | `CollectiveOp` · `ReduceOp` · `CollectiveEvt` · `Status::` · `LookupResult` · `TrackerImpl` · `CollectivePlaneIf` · `LocalCollectivePlane` · `kCollectivePlaneRegistry` · `tlm::cep::` |
| **项目内头** | 任何 `#include "tlm/..."` / `#include "cep/..."` ⇒ **只允许** `#include <...>` |
| **区域表** | `collective_region_table` |
| **转移判定** | `transition_*` · `is_silent_upgrade` |

⇒ 语法上**不可能**携带平台状态机 / 投影表 / basis 表 ⇒ 只能承载**本协议自身的**语义与 basis。

⚠️ **载体自建的教训**(per 队列 #14 Phase 1 自查,已固化于此):禁项清单里的**短 token**
会被**载体自身的标识符**命中 —— 本轮首跑即命中 `kReduceOperatorFamilyDefinedInCarrier`
含子串 `ReduceOp` ⇒ 已改名 `kOperatorFamilyDefinedInCarrier`(**保留扫描的严格性**,
不做词边界放宽)⇒ **下一个载体**新增标识符时,须把**自己的**标识符也纳入禁项清单复核。

### §2.2 INC 四原语(自持 id + 映射)

| INC 原语 | 映射到的既有原语 | 性质 |
|---|---|---|
| `ReadReduce` | `REDUCE` | 一原语 |
| `WriteMulticast` | `MULTICAST` | 一原语 |
| `Broadcast` | `BROADCAST` | 一原语 |
| `AllReduce` | `REDUCE` + `BROADCAST` | **复合**(⛔ 不是第 5 原语) |

⇒ **覆盖集 = {REDUCE, BROADCAST, MULTICAST}** ⊆ `CollectiveOp` 的 4 原语 ⇒ **不新增原语**
(跨载体一致性由 §2.4 的元测试断言)。

### §2.3 逐格 `basis` + 9 项 `pending`

- 每格 `basis` 必有 `ref`(指向锚点摘要的 `A-n` / `C-n` / `N-n` id)⇒ **无 `ref` 的格即红**。
- 每格 `basis` 有 `kind ∈ {self-derived, pending}`;**`kind` ↔ `ref` 类别 SHALL 自洽**
  (per critic 第 1 轮 **S1** 的修订 —— 原设计只有散文约束、**无任何可执行断言**):
  - **`ref` token 按前缀分类**:`C-` = 仓内已签锚点 · `N-` = pending 白名单 · `A-` = 公开来源
    (其余 = 未知类 ⇒ 一律判否);⛔ **不用子串**(短 token 会误伤);
  - `self-derived` ⇒ **全部** token 必须是 `C-` 类;
  - `pending` ⇒ **至少一个** `N-` 类 token(语义待定的依据)· **不得**含 `A-` 类
    (公开来源不得冒充)· 其余 token **只允许** `C-` 类(名字来源的**附加**依据)
    ⇒ **未知类 token 一律判否**;
  - ⇒ **三种冒充全部判否**:`self-derived` 却引 `N-` / `pending` 却**纯**引 `C-`
    (无 `N-` token ⇒ 拿已定稿冒充待定)/ `pending` 却引 `A-`;
  - ⚠️ ⛔ **不得**把结论写成「`pending` 却引 `C-` ⇒ 判否」—— 那**强于**实现
    (INC 原语格 = `Pending` + `"C-6 N-1"`,`C-` 作名字来源的附加依据)
    ⇒ Phase 2 的 fixture 会**假绿** ⇒ 以**载体头注释 + 函数 doc 注释**为准;
  - ⇒ 载体提供**遍历式**判定(`all_obligations_kind_match_ref_class()` /
    `all_inc_op_basis_kind_match_ref_class()`)—— ⛔ **只断言字面量组合是不够的**
    (毒化表格里的 `ref` 不会触发字面量断言 ⇒ 判据无牙;per Phase 1 实测)。
- **9 项 pending**(`N-1` ~ `N-9`,与锚点摘要 §3 **逐项对应**)⇒ 载体 SHALL 能**逐项**报出其 id 与 `ref`。
  ⚠️ **「恰 9 项 + 与锚点摘要逐项相等」不由载体硬编**(per critic 第 1 轮 **S3** 的修订):
  - 载体只保证**自洽型**(`表大小 == kPendingCount`)+ **id 唯一且落在 `N1`~`N9` 内**
    + **id ↔ ref 一一对应**;
  - ⛔ **不**要求「id == 数组下标」—— 那会把「一次语义转正」强制变成「一次审计键重写」
    ⇒ **可逆性**:转正一项时其余 id **保持稳定**;
  - 「恰 9 项」的义务归 **X-2 元测试**(Phase 2 · **可执行**:读锚点摘要解析 `N-n` 集合)+ 人工审计面。

### §2.4 跨载体一致性的**元测试**形态

| 断言 | 内容 |
|---|---|
| **X-1 原语覆盖** | 载体的 INC 四原语的**覆盖集** **⊆** `tlm::cep::CollectiveOp` 的取值集(⇒ UIS-01 的「不新增原语」成立);且 `AllReduce` 的映射**恰为 2 个**(REDUCE + BROADCAST)⇒ **不是**第 5 原语 |
| **X-1b 非副本** | 覆盖集 **⊊** 平台取值集(严格子集)⇒ 机器化「载体**不是**平台词表的副本」。⚠️ 载体**不可自证**(`kUsedPrimitiveNames.size() == 3` 只陈述**事实**,不蕴含「非副本」——平台集若也是 3 个则它**就是**副本而断言仍绿)⇒ 必须**跨载体**判 |
| **X-2 pending 完备** | 载体的 pending 条目集合 == 锚点摘要的 `N-1` ~ `N-9`(**逐项相等**,不多不少) |
| **X-3 basis 完整** | 每格 basis 有 `ref`;且 `self-derived` 格的 `ref` **不得**指 `A-n`(公开来源撑不住 ⇒ 禁止冒充) |

---

## §3 证书的规约面(四条)

| # | 证书 | 规约面提供的**可判定形式** |
|---|---|---|
| **C-1′** | 槽位语义继承 | 参照域 = **两域并集**(ArchForge 派生域 + `repo_owned_instance_ids[]`);判定 = 该 network 上**全部有绑定的**既有实例的映射**逐类相等**;**不一致 ⇒ 该网不得用 M-2** |
| **C-2** | VC 覆盖完整 | 覆盖集 SHALL ⊇ 该 network 的 `vc_count` 全部取值(直接对 `fabric.json` 判)⇒ ⛔ **不**依赖「边表是否引用某 VC」 |
| **C-3** | 控制面与数据面互斥 | 某 group 有 op in-flight 时 SHALL **拒绝** `group_register_region` / `group_destroy` |
| **C-4** | 未检验项登记 | **U-1**(H6 隔离假设)与 **U-2**(同 VC 内两类流量的相互阻塞**不被图模型表达**)SHALL 登记为**未检验** |

⚠️ **证书的判据面**属 `cep-collective-plane-contract`(实现层的 delta 承载)⇒ 本层**只**给规约面。

### §3.1 ⛔ 不得把「重跑 DAG」当证书(实测,写进规约面)

`evaluate_channel_dependency_gate` 的边**按 network 声明、不按 instance** ⇒ 任一实例的图 = 声明边表的
**诱导子图** ⇒ **DAG 的任一子图仍是 DAG** ⇒ 该闸**在结构上不可能**报出由 `vc_binding` 引起的环
(实测:把 `ualink_fabric` 的 `request` 映到 `response` 的槽 ⇒ 仍 **PASS**,`cycles=0`)⇒
「重跑 DAG」**恒真** ⇒ ⛔ **不得**作为证书的判据面。
⇒ 语义部分由 **C-1′ / C-3** 承担;**C-2** 只承担覆盖完整性。

---

## §4 判据面(**11** 组反向 fixture ↔ **11** 个 Scenario)

| 组 | Scenario | 义务 | 归因 id | §5 登记行 | 毒化锚点(**载体中确实存在的翻转点**) | 红形态 |
|---|---|---|---|---|---|---|
| **F-1** | `UIS-01` | 原语集**恰 4** 且**不新增**原语 | `UIS-01` | 原语集恰 4 / 不新增原语 | 影子副本把 `kIncOpCount` 改为 **5**(加第 5 个 INC 原语) | **编译期红** |
| **F-2** | `UIS-02` | reduce 算子**只引用**既有算子族(恰 6 值) | `UIS-02` | reduce 算子复用 | 影子副本翻转 `kOperatorFamilyDefinedInCarrier` ⇒ `true`(自带算子枚举的**声明锁**) | **编译期红** |
| **F-3** | `UIS-03` | partial failure ⇒ `Fault`(不静默) | `UIS-03` | partial failure ⇒ `Fault` | 影子副本翻转 `partial_failure_must_be_explicit` 的**函数体**(恒返回 `silent`) | **编译期红** |
| **F-4** | `UIS-04` | `ReadReduce` 的 coherence 可见性**边界**(pending 不得被当作已定稿) | `UIS-04` | `ReadReduce` 可见性边界 | 影子副本把该格 `kind` 由 `Pending` 改为 `SelfDerived` | **编译期红** |
| **F-5** | `UIS-05` | 排序 / 原子性要求(同 F-4 的形态) | `UIS-05` | 排序 / 原子性边界 | 同上 | **编译期红** |
| **F-6** | `UIS-06` | 控制面归属(pending 边界) | `UIS-06` | 控制面归属边界 | 同上 | **编译期红** |
| **F-7** | `UIS-07` | 集合 VC **继承**(不新造映射)+ 「不一致 ⇒ 不得用 M-2」 | `UIS-07` | 集合 VC 继承 + 不一致即禁用 | 影子副本翻转 `vc_inheritance_is_well_defined` 的**函数体**(恒真 ⇒ 「不一致即禁用」判定失效) | **编译期红** |
| **F-8** | `UIS-08` | `ualink_4` 的 `flow_support` **待定**(不得先填后证) | `UIS-08` | `ualink_4` 的 `flow_support` 待定 | 影子副本把该格 `kind` 由 `Pending` 改为 `SelfDerived`(**冒充**) | **编译期红** |
| **F-9** | `UIS-09` | `perm` 子集 **待定** | `UIS-09` | `ualink_4` 的 `perm` 待定 | 同上 | **编译期红** |
| **F-10** | `UIS-10` | 逐格 `basis` 完备 + `kind ↔ ref` 类别自洽 + pending 自洽 | `UIS-10` | 逐格 basis 完备 + pending 逐项相等 | 影子副本把某格 `ref` **冒充**(如 `{Uis04, "C-9", Pending}`)⇒ 遍历式自洽断言红 | **编译期红** |
| **F-11** | `UIS-11` | 四条证书的**规约面** + ⛔ **不得**以「重跑 DAG」为判据 | `UIS-11` | 四条证书(C-1′~C-4)的规约面 | 影子副本翻转 `kDagRerunIsCertificate` ⇒ `true`(把「重跑 DAG」当作证书) | **编译期红** |

⇒ **11 组**,1:1 对应 `UIS-01` ~ `UIS-11`(= §5 登记的 **11** 个本层归因键)⇒ **每个本层新建 id ≥ 1 条 fixture**
⇒ task 2.4 的「双向覆盖」**可达**。
⇒ 「§5 登记行」列 ⇒ **机器可核对**(每行恰 1 组 fixture ⇔ 每组恰 1 行登记)
⇒ Phase 2 的元测试 SHALL 断言该 **1:1** 对应关系(避免本轮这类「登记增行、fixture 未增」的漂移**再次发生**)
⇒ ⚠️ **机器核对的键 = `UIS-xx`**(§4 的「归因 id」列 ↔ §5 的「归因 id(本层键)」列);
  「§5 登记行」列是**自由文本** ⇒ 只作**人工**可读交叉引用,⛔ **不**用作机器配对的键
  (措辞一改即**静默失配**;per critic 第 3 轮 S2)。

> ⚠️ **红形态 = 编译期红**(11 组**统一**):每组的毒化锚点 SHALL 指向**载体中确实存在的翻转点**
> (声明锁 / 纯函数体 / 表格项)⇒ 影子副本**必然编译期红**(`static_assert` 失败)
> ⇒ 归因来自**编译日志的 `static_assert` 文案**。
> ⇒ ⚠️ 每组锚点 SHALL 在载体中**可定位到具体符号**(per critic 第 2 轮 finding:锚点若指向
>   **平台面**或**载体中不存在的概念** ⇒ 影子副本**编译仍绿** ⇒ 该组会被判成「无鉴别力」)。
> ⇒ ⛔ **不得**把分组文案(如 `UIS-04/05/06/08/09:`)用在断言里 —— 该串**不含** `UIS-05` 等子串
> ⇒ 毒化某格时**无法精确命中**该格的归因 id ⇒ 「归因精确相等」对该格**不可能通过**。
> ⇒ 每条断言 = **一个**归因 id,文案以 `<id>: ` 开头(照队列 #13 载体形态)。
>
> ⚠️ **归因的匹配形态** = `grep -q "static assertion failed: <id>: "`(**带尾冒号 + 空格**):
> - ⛔ **不得**用子串 `grep -q "<id>"` —— `UIS-01` 是 `UIS-10` 的**前缀** ⇒ 毒化 `UIS-10`
>   时 `UIS-01` 会被**误命中** ⇒ fixture 失真(per Phase 1 实测已复现并排除);
> - 毒化**某格的 `kind`** ⇒ 该格**自身**的断言先红(归因 = 该格 id,最贴切);
>   毒化**某格的 `ref`**(冒充)⇒ `UIS-10`(basis 自洽性义务)红。
> - ⚠️ 上述字面量是 **g++ 专有**(clang 为 `static_assert failed due to requirement`)
>   ⇒ 本仓 CI 为 g++-14 ⇒ 成立;若 Phase 2 想**跨编译器**,改为在 error 行上匹配 `<id>: `
>   (per critic 第 3 轮 NIT)。
>
> ⚠️ **归因判定 = presence,不是「唯一」**(per critic 第 2 轮 finding):毒化某格的 `kind` 时,
> 编译日志**同时**含**该格 id** 与 **`UIS-10:`**(遍历式自洽断言必红)⇒ 驱动 SHALL 做
> 「**含**期望 id」的 presence 判定(照队列 #13 的 `grep -q` 形态),
> ⛔ **不得**要求「日志中只有一个归因 id」。
>
> ⚠️ **驱动脚本 SHALL 走双分支**(照队列 #13 的 `cep_nvlink_sharp_reverse_verify.sh`):
> ① 影子副本**编译红** ⇒ 归因取 `grep -q "static assertion failed: <id>: "`;
> ② 影子副本**编译绿** ⇒ 跑探针 ⇒ 归因取 `grep -qx "PROBE:FAIL <id> .*"`;
> ③ 两者都绿 ⇒ 判「**毒化后仍绿 ⇒ 无鉴别力**」⇒ 该组 fixture 失效。
>
> ⚠️ **F-8 / F-9 的毒化锚点已在 Phase 1 修订**(per critic 第 1 轮 **B1**):原写法「把该格填成具体值」
> 的目标是 `ualink_4` 实例声明的 `flow_support` / `perm` ⇒ **本仓无 `ualink_4` 实例**
> (`protocol_instances.json` 实测仅 7 个实例)⇒ **无处可毒** ⇒ task 2.2 的依赖顺序不可行。
> ⇒ 改为**载体可表达**的形态(与 F-4 ~ F-6 同形:毒化该格的 `kind`)。
> ⇒ ⚠️ `ualink_4` 的**实例声明本身**(含具体取值)由**实现层 change** `cpptlm-cep-ualink-inc` 交付
> (per `ADR-CEP-01` 附录「**悬空判据禁令**」:载体尚不存在者该判据在载体落地前**不可执行**)
> ⇒ 本 change 只承载「该格**保持 pending**」这一条 ⇒ **无悬空判据**。

⇒ **11 组**,1:1 对应 `UIS-01` ~ `UIS-11`;每组**必红** + 归因**命中**该组 id;**基线正控**(干净载体必绿)。
⇒ ⚠️ **F-1 ~ F-11 的红形态统一为编译期红** ⇒ 归因匹配用编译日志的 `static_assert` 文案
(形态见上)⇒ Phase 2 的驱动脚本 SHALL 走**影子目录优先**的编译路径(照队列 #13 先例)
+ **双分支**(编译红 or 探针红)。

⇒ ⚠️ **探针的「鉴别力自测」另计**(运行期;验证探针对毒化**可观测**)⇒ **不**计入上表的 11 组
⇒ 保持「11 组 ⟺ 11 个归因 id」的 **1:1** 不被破坏。
⇒ **跨载体**元测试(X-1 / X-1b / X-2 / X-3)由 `scripts/test/cep_ualink_inc_state_spec_probe.cc`
承担(载体**自足** ⇒ 跨载体比对**只能**在探针侧)⇒ 经驱动脚本进入 ctest。

---

## §5 归因 id 登记(**唯一**登记处 = 本 design §5)

| 义务 | 归因 id(本层键) | **系统级归因 id** | **`detail` 子标记** | 状态 | 代码登记点 |
|---|---|---|---|---|---|
| 原语集恰 4 / 不新增原语 | `UIS-01` | —(判据本体 = `ADR-CEP-03 §9 V2`;`CP-PRIM-SET` 仅见于台账描述、**不入库** ⇒ ⛔ 不作归因) | —| 本层新建 | 载体 + 测试用例名 |
| reduce 算子复用 | `UIS-02` | —(同源义务 = 队列 #13 的 `NVA-05`;该 change **已归档、不入库** ⇒ 本层以 `UIS-02` 为**可执行**归因键) | —| 本层新建 | 同上 |
| partial failure ⇒ `Fault` | `UIS-03` | **`CP-PARTIAL-FAIL`**(队列 #13 已确立,committed 于 `openspec/specs/cep-nvlink-sharp/spec.md`) | —| 本层新建(键)+ **复用**(系统 id) | 同上 |
| `ReadReduce` 可见性边界 | `UIS-04` | —(UALink 专属) | —| 本层新建 | 同上 |
| 排序 / 原子性边界 | `UIS-05` | —(UALink 专属) | —| 本层新建 | 同上 |
| 控制面归属边界 | `UIS-06` | —(UALink 专属) | —| 本层新建 | 同上 |
| 集合 VC 继承 + 不一致即禁用 | `UIS-07` | **`CP-03-VC-CERTIFICATE`**(`ADR-CEP-01` SU#4 的 E.3.1) | **`/C-1′`** | 本层新建(键)+ **复用**(系统 id) | 同上 |
| `ualink_4` 的 `flow_support` 待定 | `UIS-08` | —(UALink 专属) | —| 本层新建 | 同上 |
| `ualink_4` 的 `perm` 待定 | `UIS-09` | —(UALink 专属) | —| 本层新建 | 同上 |
| 逐格 basis 完备 + pending 逐项相等 | `UIS-10` | —(本层元判据) | —| 本层新建 | 同上 |
| 四条证书(C-1′~C-4)的规约面 | `UIS-11` | **`CP-03-VC-CERTIFICATE`**(C-1′ / C-3 / C-4)· **`CP-09-IC1`**(= **C-2**,per `ADR-CEP-01` E.3.1「C-2 复用 `ADR-CEP-09` 既有的 `CP-09-IC1`」) | **`/C-1′ ~/C-3 ~/C-4`** 与 **`CP-09-IC1`**(=C-2) | 本层新建(键)+ **复用**(系统 id) | 载体(`kDagRerunIsCertificate` + 义务格) |

⇒ **基数规则(双向)**:
- **层键 → 系统 id 允许 1:N** —— 「同一义务不得有两个 id」约束的是**同一义务**,
  ⛔ **不**禁止**一个义务格**登记**多条系统 id**(本表的 `UIS-11` 即是);
- **系统 id → 层键 允许 1:N** —— `ADR-CEP-01` SU#4 的 **E.3.1** 明令**四条证书共用**一个
  `CP-03-VC-CERTIFICATE` ⇒ 同一 id 必然出现在多个层键上(`UIS-07` / `UIS-11`);
- ⇒ **消歧靠两级**:**`detail` 子标记**(per E.3.1 的 `[CP-03-VC-CERTIFICATE/C-1′]` 等
  +「归因匹配按**完整前缀精确相等**」)+ **层键**本身;
⇒ ⚠️ **`detail` 子标记列是 Phase 2「两列对应关系」元测试的权威期望**(per critic 第 3 轮 S1)
⇒ 归因 id → 义务的映射**严格按 ADR E.3.1**:`C-1′` / `C-3` / `C-4` ⇒ `CP-03-VC-CERTIFICATE`;
**`C-2` ⇒ `CP-09-IC1`**(per E.3.1「C-2 复用 `ADR-CEP-09` 既有的 `CP-09-IC1`,**不**新造」)。

⇒ **同一义务不得有两个 id**(照队列 #13 的 `design §7` 纪律)⇒ 本层**不新建** `CP-*`
⇒ `UIS-xx` 是**本层归因键**(Scenario 粒度),与**系统级** `CP-*` **指向同一义务**时
在上表**并列登记** ⇒ Phase 2 的元测试 SHALL 校验两列的**对应关系**(不新建系统 id)。
⇒ ⚠️ **不得**引用**不入库**的归档件作为归因(`NVA-05` / `CP-PRIM-SET` 仅作**语义溯源**)。

⇒ ⛔ **不新造 `CP-*` 体系**(per `ADR-CEP-01` 附录「禁在本仓新造平行 id 体系」)⇒ 本层用 `UIS-xx` 作归因键
(照队列 #13 规约层的 `NSS-xx` 先例)。

---

## §6 pending 边界(9 项 ⇒ 本层纪律)

`N-1` ~ `N-9`(锚点摘要 §3,逐项对应)⇒ **不得**被本层或实现层**推理填空**。
⇒ 本层只实现**结构性**语义;凡落到这 9 项 ⇒ **一律** `pending` + 登记 `ref`。
⇒ ⚠️ **合规注记**:UALink Common 2.0 的规范正文受 **Evaluation Copy Agreement** 约束(实测,锚点摘要 §0)
⇒ 与本仓 **CXL ECA §3** 同型 ⇒ 技术正文**禁投喂 AI** ⇒ `N-n` 的转正需**新**的 Task 0 轮次(不得就地扩写)。

---

## §7 文档同步面 + 冻结面

| 面 | 动作 | 时点 |
|---|---|---|
| 台账 `obj §10` | 登记本 change(规约层) | 建档时 |
| `scripts/README.md` | 守门表新增行 + **反向 fixture 登记**新增节 | Phase 3 |
| 冻结面 | **零 diff**(本层不动任何 M1 产物)⇒ 归档时**无**需两步授权 | — |

---

## §8 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| **公开来源受限** ⇒ 9 项 pending ⇒ 本层的语义覆盖面窄 | 🟡 中 | 逐格 basis 全部指向 `C-n`;pending 逐项登记;⛔ 实现层不得推理填空 |
| **禁项清单**漏检(载体偷偷引用平台头)⇒ basis 随实现漂移 | 🟡 中 | 静态扫描 + **鉴别力自测**(毒化载体 ⇒ 必红) |
| **`IC1` 恒真** ⇒ 证书退化 | 🟢 低(已处置) | §3.1 写进规约面 + ⛔ 禁止作判据 |
