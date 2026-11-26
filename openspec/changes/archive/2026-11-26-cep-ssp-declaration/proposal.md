# proposal: cep-ssp-declaration —— CEP 用户自定义稳定态协议（SSP）声明面 + 通用解释器（批 1）

> **来源**：objective `.rddf/roadmap/objectives/obj-cep-custom-protocol.md` 的 **`CP-I-2`**（S1 实施）· 台账 Entry **#553**（立项授权）· 用户指令「继续 CP-I-2 立项 → 实施」
> **架构依据**：`ADR-CEP-24`（✅ 已签发 2026-11-26）· **`CP-Q-1a` 裁决**（✅ 已 ratify：事件字母表 = **新建** + 14 核心锚 + 扩权方 = architect + 先例库摊销）
> **形态**：**数据声明面 + 通用解释器**（⛔ 与 `ADR-CEP-24` 决策 1.3 的五件排除物 —— DSL / 解析器 / IR / 生成器 / **烘焙** —— **零交集**）
> **⚠️ 交付分两批**：**批 1（本 change）= 声明面 + loader + V-24-2 逐格对账**；**批 2（另立）= 解释器 + 装配集成 + 掩码格扩展 + 实验 4 件套**（见 §Why 的「为何拆」）。

## Why

| 事实 | 一手出处 |
|---|---|
| `ADR-CEP-24` **已签发但不可实施** ⇒ 「用户自定义协议」仍 = 写 C++ 插件 | `ADR-CEP-24` §8 F5 · `CP-Q-1a` 裁决 |
| 3 份 `*_state_spec.hh` 是事件式转移表：`mesif` **6** / `tilelink_c` **4** / `cxl_2_0` **17** | `mesif_state_spec.hh:60-67` · `tilelink_state_spec.hh:71-76` · `cxl_state_spec.hh:76-98` |
| 格载荷**不同**：MESIF/CXL = `uint8_t` + `0xFF`；**TileLink = `Transition{kind, target_mask, …}`**（3 位掩码） | `mesif_state_spec.hh:132` · `tilelink_state_spec.hh:131-139` |
| 「每协议词表 + flow 映射」**已有先例**（`kChiOpcodeTable` **25** 条）；逐字断言已有先例（`kEventNames`） | `chi_rnf_protocol_engine.hh:139-167` · `tilelink_state_spec.hh:119-120` |
| 逐格 `Basis` 五元组 + 「数据只存在这一处」纪律 | `mesif_state_spec.hh:87-99` · `:142-143` |
| 另 3 份 state_spec **无 Event 枚举、无转移表** ⇒ 表达力 **3/6 体裁 / 2/6 干净覆盖** | `ADR-CEP-24` SU 第二则 · `CP-Q-1a` 裁决 §5 |

**不做的代价**：`CP-Q-1a` 的裁决胜利（14 锚 / 扩权方 / 私有事件规则）**无处落地** ⇒ S1 必达 DoD 永久挂起 ⇒ 本 objective 只剩「规划」。

**为何拆两批（⛔ 不是偷懒，是三条硬理由）**：
1. **解释器要驱动 engine 行为**（时序 / 在飞语义）⇒ 与 S2（`CP-Q-2`：hop/时序 + egress）**耦合**；先做解释器会与 S2 的契约面冲突。
2. **掩码格是 contract 面扩展**（`ssp.schema.json` 的格形状）⇒ 须独立评审；塞进批 1 会让「声明面」与「格形状扩展」两个决策点混在一份 change 里。
3. **「能实验（4 件套）」需要 runner 集成**（`ADR-CEP-06` 的 4 件套 + `experimental/preregistration.json`）⇒ 改动面远超声明面。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **事件字母表契约**（新头 `include/tlm/cep/event_alphabet.hh`） | 协议中立 · 通用层 · **14 核心锚**（`ReadShared` `ReadUnique` `Write` `WriteNoAlloc` `Upgrade` `SnoopShared` `SnoopUnique` `SnoopInvalidate` `EvictDirty` `EvictClean` `Cap` `Grow` `Prune` `Report`）· 每锚带 `flow` ∈ canonical 10 流 · **封闭**（新增须改常量 ⇒ 触发复核）· 逐字名字表 |
| 2 | **SSP 声明面**（新 `include/tlm/cep/ssp.schema.json` + `include/tlm/cep/ssps.json`） | JSON + **同名 schema**（`AGENTS.md` 铁律 3）· 字段组 = 身份 / 状态集 / 事件集 / 转移表（`transition[state][event]`，`0xFF` 空洞 + 逐格 `Basis`）/ perm 投影（4 值锁）/ 排序假设（逐 network）· ⛔ **不进** `protocol_instance.schema.json` |
| 3 | **loader**（新头 `include/tlm/cep/ssp_loader.hh`） | **fail-closed**：未知状态 / 未知事件 / 未登记 `Basis.ref` / 第 5 个 perm / 非 10 流 flow ⇒ **按正确原因红 + 红因签名**；⛔ **只保形态 + 引用闭环，⛔ 不承诺语义**（`ADR-CEP-24` 决策 2.3）|
| 4 | **`ssps.json` 的首批数据 = MESIF 的 SSP** | 由 `mesif_state_spec.hh` 的 `kTable` **逐格转写**（含 `0xFF` 空洞 + 五元组 `Basis`）⇒ 作为 **V-24-2 killer 判据**的对照面 |
| 5 | **私有事件规则**（`event_alphabet.hh` + schema 约束） | ① `flow` 映射 ② 等价核心锚 或 **architect 批准记录**（`CP-Q-1a` 裁决 AC-3）③ 逐字断言 ④ **原子性**（⛔ 禁组合）⑤ 「无等价」占比 > 半数 ⇒ 触发锚集复审 |
| 6 | **闸**（新 `scripts/test/cep_ssp_check.sh`） | 覆盖 **A-1 ~ A-14**（见 §验收标准）· 每条带**反向 fixture**（红因有签名）|
| 7 | **main spec**（新建 `openspec/specs/cep-ssp-declaration/spec.md`） | **冻结面新增 1**（96 → 97）⇒ 五步链 1 次 |
| 8 | **计数同步面** | 闸 55→56 · `test_cep_*.cc` 80→81 · spec `cep-*` 50→51 · `scripts/README.md` · 登记册 §0.1 · `include/tlm/cep/AGENTS.md` 文件索引 |

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **⚠️ 回归口径**：**零回归证据 = 全量 ctest（⛔ 不按 `-R` 过滤）+ 全部 `cep_*.sh` + 非 `cep` 只读闸 + `docs_sync --strict`**。
> **来源**：`ADR-CEP-24` §6 的 **V-24-1~13** · `CP-Q-1a` 裁决 §8 的 **8 条 AC**。⛔ 只列**批 1 可判**的。

