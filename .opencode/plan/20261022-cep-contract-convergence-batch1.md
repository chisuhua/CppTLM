# 执行计划：CEP 契约侧收敛批 1（A10 + A11 + A12 + A14）

> **日期**: 2026-10-22 · **作者**: CppTLM CEP DRI（build agent）
> **状态**: 📋 **待 DRI 拍板**（已过 oracle + metis + explore 三方评审并据此修订，v2）
> **范围**: CppTLM 仓内契约演进（不含 ArchForge 侧动作）
> **上游依据**: `docs/implementation/2026-10-21-cep-archforge-response-receipt.md`（§6 M2 结论 / §8.3 M-1~M-3 / §9 A10~A15）
> **跨仓输入**: ArchForge `8290b4c`（N-1~N-4 裁决 + `32 号 §6.4` schema delta）+ `ac956b8`（D-9 批 1）
> **v2 修订**: 经 oracle（架构/契约自洽性）+ metis（缺口/边界/slop）+ explore（事实核对）三方评审，修订 12 处，**范围显著收窄**，见 §8

---

## 0. 一句话

把 ArchForge 交付的 **per-network schema delta** 落到本仓**冻结契约**，
让 M2 判据 **C-4 从「人工满足」变成「机器满足」**——从而在**不掩盖 P0** 的前提下放行 M2。

**v2 的核心变化**：形态采纳不变，但**补上 3 处会让"契约收敛"名不副实的活矛盾**（`ADR-CEP-08` / `ADR-CEP-09` / 死锁图顶点），
**砍掉 3 处会把"声称可自动化"变成"又一次假牙"的自加项**（白名单 / schema 内词表 / 过早冻结 fabric），
并把**「检查器真的跑过」变成可出示的证据**（RUN 留痕）而不是 CI 里的一个绿色 exit code。

---

## 1. 背景与动机

### 1.1 现状（2026-10-22 实测）

| 项 | 状态 |
|----|------|
| ArchForge 侧 | `8290b4c` + `ac956b8` 已提交；N-1~N-4 **全部裁决**；Q-2 **已交付**；Q-3 **仍未交付** |
| C-1 / C-2 / C-3 / C-5 | ✅ 全部闭合 |
| C-4 | 🔶 **判据文本满足**（13/13 字段有承载物）**但机器层红** —— 对方声明已切**嵌套形态**，我方 schema 仍**扁平** → 实测**每份 4 处失配** |
| C-4 的机器检查 | ❌ **从未实现**（8 组 lint + alignment check 都不跑声明↔schema 映射）→ 失配**静默存在** |
| D-9 | 批 1 已落地（不含锚点的 2 份）→ 本仓 **33 锚点全绿**；批 2 含全部 20 锚点，待同批 |

### 1.2 为什么现在做

C-4 是 5 条放行判据中**唯一声称「✅ 可自动化」**的一条。若在「检查器缺失 + schema 与声明已失配」的状态下放行 M2，
等于用人工核验给一条**声称可自动化却未实现**的判据结案 —— 这正是 R10（独立性缺口）要防的事。

### 1.3 本批**不**解决（明确排除，防范围蔓延）

| 排除项 | 归属 | 理由 |
|-------|------|------|
| **A4**（R-13 锁表增 2 行 + `07 号 §5.1` + 计数 **14→16**） | 单独批 2 | 动的是**族/实例计数口径**，与本批「schema 结构」性质不同 |
| **A6**（`ADR-CEP-05 §7` 触发状态） | 单独批 3 | 阻塞于 **Q-3**（R-2 判据 (b)），跨仓未交付 → 绝不能捆绑 |
| **A5**（D-9 批 2 后的 21 锚点重生成） | 阻塞于 ArchForge 批 2 | — |
| **Stage 1 插件接口实现** | 后续 change | — |
| `CATEGORY_*` 扩展 | ❌ **明确不做**（M-1 裁定，见 §2.4） | 词表混层 |
| **`protocol_version` ∈ 锁表校验** | ❌ **本批不做**（v2 收窄） | 属 A4 批次；校验器内嵌白名单 = M-2 假阴性家族的同款结构（oracle 最小充分集） |
| **schema 内 6 类词表 `propertyNames`** | ❌ **本批不做**（v2 收窄） | 放进**检查器**即可达到同等机器可验收，不必第一刀就冻进 schema |
| **`fabric.schema.json` 进 M1 冻结集** | ❌ **本批不做**（v2 收窄） | 零实例新产物不该诞生即冻结；等 Stage 1 首个 implemented 实例跑过再纳冻（oracle Q4） |

---

## 2. 设计方案

### 2.1 总体形态

```
                    ┌─────────────────────────────────────────────┐
  ArchForge 侧     │ 32 号 §6.4 schema delta（diff 级，已交付）    │
  （只读输入）      │ 32 号 §6.0.2 per-network VC 矩阵             │
                    │ 22-nsa §0.5.4 网络清单 + vc_count（单点权威） │
                    └───────────────────┬─────────────────────────┘
                                        │ 采纳（provisional 标注）
                    ┌───────────────────▼─────────────────────────┐
  本仓 A10          │ protocol_instance.schema.json                │
                    │  · ordering_assumption → {network: {vc: ord}}│
                    │  · vc_binding         → {network: {cls: vc}} │
                    │ ADR-CEP-12 决策 5/6 修订 + IC2 重写          │
                    │ ADR-CEP-08:131,164 修订（key 粒度）          │
                    │ ADR-CEP-09:75,77,92 修订（死锁图顶点）       │
                    │ umbrella spec.md IC2 scenario（:249-254）    │
                    └───────────────────┬─────────────────────────┘
                    ┌───────────────────▼─────────────────────────┐
  本仓 A11          │ include/tlm/cep/fabric.schema.json（新增）   │
                    │  → 类型描述（networks / vc_count）           │
                    │ include/tlm/cep/fabric.json（新增）         │
                    │  → 实例数据 compute_noc/ualink_fabric:4      │
                    │    （derived-from 22-nsa §0.5.4.1）          │
                    └───────────────────┬─────────────────────────┘
                    ┌───────────────────▼─────────────────────────┐
  本仓 A12          │ scripts/test/cep_decl_alignment_check.sh    │
                    │  ⚠️ 零外部依赖（手写校验）—— **已被 §9.8 D-6 推翻** │
                    │  ① 声明 ↔ schema 结构（手写，仅 2 字段）    │
                    │  ② vc_id < vc_count + 网络键对称             │
                    │  ③ ordering ⊆ vc_binding（per network）      │
                    │  ④ 消息类 ∈ 6 类词表                        │
                    │  ⑤ network ⊆ fabric + 反向 vc_count 比对    │
                    │  输出 [RUN]/[SKIP] 显式区分 + RUN 留痕       │
                    └─────────────────────────────────────────────┘
```

### 2.2 关键设计决策（需 DRI 拍板）

#### D1 · 检查器形态 → **A（不变）**

独立脚本 `cep_decl_alignment_check.sh` + **独立 ctest** + 被 `cep_docs_sync_check.sh` **第 9 组**调用。
与既有 `cep_archforge_alignment_check.sh` 同款模式（:30-34 优雅跳过 / :23 `ARCHFORGE_DIR` / :36 python heredoc）。

> **v2 变化**：第 9 组**去掉「双向」命名** → 改「ArchForge 协议实例声明 ↔ 本仓 schema/fabric **对齐检查**」。
> 实际校验方向是「声明 ⊆ 我方」单向；反向（我方 fabric 多了网络）判为 **warning 不阻断**。

#### D2 · fabric config 载体 → **A + A'（v2 拆分，Blocker）**

| | 产物 | 内容 | 冻结 |
|---|------|------|:----:|
| **A'** | `include/tlm/cep/fabric.schema.json` | **类型描述**：`networks: { <name>: { vc_count: int } }` | ❌ 不进 M1（v2） |
| **A** | `include/tlm/cep/fabric.json` | **实例数据**：`compute_noc: {vc_count: 4}` / `ualink_fabric: {vc_count: 4}`，附 `derived_from: 22-nsa §0.5.4.1` | ❌ 不进 M1（v2） |

