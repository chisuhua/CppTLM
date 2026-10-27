# ADR-CEP-07A: 06-bis 既有产物处置

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **06-bis v0.3 规划的 C++ / JSON / Python 产物的逐项处置(采用 / 改名 / 废弃)**
> **关联真源**:
> - 真源 1:`docs/architecture/06-bis-coherence-as-network-extension.md` v0.3 §3.4 / §4(候选 D / 产物清单)
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §3.2(Stub Cache RN 投影)+ §5.4(Bundle 约束)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
>
> **一手锚点(代码实证,2026-10-21)**:
> - `include/core/coherence_domain.hh`(69 行)**存在** —— TDD Red stub,含 `enum class Protocol { MESI, MOESI }`(:19-22)
> - `src/core/coherence_domain.cc` **存在** —— `set_protocol` / `set_members` / `set_snoop_fanout` / `is_member`
> - `include/tlm/coherent_xbar_tlm.hh`(51 行)**存在** —— TDD stub,write-through 透传,**不下场做 coherence 决策**(文件头自述)
> - `include/bundles/coherence_bundles_tlm.hh` **不存在**
> - `src/core/topology_parser.cc:41` `parse_coherence_domains_array`;`:109-114` **接受但不消费**,告警 `"[STUB] coherence_domains field accepted but not consumed; see ADR-X.14 ..."`
> - `cpptlm/topo/layer.py:211` `add_coherence_domain(self, name, protocol: str = "MESI", ...)` —— **带协议字符串参数**
> - `include/core/module_factory.hh:307` `static bool validate_domain_boundary(...)` 声明存在
> - `docs/adr/ADR-X.14-coherence-domains-stub.md` **存在**(Accepted 2026-06-17)

---

## 1. 背景

### 1.1 问题

06-bis v0.3 §3.4 / §4 规划了一组 Coherence 产物。CEP 承接了该 scope,但**逐项处置未定义**。风险有二:

| 风险 | 表现 |
|------|------|
| **协议泄漏** | 既有 stub 把协议名写进了**与协议无关的层** —— `coherence_domain.hh` 的 `enum class Protocol{MESI,MOESI}`、`layer.py` 的 `protocol="MESI"` 参数 |
| **僵尸 stub** | `coherence_domains` 被解析但**不消费**;`validate_domain_boundary()` **不被任何代码路径调用** —— 长期占位,读者误以为已实现 |

### 1.2 处置的三个选项

per `ADR-CEP-00 §3` 的处置语义:**采用 / 改名 / 废弃** 三选一,每项**必须**给理由。

---

## 2. 决策

### 决策 1:逐项处置表

