# ADR-CEP-14: Stub 可验证性合同测试

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **Stub Cache Core 的合同测试边界 + 10 条合同测试 + 组合接缝检查项的落地位置**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §3.2(Stub Cache 的 RN 侧投影)+ §11(Gap → 实现状态映射)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §4.2.4 / §5.4 F12
>
> **一手锚点**:
> - ProtoGen ISCA 2018 **p.1 / p.12**:真实 non-stalling MSI controller 约 **18-20 状态 / 46-60 转换**(E1)→ Stub 的 4 个稳定态是**刻意简化**,不是缓存模型
> - 源码 `include/tlm/cache_tlm.hh`:既有 `CacheTLM: public ChStreamModuleBase`(174 行)→ CEP Stub 与它是**不同物**(per `ADR-CEP-15`)

---

## 1. 背景

### 1.1 问题

`ADR-CEP-05` 定义了 Stub Cache Core 的 6 个接口、折叠映射表与 7 条不变量,并在 §7 给出 2 条 α 失效判据。但:

- **"合同"从何处变成"可执行测试"** 未定义 —— Stage 0 需交付 `test/test_cep_canonical_invariants.cc`,但它测什么、测到哪一层,没有边界
- **双向性缺失** —— 现有不变量只约束 Stub **必须做什么**;没有约束 Stub **不得做什么**(即协议信息不得泄漏给 Stub)
- **组合接缝的检查位置缺失** —— `ADR-CEP-07 IC7`(插件声明 `ordering_assumption` + `vc_binding`)与 `ADR-CEP-08 IC2`(E 态静默升级必查项)都要求"**被合同测试覆盖**",但**覆盖点**未指定

### 1.2 关键前提:Stub **不是**真实缓存

`ADR-CEP-05 §2` 的定位澄清已写死:Stub Cache Core 是**客户端侧协议状态投影**,不是缓存性能模型。因此合同测试**不应**测缓存性能(命中率 / 替换 / 预取),而应测**投影契约**。

→ 这条前提直接决定了合同测试的边界。

---

## 2. 决策

### 决策 1:合同测试的边界 —— 测**投影契约**,不测**缓存行为**

**决策**:合同测试断言的是「Stub 对 Coherence Interface 的投影行为是否符合契约」,**不**断言「Stub 作为一个缓存是否行为正确」。

| 测 | 不测 | 理由 |
|----|------|------|
| Stub 收到 `request(perm=READ)` 后发的 `completion` 携带正确 perm | 命中率 / 替换策略 / 预取 | Stub 无性能模型(per `ADR-CEP-05 §2`) |
| Stub 对 `snoop` 的响应延迟是**参数化固定值** | 延迟是否"真实" | 同上 |
| Stub 的 `evict_line` **不选** outstanding line | 驱逐策略优劣 | 同上 |
| Stub **不暴露**瞬态态 / 协议私有态 | —— | `INV-CEP-05-IC1` |
| Stub 的 `writeback` 载荷规则(dirty 必带 data) | 写回合并策略 | `INV-SC5` |

**代价**:合同测试**不能**发现 Stub 的性能建模缺陷 —— 这是刻意的(Stub 无性能模型)。

### 决策 2:10 条合同测试的清单与归属