> **v2 根因**：v1 计划把 A11 写成**一份** `fabric.schema.json`，但 Step 1（读 `networks[n].vc_count`）
> 与 Step 7（把某网络 vc_count 从 4 改成 2）都把它当**实例数据**读 —— 从类型 schema 里读不到
> `compute_noc` 的 vc_count。两种合理解释 → 两种完全不同的实现。**拆成 schema + 实例两份**。
> `vc_count` 的 `maximum` 与 schema 的 `vc_id maximum: 3` 对齐 → **4**（不是 8），避免两份冻结产物互相矛盾（metis 边界 1）。

#### D3 · 6 类消息类词表落点 → **A'（v2 改）**

| | 方案 | 裁决 |
|---|------|------|
| ~~A~~ | schema 内层 `propertyNames: {enum: [...]}` | ⏸ **本批不做**（推迟） |
| **A'（推荐）** | **检查器第 ④ 项**校验内层 key ∈ 6 类词表 | ✅ **本批做** |
| B | 只写 `ADR-CEP-12` 文本 | ❌ 机器不可验收 |
| C | 新增 C++ 头 | ❌ 冻结集再增 1 项，且不参与 JSON 校验 → 仍是「两处词表」 |

> **理由（oracle）**：`32 号 §6.4.1` 的 delta **不含** `propertyNames`（explore 已证实），该 enum 是本仓自加项；
> 放进检查器可达到**同等机器可验收**，且**不扩大冻结面**。词表出处仍在 ArchForge `32 号 §6.0.1`（含 3 条排除项的理由）→
> 我方**引用而不重复定义**（同 `22-nsa §0.5.2` 纪律）。

#### D4 · 批次划分 → **A（不变）**

批 1 = A10 + A11 + A12 + A14；批 2 = A4；批 3 = A6（被 Q-3 阻塞）。
A6 跨仓未交付 → **绝不能与本批捆绑**。

#### D5 · schema 里 `vc_id` 的上界 → **A（不变）**

保留 `minimum: 0, maximum: 3`；严格上界由**检查器按 fabric 实例的 `vc_count` 判**。
两层防线：schema 管「取值域合法性（0-3）」，检查器管「该网络实际槽位上界（< vc_count）」。
`fabric.json` 的 `vc_count` 上界同步取 **4**（与 schema 对齐；>4 时两处同步改）。

#### D6 · **新增** · 「机器满足」的证据形态 → **A**

> **v2 根因（metis 隐藏意图 1 + oracle 最小充分集 1）**：检查器依赖 `ARCHFORGE_DIR`（默认 `/workspace/main/ArchForge`），
> **CI 环境无 ArchForge → 脚本 exit 0 跳过** → 「C-4 机器满足」实际退化为「在 DRI 本机双仓环境下跑过一次」，
> **与 M-2 的病根在证据层面同构**。

| | 方案 | 裁决 |
|---|------|------|
| **A（推荐）** | 脚本输出**显式区分** `[RUN]` / `[SKIP]`；lint 第 9 组把 SKIP 打成 `SKIPPED(archforge absent)` 而非 ✓；**AC-1 拆两条**；一次非 skip 的 **RUN 完整输出留痕进台账 Entry #9**，M2 gate ④ 证据块引用该 entry | ✅ |
| B | 只在文档里写「检查器已实现」 | ❌ 证据不可出示 |

#### D7 · **新增** · 嵌套形态的「定死 vs provisional」→ **A**

> **v2 根因（metis 隐藏意图 2 + oracle Q4）**：对方两份声明 `status` 均为 `planned`、`protocol_version`
> **自认不在锁表**、`32 号 §6.0.3` 明说死锁自由性「归 CEP 侧机器校验（ArchForge 侧不背书）」；
> Stage 1 **零实现、零实例跑过这个形状**。把它写进冻结 schema = 用一个尚未定稿的跨仓形状再制造一次冻结漂移。

| | 方案 | 裁决 |
|---|------|------|
| **A（推荐）** | 在 `ADR-CEP-12` 决策 5 修订文本中显式标注：**「Stage 0 跨仓对齐形态（provisional）；首个 `implemented` 实例装配后 Stage 1 复核」** | ✅ |
| B | 不标注，直接定死 | ❌ |

> **provisional ≠ 免台账**：每次演进照常走 adr-amendment + 台账 entry（这正是 M1 的服务方式 —— 防静默自我改写，不防演进）。

#### D8 · **新增** · 跨仓权威与「双真源」→ **A**

> **v2 根因（oracle 风险 3）**：ArchForge 已用 **NS-R2** 宣告「`network_id` 单点权威，增删走 ADR」——
> 权威在**对方**。本仓若落第二份 `networks/vc_count` 数据而**只查单向 ⊆**，则
> 「ArchForge 改了清单而我仓没跟」永远不会被发现 → `vc_count` 静默漂移 → 条件② 用过期上界通过 →
> **又一条「声称有牙、实则假牙」的链路**。

| | 方案 | 裁决 |
|---|------|------|
| **A（推荐）** | 我仓 `fabric.json` **显式标注 `derived_from: 22-nsa §0.5.4.1`**（非独立权威）；检查器第 ⑤ 项加**反向比对**（我方 `vc_count` == ArchForge 清单值）；架构分工写进 `ADR-CEP-12` | ✅ |
| B | 我仓 fabric 是独立权威 | ❌ 单方面否定 NS-R2，需一次显式跨仓裁决，本批默认滑过去 = 职责边界裂缝 |

### 2.3 排序假设字段的语义（oracle 发现，v2 新增修订）

oracle 指出：`ordering_assumption` 实际**同时承载两种东西**——
① 对 fabric 的**事实陈述**（VC k 是否保序 = 物理属性，两个协议实例的 ordering map 逐字节相同）；
② 协议的**需求**（我的 response 类必须走保序 VC = ProtoGen/SLICC 语义，`.pcc` 的 `Network {}` 块）。

现状按实例复制 ①，靠 `ADR-CEP-09` 前提 3（声明与 fabric 实际行为一致）兜底 → N 实例 × M 网 = N×M 份需互相同步的拷贝。

> **v2 处置（不改形态，只改措辞）**：在 `ADR-CEP-12` 决策 5 修订文本里，把
> 「排序是 VC 的静态属性」改写为
> **「排序是协议对所用 (network, vc_id) 的保序需求 + fabric 侧担保，一致性由装配期校验承担」**。
> **理由**：形态可以妥协（不重开 N-2 谈判），**语义归属不能在冻结 ADR 里写错**。

> **登记备选（不实施）**：oracle 指出更根本的正解是 **SLICC 式「协议本地虚拟网络 + 装配期绑定」**
> （协议声明只带本地 vnet + ordering，另设装配绑定产物承载 (协议, vnet) → (network, vc_id)），
> 可消除跨实例重复、把前提 3 变强校验。**代价** = 推翻 N-2 已交付形态，跨仓谈判重开。
> → **本批不实施**，但在 `ADR-CEP-12` 修订记录里**显式登记为已知备选**，
> 触发条件 =「Stage 1 出现第 3 张网或第 3 个协议实例」——现在是（0 implemented 实例）将来最便宜的改点，
> 别让这个选项无声死掉。

### 2.4 M-1 裁定：不做 `CATEGORY_*` 扩展（已定，非选项）

ArchForge R-6' 的 CppTLM 附带项要求「`CATEGORY_*` 从 3 类扩到覆盖 6 类一致性消息类」。**我方不接受**：

- `flit_category` 是**链路层 flit 类别**（`soc_internal_noc.hh:10` 自述「简化抽象为 flit_category + vc_counts_[4]」）；
- 实际消费者：`bidirectional_port_adapter.hh:235`（credit 回信）、`soc_internal_noc.hh:118`（`++vc_counts_[vc]`，下标是
  `(cat < VC_COUNT) ? cat : VC_SNP` **钳位后**的值 —— 已有饱和保护，说明该字段本就被当作「VC 近似量」用，扩到 6 类会与之冲突）、
  `sdma_noc_adapter.hh:57`；
- 扩到 6 类 = 把**一致性语义搬进传输层**（词表混层）。

**替代落点**：6 类词表落 **CEP Coherence Interface 层**（检查器第 ④ 项，D3-A'）；
`CATEGORY_*` **保持 3 类不变**；两者关系由 adapter 处**显式映射表**承担（**非恒等**）——
映射表形态属 Stage 1，但**须挂进 Step 9 的 Stage 1 任务**（metis：否则裁定只活在答复文本里）。

