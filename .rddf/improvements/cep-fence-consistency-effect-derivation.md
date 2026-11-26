---
name: cep-fence-consistency-effect-derivation
priority: P1
phase: phase-10
主题: G4 —— fence 一致性效果的派生声明面（由一致性标签 + barrier 支持推导）
依赖:
  - ADR-CEP-02
  - ADR-CEP-12
  - ADR-CEP-13
状态: "✅ **已归档**（2026-11-24 · change `cep-fence-consistency-effect-derivation` → `openspec/changes/archive/2026-11-24-cep-fence-consistency-effect-derivation/` · 台账 Entry #544 · 裁决 = **G4-a**（per 用户「同意你的建议」· metis 1 轮 P0/P1/P2 已逐条处置）· 状态行 2026-11-26 回填）"
created: 2026-11-24
---

# cep-fence-consistency-effect-derivation: fence 一致性效果的派生声明面

> **来源**：gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G4** + **§2.3.1 素材盘点结论**（2026-11-24）
> **形态**：**照抄 G10**（声明面 + 闸 + 反向 fixture）⇒ 零发明
> **⚠️ 决策随五步链走（metis P1-5 处置）**：本单元把调研文档 §4.3.1 的**论文口径推导规则**提升为
> **机器可检声明** ⇒ 该规则本身是**语义决策** ⇒ **不再声称「零决策变更」** ⇒ 规则表写进
> **新建 main spec 的 Requirements**，随**五步授权链** ratify（⛔ 规则表含 **1 条 provisional**：
> RC 族「read=RC-data ⇒ fence 不提供 acquire」的解读待 architect 追认）。

## 架构依据

### 1. 论文侧（G4 的语义来源）

- 一致性接口 = read / write / **fence**（HeteroGen HPCA2022 p.3 §II-B）⇒ fence 是**一等操作**。
- **fence 效果按一致性模型而异**：SC 无 / TSO 全屏障 / RC release·acquire / PLO 只补 W→W·R→W。
- 一致性标签体系（SC / TSO / RC-×4 / PLO，**read·write 方向标签**）⇒ 论文**没有**「fence 标签」。

### 2. 本仓侧（现状 · 2026-11-24 实测）

- canonical 流 **#9**（共 10 条）`barrier` **存在**（**requester-scoped** · per ADR-CEP-02 决策 3）⇒ 无需新增 canonical 流。
- `protocol_instances.json` 的 `consistency_labels` = read / write（**无 fence**）+ `flow_support.barrier`（布尔）。
- 8 实例的 `flow_support.barrier`：`mesif` / `moesi_gpu` / `nvlink_4` = true，其余 5 个 = false。
- **ADR-CEP-13 决策 1**（barrier_domain ≠ 一致性域边界）+ **决策 2**（`INV-CEP-13-IC1` = ppo_com）：本单元**只登记
  实例粒度的声明效果**，⛔ **不**声称跨一致性域的 barrier 保证 ⇒ 派生视图的粒度 = **实例**。

### 3. 推导规则表（**本单元的决策面** · 随五步链 ratify）

`derive_fence_effect(read_label, write_label, has_barrier) -> FenceEffect`（constexpr · 协议中立）：

| # | 规则 | 优先级 | 依据 |
|:--:|---|:--:|---|
| R1 | `has_barrier == false` ⇒ **None**（无 fence 通道） | **最高**（遮蔽模型规则） | 无 barrier 流 ⇒ fence 无处发生 |
| R2 | read / write 标签**族不同** ⇒ **Invalid** | 次高（域外 ⇒ 必红） | 跨族组合不存在 |
| R3 | 族 = **SC** ⇒ **None**（SC 已全序 ⇒ 与 barrier 存在与否**无关**） | 模型内 | 论文 SC 行 |
| R4 | 族 = **TSO** ⇒ **FullBarrier** | 模型内 | 论文 TSO 行 |
| R5 | 族 = **RC** ⇒ **按方向逐项**：write=RC-release ⇒ release ✓；read=RC-acquire ⇒ acquire ✓；两者皆无 ⇒ None | 模型内 | 论文 RC 行（**并集上界**；本单元取**子集** ⇒ 更严） |
| R6 | 族 = **PLO** ⇒ **Partial**（只补 W→W 与 R→W） | 模型内 | 论文 PLO 行 |
| R7 | 标签族 = {SC, TSO, RC-*, PLO} 之外 ⇒ **Invalid** | 兜底 | schema 枚举外 |