- [ ] **AC-1（✱ V-24-2 killer · 逐格对账）**：`ssps.json` 的 MESIF SSP 与 `mesif_state_spec.hh` 的 `kTable` **逐格相等**（含 `0xFF` 空洞 + 逐格 `Basis` 五元组）；**双向**（任一侧改一格 ⇒ 必红）。
- [ ] **AC-2（loader fail-closed）**：未知状态 / 未知事件 / 未登记 `Basis.ref` / 第 5 个 perm / 非 10 流 flow ⇒ 按**正确原因**红（红因有签名）。
- [ ] **AC-3（事件 flow ∈ 10 流）**：schema `enum` + 集合校验（`CP-Q-1a` 裁决 AC-1）。
- [ ] **AC-4（核心锚引用 ∈ 14 锚集）**：`event_alphabet.hh` 常量集 + 集合校验（裁决 AC-2）。
- [ ] **AC-5（无等价锚须 architect 批准记录）**：批准记录存在性校验（裁决 AC-3）。
- [ ] **AC-6（事件名逐字断言）**：字面清单 + 毒化（裁决 AC-4；先例 `kEventNames`）。
- [ ] **AC-7（核心锚集封闭）**：规模常量 + 反向存在性闸（裁决 AC-5）。
- [ ] **AC-8（私有事件原子性）**：⛔ 禁私有事件引用/组合其他私有事件（裁决 AC-7）。
- [ ] **AC-9（「无等价」占比可观测）**：占比 > 半数 ⇒ 触发锚集复审（裁决 AC-8）。
- [ ] **AC-10（实例面 14 属性不动 · V-24-1）**：`test_cep_schema.cc:443` 仍绿。
- [ ] **AC-11（协议中立 · V-24-3）**：SSP 数据文件是**协议名唯一允许面**，通用层代码零协议名（`INV-CEP-07A-IC2`）。
- [ ] **AC-12（表述与措辞禁令 · V-24-5/10/13）**：声明面不表达 belief / token / atomic / 瞬态态 · 无「保证无死锁」类措辞 · 事件集 ⊆ 已登记词表 · `family` ⊆ 3 值。
- [ ] **AC-13（回归 · V-24-12）**：既有 8 实例行为逐字不变（全量 ctest + 55 闸零红）。
- [ ] **AC-14（表达力表述）**：用「**3/6 体裁 / 2/6 干净覆盖**」，⛔ 不得写 6/6。

## Out of Scope（批 2 及其它）

- ⛔ **通用解释器**（`ssp_interpreter.hh`：把声明表跑成协议行为）⇒ **批 2**
- ⛔ **装配集成**（SSP → engine 的 seam）⇒ **批 2**
- ⛔ **掩码格扩展**（TileLink 的 `target_mask`）⇒ **批 2**（contract 面扩展，须独立评审）
- ⛔ **「能实验（4 件套）」端到端判据** ⇒ **批 2**
- ⛔ **既有 6 份 `*_state_spec.hh` 的迁移**（`ADR-CEP-24` §8 F3）
- ⛔ **`CP-Q-1b`**（键子集 / `family` / `protocol_version`）⇒ 仍待裁，**不阻塞本 change**