---

## 3. 实施步骤

> 冻结集内的产物在**最后一步**才动（先写检查器跑红 → 再改 schema/ADR 跑绿 → 最后重生成冻结清单），
> 避免「先改冻结面 → 冻结校验红 → 不知道是哪个改动引起」的排查成本。

### Step 1 — 建检查器（此时**必红**，是预期）

**新增** `scripts/test/cep_decl_alignment_check.sh`

**规格（v2 修订）**：

| # | 要求 | 依据 |
|---|------|------|
| 1 | 形态仿 `cep_archforge_alignment_check.sh`：:23 `ARCHFORGE_DIR` 默认值 / :30-34 **缺席 → 打印 `[SKIP]` + exit 0** / :36 `python3 - "$ROOT" "$ARCHFORGE" << 'PYEOF'` 内联 | 既有脚本 |
| 2 | ⚠️ **零外部依赖** —— **已被 §9.8 D-6 推翻**(B2:手写子集导致 11 个 required 属性零校验 = 「声称有自动化、实则没有」的变体)。~~**不用 `jsonschema` 库**,手写校验~~ → 现为「schema 层用 `jsonschema` 全覆盖 + 语义层手写」,pip 行**前置到 ctest 之前**且**覆盖所有 matrix 腿** |
| 3 | 从 ArchForge `32 号` 提取 ```json fence → **块数 ≠ 2 → exit 1**（非法跳过，不当「找不到就跳过」）| metis 边界 3 |
| 4 | 校验 **①**：手写校验 2 份声明的 `ordering_assumption` / `vc_binding` **嵌套结构**（network → {key → 值}）；不写通用 JSON Schema 引擎 | 同上 |
| 5 | 校验 **②**：读本仓 `include/tlm/cep/fabric.json` → `networks[n].vc_count`：<br>· `vc_binding[n]` 每个 `vc_id` **<** `vc_count`<br>· **网络键对称**：`set(ordering_assumption.keys()) == set(vc_binding.keys())`（per `32 号 §6.2` 纪律）<br>· `ordering_assumption[n]` 的内层 key **一律 `int()` 解析**；解析失败 → **该项红**（不崩栈）<br>· 解析后每个 vc_id **∈** `vc_binding[n].values()`<br>· **同一 vc_id 可绑多个消息类合法**（per `32 号 §6.0.2` ualink VC3 = response + snoop_resp + forward_data）→ **不做唯一性约束** | metis 边界 2/4/5 + ArchForge §6.0.2 |
| 6 | 校验 **③**：`ordering_assumption[n]` 每个 vc_id **∈** `vc_binding[n].values()` | ArchForge §6.4.2 ① |
| 7 | 校验 **④**：`vc_binding[n]` 每个消息类 **∈** 6 类词表 `{request,response,snoop,snoop_resp,atomic,query}` | ArchForge §6.0.1 |
| 8 | 校验 **⑤**：network key **⊆** `fabric.json` 的 networks 键集（NS-R1）**+ 反向比对**：我方 `vc_count` == ArchForge `22-nsa §0.5.4.1` 清单值（反向差异 → **warning 不阻断**，但输出醒目）| D8 |
| 9 | **输出格式**：**首行必含 `[RUN]` 或 `[SKIP]`**；RUN 时逐项输出 ①~⑤ 的 ✓/✗ 与计数；退出码非零当且仅当有 ✗ | D6 |
| 10 | **不做** `protocol_version` ∈ 锁表校验（v2 收窄，归 A4 批）——**避免在检查器里内嵌白名单** | oracle 最小充分集 |

**验收**:
- `ARCHFORGE_DIR=/workspace/main/ArchForge bash scripts/test/cep_decl_alignment_check.sh` → **红**，
  且红的原因**只有**「声明 vs 扁平 schema 的 4 处失配」；
- `ARCHFORGE_DIR=/nonexistent bash ...` → 打印 `[SKIP]` 且 exit 0。

### Step 2 — 写测试（先红）

**改** `test/test_cep_schema.cc`。v1 计划**低估了改写面** —— 实测以下位置**都携带扁平语义**，必须一并处理：

| 位置 | 内容 | 处置 |
|------|------|------|
| `:52-56` | `using NetworkMap = std::map<std::string, std::set<int>>` + `resolve_named_networks()`（`:46-91`）| **重写为嵌套语义**（network → 内层 vc_id 子键）；`NetworkMap` 注释指向新的 `fabric.json` 实例形态 |
| `:132-133` | `minimal_valid_instance()` 用扁平形态造实例 | 改为嵌套形态 |
| `:220-233` | `ordering_assumption` / `vc_binding` 结构断言 | **改写**为断言嵌套形态（这是计划点名的那个） |
| `:377-395` | IC2 **正向**用例「named_network 解析到唯一 vc_id 时必须 pass」 | 改写为 per-network 语义 |
| `:396-438` | IC2 **反向**用例「named_network 不可解析到唯一 vc_id 时必须 fail」 | 改写为 per-network 语义（**保留反向用例** —— 它是「有牙」的另一半） |
| `:147` | 测试**标题**写「schema 必含 8 个字段组」，但正文实际断言 13 个属性 | **标题口径修正为 13 属性**（权威分组在 `:268` 的「7 组 / 13 属性」用例）—— 既有 cosmetic 漂移，顺手修 |
| **新增** | `fabric.schema.json` / `fabric.json` 的断言用例 | 新增 |

**不动**：`[cep][schema][negative] schema 不得出现协议标识符(C9 负向断言)`（`:358`）——
C9 是**按字段名**做的，嵌套化不改字段名 → **机制不受影响**（oracle 确认）。
**在 AC 里显式锁死这条用例零改动且仍绿**，防止执行者「顺手同步」负向断言。

**验收**: `ctest -R cep_schema` → **红**（尚未改 schema）。

### Step 3 — 采纳 schema delta（跑绿）

**改** `include/tlm/cep/protocol_instance.schema.json`：

> **v2 澄清**：以下为**结构示意**；执行时以 **ArchForge `32 号 §6.4.1` 的 diff 原文为准逐字采纳**（:844-871），
> 不加 `propertyNames`（D3-A' 推迟）、不加内层 key 约束（由检查器 ②/④ 兜）。

```
ordering_assumption:
  additionalProperties: { type: object, minProperties: 1,
                          additionalProperties: { enum: [p2p_ordered, unordered] } }
vc_binding:
  additionalProperties: { type: object, minProperties: 1,
                          additionalProperties: { type: integer, minimum: 0, maximum: 3 } }
```

**新增** `include/tlm/cep/fabric.schema.json`（类型描述，`vc_count` 上界 **4** 与 schema 对齐）：

```
{ "$schema": "...draft/2020-12",
  "title": "CEP fabric config schema —— 网络命名空间与每网络 VC 槽位上界",
  "type": "object", "additionalProperties": false, "required": ["networks"],
  "properties": { "networks": {
      "type": "object", "minProperties": 1,
      "propertyNames": { "pattern": "^[a-z0-9][a-z0-9_]*$" },
      "additionalProperties": { "type": "object", "additionalProperties": false,
        "required": ["vc_count"],
        "properties": { "vc_count": { "type": "integer", "minimum": 1, "maximum": 4 } } } } } }
```

**新增** `include/tlm/cep/fabric.json`（**实例数据**，非独立权威 → 显式 derived-from）：

```
{ "networks": {
    "compute_noc":   { "vc_count": 4 },
    "ualink_fabric": { "vc_count": 4 } },
  "derived_from": "ArchForge 22-nsa-fabric-address-spec.md §0.5.4.1 + 32 号 §6.0.2",
  "authority_note": "本文件是 ArchForge 单点权威的**派生副本**，非独立权威（per ADR-CEP-12 决策 5 修订 / NS-R2）" }
