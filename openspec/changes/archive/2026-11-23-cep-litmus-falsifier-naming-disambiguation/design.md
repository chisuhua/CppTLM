# design: cep-litmus-falsifier-naming-disambiguation —— 阶段 1 设计（命名消歧）

> **来源**：`proposal.md`（同目录）· **形态**：`skip_specs: true`（零 delta spec）
> **范围纪律（硬约束）**：⛔ **不改**任何证伪逻辑（找环算法 / 语料格式 / 三态判定语义）· ⛔ **不扩**语料（⊆ 既有 backlog `cep-memory-model-operational-trace`）· ⛔ **不定义** `rfe` / `fr` / `ws` 语义 · ⛔ **新增 invariant 数 = 0** · ⛔ **零冻结面改动**（AC-5 硬查）。

---

## D-1 ~ D-7：设计决策

### D-1 消歧常量的形态：**否定式命名 + 声明式 scope 串**

```cpp
inline constexpr const char* kFalsifierScope = "channel-dependency-graph";
inline constexpr bool kFalsifierIsMemoryModelLitmus = false;
```

**取舍**：
- `kFalsifierIsMemoryModelLitmus = false` 用**否定式命名** ⇒ 读者**光看名字**就知道「这不是内存模型 litmus」，无需读注释（缓解「常量被误读为能力增强」的风险）。
- `kFalsifierScope` 用**声明式字符串**而非枚举 ⇒ 下游文档可**直接引用该串**（避免两处各写一份、久而久之漂移）。

⛔ **不带 `instance_id`**：本单元与具体协议实例**无关**（判据面向工装整体），带实例名会把命名权烧进类型系统。

### D-2 常量放**头文件顶部**（紧邻「诚实边界」段）

**取舍**：常量与解释它的散文**物理相邻** ⇒ 后续维护者改一处不会漏另一处。
⛔ 不放文件末尾：末尾是「读代码的人最后才看到」的位置，消歧信息应**先于**实现细节被读到。

### D-3 诚实边界段的**三要素**（AC-3 的机检依据）

段内必须同时出现（**关键词共现**即可机检）：

| 要素 | 关键词 | 语义 |
|---|---|---|
| 顶点形态 | `(instance, network, vc)` | 图的顶点是什么 |
| 判据对象 | `环` | 判据 = 环存在性 |
| 与内存模型 litmus 的区别 | 否定标记 | 本工装**不是** herd7 memory-model litmus |

⚠️ 「否定标记」用**词表**判定（`不是` / `非` / `⛔` / `不`），⛔ **不含「区别」** —— 「X 与内存模型 litmus 的区别是 Y」是**肯定句**（断言两者有某种对应关系），不构成「本工装不是它」的否定。

### D-4 禁用表述闸的**扫描集**（3 文件）

| 文件 | 角色 | 为什么在集内 |
|---|---|---|
| `include/tlm/cep/litmus_falsifier.hh` | 被消歧的**主体** | 最可能复发误表述的地方 |
| `test/test_cep_litmus_falsification.cc` | 断言载体 | 注释里最容易写「litmus test」 |
| `openspec/specs/cep-litmus-falsification/spec.md` | **契约面**（冻结） | ⛔ **只读** ⇒ 闸不制造冻结面改动；但它是最权威的表述面 ⇒ 必须受检 |

⛔ **不扩到全仓**：全仓含 `docs/research/cep-coherence-protocol-space-survey.md`（论文侧**正确**讨论 herd7 litmus ⇒ 必然命中 ⇒ 会假红）。扫描集**精确限定**在「本仓工装自身表述」的三处。

### D-5 否定语境 = **同一行**内含否定标记

**取舍**：以 `\n` 为界，⛔ **不按空行分段**（否则维护期「一行换行」即抖动 ⇒ 闸行为不稳定）。
**代价**：跨行的否定句（「本工装不是\n内存模型 litmus」）会假红 —— ⛔ **接受**（宁可假红，不可假绿：假绿意味着能力高估复发，正是本单元要治的病）。

### D-6 闸的**红因码**：`litmus-scope-claim`

单签名 ⇒ 失败可归因（⛔ 「rc≠0」不构成红因 —— 判据必须**点名**违规形态）。

### D-7 AC-2 的**三路反 fixture**（闸自身的有牙证明）

| 路 | 注入内容 | 期望 | 覆盖的判据缺陷 |
|:--:|---|---|---|
| ① | 「本工装是 memory-model litmus」 | **红**（`litmus-scope-claim`） | 肯定句必须被抓住 |
| ② | 「本工装**不是** memory-model litmus」 | **绿** | 否定句**不得**误报（否则闸不可用） |
| ③ | 「X 与内存模型 litmus 的**区别是** Y」 | **红** | 「区别」**不是**否定词（metis P1-11） |

⇒ 闸脚本内含**自测**：对**影子副本**注入三路内容，断言 ①③ 红 ② 绿 ⇒ 闸**自证有牙**（⛔ 不得只跑正控）。

---

## 实施清单（TDD 5 步）

| 步 | 动作 | 落点 |
|:--:|---|---|
| 1 | **红**：先写闸 + 断言（此时常量不存在 ⇒ 编译失败 / 闸红） | `scripts/test/cep_litmus_naming_check.sh` · `test/test_cep_litmus_falsification.cc` |
| 2 | **绿**：加常量 + 边界段 ⇒ 断言与闸转绿 | `include/tlm/cep/litmus_falsifier.hh` |
| 3 | **反 fixture 自测**：闸内三路（①③红 / ②绿）+ probe TU（常量改 `true` ⇒ 编译失败且命中签名） | 闸脚本内 |
| 4 | **回归**：既有 `cep_litmus_falsification_check.sh` rc=0 · `ctest -R cep` 全绿 · 全部 `cep_*.sh` rc=0 | — |
| 5 | **收口**：ctest 注册 + `scripts/README.md` 守门表登记 + 计数同步 | `test/CMakeLists.txt` · `scripts/README.md` · 登记册 §0.1 |

---

## 遗留（阶段 2 · 另立）

| 项 | 触发条件 | 成本 |
|---|---|---|
| 实际改名（`litmus_falsifier.hh` ⇒ `compound_graph_falsifier.hh` 等 3 个符号） | ① 有人主张改名，或 ② 操作级模型落地需「litmus」名让位 | 冻结面五步链 ×1（spec REMOVED requirement）+ 全仓引用改名 |

⛔ **阶段 1 不做**（proposal 的「What Changes #5」已登记）。

## 关联

`.rddf/improvements/cep-litmus-falsifier-naming-disambiguation.md`（I2 · metis 1 轮）· `.rddf/roadmap/cep-gen-family-gap-closure.md`（实施路径 · G6）· `docs/adr/ADR-CEP-13-coherence-domain-ownership.md`（决策 5 ②「平台不提供无违例保证」）· 台账 `obj-cep-platform.md`
