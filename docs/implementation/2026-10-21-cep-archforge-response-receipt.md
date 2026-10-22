# CEP ↔ ArchForge 对齐答复回执(R-1 ~ R-13 + P0-1 ~ P0-5 逐条)

> **类别**: 实施笔记 · 跨仓回执
> **日期**: 2026-10-21 · **发出方**: CppTLM CEP DRI · **接收方**: ArchForge coherence owner
> **上游提问单**: [`2026-10-21-cep-archforge-interface-alignment.md`](./2026-10-21-cep-archforge-interface-alignment.md)
> **答复载体**: ArchForge `docs/roadmap/cep-interface-alignment.md`(v0.2 已填答复) +
> `docs/adr/ADR-SOC-25-cep-aligned-coherence-architecture.md` +
> `docs/architecture/32-cep-based-coherence-architecture.md`
> **状态**: ✅ **已回执** —— R-1 ~ R-13 共 13 项**全部给出裁定,无一项被拒**;
> 其中 **11 项**采纳(2 项的**落点由本方指定**)、**1 项**采纳但**论证待补**(R-2)、**1 项**部分采纳(R-3)
> **副作用**: 本回执不构成 M2 放行(判据 C-1 ~ C-5 见 §6)

---

## 0. 一句话

ArchForge 的答复**技术结论全部可接受**,我方复核其**代码级断言 6/6 属实**、**契约级引用 23 组逐条可追**、
**2 份协议实例声明经复跑 schema 校验通过**;
第一轮复核提出的 **2 个 P0 + 2 个 P1(N-1~N-4)已被对方全部裁决并回写**(含 2 个 P0 的采纳);
第二轮复核(`8290b4c` + `ac956b8`)再发现 **3 项** ——
**1 个 P0(判据 C-4 的机器验收从未实现)+ 1 个 P0(我方无 fabric config 载体)+ 1 个 P1(R-6' 附带项「扩 `CATEGORY_*`」会造成词表混层,本方不接受)**;
当前 **Q-3(R-2 的判据 (b))** 仍是唯一未交付的对外开口。

---

## 1. 答复载体确认与取证

| 文件 | 角色 | 行数 | 提交状态 |
|------|------|------|---------|
| `docs/roadmap/cep-interface-alignment.md` | 答复载体(§2 勘误 / §4 P0 逐项 / §5 答复表 / §5.1 R-1~R-13 / §6 C-1 协商) | 386 | ✅ **已提交** `6020f42` |
| `docs/adr/ADR-SOC-25-cep-aligned-coherence-architecture.md` | 决策载体(D1~D11 + 9 份返工清单 + 8 项验收) | 327 | ✅ **已提交** `6020f42` → **`8290b4c` 推进至 Accepted** |
| `docs/architecture/32-cep-based-coherence-architecture.md` | 详细设计(8 项发现 + 3 域重划 + 2 份声明 + P0-3 映射 + R-1~R-13) | 924 | ✅ **已提交** `6020f42` → **`8290b4c` 新增 §6.0 per-network 矩阵 + §6.4 schema delta,§6.1/§6.2 声明已改嵌套形态** |
| `docs/roadmap/cep-interface-alignment-review-2026-10-21.md` | **新增** —— N-1~N-4 裁决记录(V-8 的可追溯载体) | 289 | ✅ **已提交** `8290b4c` |

> ✅ **提交复核(2026-10-22 复核 `8290b4c` + `ac956b8`)**:
> - ArchForge 仓新增 **2 个提交**:`8290b4c`(ADR-SOC-25 → **Accepted** + N-1~N-4 裁决回写)+
>   `ac956b8`(D-9 **返工批 1**);工作区干净;
> - ✅ **N-1 / N-2 / N-3 / N-4 全部裁决完毕并回写落点** —— 我方两条 P0 **被采纳**:
>   N-1 裁决为 **per-network 矩阵**(根因 = D8 把两个 fabric 压成扁平表);
>   N-2 裁决为 **选项 A(加 network 维度)**,理由与本方给出的两个选项一致;
> - ✅ **Q-2 已交付** —— `22-nsa §0.5.4` 网络命名空间(5 网络 + 每网络 `vc_count` + NS-R1/NS-R2);
> - 🔶 **Q-3 仍未交付** —— `32 号 §5.2` 与 `ADR-SOC-25` D7 仍只写判据 (a),(b) 未评估;
> - ✅ **D-9 批 1 已落地**(只动**不含锚点**的 2 份:22-nsa + snoop_filter)→ 本仓 33 锚点**仍全绿**;
>   **批 2**(09 + coherence-{protocol,domain,bridge},含全部 20 个锚点)**留待同批重生成**;
> - 🔴 **新发现 3 项**(见 §8.3 M-1 ~ M-3),其中 **M-2 改变了 M2 放行的建议**。

> ✅ **C-1 已闭合**: 「已合入的 coherence 接口规约文档(有 commit hash)」的判据**已满足**
> (commit `6020f42`)。措辞修订(`ADR-CEP-00 §8`)的**必要条件已具备**,
> 剩余的 M2 放行条件见 §6。

---

## 2. 我方独立复核

> 方法: 不接受「文档断言」本身,逐条回查 **CppTLM 仓代码** + **CEP 冻结契约**。

### 2.0 抽验覆盖范围(可审计清单)

> 下表列出本轮回执**实际执行过的每一条**取证动作 —— 便于第三方按图索骥复现。

| # | 取证对象 | 手段 | 结论 |
|---|---------|------|------|
| **V-1** | `register_cache_callback` | `grep -rn` 全仓 `include/ src/ test/` | **0 命中** |
| **V-2** | `class CoherenceProtocol` | `grep -rn` 全仓 `include/ src/` | **0 命中** |
| **V-3** | `class CoherenceBridge` | `grep -rln` 全仓 `include/ src/` | **0 文件** |
| **V-4** | `cache_tlm.hh` 的 snoop/coherence 标识符 | `grep -c "snoop\|coherence"` | **0** |
| **V-5** | `coherence_domain.hh` 的 `[[deprecated]]` 落点 | 读 `:25-29` / `:30` / `:55` | 枚举**无**属性;属性在**类**上(见 §2.2 E-1) |
| **V-6** | `noc_bundles_tlm.hh` 的 `vc_id` / `CATEGORY_*` | 读 `:38` / `:60-62` | 行号与取值**全部吻合** |
| **V-7** | `ADR-CEP-05:170`(`INV-CEP-05-IC1`) | 读 | ✅ 表述一致 |
| **V-8** | `ADR-CEP-05 §10:223-225`(α 失效判据 2) | 读 | ✅ 原文含「静默 E→M 升级」触发条件 |
| **V-9** | `ADR-CEP-11 §3` 锁定表 + `INV-CEP-11-IC1` | 读 | ✅ 6 族无 `moesi_gpu`/`mesif`;表外取值 = 装配期 fail |
| **V-10** | `ADR-CEP-12:163`(`IC1`)/ `:168`(`IC2`)/ 决策 4/5/9 | 读 | ✅ 全部一致 |
| **V-11** | `ADR-CEP-13` 决策 1/2/3 + `IC1`/`IC2`/`IC3` + 决策 4 检查表 | 读 | ✅ 一致;**「(a)(b) 必须同时评估」原文在案** |
| **V-12** | `ADR-CEP-15:115`(`IC3`)/ 决策 4 | 读 | ✅ 一致(adapter 归 Stage 4+) |
| **V-13** | `ADR-CEP-02:55`(`CEN-Ib1`)/ `:66`(`CEN-Ib11`)/ §2 流 1~10 表 | 读 | ✅ 一致(4 值锁 / 流 1 = request) |
| **V-14** | `ADR-CEP-04 §2` 决策 1 + 决策 3 | 读 | ✅ 5 实现 + caps 4 维一致 |
| **V-15** | `ADR-CEP-17` 决策 2 / 决策 7 | 读 | ✅ 一致(见 §2.2 E-2 的归属修正) |
| **V-16** | `ADR-CEP-07A:98-99`(`IC1`/`IC2`) | 读 | ✅ 一致 |
| **V-17** | `ADR-CEP-06 §3`(`verification_mode`) | 读 | ✅ 存在(降级方案依据) |
| **V-18** | `ADR-CEP-09` 装配期静态论证(决策 1) | 读 | ✅ 一致(死锁自由性归 CEP 侧) |
| **V-19** | `07 号 §1.1` / `§1.4` | 读 | ✅ 框架级定位 + gem5 Ruby 类比一致 |
| **V-20** | `07 号 §2.2` / `§3.2` / `§5.4` | 读 | ✅ 目录平面职责 / stub 折叠表 / 「不做:协议专属字段泄漏」一致 |
| **V-21** | `07 号 §5.1.1` / `§5.1.2` / `§5.1.4` / `:310` / `:254-258` | 读 | ✅ 族/实例计数与 invariant 分组一致 |
| **V-22** | `protocol_instance.schema.json` 的 7 处字段行号 + `protocol_version` pattern | 读 | ✅ 行号与 pattern 全部吻合 |
| **V-23** | `test/test_cep_schema.cc:190-198` / `:200-206` | 读 | ✅ 4 值锁**结构断言**确实存在(见 §4 R-2) |

> **抽验范围声明**: 上表 23 组为本轮回执**实际执行**的取证动作。
> 本回执**未**对 ArchForge 仓自身的 `file:line` 做独立校验(该侧由 ArchForge 自查,见 §11 边界 2)。

### 2.1 ArchForge 8 项发现的裁定

| # | ArchForge 发现 | 严重度 | 我方裁定 | 复核证据 |
|---|---------------|--------|---------|---------|
| **F1** | 提问单 §2 把「设计文档级」类型写成「已实现」 | 严重 | ✅ **成立** | `grep -rn CoherenceProtocol include/ src/` → **0**;`grep -rln CoherenceBridge include/ src/` → **0**;`grep -rn register_cache_callback include/ src/ test/` → **0**;`grep -c "snoop\|coherence" include/tlm/cache_tlm.hh` → **0** |
| **F2** | Q3 前提不成立 → 「不要建成这样」 | 严重 | ✅ **成立** | 同上 3 项 0 命中;`INV-CEP-15-IC3`(`ADR-CEP-15:115`)**当前已满足**,迁移成本为零 |
| **F3** | ArchForge 自身对 `SnoopFilter` 有两套结构 | 中 | ✅ **成立** | `09-coherence-protocol.md:311-323`(`vector<uint16_t> sharers` + `State shared_state`)<br>vs `microarchitecture/snoop_filter.md:106-146`(`bitset<16> sharers` + `CoherenceState` + `lru_counter`) |
| **F4** | 提问单漏掉 ArchForge 第二套一致性架构(NSA 目录式) | 严重 | ✅ **成立** | `25-nsa-hardware.md §4.1`(三级目录 L1/L2/L3)+ `§4.2`(MESIF 状态编码)+ `§4.3`(Remote Query VC2)+ `30-nsa §2.1/§2.3`(按 fabric scope 分级)—— **全部存在** |
| **F5** | Q4 前提「ArchForge 无 VC/ordering 概念」不成立 | 中 | ✅ **成立** | `include/bundles/noc_bundles_tlm.hh:38`(`ch_uint<8> vc_id` 0-3)+ `:60-62`(`CATEGORY_REQUEST/RESPONSE/CREDIT`);schema `vc_binding` 值域 0..3 —— **完全同域** |
| **F6** | 「21 ADR / 66 invariant」口径需校准 | 中 | ✅ **成立** | `07 号:254-258` invariant 分组表:Sharer Tracker 组为 `INV-CEP-04-IC1/2/3/5/6` + `IC7`,**IC4 跳空** → 应表述为「67 编号位 / 66 实际定义」 |
| **F7** | §5 答复表覆盖不了 P0-1~P0-5 → C-2 不可验收 | 中 | ✅ **成立** | 6 问 ≠ 5 个 P0:P0-2 / P0-3 **无对应问** → 已在答复轮通过 §4 增列结论列 + `32 号 §7.1` 独立交付**补齐** |
| **F8** | C-1 与 ArchForge 仓性质冲突 → 永久阻塞 | 严重·治理 | ✅ **成立** | ArchForge `git log` 全为 `docs(...)` 提交,无 `src/`、无 `include/` → **结构上产不出「接口 PR」** |

### 2.2 2 处事实精度勘误 + 1 处对照确认

> **E-1 / E-2 为需修正项**;**E-3 为对照确认项**(映射表位置经复核**准确**,列此以证明 §3 的 P0-3 映射表可信)。

| # | ArchForge 表述 | 实测 | 影响 |
|---|---------------|------|------|
| **E-1** | F1 表:「`include/core/coherence_domain.hh:25-28` —— **已带 `[[deprecated]]`**」 | `enum class Protocol` 实际在 **:25-29**,且该枚举**本身没有** `[[deprecated]]` 属性;`[[deprecated]]` 属性在 **`CoherenceDomain`(:30)** 与 **`DomainRegistry`(:55)**;文件头 `:3-4` 声明整文件废弃 | 🟢 结论不变(P0-2 已消解成立 —— `INV-CEP-07A-IC1` 要求整文件带标记,已满足)。但**「枚举已 deprecated」的精确说法**应为「枚举受整文件废弃 + `INV-CEP-07A-IC1` 不得被新代码引用约束」,而非枚举自带属性 |
| **E-2** | `32 号 §10 Q2-b`:「`ADR-CEP-17` 决策 7 / HeteroGen §VII-B:目录无法区分 read vs acquire → **请求改名**」 | `ADR-CEP-17` 决策 7 的内容是「学术/一致性模型族纳入 + `consistency_labels` 为**硬前置**」;「请求改名」出自 HeteroGen §VII-B,**不在该 ADR 内** | 🟢 不影响 R-9 / D3-b 的裁定;仅**归属标注**需修正 |
| **E-3** | `32 号 §7.1`:`SnoopProbe` 记作 `coherence-protocol.md:177`;`SnoopRespBundle` 记作 `:186`;`CoherenceMsg` 记作 `:194` | 实测:`struct SnoopProbe` **:177** ✅;`struct SnoopRespBundle` **:186** ✅;`struct CoherenceMsg` **:194** ✅ | ✅ **对照确认,无需修正** —— 映射表的 `file:line` 精确度经抽查可信 |

### 2.3 1 处论证缺口(R-2 的判据 (b))

> **这是本次回执唯一的实质性技术异议**,必须由 ArchForge 补齐。

**ArchForge 的论证**(32 号 §5.2 / §7.2 / §11 R-2 / ADR-SOC-25 D7):

> `moesi_gpu` 的 `state_transition[2][1] = 3`(E + Write → M)满足 `ADR-CEP-13` 决策 3「域可见写」判据 **(a)**
> (终态允许 store 命中且无需外部通信)→ 因此**是一个域可见写** → 跨域时**必须触发传播**
> → 4 值 `perm` 无法区分静默/显式升级 → **R-2 命中 `ADR-CEP-05 §10` 已写下的触发条件**。

**我方复核发现**:`ADR-CEP-13` 的 `INV-CEP-13-IC2` 判据是 **(a) 与 (b) 的合取**,不是 (a) 单独:

> 请求 `r` 是**域可见写** ⟺ **(a)** 终态 `s1` 允许 store 命中且**无需外部通信**(可能转到 `s2`)
> **且 (b)** `s1` / `s2` 接受导致**数据值响应**的转发请求。

且 `ADR-CEP-13` **自身的决策 4 检查表第 2 项**明文:

> **域可见写判据强制**:`INV-CEP-13-IC2` 的 (a)(b) 两条件**必须同时评估**,缺 (b) 的实现会把 RC 的读请求误判为写。

**结论**:
- ✅ **需求本体成立** —— `perm` 4 值确实无法表达「静默升级 vs 显式请求升级」,这是真实的表达力缺口;
- 🔴 **但「已命中 `ADR-CEP-05 §10` 触发条件」这一断言证据不足** —— 只评估了 (a),未评估 (b);
- 因此 **R-2 的优先级论证与「D-CPU↔D-GPU 跨域正确性无法论证」的强度表述需要补 (b) 后才能成立**。

**要求 ArchForge 补充**:对 `E → M` 逐项给出 (b) 的评估结论
(`E` / `M` 是否**接受**「导致数据值响应的转发请求」—— 若是,说明转发请求在 MOESI 下的具体形态;
若否,则该转换**不满足** (a)+(b) 合取,R-2 的「命中」结论须改写为「潜在的表达力缺口,尚未证实触发」)。

> **我方不同步下调 R-2 的优先级**(需求本体仍须解决),但**不接受**「已命中触发条件」的表述,
> 直到 (b) 补齐 —— 这直接决定 `ADR-CEP-05 §10` 判据 2 的状态是「已触发」还是「仍不触发」。

---

## 3. P0-1 ~ P0-5 逐项回执

> 对应 ArchForge `32 号 §10` / `roadmap §4` 的 P0 逐项结论列。

| # | 冲突 | ArchForge 结论 | 我方裁定 | 落点 | 状态 |
|---|------|---------------|---------|------|------|
| **P0-1** | 6 态枚举归属 | **A** 下沉到协议插件;顶层只留 4 值 `perm` 投影 | ✅ **接受** | ArchForge: `09:242` 删嵌套枚举 / `coherence-protocol.md:112` 标注插件私有 / `32 号 §7.2` 投影表 | **技术闭合**;待 ArchForge D-9 落地 |
| **P0-2** | `enum class Protocol` 是配置入口 | **已消解**(`coherence_domain.hh` 已 deprecated) | ✅ **接受**(附 E-1 精度修正) | 本仓**零改动**;`INV-CEP-07A-IC1` 维持 | ✅ **已闭合** |
| **P0-3** | 4 类 Bundle ↔ canonical 10 流无映射 | **已交付映射表**(`32 号 §7.1`,逐类型 20 行) | ✅ **接受** | 见 §4 逐条核验(下) | **技术闭合** |
| **P0-4** | `snoop_callback` 直连 CacheTLM | **A** 改走 adapter,且**无直连可改** → 动作是**撤下设计**(5 处) | ✅ **接受** | ArchForge 5 处删改 + `ADR-SOC-01 §3` 7.C 改写 | **技术闭合**;`CacheTLM` 侧**零改动** |
| **P0-5** | `CoherenceProtocol` 归属 | **B** 下沉为 ProtocolEngine 插件接口 **+ 3 项强制修订**(D3-a/b/c) | ✅ **接受** | 见 §4 R-9 / A3 | **技术闭合**;待 Stage 1 定稿 |

**P0-3 映射表我方逐行抽验**(5 行关键行,对照 `ADR-CEP-02` 的 canonical 10 流):

| ArchForge 类型 | ArchForge 映射 | 我方裁定 | 依据 |
|---------------|---------------|---------|------|
| `CacheReqBundle`(Read) | 流 1 `request`,`perm = READ` | ✅ 成立 | `ADR-CEP-02 §2` 流 1 语义 |
| `CacheRespBundle` | 流 2 `completion` | ✅ 成立 | CEN-Ib1(`request` 必有配对 `completion`) |
| `CoherenceMsg`(`msg_type` 协议特异) | **无对应 → 解散** | ✅ 成立 | `07 号 §5.4`「不做:协议专属字段泄漏进共享抽象」 |
| `SnoopProbe.source_protocol` / `target_protocol` | **无对应 → 删除** | ✅ 成立 | `INV-CEP-07A-IC2` 协议标识符禁令 |
| NSA Remote Atomic | 流 1 `request` + `atomic.intent_field` | ✅ 成立 | CEN-Ib11「atomic 是 request 的独立属性,不新增 perm 值」 |

> **P0-3 映射表无异议** —— 与本仓冻结契约逐行一致。

---

## 4. R-1 ~ R-13 逐条回执

> 裁定口径:✅ 采纳 / 🟡 部分采纳 / ⏸ 待补证据
> 「冻结影响」列标注该需求是否触碰 **M1 冻结集**(29 产物)——
> 触碰者须走 `.rddf/roadmap/objectives/obj-cep-platform.md §11` 台账 `adr-amendment` entry + 冻结清单重生成。

| # | 需求摘要 | ArchForge 优先级 | 我方裁定 | 冻结影响 | 落点与备注 |
|---|---------|-----------------|---------|---------|-----------|
| **R-1** | SharerTracker 需「分级目录」第 6 种实现 / 放宽 `caps` 维度 | P1 | ✅ **采纳** | 🔴 `ADR-CEP-04` + `schema` | **倾向扩枚举值而非扩维度**:新增 `caps.aggregation_at` 枚举值 `HIERARCHICAL`,**不**新增 `hierarchy_depth` 维度 —— 「深度」属实例参数,「聚合位置」才是 `caps` 语义。`ADR-CEP-04` 决策 3 的 caps 4 维为冻结契约 → 需 adr-amendment |
| **R-2** | `perm` 加「可静默升级」判别位 | P0 | ✅ **采纳需求 / ⏸ 论证待补** | 🔴 `schema` + `ADR-CEP-05 §10` | **落点不能按 ArchForge 建议的「`perm` 改对象数组 `[{value, silent_upgrade_to}]`」** —— 该形态会击穿 `test/test_cep_schema.cc:190-198` 的**结构断言**(`perm.at("items").at("enum")` 4 值锁)。<br>**替代落点(我方指定)**:`perm` **保持纯枚举数组不变**,另在 `perm` **旁新增顶层字段**(如 `perm_upgrade: {from, to, mode}`),或在 `sharer_tracker`/`consistency_labels` 侧承载。<br>先决条件:补 §2.3 的 (b) 评估 |
| **R-3** | `consistency_labels` 支持按 fabric scope 分级 | P1 | 🟡 **部分采纳** | 🔴 `schema` | **不采纳**「一个实例 → 多条 scope 变体声明」—— 会与 `INV-CEP-12-IC3` 的 adapter 计数口径打架(1 实例变 N 条)。<br>**我方倾向**:在 `consistency_labels` 内新增 `scopes` 子对象(`scope → {read, write}`),**实例数不变**。`schema` 的 `consistency_labels` 为 `additionalProperties: false`,新增键须显式声明 |
| **R-4** | `atomic` 承载一致性语义标签(`relaxed`/`acquire`/`release`/`acq_rel`/`seq_cst`) | P1 | ✅ **采纳** | 🔴 `schema` + `ADR-CEP-02` CEN-Ib11 | 落点:`atomic` 增 `intents` 枚举数组。**该形态不破坏现有断言**(`test_cep_schema.cc:200-206` 只校验 `supported`/`intent_field` 存在 + `additionalProperties == false`,新增声明键仍为 false)。CEN-Ib11 补「intent 取值域」 |
| **R-5** | network 命名空间权威清单 | P1 | ✅ **采纳** | 🟢 本仓零改动 | 权威方 = **ArchForge 侧**(扩 `22-nsa §0.5` 为「15 术语 + network 命名空间」);CEP schema **引用而不定义**。<br>**C-4 判据依赖此项**,且**不由本仓提供** → 列入 §8 开口项 **Q-2** |
| **R-6** | 消息类词表 `{request,response,snoop,atomic,query}` + `CATEGORY_*` 扩展 | P0 | ✅ **采纳** | 🟡 `noc_bundles_tlm.hh`(非冻结) | **词表采纳**。**需补 1 项**:本仓现有 `CATEGORY_CREDIT = 2`(实测 `noc_bundles_tlm.hh:62`),而 ArchForge 的 5 类**无 credit** → 扩展须同时裁定 credit 的归属(建议保留为第 6 类,或明确归入 response)。<br>`vc_binding` 现为 `additionalProperties: {integer 0..3}` —— **key 无约束**,故 R-6 的词表确有必要 |
| **R-7** | 「目录式远端查询」的流组合规范(流 8 + 流 3) | P2 | ✅ **采纳** | 🔴 `ADR-CEP-02` / `07 号 §3.1` | P2,Stage 1 前不阻塞 |
| **R-8** | `family` 口径澄清(`moesi_gpu` vs Ruby `MOESI_AMD_Base`/`GPU_VIPER`) | P2 | ✅ **采纳** | 🔴 `07 号 §5.1` | 「同语义不同实现,两边各计各的」与 `07 号 §5.1.4`(:366)原文一致 → 仅需在 `§5.1.1` 增补一行交叉索引 |
| **R-9** | CEP 显式声明「插件接口**不含** `translate(from,to)`」 | P0 | ✅ **采纳** | 🔴 `ADR-CEP-12` + `ADR-CEP-13` | **零实现成本** —— `CoherenceProtocol` 本仓 **0 命中**,从未实现。落点:`ADR-CEP-13` 决策 3 增**负向约束**,`ADR-CEP-12` 增「插件能力映射不含双协议转换方法」 |
| **R-10** | `CoherenceDomain` 弃用后的「域」概念对齐 | P1 | ✅ **采纳** | 🔴 `ADR-CEP-13` + `INV-CEP-07A-IC3` | ArchForge 侧停用该模块名;本仓 `coherence_domains` JSON 字段维持 `not-consumed`(Stage 2 消费)→ **本仓零改动** |
| **R-11** | 「真实缓存 + CEP 协议」路线在 Stage 1 前定案 | P0 | ✅ **采纳 (a)** | 🔴 `ADR-CEP-15` | 采纳 **路线 (a)= CEP `Stub Cache Core`**;`CacheTLM` 侧**永不**实现 snoop。与 `ADR-CEP-15` 决策 4(adapter 归 Stage 4+)一致,且 `INV-CEP-15-IC1`(`cache_tlm.hh` **0 diff**)得以保持。<br>**补一句到 `ADR-CEP-15`**:「若走 (a),`CacheTLM` 侧无需任何 snoop 入口」 |
| **R-12** | `perm` 与 Capability 的正交性 | P1 | ✅ **采纳** | 🔴 `schema` + `ADR-CEP-12` | 显式声明 `perm` 不承载安全/租户语义;capability 属 Fabric / 控制面层 |
| **R-13** | `ADR-CEP-11 §3` 锁表增 `moesi-gpu-1.0` / `mesif-1.0` | P0 | ✅ **采纳(附强制连带条件)** | 🔴 `ADR-CEP-11` **+** `07 号 §5.1` | **必须同批改 2 个冻结产物**:锁表使用规则第 2 条「表中**不含**任何未列入 §5.1 的协议族」→ 这 2 个实例必须同时进入 `07 号 §5.1`。<br>🔴 **连带计数变更**:`07 号:310` 现为「**2 个来源族 = 14 个 adapter 实例**」→ 工业族由「5 族 / 6 实例」变为「**6 族 / 8 实例**」→ 总数 **14 → 16** → `ADR-CEP-12` 决策 9 的计数口径 + umbrella `spec.md` 同步。<br>✅ `protocol_version` 取值形式 `moesi-gpu-1.0` / `mesif-1.0` 经实测**符合** schema pattern `^[a-z0-9]+(-[a-z0-9.]+)*$` |

### 4.1 汇总

| 裁定 | 项数 | 编号 |
|------|------|------|
| ✅ **采纳** | 11 | R-1 / R-4 / R-5 / R-6 / R-7 / R-8 / R-9 / R-10 / R-11 / R-12 / R-13 |
| ✅ **采纳需求**,但**落点由本方指定** + **论证待补** | 1 | **R-2** |
| 🟡 **部分采纳** | 1 | **R-3** |
| ❌ **拒绝** | **0** | — |

> **统计口径**:R-1 ~ R-13 共 **13 项**;**无一项被拒**。
> **11 项**可直接进入执行(其中 R-1 / R-13 的**落点**由本方指定,见上表);
> **2 项**需先补一轮 —— R-2(补 `ADR-CEP-13` 判据 (b) 的评估)与 R-3(定 scope 承载形态)。

---

## 5. D3-a/b/c 的裁定(A3 的前置)

> 答复要求 CEP 侧在 Stage 1 前定案插件接口形状。本方**先行裁定**(ADR 文本待 adr-amendment 落位):

| 修订 | ArchForge 要求 | 本方裁定 | 理由 |
|------|---------------|---------|------|
| **D3-a** | `translate(probe, from, to)` 从插件接口**移除** | ✅ **采纳** | 与 `ADR-CEP-13` 决策 3 的「一致性模型翻译 + proxy-cache」机制不兼容;跨域语义改由 `consistency_labels` 三元组映射表承担(数据表,非代码) |
| **D3-b** | 请求语义**携带一致性标签**,不得用 `bool is_write/is_read` | ✅ **采纳** | `consistency_labels.request_level` 已为此存在(`ADR-CEP-12` 决策 4);`bool` 对「read vs acquire」不可区分(与 R-7 同源) |
| **D3-c** | 基类更名 `ProtocolEngine`;`can_coexist` 降为插件内部工具 | ✅ **采纳** | 协议无关层不得出现协议标识符(`INV-CEP-07A-IC2`);`can_coexist` 属协议私有状态机 |

> **零实现成本确认**:本仓 `CoherenceProtocol` **0 命中**,`include/core/coherence_protocol.hh` 不存在
> → D3 三项修订**不产生任何回改**,只在 Stage 1 新建插件接口时生效。

---

## 6. 放行判据 C-1 ~ C-5 状态更新

> 对应 `ADR-CEP-00 §2 决策 3` 的准入门 + 本清单 §6 的 5 条判据。

| # | 判据 | 答复轮状态 | 本方更新后状态 |
|---|------|-----------|--------------|
| **C-1** | ArchForge 侧存在**已合入**的 coherence 接口 PR(有 commit hash)<br>—— **原文**;已按 `ADR-CEP-00 §8` 修订为「接口**规约文档**」 | 🔴 **结构性不可闭合**(ArchForge 无代码仓) | ✅ **已闭合** —— commit **`6020f42`** 已合入三份规约文档(8 文件 / +1415 行);措辞修订的**必要条件已具备** |
| **C-2** | 该 PR 含 P0-1 ~ P0-5 的结论(逐项可追溯到 ArchForge 文档行) | ✅ 已满足(载体 `ADR-SOC-25` + `32 号`) | ✅ **已满足** —— 本回执 §3 已逐项签收 |
| **C-3** | §3 的 6 问各有书面结论 | ✅ 已满足(§5 答复表) | ✅ **已满足** —— 本回执 §4/§5 已逐条签收 |
| **C-4** | CppTLM 的 `protocol_instance.schema.json` 与 ArchForge 侧声明**能双向映射** | 🔶 部分(声明 JSON 已交付,但 R-6 消息类词表 + R-5 network 清单待对齐) | 🔶 **部分** —— 声明 JSON **经我方复跑 schema 校验通过**(2/2);剩余缺口 = **R-5**(network 清单,权威方在 ArchForge)+ **R-6**(消息类词表)+ **R-3**(scope 形态) |
| **C-5** | CppTLM 侧的 10 组反向验证仍全绿(M1 冻结 + lint + ctest) | ⏳ 不在本仓职责内 | ✅ **已复跑全绿**(2026-10-21):`cep_freeze_check` 28/28 hash 一致 + `cep_docs_sync_check` 8/8 组通过 + `cep_archforge_alignment_check` 33 锚点全有效 |

> **M2 结论(2026-10-22 复核 `8290b4c` + `ac956b8` 后更新)**:
> - **C-1 ✅ / C-2 ✅ / C-3 ✅ / C-5 ✅** —— 四条已闭合;
> - **C-4 的判据文本仍满足** —— 13/13 顶层属性在 ArchForge 侧均有承载物(架构形状已定稿);
>   🔶 **但机器层面当前是「红」的** —— ArchForge 已把 §6.1/§6.2 声明切成**嵌套形态**
>   (per N-2 裁决选项 A),而**我方 schema 仍是扁平形态** → 实测两份声明**各 4 处校验失败**;
>   且**该失配无任何 CI 会报红**(M-2:该检查器从未实现);
> - → **M2 放行建议(较 2026-10-21 收紧)**: **先补 A10(schema delta)+ A12(检查器),再放行**。
>   理由:C-4 是 5 条判据中**唯一声称「✅ 可自动化」**的一条;
>   若在检查器缺失 + schema 与声明已失配的状态下放行,等于用人工核验给一条**声称可自动化却未实现**的判据结案 ——
>   这正是 R10 要防的事。先补检查器并跑出「红 → 修 → 绿」,放行记录才站得住;
> - 若你仍决定**立即放行**,则**必须**同时把 A10 / A11 / A12 / M-1 显式写成 umbrella 的 **Stage 1 准入前置**,
>   否则等于把 1 个 P0 + 1 个 P1 悄悄放过去;
> - Stage 0 **仍不 archive** —— R10(独立性缺口)未闭合,与 M2 无关。

---

## 7. D-9 返工对本仓 CI 的影响(独立复核)

> **风险来源**:ArchForge `ADR-SOC-25 §3.1` 列出 **9 份文档返工清单**;其中 **D-9** = `09-coherence-protocol.md` 的 8 处改动。
> 本仓 `scripts/test/cep_archforge_alignment_check.sh` 以**行号 + 符号**双重钉住 ArchForge 引用。

### 7.1 21 个 ArchForge 锚点的受影响判定

> 我方**逐个复核**(非采信二手结论):当前实测 **33 锚点全绿 / 21 ArchForge 锚点全有效**(2026-10-21)。

| # | 锚点 | 返工动作 | 失效形式 |
|---|------|---------|---------|
| 1 | `09:242` `enum class State` | D9 **删**嵌套枚举 | 🔴 内容删除 |
| 2 | `09:243` `enum class Event` | 随 #1 位移 | 🔴 行号位移 |
| 3 | `09:250` `register_cache_callback` | D9 **删** | 🔴 内容删除 |
| 4 | `09:311` `class SnoopFilter` | §7.2 `SnoopEntry` 归并 `snoop_filter.md` | 🔴 改写 / 位移 |
| 5 | `09:314` `struct SnoopEntry` | 同上(归并) | 🔴 内容删除 |
| 6 | `coherence-bridge.md:135` `class CoherenceBridge` | `:49/:81/:121` 改写 + `§2.2` 重述 | 🔴 行号位移 |
| 7 | `coherence-domain.md:68` `enum class Protocol` | `:68` 标注 deprecated | 🟡 同行加注 → 位移风险 |
| 8 | `coherence-domain.md:94` `class DomainRegistry` | `:70-101` 整块重写 | 🔴 行号位移 |
| 9 | `coherence-protocol.md:112` `enum class CoherenceState` | 标注「插件私有」 | 🟡 位移风险 |
| 10 | `coherence-protocol.md:117` `enum class SnoopType` | 同块标注 | 🟡 位移风险 |
| 11 | `coherence-protocol.md:125` `enum class SnoopResp` | 同块标注 | 🟡 位移风险 |
| 12 | `coherence-protocol.md:133` `class CoherenceProtocol` | `:133-155` 拆分「插件接口 / 插件实现」 | 🔴 重写 → 位移 |
| 13 | `coherence-protocol.md:138` `handle_probe` | 同上 | 🔴 |
| 14 | `coherence-protocol.md:141` `handle_request` | 同上 | 🔴 |
| 15 | `coherence-protocol.md:146` `can_coexist` | 同上(D3-c 降为插件内部工具) | 🔴 |
| 16 | `coherence-protocol.md:149` `translate` | **D3-a 删** | 🔴 内容删除 |
| 17 | `coherence-protocol.md:154` `get_protocol_name` | 同 #12 重写 | 🔴 |
| 18 | `coherence-protocol.md:181` `source_protocol` | §7.1 **删** | 🔴 内容删除 |
| 19 | `coherence-protocol.md:182` `target_protocol` | §7.1 **删** | 🔴 内容删除 |
| 20 | `coherence-protocol.md:183` `requestor_cache_id` | §7.1 同块重述 | 🔴 |
| 21 | `roadmap/cep-interface-alignment.md:1` | 不涉及 | 🟢 **安全** |

**判定**: **21 个锚点中 20 个受影响**(其中 **11 个必然失效**、9 个有位移风险),**仅 1 个安全**。

> ✅ **该判断与 ArchForge 侧结论一致**(20/21),我方**独立复核确认**。
> ⚠️ 补充 ArchForge 侧清单未点名的 3 个:`coherence-bridge.md:135` / `coherence-domain.md:68` / `coherence-domain.md:94`
> —— 三者虽不在「09 号 8 处」的字面清单内,但落在 `ADR-SOC-25 §3.1` 的**同一张 9 份返工表**内,同样受影响。

### 7.2 处置结论

1. **风险当前为 0**:D-9 **批 1 已落地**(commit `ac956b8`),但**只动不含锚点的 2 份**
   (`22-nsa-fabric-address-spec.md` + `snoop_filter.md`)→ 本仓 33 锚点现仍全绿;
   **批 2**(09 + `coherence-{protocol,domain,bridge}.md`,含**全部 20 个锚点**)按对方设计留待同批重生成;
2. **风险是「未来时」**:批 2 一落地,`cep_archforge_alignment_check.sh` 立即红 → ctest `cep_archforge_alignment_check` 红 → `cep_docs_sync_check.sh` 第 8 组红;
3. **必须同批**:D-9 落地**必须**与本仓锚点更新**同批**,不可分两批;
4. **锚点更新方式**(A5):重新生成 21 个 ArchForge 锚点 + 重跑 `cep_archforge_alignment_check.sh`。
   建议在 D-9 合入 ArchForge 的**同一个 commit 之后**执行,以便 diff 可对照。

> **不采用**「把行号检查降级为符号存在性检查」的做法 —— 那样会**永久失去**「引用漂移即红」的能力,
> 而这正是本仓 R10(独立性缺口)的**机器化补偿手段**。宁可承担每次 D-9 同步 21 个锚点的成本。

---

## 8. 需 ArchForge 回应的开口项

### 8.1 原 3 项(Q-1 已闭合)

| # | 开口项 | 阻塞什么 | 状态 |
|---|-------|---------|------|
| **Q-1** | 三份答复文档提交进 git | C-1 的充分条件 | ✅ **已闭合** —— commit `6020f42`(2026-10-21 复核) |
| **Q-2** | **network 命名空间权威清单**(R-5) | C-4 机器验收 | ✅ **已交付** —— `22-nsa §0.5.4`(commit `ac956b8`):5 网络清单 + 每网络 `vc_count` + NS-R1/NS-R2;并升级为 **R-5'**(fabric config 需提供 `networks`)+ **R-5''**(`ordering_assumption` 升 per-(network, vc_id)) |
| **Q-3** | **R-2 的判据 (b) 评估**(见 §2.3) | `ADR-CEP-05 §10` 判据 2 的状态 | ⏳ **仍未交付** —— `8290b4c` 未触及;`32 号 §5.2` 与 `ADR-SOC-25` D7 仍只写判据 (a) |

### 8.2 N-1 ~ N-4 的裁决状态(全部已闭合)

| # | 原发现 | ArchForge 裁决(`8290b4c`) | 我方裁定 |
|---|-------|--------------------------|---------|
| **N-1** | Compute NoC 的 VC 分配两套互斥答案 | ✅ **裁决** —— 根因 = D8 把 `compute_noc` 与 `ualink_fabric` 两个 fabric 压成扁平表;改 **per-network 矩阵**(`32 号 §6.0.2`):`08 §4.3` 四槽**保持不变**仅补 6 类词表映射;`25-nsa` VC0/VC1/VC2 原样保留、**VC3 补齐** | ✅ **接受** —— 与我方根因判断一致 |
| **N-2** | `vc_binding` 无 network 维度 | ✅ **裁决选项 A(加 network 维度)**,理由:① 选项 B 无法表达同一协议跑 2 个网络 ② 死锁是 per-fabric 现象,`ADR-CEP-09` 依赖图节点必须是 `(network, vc_id)` ③ 选项 B 令 C-4 不可自校验;`ordering_assumption` 连带升 per-(network, vc_id);**新增 `32 号 §6.4` 可执行 schema delta** | ✅ **接受** —— 选项 A/B 本由本方给出,对方选了 A 且理由更完整 |
| **N-3** | 消息类词表 5 类 vs 声明 6 类 | ✅ **定为 6 类**;**排除** `CATEGORY_CREDIT`(流控)/ `forward_data`(流 6)/ `snoop_completion_ack`(流 10)—— 后两者是已有槽位的承载实例 | ✅ **接受** —— 与本方结论一致 |
| **N-4** | `ADR-SOC-25` 仍为 Proposed | ✅ **推进至 Accepted**;验收项按事实显式化(V-5 待 CEP schema 修订 / V-7 进行中 / **V-8 部分满足** —— 「送达已满足;CEP 侧逐条回执仍待」) | ✅ **接受** —— V-8 的后半段需我方回执**提交**后闭合(见 §9 A13) |

### 8.3 提交复核后新发现的 3 项(M-1 ~ M-3)

> 复核 `8290b4c` + `ac956b8` 后新产生。**M-1 是对我方 R-6' 附带项的反向裁定**,**M-2 是判据实现缺口**。

| # | 发现 | 证据 | 严重度 | 我方裁定 / 期望 |
|---|------|------|--------|-------------|
| **M-1** | **R-6' 的「CppTLM 附带项」不可接受** —— 「`noc_bundles_tlm.hh:60-62` 的 `CATEGORY_*` 需从 3 类扩到覆盖 6 类一致性消息类」会造成**词表混层** | `CATEGORY_*` 是**链路层 flit 类别**:`soc_internal_noc.hh:10` 自述「真实芯片: ARM CHI 4 通道 (REQ/RSP/DAT/SNP) + 4 独立 VC → **简化抽象为 flit_category + vc_counts_[4]**」;<br>实际消费者:`bidirectional_port_adapter.hh:165/235`(credit 回信)、`soc_internal_noc.hh:116`(`vc_counts_[flit_category]` 分桶)、`sdma_noc_adapter.hh:57`;<br>把 `snoop`/`snoop_resp`/`atomic`/`query` 塞进 flit 类别 = **把一致性语义搬进传输层**,且 `vc_counts_[flit_category]` 的桶下标会与「VC 0-3」值域混用 | 🔴 **P1** | ❌ **不扩 `CATEGORY_*`**。<br>✅ **6 类词表落在 CEP Coherence Interface 层**(新增该层常量),`CATEGORY_*` 保持**传输层流控类别**(3 类不变);两者关系用**显式映射表**(adapter 处,**非恒等**)。<br>✅ ArchForge 自己也提示了这点(R-6' 附带项末句:「`CATEGORY_*` 是 flit 类别(链路层),与 `vc_binding` 的消息类(一致性层)是两个正交维度 —— 建议 CEP 侧明确二者关系,避免又一次『词表混层』」)→ 本条即该建议的正式答复 |
| **M-2** | **C-4 声称「✅ 可自动化」,但该检查器从未实现** | 现有 8 组 lint + `cep_archforge_alignment_check.sh` **均不**拿 ArchForge 的协议实例声明跑我方 schema(实测:8 组里只有第 8 组跑 alignment check,alignment check 只校验 `file:line` + 符号)<br>→ **直接后果**:`8290b4c` 把 §6.1/§6.2 声明切成**嵌套形态**后,它们**已不再通过我方现行 schema 校验**(实测 **4 处/份**失败),而**没有任何 CI 会报红** —— 「双方切一半」的中间态**静默存在** | 🔴 **P0** | ✅ **新增第 9 组检查器**「ArchForge 协议实例声明 ↔ 我方 schema 双向映射 + `vc_count` 上界校验」(A12)。<br>该检查器应能在**装配期**跑出本轮的 4 处失配 → **反向验证有牙** |
| **M-3** | **我方无 fabric config 载体**,R-5' 无消费端 | `include/tlm/cep/` 下**只有** `protocol_instance.schema.json`;全仓无 `fabric_config` / `networks` / `vc_count` 概念(实测 grep 0 命中)<br>→ R-5' 要求 fabric config 提供 `networks: { <network_name>: { vc_count: <int> } }`,`INV-CEP-12-IC2` 条件 ② 直接依赖它 | 🔴 **P0** | ✅ 新增 **fabric config schema**(CEP 侧产物,A11),消费 ArchForge `22-nsa §0.5.4` 的网络清单 + `vc_count` |