```

**验收**: `ctest -R cep_schema` → **绿**。

### Step 4 — 同步冻结契约（ADR ×4 + spec）

> **v2 根因（oracle 契约自洽性）**：v1 计划只改 `ADR-CEP-12` + spec.md，
> 而**冻结集内另有 2 份 ADR 携带旧扁平语义** → 「契约收敛」会名不副实（只是把自己的 schema 收敛到对方形态，冻结集内部反而更不收敛）。

| 文件 | 修订 | 依据 |
|------|------|------|
| `ADR-CEP-12` | 决策 5 修订（key 从 `<vc_id \| named_network>` → `network`；值升为 `{vc_id: ordering}`；**理由措辞按 §2.3 改写**；**provisional 标注按 D7**）<br>决策 6 修订（`vc_binding` 升逐网络）<br>**`INV-CEP-12-IC2` 重写**（按 `32 号 §6.4.2` ①②③）<br>**新增 `INV-CEP-12-IC4`**（6 类词表 + 3 条排除项，理由引 `32 号 §6.0.1`）<br>**修订记录登记** SLICC vnet 备选（§2.3） | ArchForge delta + oracle |
| **`ADR-CEP-08`** | **`:131`**（key 粒度：逐网络/逐 vc_id → 必须解析到**唯一** vc_id）<br>**`:164`**（`ordering_assumption` key 解析）<br>→ 嵌套形态下 network → **多个** vc_id，「解析到唯一 vc_id」这个**谓词整个消失** → 改为 per-network 语义 | oracle 活矛盾 ① |
| **`ADR-CEP-09`** | **`:75`**（**图顶点定义**：`(协议实例, vc_id)` → `(协议实例, network, vc_id)`）<br>**`:77`**（顶点来源：声明导出）<br>**`:92`**（`ordering_assumption` per-VC 消费描述）<br>→ 这是**数学对象的变更**，不是措辞：两张网共享 vc_id 值域 0-3，顶点不带 network ⇒ 跨网依赖边被合并 ⇒ 无环判据可能给**错误 verdict** | oracle 活矛盾 ② |
| `umbrella spec.md` | **仅** `:249-254` 的 IC2 scenario 重写为嵌套形态<br>`:163` / `:223` / `:239` / `:331` 为**按名引用**，经 grep 确认无形态残留即可，**不动文** | metis AI-slop 4 + explore 14b |
| `07 号` | **零 diff**（`:148` Fabric 职责、`:588` 已预见嵌套形态，无需改）—— 在 AC 里锁死 | explore 15 |

> **附带修正**：ArchForge `32 号 §6.4.2` 末句「沿用 `ADR-CEP-12` **决策 3** 的『不静默降级』处置」是**错误指针**
> —— `ADR-CEP-12` 决策 3 是 consistency_labels（`:104-108`），「不得静默降级」出在 **`ADR-CEP-11:134/106`**。
> 我方重写 IC2 时**必须引 `ADR-CEP-11` 决策 3**，不要继承这条悬引用。

**验收**: `bash scripts/test/cep_freeze_check.sh` → **红**（预期，冻结面已动）→ Step 6 重生成后转绿。

### Step 5 — 登记 + 接入 CI

- `test/CMakeLists.txt`：注册 `add_test(NAME cep_decl_alignment_check COMMAND bash .../cep_decl_alignment_check.sh)`
  + `set_tests_properties(... WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})`（**必设** —— ctest 默认在 `build/test` 下跑，
  相对路径会解析错；`test/CMakeLists.txt:257-260` 有既有注释说明）
- `scripts/test/cep_docs_sync_check.sh`：加**第 9 组** = **调用 `cep_decl_alignment_check.sh` 并透传退出码**（**不重复实现**）；
  该组把 `[SKIP]` 显示为 `SKIPPED(archforge absent)` 而非 ✓；
  同步修正该脚本**头部注释的组数清单**（现只列 6 项、实际 8 组 → 改 9，**既有漂移顺手修**）
- **不新增 CI 依赖**（不碰 `ci.yml:71` 的 pip 行）—— 检查器零依赖手写

**验收**: `ctest -R cep` → 16 全绿；`bash scripts/test/cep_docs_sync_check.sh --strict` → 9/9 组通过。

### Step 6 — 台账 + 冻结清单重生成 + 全量回归

- `.rddf/roadmap/objectives/obj-cep-platform.md` §11 追加 `### Entry #9 — 2026-10-22 [adr-amendment]`，**必含 4 小节**：
  ① 变更的冻结产物清单（逐个 + 为什么）② **RUN 证据**（非 skip 的完整输出粘贴）③ 反向验证粘贴 ④ 白名单/遗留声明（本批无白名单 → 明确写「无」）
- `bash scripts/test/cep_freeze_check.sh --update`
  —— **v2 澄清**：脚本的 `frozen_at_commit` 是**无条件取运行时 HEAD**（`:61-69`），不是「变更前 HEAD」。
  正确表述：**commit 前跑一次**（此时 HEAD = 变更前）、**commit 后必须再跑一次**（使清单与 HEAD 一致）。
  第二次会覆盖为「引入变更的那个 commit」，**变更前锚点不再保留** —— 这是既有机制，不是本批引入；
  若要保留需改脚本语义，**不在本批**。
- 全量回归：`ctest --test-dir build -R cep` + `docs_sync_check.sh --strict` + `markdown_link_check.sh`（基线 **115**，不得增加）

### Step 7 — 反向验证（必须「有牙」，per M-2 的教训）

| 破坏动作 | 期望 |
|---------|------|
| 把 `fabric.json` 的 `compute_noc.vc_count` 从 4 改成 2 | 检查器 **红**（条件② 上界） |
| 把 `fabric.json` 增一个不存在的网络 `nsa_switch` | 检查器 **红**（NS-R1）+ 反向 **warning**（ArchForge 清单无此项） |
| 把声明的 `vc_binding` 某 network 的 vc_id 改成 9 | 检查器 **红**（schema `maximum:3` / 手写结构校验） |
| 把声明的 `ordering_assumption` 删掉某个 network | 检查器 **红**（网络键对称性） |
| 把声明的 `vc_binding` 消息类写成 `forward_data` | 检查器 **红**（6 类词表） |
| 把声明的 `ordering_assumption[n]` 内层 key 写成 `"high_pri"` | 检查器 **红**（`int()` 解析失败路径，不崩栈） |
| 恢复全部 | **绿** |

> ⚠️ **反向验证必须真跑**并把结果贴进台账 Entry #9 —— M-2 的病根就是「声称有自动化、实则没有」。
> ⚠️ 反向验证 3「vc_id 改成 9」若只靠 schema 的 `maximum:3`，在**手写校验**下需显式实现该上界（不引入 jsonschema）。

### Step 8 — 对外动作（A13 / A14 / A15）

| 动作 | 内容 | 时机 |
|------|------|------|
| **A13** | 提交本仓回执 + 本批改动 → 闭合 ArchForge `ADR-SOC-25` 的 **V-8**（「CEP 侧逐条回执仍待」） | **需用户确认后 commit**（不主动 commit） |
| **A14** | 正式答复 R-6' 附带项：❌ 不扩 `CATEGORY_*`；6 类词表落 CEP 层；`CATEGORY_*` 保持 3 类；关系由 adapter 显式映射表承担 | 随 A13 提交 |
| **A15** | 催 **Q-3**（R-2 判据 (b)）—— 2 轮未交付，阻塞 A6 | 随 A13 提交 |

### Step 9 — M2 放行（A7，**在 Step 1~7 全绿 + RUN 留痕之后**）

| 文件 | 改动 |
|------|------|
| `openspec/changes/cpptlm-cep-umbrella/proposal.md` | 状态行 🟡 Proposed Draft → 🟡 **Proposed**；`前置 gate` ④ ⛔ → ✅；gate ④ 结论块换成 **ArchForge 侧** `6020f42` / `8290b4c` / `ac956b8` + **本仓** `<BATCH1_COMMIT>`（A13 后回填）+ **台账 Entry #9 的 RUN 证据引用** |
| `openspec/changes/cpptlm-cep-umbrella/tasks.md` | Step 4 的 ⛔ 文本更新；结论行改为「Stage 0 **仍不 archive** —— R10 未闭合」；**新增 Stage 1 任务**「adapter 消息类 → `CATEGORY_*` 显式映射表（非恒等），验收：映射表存在且经 contract test 断言」（挂住 A14 裁定，metis 隐藏意图 4） |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §5.2 baseline **0/3 → 1/3**（升 Proposed 占 1 个 KPI slot）；§11 追加 `Entry #10 [go-decision]`，其中**必须显式 risk-accept**：「未经独立复审（R10 未闭合）」—— **不存在默认放行** |

