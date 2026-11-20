# 裁决记录 · **Q-12**（R-18 doorbell 契约面 vs 实现面）

> **⚠️ 域声明**：本裁决的**对象是 dGPU 域**（`openspec/specs/minimal-dgpu-soc-architecture/` + `docs/designs/dgpu-*/`），⛔ **不是 CEP 域**。
> 承载于 CEP 登记册（`.rddf/roadmap/objectives/obj-cep-platform-remaining.md`）**仅作指针登记**（per 该册 §6.7 #7「不把非 CEP spec 存废排进 CEP 队列（仅登记）」）⇒ 裁决**独立成文**，⛔ 不 append 进文件名含 `q1-q11` 的 CEP 裁决记录（那会把 dGPU 议题钉进 CEP 载体）。
>
> **审查轨迹**：**metis 1 轮**（歧义 / 边界 / slop 枚举）→ **oracle 1 轮**（对抗式，判 **REJECT** · 3 类 BLOCKER）→ 本记录逐条闭合 oracle 的 BLOCKER ⇒ **critic 复审（本记录为送审对象）**。
> **用户输入**：2026-11-20 表达倾向 **① 改代码对齐配置**（登记册 Q-12 行已记录）。

---

## §1 · 裁决结论（**选项②**，但**范围显著大于用户设想的二选一**）

> **裁决**：采 **② 改契约** —— 但②的落地**不是**「改 `:101` 一行标题」，而是**连带改写 R-18 的全部 4 处 + 3 处连带 spec**（见 §3 清单）。⛔ **只改 `:101` 一行 ⇒ 裁决不成立**。

**裁决的实质**（与用户倾向的关系）：

| 项 | 结论 |
|---|---|
| 「doorbell 地址的**取值来源**」 | ⛔ **不变**（仍是代码常量）⇒ ①(a) 不采纳 |
| 「doorbell 地址的**取值**」 | ⛔ **不变**（仍 `0x10010000`）⇒ 与既有 ADR-DGPU-09:120 / ADR-DGPU-03:583 **不冲突** |
| **真正被裁的** | **`openspec/specs/minimal-dgpu-soc-architecture/spec.md` 对同一行为写了 3 个互相矛盾、且被实现全部违反而测试全绿的断言** ⇒ 裁决对象 = **契约面的失真**，不是「配置 vs hardcoded」这个提法 |

⇒ **本裁决不是「否决用户的方向」，而是「发现问题不在取值来源」** —— 见 §5「与用户倾向的关系」。

---

## §2 · 决策背景（**事实** · 2026-11-20 实测 · 每条可由一条命令复现）

### §2.1 实现与配置的事实

```
配置（三份 shipped 配置**全部**同值）：
  configs/dgpu_soc_minimal_v1.json:27          "bar_sizes": [4096, 16777216, 8589934592]
  configs/dgpu_soc_timing_v1.json:20           "bar_sizes": [4096, 16777216, 8589934592]
  examples/dgpu_soc_with_pcie_ip.json:55       "bar_sizes": [4096, 16777216, 8589934592]
  ⇒ BAR1 实窗 = 16777216 = 16MB；配置侧**无任何 doorbell 字段**

代码（**3 处独立硬编码**，非 1 处）：
  include/tlm/gpu/dgpu_board_shell.hh:172      DGpuBoard::kBar1DoorbellOffset = 0x10010000ULL
  include/tlm/pcie/pcie_endpoint_ip.hh:260      PcieEndpointIP::kBar1DoorbellOffset = 0x10010000ULL
  src/tlm/gpu/sdma_engine_tlm.cc:679           裸字面量 0x10010000ULL（⇦ 第三处，非符号引用）
  ⇒ 0x10010000 = 268435456 = 256MB

行为：**doorbell 路径在 bound 检查之前优先匹配** ⇒ 16MB 配置下 doorbell 写**仍然生效**
  证据：src/tlm/gpu/dgpu_board_shell.cc:566  `if (bar == 1 && offset == kBar1DoorbellOffset)`
        test/test_cpptlm_emulator_minimal_soc_smoke.cc:110-121 用**生产 16MB 配置**写 (bar=1, 0x10010000) 断言 rc==0
```

