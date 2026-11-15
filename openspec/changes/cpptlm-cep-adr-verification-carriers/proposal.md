# proposal: cpptlm-cep-adr-verification-carriers —— 已签批 ADR 的 Verification 判据补机器载体

> **状态**: 🟡 Proposed（2026-11-15 立项 · planner 起草 · 待 builder 补 design/tasks 并实施）
> **依据**: improvement `.rddf/improvements/cpptlm-cep-adr-verification-carriers.md`（v2 · metis 第 1 轮 **14 MUST + 9 SHOULD + 6 NIT 全处置**）· `ADR-CEP-21` V1-6 / V2-6 / §背景 47 · `ADR-CEP-00` SU 2026-11-11 §⑤ · 台账 **Entry #431**
> **KPI**: 立项后 Proposed **0/3 → 1/3**

## 为什么

本 change 补**两个**已签批 ADR 的 Verification 判据**机器载体**缺口。二者同域：**「ADR 说要有判据，但仓内没有能令它变红的载体」**。

### 缺口 1：`ADR-CEP-21` 的「阶段 2 未执行」无机器载体（ADR 自认的真空）

`ADR-CEP-21` 把 responder 位宽扩容设计成**两阶段**：阶段 1 机制化（本 ADR 成立即生效）· 阶段 2 触发式扩容（默认 `W = 8`）。两阶段之间**唯一**的闸门是「阶段 2 **未执行**」这个状态。

**实测（2026-11-15，全仓 grep + 影子树编译探针）**：

| 载体 | 现状 | 缺口 |
|---|---|---|
| `kResponderMappedBits` | `include/tlm/cep/sharer_tracker.hh:92` = `4` | ⚠️ **全仓无任何测试或闸断言它 == 4** |
| `kMappedResponderMask` | `sharer_tracker.hh:141-142` = `0xF` | ⚠️ **无断言它是 `0xF`** |
| 文本锚 | `scripts/test/cep_tilelink_adapter_reverse_verify.sh:49` 的 `join-no-unmapped-reject` 锚 = 源码**文本** `if ((extra_responders & ~kMappedResponderMask) != 0)` | ⚠️ `ADR-CEP-21:47` **自认**：常量 4→8 时**源码文本不变 ⇒ 该闸照绿** |

⚠️ **可复现证据**（本轮影子树探针，宽度毒化为 8）：编译 rc=**0**、运行打印 `kResponderMappedBits=8 mask=0xff` ⇒ **mask 由 `0xF` 变 `0xFF`，而源码文本一字未改** ⇒ 文本锚闸**确实**照绿 ⇒ ADR 的登记**成立且可复现**。

⇒ **代价**：任何人把常量从 4 改成 8（哪怕误改、哪怕「提前准备」），`ADR-CEP-21` 的「阶段 2 未执行」前提被**静默破坏** —— 而阶段 2 需要 **architect 裁决 + 台账 Entry**（2b 人裁段）⇒ **一个本该被人裁的事，变成了一条无人察觉的改常量动作**。且 `kResponderBitToNode` 数组尺寸**已是 8** ⇒ 改常量**不会编译失败** ⇒ 连编译器都不报警。

### 缺口 2：决策块治理闸对 `ADR-CEP-20` 零覆盖

决策块闸 `scripts/test/cep_decision_block_check.py` 的 R-d 段**只遍历闸外快照里的 ADR**（源码 `:247` `for basename in sorted(files):`）⇒ **不在快照里的 ADR 零覆盖**。

| ADR | `## 2` 决策章 | 决策块 | 在快照? | 闸的覆盖 |
|---|:--:|:--:|:--:|:--:|
| `ADR-CEP-00` ~ `-19` + `-00A` + `NUMBER-MAP`（22 份） | ✅ | 3~10 | ✅ | ✅ 逐份比对 + 反向变异 |
| **`ADR-CEP-20`**（2026-11-13 签发，Entry #402） | ✅ | **6** | ❌ | ❌ **零覆盖** |
| `ADR-CEP-21`（2026-11-14 签发，Entry #421） | ❌ **无 `## 2` 章** | — | ❌ | ✅ **结构性不适用**（非缺口） |

⚠️ 期望值 **6** 由**闸自己的** `analyze_section2` 实测得出（`(6, [])`），⛔ 非外部 `grep`。

⇒ **代价**：`ADR-CEP-20` 的 6 个决策块**不受任何机器判据守** ⇒ 未来若有人把「决策 3」从 `### 决策 3` 降级为 `#### 决策 3`（或增删决策块），**闸不会红** —— 而闸对另外 22 份 ADR **会**红 ⇒ **同一仓内两套覆盖强度**。且快照覆盖集合**无自愈机制** ⇒ 盲区**单调增长**。

### ⛔ 认识论边界（不得过度声称）