> ⚠️ **Stage 0 仍不 archive** —— R10 独立性缺口未闭合，与 M2 无关，措辞不得混。
> ⚠️ **Stage 1 必须承接的 3 项**（本批的 provisional 债）：N-1/N-2 的实现侧复核、
> SLICC vnet 备选重评触发条件（第 3 网/第 3 实例）、`fabric.json` 进 M1 冻结集。
> ⚠️ 若在 Step 1~7 完成前就要放行，则**必须**把 A10/A11/A12/M-1 + **ADR-CEP-08/09** 显式写成 Stage 1 准入前置。

---

## 4. 验收标准

> **v2 原则**：每条 AC 必须是**可机器判定**的（给命令 + 期望输出），禁止「✅ 通过」式空壳；
> 且必须有**「没破坏别的」**类 AC。

| # | AC | 判据（可执行） |
|---|-----|------------|
| **AC-1a** | 检查器在双仓环境**真的跑过** | `ARCHFORGE_DIR=/workspace/main/ArchForge bash scripts/test/cep_decl_alignment_check.sh` 首行含 `[RUN]`，逐项输出 ①~⑤ 的 ✓，exit 0；**完整输出粘贴进台账 Entry #9** |
| **AC-1b** | 跨仓缺席**显式 skip 而非静默绿** | `ARCHFORGE_DIR=/nonexistent bash ...` 首行含 `[SKIP]`，exit 0；lint 第 9 组显示 `SKIPPED(archforge absent)` |
| **AC-2** | ctest 全绿 | `ctest -R cep` → **16/16** 通过 |
| **AC-3** | lint 9 组 | `bash scripts/test/cep_docs_sync_check.sh --strict` → 9/9 组通过 |
| **AC-4** | 冻结校验 | `bash scripts/test/cep_freeze_check.sh` → 绿；**产物数仍为 28**（v2：`fabric.*` **不进**冻结集） |
| **AC-5** | 声明 ↔ schema 失配归零 | 检查器 ① 全绿；ArchForge 2 份声明不再有失配（实现按**字段级**报告 = 2 处/份，见 §9.2） |
| **AC-6** | IC2 落位（**不另立 IC4**，见 §9.1） | `grep -c "INV-CEP-12-IC2" docs/adr/ADR-CEP-12-*.md` ≥ 1；IC2 原文含 `vc_count` 与 6 类词表；invariant 总数仍 **66 实际 / 67 编号位** |
| **AC-7** | 旧扁平语义**无残留** | `grep -rn "named_network" docs/adr/ADR-CEP-08*.md docs/adr/ADR-CEP-09*.md openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md` → **仅** 历史引述/Status Update 段命中（人工确认） |
| **AC-8** | schema 逐字对齐 delta | `protocol_instance.schema.json` 的 `ordering_assumption`/`vc_binding` 子树与 ArchForge `32 号 §6.4.1` diff 原文**逐字 diff**（除显式叠加项：无） |
| **AC-9** | **反向验证 7 条全部有牙** | Step 7 表格逐条「破坏 → 红 → 恢复 → 绿」，结果贴进台账 Entry #9 |
| **AC-10** | **没破坏别的：测试改写面受限** | `git diff test/test_cep_schema.cc` 改动的 TEST_CASE **仅限** Step 2 清单（含新增 fabric 用例）；`[negative] C9` 用例**零改动**且绿 |
| **AC-11** | **没破坏别的：`07 号` 零 diff** | `git diff --stat docs/architecture/07-coherence-exploration-platform.md` → 空 |
| **AC-12** | **没破坏别的：兄弟 ADR 已同步** | `git diff --stat docs/adr/ADR-CEP-08*.md docs/adr/ADR-CEP-09*.md` → **非空**（证明确实改了，不是漏了） |
| **AC-13** | **没破坏别的：既有 8 组 lint + 33 锚点仍绿** | `cep_docs_sync_check.sh --strict` 9/9 + `cep_archforge_alignment_check.sh` 33/33 |
| **AC-14** | 无新增 CI 依赖 | `git diff .github/workflows/ci.yml` → **空**（检查器零依赖） |
| **AC-15** | 失效链接未增加 | `markdown_link_check.sh` 总数 **115**（= 基线） |
| **AC-16** | M2 放行四处同步 | `grep -q "🟡 Proposed" proposal.md`；`grep -q "✅" proposal.md`（gate ④ 行）；`grep -q "仍不 archive" tasks.md`；`grep -q "1/3" obj-cep-platform.md` |
| **AC-17** | provisional 已标注 | `grep -q "provisional" docs/adr/ADR-CEP-12-*.md` |
| **AC-18** | SLICC 备选已登记 | `grep -q "vnet\|虚拟网络" docs/adr/ADR-CEP-12-*.md`（修订记录段） |
| **AC-19** | fabric 非独立权威已标注 | `grep -q "derived_from" include/tlm/cep/fabric.json` + `grep -q "authority_note" include/tlm/cep/fabric.json` |

---

## 5. 风险与回滚

| # | 风险 | 等级 | 缓解 / 回滚 |
|---|------|:----:|------------|
| **RK-1** | 冻结面在清单重生成前**必红** → 误判为回归 | 🟡 | 步骤顺序已把重生成放最后;台账注明「红是预期的」 |
| **RK-2** | `--update` 被当作「抹掉漂移」 | 🟡 | 授权记录 = §11 台账 entry（必含 4 小节） |
| **RK-3** | `INV-CEP-12-IC2` 重写后判据语义漂移 | 🟡 | Step 7 反向验证第 1 条直接测条件②;旧判据原文保留在修订记录 |
| **RK-4** | ArchForge 后续改声明 → 检查器红 | 🟡 | **这是检查器的目的**，不是故障 |
| **RK-5** | **检查器在 CI 里恒跳过** → 「机器满足」名不副实 | 🔴 | **D6**：输出显式 `[RUN]`/`[SKIP]` + **RUN 留痕进台账** + gate ④ 引用该 entry。**这是本批存在的理由**，不是可接受的副作用 |
| **RK-6** | **jsonschema 依赖缺失** → 仓在场时 ImportError 崩 | 🔴 | **D1/Step 1 第 2 条**：**零依赖手写校验**;不碰 `ci.yml:71` |
| **RK-7** | **fabric `vc_count` 双真源静默漂移** | 🔴 | **D8**：`fabric.json` 标注 derived-from + 检查器第 ⑤ 项**反向比对** |
| **RK-8** | **冻结集内 `ADR-CEP-08`/`ADR-CEP-09` 留活矛盾** | 🔴 | **Step 4 纳入同一 adr-amendment**（含 `ADR-CEP-09:75` 图顶点数学对象变更） |
| **RK-9** | **死锁论证的图顶点与声明形状脱钩** → 无环判据可能给错误 verdict | 🔴 | 同 RK-8：`ADR-CEP-09:75` 必须改 `(协议实例, network, vc_id)`；且 Stage 1 前不得据此判据放行任何协议 |
| **RK-10** | **为对方 `planned` 声明的当前形态过早固化** | 🔴 | **D7 provisional 标注** + `fabric.*` 不进冻结集 + SLICC 备选登记 |
| **RK-11** | 独立复审通道不可用（subagent `Invalid token`） | 🟡 | 已登记 R10;**放行记录必须显式 risk-accept「未经独立复审」**（Step 9 台账 Entry #10）;**不存在默认放行** |
| **RK-12** | 一次改太多 | 🟡 | 9 个可独立验收的 Step;冻结面在 Step 3/4 动一次 |

**回滚方式**：全部改动集中在
`{protocol_instance.schema.json, fabric.schema.json, fabric.json, ADR-CEP-12, ADR-CEP-08, ADR-CEP-09,
umbrella spec.md, test_cep_schema.cc, cep_decl_alignment_check.sh, cep_docs_sync_check.sh, test/CMakeLists.txt,
obj-cep-platform.md, 回执, 清单, ADR-CEP-00}`。
`git checkout -- <上述文件>` + `cep_freeze_check.sh --update` 即可整体回退；已 commit 则 `git revert` + 再 `--update`。

---

## 6. 需 DRI 拍板的事项