### §2.2 契约面的**三个矛盾断言**（本裁决的对象）

| # | 断言 | 出处 | 实现事实 | 状态 |
|:--:|---|---|---|---|
| **A-1** | 「`kBar1DoorbellOffset = 0x10010000ULL` SHALL be hardcoded 常量, `bar_sizes[1]` **必须 ≥ 256MB+64KB**」 | `openspec/specs/minimal-dgpu-soc-architecture/spec.md:54` | 三份 shipped 配置全为 **16MB**（违反）；**无任何闸检查该 MUST** | ⚠️ **MUST 被全体违反而全绿**（最危险的假绿形态）|
| **A-2** | 「16MB ⇒ doorbell 访问越界，**不**触发 doorbell 路由」+「测试期望此为 config bug，不应破坏 SDMA ring consume 路径」 | 同文件 `:62-66` | doorbell 优先匹配 ⇒ **越窗仍生效**；smoke 测试用 16MB 配置断言 `rc==0` | ⚠️ **Scenario 与实现相反而全绿** |
| **A-3** | 「config 错误与代码 hardcoded 的不一致 SHALL be **已知推迟项**，需在 §13 风险表中显式记录」+ GIVEN 指向 §13 风险表行 + THEN 记录「v1.0 仅 ABI 路径,v1.1 补」 | 同文件 `:101-109` | §13 已指针化（`docs/designs/dgpu-soc/architecture.md:789` → §0.2 风险表）；「v1.0 仅 ABI 路径」全仓唯一（无落点）| ⚠️ **僵尸条款**：GIVEN 不可满足 ⇒ 「已记录」永不可达 |

⚠️ **A-1 / A-2 与 A-3 的关系**（oracle 的关键发现）：**A-1 与 A-2 独立于「取值来源」而存在** ⇒ 只处置 A-3（用户设想的「不一致」那条）**必然留下两个僵尸断言** ⇒ 裁决**必须连带处理 A-1 / A-2**。

### §2.3 既有已签批决策（**裁决须与之对齐**）

| 出处 | 原文 | 与本裁决的关系 |
|---|---|---|
| `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md:120` | 「BAR1 doorbell offset 0x10010000 > 16MB 实窗 \| 测试专用合成偏移；不依赖 BAR1 实窗大小；**改常量弄断既有 `[sdma][doorbell]` 测试** \| **不修**」 | ⇒ **本裁决 = 落实**（不修 + 改契约），**⛔ 非反转** ⇒ 走 **ADR SU 追加**，⛔ 不走取代流程 |
| `docs/adr/ADR-DGPU-09-...md` Status Update 2027-02-09 | 「范围不变性验证」表列 `bar_sizes: [4096, 16777216, 8589934592]` ✅ | ⇒ **已签发 ADR 的实施记录反向 ratify 了 16MB** ⇒ A-1 的 MUST 已被事实推翻 |
| `docs/designs/dgpu-soc/d3-evolution-roadmap.md:128,135` | 「❌ **维持现状，不修**」+「**决策**: 维持现状。后续若真 driver 集成时 doorbell 物理地址 ≠ 0x10010000 触发, 才回头修。」 | ⇒ **触发条件必须原样保留**进新契约（见 §3 判据 A-5）|
| `docs/adr/ADR-DGPU-03-...md:583` | 「BAR1 doorbell 行为」列为冻结项（保留 `kBar1DoorbellOffset = 0x10010000`）| ⇒ **取值与 ABI 面冻结** ⇒ ①(a)「只换来源」也需穿透 3 处硬编码 + 1 处 ABI 冻结面 ⇒ 收益/代价失衡 |

---

## §3 · 裁决内容（**逐条可机判**）

