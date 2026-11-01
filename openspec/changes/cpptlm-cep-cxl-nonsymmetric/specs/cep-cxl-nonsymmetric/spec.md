# cep-cxl-nonsymmetric Specification

## Purpose
定义 **CXL 2.0 非对称**(CXL.cache · host-centric)在 CEP 平台上的**实现层可执行契约**:
`CxlCacheAdapter`(委托规约载体 · 零自有表副本)· **三态转换查询面** · CXL 实例声明 ·
静默升级登记。

**上游**:`cep-cxl-nonsymmetric-state-spec`(**已归档**;main spec
`openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`)—— 本能力**只消费**它,**不重推**语义。

> **边界**:不改 `perm` 4 值锁 · 不扩 `CATEGORY_*` 3 类 · 不改 `CohIfBundle` ·
> 不新增 ABI · 不新增 invariant 编号数 · **不产出任何性能结论** ·
> **不写** CXL protocol engine(照 `moesi_gpu` 先例 = adapter-only)。
>
> **合规边界**:CXL 规范正文**不得**投喂任何 AI(ECA §3 + AI AUP)⇒ 本能力的所有语义
> **只来自**公开非规范来源(锚点摘要)与**已裁决**口径。

---

## ADDED Requirements

### Requirement: cep-cxl-nonsymmetric

CEP SHALL 为 **CXL 2.0 非对称**提供**实现层**载体:`CxlCacheAdapter` SHALL **委托**规约载体
(**零自有表副本**)· 转换查询 SHALL 区分 `Known`/`Illegal`/`Unknown` 三态 ·
CXL 实例 SHALL 登记且其 `sharer_tracker` SHALL 为「不参与」· 静默升级 SHALL 登记于
`perm_upgrades.json`。

#### Scenario: CNA-01 —— adapter SHALL **零自有表副本**地委托规约载体

- **WHEN** 扫描 `CxlCacheAdapter` 的**去注释后**文本
- **THEN** 子条件 ① SHALL 成立:SHALL **不**含 `State::` / `Event::` / `Perm::` **任何字面量**
- **AND** 子条件 ② SHALL 成立:转换 / 投影 / 静默升级查询 SHALL **全部**委托 `CxlStateSpec`
- **AND** 子条件 ③ SHALL 成立:该扫描器 SHALL **有鉴别力** —— 注入任一字面量 ⇒ **必须因正确原因红**

> **语义**:双真源 = 规约改一格而 adapter 不红 ⇒ #9 的教训(`MoesiGpuAdapter` 自带 6×6 表)。

#### Scenario: CNA-02 —— 转换查询 SHALL **区分三态**,`Unknown` SHALL **不**被折叠

- **WHEN** 对 68 格逐格查询 `transition_kind` / `transition_target`
- **THEN** 子条件 ① SHALL 成立:每格的 `kind` SHALL 与规约载体**逐格相同**
- **AND** 子条件 ② SHALL 成立:`kind = Unknown` ⇒ `target` **SHALL NOT** 被当作有效目标态消费
- **AND** 子条件 ③ SHALL 成立:adapter SHALL **不**提供把 `Unknown` 折叠为哨兵值的接口
  (⇒ 反向 fixture:要求返回 `0xFF` 空洞哨兵的形态 ⇒ **必须因正确原因红**)

> **语义**:「未知」≠「非法」—— 折叠即**新的未实测断言**(per 规约层 `design.md D-γbis` 已否决
> mesif 的 `0xFF` 空洞做法)。

#### Scenario: CNA-03 —— CXL 实例声明 SHALL 登记,且 `sharer_tracker` SHALL 为「不参与」

- **WHEN** 核对 `protocol_instances.json` 的 `cxl_2_0` 实例块
- **THEN** 子条件 ① SHALL 成立:`13` 个 required 键 SHALL 齐备(schema 驱动)
- **AND** 子条件 ② SHALL 成立:`sharer_tracker.impl` SHALL 为 **`"不参与"`**
  (⇒ **不得**给 CXL 实例挂 `SparseSnoopFilter` —— peer 集平凡为空,挂 SF 是过度声明)
- **AND** 子条件 ③ SHALL 成立:`cxl_2_0` SHALL 登记进 `repo_owned_instance_ids`
  (⇒ 避开声明镜像比对闸)
- **AND** 子条件 ④ SHALL 成立:该实例 SHALL **不**被登记进任何跨仓镜像面

#### Scenario: CNA-04 —— 静默升级 SHALL 登记于 `perm_upgrades.json`,且 SHALL **不**引入第 5 个 `perm` 值

- **WHEN** 核对 `perm_upgrades.json` 的 `silent_upgrades[]`
- **THEN** 子条件 ① SHALL 成立:SHALL 存在 `instance_id = "cxl_2_0"` 且 `from = UNIQUE` / `to = WRITEBACK`
- **AND** 子条件 ② SHALL 成立:`from` / `to` SHALL 解析到 `perm` 枚举(跨 schema `$ref`)
- **AND** 子条件 ③ SHALL 成立:该登记 SHALL **不**改变对应格的证据强度(仍 `pending`)⇒
  实例声明旁 SHALL 有 `pending` 注记(⇒ 不得被读成「已确证」)

#### Scenario: CNA-05 —— `Unknown` 格的消费 SHALL 走**显式降级**路径

- **WHEN** 实现层在转换查询遇到 `kind = Unknown`
- **THEN** 子条件 ① SHALL 成立:SHALL 走**保守降级**(保守行为 + 事件登记)
- **AND** 子条件 ② SHALL 成立:SHALL **不**静默当作 `Illegal`
- **AND** 子条件 ③ SHALL 成立:SHALL **不**臆断目标态
  (⇒ 反向 fixture:把 `Unknown` 当 `Illegal` ⇒ **必须因正确原因红**)

> **语义**:42 格 `Unknown` 的合法转移在公开来源不可得(R-2;规范 §3.2.5.15 禁读)⇒
> 实现层**只**能保守处理,不得声称已确证。
