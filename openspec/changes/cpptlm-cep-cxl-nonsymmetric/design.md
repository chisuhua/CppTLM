# design: cpptlm-cep-cxl-nonsymmetric(实现层)

> 上游 = 第 0 个 change `cpptlm-cep-cxl-nonsymmetric-state-spec`(**已归档**;main spec
> `openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`)。本设计**只消费**它,**不重推**语义。

## D-α `CxlCacheAdapter` = **委托**规约载体(零自有表副本)

**决策**:新文件 `include/tlm/cep/cxl_cache_adapter.hh`,形态**照 `MesifAdapter`**
(`mesif_adapter.hh`):类型**别名** + `constexpr` **委托** + **header-only** + **不进 `ModuleFactory`**。

| adapter | 状态表来源 | 先例 |
|---|---|---|
| `MesifAdapter` | **委托** `MesifStateSpec::kTable` | ✅ 本 change 照此 |
| `MoesiGpuAdapter` | **自带** 6×6 表 | ❌ 反例(双真源) |
| `ChiRnfAdapter` | **自带** 5×5 表 | ❌ 反例(无独立规约载体) |

**结构性保证**(照 `mesif_adapter.hh:19-27`):本文件**去注释后**文本 SHALL **不**含
`State::` / `Event::` / `Perm::` **任何字面量**,SHALL **不**含任何**表名**
(`kTransitions` / `kPermProjection` / `kTable`)⇒ 语法上**不可能**携带自己的表,
**只能调访问器**。

### D-α.0 ⚠️ 载体增补 `perm_of()` 访问器(对已归档规约层的**受控扩展**)

**问题**:载体原本只暴露 `kPermProjection`(**表**)。adapter 若要委托投影,就**必须指名该表名**
⇒ 直接撞上「零副本」的静态扫描(禁止记号含表名)⇒ 二者**互斥**。

**决策**:在**规约载体**增补**纯访问器** `perm_of(State, Perm&) -> bool`(逐行等价于
`kPermProjection`,**零语义变更**)⇒ adapter 只调**函数**,不碰表名。

⚠️ **对已归档 change 的影响登记**:`cep-cxl-nonsymmetric-state-spec` 已于 Entry #127
**归档**;本次是**纯访问器**增补 ⇒ 该 change 的 **CSS-01 ~ CSS-06 全部不受影响**
(判据与表**未变**)⇒ **无需**改动其 main spec ⇒ **不产生**冻结面冲突。
⇒ ⚠️ 教训:**零副本纪律要求「访问器优先」** —— 载体**只暴露表**时,adapter 只能「指名表」⇒
   纪律与载体形态**互斥** ⇒ 载体形态须**先**满足访问器需求(本次已补)。

### D-α.1 三态查询面(**不折叠** `Unknown`)

规约层的 `transition()` 返回 `Transition{kind, target, basis, pending, ref, note}`。
⇒ adapter SHALL 暴露**三态**查询,而**不是** MESIF 式的 `std::uint8_t`(会把 `Unknown` 折叠成哨兵值):

| 查询 | 返回 | 语义 |
|---|---|---|
| `transition_kind(s, e)` | `CellKind` | `Known` / `Illegal` / `Unknown` |
| `transition_target(s, e)` | `State` | **仅 `Known` 时有意义** |
| `transition_is_known(s, e)` | `bool` | 便捷判定 |

⚠️ **不做** MESIF 式的 `0xFF` 空洞哨兵 —— 那正是 D-γbis 判定为「新的未实测断言」的做法
(`Unknown` 被读成 `Illegal`)。⇒ **实现层必须能区分「不知道」与「非法」**。

### D-α.2 `perm` 投影 + 静默升级

- `perm_of(s, out)` ⇒ 委托 `CxlStateSpec::kPermProjection`(4→4);
- `is_silent_upgrade(s, e)` ⇒ 委托 `CxlStateSpec::is_silent_upgrade`
  ⚠️ **该判据的 basis = `public-analogical` + `pending`**(MESI 通用性质;公开来源未逐字断言)
  ⇒ adapter **SHALL NOT** 把它当已裁决事实陈述;登记为**残余风险**。

---

## D-β `SparseSnoopFilter` 行为实现 = **CONSERVATIVE 语义**

**落点**:`include/tlm/cep/sharer_tracker.hh`(与 `FullMapDirectory` / `HierarchicalDirectory` 同文件)
⇒ **不动** `kTrackerTraits` 表(`SparseSnoopFilter` 的 traits 行**已存在**,`sharer_tracker.hh:196-198`)。

**组合**:`class SparseSnoopFilter final : public SharerTrackerIf, public SnoopMechanics`
(照 `FullMapDirectory`;`SnoopMechanics` 提供 snoop 在飞句柄与被动记账 ⇒ **禁双实现**)。

