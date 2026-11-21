# proposal: cpptlm-doorbell-contract-alignment —— doorbell 契约面失真修正（7 处连带）

> **来源**：`.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`（architect 裁决 **Q-12** · 登记册 §6.4 队列 2 单元 **I-12**）
> **⚠️ 域声明**：本 change 的对象是 **dGPU 域**（`openspec/specs/minimal-dgpu-soc-architecture/` + `dgpu-soc-timing/` + `lwc-bus-hub-mvp/` + `dgpu-soc-control-hub/` + `docs/adr/ADR-DGPU-09-*.md`），⛔ **不是 CEP 域**（借 CEP 队列作载体，⛔ 不表示 CEP 范围扩张 —— 先例 = I-10 的「仓级域」）。
> **裁决结论**：采**选项②改契约**（⛔ 非用户倾向①）；**问题不在取值来源，在契约面失真**。

## Why

`openspec/specs/minimal-dgpu-soc-architecture/spec.md` 对**同一行为**写了 **3 个互相矛盾、且被实现全部违反而测试全绿**的断言：

| # | 断言（裁决 §2.2） | 实现事实 | 状态 |
|:--:|---|---|---|
| **A-1** | 「`bar_sizes[1]` **必须 ≥ 256MB+64KB**」 | 三份 shipped 配置**全部** 16MB；**无任何闸检查该 MUST** | ⚠️ **MUST 被全体违反而全绿** |
| **A-2** | 「16MB ⇒ doorbell **访问越界,不触发 doorbell 路由**」 | doorbell **优先匹配** ⇒ 越窗仍生效；smoke 测试用 16MB 配置断言 `rc == 0` | ⚠️ **Scenario 与实现相反而全绿** |
| **A-3** | 「不一致 SHALL 是**已知推迟项**,需在 §13 风险表中显式记录」 | GIVEN 指向 §13 风险表行（该表已指针化到别仓）；「v1.0 仅 ABI 路径」全仓唯一（无落点） | ⚠️ **僵尸条款**（GIVEN 不可满足 ⇒「已记录」永不可达） |

⇒ 处置方式 = **把契约改写到与实现一致**（契约向实现看齐，⛔ **不是**实现向契约看齐 —— 真去实现「16MB ⇒ 不触发路由」会破坏 `test/test_cpptlm_emulator_minimal_soc_smoke.cc` 的 smoke T1.4 断言）。

## What Changes（裁决 §3.1 的 **7 处连带** —— ⛔ 缺任一处 ⇒ 裁决不成立）

| # | 落点 | 处置 | 判据（**措辞锚定** · ⛔ 不用行号）|
|:--:|---|---|---|
| **C-1** | `minimal-dgpu-soc-architecture` · Requirement「BAR 布局与代码真相对齐」 | A-1 的 MUST 降级为「doorbell 用**测试专用合成偏移**,**不依赖** `bar_sizes[1]` 大小」；删「必须 ≥ 256MB+64KB」 | `grep -cF "必须 ≥ 256MB+64KB" <spec>` == **0** |
| **C-2** | 同文件 · Scenario「doorbell 配置过小(检测 config bug)」 | **改写为正向断言**（⛔ 不删除 —— 保留 config-bug 检测意图）：「16MB ⇒ doorbell 写**仍命中**路由,**不**视为 config bug」 | `grep -cF "访问越界" <spec>` == **0** ∧ `grep -cF "仍命中" <spec>` ≥ **1** |
| **C-3** | 同文件 · Requirement「doorbell 配置 vs 代码 hardcoded 不一致」+ Scenario「风险追踪」 | **整条改写**为「doorbell 合成偏移是**已裁定的现状**」（per ADR-DGPU-09 §2.2.2 + d3-roadmap）；GIVEN 重锚到二者；⛔ 删「§13 风险表」与「v1.0 仅 ABI 路径」两处悬空引用 | `grep -cF "§13 风险表" <spec>` == **0** ∧ `grep -cF "v1.0 仅 ABI 路径" <spec>` == **0** |
| **C-4** | `dgpu-soc-timing` · Requirement「BAR 路由优先级」 | **同步**：Requirement 正文补「doorbell 识别**基于偏移量**,**不依赖** BAR1 实窗大小」；⛔ **不动** 3 个含偏移字面量的 Scenario 行 | 该 spec 的 `0x10010000\|kBar1DoorbellOffset` **命中集合逐行不变** ∧ `grep -cE "不触发.*(doorbell\|路由)"` == **0** |
| **C-5** | `lwc-bus-hub-mvp` · Requirement「sdma-doorbell-in-handling」 | **同步**（同 C-4 形态） | 同 C-4 |
| **C-6** | `dgpu-soc-control-hub` | **核对结论：不含**（实测 `0x10010000\|kBar1DoorbellOffset` 命中 == **0**）⇒ ⛔ **无需改**；本条**显式记录**（⛔ 不得静默跳过） | 该 spec 的命中数**裁决前后一致**（== 0） |
| **C-7** | `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` | **追加** `## Status Update` 段（⛔ 正文零改动）：记录 Q-12 裁决已落地 + 指向本 change | `git diff` 对该 ADR **只增行**（既有行 diff 行数 == **0**）∧ 新增行含「Q-12」 |

