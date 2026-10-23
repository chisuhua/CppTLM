---
name: cpptlm-cep-tools-test-isolation
priority: P2
phase: phase-10
主题: CEP tools 契约测试的临时目录隔离修复 —— 消除 `ctest -j4` 下的偶发失败
依赖:
  - ADR-CEP-06
  - ADR-CEP-14
状态: Proposed
created: 2026-10-23
---

# cpptlm-cep-tools-test-isolation: CEP tools 契约测试临时目录隔离修复

> **状态**: 🟡 **Proposed**(2026-10-23,planner 起草)—— builder 实施
> **关联路线图**: [`phase-10`](../roadmap/phases/phase-10.md)(in-progress)+ [`objective-cep-platform`](../roadmap/objectives/obj-cep-platform.md)(§11 **Entry #24**)
> **根因**: 台账 Entry #22 ⑩ **L5** 的「ctest 偶发失败 1 次(未复现)」—— **2026-10-23 已定位根因**
> **边界**: 仅改**测试代码**(`test/test_cep_tools_contract.cc`);**零** ADR / 冻结面 / schema / tools 改动;**零** ABI 改动

---

## 架构依据

| 依据 | ADR | 决策要点 |
|------|-----|---------|
| 实验输出契约 | `ADR-CEP-06` §2 | 4 件套契约 —— 测试临时目录是契约的载体,须可并发 |
| Stub 可验证性合同测试 | `ADR-CEP-14` | C 系列合同测试须**可重复、可并行**,否则门本身不可信 |

---

## 范围

**In-Scope**:
- `test/test_cep_tools_contract.cc` 的临时目录**唯一化**(进程级隔离)
- 同一文件内其它固定名临时路径(`cep_tools_ab_out` / `cep_ast_dep_check.py`)

**Out-of-Scope**:
- ❌ tools 脚本(`tools/*.py`)本身的任何改动 —— **冻结面**
- ❌ `tools/render_report.py` 等 3 个 py 的行为变更
- ❌ ADR / schema / 冻结清单

---

## Why

**现象**:`ctest --test-dir build -j4` 偶发 1 个用例失败(3~4 次复现 1 次),**串行必绿**。

**已定位的根因(2026-10-23 实测复现)**:

```
Test #78: cep_tools_contract  →  cpptlm_tests "[cep][tools]"
Test #82: cep_all            →  cpptlm_tests "[cep]"      ← 也含 [cep][tools] 用例
```

两个 ctest 项在 `-j4` 下**并行执行同一组用例**,而 `test/test_cep_tools_contract.cc` 的 `make_exp_dir()` 使用**固定名**临时目录:

```cpp
const auto base = std::filesystem::temp_directory_path() / ("cep_tools_" + tag);   // tag = gate_good / gate_bad / ab_run
std::filesystem::remove_all(base);          // ← 进程 A 清掉进程 B 正在用的目录
```

→ 进程 A 的 `remove_all` 与进程 B 的 `render_report` 读写**竞态** → 断言 `html.find("一致性违例")` 读到**已被删除/覆盖**的 `report.html` → `npos` → 失败。

**为什么必须修**:
1. **守门不可信** —— 一个会随机红的门,等于没有门。CEP 的 4 个守门脚本 + 96 个 ctest 项是本仓对「契约层自洽」的**唯一**机器证据;它随机红,会让「机器检查全绿」这句话失去意义(Entry #22 ⑫ 已把动态量快照登记为口径,更不能容忍快照本身随机)。
2. **它已被当成真信号** —— 台账 Entry #22 ⑩ L5 曾因「未复现」被登记为观察项;现在根因明确,**观察项应转为待修项**。
3. **修复面极小** —— 单文件、数行,零冻结面影响。

---

## What Changes

### 修改文件(1 个)

| 文件 | 改动 |
|------|------|
| `test/test_cep_tools_contract.cc` | ①`make_exp_dir()` 的目录名加**进程唯一后缀**(PID 或单调 token):`temp_directory_path() / ("cep_tools_" + tag + "_" + unique_token())`;②`cep_tools_ab_out`(`:124`)与 `cep_ast_dep_check.py`(`:183`)两处固定名路径同样唯一化;③**清理路径**保持不变(`remove_all` 各自只删自己的目录) |

> **实现约束**:唯一化 token 必须在**同进程内稳定**(同一 TEST_CASE 内多次调用得到同一 token),跨进程必须不同 —— 推荐 `std::to_string(::getpid())`(+ 可选单调计数)。**不得**引入新的第三方依赖。

### 为什么不改 ctest 注册

`set_tests_properties(cep_all PROPERTIES RUN_SERIAL TRUE)` 也能消除撞车,但那是**绕过**:两个 ctest 项仍可能同时被手工并发执行,且 `--repeat until-fail` 场景仍会撞。**正解是路径唯一化**。

---

## Non-Goals

- ❌ 不改 tools 脚本行为(冻结面)
- ❌ 不改断言强度(不改测试的判据,只改其执行环境)
- ❌ 不新增 `[cep]` 标签的用例

---

## Capabilities

#### 1. 测试执行环境隔离(新增 —— 非能力,是门的基础属性)

- 同一 ctest 集合内的多个测试项**并行**执行时,不得共享任何可写临时路径
- 临时路径唯一性:**跨进程必异、进程内稳定**

---

## Acceptance

> **机器验证要求**(per `rdd-planner/SKILL.md`):每条给出**可执行命令 + 期望值**。

### A1. 并行复现不再失败(核心判据)

- [ ] `ctest --test-dir build -j4 --repeat until-fail:20 -R "cep_(all|tools_contract)"` 退出码 == **0**,且输出**零** `Failed`
  - **修复前基线**:该命令在 3~4 次中复现 1 次失败(`test_cep_tools_contract.cc:115` → `html.find("一致性违例")` == npos)
- [ ] `ctest --test-dir build -j4 --repeat until-fail:20` 退出码 == **0**(全 96 项,20 轮)

### A2. 单测与标签不回归

- [ ] `./build/bin/cpptlm_tests "[cep][tools]"` 退出码 == **0**
- [ ] `./build/bin/cpptlm_tests "[cep]"` 退出码 == **0**(**76 case / 1284 assertion** 不变)
- [ ] `ctest --test-dir build -R cep` 退出码 == **0**(**18/18** 不变)

### A3. 冻结面与契约面零改动

- [ ] `git diff --name-only` 输出**不含** `docs/adr/ADR-CEP-*.md` · `docs/architecture/07-coherence-exploration-platform.md` · `include/tlm/cep/` · `experimental/metrics.schema.json` · `tools/` · `openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/`
- [ ] `bash scripts/test/cep_freeze_check.sh --strict` 退出码 == **0**
- [ ] `bash scripts/test/cep_docs_sync_check.sh --strict` 退出码 == **0**

### A4. 台账闭环

- [ ] `obj-cep-platform` §11 追加台账 entry(记录 L5 根因关闭 + 修复 commit),且 **Entry #1 ~ #24 正文零改写**

---

## Impact

### A. 行为影响

- **零**生产代码改动;**零**冻结面改动;**零** ABI 改动
- **零**对外可观测行为变化(仅测试临时路径的命名)

### B. KPI

- Stage 0 仍占 1 个 KPI slot(`cpptlm-cep-umbrella`);本项属 **Stage 0 收尾修补**,**不新增 change、不新增 slot** —— 在 umbrella change 的 tasks.md 内追加一条 task 即可
- **不触碰**「任一时刻 Proposed 并发 ≤ 1」

### C. 测试增量

- `[cep]` case 数不变(**76**);仅消除偶发失败
- 新增**回归判据**:A1 的 `--repeat until-fail:20` 并行复现命令(可纳入 CI)

### D. 风险

| 风险 | 等级 | 缓解 |
|------|:----:|------|
| 唯一 token 在同进程内不一致 → 清理路径漏删 | 🟢 低 | token 由 PID + 单调计数生成,进程内稳定 |
| `--repeat until-fail` 让 CI 时间翻倍 | 🟢 低 | 仅对 `-R "cep_(all\|tools_contract)"` 子集跑 20 轮;全量 CI 不开 repeat |

---

**版本**: v1.0(2026-10-23,planner 起草;根因经 3~4 轮 `ctest -j4` 复现定位)
**最后**: 待 builder 实施
**作者**: CppTLM build agent(planner mode)