| # | 合同测试 | 断言的不变量 | 归属测试文件 |
|---|---------|------------|------------|
| **C1** | 接口完备性 | net-side 恰 5 + core-side 恰 1 = **6** | `test_cep_canonical_invariants.cc` |
| **C2** | 折叠映射表:5 隐藏流 → stub 原语 | `CEN-Ib6` 不可分辨性 + 5 条折叠不变量 | 同上 |
| **C3** | 权限状态值域 | `perm` 恰 5 值 `NONE/I/S/E/M`,**不含**瞬态 / token / CHI 七态 | 同上(`INV-CEP-05-IC1`) |
| **C4** | outstanding 纪律 | `INV-SC1` + `INV-SC2` | 同上 |
| **C5** | snoop 响应延迟参数化 | `INV-SC3` | 同上 |
| **C6** | completion 配对 + `txn_id` 命名 | `INV-SC4` | 同上 |
| **C7** | writeback 载荷规则 | `INV-SC5` | 同上 |
| **C8** | hit 路径零网络流量 | `INV-SC6` | 同上 |
| **C9** | **协议信息不泄漏**(负向) | `INV-CEP-05-IC1` 的**反向**断言 | 同上 |
| **C10** | **组合接缝检查项** | `INV-CEP-07-IC7` + `INV-CEP-08-IC2` | 同上(Stage 0 契约层)+ Stage 1 实现层 |

### 决策 3:合同测试是**双向**的(C9 负向断言)

**决策**:除 C1~C8 的**正向**断言外,必须有 C9 的**负向**断言 —— 验证 Stub **没有**泄漏协议信息。

**C9 的具体断言**:
| 负向断言 | 检验方式 |
|---------|---------|
| Stub 的公开接口**不含**任何瞬态态 / token / read-set 字段 | 反射或类型 trait 检查 Stub 的公开成员类型 |
| Stub **不感知**协议名(CHI / CXL / MESI / MOESI) | Stub 源码 grep 协议标识符 |
| Stub **不感知**协议族 | 同上 |
| Stub **不感知** `ordering_assumption` / `vc_binding` / `consistency_labels` | 同上(这些是 ProtocolEngine 侧声明,per `ADR-CEP-07 IC7`) |

**理由**:`ADR-CEP-05 §7` 的两条 α 失效判据**方向相反**:
- 判据 1(**协议 → stub 泄漏**):Stub 收到 `completion` 时**无法**区分数据来自 peer 还是 memory ⇒ 不可分辨性
- 判据 2(**stub → 协议信息不足**):Stub 暴露的 4 值 `perm` 可能**不足以**让协议侧区分「静默 E→M」与「显式请求升级」

若合同测试只做正向断言,**判据 1** 会漏(C2 只测了"forward_data 不可分辨"这一个具体点,未测一般性的"无协议信息")。→ C9 是判据 1 的**一般化**。

### 决策 4:组合接缝检查项(C10)的两阶段落地

**决策**:C10 分两阶段,Stage 0 只落**契约层**(检查项**存在且被引用**),Stage 1 落**实现层**(检查项**真的执行**)。

| 阶段 | C10 断言 | 归属 |
|------|---------|------|
| **Stage 0** | 组合接缝的**检查项清单存在**(至少含"E 态静默升级"项);**组合检查器缺失该项时,其结果判定为 fail**(不得静默跳过) | `test_cep_canonical_invariants.cc` |
| **Stage 1** | 组合检查器**实际执行**该项并给出正确判定 | `cpptlm-cep-directory-ordering-plane`(per `ADR-CEP-08 IC2` / `ADR-CEP-09 Decision A`) |

**理由**:与 umbrella change 的 **D1(契约层 / 实现层分离)**一致。

### 决策 5:测试替身策略 —— 用**协议替身**验证 Stub,而非用真协议

**决策**:合同测试用一个**协议替身**(test double)驱动 Stub,而非真实 adapter。

| 替身 | 用途 | 约束 |
|------|------|------|
| `NullProtocolEngine`(既有) | 10 流 no-op,用于 C1/C4/C5/C6/C8 | 已是 `ADR-CEP-07` 决策 4 的默认实现 |
| **协议感知替身**(新增,test 内部) | 驱动 C2/C3/C7/C9 —— 需产生特定 perm / dirty 组合 / 数据来源 | 替身**不受** Stub 合同约束(它测的是 Stub) |

**约束**:替身**不得**进 `include/tlm/cep/`(那是产品代码目录);只在 `test/` 下。

### 决策 6:显式排除