### 复活锁（C-3 新文本内 · **择一写死 = R-2**）

C-3 新文本**同时**含：① d3-roadmap 的语义触发条件（「**真 driver 集成**时 doorbell **物理地址 ≠ 0x10010000** ⇒ 才回头修」）+ ② **可机判触发带 R-2**（数值比较，⛔ 禁用正则）：

```
任一含 SoC 的配置出现 pcie_ep.params.bar_sizes[1] >= 268435464 (= 256MB+64KB)
⇒ 合成偏移落入 BAR1 实窗内 ⇒「测试专用偏移」的前提失效 ⇒ 必须回头修订
机判: grep '"bar_sizes"' configs/*.json examples/*.json | awk -F'[:,]' '{b=$3+0; if (b>=268435464) print}' | wc -l 命中数 ≥ 1
```

## Out of Scope（裁决 §3.4 的 **N-1 ~ N-6**）

| # | 不做 | 理由 |
|:--:|---|---|
| **N-1** | ⛔ 不改 `docs/designs/dgpu-driver/architecture.md` 的 DOORBELL / RING_DOORBELL 行 | ABI 冻结面（per ADR-DGPU-08 / ADR-DGPU-03）|
| **N-2** | ⛔ 不改 `configs/*.json` 的 `bar_sizes` | ① `framebuffer_size_` 从 `bar_sizes[1]` 派生 ⇒ 256MB BAR1 = 256MB FB 分配；② ADR-DGPU-09 SU 2027-02-09 已把 16MB 写进不变性验证表 |
| **N-3** | ⛔ 不新增 JSON 字段 | 新增键 = 公开 config schema 变更（`param_effect_inventory.py` 有硬编码常量账）且顶层未知键被**静默忽略** ⇒ 预留字段 = 静默死键 |
| **N-4** | ⛔ 不改代码的取值来源（3 处硬编码保持） | 须穿透 `DGpuBoard::kBar1DoorbellOffset` + `PcieEndpointIP::kBar1DoorbellOffset` + `sdma_engine_tlm.cc` 裸字面量 **3 处**；且顶层键静默忽略 ⇒ 配置写了不生效**比 hardcoded 更糟** |
| **N-5** | ⛔ 不复用「改常量弄断既有测试」作为延期理由 | 该理由被夸大（实测值断言仅 1 处 + 4 处字面量，其余为符号引用）|
| **N-6** | ⛔ 不以 `bar1_window_size_` 为前提 | 该字段在 `include/` + `src/` **0 命中**（文档幻觉）|
| — | ⛔ 不改 `docs/designs/dgpu-soc/d3-evolution-roadmap.md` | 🟡 DRAFT 非签发 ADR；裁决只**引用**其触发条件 |
| — | ⛔ 不碰任何 `cep-*` spec / `docs/adr/ADR-CEP-*.md` | ⛔ 不表示 CEP 范围扩张 |

## 验收标准

> **判据纪律**：全部**措辞锚定**、⛔ 不用行号（裁决 §6：本题已有 3 处行号锚点腐化）。
> **⚠️ 一处判据收紧（对裁决 AC-4 的括注修正 · 见 design.md D-4）**：裁决写「红因 = `REQUIRE(rc == 0)` 失败」，**实测不成立**（删除 doorbell 分支后写路径落到 `default: break` ⇒ `rc` 仍为 0）⇒ 本 change 的 AC-4 改以**实测红因**表述。