| # | 产物 | 真实状态(2026-10-21) | 处置 | 理由 |
|---|------|-------------------|------|------|
| **1** | `include/core/coherence_domain.hh`<br>`src/core/coherence_domain.cc` | **存在**(69 行 TDD Red stub);含 `enum class Protocol { MESI, MOESI }` | **废弃**(文件保留,加 `[[deprecated]]` + 顶部注释;**不删**,以免破坏既有引用) | ① **协议泄漏**:`enum Protocol` 把 MESI/MOESI 写进了通用层,违反 `INV-CEP-05-IC1` 与 `ADR-CEP-14` C9 负向断言<br>② CEP 用 **Stub Cache Core** 取代它(per `ADR-CEP-05`),二者职责不同(per `ADR-CEP-15` 决策 1)<br>③ dGPU 侧若需 coherence 语义,走 ArchForge(per `ADR-CEP-16` 决策 3) |
| **2** | `include/tlm/coherent_xbar_tlm.hh` | **存在**(51 行 TDD stub);write-through 透传,**不下场做 coherence 决策** | **保留 + 重新定位**(文件 0 改动) | ① 它的行为(write-through 透传)**恰好符合** CEP 的"传输平面不做协议决策"原则(per `ADR-CEP-01` ADR-G)<br>② 归属改为 **Fabric & Transport Plane 的 snoop broadcast 通道**<br>③ 其文件头自述的 "Phase 7.C: 引入 6×6 state transition switch 表" 意图 → 由 **CEP 的 ProtocolEngine** 承接(架构侧),**该文件不改** |
| **3** | `include/bundles/coherence_bundles_tlm.hh` | **不存在** | **不创建** | CEP 用 **canonical 10 流的 Bundle**(per `ADR-CEP-02` / `07 号 §5.4`);06-bis 规划的"3 个 coherence Bundle"是单协议时代的设计,与多协议骨架不兼容 |
| **4** | JSON 字段 `coherence_domains` | **存在**;`topology_parser.cc:109-114` 接受但**不消费**(告警指向 `ADR-X.14`) | **保留字段 + 语义重定义 + 延后消费** | ① **字段删除**会破坏既有 4 个 config(`apu_soc_full` / `apu_soc_phase7a` / `apu_soc_phase7b` / `hierarchy_tree_3level`)<br>② 但当前"接受但不消费"是**僵尸 stub**,必须改语义<br>③ 新语义 = **CEP 一致性域声明**的载体,**消费**在 **Stage 2**(per `ADR-CEP-13`)<br>④ **Stage 0~1 期间**该字段仍标 `not-consumed`,但**告警文案**须更新,明确指向 `ADR-CEP-13` 而非 `ADR-X.14` |
| **5** | `ModuleFactory::validate_domain_boundary(...)` | 声明存在于 `include/core/module_factory.hh:307`;**不被任何代码路径调用**(`ADR-X.14` 记实现于 `module_factory.cc:451-483`,本次 grep 未命中 .cc —— 行号口径待核实) | **保留 + 接入 lint** | 它是**只读校验器**,正适合做 CEP 域边界的 lint 钩子(替代"僵尸"状态)。落点:`scripts/test/cep_docs_sync_check.sh`(Stage 0) |
| **6** | Python `cpptlm.library.coherence_bridge()` 工厂 | **不存在** | **不实现** | CEP 不做 bridge 工厂;其对应物是 `tools/` 的 3 个脚本(per `ADR-CEP-06 §4`,per `ADR-CEP-16` 决策 1 的 P3.5 行) |
| **7** | `cpptlm/topo/layer.py:211` `add_coherence_domain(name, protocol="MESI", ...)` | **存在**;**带 `protocol` 字符串参数** | **保留拓扑能力 + 剥离协议参数**(Stage 2 起) | ① 拓扑侧"声明一个域"是**有用**的能力,应保留<br>② 但 `protocol="MESI"` 是**协议泄漏**(per `ADR-CEP-14` C9);新语义应是**域标识 + 成员**,协议归属由 **capability schema** 承载(per `ADR-CEP-12`)<br>③ **Stage 0~1 不改**(避免动既有 Python 测试),Stage 2 与 `ADR-CEP-13` 同步改 |

### 决策 2:处置的时间分期

| 分期 | 动作 | 归属 |
|------|------|------|
| **Stage 0** | ① 加 `[[deprecated]]` + 顶部注释(产物 1)<br>④ 更新告警文案(产物 4)<br>⑤ 接入 lint(产物 5) | `cpptlm-cep-umbrella`(Task 2.1 / 7.3) |
| **Stage 1** | 无(产物 2 / 7 暂不动) | — |
| **Stage 2** | ④ `coherence_domains` **真正消费** + ⑦ `layer.py` 剥离 `protocol` 参数 | `cpptlm-cep-cxl-dcoh-symmetric`(Stage 2.1) |
| **Stage 4+** | 产物 1 的实体删除(若届时 0 引用) | 维护态 change |

### 决策 3:协议泄漏的**通用禁令**

**决策**:CEP 体系内,**任何**与协议无关的层(Stub Cache Core / Directory & Ordering Plane / Traffic Generator / 拓扑 Python 层)**不得**出现协议标识符(MESI / MOESI / CHI / CXL / TileLink / NVLink / UALink / token / 瞬态态名)。

**协议标识符的合法归属**:
| 位置 | 允许 |
|------|------|
| ProtocolEngine 插件内部 | ✅ 全部 |
| capability schema 的**数据值**(per `ADR-CEP-12`) | ✅ 全部 |
| Stub Cache Core | ❌ |
| Directory & Ordering Plane | ❌ |
| 拓扑 Python 层 | ❌ |
| Bundle 定义 | ❌ |

**理由**:这正是 `ADR-CEP-14` 的 **C9 负向断言**要机器检查的内容;把它提升为**通用禁令**后,`coherence_domain.hh` 的 `enum Protocol` 与 `layer.py` 的 `protocol=` 参数就都是**同一类违规的两个实例**,处置口径统一。

### 决策 4:显式排除

