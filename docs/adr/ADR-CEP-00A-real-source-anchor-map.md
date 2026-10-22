# ADR-CEP-00A: Coherence Exploration Platform 真源章节映射表

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 系列 ADR 引用的真源文档章节映射单一真源**,解 oracle B2-2/B2-3 锚点集群问题
> **关联真源**:
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4
> - 真源 1:`docs/architecture/06-bis-coherence-as-network-extension.md` v0.3
> - 真源 1:`docs/architecture/01-hybrid-architecture-v2.1.md` v2.1.10
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口 ADR)
> - 真源 3:`ADR-X.1-transaction-id.md`(txn_id 命名空间)
> - 真源 3:本系列 CEP ADR(CEP-01 ~ CEP-05 + 00A + 06 + 07 + NUM)

---

## 1. 背景

### 1.1 问题

CEP 系列 ADR(CEP-00 ~ CEP-05)在 v0.1/v0.2 起草过程中,**对真源文档章节引用存在系统性失锚**:

| 引用错误 | 实证 | 影响 |
|---------|------|------|
| "06-bis §7.8 P2-P5" | §7.8 实为"v0.3 总 KPI 调整"(per `06-bis:653`);P2@`06-bis:621` / P3@`:634` / P3.5@`:638` / P4@`:645` / P5@`:649` | 5 份 ADR 共 9 处 supersede 范围失锚 |
| "06-bis §11.5 P3-P6" | §11.5 实为"相关测试"(per `06-bis:794`);且 06-bis 无 P6 | 2 处头部残留 |
| "ADR-I01" | 不存在 | 4 处引用落空 |
| "ADR-G04" | 不存在 | 1 处引用落空 |
| "06 §15.4 P3 Stub Cache 5 接口" | §15.4 P3 实为 SnoopFilter(per `06:1213-1222`)| 2 处引用失锚 |

**根因**:起草者依赖回忆而非 grep 实证;AGENTS.md "真源原则"要求"引用必须真实" — 这是 ADR 起草的硬约束。

### 1.2 修复方法

**长期**(`scripts/test/docs_sync_check.sh` 增加 ADR 锚点 lint) + **短期**(本 ADR):

1. **本 ADR 作为"真源章节映射"单一真源** — 所有 CEP 系列 ADR 引用真源文档章节时,必须先查本表确认章节存在
2. **每条映射含 3 字段**:真源文档路径 / 章节号 / 章节标题(从真实文件 grep 提取)
3. **章节不存在 = 标 `(伪引, 本 ADR 不收录)`** — 起草时禁止引用

---

## 2. 决策

### 决策 1:本 ADR 作为 CEP 系列 ADR 真源引用的"单一真源"

任何 CEP 系列 ADR(v0.3 及以后)的"关联真源"或"参考章节"引用,必须先查本 ADR 确认章节存在。若章节不存在,引用视为**伪引**,本 ADR 不收录,起草者必须重新选择锚点或声明"首次定义"。

### 决策 2:不变量 CEP-00A-IC1(机器可查)

```text
CEN-00A-IC1: CEP 系列 ADR 引用的所有真源文档章节号,必须在 ADR-CEP-00A §3 表中存在;
否则视为伪引,机器可查(grep 引用章节号 + grep 本表章节号 = 必须命中)。
```

**实施机制**:可作为 `scripts/test/docs_sync_check.sh` 后续 lint。

### 决策 3:章节标题从真源文件 grep 提取,不允许凭记忆

**未来 ADR 起草流程**:

1. 起草 ADR 时,所有真源章节引用先用 `grep -n "^## \|^### "` 提取章节号 + 标题
2. 复制 3 字段到本表(路径 / 章节号 / 标题)
3. ADR 正文从本表 cross-ref 章节,不允许直接写章节号

---

## 3. 真源章节映射表

### 3.1 `docs/architecture/06-inter-module-network.md` v0.4