### §3.1 连带改动清单（**7 处** · ⛔ 缺任一处 ⇒ 裁决不成立）

| # | 落点 | 处置 | 判据（措辞锚定 · ⛔ **不用行号**）|
|:--:|---|---|---|
| **C-1** | `openspec/specs/minimal-dgpu-soc-architecture/spec.md` **`:52-60`**（Requirement「BAR 布局与代码真相对齐」+ Scenario「doorbell 配置命中」）| **改写**：A-1 的 MUST 降级为「doorbell 使用**测试专用合成偏移**，其**不依赖** `bar_sizes[1]` 大小」；删「`bar_sizes[1]` 必须 ≥ 256MB+64KB」 | `grep -c "必须 ≥ 256MB+64KB" <spec>` == **0** |
| **C-2** | 同文件 **`:62-66`**（Scenario「doorbell 配置过小(检测 config bug)」）| **改写为正向断言**（⛔ 不删除 —— 保留「config-bug 检测」意图）：「`bar_sizes[1] = 16MB` ⇒ doorbell 写**仍命中** doorbell 路由（合成偏移走优先匹配），**不**视为 config bug」 | `grep -c "不触发 doorbell 路由" <spec>` == **0** ∧ `grep -c "仍命中" <spec>` ≥ **1** |
| **C-3** | 同文件 **`:101-109`**（Requirement「doorbell 配置 vs 代码 hardcoded 不一致」+ Scenario「风险追踪」）| **整条改写**为「doorbell 合成偏移是**已裁定的现状**（per ADR-DGPU-09 §2.2.2 + d3-roadmap）」；GIVEN **重锚**到 ADR-DGPU-09 §2.2.2 与 d3-roadmap 的**措辞级锚点**（⛔ 不锚行号）；⛔ 删除「§13 风险表」与「v1.0 仅 ABI 路径,v1.1 补」两处悬空引用 | `grep -cF "§13 风险表" <spec>` == **0** ∧ `grep -cF "v1.0 仅 ABI 路径" <spec>` == **0** |
| **C-4** | `openspec/specs/dgpu-soc-timing/spec.md`（doorbell 相关 4 行）| **同步**：把「检测 kBar1DoorbellOffset」的表述与新契约一致（⛔ 不改行为语义）| `grep -c "kBar1DoorbellOffset" <spec>` 与改前一致 ∧ `grep -cE "不触发.*(doorbell|路由)" <spec>` == **0**（显式底线，per critic 建议 4）|
| **C-5** | `openspec/specs/lwc-bus-hub-mvp/spec.md`（doorbell 相关 2 行）| **同步**（同上）| 同 C-4（显式底线 `grep -cE "不触发.*(doorbell|路由)" <spec>` == **0**）|
| **C-6** | `openspec/specs/dgpu-soc-control-hub/spec.md` | **核对**：若含 doorbell 偏移语义 ⇒ 同步；⛔ 若不含 ⇒ **显式记录「不含，无需改」**（⛔ 不得静默跳过）| `grep -c "0x10010000\|kBar1DoorbellOffset" <spec>` 的取值**在裁决前后一致**（若不一致 ⇒ 裁决未连带到位）|
| **C-7** | `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` | **追加 3 行内 Status Update**（⛔ 正文零改动）：Q-12 裁决把 §2.2.2 的「不修」**转写进 spec 契约** + 指针 | `git diff` 对该 ADR 的改动**只增行**（既有行 diff 行数 == **0**）∧ 新增行含「Q-12」|

### §3.2 **决定性变异**（判据必须有鉴别力 · ⛔ 「rc≠0」不构成红因）