| 排除项 | 理由 |
|--------|------|
| **不**删除 `coherence_domain.hh` / `.cc` | 避免破坏既有引用;先 deprecated,Stage 4+ 再议 |
| **不**创建 `coherence_bundles_tlm.hh` | 决策 1 的产物 3 |
| **不**实现 `cpptlm.library` 的 bridge 工厂 | 决策 1 的产物 6 |
| **不**在 Stage 0~1 改 `layer.py` | 决策 2 分期(避免动既有 Python 测试) |
| **不**在 `include/tlm/cep/` 放 `CoherenceDomain` 的副本 | 决策 3 |

---

## 3. 不变量

- **INV-CEP-07A-IC1**:`include/core/coherence_domain.hh` 与 `src/core/coherence_domain.cc` **必须**带 `[[deprecated]]` 标记 + 指向 `ADR-CEP-07A` 的顶部注释;**不得**被新代码引用
- **INV-CEP-07A-IC2**:与协议无关的层(Stub Cache Core / Directory & Ordering Plane / Traffic Generator / 拓扑 Python 层)**不得**出现协议标识符;例外仅 ProtocolEngine 插件内部与 capability schema 的数据值
- **INV-CEP-07A-IC3**:`coherence_domains` JSON 字段**必须**保留(4 个既有 config 依赖);其告警文案**必须**指向 `ADR-CEP-13`(消费阶段)而非 `ADR-X.14`;消费**必须**在 Stage 2 落地,Stage 0~1 期间标 `not-consumed`

> **编号位说明**:`INV-CEP-07A-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **deprecated 标记**:`coherence_domain.hh` 有 `[[deprecated]]` + `ADR-CEP-07A` 引用(per `INV-CEP-07A-IC1`)
2. **协议标识符扫描**:Stub / Ordering Plane / 拓扑 Python 层的源码 grep `MESI|MOESI|CHI|CXL|TileLink|NVLink|UALink` **必须为空**(per `INV-CEP-07A-IC2`;Stage 0 由 `test_cep_canonical_invariants.cc` 的 C9 负向断言覆盖)
3. **JSON 字段保留**:4 个既有 config(`apu_soc_full` / `apu_soc_phase7a` / `apu_soc_phase7b` / `hierarchy_tree_3level`)仍含 `coherence_domains`,且解析**不报错**(per `INV-CEP-07A-IC3`)
4. **告警文案**:`topology_parser.cc` 的 stub 告警**必须**指向 `ADR-CEP-13`(per `INV-CEP-07A-IC3`)

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-05`(Stub Cache)| ✅ v0.7 | 产物 1 的取代者 |
| `ADR-CEP-12`(能力映射表)| ✅ v0.1 | 产物 4 / 7 的协议归属载体 |
| `ADR-CEP-13`(一致性域归属)| ✅ v0.1 | 产物 4 的**消费**阶段;产物 7 的改造时点 |
| `ADR-CEP-14`(Stub 合同测试)| ✅ v0.1 | C9 负向断言 = 决策 3 的机器化 |
| `ADR-CEP-15`(CacheTLM 对齐)| ✅ v0.1 | 产物 1 与 `CacheTLM` 的边界一致 |
| `ADR-CEP-16`(06 §15.4 映射)| ✅ v0.1 | P2.1 的 parser stub 替换 → 本 ADR |
| `ADR-X.14-coherence-domains-stub.md` | ✅ Accepted | 本 ADR **取代**其"`coherence_domains` 是 stub"的定位(改为"待 Stage 2 消费") |
| `06-bis-coherence-as-network-extension.md` | 🟡 v0.3 | 顶部加 `## Status Update` 段指向本 ADR(per `cpptlm-cep-umbrella` Task 2.1) |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 逐项处置 | 7 项产物:废弃 1 / 保留重定位 1 / 不创建 1 / 保留改语义 1 / 保留接 lint 1 / 不实现 1 / 保留改参数 1 | ✅ 已拍板 |
| 时间分期 | Stage 0 三项 / Stage 1 无 / Stage 2 两项 / Stage 4+ 一项 | ✅ 已拍板 |
| 协议泄漏通用禁令 | 与协议无关的层**不得**出现协议标识符 | ✅ 已拍板 |
| 显式排除 | 不删文件 / 不建 Bundle / 不做 bridge 工厂 / Stage 0~1 不改 Python / 不放副本 | ✅ 已拍板 |

---