| 章节号 | 章节标题 | 行号 | 用途 |
|--------|---------|------|------|
| §1.1 | **用户使用场景(源自 4 轮讨论锁定)**(per oracle B3-3 实证 — 非"通信框架层")| 行 75 | **不用于真源 1 canonical 6 层**(错引自此)|
| §2.1 | 6 层结构(L1-L6)对齐 | 行 110-151 | 真源 1 canonical 6 层模型 — **与 CEP 6 层架构正交共存** |
| §4.1 | seam 在 L5 Bundle ↔ L4 StreamAdapter 之间 | 行 328+ | seam 锚点 |
| §4.4 | ProtocolBridge<InB, OutB, Ordered> | 行 365-414 | 真源 3 沿用(协议桥模板) |
| §4.7 | **不存在**(06 §4 仅至 §4.5,行 417)| — | **per oracle B3-5 实证,simulation_mode 真源在 §5.1 + 01-hybrid §4.7** |
| §5.1 | 单一 JSON 维度: `simulation_mode` | 行 436 | **simulation_mode 真源之一**(per B3-5)|
| §5.2 | 不引入 network.mode 字段 | 行 451 | 真源 1 拒绝 network.mode 的依据 |
| §15.1 | Coherence 演进总览 | 行 1170-1179 | ArchForge coherence 文档清单引用 — **无 10 条流清单**(per B1)|
| §15.4 | **Coherence 演进推进路径(P2.0-P5,**无 P6**)** | 行 1213-1222 | **per oracle B3-7 实证 — 与 06-bis §7 编号一致,无 P6** |
| §15.4 P2.0 | CoherenceProtocol C++ 实现 | 行 ~1215 | 阶段定义 |
| §15.4 P2.1 | CoherenceBridge C++ 实施 | 行 ~1217 | 阶段定义 |
| §15.4 P3 | SnoopFilter + MesiGpuProtocol | 行 ~1219 | **非 Stub Cache**|
| §15.4 P3.5 | JSON 端口路径打通 | 行 ~1220 | 阶段定义 |
| §15.4 P4 | CHI 完整协议支持 | 行 ~1221 | 阶段定义 |
| §15.4 P5 | IOMMU 集成 | 行 ~1222 | 阶段定义 |
| §11.4 | 主 roadmap 表 P3/P3.5/P4/P5/P6 | 行 951+ | **与 §15.4 编号不一致(P6 不在 §15.4)**,以 §15.4 为准(per NUMBER-MAP §2 Decision 3) |

### 3.2 `docs/architecture/06-bis-coherence-as-network-extension.md` v0.3(行号实证 per oracle B3-2 修订)

| 章节号 | 章节标题 | 行号 | 用途 |
|--------|---------|------|------|
| §1.1 | 跨仓分工(ArchForge 设计 + CppTLM 实施)| 行 66-76 | 跨仓治理依据 |
| §2.1 | 跨仓分工契约 | — | ArchForge = 设计真源,CppTLM = 实施责任 |
| §2.2 | 与 ArchForge 的同步契约 | 行 130 | 跨仓同步依据(per S6 补充)|
| §2.3 | **命名空间一致性**(非 D-5)| 行 138-150 | **06-bis 不存在 D-5 Sharer Tracker 接口**(per B5)|
| §3.4 | 选 C: programmatic bridge + 与 active Draft 豁免条款 | 行 202+ | supersede 候选 — 06-bis §3.4 无 Fabric Collective 候选(per N10)|
| §5 | coherence_bundles(SnoopProbe/SnoopRespBundle/CoherenceMsg)| 行 378-540 | 真源 3 沿用;**无 non-cacheable 概念**(per B10)|
| §5.2 | `struct SnoopProbe: public bundle_base` | `06-bis:411` | 真源 3 沿用 |
| §5.3 | SnoopRespBundle 字段定义 | `06-bis:456` | 真源 3 沿用 |
| §5.4 | CoherenceMsg Bundle | `06-bis:475` | 真源 3 沿用 |
| §5.5 | 命名空间 + 命名 + 类型 一致性 | `06-bis:493` | 命名空间一致性 |
| **§7.3** | **P2.0 + P2.1 合并节**(per oracle B3-2 实证:实为 P2 拆分 P2.0/P2.1) | 行 620-630 | 阶段范围定义 |
| **§7.4** | **P3** — SnoopFilter + MesiGpu + translate_snoop | 行 ~634 | 阶段范围定义 |
| **§7.5** | **P3.5** — JSON 端口路径打通 + Python 工厂 | 行 ~638 | 阶段范围定义 |
| **§7.6** | **P4** — CHI 完整协议 | 行 ~645 | 阶段范围定义 |
| **§7.7** | **P5** — IOMMU + 跨 vendor | 行 ~649 | 阶段范围定义 |
| **§7.8** | **v0.3 总 KPI 调整 — 串行激活,并发 ≤ 1** | 行 ~653 | **KPI 串行约束**,非阶段范围(per B2-2) |
| §11.5 | **相关测试**(非 P3-P6 演进路线)| `06-bis:794` | **06-bis 不存在 P3-P6 章节**(per B2-3) |
| §12.1 | v0.3 修订明细 | `06-bis:845` | 修订历史(per N8)|

### 3.3 `docs/architecture/01-hybrid-architecture-v2.1.md` v2.1.10