| # | 决策 | 我的推荐 | v2 变化 |
|---|------|---------|:---:|
| D1 | 检查器形态 | **A**：独立脚本 + 独立 ctest + lint 第 9 组（透传退出码，不重复实现） | 命名改「对齐检查」 |
| D2 | fabric 载体 | **A+A'**：`fabric.schema.json`（类型）+ `fabric.json`（实例）两份 | ✅ **v2 拆分** |
| D3 | 6 类词表落点 | **A'**：**检查器第 ④ 项**（schema 内 `propertyNames` 推迟） | ✅ **v2 改** |
| D4 | 批次划分 | **A**：批 1 = A10+A11+A12+A14；A4 批 2；A6 批 3 | — |
| D5 | `vc_id` 上界 | **A**：schema 保留 `maximum:3`；检查器按 `vc_count` 判；`fabric.vc_count` 上界 **4** | ✅ v2 对齐 |
| **D6** | 「机器满足」的证据形态 | **A**：显式 `[RUN]`/`[SKIP]` + RUN 留痕进台账 + gate ④ 引用 | ✅ **v2 新增** |
| **D7** | 嵌套形态 provisional vs 定死 | **A**：provisional 标注 + 修订记录登记 SLICC 备选 | ✅ **v2 新增** |
| **D8** | fabric 权威归属 | **A**：我仓为 derived 副本 + 检查器反向比对 | ✅ **v2 新增** |
| D9 | M2 放行时机 | **Step 1~7 全绿 + RUN 留痕之后** | — |
| D10 | A13 是否由我执行 commit | **否** —— 按仓惯例不主动 commit | — |

---

## 7. 引用

**跨仓输入（只读）**: ArchForge `8290b4c`（`32 号 §6.0` per-network 矩阵 `:322` / `§6.0.1` 6 类词表 `:299` /
`§6.4.1` delta `:842-871` / `§6.4.2` 伪码 `:875-883` / `docs/roadmap/cep-interface-alignment-review-2026-10-21.md`）、
`ac956b8`（`22-nsa §0.5.4.1` 网络清单 `:144-152` + NS-R1 `:156` + NS-R2 `:157` + §0.5.2 纪律 `:92-111`）

**本仓真源**: `docs/implementation/2026-10-21-cep-archforge-response-receipt.md`（§6 M2 结论 / §8.2 N-1~N-4 裁决状态 / §8.3 M-1~M-3 / §9 A10~A15）

**冻结契约（v2 新增 ADR-CEP-08 / ADR-CEP-09）**:
- `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md`（决策 5 `:118` / key 原文 `:121` / 决策 6 `:126` / IC1 `:163` / IC2 `:168` / IC3 `:169`）
- **`docs/adr/ADR-CEP-08-ordering-point-portability.md:131,164`**（key 粒度与 key 解析 —— **活矛盾**）
- **`docs/adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md:75,77,92`**（**死锁图顶点** `(协议实例, vc_id)` → 需 `(协议实例, network, vc_id)`）
- `docs/adr/ADR-CEP-11-protocol-version-locking.md`（§3 锁表 6 族 `:116-123` + `INV-CEP-11-IC1` `:134`）
- `docs/adr/ADR-CEP-14-stub-contract-tests.md`（C9 负向断言按**字段名**做 → 嵌套化不击穿）
- `docs/architecture/07-coherence-exploration-platform.md`（`:148` Fabric & Transport Plane 职责 / `:588` 已预见嵌套形态 —— **零 diff**）
- `openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md`（IC2 scenario `:249-254`；按名引用 `:163 :223 :239 :331`）

**代码依据（M-1 裁定，v2 已修行号）**:
`include/bundles/noc_bundles_tlm.hh:60-62`（`CATEGORY_*` 3 类 + 「Credit 信号专用类别」）、
`include/tlm/cluster/soc_internal_noc.hh:10`（自述「简化抽象为 flit_category + vc_counts_[4]」）、
`:118`（`++vc_counts_[vc]`，下标经 `(cat < VC_COUNT) ? cat : VC_SNP` **钳位**）、
`include/framework/bidirectional_port_adapter.hh:235`（credit 回信写 `CATEGORY_CREDIT`）、
`include/tlm/gpu/sdma_noc_adapter.hh:57`

**机制**: `.rddf/roadmap/objectives/obj-cep-platform.md` §9.2（M1 冻结 / M2 准入门 / M4 独立复审）、§11 台账（末号 Entry #8）；
`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`（28 产物冻结集）

**CI**: `.github/workflows/ci.yml:71`（`pip install networkx pydot pydantic pytest` —— **jsonschema 未装** → 检查器零依赖）

---

## 8. 三方评审结论与处置（v2 修订记录）

> 评审：oracle（架构/契约自洽性）+ metis（缺口/边界/slop）+ explore（事实核对）。
> 独立复审通道本轮**可用**（与前两轮的 `Invalid token` 不同）→ 本节的评审本身构成一次**真实的对抗式复审**。

### 8.1 explore（事实核对）—— 3 处引用已修

| 原引用 | 更正为 |
|--------|-------|
| `soc_internal_noc.hh:116` = `vc_counts_[flit_category]` | 注释 `:111`；代码 `:118` `++vc_counts_[vc]`（下标经钳位，**非**直接以 `flit_category` 为下标） |
| spec.md 涉该两字段 = `:163` / `:223` | 实为 **5 处**：`:163 :223 :239 :251 :331` |
| `--update` 使 `frozen_at_commit` = 变更前 HEAD | 脚本**无条件取运行时 HEAD**；「变更前」是流程效果而非脚本语义 |

**顺带发现（计划未察觉）**：`test/test_cep_schema.cc:147` 的用例**标题**写「schema 必含 8 个字段组」，
但正文实际断言 **13 个属性**（权威分组在 `:268` 的「7 组 / 13 属性」）→ 既有 cosmetic 漂移，已列进 Step 2 顺手修。

### 8.2 oracle（架构）—— 采纳 5 条，其中 2 条**改变了范围**

| # | 结论 | 处置 |
|---|------|------|
| O-1 | 嵌套形态放协议实例声明**分层正确**（`07 号:145` 划归 Protocol Engine；`:148` 明说死锁论证「输入来自 Protocol Engine 的排序假设 + VC 绑定声明」）；ArchForge §6.4.3 三面分工**自洽** | ✅ 采纳形态，不重开 N-2 谈判 |
| O-2 | 「排序是 VC 的静态属性」措辞**误导**（论证的是 key 粒度 + flit 不带 ordering，非所有权）；真正的一手依据是 `.pcc` 的 `Network {}`（协议规格文件 = 协议生成器输入） | ✅ **§2.3**：决策 5 理由文本改写，形态不变 |
| O-3 | **`ADR-CEP-08:131,164` + `ADR-CEP-09:75,77,92` 是活矛盾**；其中 `ADR-CEP-09:75` 的图顶点 `(协议实例, vc_id)` → `(协议实例, network, vc_id)` 是**数学对象变更**，不是措辞；不改则「契约收敛」名不副实 | ✅ **Step 4 纳入同批** + **RK-8/RK-9** |
| O-4 | 更根本的正解是 **SLICC 式「协议本地 vnet + 装配期绑定」**；但推翻 N-2 已交付形态、跨仓谈判重开 | ✅ **§2.3 登记为已知备选**，触发条件 = 第 3 网/第 3 实例；本批不实施 |
| O-5 | **冻结集口径越界**：M1 冻结「CEP 自己的契约」合理，把**跨仓实例形状**（对方 `status: planned` 的草案）连同 SHA-256 冻进 M1 = 把对方草案升格为本仓不可变物 | ✅ **D7 provisional** + **D8 fabric 不进冻结集**；M1 机制本身不动 |
| O-6 | 最小充分集：「不该做」里最重的是**在 CppTLM 侧自建 fabric 权威拷贝而不设反向闸门** | ✅ **D8** |

### 8.3 metis（缺口）—— 采纳 8 条 Blocker/Quality