⚠️ **provisional（1 条 · 待 architect 追认）**：R5 中「read=RC-data ⇒ fence **不**提供 acquire」的解读
⇒ 由此 `moesi_gpu` 派生为 **ReleaseOnly**（论文 RC 行是**上界**，本单元取**子集** ⇒ 更严、更诚实）。

### 4. 派生结果（8 实例 · 2026-11-24 实测）

| 实例 | read | write | barrier | 派生效果 | 规则依据 |
|---|---|:--:|:--:|---|---|
| `mesif` | RC-acquire | RC-release | ✅ | **ReleaseAcquire** | R5（两方向齐备 ⇒ 论文 RC 行） |
| `moesi_gpu` | RC-data | RC-release | ✅ | **ReleaseOnly** ⚠ provisional | R5（read 标签非 RC-acquire ⇒ 无 acquire） |
| `nvlink_4` | SC | SC | ✅ | **None**（有通道但空操作） | R3（SC 已全序） |
| `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c` | SC | SC | ❌ | **None**（无通道） | R1 |
| `ualink_4` | PLO | PLO | ❌ | **None**（无通道；R1 遮蔽 R6） | R1（未来开 barrier ⇒ 翻转为 Partial ⇒ 可检出） |

### 5. 不做会怎样

fence 效果既不被声明面表达、也不被闸校验 ⇒ 标签 / barrier 漂移无人发现 ⇒ 与 G10 实施前的状态同型（散文）⇒
**缺口不可检出**；且「fence 无标签」易被误读为「fence 无关」⇒ **假绿**。

## 范围

### 做

1. **派生声明面**（新增通用层头 · 协议中立 · 零协议名）：枚举（方向化）+ `derive_fence_effect()` constexpr
   + **由函数生成**的 8 行表（逐行 `basis`）+ `Invalid` 哨兵 + 机器可读边界常量。
2. **闸**（新脚本）：① **独立 oracle** 比对（闸内**手写**期望表 ⇄ 声明面派生值 ⇒ 非自证）
   ② **输入副本漂移**检测（头文件副本 ⇄ `protocol_instances.json` 逐实例逐字段）
   ③ JSON 缺键 ⇒ **带签名必红**（⛔ 不静默 SKIP）④ 反向 fixture（各自红因签名）+ `--root` / `--selftest`。
3. **测试**（新 cc）：逐行 `static_assert`（行效果 == 函数推导）+ **合成行正控**（TSO / PLO 分支当前无实例 ⇒ 用合成行覆盖）
   + 边界常量 + 变异副本负控。
4. **main spec**（新建 · 含推导规则表 ⇒ 随五步链 ratify）+ delta（由主 spec **提取生成**）+ design / tasks。
5. **计数同步面**：闸 / ctest / `test_cep_*.cc` / 主 spec / README / 登记册 §0.1 / `include/tlm/cep/AGENTS.md`。

### 不做（⛔ 边界）

| 不做项 | 理由 | 归属 |
|---|---|---|
| 改 `protocol_instance.schema.json`（加 `fence` 子字段） | 冻结面 ⇒ 五步链；且**语义上**把「组合效果」误当「标签」 | architect（G4-b） |
| 改 `protocol_instances.json` 实例声明面 | 同上 | architect（G4-b） |
| 改 `ADR-CEP-13` / `ADR-CEP-12` 正文 | 冻结面 ⇒ 五步链 | architect（G4-c） |
| 实现 fence 的**协议语义** / 跨域 barrier 保证 | 属协议 FSM 语义设计，归 ArchForge；⛔ 本视图只到**实例粒度** | ArchForge |
| **裁决** R5 的 provisional 解读 | 随五步链 ratify 的规则面 + architect follow-up | architect（follow-up） |