| 章节号 | 章节标题 | 行号 | 用途 |
|--------|---------|------|------|
| §1.1 | **通信框架层 vs 具体总线层(v2.1.10 新增)** | 行 46 | canonical 6 层(L1-L6)— **per oracle B3-3 实证 — 真源在此,非 06 §1.1** |
| §4.7 | **实现模式枚举(未来扩展, v2.1.10 修订)** | 行 532 | **simulation_mode 真源之一**(per B3-5)|
| §4.5 | Transaction/Debug 架构 | 行 512 | (per S6 补)|

### 3.4 `include/bundles/noc_bundles_tlm.hh`(代码真源,per oracle B2-4/B3-4 实证修订)

| 字段/常量 | 行号 | 用途 |
|----------|------|------|
| `vc_id` 字段(0-3) | `noc_bundles_tlm.hh:38` | **通道隔离正解字段**|
| `flit_type` 字段 | `:39` | "0=HEAD, 1=BODY, 2=TAIL, 3=HEAD_TAIL"(**非 flit_category,per B3-4 实证**)|
| `flit_index` 字段 | `:40` | 分组索引 |
| `flit_category` 注释 | `:45` | "0=REQUEST, 1=RESPONSE" 注释过时,漏 CREDIT |
| `CATEGORY_REQUEST = 0` | `:60-62` | 实证|
| `CATEGORY_RESPONSE = 1` | `:60-62` | 实证 |
| `CATEGORY_CREDIT = 2` | `:60-62` | 实证 |

### 3.5 `include/tlm/cache_tlm.hh:35`(A13 待审)

| 字段 | 用途 |
|------|------|
| 现有 CacheTLM API 入口 | 待 A13 议题(CEP-15)决议 |

### 3.6 `include/bundles/coherence_bundles_tlm.hh`(**文件不存在**,per metis G3)

| 字段 | 用途 |
|------|------|
| `bundles::coherence_bundles_tlm.hh` 路径 | **不存在**;仅 `include/bundles/` 下 17 个其他 bundle 文件 |
| 引用建议 | 改为"待创建"或"per ADR-CEP-03 §5 沿用 + 待 P3 创建" |

### 3.7 `docs/adr/ADR-X.1-transaction-id.md`(真源 3 沿用,per S4)

| 字段 | 用途 |
|------|------|
| `Packet::stream_id` + `TransactionContextExt` | 已有 transaction_id 双层同步方案 |
| 命名空间 | `txn_id`(snake_case per AGENTS.md);`TransactionContextExt` 沿用 |

### 3.8 `scripts/test/docs_sync_check.sh`

| 字段 | 用途 |
|------|------|
| `VIRTUAL_PATHS` 数组 | 跨仓 ArchForge 路径前缀 |
| 拟加 lint | ADR 锚点 lint(per oracle next_steps,本 ADR §4 实施)|

### 3.9 `docs/architecture/07-coherence-exploration-platform.md`(2026-10-20 创建,per `ADR-CEP-00 §5.2` 候选 α)

| 章节号 | 章节标题 | 用途 |
|--------|---------|------|
| §1 | 文档定位与真源关系 | CEP 与 06 L1-L6 / 06-bis 的关系(正交 vs supersede)|
| §2 | 6 层 CEP 架构 | CEP 6 层 + Fabric Collective 旁路平面 |
| §3 | canonical 10 条 Coherence Interface 流 | 流清单 + 完成纪律 + CEN-Ib1~10 |
| §4 | 67 编号位 / 66 条守门 invariant 总表 | 单一真源表(15 组;`INV-CEP-04-IC4` **唯一跳空**)|
| §5 | Bundle 与协议适配 | **2 来源族 / 16 adapter 实例**映射(工业 8 + Ruby 8;§5.1.4 候选语义落地后 20)|
| §6 | 仿真语义与集成 seam | simulation_mode + ModuleFactory 真实注册 |
| §7 | 实验输出协议 | 4 件套 + 13 必纳 metric |
| §8 | 演进路线与阶段边界 | Stage 0~3 + 阶段划分 + supersede 映射 |

---

## 4. 伪引清单(起草时禁止引用,per oracle v0.3 B3-6 + B3-7 修订)