| # | 结论 | 处置 |
|---|------|------|
| M-a | **skip-hole**：CI 无 ArchForge → 恒跳过 → 「机器满足」退化为「本机跑过一次」，与 M-2 病根**证据层面同构** | ✅ **D6** + AC-1a/1b + RK-5 |
| M-b | **fabric schema vs 实例混淆**（Step 1/7 当实例读，Step 3 写类型） | ✅ **D2 拆分** + AC-4（仍 28 产物） |
| M-c | **`jsonschema` 不在 CI**（`ci.yml:71` 只装 4 个包）；仓在场 + 依赖缺失 = ImportError 崩，与「优雅跳过」承诺冲突 | ✅ **Step 1 第 2 条零依赖手写** + AC-14 + RK-6 |
| M-d | **网络键不对称未查**（`32 号 §6.2` 要求每网络同时出现在两个字段） | ✅ **Step 1 校验②** + Step 7 反向验证第 4 条 |
| M-e | **`vc_count` 上界 8 vs schema `maximum:3` 跨产物矛盾**且无子校验报它 | ✅ **D5 对齐为 4** |
| M-f | **冻结集内 `ADR-CEP-08`/`ADR-CEP-09` 旧规则残留** | ✅ **Step 4** + AC-7/AC-12 + RK-8 |
| M-g | **测试改写面被低估**（`resolve_named_networks` `:46-91` / `minimal_valid_instance` `:132-133` / IC2 正负 2 用例 `:377-438` / `NetworkMap` `:52`） | ✅ **Step 2 清单** + AC-10 |
| M-h | **AC 空壳化**（「✅ 通过」式判据；AC-7/AC-10 不可机器判定） | ✅ **AC 全面改为「命令 + 期望输出」**（AC-1a~AC-19） |
| M-i | **RK 表全 🟡 无 🔴** → 评级不可信；RK-7 缓解「建议请真人过一遍」**没有决策机制** | ✅ **RK-5~RK-10 新增 6 个 🔴**；放行必须 **risk-accept**（Step 9） |
| M-j | Step 4「`:163`/`:223` 逐条核对措辞」会诱导**churn 无关 scenario** | ✅ 改为「仅重写 `:249-254`；其余按名引用经 grep 确认后不动文」 |
| M-k | A14 裁定的「Stage 1 映射表」**没有可检查的落点** | ✅ **Step 9 新增 Stage 1 任务**（映射表存在 + contract test 断言） |
| M-l | `cep_docs_sync_check.sh` 头部注释 6 项 vs 实际 8 组（既有漂移） | ✅ **Step 5 顺手修**（6→9） |
| M-m | metis 认为 Step 9 的 `6020f42` 是「本批 commit」 | ⚠️ **misreading**：`6020f42` 是 **ArchForge 侧** commit（证据），非本仓；本仓 commit 用占位 `<BATCH1_COMMIT>` |

### 8.4 未采纳

| 意见 | 理由 |
|------|------|
| metis:「`fabric.schema.json` 应进冻结集（AC-4 改 28→30）」 | 与 oracle O-5 冲突 → 采 oracle：**零实例新产物不冻结**（D7/O-5） |
| metis:「`protocol_version` 白名单须显式写在脚本里并留出口」 | 与 oracle 冲突 → 采 oracle：**本批整个不做该校验**（§1.3），白名单问题消失（比「写好白名单」更彻底） |
| metis:「schema 内层加 `propertyNames: {pattern: ^[0-3]$}`」 | 采 oracle/§6.4.2：内层 key 约束由**检查器**兜，schema 不加 → 冻结面更小 |
| oracle:「引用矩阵 id 只查存在性」（其 §3 问 (b)） | oracle 自己已判**不采纳**（会把 13/13 字段校验退化成 1 个字符串校验 = M-2 病根复刻） |

---

## 9. 实施记录(2026-10-22,批 1 执行完毕)

> 计划与实施之间的**偏差**全部记录于此;每条给「偏差 → 原因 → 处置」。
> 台账 Entry #9(`obj-cep-platform §11`)是本批的**授权记录**,含 RUN 证据与反向验证粘贴。

### 9.1 偏差 D-1:6 类词表**不另立 `INV-CEP-12-IC4`**,并入 `INV-CEP-12-IC2` 条件 ③

- **触发**:实施 Step 4 后跑 `cep_docs_sync_check.sh` 第 2 组 → **红**:
  「`07 号 §4` 组『Capability Map』写 3 条,owner ADR 实际定义 4 条」
  + 「ID 区间漏写 `INV-CEP-12-IC4`」+ 「总数 66 != 实际定义 67」
- **根因**:新增 invariant 会连带触发 **4 处计数同步**
  (`07 号 §4` 总表 + `objective-cep-platform` DoD 7 的 15 组分解 + `improvement A2` + `phase-10` 交付预期)
  + canonical invariant 用例数 —— 远超本批「让 C-4 机器可验收」的范围
- **处置**:词表约束并入 `IC2` 条件 ③ + 展开注记(排除项 + 分层纪律);**invariant 总数维持 66/67**;
  `07 号` **零 diff** ✅(AC-11 得以成立)
- **附带发现**:第一次改写时把 `INV-CEP-12-IC4` 放进**加粗 span + 冒号**的句子里 → lint 的
  「定义位」探测器(P1~P5 五形态)把它当成**定义** → 报「实际定义 67 条」。
  这正是本仓 `07 号` §3.1 记录的「**「提及」≠「定义」**」陷阱 —— 改写措辞使其不落在定义位形态后转绿。

### 9.2 偏差 D-2:检查器的失配报告粒度是**字段级**(2 处/份),不是网络级(4 处/份)

- 计划 AC-5 写「4 处/份失配 → 0」;实现按**字段级**报告(`ordering_assumption` / `vc_binding` 各 1 处)
- **处置**:AC-5 改为「① 从 2 处/份 失配 → 0」;粒度选择理由 = 字段级噪声更低,
  且 `jsonschema` 的逐网络报错在本仓**手写校验**下需额外分支

### 9.3 偏差 D-3:第 ⑤ 项的**反向差异判为 error**(计划原文写 warning)

- 计划 D8 原写「反向比对差异 → warning 不阻断」
- **改为 error** —— 依据计划自身的 **RK-7**:`vc_count` 静默漂移 → 条件② 用过期上界通过 →
  装配期判据失效但不报红 = **M-2 病根的第三次复发**。warning 不构成闸门
- 新增子项:我方 fabric 的网络集合 **必须等于** ArchForge 已锁定集合(NS-R2 单点权威的直接体现)

### 9.4 偏差 D-4:反向验证的作用域 —— 破坏 3~6 用 **ArchForge 文档副本**

- 直接改 ArchForge 真仓做破坏验证**不可接受**(那是另一个仓,且已提交)
- **处置**:`ARCHFORGE_DIR` 指向临时副本(仅 2 份文档)→ 真仓**零改动**;
  破坏 1~2 作用于本仓 `fabric.json`(备份 + 还原,已复验字节一致)

### 9.5 偏差 D-5:构建环境 —— g++-14 工具链**被迁移**

- `build/` / `build-on/` 均按 `/usr/bin/g++-14` 配置,而该编译器与其头文件(`/usr/include/c++/14/`)
  在本机**已不存在**(工具链迁至 `~/.bashrc` 的 `/workspace/main/opt/bin`,头文件随之迁至
  `/workspace/main/opt/usr/include/c++/14/`)→ 既有 `build.ninja` 的头文件依赖全失效 → **全量重编**
- **处置**:`sudo ln -sf /workspace/main/opt/bin/g++-14 /usr/bin/g++-14`(恢复软链)→ 重新 configure + 全量重建
- **副作用**:g++-13 不可用(`<print>` 是 C++23 头,仅 GCC 14 有;`include/utils/module_group.hh:8`
  与 `include/core/module_factory.hh:20` 依赖它)→ 本仓**必须**用 g++-14 构建

### 9.6 实施产物清单(与计划 §3 的对应)