> **M-2 / M-3 合起来改变了 M2 放行的建议** —— 见 §6 的 M2 结论与 §9 的 A7。

### 8.4 对 ArchForge 的正式答复(A14 —— R-6' 的「CppTLM 附带项」)

> **背景**:R-6' 的「CppTLM 附带项」要求本仓把 `include/bundles/noc_bundles_tlm.hh:60-62` 的
> `CATEGORY_*` 从 3 类扩到覆盖 **6 类一致性消息类**。我方裁定 **不扩**(§8.3 M-1:词表混层 +
> `vc_counts_[vc]` 分桶下标与 VC 0-3 值域冲突)。
>
> 本节是**正式答复** —— 把「我方已定的」与「请对方确认的」分开列出,避免对方第三次只答一半。
> **本节裁定为最终态**:除非贵方提出**新的反证**,本仓不再就 R-6' 附带项重复往返。

#### 8.4.1 我方已定的 4 条(请勿再议)

| # | 我方裁定 | 依据 |
|---|---------|------|
| **A14-1** | 6 类消息类词表落在 **Coherence Interface 层**(`INV-CEP-12-IC2` 条件 ③),**不**扩 `CATEGORY_*` | `ADR-CEP-12:319` 的分层纪律注记(批 1 已落位) |
| **A14-2** | `CATEGORY_*`(`noc_bundles_tlm.hh:60-62`)保持**传输层 3 类**不变 | 其消费者中**只有 `soc_internal_noc.hh:117-118` 把 `flit_category` 当作数组下标**(`vc_counts_[]` 桶下标,**索引对齐**);其余消费者(`bidirectional_port_adapter.hh:165/235` · `sdma_noc_adapter.hh:57,81-83` · `vram_noc_adapter.hh:96` · `src/tlm/nic_tlm.cc:114`)只做**相等比较**或**写入固定值** → 见 §8.4.3 的**互斥论证** |
| **A14-3** | 二者关系由 **adapter 处的显式映射表**承担(**非恒等**) | 6 类词表与 3 类 flit 类**不是**一一展开(`CATEGORY_CREDIT` / `forward_data` / `snoop_completion_ack` 已由贵方 `32 号 §6.0.1` 明确排除) |
| **A14-4** | 6 类词表**不另立 invariant 编号**(并入 IC2 ③) | 另立编号会连带触发 4 处 invariant 计数同步,超出本批范围(台账 Entry #9 ④ 已记录) |

#### 8.4.2 请 ArchForge 确认的 4 条

| # | 待确认 | 确认后的后果 |
|---|-------|------------|
| **A14-C1** | 词表直接**引用**贵方 `32 号 §6.0.1` 的 6 类表,本仓**不重复定义** —— 请确认无异议 | 我方 `ADR-CEP-12:304` 的 IC2 ③ 只写「∈ 6 类规范词表」并指向 `32 号 §6.0.1`,不复制词表 |
| **A14-C2** | 请在 `32 号` 侧对 `CATEGORY_*` 的引用处补注「这是**链路层 flit 类别**,与一致性消息类**正交**」 | 消除对方文档内的第二个歧义源(贵方排除表已隐含此意,只是未在引用处写明) |
| **A14-C3** | 6 类词表 → `CATEGORY_*` 的**映射表归 Stage 1 交付** —— 请确认该归属 | 该映射表是 Stage 1 准入前置之一(`openspec/changes/cpptlm-cep-umbrella/tasks.md` S1-x) |
| **A14-C4** | 我方「6 类词表不另立 invariant 编号」**不影响**贵方对 `ADR-CEP-12` 的引用 —— 请确认 | |

#### 8.4.3 一条**可复现的**互斥论证(本轮新发现 M-4)

> **这不是口味之争**。R-6' 附带项要求的「扩到 6 类」在本仓**无法实现** —— 两条路都走不通:
>
> **前提**:其余消费者对 `CATEGORY_*` 只做**相等比较**或**写入固定常量**,扩到 6 类**不会**破坏它们;
> **唯一的承重约束**是 `soc_internal_noc.hh:117-118` —— 它把 `flit_category` **直接当作 `vc_counts_[]` 的桶下标**。
> 即:该扩展的代价**全部**落在「消息类 → VC 槽位」的**索引对齐**假设上。

| 路线 | 后果 | 证据 |
|------|------|------|
| **保持 `vc_counts_[4]`** | `soc_internal_noc.hh:117` 的 `const unsigned vc = (cat < VC_COUNT) ? cat : VC_SNP;` 会把 6 类中**下标 ≥ 4 的 2 类**(`atomic` / `query`,按贵方 `32 号 §6.0.1` 词表顺序编到 4/5)**静默折叠**进 `VC_SNP` 桶 —— **无报错、无计数**,该 2 类消息的流量**永久不可观测** | `soc_internal_noc.hh:116-118` |
| **扩 `vc_counts_[6]`** | 与该文件**自述**的物理模型矛盾:`soc_internal_noc.hh:10`「真实芯片: ARM CHI **4 通道** (REQ/RSP/DAT/SNP) + **4 独立 VC** → 简化抽象为 flit_category + `vc_counts_[4]`」 | `soc_internal_noc.hh:6` / `:10` |

> **更精确地说**:按贵方词表顺序(`request`=0 / `response`=1 / `snoop`=2 / `snoop_resp`=3 / `atomic`=4 / `query`=5)
> 落到 4 个 VC 桶上,**只有 `request` / `response` 2 类**能落到语义正确的桶;
> `snoop` → `VC_DAT`、`snoop_resp` → `VC_SNP` 属**错义映射**;
> `atomic` / `query` 被**静默折叠**进 `VC_SNP`。
> → 即「扩到 6 类」会同时造成 **2 类错义 + 2 类丢失**。

> **M-4(本轮新发现,独立于 R-6' 附带项)**:`soc_internal_noc.hh:68-71` 的 `flit_category → VC` 注释块
> 描述的是一个**不存在的 4 类** `CATEGORY_` 集 —— 它引用了 `CATEGORY_DAT`(全仓 **0 命中**),
> 并称 `VC_SNP = 3 (CATEGORY_CREDIT)`;而实际 `CATEGORY_CREDIT = 2`,按**索引对齐**落到 **`VC_DAT`**。
> **代码与测试一致**(`test/test_soc_internal_noc_data_flow.cc:107` 断言 `vc_counts_[2]` 增)→ **注释是错的**。
> → 该注释描述的「4 类」正是 R-6' 附带项设想的形态;**它从未被实现**。
> 本仓**不在本轮修改代码**(超出本批范围),但**登记为未闭合项**(台账 Entry #9 ④)——
> 注释与代码不一致会让下一个读者以为存在 4 类 `CATEGORY_`,从而**重复 R-6' 的同一个误判**。

> **一条可复用的佐证**:贵方 `32 号 §6.0.1` 的「明确排除」表**已经**给出与本方一致的判断 ——
> `CATEGORY_CREDIT` 被排除的理由即「**流控类别**,非一致性消息类(`noc_bundles_tlm.hh:62` 注释即『Credit 信号专用类别』)」。
> 即:**贵方自己的裁决表已经把 `CATEGORY_*` 定位为传输层类别**;
> R-6' 附带项要求把它扩成 6 类,与该表**自相矛盾** —— 我方据此判定该附带项**不可接受**(而非「两方案待选」)。

### 8.5 催问 Q-3(R-2 的判据 (b))—— 第 3 轮,一次问完

> **背景**:Q-3 = 「R-2 的判据 (b) 评估」,已 **2 轮未交付**(§8.1)。它阻塞 **A6**
> (`ADR-CEP-05 §10` 判据 2 状态收敛),且 **A4** 的路线选择也取决于它。
> 本节把 **5 个问题**一次问完并具体化,请贵方**逐条书面答复**
> (可直接在本回执对应条目下回复,或回一份逐条答复表)。

| # | 问 | 为什么必须问 | 期望答复形式 |
|---|-----|------------|------------|
| **Q-3a** | 对 `moesi_gpu` 的 `E → M`(`state_transition[2][1] = 3`):`E` / `M` 是否**接受**「导致数据值响应的转发请求」?<br>若是 —— 请给出该转发请求在 MOESI 下的**具体形态**(canonical 流号 / 消息类 / 方向);<br>若否 —— 则该转换**不满足** `INV-CEP-13-IC2` 的 (a)∧(b) 合取 | (b) 的结论直接决定 `ADR-CEP-05 §10` 判据 2 是「已触发」还是「仍不触发」 | 一句话结论 + (若「是」)形态描述 |
| **Q-3b** | 若 (b) **不成立**,贵方是否**仍**要求 `perm` 加「可静默升级」判别位?<br>(即:需求本体是否**独立于**「已命中」这一论证) | 决定 R-2 是「需求仍成立但论证需改写」,还是「需求与论证一并重估」 | 是 / 否;若「否」是否撤回 R-2 |
| **Q-3c** | `ADR-CEP-05 §10` 判据 2 的状态由**哪一侧**标注?(写进贵方 `ADR-SOC-25`,还是写进本仓 `ADR-CEP-05` 的 `## Status Update`) | 避免两侧各写一份 → 又一处双真源 | 指定一侧 |
| **Q-3d** | 贵方贡献的 2 个协议实例(`moesi_gpu` / `mesif`)进入 `ADR-CEP-11 §3` 锁表与 `07 号 §5.1` 的**时点**,是否需要与判据 2 的收敛**同步**? | 直接决定本仓 A4 的路线:等 Q-3 → A4+A6 同批;还是 A4 先行 + 限定注记 | 同步 / 可独立 |
| **Q-3e** | 贵方是否接受以 **PoC 的 S2 场景实验判定**替代 (b) 的纸面评估?<br>(即:「D-GPU 的 E 态静默升级后,D-FABRIC 读同一行 → **读到旧值还是新值**」—— **观测值**即 (b) 的答案) | **若接受 → Q-3 不再阻塞 A6**:判据由**可执行证据**裁定,而非纸面辩论<br>(`ADR-CEP-06 §3` 的 `verification_mode` 本就提供「声明 + 实验证伪」路径) | 接受 / 不接受(若不接受,请仍答 Q-3a) |

> **Q-3d 是解锁 T2 的关键一问** —— 它把「催 Q-3」从单纯的催办,升级为**一次能同时解锁 A4 与 A6 的问询**。
> **Q-3e 是第 3 轮催问的破局一问** —— 前 2 轮之所以悬空,是因为 (b) 被当成**纸面论证题**;
> 实际上它是一个**可执行的观测题**(见 §8.5.1)。

#### 8.5.1 附带提议:以 PoC 的 S2 场景实验判定替代 (b) 的纸面评估(Q-3e)

> 完整用例规格(S1/S2/S3 的输入 / 期望 / 判定)见
> [`2026-10-22-cep-poc-spike-spec.md`](./2026-10-22-cep-poc-spike-spec.md)。
> **PoC 是可丢弃的 spike** —— 不进 `ModuleFactory` / 不注册 ABI / 无公共 API,
> **只验证可组合性 + 契约自洽**,**不是 conformance test**,不得对外宣称协议合规。

**S2 摘要**(问 Q-3e 的最小充分信息):

| 项 | 内容 |
|----|------|
| **输入** | 同一缓存行 X 由 **D-GPU** 域持有,状态 `E`;D-GPU 侧对 X 发起 **store**(store 命中本地 `E` 行,MOESI `E → M`);随后 **D-FABRIC** 域对 X 发起**读** |
| **期望(若 (b) 成立)** | D-FABRIC 的读**必须**看到 D-GPU 已写的新值 → 跨域可见写成立 → `INV-CEP-13-IC2` 触发 → R-2 命中 |
| **期望(若 (b) 不成立)** | D-FABRIC 的读**看不到**该写(或该写在协议侧根本无传播路径)→ 不构成域可见写 → R-2 降级为「潜在表达力缺口,尚未证实触发」 |
| **判定** | **观测值**(读到旧值 / 新值)即为 (b) 的答案 —— **无需贵方再做纸面论证** |
| **边界** | 本 PoC **不验证正确性**,只验证「6 态 → 4 值 `perm` 投影 + 跨域桥接 + per-network VC 绑定」在真实路径上**可组合** |

> **为什么这能解开僵局**:Q-3 已 2 轮未答,而 (b) 的判定本质是「**跨域能否观测到该写**」——
> 这是一个**可执行**的问题,不需要纸面辩论。
> 若贵方接受,Q-3 的 (b) 由 S2 的观测值裁定,A6 不再等邮件。

---

## 9. 本仓行动清单(A1 ~ A15)

> 依本回执的裁定结果重新排序;**标注了冻结集影响与决策归属**。

| # | 行动 | 状态 | 冻结影响 | 归属 |
|---|------|------|---------|------|
| **A1** | 审阅三件套 + 逐条回执 R-1~R-13 | ✅ **本文件完成** | 🟢 新增实施笔记(非冻结) | ⚠️ **独立复审通道不可用** —— 本环境 subagent(`oracle`/`critic`/`context-builder`)全部返回 `Invalid token`(与台账 Entry #7 记录的通道掉线一致)→ 本次回执改为**自审 + 23 组可复现取证**(§2.0)。**这不构成独立复审**,R10 残余缺口**未闭合** |
| **A2** | 修 C-1 措辞(D11):「接口 PR」→「接口规约 PR」 | ✅ **已执行** —— `ADR-CEP-00` 追加 `## Status Update` 段(per ADR 不可变约定),台账 Entry #8 记录 `adr-amendment` | 🔴 `ADR-CEP-00` 属冻结集 → 冻结清单已重生成。<br>⚠️ 清单的 `frozen_at_commit` 字段当前指向**变更前** HEAD(本次改动未 commit)→ **commit 时需再跑一次 `--update`** 校正该字段(hash 本身已一致) | DRI 已拍板(用户明示措辞) |
| **A3** | 决定 CEP Stage 1 插件接口是否采纳 D3-a/b/c | ✅ **裁定完成**(§5 三项全采纳) —— **ADR 文本落位待 A4 同批** | 🔴 `ADR-CEP-12` / `ADR-CEP-13` | DRI 拍板 |
| **A4** | 把 `moesi-gpu-1.0` / `mesif-1.0` 落进 `ADR-CEP-11 §3` 锁表(R-13) | ⏸ **待 DRI 拍板** —— 因**连带 2 个冻结产物 + 计数 14→16**,须同批改 `ADR-CEP-11` + `07 号 §5.1` + `ADR-CEP-12` 计数口径 + umbrella `spec.md` | 🔴 4 个冻结产物 | DRI 拍板(计数口径变更) |
| **A5** | D-9 落地后同步重生成 21 个 archforge 锚点 + 重跑 alignment check | ⏸ **阻塞于 ArchForge D-9** —— 当前 D-9 未执行,21 锚点全绿 | 🟢 工作清单(非冻结) | D-9 合入后立即执行,**必须同批** |
| **A6** | 收敛 `ADR-CEP-05 §10` 的「重新评估 (β)」触发状态(R-2 已命中) | ⏸ **待 Q-3 补 (b)** —— 现阶段**不能**标「已触发」(见 §2.3) | 🔴 `ADR-CEP-05` | 待 ArchForge 回应 |
| **A7** | **M2 放行裁决** —— C-1/C-2/C-3/C-5 已闭合;C-4 判据文本满足但**机器层红** | 🟡 **待 DRI 拍板** —— **建议改为「先 A10 + A12,再放行」**(见 §6);若立即放行,须把 A10/A11/A12/M-1 写成 **Stage 1 准入前置** | 🟡 umbrella `tasks.md` / `proposal.md`(**非** M1 冻结集;`spec.md` 属冻结集,改则需 adr-amendment) | **DRI 拍板** |
| **A8** | 向 ArchForge 发**第二轮请求**(N-1 ~ N-4) | ✅ **已交付** —— `8290b4c` 已全部裁决并回写;**追加 A14**(R-6' 附带项裁定) | 🟢 回执文档(非冻结) | 已完成 |
| **A9** | 给 ArchForge 的 **D-9 锚点更新对照表**(21 锚点 ↔ 返工位置) | ✅ **已生效** —— 对方**主动**把 D-9 拆成「不含锚点的批 1」+「含全部 20 锚点的批 2」,CI 安全;本仓 33 锚点仍全绿 | 🟢 工作清单(非冻结) | 已完成 |
| **A10** | **采纳 `32 号 §6.4` 的 schema delta** —— `ordering_assumption` / `vc_binding` 各嵌一层(network 维度)+ 同步 `test_cep_schema.cc` 的结构断言 + `ADR-CEP-12` 决策 5/6 修订 + `INV-CEP-12-IC2` 按 §6.4.2 重写 | ✅ **已执行**(2026-10-22)—— 另按 oracle「活矛盾」发现**追加** `ADR-CEP-08` / `ADR-CEP-09` 的 Status Update(图顶点升为 `(协议实例, network, vc_id)`) | 🔴 5 个冻结产物 → 冻结清单已重生成 | 已完成(台账 Entry #9) |
| **A11** | **新增 fabric config 载体**(R-5' 消费端)—— `networks: { <name>: { vc_count } }` | ✅ **已执行** —— 拆为 **两份**:`fabric.schema.json`(类型)+ `fabric.json`(实例,标注 `derived_from` = ArchForge `22-nsa §0.5.4.1`,**非独立权威** per NS-R2);**不进 M1 冻结集** | 🟢 非冻结(Stage 1 再纳冻) | 已完成 |
| **A12** | **新增第 9 组检查器**:「ArchForge 协议实例声明 ↔ 我方 schema/fabric 对齐 + `vc_count` 上界 + 消息类词表」(依赖 `jsonschema` 做全量校验,**刻意不降级运行**) | ✅ **已执行** —— `scripts/test/cep_decl_alignment_check.sh`,**5 项子校验(① `jsonschema` 全量校验 13 个 required 属性 + ②~⑤ 手写 per-network 语义规则;**刻意不降级运行**),**反向验证 12/12 全绿** | 🟢 checker 自身不冻结 | 已完成 |
| **A13** | **提交本回执** → 闭合 ArchForge `ADR-SOC-25` 的 **V-8**(「CEP 侧逐条回执仍待」) | ✅ **已完成**(2026-10-22)—— 回执已随 commit `97ecbf9d` 提交 → **V-8 已闭合** | 🟢 回执文档(非冻结) | 已完成 |
| **A14** | **答复 R-6' 附带项**:不扩 `CATEGORY_*`(M-1)—— 6 类词表落 CEP Coherence Interface 层,`CATEGORY_*` 保持传输层,二者用显式映射表 | ✅ **已发出**(2026-10-22)—— 回执 **§8.4**:4 条已定 + 4 条待确认 + **M-4 互斥论证** | 🟢 回执文档(非冻结) | 已交付,待 ArchForge 确认 |
| **A15** | **催 Q-3**(R-2 的判据 (b))—— 至今 2 轮未交付,阻塞 A6 | ✅ **已发出**(2026-10-22)—— 回执 **§8.5**:**Q-3a~e 五问**,含 Q-3e「以 PoC S2 实验判定替代 (b) 纸面评估」 | 🟢 回执文档(非冻结) | 已交付,待 ArchForge 答复 |

> **A4 / A6 的推进条件**:两者都改冻结契约,须走 §11 台账 `adr-amendment` entry + 冻结清单重生成。
> **建议与 A3 合并为一次 adr-amendment**(同批改 `ADR-CEP-05` / `ADR-CEP-11` / `ADR-CEP-12` / `ADR-CEP-13` / `ADR-CEP-15` / `07 号`),
> 避免多轮冻结漂移。

---

## 10. 执行记录(批 1,2026-10-22)

> **台账 Entry #9**(`.rddf/roadmap/objectives/obj-cep-platform.md §11`)是本批的**授权记录**,
> 含 ①变更的冻结产物清单 ②**RUN 证据**(非 skip 的完整输出)③**反向验证 12/12** ④遗留声明。
> **计划与实施之间的 5 处偏差**记录在 `.opencode/plan/20261022-cep-contract-convergence-batch1.md §9`。

| 批 1 产出 | 位置 |
|-----------|------|
| 第 9 组检查器(5 项子校验;① 用 **jsonschema** 全覆盖 13 个 required 属性,②~⑤ 手写语义规则) | `scripts/test/cep_decl_alignment_check.sh` |
| **反向验证脚本(入库,可复现)** | `scripts/test/cep_decl_reverse_verify.sh` |
| schema delta(2 字段各嵌一层 network + 内层 `propertyNames` 词表约束) | `include/tlm/cep/protocol_instance.schema.json` |
| fabric 载体(**两份**:类型 + 实例) | `include/tlm/cep/fabric.schema.json` · `include/tlm/cep/fabric.json` |
| **4 份** ADR 的 Status Update(含 `ADR-CEP-09` 图顶点升为 `(协议实例, network, vc_id)`) | `docs/adr/ADR-CEP-{08,09,12,17}-*.md` §8 |
| umbrella spec 的 IC2 scenario 改写 | `openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` |
| lint 第 9 组 + ctest 注册(`SKIP_REGULAR_EXPRESSION` 使「跳过」显示为 Skipped 而非 Passed) | `scripts/test/cep_docs_sync_check.sh` · `test/CMakeLists.txt` |
| M1 冻结机制加固(`--update` 授权闸门 + `authorized_by` 字段 + provenance 自检) | `scripts/test/cep_freeze_check.sh` |
| 测试改写(`resolve_named_networks` → `resolve_ordering_assumptions`;resolver 增消费 `vc_binding` → IC2 ①③ 有 C++ 覆盖) | `test/test_cep_schema.cc` |
| CI 补装 `jsonschema`(检查器**刻意不降级运行**,缺依赖即报错退出) | `.github/workflows/ci.yml` |

**2026-10-22 对抗式复审(critic)处置**:REJECT,7 个 BLOCKER + 10 个 SHOULD_FIX —— **全部已修并复验**:
B1 `fabric.json` 违反自身 schema(补声明 3 个注记字段 + 加机器校验)· B2 检查器只校验 2/13 字段(改用 jsonschema 全覆盖,
CI 补装 `jsonschema`,**刻意不降级运行**)· B3 声明解析失败静默丢弃 + 硬编码份数(改为解析失败即红 + 按 id 集判定)·
B4 冻结清单 provenance 造假(加 provenance 自检 + 两步 commit 流程)· B5 `--update` 无授权闸门(加 `--authorized-by`)·
B6 `ADR-CEP-17` 仍持旧 fail 判据(补 `## 8 Status Update`)· B7 反向漂移守卫可静默关闭(权威源解析失败即判红)。
反向验证由 9 条扩到 **12 条**(新增 3 条钉住 B2/B3/B7 三个可绕过缺口),**脚本已入库**。

**批 1 未能闭合的**:
① **M-1 的对外答复**(A14)—— ✅ **已于第 2 轮交付**(见 §10.2);② **Q-3** 仍未交付(2 轮)→ A6 阻塞;
③ **A4**(R-13 锁表 + 计数 14→16)待批;④ **D-9 批 2** 的 20 个锚点待同批重生成;
⑤ `fabric.*` 待 Stage 1 首个 implemented 实例跑过后**再纳 M1 冻结集**;
⑥ **真人外部评审仍缺**(R10 未闭合)—— 本批的独立复审由 oracle + metis + explore 三方 agent 完成,
**不等于真人评审**。

### 10.1 ✅ M2 准入门已放行(2026-10-22)

**判据 C-1 ~ C-5 全部满足**(逐条证据见台账 **Entry #10**):

| 判据 | 结论 |
|------|------|
| C-1 接口规约文档已合入 | ✅ `6020f42` + `8290b4c` + `ac956b8` |
| C-2 P0-1~P0-5 逐项可追 | ✅ 本回执 §3 已签收 |
| C-3 6 问书面结论 | ✅ 本回执 §4/§5 已签收 |
| C-4 schema ↔ 声明双向映射 | ✅ **机器化**(5 项子校验 + 反向验证 12/12) |
| C-5 反向验证全绿 | ✅ 声明对齐 **12/12** + `ctest` **94/94** · `ctest -R cep` **16/16** · lint **9/9** · 冻结 **29/29** · 锚点 **33/33** |

> ⚠️ **计数漂移说明**:上表的 `lint 9/9` 是 **M2 gate 当日(第 1 轮)**的实测值。
> 2026-10-22 第 2 轮**新增 lint 第 10 组**(传输层 `CATEGORY_*` 分层纪律 = A14-1 / A14-2 的机器化)
> → 现为 **10/10**。`ctest -R cep` 仍为 **16/16**(本轮未新增 ctest)。

**连带**:`cpptlm-cep-umbrella` 🟡 Proposed Draft → 🟡 **Proposed**;objective §5.2 baseline **0/3 → 1/3**;
`tasks.md` 新增 **5 项 Stage 1 准入前置**(S1-1 ~ S1-5),把本批的 provisional 债落成可勾选条目。

> ⚠️ **Stage 0 仍不 archive** —— **唯一**未闭合项是 **R10**(真人外部评审);
> 放行记录已显式 **risk-accept「未经真人复审」**,不存在默认放行。

### 10.2 ✅ 第 2 轮对外交付:A14 正式答复 + A15 催 Q-3(2026-10-22)

> **本轮性质**:**对外动作批**(零决策成本)—— 不改任何**冻结产物**,不需 `adr-amendment`。
> **交付载体**:本回执 v1.1(§8.4 + §8.5)。

| 产出 | 位置 | 说明 |
|------|------|------|
| **A14 正式答复** | 本回执 **§8.4** | 4 条**已定**(A14-1~4)+ 4 条**待确认**(A14-C1~C4)+ **M-4 互斥论证** |
| **A15 催 Q-3** | 本回执 **§8.5** | **Q-3a~e 五问**;含 **Q-3e**(= P-3)「以 PoC S2 实验判定替代 (b) 纸面评估」 |
| **PoC 用例规格** | `docs/implementation/2026-10-22-cep-poc-spike-spec.md` | **P-2** —— S1/S2/S3 的输入 / 期望 / 判定;**不含实现** |

**本轮修复并机器化 1 项(M-4)** —— `soc_internal_noc.hh:68-71` 的 `flit_category → VC` 注释块描述了一个
**不存在的 4 类** `CATEGORY_` 集(引用 `CATEGORY_DAT`,全仓 **0 命中**),且称 `CATEGORY_CREDIT → VC_SNP`,
而实际按**索引对齐**落到 `VC_DAT`(**代码与测试一致 → 注释是错的**)。
> ⚠️ **不是新发现** —— `docs/research/gem5-ruby-protocol-bridge-survey.md:597` 早已记载
> 「实测只有 3 类 CATEGORY,缺 CATEGORY_DAT,**2026-10-16 critic 校正**」;本轮是**重发现 + 修复 + 机器强制**。
→ 该注释正是 R-6' 附带项设想形态的「**幽灵实现**」;**本轮已修** + 新增 lint 第 10 组封死复发
(3 项子校验;4 个绕过点已实测堵住:花括号初始化 / 表达式初始化 / `using namespace` + 裸类别名)。

**本轮有意未做**:① 冻结产物**零改动** → 无需 `adr-amendment`、无需重生成冻结清单;
② **台账 Entry #11**(独立成条 —— 本轮含代码改动 + 机器契约);T2(A4)的 adr-amendment 顺延为 **Entry #12**;
③ **P-1**(PoC 登记为 Stage 1 第 7 个 change)待 DRI 拍板(排队口径变更,与 A4 的 D2 同类)。

---

## 11. 诚实边界

1. 本回执的裁定**仅覆盖 ArchForge 三份答复文件所声明的内容**;`ADR-SOC-25` 自身状态为「📋 Proposed」,
   9 份文档返工清单**尚未执行** → 结论在返工落地前均为**意图级**,不是实现级;
2. §2.0 的 **23 组取证**全部在 **CppTLM 仓**(代码 + 冻结契约)侧完成;
   **未**对 **ArchForge 仓自身的 `file:line`** 做独立校验 —— 该侧由 ArchForge 自查;
3. §2.1 的「ArchForge 8 项发现成立」= **我方复核未发现反证**,不等于我方逐条重做了 ArchForge 的取证;
4. 本回执**不构成 M2 放行**;M2 仍由 `ADR-CEP-00 §2 决策 3` 的 DRI 人工 gate 裁决,
   且 C-1 **仍红**(ArchForge 三份规约文档尚未提交);
5. R-2 / R-3 / R-13 的具体落点**尚需经 §11 台账 `adr-amendment`** 落进冻结契约 ——
   本回执的裁定是**设计意图**,不是已生效契约;
6. ⚠️ **本回执未经独立复审** —— 本环境 subagent 通道全部不可用(`Invalid token`)→
   复审强度**弱于**原生 oracle/critic,与台账 Entry #7 登记的通道掉线一致 →
   **R10(独立性缺口)残余未闭合**,不得表述为「已独立验收」。

---

**版本**: v1.1 · **日期**: 2026-10-22
**修订**: v1.1 = 第 2 轮对外动作 —— 新增 §8.4(A14 正式答复)+ §8.5(A15 催 Q-3,含 Q-3e)+ §10.2(交付记录);
A13 / A14 / A15 状态更新;「M1 冻结集 28 → **29** 产物」口径校正。**v1.0 的实质裁定内容未改**。
**发出方**: CppTLM CEP DRI(builder mode) · **接收方**: ArchForge coherence owner
**关联**: [`2026-10-21-cep-archforge-interface-alignment.md`](./2026-10-21-cep-archforge-interface-alignment.md) ·
[`2026-10-21-cep-stage0-contracts.md`](./2026-10-21-cep-stage0-contracts.md) ·
`.rddf/roadmap/objectives/obj-cep-platform.md §11`
