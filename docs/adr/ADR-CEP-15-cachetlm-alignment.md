# ADR-CEP-15: CppTLM CacheTLM 与 CEP Stub Cache 的对齐边界

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **既有 `CacheTLM` 与 CEP Stub Cache Core 的关系 / 冻结面 / 未来"真实缓存 + CEP 协议"的路径**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §3.2(Stub Cache RN 侧投影)+ §5.4(Bundle 约束)
> - 真源 3:`ADR-CEP-05-stub-cache-core-interface.md`(Stub 的 6 接口 + 定位澄清)
> - 真源 3:`ADR-CEP-14-stub-contract-tests.md`(合同测试边界)
> - 真源 3:`ADR-CEP-17-ruby-slicc-protocol-family.md`(协议族接入)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §5.4 F12(Stub 是协议状态投影,非缓存模型)
>
> **一手锚点(代码实证)**:
> - `include/tlm/cache_tlm.hh:35` — `class CacheTLM : public ChStreamModuleBase`(174 行)
> - `include/tlm/cache_tlm.hh:42` — `std::map<uint64_t, uint64_t> cache_lines_`
> - `include/tlm/cache_tlm.hh:89` — `uint64_t access_latency = hit ? 5 : 50;`
> - `include/tlm/cache_tlm.hh` — **全文 0 匹配** `snoop|coheren|mesi|moesi|directory`

---

## 1. 背景

### 1.1 问题

仓内已有 `CacheTLM`(174 行,`ChStreamModuleBase` 派生),CEP 又要引入 **Stub Cache Core**。二者都叫"缓存",极易被误认为同类,进而产生两类错误:

| 错误 | 后果 |
|------|------|
| **把 CacheTLM 当成 CEP 的 Stub** | 以为 CEP 已有缓存实现,实际二者语义不同 |
| **为"复用"而改 CacheTLM** | 破坏 dGPU 业务侧(phase-9 依赖),且违反冻结面 |

### 1.2 代码实证:二者**定位同向、粒度不同**

| 维度 | `CacheTLM`(既有) | CEP **Stub Cache Core**(新) |
|------|-----------------|--------------------------|
| 基类 | `ChStreamModuleBase` | `ChStreamModuleBase`(同类) |
| 状态载体 | `std::map<uint64_t, uint64_t> cache_lines_` | 4 个稳定权限态 `NONE/I/S/E/M` |
| 延迟模型 | hit=5 / miss=50 **周期**(硬编码) | **无性能模型**(参数化 snoop 响应延迟) |
| 协议知识 | **0**(无 snoop / coherence / mesi / moesi / directory) | **0**(per `ADR-CEP-05 §2` 定位澄清) |
| 职责 | **缓存性能模型** | **客户端侧协议状态投影** |
| 与 Coherence Interface 的关系 | **无关**(不实现 10 流) | **RN 侧投影**(6 接口,5 条 canonical 流) |

**关键结论**:二者的**"协议无关"取向同向**,但**粒度与职责完全不同**:
- `CacheTLM` 回答「**这个访问要花多久**」
- CEP Stub 回答「**这个访问在协议上意味着什么**」

---

## 2. 决策

### 决策 1:二者**共存,不替换、不合并**

**决策**:`CacheTLM` 与 CEP Stub Cache Core 是**两个不同的类**,互不替代、互不继承。

**理由**:
- 职责不同(上表)
- 生命周期不同:`CacheTLM` 属 dGPU 业务侧(phase-9);Stub Cache Core 属 CEP 探索平台
- 合并会产出一个"既无协议知识、又有协议知识"的两不像类

### 决策 2:`CacheTLM` **0 diff**(冻结面)