| 计划步骤 | 产物 | 状态 |
|---------|------|:----:|
| Step 1 | `scripts/test/cep_decl_alignment_check.sh`(零依赖手写,5 项子校验) | ✅ |
| Step 2 | `test/test_cep_schema.cc`(`resolve_named_networks` → `resolve_ordering_assumptions`;<br>`NetworkMap` → `network→vc_count`;`minimal_valid_instance` 改嵌套;IC2 正负 2 用例重写;<br>`:147` 标题口径修;新增 fabric 用例) | ✅ |
| Step 3 | `include/tlm/cep/protocol_instance.schema.json`(2 字段嵌一层)+ `fabric.schema.json` + `fabric.json` | ✅ |
| Step 4 | `ADR-CEP-12 §8` / `ADR-CEP-08 §8` / `ADR-CEP-09 §8` / umbrella `spec.md` IC2 scenario | ✅ |
| Step 5 | `test/CMakeLists.txt`(ctest)+ `cep_docs_sync_check.sh`(第 9 组,8→9 计数同步 + 头部注释修正) | ✅ |
| Step 6 | 台账 Entry #9 + `cep_freeze_check.sh --update`(28 产物,`fabric.*` **不**入冻结集) | ✅ |
| Step 7 | 反向验证 **9/9 全绿** | ✅ |
| Step 8 | A13(提交回执,闭合 ArchForge V-8)/ A14(答复 R-6' 附带项)/ A15(催 Q-3) | ⏸ 待 commit(D10:不主动 commit) |
| Step 9 | M2 放行(proposal.md / tasks.md / baseline 1/3 / 台账 Entry #10) | ✅ **已执行**(2026-10-22)—— `openspec validate --strict` PASS;`tasks.md` 新增 **5 项 Stage 1 准入前置**(S1-1 ~ S1-5) |

### 9.7 最终验收对照(2026-10-22 实测)

| AC | 结果 | 证据 |
|----|:----:|------|
| AC-1a | ✅ | RUN 输出粘贴进台账 Entry #9 ②;首行含 `[RUN]`,5 项子校验逐行 ✓ |
| AC-1b | ✅ | `ARCHFORGE_DIR=/nonexistent` → 首行含 `[SKIP]`;lint 第 9 组显示 `!` warn 而非 `✓` |
| AC-2 | ✅ | `ctest -R cep` **16/16** |
| AC-3 | ✅ | CEP lint **9/9 组** |
| AC-4 | ✅ | M1 冻结 **28 个**(未增 —— `fabric.*` 不进冻结集) |
| AC-5 | ✅ | ① 由 2 处/份失配 → **0**;声明 2/2 份校验通过 |
| AC-6 | ✅ | IC2 已按 §6.4.2 重写;invariant 总数仍 **66 实际 / 67 编号位** |
| AC-7 | ✅ | 旧扁平语义仅存于各 ADR 的 **Status Update 取代范围**说明内;新增 normative 文本中 0 命中 |
| AC-8 | ✅ | schema 两字段子树与 `32 号 §6.4.1` diff 原文一致(仅 description 措辞按 §2.3/oracle 调整) |
| AC-9 | ✅ | 反向验证 **9/9** 全绿(破坏 → 红 → 恢复 → 绿) |
| AC-10 | ✅ | `test_cep_schema.cc` 的 diff 限定于 Step 2 清单(5 个 TEST_CASE);`[negative] C9` 用例**零改动**且绿 |
| AC-11 | ✅ | `07 号` **零 diff**(IC4 并入 IC2 后 invariant 计数不变) |
| AC-12 | ✅ | `ADR-CEP-08` / `ADR-CEP-09` 均已追加 Status Update(diff 非空) |
| AC-13 | ✅ | lint 9/9 + 锚点 33/33 |
| AC-14 | ✅ | `.github/workflows/ci.yml` **零改动**(检查器零依赖) |
| AC-15 | ✅ | 失效链接 **115** = 基线(未增加) |
| AC-16 | ✅ | proposal 状态行含 🟡 Proposed + gate ④ ✅;tasks 结论行含「仍不 archive」;objective §5.2 含 `1/3` + Entry #10 |
| AC-17 | ✅ | `ADR-CEP-12 §8.2` 含 provisional 标注 |
| AC-18 | ✅ | `ADR-CEP-12 §8.4` 登记 SLICC vnet 备选 |
| AC-19 | ✅ | `fabric.json` 含 `derived_from` + `authority_note` |

**全量回归**:`ctest --test-dir build` **94/94 全绿**(2026-10-22 实测)。

> **尚未闭合的唯一项**:**A13(commit)** —— 按 D10 与仓惯例,**不主动 commit**,等 DRI 确认。

### 9.8 对抗式复审(critic)的 REJECT 与处置(2026-10-22)

计划经 oracle + metis + explore 三方评审修订(v2)后,又经 **critic** 对**实施结果**做对抗式审稿:
**REJECT,7 个 BLOCKER + 10 个 SHOULD_FIX + 5 个 NITS**。**全部已修并复验**。

| # | BLOCKER | 处置 |
|---|---------|------|
| B1 | `fabric.json` 违反自身 `fabric.schema.json`(`additionalProperties:false` vs 3 个注记字段) | schema 补声明这 3 个可选 property + C++ 侧加「实例键集 ⊆ schema 声明键集」断言 + shell 侧加 jsonschema 校验 |
| B2 | 检查器只校验 2/13 个 required 属性,其余 11 个零校验 | **改用 `jsonschema` 做全量校验**;CI 补装 `jsonschema`;**刻意不降级运行**(缺依赖 → 报错退出 exit 2,不静默跳过) |
| B3 | 声明 JSON 解析失败被静默丢弃 + 硬编码「必须 2 份」 | 解析失败 → **判红并报出是第几个 fence**;份数改为「≥1 + id 唯一 + 全部被校验」(**不硬编码 2** —— 第 3 个实例是 Stage 1 的合法演进) |
| B4 | 冻结清单 `frozen_at_commit` 与产物内容不符(**假 provenance**) | 校验器加 **provenance 自检**(逐个比对「清单 hash」vs「该 commit 处该产物的内容」),不一致即响亮报出;配**两步 commit 流程**(先 commit 产物 → `--update` → 再 commit 清单) |
| B5 | `--update` 无授权闸门(一次命令即可抹掉任何漂移) | `--update` 须带 `--authorized-by <台账 entry id>` 并核对该 id 确实存在于台账 §11;清单新增 `authorized_by` 字段;`frozen_at` 改为取自 git |
| B6 | `ADR-CEP-17` 仍持「`named_network` 解析到唯一 `vc_id`」旧 fail 判据 | 补 `## 8 Status Update`(取代决策 8 草图 / §4 模板 / §6 invariant 建议) |
| B7 | ⑤ 的反向漂移守卫在 ArchForge 权威源解析失败时**静默关闭**且 RC=0 | 权威源解析失败 → **判红**(不降级为 warn);并修「同时打印 ✓ 与 ⚠」的输出歧义 |

**计划层面的偏差 D-6**:v2 §2.2/§3 Step 1 定的「**零外部依赖**手写校验」被 **B2 推翻** ——
手写子集导致 11 个 required 属性零校验,正是「声称有自动化、实则没有」的变体。
现改为「**schema 层用 jsonschema(全覆盖)+ 语义层手写(per-network 规则)**」,并接受 CI 多一个 pip 依赖。

**反向验证由 9 条扩到 12 条**(新增 3 条分别钉住 B2 / B3 / B7 三个「闸门可被绕过」的缺口),
**脚本已入库** `scripts/test/cep_decl_reverse_verify.sh`(此前在 `/tmp` 不可复现 —— N4)。

**NITS 处置**:N1 `ADR-CEP-00` 补尾换行 · N2 `ADR-CEP-09` 行号 `:86`→`:84` ·
N3 fabric 用例改名(名实相符)· N4 反向验证脚本入库 · N5 §0.5.4.1 正则前瞻改为 `#{2,4}`(原 `#{2,3}` 过捕获)。

**SHOULD_FIX 处置**:S1 删死代码(扁平分支)+ schema 非嵌套即报错退出 · S2 ctest `SKIP_REGULAR_EXPRESSION`
(并**撤销**误设的 `SKIP_RETURN_CODE 0` —— 那会把正常通过也标成 Skipped)· S3 lint 的 `set -e` 死代码 ·
S4 `vc_count` 上界两处常量对齐(4)· S5 resolver 增消费 `vc_binding` + schema 补 `propertyNames` ·
S6 清单 C-4 行 + §7 收录新检查器 · S7 旧谓词残留面全量处置(活文档改写 / 历史层加注记)·
S8 `ADR-CEP-12 §8.1` 取代范围补 `:82`/`:178`/`:226` · S9 计划文件入库(消悬空引用)· S10 5 个新文件全部 `git add`。