| 排除项 | 理由 |
|--------|------|
| **不**测 Stub 的缓存性能行为 | 决策 1;Stub 无性能模型 |
| **不**测 Stub 与真实 adapter 的端到端行为 | → Stage 1 的 adapter change |
| **不**把合同测试当成实现验证 | 决策 1 代价 |
| **不**在 `include/tlm/cep/` 放测试替身 | 决策 5 约束 |

---

## 3. 不变量

- **INV-CEP-14-IC1**:每条 Stub 合同测试(C1~C10)**必须**可定位到至少一条 `INV-SC*` / `INV-CEP-05-IC1` / `CEN-Ib*` / `INV-CEP-07-IC7` / `INV-CEP-08-IC2`;**不得**存在无锚点的合同测试
- **INV-CEP-14-IC2**:合同测试**必须**包含**至少一条负向断言**(C9:Stub 不泄漏协议信息);缺负向断言 = 合同集不完整
- **INV-CEP-14-IC3**:组合接缝检查项**缺失**时,组合检查结果**必须**判 fail(不得静默跳过);Stage 0 校验"检查项存在",Stage 1 校验"检查项被执行"

> **编号位说明**:`INV-CEP-14-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **锚点完备**:每个合同测试有对应 invariant id(per `INV-CEP-14-IC1`)
2. **负向断言存在**:合同集**必须**含 C9(per `INV-CEP-14-IC2`)
3. **检查项不可静默跳过**:组合检查器缺项 ⇒ fail(per `INV-CEP-14-IC3`)
4. **替身不入产品目录**:`include/tlm/cep/` **不含**测试替身
5. **与 `ADR-CEP-05 §7` 的两条 α 失效判据对齐**:C2 覆盖判据 1 的具体点,C9 覆盖判据 1 的一般化,判据 2 由 C10 的 E 态项覆盖

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-02`(canonical 10 流)| ✅ v0.3 | `CEN-Ib6` 不可分辨性(C2 的核心) |
| `ADR-CEP-05`(Stub Cache)| ✅ v0.7 | 6 接口 + 7 不变量 + §7 两条 α 失效判据 |
| `ADR-CEP-07`(仿真 + seam)| ✅ v0.7 | `INV-CEP-07-IC7`:插件声明排序假设 + VC 绑定(C10 覆盖) |
| `ADR-CEP-08`(Ordering Point)| ✅ v0.1 | `INV-CEP-08-IC2`:E 态静默升级必查项(C10 覆盖) |
| `ADR-CEP-12`(能力映射表)| ✅ v0.1 | schema 的机器可检查约束与 C1~C3 同源 |
| `ADR-CEP-15`(CacheTLM 对齐)| ✅ v0.1 | 明确 CEP Stub 与既有 `CacheTLM` 是**不同物** |
| `ADR-CEP-07`(Stage 0 umbrella)| 🟡 **Proposed** | 交付 `test_cep_canonical_invariants.cc`(C1~C10) |
| `cpptlm-cep-stub-cache-core`(Stage 1.1)| 📋 Planned | 真实 Stub 实现 → C1~C10 从**契约层**转**实现层**验证 |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 合同测试边界 | 测**投影契约**,不测缓存行为 | ✅ 已拍板 |
| 10 条合同测试 | C1~C10 清单 + 归属 | ✅ 已拍板 |
| 双向性 | **必须**含负向断言 C9 | ✅ 已拍板 |
| 组合接缝两阶段 | Stage 0 查"存在" / Stage 1 查"被执行" | ✅ 已拍板 |
| 测试替身 | 用协议替身;替身只在 `test/` | ✅ 已拍板 |
| 显式排除 | 不测性能 / 不测端到端 / 不当实现验证 / 替身不入产品目录 | ✅ 已拍板 |

---

## Status Update 2026-10-27 —— C9 四条要求的覆盖现状对账 + C++ 侧机器断言补齐

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §6 正文一律不改**,仅登记状态变化与覆盖现状。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #83**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #83)。