| # | 变异 | 期望 |
|:--:|---|---|
| **M-1** | 临时删除 `dgpu_board_shell.cc:566` 的 doorbell 优先匹配分支 ⇒ C-2 的 Scenario 必须**变红** | 变红 ⇒ 证明 C-2 **与实现挂钩**（当前绿 = 假绿证据）|
| **M-2** | 三份 shipped 配置**维持 16MB** ⇒ C-1 的新条款必须**仍被满足**（不依赖 `bar_sizes[1]` 大小）| 仍绿 ⇒ 证明 C-1 不是换一种形式的空 MUST |
| **M-3** | 把 C-3 的新措辞**删掉** ⇒ 须有判据变红 | 变红 ⇒ 证明 C-3 不是「改了等于没改」|

### §3.3 **复活锁**（新契约必须原样包含）

C-3 的新文本**必须**原样包含 d3-roadmap:135 的触发条件（⛔ 措辞可润色，**语义不得弱化**）：

> 「后续若**真 driver 集成**时 doorbell **物理地址 ≠ 0x10010000** ⇒ 才回头修。」

⚠️ 该条件当前**不可机判**（无「真 driver 集成」判定载体、无「物理地址」来源）⇒ 新契约须同时给出**可机判的替代表述**（下列任一即可，择一写死）：

| 候选 | 表述 | 机判 |
|:--:|---|---|
| **R-1** | 出现 `(bar, offset) ≠ (1, 0x10010000)` 的 driver 侧 doorbell 调用 | `grep -rn "mmio_write(0, \|mmio_write(2, " test/ src/ \| grep -i doorbell` 命中数 ≥ 1 |
| **R-2** | 任一含 SoC 的配置出现 `pcie_ep.params.bar_sizes[1] ≥ 268435464`（= 256MB+64KB） | `grep '"bar_sizes"' configs/*.json examples/*.json \| awk -F'[:,]' '{b=$3+0; if (b>=268435464) print}' \| wc -l` ≥ **1**（⚠️ **必须用数值比较，⛔ 不得用正则** —— per critic 建议 1：正则 `[3-9][0-9]{8}` **漏匹配 268,501,000–299,999,999**（9 位首位 2）⇒ 该复活触发带可被规避而 AC-7 检不出；`[2-9][0-9]{8}` 虽补上首尾，但仍是**字符串近似**；awk 的 `$3` 恰为 `bar_sizes[1]`（字段序：`$1="bar_sizes"` `$2=[4096` `$3=BAR1`）⇒ 边界值 268435464 命中 / 268435463 不命中 / 真实仓 16MB 不命中，三者均已实测）|
| **R-3** | 冻结面 `docs/designs/dgpu-driver/architecture.md` 的 DOORBELL 行被正式修订 | 该文件的 `git diff` 非空（且走 ADR SU 流程）|

### §3.4 **明确不做**（⛔ 范围纪律）

| # | 不做 | 理由 |
|:--:|---|---|
| **N-1** | ⛔ **不改 `docs/designs/dgpu-driver/architecture.md`** 的 DOORBELL / RING_DOORBELL 行 | ABI 冻结面（per ADR-DGPU-08 / ADR-DGPU-03:583）；本次 scope 禁改 |
| **N-2** | ⛔ **不改 `configs/*.json` 的 `bar_sizes`** | ① `framebuffer_size_` 从 `bar_sizes[1]` 派生（per AGENTS.md KEY INVARIANTS）⇒ 256MB BAR1 = 256MB FB 分配，破坏 16MB 现状与既有测试；② ADR-DGPU-09 SU 2027-02-09 已把 16MB 写进不变性验证表 |
| **N-3** | ⛔ **不新增 JSON 字段**（⛔ 禁「配置字段预留」措辞）| ① 新增键 = 公开 config schema 变更（`scripts/test/param_effect_inventory.py:33-34` 有硬编码常量账 `EXPECTED_TYPE_COUNT=12` / `EXPECTED_KEY_TYPE_PAIRS=22`）；② `PcieEndpointIP` 的 `warn_unconsumed_subkeys` **只覆盖子 group**（`pcie_endpoint_ip.cc:314/333/355`）⇒ **顶层 `params` 未知键被静默忽略** ⇒ 无消费者的预留字段 = **静默死键** |
| **N-4** | ⛔ **不改代码的取值来源**（3 处硬编码保持）| ① 须穿透 `DGpuBoard::kBar1DoorbellOffset` + `PcieEndpointIP::kBar1DoorbellOffset` + `sdma_engine_tlm.cc:679` 裸字面量 **3 处**（⛔ 漏一处 = 行为分裂）；② 顶层键静默忽略（见 N-3）⇒ 配置写了不生效**比 hardcoded 更糟**（hardcoded 至少诚实）|
| **N-5** | ⛔ **不复用「改常量弄断既有测试」作为延期理由** | 该理由**被夸大**：实测值断言仅 **1 处**（`test/test_pcie_endpoint_bar_routing.cc:140`）+ **4 处字面量**，其余 9 处用 `DGpuBoard::kBar1DoorbellOffset` **符号**引用（改值不破编译）⇒ 复用它 = 把 slop 传下去 |
| **N-6** | ⛔ **不在裁决文本中以 `bar1_window_size_` 为前提** | 该字段在 `include/` + `src/` **0 命中**，但 ≥6 处文档当既有字段引用 ⇒ **文档幻觉**，⛔ 不得作为 seam 依据 |

