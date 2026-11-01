# cep-tilelink-state-spec Specification

## Purpose
定义 **TileLink-C**(TileLink Tree Manager · SiFive `tilelink-c-1.7.1`)在 CEP 平台上的
**规约层可执行契约**:稳定态集合 / 事件类别词表 / 转移矩阵 / **`perm` 3→3 投影** /
逐格 `basis` 与**强度标注** / **非对称的结构级断言**(TileLink 形态)。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #134** / **#135** / **#138** +
`ADR-CEP-11` / `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17` 的 `## Status Update(2026-11-01)`)
⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改 `perm` 4 值锁 · 不扩 `CATEGORY_*` 3 类 · 不改 `CohIfBundle` · 不改冻结 schema ·
> 不新增 ABI · 不新增 invariant 编号数 · **不产出任何性能结论** ·
> **不实现** adapter / `TreeManager` tracker 行为类 / 实例声明
> (属第 1 个 change `cpptlm-cep-tilelink-tree-manager`)。
>
> **合规边界**:TileLink 是 **SiFive 明示的 free and open standard** ⇒ **规范正文可读**
> (与队列 #10 的 CXL 情形**相反**:CXL 全程禁读规范正文)⇒ basis **直接来自规范**;
> ⚠️ 规范 © SiFive(**非** CC-BY)⇒ 只记**条款级定位 + 断言摘要**,**不整段复制**原文段落。
>
> **口径边界**:本能力 SHALL 锁定**结构**(态 / 事件类别 / 转移 / 投影 / 断言 / `basis` 自洽),
> SHALL **不**锁定 opcode **编码** / 字段宽度 / credit 规则 / transient 态命名 / 错误语义。
>
> **⚠️ 载体形态与队列 #10 不同型**:本能力的转移矩阵引入 **4 种格语义**
> (`Known-single` / `Known-multi` / `Known-empty` / `Unknown`)——
> 因 `Cap` 的**源不定**语义 + `Grow`(源=Tip)与 `Prune`(源=Nothing)的**规范明确空集**
> ⇒ **不可**建成队列 #10 那样的全单值矩阵(详见 `design.md` D-β / D-γ)。

---

## ADDED Requirements

### Requirement: cep-tilelink-state-spec

CEP SHALL 为 **TileLink-C**(`tilelink-c-1.7.1`)提供一份**可执行规约载体**:
3 稳定态集合 · 4 类事件词表 · 12 格转移矩阵(4 种格语义)· **3→3** `perm` 投影 ·
逐格 `basis` 与强度三档 · 偏离清单 · 编译期锁 · **非对称结构级断言**(TileLink 形态)。

#### Scenario: TSS-01 —— 稳定态集合 SHALL **恰为 3 个**且**逐字**为 `Nothing` / `Branch` / `Tip`

- **WHEN** 核对规约载体的稳定态集合
- **THEN** 子条件 ① SHALL 成立:稳定态集合 SHALL **恰为 3 个**
- **AND** 子条件 ② SHALL 成立:逐字 SHALL 为 **`Nothing`** / **`Branch`** / **`Tip`**
- **AND** 子条件 ③ SHALL 成立:**`Trunk` SHALL NOT 出现**在稳定态集合中
  (依据 = D1 §8.1.4 `Table 8.3` 的转移记号全集 = **N/B/T**,无任何 `Trunk` 参与的转移
  ⇒ `Trunk` 是**过渡态**)
- **AND** 子条件 ④ SHALL 成立:稳定态集合 SHALL **逐字断言**每个名字
  (⇒ **不得**只断言「恰为 3 个」—— 否则「加回 `Trunk` 仍 3 个」的漂移**不可分辨**)
- **AND** 子条件 ⑤ SHALL 成立:**反向 fixture**:把 `Tip` 换成 `Trunk`(仍 3 个)⇒ **必须因正确原因红**

#### Scenario: TSS-02 —— 事件词表 SHALL **恰为 4 类**且**逐字**为 `Cap` / `Grow` / `Prune` / `Report`

- **WHEN** 核对规约载体的事件类别词表
- **THEN** 子条件 ① SHALL 成立:事件类别 SHALL **恰为 4 类**
- **AND** 子条件 ② SHALL 成立:逐字 SHALL 为 **`Cap`** / **`Grow`** / **`Prune`** / **`Report`**
  (依据 = D1 §8.1.4 `Table 8.3` 的 4 个 Category 行)
- **AND** 子条件 ③ SHALL 成立:每类 SHALL 有 D1 §8.1.4 `Table 8.3` 的**枚举转移**支撑
  (`Cap` = `toT`/`toB`/`toN` · `Grow` = `NtoB`/`NtoT`/`BtoT` ·
  `Prune` = `TtoB`/`TtoN`/`BtoN` · `Report` = `TtoT`/`BtoB`/`NtoN`)
- **AND** 子条件 ④ SHALL 成立:**反向 fixture**:把 `Report` 换成 `Query` ⇒ **必须因正确原因红**

#### Scenario: TSS-03 —— 转移矩阵 SHALL 为 **12 格**且 `Unknown` 格 SHALL **为 0**

- **WHEN** 核对规约载体的转移矩阵
- **THEN** 子条件 ① SHALL 成立:矩阵 SHALL 为 **3 态 × 4 事件 = 12 格**
- **AND** 子条件 ② SHALL 成立:**`Unknown` 格数 SHALL 为 0**
  (依据 = `Grow` + `Prune` + `Report` 的 9 条转移**恰好覆盖** {N,B,T} 上的**全部 9 个有序对**,
  `Cap` 另给 3 条**源不定**通配转移)
- **AND** 子条件 ③ SHALL 成立:**`Unknown` 与「规范明确空集」SHALL 分别断言**
  (`Grow` 源=`Tip` 与 `Prune` 源=`Nothing` 为**规范明确**的空集 ⇒ 属 `Known-empty`,
  **不得**与 `Unknown` 混用 —— 否则「可判别性」丢失)
- **AND** 子条件 ④ SHALL 成立:编译期锁(`kTransitionCellCount == 12` · `kUnknownCellCount == 0`)
  SHALL **同时**有**运行时**断言(⇒ 编译期锁**不得**只以「编译红」充当 ctest 判据)
- **AND** 子条件 ⑤ SHALL 成立:**反向 fixture ①**:把某格改成 `Unknown` ⇒ **必须因正确原因红**
- **AND** 子条件 ⑥ SHALL 成立:**反向 fixture ②**:把空集格数改成 3 ⇒ **必须因正确原因红**

#### Scenario: TSS-04 —— `perm` 投影 SHALL **恰为 3 行**且 **`UNIQUE` SHALL NOT 出现**

- **WHEN** 核对规约载体的 `perm` 投影表
- **THEN** 子条件 ① SHALL 成立:投影表 SHALL **恰为 3 行**
- **AND** 子条件 ② SHALL 成立:逐行 SHALL 为
  `Nothing→NONE` / `Branch→READ` / `Tip→WRITEBACK`
  (依据 = D1 §8.1:副本权限全集 = `None` / `Read` / `Read+Write`;
  `Tip` = 「the **point of memory access serialization**」+ Read/Write + **may contain dirty data**
  ⇒ 独占但**可能脏** ⇒ 保守投影 `WRITEBACK`)
- **AND** 子条件 ③ SHALL 成立:**`UNIQUE` SHALL NOT 出现**在投影结果中
  (规范**只说**副本「**may** contain dirty data」⇒ **不承诺**干净 ⇒
  `UNIQUE`(干净独占)**不可确证** ⇒ 保守降级方向唯一)
- **AND** 子条件 ④ SHALL 成立:**反向 fixture**:加回 `UNIQUE` 行 ⇒ **必须因正确原因红**

#### Scenario: TSS-05 —— `perm` 值域 SHALL 维持 **4 值锁**(本协议族**用不到** `UNIQUE`)

- **WHEN** 核对 `perm` 的值域约束
- **THEN** 子条件 ① SHALL 成立:`perm` 的**值域** SHALL 维持 **4 值**
  (`NONE` / `READ` / `UNIQUE` / `WRITEBACK`,per `ADR-CEP-02` `CEN-Ib11`)
- **AND** 子条件 ② SHALL 成立:本能力 SHALL **不**新增第 5 个 `perm` 值
- **AND** 子条件 ③ SHALL 成立:本能力 SHALL 登记「`UNIQUE` 在 TileLink-C 投影中**不可达**」
  这一事实 ⇒ 并指向 **R-T4**(「实例 `perm` 数组填 3 值还是 4 值」)**不在本能力内裁决**
- **AND** 子条件 ④ SHALL 成立:本能力 SHALL **不**裁决**静默升级**的声明与否
  (⇒ 属**实例声明**面,第 1 个 change)

#### Scenario: TSS-06 —— 非对称断言 SHALL 用 **TileLink 形态**

- **WHEN** 核对规约载体的非对称结构级断言
- **THEN** 子条件 ① SHALL 成立:断言 SHALL 表征「**权限的授予与强制撤销只由 slave 发起**」
- **AND** 子条件 ② SHALL 成立:`Grant` / `GrantData` SHALL **只**出现在 **D 通道**
- **AND** 子条件 ③ SHALL 成立:`Probe` SHALL **只**出现在 **B 通道**
- **AND** 子条件 ④ SHALL 成立:断言 SHALL **不**表征「request 类消息只在一个方向」
  (TileLink 的 A / B **都是** request 类 ⇒ 该形态在 TileLink 上**恒假**)
- **AND** 子条件 ⑤ SHALL 成立:**反向 fixture**:把断言换成**队列 #10 的 CXL 形态**
  ⇒ **必须因正确原因红**
  (⚠️ 依据 = `ADR-CEP-17` 的 `## Status Update(2026-11-01,per 台账 Entry #138)`:
  CXL = request 方向 / TileLink = 权限变更发起方 ⇒ **两者不可互相套用**)

#### Scenario: TSS-07 —— 逐格 `basis` SHALL **全部** `direct` + `pending=false` 且指向**真实条目号**

- **WHEN** 核对转移矩阵每格的 `basis`
- **THEN** 子条件 ① SHALL 成立:每格 SHALL 有 `basis{kind, pending, ref, note}`
- **AND** 子条件 ② SHALL 成立:每格 `kind` SHALL 为 **`direct`**(D1 为 `pin=strong` ⇒ 规范正文可直接引用)
- **AND** 子条件 ③ SHALL 成立:每格 `pending` SHALL 为 **`false`**
- **AND** 子条件 ④ SHALL 成立:每格 `ref` SHALL 解析到锚点摘要的**真实条目号**
  (⇒ 载体与摘要**交叉核对**;手改任一侧 ⇒ **必须因正确原因红**)
- **AND** 子条件 ⑤ SHALL 成立:载体 SHALL **不**含任何规范正文的**整段复制**
  (只记**条款级定位 + 断言摘要**,per SiFive 许可边界)

#### Scenario: TSS-08 —— 偏离清单 SHALL 登记规范内部矛盾与保守降级

- **WHEN** 核对规约载体的偏离清单
- **THEN** 子条件 ① SHALL 成立:清单 SHALL 登记 D1 §8.1.4 正文「**six** subsets」与表列 **4** 类的矛盾
- **AND** 子条件 ② SHALL 成立:清单 SHALL 登记 D1 `Table 8.3` 表头写 `Trunk` 而转移记号用 `T = Tip` 的**表头笔误**
- **AND** 子条件 ③ SHALL 成立:清单 SHALL 记录处置口径 = **以转移记号为准**
- **AND** 子条件 ④ SHALL 成立:清单 SHALL 登记「`UNIQUE` 不可达 ⇒ 保守降级」这一事实
- **AND** 子条件 ⑤ SHALL 成立:清单 SHALL **不**把上述矛盾**消解**掉
  (⇒ 矛盾**显式保留**,供未来规范修订时复核)

#### Scenario: TSS-09 —— 载体 SHALL **零自有表副本**且 **零运行期状态**

- **WHEN** 核对规约载体的形态
- **THEN** 子条件 ① SHALL 成立:载体 SHALL 只提供**结构**(态 / 事件 / 转移 / 投影 / 断言 / `basis`)
- **AND** 子条件 ② SHALL 成立:载体 SHALL **不**持有任何**运行期状态**(纯 `static constexpr`)
- **AND** 子条件 ③ SHALL 成立:载体 SHALL **不**复制 adapter 的实现状态
  (⇒ 第 1 个 change 的 `TileLinkCAdapter` SHALL **消费**载体而非复制其转移表)

#### Scenario: TSS-10 —— 编译期锁 SHALL 与运行时断言**双证**

- **WHEN** 核对规约载体的编译期锁
- **THEN** 子条件 ① SHALL 成立:`kStateCount == 3` SHALL 有编译期锁
- **AND** 子条件 ② SHALL 成立:`kEventCount == 4` SHALL 有编译期锁
- **AND** 子条件 ③ SHALL 成立:`kTransitionCellCount == 12` SHALL 有编译期锁
- **AND** 子条件 ④ SHALL 成立:`kUnknownCellCount == 0` SHALL 有编译期锁
- **AND** 子条件 ⑤ SHALL 成立:**每条**编译期锁 SHALL **同时**有**运行时**断言
  (⇒ 铁律 ③:编译期锁**不得**只以「编译红」充当 ctest 判据)

#### Scenario: TSS-11 —— 判据 SHALL 遵守 4 铁律

- **WHEN** 核对本能力的判据设计
- **THEN** 子条件 ① SHALL 成立:纯负向断言 SHALL 有**正控**
- **AND** 子条件 ② SHALL 成立:每条判据 SHALL 有**可执行**反向 fixture 且**因正确原因红**
- **AND** 子条件 ③ SHALL 成立:编译期锁 SHALL **不**只以「编译红」充当 ctest 判据
- **AND** 子条件 ④ SHALL 成立:fixture 状态变迁后 SHALL **重新基线化**
  (⚠️ 本仓已 **3 次**复现「状态变迁 ⇒ 锚点失效」⇒ 毒化锚点须**跟随 owner 行**)
