# design: cep-fence-consistency-effect-derivation —— fence 一致性效果的派生声明面

> **来源**：`proposal.md`（同目录）· **gap**：G4（形态 **G4-a** · 用户 2026-11-24 裁决）· **形态模板**：G10
> **台账**：Entry **#544**（含推导规则表 + metis 12 条 finding 处置）· **metis**：1 轮（P0×4 全闭）
> **范围纪律**：⛔ **只登记现状** · ⛔ **不补** fence 协议语义（归 ArchForge）· ⛔ 零 schema / instances / ADR 正文 / invariant 变更 · ⛔ 零协议头改动 · ⛔ 与 G1 / G10 正交

---

## D-1 落点（3 个新代码面 + 1 个新契约面）

| 文件 | 角色 | 内容 |
|---|---|---|
| `include/tlm/cep/fence_effects.hh`（**新增**） | **派生声明面**（通用层 · 协议中立 · 零协议类名） | `FenceEffect` 枚举（7 值 · **方向化**）+ `ConsistencyFamily` + `consistency_family_of()` + `derive_fence_effect()`（constexpr · 唯一推导入口）+ **由函数生成**的 8 行表 + 自洽谓词 + **6 个诚实边界常量** |
| `test/test_cep_fence_effects.cc`（**新增**） | **判据测试** | **编译期**：逐行 `effect == derive(行输入)`（8 条 static_assert · 失败签名带实例 id）· **独立 oracle**（硬编码期望表 ⇄ 派生值）· **死分支合成行正控**（TSO / PLO / RC-acquire-only / 跨族 / 未知标签）· **运行期**：输入副本漂移 ⇄ JSON · 双向零 orphan · 边界常量 · 谓词变异副本负控 |
| `scripts/test/cep_fence_effects_check.sh`（**新增**） | **契约闸** | 8 条判据 + **2 条反向 fixture**（各按自己的红因签名）+ `--root` / `--selftest`（拷贝根 · 零污染） |
| `openspec/specs/cep-fence-consistency-effect-derivation/spec.md`（**新增**） | **main spec**（**冻结面新增 1**） | 6 个 Requirement（含**规则表 R1–R7** ⇒ 随链 ratify）；delta 由主 spec **提取生成** |

⛔ **不改** `protocol_instance.schema.json` / `protocol_instances.json` / `engine_capabilities.hh` / `hop_timing_capabilities.hh` / 任何 `*_protocol_engine.hh`。

## D-2 推导规则（决策面）与枚举

| # | 规则 | 优先级 |
|:--:|---|:--:|
| R1 | `has_barrier == false` ⇒ **None** | **最高**（遮蔽模型规则） |
| R2 | 标签**跨族** ⇒ **Invalid** | 次高 |
| R3 | SC ⇒ **None**（与 barrier 存在与否无关） | 模型内 |
| R4 | TSO ⇒ **FullBarrier** | 模型内 |
| R5 | RC ⇒ **按方向逐项**（论文 RC 行 = **并集上界** ⇒ 本单元取**子集**） | 模型内 |
| R6 | PLO ⇒ **Partial** | 模型内 |
| R7 | 标签族 ∉ 枚举 ⇒ **Invalid** | 兜底 |

**枚举方向化**（metis P0-1 ⇒ 消解 moesi_gpu 无合法落点）：`None` / `ReleaseOnly` / `AcquireOnly` /
`ReleaseAcquire` / `FullBarrier` / `Partial` / `Invalid`。
⚠️ R5 的「read=RC-data ⇒ 无 acquire」解读为 **provisional**（待 architect 追认）。

## D-3 判据骨架

| 判据 | 形态 | 红因码 |
|---|---|---|
| **J-1** | 实例集双向零 orphan | `fence-orphan-missing` / `-extra` |
| **J-2** | 派生正确性 —— **独立 oracle**（测试内硬编码期望表）· ⚠️ **闸不重算派生值**（⛔ 防自证） | `fence-oracle-missing`（oracle 缺失 ⇒ 判据退化） |
| **J-3** | 输入副本漂移（头文件副本 ⇄ JSON 逐字段） | `fence-input-copy-drift` |
| **J-4** | 域外必红（跨族 / 未知标签） | `fence-derivation-out-of-domain` |
| **J-5** | JSON 缺键必红（⛔ 不静默 SKIP） | `fence-json-key-missing` |
| **J-6** | basis 非空 + id 唯一 + 诚实边界常量 | `fence-empty-basis` / `-duplicate-id` / `-boundary-constant-missing` |
| **J-7** | 通用层零协议类名 / 零 include 协议头 + 与 G1 / G10 正交 | `fence-generic-protocol-name` / `-include` / `fence-orthogonality-violation` |
| **J-8** | 规则表落在契约面（main spec 的 R1–R7） | `fence-rule-table-missing` |

### D-3.1 「防自证」的三重构造（metis P0-2 / P0-3 的落法）

1. **表行由函数生成** ⇒ 行值与规则**不可能**不一致（编译期保证）⇒ 但这也意味着「表」本身**不构成**独立证据。
2. **独立 oracle**（测试内**硬编码**期望表 ⇄ 编译出的派生值）⇒ 规则实现写错 ⇒ 红（**真证据**）。
3. **闸不重算**派生值 ⇒ 闸与测试**不共享**同一推导路径 ⇒ 任一路径错都会被另一条抓住。
   ⚠️ 由此产生的**新真源面**：头文件内含 labels / barrier 的**镜像副本** ⇒ 由 **J-3**（逐字段比对）兜底
   ⇒ 副本是**受监督的镜像**，不是第二真源。

## D-4 两条反向 fixture（各按**自己的**红因签名）

| # | 变异 | 期望红因 |
|:--:|---|---|
| 1 | 拷贝根头文件注入域外标签（`RC-acquire` → `XYZ`） | `fence-derivation-out-of-domain` |
| 2 | 拷贝根 JSON 改标签（`mesif.read` → `SC`）而头文件副本不动 | `fence-input-copy-drift` |

⇒ 跑在 **mktemp 拷贝根**（唯一临时名）⇒ **零工作树污染**；⛔ **不与** `cep_freeze_reverse_verify.sh` /
`cep_decl_reverse_verify.sh` 共享任何工作树文件（per 台账 Entry #543 ⑨ / Stage B'''' 止血教训）。

## D-5 计数同步面（AC-13）

| 面 | 变更 |
|---|---|
| `scripts/test/cep_*.sh` | +1 |
| ctest 条目 | +2 |
| `test/test_cep_*.cc` | +1 |
| 主 spec | +1（`cep-*` +1） |
| `scripts/README.md` | 守门表新增行 |
| 登记册 `obj-cep-platform-remaining.md` §0.1 | 闸族 / CEP 测试 / 主 spec / OpenSpec 四行 |
| `include/tlm/cep/AGENTS.md` | 文件索引新增本头（头文件数 +1） |

## D-6 冻结面（**新增 1** ⇒ 五步链 1 次）

- 新建 main spec ⇒ 命中 `openspec/specs/cep-*` 冻结 glob ⇒ `--update` 期望「**新增 1 · 移出 0 · hash 变更 0**」· `artifact_count` **+1**（⛔ 数字以**命令实测**为准，⛔ 不写死）。
- ⛔ 其余变更面（`include/` / `test/` / `scripts/`）**不在**冻结集 ⇒ 零 hash 变更。
- ⚠️ **规则表 R1–R7 随链 ratify** ⇒ 本单元**不是**「零决策变更」⇒ 台账 Entry #544 记录该决策面。