---

## §4 · 后果 / 代价

| 面 | 后果 |
|---|---|
| **正面** | ① 消除 A-1 / A-2 / A-3 三处矛盾断言 ⇒ **「MUST 被全体违反而全绿」与「Scenario 与实现相反而全绿」两种假绿同时消解**；② Q-12（C 类）**可关闭**且不留自我复活的僵尸条款；③ 与既有三份已签批决策**同向**（落实而非反转）⇒ 无需取代流程 |
| **负面 / 代价** | ① **spec 侧改动面 = 7 处**（3 主 + 3 连带 + 1 ADR SU）⇒ builder 工作量 ≈ **0.5~1 d**（须走 OpenSpec change，占 1 KPI slot）；② 契约面**主动放宽**了 A-1 的 MUST（从「bar_sizes[1] ≥ 256MB」改为「不依赖其大小」）—— ⚠️ 这是**放弃一条约束换一条真约束**，须确认无下游依赖该 MUST（实测：无闸检查 ⇒ 无自动化依赖）；③ `docs/designs/dgpu-soc/d3-evolution-roadmap.md` 的「维持现状」措辞与新契约并存 ⇒ 需追加澄清（⛔ 不改其正文） |
| **风险** | ① **只改 `:101` 一行** ⇒ A-1 / A-2 留存 ⇒ 裁决自我复活（**这是本裁决最可能的失败模式**）② 改写 C-2 时**误把实现改成符合旧 spec**（即真去实现「16MB ⇒ 不触发路由」）⇒ **破坏 smoke 测试**（`test_cpptlm_emulator_minimal_soc_smoke.cc:110-121` 断言 `rc==0`）⇒ ⛔ **契约向实现看齐，不是实现向契约看齐** ③ 复活条件写成不可机判措辞 ⇒ 下轮又「重新讨论」 |

---

## §5 · 与用户倾向的关系（**诚实说明**）

用户 2026-11-20 倾向 **①（doorbell 走配置）**。**本裁决未采纳①**，理由**不是**「① 更差」这条泛泛判断，而是三条可复验的事实：