## Status Update 2026-10-27 —— 决策 1 第 3 项的**替代物一直是悬空引用** + 新路径登记

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记状态变化与路径偏离。
> **触发**: 2026-10-27 architect 角色架构重审 + `ADR-CEP-19` 签发;台账 Entry #74。
> **登记 ① —— 决策 1 第 3 项的替代物是悬空引用**:该项处置 `include/bundles/coherence_bundles_tlm.hh`(**不存在** → **不创建**),理由栏写「CEP 用 **canonical 10 流的 Bundle**(per `ADR-CEP-02` / `07 号 §5.4`)」。实测该替代物**在本仓不存在**:`ADR-CEP-02` 全文 `Bundle` **零命中**;`07 号 §5.4` 业务 Bundle 列表(7 项)**无它**;`include/bundles/` 17 个 `.hh` grep `coherence` **零命中**。⇒ 「不创建 `coherence_bundles_tlm.hh`」这个**决策本身仍然正确**(理由是避免抢 06-bis P2 的文件,而 06-bis P2 随 06-bis 被 CEP supersede 后**永不实施**),但**其指定的替代物并不存在** ⇒ CohIf 长期**无承载形态**。**同源表述另见** `ADR-CEP-16` §2 决策 1 的 P2.0 行(同日登记)。
> **登记 ② —— 路径偏离(不违反本决策)**:本决策**只**禁止 `include/bundles/coherence_bundles_tlm.hh` 这一个路径,**未**禁止其他路径。`ADR-CEP-19` 决策 6 选定 **`include/bundles/cep_coherence_bundles_tlm.hh`** ⇒ 路径不同 ⇒ **不构成对本决策的违反**,登记备查。命名风格与 `ADR-CEP-03` Status Update 2026-10-26 登记的 collective 未来路径(`include/bundles/collective_bundles_tlm.hh`)**同构**。
> **登记 ③ —— 交付主体**:该 Bundle 的交付队列 = **`cpptlm-cep-cohif-carrier`**(队列 #7,Stage 1),**不**在本 ADR 的处置范围内;本 ADR 只负责**既有产物的处置**,新增产物由 `ADR-CEP-19` 承接。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #74**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #74)。

## Status Update 2026-10-27(第二则)—— §4 验证口径的一处**不实声称**订正 + `INV-CEP-07A-IC2` 的覆盖现状与时效张力登记

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记事实订正与覆盖现状。
> **触发**: 2026-10-27 CEP 代码 / 路线图 / 架构文档三方对齐审查;台账 Entry #83。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #83**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #83)。

### ① §4 验证口径第 2 项的**不实声称**订正(事实错误,非决策变更)

§4「验证口径」第 2 项原文:
> 「协议标识符扫描:Stub / Ordering Plane / 拓扑 Python 层的源码 grep `MESI|MOESI|CHI|CXL|TileLink|NVLink|UALink` **必须为空**(per `INV-CEP-07A-IC2`;Stage 0 由 **`test_cep_canonical_invariants.cc` 的 C9 负向断言覆盖**)」

**2026-10-27 实测**:该声称**不成立** —— ① `test_cep_canonical_invariants.cc` 内 **C9 零命中**(该文件只断言「契约存在且自洽」,不扫源码);② C9 负向断言实际落在 `test_cep_schema.cc`,且**只扫 `protocol_instance.schema.json` 一个 JSON 文件**;③ C++ 侧(Stub / Directory & Ordering Plane)与拓扑 Python 层此前**零机器断言**。
⇒ **订正**:验证口径第 2 项的「Stage 0 由 `test_cep_canonical_invariants.cc` 覆盖」**改为**「C++ 侧由 `test_cep_contracts_crosscheck.cc` 的 IC2 断言覆盖(2026-10-27 补齐,commit `7f91abd9`);schema 侧由 `test_cep_schema.cc` 的 C9 覆盖;**拓扑 Python 层暂不覆盖** —— 见 ②」。
> ⚠️ **本次只订正「谁在覆盖」这一事实**,**不改变 IC2 的判据内容**(禁止协议标识符出现在与协议无关的层)—— 判据本身仍然成立且更强。

### ② `INV-CEP-07A-IC2` 与决策 1 第 7 项 ③ 的**时效张力**(登记,不消解)

- `INV-CEP-07A-IC2` 是**无条件**禁令:「与协议无关的层(Stub Cache Core / Directory & Ordering Plane / Traffic Generator / **拓扑 Python 层**)**不得**出现协议标识符」。
- 但决策 1 第 7 项 ③ 明写:`cpptlm/topo/layer.py` 的 `protocol=` 参数 **「Stage 0~1 不改」**(避免动既有 Python 测试),Stage 2 与 `ADR-CEP-13` 同步改。
- ⇒ **Stage 0~1 期间 IC2 对拓扑 Python 层必然不成立**,而 IC2 条文**未带时效限定** ⇒ 读本 ADR 的人会误判当前状态违规。
- **精确位置登记**(拓扑 Python 层的两处违规实例):`cpptlm/topo/layer.py:47`(`protocol: str = "MESI"`)、`:211`(`add_coherence_domain(..., protocol: str = "MESI", ...)`)、`:257`(`protocol=d.get("protocol", "MESI")`)。§3 点名的另一处(`coherence_domain.hh` 的 `enum Protocol`)已按决策 1 第 1 项加 `[[deprecated]]` 处置(`.hh:30` / `.cc:3-6`)。
- **处置**:本次**不改 IC2 条文**(收紧 invariant 属决策变更,需 architect 签发);改为 ① 在 `test_cep_contracts_crosscheck.cc` 的 IC2 断言内**显式登记该层豁免**(含解除条件:Stage 2 + `ADR-CEP-13` 落地后改为纳入扫描并删除 `protocol` 参数)⇒ 豁免**不是静默漏检**;② 在本 Status Update 登记张力。**建议**:IC2 加时效限定(「拓扑 Python 层自 Stage 2 起适用」)—— **待 architect 裁决**。

### ③ C++ 侧 IC2 的覆盖现状(2026-10-27 实测 + 补齐)

| 层 | 载体 | 覆盖前 | 覆盖后 |
|---|------|--------|--------|
| Stub Cache Core | `include/tlm/cep/stub_cache_core.hh` | ❌ 零断言 | ✅ 去注释后扫协议标识符 + 4 条声明字段(`ordering_assumption` / `vc_binding` / `consistency_labels` / `sharer_tracker`)不出现(commit `7f91abd9`) |
| Directory | `include/tlm/cep/sharer_tracker.hh` | ❌ 零断言 | ✅ 同上 |
| Ordering Plane | `include/tlm/cep/ordering_point_checker.hh` / `address_resolver.hh` / `channel_dependency_graph.hh` | ❌ 零断言 | ✅ 同上 |
| **拓扑 Python 层** | `cpptlm/topo/layer.py` | ❌ 零断言 | ⏸ **显式豁免登记**(决策 1 第 7 项 ③「Stage 0~1 不改」)—— 见 ② |
| Traffic Generator | —— | ⏸ 尚无载体 | ⏸ 无载体 ⇒ 无可扫对象 |

**扫描口径**:去注释后按**标识符词**判定(`chi_rnf` / `ChiRnfAdapter` 命中;`chip_id` / `hierarchical` 不命中);词集 = {MESI, MOESI, CHI, CXL, TILELINK, NVLINK, UALINK, MESIF},**不含 TOKEN**(token 是 MOESI_CMP 的**协议私有状态**概念,per `ADR-CEP-17` 决策 3,不是协议名;源码里 `token` 常见于词法解析局部变量 ⇒ 纳入即假阳性)。

### ④ 变异测试反证

新断言经 2 类变异验证可变红(① 注入 `ChiRnfAdapter`;② 注入 `int vc_binding`)。其中 ② 暴露了首版断言「负向断言恒真空」的缺陷(比较口径错误),已修正并补正控 —— 详见 `ADR-CEP-14` 同日 Status Update ③。

---

## Status Update 2026-10-28(第三则)—— `module_factory.hh` 行号漂移登记

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §6 正文一律不改**,仅登记状态变化。
> **触发**: 2026-10-28 队列 #7 `cpptlm-cep-cohif-carrier` **Phase 3 实施**;台账 Entry #86。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #86**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #86)。

> **登记 ① —— 行号漂移**:本文件 §2 表格引用的 `include/core/module_factory.hh:307`(`validate_domain_boundary` 声明)**按「ADR 不可变」不改**;Phase 3 之后该符号在 **313** 行(完整对照表见 `06 号` 文末「附录 · 行号对照表」,2026-10-28)。**符号本身未变、仍存在**,仅行号下移。
> **登记 ② —— 决策 1 第 5 项「保留 + 接入 lint」的载体不受影响**:`validate_domain_boundary` 的落点(`scripts/test/cep_docs_sync_check.sh`)与本 change 无关 ⇒ 本则不处理。

## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