- [ ] **AC-1（C-1 ~ C-3 · 三处主断言消解）**：归档合并后 `openspec/specs/minimal-dgpu-soc-architecture/spec.md` 满足 —— `grep -cF "必须 ≥ 256MB+64KB"` == **0** ∧ `grep -cF "访问越界"` == **0**（⚠️ **追加项**：裁决原文用 `grep -cF "不触发 doorbell 路由"`，但原文实为 `**不**触发`（带 markdown 粗体标记）⇒ 该判据**恒 0 = 空判据**）∧ `grep -cF "§13 风险表"` == **0** ∧ `grep -cF "v1.0 仅 ABI 路径"` == **0** ∧ `grep -cF "仍命中"` ≥ **1**
- [ ] **AC-2（C-4 ~ C-6 · 连带面同步）**：① 3 份连带 spec 的 `grep -n "0x10010000\|kBar1DoorbellOffset"` **命中集合逐行与裁决前一致**（`dgpu-soc-timing` 3 行 / `lwc-bus-hub-mvp` 3 行 / `dgpu-soc-control-hub` 0 行 —— 逐行内容也须一致）；② **显式底线**：`grep -cE "不触发.*(doorbell\|路由)"` 在 3 份上**逐份 == 0**
- [ ] **AC-3（C-7 · ADR SU 追加）**：`git diff` 对 `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` **只增行**（既有行 diff 行数 == **0**）+ 新增行含「Q-12」+ 指向本 change / 裁决文件
- [ ] **AC-4（M-1 · 决定性变异 · C-2 与实现挂钩）**：影子副本中临时删除 doorbell 优先匹配分支 ⇒ **doorbell 路由断言**变红 ⇒ 还原 ⇒ 绿。⚠️ **变异前必须先证明「当前绿」**；⚠️ **红因判据**：必须是 **doorbell 路由断言失败**（`pcie_ep_doorbell_count()` 断言或等价的 doorbell 消费断言）⇒ ⛔ **编译失败 / 构建错误 / 找不到测试二进制 均不构成红因**
- [ ] **AC-5（M-2 · 三份配置维持 16MB ⇒ 新条款仍满足）**：改动后三份 shipped 配置**字节不变**（`git diff --name-only` 不含 `configs/**` ∧ `examples/**`）∧ 新契约条款被满足
- [ ] **AC-6（M-3 · C-3 非空转）**：影子副本中删掉 C-3 的新措辞 ⇒ 须有判据变红 ⇒ 还原 ⇒ 绿。⚠️ **须与 AC-7 复合执行**：**红因 = AC-7 的 `grep -cF "<复活锁表述>"` 从 1 变 0**；⛔ AC-6 单独不可判
- [ ] **AC-7（复活锁 · 可机判触发条件）**：C-3 新文本含 **R-2** 的机判表述 ⇒ `grep -cF "pcie_ep.params.bar_sizes[1] >= 268435464" <spec>` == **1**
- [ ] **AC-8（范围纪律 · 零越权）**：`git diff --name-only` **不含** `include/**` · `src/**` · `configs/**` · `examples/**` · `docs/designs/dgpu-driver/architecture.md` · `scripts/test/param_effect_inventory.py`
- [ ] **AC-9（0 回归）**：`ctest` 全绿（⛔ 不写死数字）· 全部 `scripts/test/cep_*.sh`（数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` 全 rc=0（⚠️ `format --check` 现有 **21 个既有 C++ 文件**待格式化 ⇒ 本单元零 C++ 改动 ⇒ 须**排除既有债**后判，或**如实记录**该项为既有红）
- [ ] **AC-10（登记册回填）**：登记册 §6.4 队列 2 的 **I-12** 行标记为已落地 + §6.4 队列 1 的 **Q-12** 行补「✅ 已裁（architect 2026-11-20 · 裁决文件 = …）」+ **修正 `:49` 错锚**为措辞锚定「`bar_sizes` 数组第 2 元素」+ §10.4 补「裁决见 <裁决文件>」指针