1. **① 的落地形态有 3 个字母级变体，各自动作面与回归面完全不同**（metis 歧义 1）：(a) 值不变只换来源 ⇒ **行为零变化** ⇒ 「不一致」在**空间维度**原封不动（metis 边界 2）⇒ 表面闭合而事实未变；(b) 由 `bar_sizes[1]` 派生 ⇒ 落进 framebuffer 全窗（`dgpu_board_shell.cc:526-531` 先 memcpy 进像素区）⇒ **数据损坏路径**；(c) 移 BAR0 ⇒ BAR0 仅 4KB 且被 GMMU/SDMA_STATUS 占用 ⇒ 物理装不下。
2. **①(a) 需穿透 3 处独立硬编码**（`dgpu_board_shell.hh:172` / `pcie_endpoint_ip.hh:260` / `sdma_engine_tlm.cc:679` 裸字面量）+ 1 处 ABI 冻结面 ⇒ 收益（消除一个提法）与代价（3 处 + ABI 面 + config schema 常量账）**严重失衡**。
3. **① 并不触及真问题**：A-1（MUST 被全体违反）与 A-2（Scenario 与实现相反）**独立于取值来源** ⇒ 即便①全做完，这两个假绿仍在。

⇒ **⇒ 本裁决的真实结论是：「问题不在取值来源，在契约面失真」**。若未来仍要①（例如真 driver 集成要求物理地址可变），**必须先走新 ADR 取代 ADR-DGPU-09:120 / ADR-DGPU-03:583**，⛔ 不能在本裁决框架内翻案。

---

## §6 · Verification（可勾选验收判据 · builder 域）

> ⚠️ **落地路径**：builder 走 **OpenSpec change**（`openspec/changes/<name>/`）改 spec 契约 ⇒ **⛔ 不得直改 `openspec/specs/**` 文本**（per 仓内契约面变更流程）⇒ 归档时 delta 合并进 main spec。
> ⚠️ **锚定纪律**：全部判据**锚定唯一措辞**、⛔ **不用行号**（本议题已有 3 处行号锚点腐化：登记册 `:502` 的 `configs/...:49` 实为 `sdma.params.max_inflight`，`bar_sizes` 在 `:27`；`lwc-bus-hub-mvp:148` 的 `cc:537` 实为 `:566-571`；`docs/pcie:309` 的 `hh:171` 实为 `:172`）。

