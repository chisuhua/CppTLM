# design: cpptlm-cep-adr-verification-carriers

> **角色**:builder · **上游**:`openspec/changes/cpptlm-cep-adr-verification-carriers/proposal.md` + improvement `.rddf/improvements/cpptlm-cep-adr-verification-carriers.md`(v2 · metis 第 1 轮 14 MUST + 9 SHOULD + 6 NIT 全处置)· 台账 **Entry #431**
> **范围**:`ADR-CEP-21` 位宽状态机器锁 + `ADR-CEP-20` 纳入决策块闸覆盖 · **零 `include/tlm/cep/**` 改动 · 零闸代码改动 · 零 ADR 改动**

## D-1 · W1 判据形态 = **两个数值锚**(⛔ 非恒等式)

**决策**:新增 1 个 TEST_CASE,两条断言:

```cpp
REQUIRE(tlm::cep::kResponderMappedBits == 4u);      // 宽度状态
REQUIRE(tlm::cep::kMappedResponderMask == 0xFu);      // 掩码数值锚
```

**理由**:
1. `kMappedResponderMask` 的**定义**(`sharer_tracker.hh:141-142`)= `(kResponderMappedBits >= 64) ? ~0 : ((1 << kResponderMappedBits) - 1)` ⇒ 断言 `== (1 << kResponderMappedBits) - 1` **恒真** ⇒ 违反 `test_cep_sharer_tracker_base.cc:12` 自订的「不做恒真的常量存在性检查」纪律 ⇒ **零判别力**。
2. **数值锚 `0xF` 有牙齿** —— 本轮**实测**(影子树探针,宽度毒化为 8):运行打印 `mask=0xff` ⇒ 该断言必红 ⇒ 正是 `ADR-CEP-21:47` 所指的**文本锚盲区**(源码文本不变而语义漂移)的唯一堵法。
3. 宽度断言 `== 4u` 把「阶段 2 **未执行**」从**人肉复核**变成**机器判据** ⇒ 兑现 `ADR-CEP-21` **V1-6** 的自认真空。

⚠️ **⛔ 不写成编译期 `static_assert`**:那会让**阶段 2 的合法扩容也编译不过** ⇒ 与 ADR 的两阶段设计**直接冲突**(metis SHOULD-1 ⇒ 登记为 R-4,⛔ 不追)。

⚠️ **⛔ 不新增测试文件**:`test/CMakeLists.txt:6` 的 `file(GLOB TEST_SOURCES "test_*.cc")` 在 **configure 时**展开 ⇒ 新文件须重跑 cmake ⇒ 改现有文件成本最低。

## D-2 · W2 快照 = **期望值用闸自己的函数实测**

**决策**:`test/cep_decision_block_snapshot.json` 加 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`;`count` 22→**23**;`total` 125→**131**;`authorized_by` → `Entry #431`。

**理由**:期望值**不得**从被检对象自身派生(闸文件头 D-4 明文)。⇒ 用**闸自己的** `analyze_section2` 实测:

```bash
python3 -c "import importlib.util as u; s=u.spec_from_file_location('g','scripts/test/cep_decision_block_check.py'); \
g=u.module_from_spec(s); s.loader.exec_module(g); \
print(g.analyze_section2(g.section2(g.strip_fences(open('docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md').read()))))"
# ⇒ (6, [])
```

⚠️ **⛔ 不用外部 `grep -c`** —— 闸口径(剥围栏 → 取 `## 2` 章 → 仅 `###` 级 → 编号正则全匹配)与 grep 可能不同。⇒ **若实测 ≠ 6,以闸口径为准**并在本 change 内订正。

⚠️ **闸的 R-d 段只遍历快照里的 ADR**(`cep_decision_block_check.py:247`)⇒ 加了它,闸**立即**开始对它逐份比对 ⇒ **期望值错 ⇒ 闸立刻红** ⇒ 这正是本 change 的牙齿。

⚠️ **`ADR-CEP-21` 不入快照** —— 该 ADR **无 `## 2` 决策章**(章节为 `## 背景与问题` / `## 决策` / `## 选项对比表` / …)⇒ 闸的 `section2()` 返回 `None` ⇒ **结构性不适用** ⇒ 塞进去只会造出「期望 0 块」的**虚假断言**。

## D-3 · W4 持久 fixture = **影子树 + 探针 + 影子树变异**

**决策**:新增 `scripts/test/cep_adr_verification_carriers_reverse_verify.sh`,**只写 `mktemp -d`**,4 条闭环:

| # | 毒化 | 手法 | 期望 |
|:--:|---|---|---|
| ① | 无 | — | **绿** |
| ② | **宽度** | 影子树复制 `include/tlm/cep/sharer_tracker.hh` ⇒ 改 `kResponderMappedBits` 4→8 ⇒ 编译**最小探针 TU** ⇒ **运行期退出码**即判决 | **红**(退出码 ≠ 0) |
| ③ | **覆盖** | 影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 一个 `### 决策 N` 段 ⇒ `python3 scripts/test/cep_decision_block_check.py --root <影子树>` | **红** + 输出含 `决策块计数快照偏离` **与** `ADR-CEP-20` |
| ④ | 恢复 | — | **绿** |

**理由 / 关键约束**:
1. **探针而非测试二进制重编译** ⇒ 编译**快**且**归因清晰**。探针用**运行期判定**(`return (kResponderMappedBits == 4u && kMappedResponderMask == 0xFu) ? 0 : 1;`)⇒ **退出码**即判决 ⇒ ⛔ **编译失败不会被误当成判据失败**。⚠️ **编译失败 ⇒ exit 2 环境错误**(⛔ 仓内铁律:编译失败不得冒充判据失败,per `cep_flow_support_reverse_verify.sh:16`)。
   - **本轮已实测该形态可行**:影子树毒化 ⇒ `g++ -std=c++17 -I<影子树>/include -I<真实仓>/include` ⇒ 编译 rc=**0** ⇒ 运行退出码 **1**。
2. **⛔ 不毒化快照** —— 闸的 `load_snapshot()` 固定读**真实仓根**的 `ROOT/test/...`;`--root` **只**重定向**文档语料** ⇒ **已签批契约**(`openspec/specs/cep-invariant-gate/spec.md` 的「判据 SHALL 区分『文档语料』与『被指物存在性』两个根」Scenario)明文如此 ⇒ **快照毒化在不改仓内文件的前提下不可实现** ⇒ 覆盖证明只能走**影子树改 ADR**。
3. **⛔ 不复用 wrapper 的固定影子路径**(`/tmp/opencode/cep-decision-block`)⇒ 用 `mktemp -d` + `trap` ⇒ 并行安全。
4. **仓内零改动**自证用 **`sha256sum` 前后比对**(⛔ 不用 `git diff --quiet` —— 脏工作区会假红)。

## D-4 · 冻结链 = **3 份产物** + **07 文档的同步时序**

**决策**:冻结链 = 台账 Entry(**归档期**那条)→ 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单**单独** commit → 无参复验 rc=0。

**3 份冻结产物**:
1. `openspec/specs/cep-sharer-tracker-base/spec.md`(delta 合并 ⇒ 2 条新 Scenario)
2. `openspec/specs/cep-invariant-gate/spec.md`(delta 合并 ⇒ `:40` 的 125/22 → 131/23)
3. `docs/architecture/07-coherence-exploration-platform.md`(`:590` 的 22 份/125 → 23 份/131)—— **手工同步**(不是 delta)

⚠️ **07 文档的同步必须放在 `archive` 之后、`--update` 之前**:
- 若在 `archive` **之前**改 ⇒ 冻结闸在归档前就报红 ⇒ 污染「归档前 43 闸全绿」的复验口径;
- 若在 `--update` **之后**改 ⇒ 清单里它的 hash 是**旧的** ⇒ 无参复验必红。
⇒ **正确位置**:`archive` → 改 07 文档 → 产物 commit → `--update`。

⚠️ 归档**禁 `--skip-specs`**(本 change 是「✅ 已完成归档」语义 ⇒ 必须合并 delta)。⚠️ `artifact_count` **不变**(86;不新增冻结文件)。

## D-5 · 边界(与 proposal 一致,零放宽)

⛔ `include/tlm/cep/**` 零 diff · ⛔ `scripts/test/cep_decision_block_check.py` 零 diff · ⛔ `docs/adr/**` 零 diff · ⛔ 新增 `INV-CEP-*` = **0** · ⛔ 新增 spec Requirement = **0**(只 MODIFIED)· ⛔ **不**执行 `ADR-CEP-21` 阶段 2 · ⛔ **不**满足/替代 `V2-6` · ⛔ **不**把 `ADR-CEP-21` 塞进快照 · ⛔ **不**追加任何 ADR 的 `## Status Update` · ⛔ **不**勾选任何 ADR 的 Verification checkbox。