| 伪引标识 | 状态 |
|---------|------|
| "ADR-I01" | 不存在(全仓 grep 仅 CEP ADR 自引用);被 CEP-02 v0.2 修订为"本 ADR 首次定义" — **v0.5 已修:CEP-01:75 改描述性引用** |
| "ADR-G04" | 不存在;被 CEP-04 v0.3 修订为描述性陈述"协议无关性" — **v0.5 已修:CEP-04 §3.2 沿用描述性原则** |
| "ADR-DGPU-13 §2 P1 引入 ProtocolBridge" | ADR-DGPU-13 已 superseded 正文保留,§2 不引用 |
| "06 §15.1 10 条流清单" | §15.1 是 ArchForge 文档清单引用,无 10 条流;被 CEP-02 v0.2 修订 |
| "06-bis §2.3 D-5 Sharer Tracker" | §2.3 是命名空间一致性,非 D-5;被 CEP-04 v0.3 修订 |
| "06-bis §11.5 P3-P6" | §11.5 是相关测试,06-bis 无 P6;被 CEP-02 + CEP-05 v0.3 修订 — **v0.5 已修:CEP-05:8 头部已删除 §11.5 引用** |
| "06 §15.4 P3 Stub Cache 5 接口" | §15.4 P3 是 SnoopFilter,非 Stub Cache;被 CEP-05 v0.3 修订 — **v0.5 已修:CEP-05 §9 line 226 改"沿用 stub 精神"** |
| "06 §4.7 simulation_mode" | 06 §4 仅至 §4.5;**真源在 06 §5.1 + 01-hybrid §4.7**;**v0.5 已修:CEP-07:8/39/220 三处改 06 §5.1 + 01-hybrid §4.7** |
| "06 §15.4 P2-P6" / "P3-P6" | §15.4 实为 P2.0-P5,**无 P6**;**v0.5 已修:NUMBER-MAP + CEP-02 + CEP-04 全部删 P6** |
| "06-bis §7.8 P2-P5" | §7.8 实为"v0.3 总 KPI 调整"(行 ~653),**非阶段范围**;**v0.5 已修:CEP-00 §3 + CEP-04:198/265 + CEP-02:162 全部改 §7.3-§7.7** |
| "ADR-CEP-11" | 未起草(CEP-13 一致性域归属议题预分配为 CEP-13,per NUMBER-MAP §2 Decision 1);**v0.5 已修:CEP-02:127 改 ADR-CEP-13** |
| "ADR-CEP-09" | 未起草(多 VC 仲裁 + TileLink Tree Tip 映射预分配) |
| "07-coherence-exploration-platform.md" | **已创建 2026-10-20**(per `ADR-CEP-00 §5.2` 候选 α 拍板)— 见 §3.9 章节映射;CEP 顶层真源 1,整合 10 份 ADR v0.6 |
| "REGISTER_CHSTREAM(T)" / "REGISTER_OBJECT(T)" | **不存在**(per oracle v0.4 B7 实证 — `REGISTER_CHSTREAM` 无参,`REGISTER_OBJECT` 无参);v0.5 已改 CEP-07 §3 决策 3 真实注册机制 |

---

## 5. 实施与维护

### 5.1 ADR 锚点 lint

`scripts/test/docs_sync_check.sh` 增加 ADR 锚点 lint:

```bash
# 伪代码
for adr in docs/adr/ADR-CEP-*.md; do
  # 提取 ADR 引用的真源文档章节号
  refs=$(grep -oE "(06|06-bis|01-hybrid|ADR-X\.[0-9]+|ADR-DGPU-[0-9]+) §[0-9.]+" "$adr")
  for ref in $refs; do
    doc=$(echo "$ref" | cut -d' ' -f1)
    section=$(echo "$ref" | cut -d' ' -f2)
    # 检查章节是否在本表存在
    if ! grep -q "$section" docs/adr/ADR-CEP-00A-real-source-anchor-map.md; then
      echo "BLOCKER: $adr 引用 $ref 在 ADR-CEP-00A 中无映射"
    fi
  done
done
```

### 5.2 维护原则

- **本表更新触发**:真源文档(06/06-bis/01-hybrid/NB bundles)有章节变动时
- **本表新增触发**:CEP 系列 ADR 新增引用新章节时(起草时同步)
- **本表删除触发**:真源章节删除且无引用时

---

## 6. 关联 ADR

- 基础:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
- 配套:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
- 配套:全部 CEP-01 ~ CEP-07 + 后续 CEP-08 ~ CEP-16(均引用本表)

---

## 7. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 真源章节映射 | 本 ADR 作为 CEP 系列 ADR 引用的单一真源 | ✅ 已拍板 |
| 不变量 | CEP-00A-IC1(机器可查)| ✅ 已拍板 |
| 起草流程 | 真源章节号先用 grep 提取,再写本表,再 cross-ref | ✅ 已拍板 |
| 实施机制 | `scripts/test/docs_sync_check.sh` 增加 ADR 锚点 lint | ✅ 已拍板 |
| 伪引清单 | 10 条已识别,起草时禁止引用 | ✅ 已拍板 |

---

## 8. v0.1 修订记录(本 ADR 初版)

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