- [ ] **AC-1（C-1 ~ C-3 · 三处主断言消解）**：归档合并后，`grep -c "必须 ≥ 256MB+64KB" openspec/specs/minimal-dgpu-soc-architecture/spec.md` == **0** ∧ `grep -c "不触发 doorbell 路由" <同上>` == **0** ∧ `grep -cF "§13 风险表" <同上>` == **0** ∧ `grep -cF "v1.0 仅 ABI 路径" <同上>` == **0** ∧ `grep -c "仍命中" <同上>` ≥ **1**
- [ ] **AC-2（C-4 ~ C-6 · 连带面同步）**：① `grep -c "0x10010000\|kBar1DoorbellOffset" openspec/specs/dgpu-soc-timing/spec.md openspec/specs/lwc-bus-hub-mvp/spec.md openspec/specs/dgpu-soc-control-hub/spec.md` 的命中集合**与裁决前一致**（值不变）；② **显式底线**（per critic 建议 4 · 把「措辞同向」的人读成分收紧为机判）：`grep -cE "不触发.*(doorbell|路由)" openspec/specs/dgpu-soc-timing/spec.md openspec/specs/lwc-bus-hub-mvp/spec.md openspec/specs/dgpu-soc-control-hub/spec.md` **逐份 == 0**（⛔ 消灭残留判据的自由裁量）
- [ ] **AC-3（C-7 · ADR SU 追加）**：`git diff` 对 `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` 的改动**只增行**（既有行 diff 行数 == **0**）+ 新增行含「Q-12」+ 指向本裁决文件
- [ ] **AC-4（M-1 · 决定性变异 · C-2 与实现挂钩）**：影子副本中临时删除 doorbell 优先匹配分支 ⇒ 相关测试**变红** ⇒ 还原 ⇒ 绿。⚠️ **变异前必须先证明「当前绿」**（否则变异无意义）<br>⚠️ **红因判据（per critic 建议 2 · 收紧）**：红因**必须**是 **doorbell 路由断言失败**（鉴别测试锚定措辞「BAR1 doorbell 写」+ `REQUIRE(rc == 0)` 失败，per `test/test_cpptlm_emulator_minimal_soc_smoke.cc` 的 smoke T1.4）；⛔ **编译失败 / 构建错误 / 找不到测试二进制 均不构成红因**（⛔ 否则「删分支 ⇒ 编不过」会被误当成「判据有鉴别力」）
- [ ] **AC-5（M-2 · 三份配置维持 16MB ⇒ 新条款仍满足）**：改动后三份 shipped 配置**字节不变** ∧ 新契约条款被满足
- [ ] **AC-6（M-3 · C-3 非空转）**：影子副本中删掉 C-3 的新措辞 ⇒ 须有判据变红 ⇒ 还原 ⇒ 绿。⚠️ **本条须与 AC-7 复合执行**（per critic 建议 3）：**红因 = AC-7 的 `grep -cF "<复活锁表述>"` 从 1 变 0**；⛔ **AC-6 单独不可判**（删掉 C-3 新措辞不影响 AC-1 的任何 grep —— AC-1 grep 的是**已删的旧措辞**）
- [ ] **AC-7（复活锁 · 可机判触发条件）**：C-3 新文本含 **R-1 / R-2 / R-3 中择一**的机判表述（§3.3）⇒ `grep -cF "<该表述>" <spec>` == **1**
- [ ] **AC-8（范围纪律 · 零越权）**：`git diff --name-only` **不含** `include/**` · `src/**` · `configs/**` · `examples/**` · `docs/designs/dgpu-driver/architecture.md` · `scripts/test/param_effect_inventory.py`
- [ ] **AC-9（0 回归）**：`ctest` 全绿（⛔ 不写死数字）· 全部 `scripts/test/cep_*.sh`（数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0（⚠️ `format --check` 现有 **21 个既有 C++ 文件**待格式化 ⇒ 本单元零 C++ 改动 ⇒ 须**排除既有债**后判，或**如实记录**该项为既有红）
- [ ] **AC-10（登记册回填）**：登记册 §6.4 队列 1 的 **Q-12** 行改为「✅ 已裁（architect 2026-11-20 · 裁决文件 = `.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`）」+ **修正 `:49` 错锚**为措辞锚定「`bar_sizes` 数组第 2 元素」；§10.4 补「裁决见 <本文件>」指针

---

## §7 · 关联