## Why

| 事实 | 一手出处 |
|---|---|
| 论文的 fence 效果表与标签表**已在仓内**调研文档（**附属证据层** ⇒ 提升为决策需走链） | `docs/research/cep-coherence-protocol-space-survey.md` §4.3.1 / §4.3.2 · :961 |
| 本仓已有推导所需二维：一致性标签 + barrier 支持 | `include/tlm/cep/protocol_instances.json` |
| fence 效果**可**零 schema 变更地表达 ⇒ 不必等 architect 就能让缺口可检出 | §2.3.1 盘点结论 |
| **推导规则本身是决策** ⇒ 必须随五步链 ratify（⛔ 不再声称零决策变更） | metis P1-5 |

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **派生声明面**（新头） | 枚举（None / ReleaseOnly / AcquireOnly / ReleaseAcquire / FullBarrier / Partial / **Invalid**）+ `derive_fence_effect()` constexpr + **由函数生成**的 8 行表（逐行 `basis`）+ 边界常量 |
| 2 | **闸**（新脚本） | 12 条判据 + **独立 oracle** + **输入副本漂移**检测 + JSON 缺键带签名必红 + 反向 fixture + `--root` / `--selftest` |
| 3 | **测试**（新 cc） | 逐行 `static_assert` + **合成行正控**（TSO / PLO 死分支）+ 边界常量 + 变异负控 |
| 4 | **main spec + delta**（新建） | **冻结面新增 1** ⇒ 五步链 1 次；规则表进 Requirements ⇒ 随链 ratify |
| 5 | **计数同步面** | 闸 +1 · ctest +2 · `test_cep_*.cc` +1 · 主 spec +1 · README + 登记册 §0.1 + `include/tlm/cep/AGENTS.md` |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（推导函数存在性 · 编译期）**：8 行每行 `effect == derive_fence_effect(read, write, barrier)` ⇒ 由 **编译期 `static_assert`** 保证（⛔ 不得手抄派生值）。
- [ ] **AC-2（派生正确性 · 独立 oracle）**：闸内**独立手写**的期望表 ⇄ 声明面派生值 **逐实例相等** ⇒ ⛔ 闸**不得**用声明面自己的输入重算期望值（防自证）。
- [ ] **AC-3（输入副本漂移）**：头文件的 (read, write, barrier) 副本 ⇄ `protocol_instances.json` **逐实例逐字段**相等 ⇒ 不等即红（红因 `fence-input-copy-drift`）。
- [ ] **AC-4（域外必红）**：任一行派生为 **Invalid** ⇒ 红（红因 `fence-derivation-out-of-domain`）。
- [ ] **AC-5（JSON 缺键必红）**：实例缺 `consistency_labels` / `flow_support.barrier` ⇒ 红（带签名）⇒ ⛔ **不得**静默 SKIP。
- [ ] **AC-6（实例集双向零 orphan）**：派生表 id 集 == `protocol_instances.json` id 集（双向）。
- [ ] **AC-7（basis 非空 + id 唯一）** + basis 须声明「**不区分 barrier 发生在哪个网络侧**」（per ADR-CEP-13 决策 1）。
- [ ] **AC-8（死分支正控）**：TSO / PLO 推导分支由**测试内合成行**覆盖（不进实例表 ⇒ 不破坏 AC-6）。
- [ ] **AC-9（边界诚实 · 机器可读）**：常量齐备（派生值 = **声明效果** ⛔ 非引擎行为语义 ⛔ 非跨域保证 · 无 barrier ⇒ None · SC ⇒ 无额外效果 · RC 取论文并集的**子集**）。
- [ ] **AC-10（通用层零协议名 / 零 include 协议头）** + 与 G1 / G10 **正交**（无 `egress` / `in_flight` / `hop` / `latency` 字样）。
- [ ] **AC-11（反向 fixture · 各自红因签名）**：① 注入域外标签组合 ⇒ 红 `fence-derivation-out-of-domain`；② 改拷贝根 JSON 的 labels 而头文件副本不动 ⇒ 红 `fence-input-copy-drift` ⇒ ⛔ 跑在 **mktemp 拷贝根**（唯一临时名）⇒ **零工作树污染**（⛔ 不与 `cep_freeze_reverse_verify.sh` / `cep_decl_reverse_verify.sh` 共享任何工作树文件）。
- [ ] **AC-12（main spec + 五步链）**：新建 main spec ⇒ `--update` **实测**「新增 1 · 移出 0 · hash 变更 0」· 清单**单独** commit · 无参复验一致；规则表随链 ratify。
- [ ] **AC-13（计数同步面）**：**命令实测**取值 == 登记册 §0.1 ⇒ `cep_registry_baseline_check.py` rc=0（⛔ **不写死**具体数字）。
- [ ] **AC-14（零回归 · 全量口径）**：**全量** ctest（⛔ 不按 `-R` 过滤）· 全部 `cep_*.sh` · 非 `cep` 只读闸 · `docs_sync --strict` ⇒ 全绿 + 冻结件零残留。
- [ ] **AC-15（零 schema / 零 instances / 零 ADR 正文变更）**：`git diff --name-only` 实测零 diff（⛔ 除新建 main spec）。
- [ ] **AC-16（零 invariant 新增）**：`cep_invariant_map_check.py` rc=0。

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-fence-consistency-effect-derivation` | **ADDED** | 新能力：fence 一致性效果的**派生声明面**（8 实例）⇒ 与 G1 / G10 **正交**（G10 = hop / 时序；本单元 = fence 效果） |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | 派生声明面头 · 对应测试 · 契约闸 · main spec |
| **修改文件** | `test/CMakeLists.txt` · `scripts/README.md` · 登记册 §0.1 · `include/tlm/cep/AGENTS.md` |
| **零改动（硬查）** | `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · 任何 `docs/adr/*.md` 正文 · `test/cep_invariant_assertion_map.json` |
| **冻结面** | **1 处**（新建 main spec）⇒ 五步链 **1 次**（规则表随链 ratify） |
| **风险** | ① 派生值被误读为「fence 协议语义 / 跨域保证」⇒ 假绿（⇒ AC-7 / AC-9）② **R5 provisional 解读**（RC-data 读 ⇒ 无 acquire）待 architect 追认（⇒ 影响面 + follow-up 登记）③ **枚举可能需扩张**为按方向多值（若 architect 裁定 fence 效果按 read/write 分列 ⇒ 本单元枚举需扩张 ⇒ 回落 G4-b 判权域）④ 头文件内含 **labels/barrier 副本** ⇒ 第二真源风险（⇒ AC-3 副本漂移检测兜底）⑤ TSO / PLO 分支当前**无实例**（⇒ AC-8 合成行正控） |

## 关联

gap 分析 **G4** + §2.3.1 素材盘点结论 · **G10** `cep-hop-timing-capability-declaration`（形态模板）· **G1** `cep-protocol-engine-capability-declaration`（正交参照）· `ADR-CEP-02` 决策 3（barrier = requester-scoped）· `ADR-CEP-12` 决策 3（一致性标签读/写两级 + 请求级映射）· `ADR-CEP-13` 决策 1（barrier_domain ≠ 一致性域）+ 决策 2（`INV-CEP-13-IC1`）· 论文侧调研真源 `docs/research/cep-coherence-protocol-space-survey.md` §4.3.1 / §4.3.2（**附属证据层** ⇒ 提升为规则须随五步链 ratify）· 台账 **Entry #544**