- 缺口 1 的判据是「**当前状态的变更检测器**」：断言「现在是 4 / 现在是 `0xF`」，**不**论证「4 是正确的宽度」⇒ 正确性来自 `ADR-CEP-21` 的阶段 2 触发判据（2a 机判 / 2b 人裁），本 change **不动**该判据。
- 缺口 2 的快照是「**覆盖集合的显式登记**」⇒ 之后 `ADR-CEP-20` 的决策块**变更必须同步快照**（否则闸红）⇒ 是**变更检测义务**，**不是**「决策块计数正确」的独立裁决。
- ⚠️ 本 change **只 bump 一次快照**，**不解决「自愈」**（新签发 ADR 仍需人工登记）⇒ 登记为残余 R-3。
- ⚠️ `ADR-CEP-21` 的 V1-6 checkbox **保留为 `- [ ]`**（ADR 不可变、不可勾选）⇒ 「载体已存在」的事实**以台账 Entry #431 为准**。

## 变更内容

**范围 = 6 个产物 + 1 份台账 Entry**，详见 improvement 的 §What Changes。

| # | 内容 | 文件 |
|:--:|---|---|
| **W1** | **判据**：新增 1 个 TEST_CASE（`[cep][sharer-tracker]`）—— ① `kResponderMappedBits == 4u`（阶段 1「未扩容」钉死）② `kMappedResponderMask == 0xFu`（**数值锚**，堵住 `ADR-CEP-21:47` 的文本锚盲区）。⛔ **不用恒等式** `== (1<<k)-1` —— 定义**就是**该式 ⇒ 恒真 ⇒ 违反 `test_cep_sharer_tracker_base.cc:12` 自订的「不做恒真的常量存在性检查」纪律 | `test/test_cep_sharer_tracker_base.cc`（改） |
| **W2** | **快照**：`files` 加 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`；`count` 22→**23**；`total` 125→**131**；`authorized_by` → `Entry #431` | `test/cep_decision_block_snapshot.json`（改） |
| **W3** | **delta spec**：**MODIFIED** `responder 位宽扩容的机制与触发判据`（加 2 条 Scenario）；**MODIFIED** `cep-invariant-gate`（`:40` 的「实测 **125** / **22**」→ **131 / 23**）。⛔ **不新增 Requirement**（无载体要求 = 本仓反复打击的「有登记、无牙齿」） | `specs/cep-sharer-tracker-base/spec.md` + `specs/cep-invariant-gate/spec.md`（新） |
| **W4** | **持久反向 fixture**：**只写 `mktemp -d`** ⇒ 4 条闭环 —— ① 未毒化绿 ② **宽度毒化**（影子树复制 `sharer_tracker.hh` ⇒ 改 4→8 ⇒ 编译**最小探针 TU** ⇒ 运行期**退出码**即判决 ⇒ 非零 = 判红；⚠️ **编译失败 ⇒ exit 2 环境错误**，⛔ 不计入「红」）③ **覆盖毒化**（影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 一个 `### 决策 N` 段 ⇒ 闸必红且输出含 `决策块计数快照偏离` **与** `ADR-CEP-20`）④ 恢复态绿。⛔ **不毒化快照文件**（闸的 `load_snapshot()` 固定读真实仓根；`--root` 只重定向文档语料 ⇒ **已签批契约** `cep-invariant-gate` 明文如此） | `scripts/test/cep_adr_verification_carriers_reverse_verify.sh`（新） |
| **W5** | **台账 Entry #431**（planner 域，append-only）—— 兼作 W2 快照的 `authorized_by` ⇒ **必须先于** W2 落盘（闸 `:207-218` 会检查该 Entry 真实存在） | `.rddf/roadmap/objectives/obj-cep-platform.md` |
| **W6** | **文档连带**：`docs/architecture/07-coherence-exploration-platform.md:590` 的「22 份 ADR … 22 键 + `total`=125」→ **23 份 … 23 键 + `total`=131`（该文件 ∈ **M1 冻结面** ⇒ 归档时进冻结链 ⇒ **冻结链 = 3 份产物**）· `scripts/README.md` 脚本表 +1 行 · `test/CMakeLists.txt` 注册 W4 进 ctest | `docs/architecture/07-...md` + `scripts/README.md` + `test/CMakeLists.txt` |

**⛔ 边界**：`include/tlm/cep/**` **零 diff** · `scripts/test/cep_decision_block_check.py` **零 diff** · `docs/adr/**` **零 diff** · 新增 `INV-CEP-*` 标识 = **0** · 新增 spec Requirement = **0** · **不执行** `ADR-CEP-21` 阶段 2 · **不满足/不替代** `V2-6` · **不**把 `ADR-CEP-21` 塞进快照（无 `## 2` 决策章 ⇒ 结构性不适用）· **不追加**任何 ADR 的 `## Status Update` · **不勾选**任何 ADR 的 Verification checkbox。

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | 宽度常量被机器钉死：`kResponderMappedBits == 4u` 断言在位；**反向**：影子树毒化为 8 ⇒ 探针**非零退出**（判红） |
| **AC-02** | 掩码数值锚有效：`kMappedResponderMask == 0xFu` 断言在位；**反向**：毒化后实测 `mask=0xff` ⇒ 探针非零退出 ⇒ **恒等式做不到这一点**（实证差异） |
| **AC-03** | `ADR-CEP-20` 的覆盖**可证**（⛔ 非「零红」）：影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 一个 `### 决策 N` 段 ⇒ `python3 scripts/test/cep_decision_block_check.py --root <影子树>` ⇒ **rc≠0** 且输出**同时**含 `决策块计数快照偏离` 与 `ADR-CEP-20`；**另加**一条 `#### 决策 7` 变异 ⇒ 输出含 `决策块标题非 ### 级` + 文件名 |
| **AC-04** | 快照自洽 + 授权存在：`count == len(files) == 23`；`total == sum(files.values()) == 131`；`files['ADR-CEP-20-…md'] == 6`；`authorized_by == 'Entry #431'` 且台账 `^### Entry #431` **命中** |
| **AC-05** | `ADR-CEP-21` **不在**快照（负向断言）：`'ADR-CEP-21-sharer-bitmap-expansion.md' not in files` ⇒ 防未来误加「期望 0 块」的虚假断言 |
| **AC-06** | 持久 fixture 四态闭环 + 仓内零改动：脚本 rc=0 且输出「未毒化绿 / 宽度毒化红 / 覆盖毒化红 / 恢复绿」；**仓内零改动**用 **`sha256sum` 前后比对**目标文件（⛔ 不用 `git diff --quiet` —— 脏工作区会假红） |
| **AC-07** | 范围零改动：`git diff --name-only` **不含** `include/tlm/cep/` · `docs/adr/` · `scripts/test/cep_decision_block_check.py`；新增 `INV-CEP-*` 标识 = **0**；新增 spec Requirement = **0** |
| **AC-08** | 0 回归：`[cep]` 用例数 **+1**；`ctest -R cep` 全绿；ctest 条目数 **+1**；43 个 `cep_*.sh`（**按 ctest 同参数**：docs_sync / doc_ref 带 `--strict`）全 rc=0；`agents_md_lint` 通过；`openspec validate --changes --strict` rc=0 |
| **AC-09** | 归档后逐份 `--strict` 不劣化：归档前 `openspec validate cpptlm-cep-adr-verification-carriers --strict` rc=0；归档后 `openspec validate cep-sharer-tracker-base --strict` 与 `openspec validate cep-invariant-gate --strict` **逐份** rc=0（两者现 max 正文长度 = **298 / 348** ⇒ 不逼近 500 阈值） |
| **AC-10** | 归档五步链：顺序固定 —— **#431 台账先落 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #431"` → 清单单独 commit → 无参复验 rc=0**；清单 diff = **3 份产物**的 hash（`cep-sharer-tracker-base/spec.md` · `cep-invariant-gate/spec.md` · `docs/architecture/07-coherence-exploration-platform.md`）+ `authorized_by` + `frozen_at` + `frozen_at_commit` **元数据**，**无其他 artifact hash 变化**；`artifact_count` 不变（86） |

## 不做什么

① **不**执行 `ADR-CEP-21` 阶段 2（扩容到 `W`）—— 那是**触发后**的动作（2a 机判 / 2b 人裁），本 change **不动**；
② **不**满足 / 不替代 `ADR-CEP-21` **V2-6**（阶段 2 组的运行时 mask 判据）—— 只新增**同形的独立**断言；
③ **不**把 `ADR-CEP-21` 塞进决策块快照（**无 `## 2` 决策章** ⇒ 结构性不适用）；
④ **不**改决策块计数口径（只扩**覆盖集合**）· 闸代码**零改动**（其注释里的「22 份」将 stale ⇒ 登记）；
⑤ **不**把 `kResponderMappedBits == 4` 提升为 header `static_assert` —— 那会让**阶段 2 的合法扩容也编译不过** ⇒ 与 ADR 两阶段设计**直接冲突**；
⑥ **不**新增 `INV-CEP-*` 标识 · **不**新增 spec Requirement；
⑦ **不**追加 ADR 的 `## Status Update` · **不**勾选任何 ADR 的 Verification checkbox；
⑧ **不**修 F-1（`openspec validate --strict` 超长 Requirement 群）—— 实测范围为**全仓 87/129 份 spec**，另立 backlog；
⑨ **不**裁决「镜像 20 值的跨仓权威归属」与「ArchForge 缺席时整闸 `exit 0`」—— 属 **architect 判权**；
⑩ **不**实现「决策块闸覆盖集合自愈」—— 需改闸代码 + 独立 change，另立 backlog。