| 项 | 落点 |
|---|---|
| **登记册** | `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6.4 队列 1 Q-12 行 · §6.2 S3-8（C 类 1 项）· §10.4（衍生问题登记）|
| **既有决策** | `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md:120`（§2.2.2）· `docs/designs/dgpu-soc/d3-evolution-roadmap.md:128,135` · `docs/adr/ADR-DGPU-03-...md:583` |
| **受影响的 spec** | `openspec/specs/minimal-dgpu-soc-architecture/spec.md` · `dgpu-soc-timing/spec.md` · `lwc-bus-hub-mvp/spec.md` · `dgpu-soc-control-hub/spec.md` |
| **审稿报告** | `.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`（**本文件**）|
| **关联 change** | ⏳ 待 builder 立项（`openspec/changes/<name>/`）· 占 **1** KPI slot（当前 0/3）|

---

## §8 · oracle BLOCKER 闭合记录

| BLOCKER | 处置 |
|:--:|---|
| **① 与已签批 ADR 冲突且未走取代流程**（用户倾向① vs ADR-DGPU-09:120 / ADR-DGPU-03:583）| ✅ **闭合**：§2.3 逐条对齐 + §5 给出「① 为何不可执行」的三条可复验事实 + §1 明确「本裁决 = **落实**既有决策 ⇒ 走 **ADR SU 追加**（C-7），⛔ 非取代流程」；登记册 Q-12 行将记录「architect 裁② = 落实既有裁决，⛔ 非用户倾向被否」（§6 AC-10）|
| **② 决策框架缺达成意图的必要组成**（二选一均未覆盖真问题）| ✅ **闭合**：§1 明确「裁决对象 = 契约面失真（3 处矛盾断言）」+ §3.1 的 **7 处连带清单**（含「⛔ 只改 `:101` 一行 ⇒ 裁决不成立」）+ §3.2 的 **3 条决定性变异**（M-1/M-2/M-3）+ §3.3 的**复活锁** |
| **③ 文档内对同一决策自相矛盾**（§5 的倾向 vs 裁决）| ✅ **闭合**：§5 逐条给出「① 三个字母形态各自怎么坏」+ 明确「本裁决不是否决方向，是发现真问题不在取值来源」+ §6 AC-10 的登记册回填义务 |

**metis SHOULD_FIX 承接**：AC-10 已含「修正 `:49` 错锚」；「`bar1_window_size_` 文档幻觉」按 oracle 意见**降级为附带登记**（⛔ 不进本次判据，建议挂 dGPU 域 doc-hygiene 队列）。

### §8.1 critic 复审（2026-11-20 · **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · 无 BLOCKER）

> **结论**：critic 抽查裁决引用的 **12 处承重事实锚点**（ADR-DGPU-09:120 原文 · SU:295 · d3-roadmap:128,135 · ADR-DGPU-03:583 · spec:54/:62-66/:101-109 · smoke test:110-121 · 3 处代码硬编码 · architecture.md:789 · dgpu-soc-timing:407-410 · lwc-bus-hub:148,152 · control-hub:41）**全部与引用一致** ⇒ §2.3「落实而非反转」的断言**站得住**；§8 的 oracle 闭合**非修辞性**（裁决对象本身从「用户二选一」改为「契约面失真 + 7 处连带 + 3 变异 + 复活锁」）。

**4 条 SHOULD_FIX 已全部就地修补**（均属措辞 / 判据级，⛔ 未触动裁决结论与 §8 闭合逻辑）：

| # | critic 建议 | 处置 |
|:--:|---|---|
| **S-1** | **R-2 正则漏洞**：`[3-9][0-9]{8}` 漏匹配 268,501,000–299,999,999（9 位首位 2）⇒ 该复活触发带可被规避而 AC-7 检不出 | ✅ **改用 awk 数值比较**（`awk -F'[:,]' '{b=$3+0; if (b>=268435464) print}'`）—— 实测：边界 268435464 **命中** / 268435463 **不命中** / 16MB **不命中** / 真实仓三份配置 **无 HIT**；⛔ **禁用正则**（正则为字符串近似）|
| **S-2** | **AC-4 红因缺口**：「相关测试变红」与 §3.2「rc≠0 不构成红因」不自洽 | ✅ 补红因判据：红因**必须**是 **doorbell 路由断言失败**（`REQUIRE(rc == 0)` 失败）；⛔ **编译失败 / 构建错误 / 找不到测试二进制 均不构成红因** |
| **S-3** | **AC-6 指向不明**：删 C-3 新措辞不影响 AC-1 的任何 grep ⇒ AC-6 单独不可判 | ✅ 显式写「**本条须与 AC-7 复合执行**；红因 = AC-7 的 `grep -cF` 从 1 变 0」 |
| **S-4** | **AC-2 / C-4 / C-5 的「措辞同向」是人读** | ✅ 收紧为显式机判底线：`grep -cE "不触发.*(doorbell\|路由)" <3 份连带 spec>` **逐份 == 0** |

**critic 已排除的方案（记录以免下轮重提）**：① 把 AC-4 红因缺口升格为 BLOCKER 拒掉整个裁决（否决 —— 缺的是一句判据收紧，非「决策缺组成」）；② 要求裁决替 builder 选定 R-1/R-2/R-3 之一（否决 —— 属实施细节；边界菜单 + AC-7 机判收口已足）；③ 要求 d3-roadmap 的 SU 走 oracle 二次评审（否决 —— 该文件是 🟡 DRAFT 非签发 ADR，且本次 metis→oracle→architect 流程本身即真源 2 的修改门槛）。