**决策**:`include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 CEP 全生命周期内 **0 修改**。

**理由**:
- phase-9(dGPU Phase 2/3)依赖它;CEP 是**独立探索平台**,不接管 dGPU 业务(per `ADR-CEP-07 §2 Decision 6`)
- 既有 ADR 已把它列入冻结面(per `ADR-CEP-05 §9` 的 A13 议题 = 本 ADR)

**机器检查**:`test/test_cep_module_factory_seam.cc` 的冻结面守门断言之一(per `ADR-CEP-07` 决策 3 / Stage 0 umbrella Task 5.1)。

### 决策 3:术语共享,实现不共享

**决策**:二者共享**术语**(cache line / hit / miss / latency / evict),但**不共享代码**。

| 共享的术语 | CEP 中的对应 |
|-----------|------------|
| cache line (64B,地址对齐) | 相同(per `07 号 §3.3`) |
| hit / miss | 语义相近但**度量对象不同**:CacheTLM 测延迟;CEP 测协议语义(`INV-SC6` hit 路径零网络流量) |
| latency | CacheTLM 是硬编码周期;CEP 无 latency 概念(latency 属 Fabric & Transport) |

### 决策 4:未来"真实缓存 + CEP 协议"的路径 —— **给 CacheTLM 加 adapter,而非改 CacheTLM**

**决策**:若将来需要"用真实缓存模型跑 CEP 协议",路径是:

```
CacheTLM(性能模型) ──adapter──> Coherence Interface(10 流)
```

即 **CacheTLM 侧加一个投影 adapter**,把 CacheTLM 的 hit/miss/latency 语义映射到 canonical 10 流。**CacheTLM 本身仍 0 改动**。

**理由**:
- 保持"协议无关"取向在两侧对称(CacheTLM 无协议知识;adapter 做翻译)
- 避免把协议知识注入性能模型
- adapter 是**产品代码**(进 `include/tlm/cep/`),不是测试替身(per `ADR-CEP-14` 决策 5 的对照)

**该 adapter 的落地**:属 **Stage 4+ 维护态**(per `ADR-CEP-18` 决策 5 的 T1~T4 判据,手写 adapter 在 adapter 数 ≤ 14 时仍是最优解);**Stage 1~3 不承诺**。

> ⚠️ **2026-10-22 限定注记**(per 台账 **Entry #12** [adr-amendment];**正文不改**,限定见 §8):
> 上述「adapter 数 ≤ **14**」的阈值**已被越过** —— `moesi_gpu` / `mesif` 登记后为 **16**
> (per `07 号 §5.1.1`)。本判据的**意图**是「手写 adapter 在**实例数不大**时仍是最优解」
> —— 该结论**需按 16 重新评估**(per `ADR-CEP-18` 决策 5 的 T1~T4 判据);
> **但这不改变**本 ADR 的其余结论(决策 1~5 全部不依赖该计数)。详见 §8。

### 决策 5:显式排除

| 排除项 | 理由 |
|--------|------|
| **不**把 `CacheTLM` 重命名 / 改造成 Stub | 决策 1 + 决策 2 |
| **不**让 Stub Cache Core **继承** `CacheTLM` | 二者无 is-a 关系;继承会带入性能模型语义 |
| **不**在 CEP 的 `include/tlm/cep/` 下放 `CacheTLM` 的副本 | 决策 2 |
| **不**让 CacheTLM 实现 canonical 10 流 | CacheTLM 无 snoop 概念(代码实证);强行实现会破坏其性能模型定位 |
| Stage 1~3 **不**交付"真实缓存 + CEP 协议"的 adapter | 决策 4(Stage 4+) |

---

## 3. 不变量

- **INV-CEP-15-IC1**:`include/tlm/cache_tlm.hh` 与 `src/tlm/cache_tlm.cc` 在 CEP 生命周期内 **0 diff**(机器可检查)
- **INV-CEP-15-IC2**:CEP **不得**定义 `CacheTLM` 的派生类或副本;CEP 的缓存侧实现**只有** Stub Cache Core 一个
- **INV-CEP-15-IC3**:`CacheTLM` **不得**实现 canonical 10 流的任何一条(它无协议知识);CEP 与 `CacheTLM` 之间**只允许**通过 adapter 交互

> **编号位说明**:`INV-CEP-15-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **冻结面**:`git diff --name-only <CEP 基线 commit> -- include/tlm/cache_tlm.hh src/tlm/cache_tlm.cc` **必须为空**(per `INV-CEP-15-IC1`)
   > ⚠️ **不能用** `git diff HEAD` —— CEP commit 落地后 HEAD 已含改动 → diff 恒为空 → 守门在 CI(检出 PR 分支)里**恒绿**。
   > 契约说的是「**在 CEP 生命周期内** 0 diff」,即相对 **CEP 起始基线 commit**。
   > 基线钉死在 `test/test_cep_module_factory_seam.cc` 的 `kCepBaselineCommit`(可用环境变量 `CEP_BASELINE_COMMIT` 覆盖);
   > 机器守门见 `test_cep_module_factory_seam` 的「冻结面守门机制自证」用例(故意用真实改动验证守门非恒绿)。