### D-β.1 状态表示:「**可能**的 sharer 掩码」

| 概念 | 载体 | 说明 |
|---|---|---|
| 每个地址的**可能** sharer 掩码 | `std::map<std::uint64_t, std::uint64_t>` | 1 bit/node;**保守**:宁可多留,不可漏留 |
| 「**可能**的脏持有者」 | 同 map 的可选字段 | 用于 `FORWARD_TO` 提示 |

⚠️ **CONSERVATIVE 的含义(必须写准,否则实现会过度声称)**:
`SparseSnoopFilter` **不**能精确回答「**没有**其他 sharer」;
它只能回答「**可能**还有其他 sharer」⇒ 补偿 `O` 态投影损失时,
SHALL 表现为**保守近似**,SHALL **NOT** 声称等价恢复(per Entry #124 ⑤ / Entry #125 ⑤)。

### D-β.2 `lookup` 的三档判定

| 情形 | 判定 | 依据 |
|---|---|---|
| filter 无该地址条目 ⇒ 「**可能**有别人」 | `BROADCAST` | **保守默认**;`broadcast_fallback = true`(traits) |
| 条目存在且**唯一**可能的 sharer 是脏持有者 | `FORWARD_TO`(该持有者) | filter 的位集是 sharer 集的**上界** ⇒ 掩码只含 1 位 ⇒ 不可能有别的 sharer ⇒ 转发安全 |
| 条目存在但**可能**的 sharer > 1 | `BROADCAST` | 其余可能的 sharer 也须被 snoop ⇒ 单点转发会**漏探** |
| contention 在飞 | `LookupHazard::STALL` | `ADR-CEP-04 §4` 的 `Hazard_STALL` 契约 |

⚠️ **`ANSWER_HERE` 在本实现中**永不**发生** —— 这是 CONSERVATIVE 的**定义性后果**,不是缺陷:
`completeness = CONSERVATIVE` ⇒ filter **不能**回答「**没有**其他 sharer」
⇒ 该回答只能由 **EXACT** 目录(`FullMapDirectory`)给出。
⇒ ⚠️ **「永不 `ANSWER_HERE`」正是本实现与 `FullMapDirectory` 的可观测差异**
(⇒ 满足 `ADR-CEP-04 §13` 的「克隆体不可能」判据:EXACT 目录**能**答「没有其他 sharer」,本实现不能)。
⚠️ 另:`lookup` 的签名**不含请求者 id** ⇒ 「只含自己」在语义上**不可表达**
⇒ 原草稿的「条目只含自己 ⇒ `ANSWER_HERE`」一行为**不可达**,已删(2026-11-01 实现期收紧)。

⚠️ 「无条目 ⇒ `BROADCAST`」是 CONSERVATIVE 的**关键**:若按 EXACT 语义返回 `ANSWER_HERE`,
则 filter 的漏更新 ⇒ **漏探** ⇒ 正确性失败(判据 **AC-4** 的反向 fixture 即针对此)。

### D-β.3 R11 补偿路径

`moesi_gpu` 的 `O→WRITEBACK` 合并损失「没有其他 sharer」这一信息
⇒ 补偿 = **查 SF**:若 SF **不能**证明「没有其他 sharer」⇒ 该合并是**保守可接受**的
⇒ 补偿路径 SHALL 有**可执行**判据(判据 **AC-5**):消费 SF 的保守查询结果并**登记**结论。
⇒ ⚠️ 该判据 SHALL 判**保守补偿**语义,SHALL **NOT** 声称等价恢复(per Entry #124 ⑤)。

---

## D-γ CXL 实例声明:peer 集**平凡为空** ⇒ `sharer_tracker` = 「不参与」

**决策**:新增实例 `cxl_2_0` 到 `include/tlm/cep/protocol_instances.json`,
其 `sharer_tracker.impl` = **`"不参与"`**(照 `chi_rnf` 先例,`protocol_instances.json:208`)。

**理由**:口径 ③(host-centric / 单一一致性点)⇒ CXL 2.0 每 root port 只 1 个 cache device
⇒ 设备侧 sharer 集**平凡为空** ⇒ 给 CXL 实例挂 SF 会是**过度声明**。

⚠️ **因此**:D-β 的 SF 实现**不服务 CXL 实例**,而服务 **`moesi_gpu`** 的 R11 补偿
⇒ 二者在**同一 change** 内,但**归属不同** ⇒ 两条 delta spec 分开写
(`cep-cxl-nonsymmetric` / `cep-sparse-snoop-filter`)⇒ 归属可机器分辨。

**登记**:`repo_owned_instance_ids[]` 追加 `cxl_2_0`
(照 `chi_rnf` 先例,避开 `cep_decl_alignment_check.sh` ⑥ 镜像比对闸)。

---

## D-δ 静默升级登记:`perm_upgrades.json` 新增一行

**决策**:新增 `{ "instance_id": "cxl_2_0", "from": "UNIQUE", "to": "WRITEBACK" }`
(形态照既有 3 条;`from`/`to` 受跨 schema `$ref` 绑到 `perm` 枚举 ⇒ 机制上不可能引入第 5 个值)。

⚠️ **语义诚实性**:该登记断言「`E` + 写事件 → `M`」是 `cxl_2_0` 的静默升级。
其 basis = `public-analogical` + `pending`(D-α.2)⇒ 登记**不**改变证据强度,
但**必须**在实例声明旁注明该格 `pending`(⇒ 避免读者把登记读成「已确证」)。

---

## D-⑤ 冻结面:**3 件**,**两步授权**

| 冻结件 | 追加/修改内容 | 授权 |
|---|---|---|
| `docs/architecture/07-coherence-exploration-platform.md` | §5.1.1 **CXL 2.0 行**的措辞对齐裁决 ③⑦(「设备不感知其他设备」在 2.0 **平凡成立**;「CXL 3.0 对称」按裁决 ⑦ 指 **HDM-DB/CXL.mem** 而非 CXL.cache)⇒ **真源 1 vs 真源 3 措辞冲突消解** | 台账 entry + `--update --authorized-by` |
| `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` | 第 3 个行为实现(`SparseSnoopFilter`)**落地**登记 + R11 降级条件(V-1∧V-2∧V-3) | 台账 entry + `--update --authorized-by` |
| `openspec/specs/cep-sharer-tracker-base/spec.md` | ⚠️ **第 3 件 = 实现期才发现**(2026-11-01):该 main spec 写着「**其余 4 种实现**(`SparseSnoopFilter` / …)**的行为实现尚未落地**」+ Scenario「非 FullMap 的其余 **4** 实现 SHALL 只做声明层断言(deferred)」⇒ `SparseSnoopFilter` 落地后**已成过期陈述** ⇒ 按真源原则**必须**改 | 同上(改后须 `--update` 重冻) |

⚠️ **第 3 件的性质与前两件不同**:前两件是**追加**(正文不可变 ⇒ 只加 `## Status Update`);
第 3 件是**改写既有 Requirement 文本**(冻结 M1 产物)⇒ 须走 delta spec(`MODIFIED Requirements`)
⇒ **归档动作会写该文件** ⇒ 顺序须为:台账 entry(授权)→ delta spec 落盘 → `openspec archive`
→ `--update --authorized-by`(重冻 + provenance)→ commit ⇒ ⚠️ **归档与重冻的顺序不可颠倒**。

⚠️ `07 号` 的 CXL 行**措辞**改动**只限**该行;⚠️ `ADR-CEP-04` 的 §2 caps 表 / §4 hazard 表**零改动**
(caps/hazard **消费**,不重推 —— per `ADR-CEP-04` Status Update 的 V-1)。

---

## D-⑥ 已排除的替代方案

| # | 替代方案 | 排除理由 |
|---|---|---|
| 1 | CXL 实例挂 `SparseSnoopFilter` | peer 集平凡为空 ⇒ **过度声明**;且会让 SF 的归属再次混叠 |
| 2 | 给 CXL adapter 写**自带**转换表(照 `MoesiGpuAdapter`) | 双真源 ⇒ 规约改一格 adapter 不红(#9 教训) |
| 3 | 用 `0xFF` 空洞哨兵表示 `Unknown`(照 mesif) | `Unknown` 被读成 `Illegal` ⇒ **新的未实测断言**(D-γbis 已否决) |
| 4 | 把 `SparseSnoopFilter` 实现另起一个 change | Entry #112 去重裁决 + Entry #124 复核维持(同载体 `sharer_tracker.hh`) |
| 5 | 为 CXL 写 protocol engine + JSON 装配 | `moesi_gpu` 先例 = **无 engine** ⇒ adapter-only 是有效形态;引擎属后续(非本 change 范围) |
| 6 | 落地 SF 后**立即**把 R11 降为 🟢 低 | R11 降级须 **V-1 ∧ V-2 ∧ V-3** 全达成(行为一致 ∧ 补偿有判据 ∧ 既有测试 0 回归)⇒ 本 change 收官时才评估 |

---

## 依据

- 规约载体(main spec):`openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`
- 锚点摘要(basis 唯一人工审计面):`docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`
- `ADR-CEP-04` §2 caps / §4 hazard(消费,不重推)· `ADR-CEP-02` `perm` 4 值锁
  · `ADR-CEP-12` §10(静默升级落 `perm_upgrades.json`)· `ADR-CEP-11` SU(口径 7 条)
- 台账 Entry #122 / #124 / #125 / #126 / #127