### ① C9 四条要求的覆盖现状对账(实测)

§2 决策 3 列出的 C9 具体断言共 4 条。**2026-10-27 逐条实测结果**:

| # | C9 要求(§2 决策 3 原文) | 检验方式 | 2026-10-27 实测覆盖 |
|---|---|---|---|
| 1 | Stub 的公开接口**不含**任何瞬态态 / token / read-set 字段 | 反射或类型 trait 检查 | 🟡 **部分** —— `test_cep_stub_cache_core.cc` Scenario 3 断言 4 个 RN-F **细节态名**(`UC`/`SC`/`UD`/`SD`)不得作为**标识符**出现(去注释后扫);**反射 / 类型 trait** 形态未落地 |
| 2 | Stub **不感知**协议名(CHI / CXL / MESI / MOESI) | **Stub 源码 grep 协议标识符** | ✅ **本次补齐** —— commit `7f91abd9`:`test_cep_contracts_crosscheck.cc` 的 IC2 断言按本决策的「源码 grep」口径扫 `include/tlm/cep/stub_cache_core.hh`(去注释后 + 标识符词判定) |
| 3 | Stub **不感知**协议族 | 同上 | ✅ **本次补齐**(与第 2 条同一断言覆盖) |
| 4 | Stub **不感知** `ordering_assumption` / `vc_binding` / `consistency_labels` | 同上 | ✅ **本次补齐** —— 同一 TEST_CASE 内逐字段断言(归一化比较:`vc_binding` / `vcBinding` / `VC_Binding` 同形) |

### ② 检验口径的澄清(**不改变本决策**,只登记实现形态)

- **必须去注释后扫** —— 否则头文件里解释「禁止暴露 CHI RN-F 细节态」的中文散文会被喂进扫描器(**假阳性**),而把禁用词写进注释即可绕过(**假阴性**)。实现已上提到 `test/cep_adr_parser.hh` 的共享 `strip_comments()`(与 `test_cep_stub_cache_core.cc` 共用同一份,避免两份实现分叉静默改变断言语义)。
- **按「标识符词」判定,不按子串** —— `chi_rnf` / `ChiRnfAdapter` 均须命中;`chip_id`(「芯片」≠「CHI」协议)/ `hierarchical` 须**不**命中(子串匹配会把 `ARCHITECT` 误判为含 `CHI`)。
- **正控 + 反控随断言同在**(per 判据纪律「纯负向断言必须有正控」)。

### ③ 变异测试反证(2026-10-27 实测)

两类变异均被新断言捕获:① `sharer_tracker.hh` 注入 `ChiRnfAdapter` ⇒ 变红并精确报出**文件 + 协议名**;② `stub_cache_core.hh` 注入 `int vc_binding` ⇒ **首版未变红**,暴露首版断言误用「大写 + 拆词」的词集与小写原名比对 ⇒ **恒真空**(假绿);修正比较口径后重测 ⇒ 正确变红。
> ⚠️ **本条登记的是一次真实的「负向断言恒真空」缺陷**(纯负向断言若无正控 + 变异反证,绿不等于有效)。

### ④ 未覆盖项(诚实登记,不掩盖)

- **第 1 条的「反射 / 类型 trait」形态**仍未落地 —— 当前以「标识符词」扫描近似覆盖,**不等于**类型级证明(Stub 若以 `using` 别名间接暴露瞬态态,本断言不保证能发现)。归 Stage 1 实现层(`cpptlm-cep-stub-cache-core` 已归档的 6 接口形态上无此风险;新接口引入时须复核)。
- **`NullProtocolEngine` 侧**的 10 流覆盖矩阵(per `ADR-CEP-07` `INV-CEP-07-IC2`)尚无载体 —— owner 已定 = 队列 **#7 `cpptlm-cep-cohif-carrier`**(task 2.5 / 2.6 / 2.7,per 本次 Entry #83 增补)。

---

## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