2. **无副本**:`include/tlm/cep/` 下**不得**出现 `CacheTLM` 的派生或复制(per `INV-CEP-15-IC2`)
3. **无协议污染**:`CacheTLM` 源码**不得**出现 `snoop` / `coherence` / `MESI` / `MOESI` / `directory` 标识符(per `INV-CEP-15-IC3`)
4. **术语不混淆**:`07 号 §3.3` 的"缓存核心协议无关性"段**必须**说明与 `CacheTLM` 的边界

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-05`(Stub Cache)| ✅ v0.7 | Stub 的 6 接口 + 定位澄清(协议状态投影,非缓存模型) |
| `ADR-CEP-07`(仿真 + seam)| ✅ v0.7 | 决策 6 的冻结面边界 |
| `ADR-CEP-12`(能力映射表)| ✅ v0.1 | `combination_capabilities` 的 `concurrent` 维度与"真实缓存"路线相关 |
| `ADR-CEP-14`(Stub 合同测试)| ✅ v0.1 | 合同测试**不测**缓存性能(与 `CacheTLM` 的性能模型职责分离) |
| `ADR-CEP-18`(协议生成路线)| ✅ v0.1 | 决策 4 的 Stage 4+ 归属 |
| `ADR-CEP-07A`(06-bis 产物处置)| ✅ v0.1 | `coherence_domain.hh` / `coherent_xbar_tlm.hh` 的处置与本 ADR 的边界一致 |
| `cpptlm-cep-stub-cache-core`(Stage 1.1) | 📋 Planned | 新建 Stub Cache Core(**不**改 `CacheTLM`) |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 共存不替换 | 两个不同的类,互不替代 / 互不继承 | ✅ 已拍板 |
| `CacheTLM` 0 diff | 冻结面,机器可检查 | ✅ 已拍板 |
| 术语共享实现不共享 | 共享 hit/miss/latency 术语,代码独立 | ✅ 已拍板 |
| 未来路径 | 给 CacheTLM 加 **adapter**,不改 CacheTLM;Stage 4+ | ✅ 已拍板 |
| 显式排除 | 不重命名 / 不继承 / 不复制 / 不实现 10 流 / Stage 1~3 不交付 adapter | ✅ 已拍板 |

---

## 7. 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | 代码实证(`cache_tlm.hh` 174 行 / 0 协议关键词)+ 调研 F12 + `ADR-CEP-05 §9` A13 议题 | 对齐边界 + 冻结面 + 未来路径 |

---

## 8. Status Update(2026-10-22,per 台账 Entry #12 [adr-amendment])

> **授权**:台账 `objective-cep-platform §11` **Entry #12**;触发 = R-13(`moesi_gpu` / `mesif` 登记进
> `07 号 §5.1.1`,adapter 实例数 14 → 16)。**正文不改**(per ADR 不可变约定),限定记录于本节。

### 8.1 决策 4 末条的「adapter 数 ≤ 14」阈值 —— **已越过,需重评**

| 项 | 内容 |
|----|------|
| **原文**(决策 4 末条) | 「手写 adapter 在 adapter 数 ≤ **14** 时仍是最优解」 |
| **事实变化** | `moesi_gpu` / `mesif` 登记进 `07 号 §5.1.1` 后,adapter 实例数 **14 → 16**(工业 6 族 8 实例 + Ruby 1 族 8 实例)→ **阈值已被越过** |
| **限定后的表述** | 「手写 adapter 在 adapter 数 ≤ **16** 时是否仍是最优解,**待按新数重评**」(重评归属 `ADR-CEP-18` 决策 5 的 T1~T4 判据) |
| **不影响本 ADR 的结论** | 决策 1(共存不替换)· 决策 2(`CacheTLM` 0 diff 冻结面)· 决策 3(术语共享实现不共享)· 决策 4 的**路径方向**(给 CacheTLM 加 adapter 而非改 CacheTLM)· 决策 5(显式排除)—— **全部不依赖该计数**,零改动 |
| **对 `ADR-CEP-18` 的连带** | `ADR-CEP-18:68` 的「为 **16~20** 个实例建编译器,投入产出倒挂」是**同一问题**的另一次表述 —— 两处口径**已同步** |

### 8.2 未变更项(明示)

- **§2 决策 1~5 的全部结论** —— **零改动**
- **§3 的 3 条不变量** —— **零改动**
- **§4 的抓取点** —— **零改动**
- **`CacheTLM` 的 0 diff 冻结面**(`INV-CEP-15-IC3`)—— **零改动**

---

**版本**: v0.2 草案
**最后更新**: 2026-10-22
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
