# 架构文档对齐订正 —— 审稿报告(architect 域)

- **日期**:2026-11-14
- **角色**:architect(只读核对 + 就地措辞订正)
- **产物**:`docs/architecture/07-coherence-exploration-platform.md` · `docs/architecture/cep/protocol-engine.md` · `docs/architecture/cep/directory-ordering.md` · `docs/architecture/cep/simulation-and-experiment.md`
- **审查路径**:**改措辞 ⇒ 只 oracle**(per build.md 审查配对 architect 行;非新建 ADR / 非反转既有决策 ⇒ 无需 critic)
- **oracle 判定**:`判定: APPROVE`(3 条 actionable 已逐条处置,见 §3)
- **性质**:**零代码 / 零 spec / 零 ADR 改动**;新增 invariant 数 = **0**

---

## 1. 起因与范围

只读核对(architect 域)发现真源 1 架构文档与代码/声明源之间存在 **10 处「文档声称 ↔ 实况」漂移**,其中最关键者:`07 号` 与 `protocol-engine.md` 把 **「16 个 adapter 实例」**写在「实现数 / 当前口径」的位置,可被读成「已实现 16 个」;而实况 `include/tlm/cep/protocol_instances.json` 只有 **8 个 `status=implemented` 实例 / 8 个 adapter**,Ruby-SLICC 族 8 实例**落地 0**。

**范围边界**:
- 只改 `arch_paths` 命中的路径(`.rddf/config/arch-docs.json` 第 1 态:`docs/adr` / `docs/architecture` / `docs/designs` / `docs/guide/cep-usage.md`)
- **不动任何已签批 ADR** —— 5 份 ADR 引用「16 adapter 实例」(`ADR-CEP-00` / `00A` / `12` / `17` / `18`);ADR 不可变(只追加 SU)⇒ 口径以其自身理由句为准(`ADR-CEP-12` 已写明「16 是**下界**」)
- **不动台账** —— `.rddf/roadmap/objectives/obj-cep-platform.md` 唯一写入方是 rdd-planner ⇒ architect 只以 `backlog:` 指针承担可追性

## 2. 实测证据(改动前)

| 核对项 | 实测 | 证据 |
|---|---|---|
| 实例数 | **8 个实例**,全部 `status=implemented`;`family` = `industrial` 7 + `academic` 1(`tilelink_c`) | `include/tlm/cep/protocol_instances.json:8-529` |
| adapter 头 | **8 个**(`chi_rnf` / `cxl_cache` / `cxl_dcoh` / `mesif` / `moesi_gpu` / `nvlink` / `tilelink_c` / `ualink`) | `include/tlm/cep/*_adapter.hh` |
| 协议引擎 | **5 个真实引擎** + 1 个 Null 外壳;**CXL 2.0 / CXL 3.0 对称 / NVLink 无引擎** | `chi_rnf_protocol_engine.hh:281` · `mesif_protocol_engine.hh:133` · `moesi_gpu_protocol_engine.hh:60` · `tilelink_protocol_engine.hh:82` · `ualink_protocol_engine.hh:71` · `cohif_protocol_engine.hh:33` |
| SharerTracker | `kTrackerTraits` **6 行**;caps 四属性与文档 5 行表逐键一致 | `include/tlm/cep/sharer_tracker.hh:238-260` |
| Ruby 族落地 | 零 adapter / 零实例声明 / 零 main spec / 零 change;代码侧仅枚举常量 | `include/tlm/cep/protocol_family.hh:45` · `protocol_instance.schema.json` 的 `family` 枚举 |
| Ruby 族台账态 | `ADR-CEP-17` 的 6 条 Verification = **3 可执行 / 1 半有牙 / 2 文档层待建**;§4.0 的 14 个 change **无 Ruby 行** | `obj-cep-platform.md:17608` · `obj-cep-platform.md:176` |
| metric 字段 | schema 真源字段 = **`_not_collected`**(per-metric 原因表) | `experimental/metrics.schema.json:220-227` |
| 死路径 | `src/tlm/cep/` **不存在**(CEP 为 header-only) | `ls src/tlm/` |

## 3. oracle finding 的逐条处置

| # | oracle finding(影响) | 处置 | 状态 |
|:--:|---|---|:--:|
| 1 | 「10 处」完备性声明过宽 —— 活体残余 2 处(`docs/architecture/06-inter-module-network.md:1309` · `docs/research/cep-coherence-protocol-space-survey.md:336`)+ 冻结面残余 4 处(`06-bis` + `ADR-CEP-00`/`16`/`17`)(**高**) | 07 号 SU ① 表**追加第 11 行「残余登记」**:活体 2 处登记 `backlog: cep-sharer-tracker-5-residual-2026-11-14`;冻结面 4 处按不可变纪律**仅登记不追改**;**并把 SU 标题改为「10 处漂移的措辞澄清 + 6 处残余登记」** | ✅ 已处置 |
| 2 | 「⇒ 族标签以声明源为准」的「为准」字面造成二义(标题「6 族 / 8 实例」是**锁定字面量**,不能改)(**中**) | `protocol-engine.md` §5.1.1 注① 末句改为「**「6 族」与 `family` 字段是两个正交维度**(来源族 vs 血统分类),**互不构成反驳**;引用「族」时须注明所指维度」 | ✅ 已处置 |
| 3 | 五步链顺序风险:SU 引 `backlog:` 而非 `Entry #N`;`--update` 的 `--authorized-by` 必填且台账唯一写入方是 planner(**中**) | 07 号 SU 关联行后**追加「冻结面授权链的时序」注**:冻结面报红为预期态、`--update` 待 planner 落 Entry、在此之前**冻结清单 provenance 未闭合**、索引 B 台账列暂显 `—` 的理由 | ✅ 已处置 |
| 4 | 固化「8」的腐化风险 —— 建议点名变红闸门(**低/可选**) | §5.1.1 注② 补「计数机检 = `test/test_cep_moesi_gpu_adapter.cc` 的 `REQUIRE(implemented == 8)` ⇒ 第 9 个实例落地即红,腐化不可能静默」 | ✅ 已处置 |
| 5 | §2.2 是否加状态列(**低/可选**) | **不加** —— 07 号头部自声明「本文不承载实施状态」(状态唯一真源 = 台账 §4.0-a)⇒ 加列反违反单一真源;现状由 SU 读法边界第 5 条承担 | ✅ 已处置(不改) |
| 6 | 「6 层」标题 vs §2.2 表 7 行口径差(**低/可选**) | 本批**不扩 scope** —— §2.1 分层图已明示 Collective 为「横向并列,非堆叠」;登记为 cosmetic backlog | ✅ 已处置(不改) |
| 7 | 07 号:138 行尾多一个 `|`(cosmetic) | 已删 | ✅ 已处置 |

**oracle 未能执行、但已由调用方实测补齐的 3 项**:
- `./build/bin/cpptlm_tests '[cep]'` → **527 用例 / 11797 断言全通过**(含 `test_cep_schema.cc` 的 5 处字面量锁 + §5.1.1/§5.1.2 行数锁 8/8)
- `bash scripts/test/cep_archforge_alignment_check.sh` → **rc=0**,33 锚点全有效(跨仓锚点 `protocol-engine.md:16` 字面量逐字保留)
- `bash scripts/test/cep_index_position_check.sh` → **rc=0**(一级节 35/35/35 · SU 22/22/22)

## 4. 约束守住情况

| 约束 | 结果 |
|---|---|
| 跨仓机器锚点 `protocol-engine.md:16` 的字面量 `16 个 adapter 实例` | ✅ 逐字保留(且仍在第 16 行) |
| `test_cep_schema.cc` 的 5 处字面量锁 + 8/8 行数锁 | ✅ 全保留 ⇒ **零测试改动**;§5.1.1 / §5.1.2 区内**未新增以 `\|` 开头的行** |
| `07 号` 的 `07 号:N` 活锚点(实测 ≤ 690)与 `path:line` 锚点(`:639` / `:696` / `:582-600`) | ✅ **零位移**(新增 SU 插在 `## §末 文档索引` 之前) |
| M1 冻结面 | ⚠️ 4 个被改文件**全在 86 产物内** ⇒ `cep_freeze_check.sh` **报红为预期态**,待五步链第 3 步 `--update` 闭合 |
| ADR 不可变 | ✅ **零 ADR 改动**;5 份引用「16 adapter」的 ADR 未动 |

## 5. 复验结果(2026-11-14 实测)

| 项 | 结果 |
|---|---|
| `./build/bin/cpptlm_tests '[cep]'` | ✅ **527 用例 / 11797 断言全通过** |
| `cep_doc_ref_check.sh` | ✅ rc=0 |
| `cep_archforge_alignment_check.sh` | ✅ rc=0(33 锚点) |
| `cep_index_position_check.sh` | ✅ rc=0(35/35/35 · 22/22/22) |
| `cep_decl_alignment_check.sh` | ✅ rc=0(7 项子校验) |
| `cep_adr_structural_check.sh` | ✅ rc=0(4 项) |
| `cep_validate_topology_reconcile_check.sh` | ✅ rc=0(4 config) |
| `cep_gate_selftest.sh` | ✅ rc=0(34 项) |
| `cep_freeze_check.sh` | ⛔ **rc=1 —— 预期态**(4 个冻结产物被改,待 `--update --authorized-by`) |
| `cep_docs_sync_check.sh --strict` | ⛔ **rc=1 —— 同上**(其第 1 项子校验即 freeze_check) |

## 6. 遗留(交办)

| # | 项 | 载体 / 阻塞类型 | 交办 |
|:--:|---|---|---|
| L-1 | **台账 Entry 落盘** —— `obj-cep-platform.md` §11 追加一条(授权记录) | **唯一写入方 = rdd-planner** | **planner** |
| L-2 | **五步授权链第 2~5 步** —— 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单**单独** commit → 无参复验 rc=0 | builder | **builder** |
| L-3 | **活体残余 2 处**(`06-inter-module-network.md:1309` · `research/cep-coherence-protocol-space-survey.md:336`)的「Sharer Tracker 5 实现」 | `backlog: cep-sharer-tracker-5-residual-2026-11-14` | 后续批 |
| L-4 | **冻结面残余 4 处**(`06-bis` + `ADR-CEP-00` / `16` / `17`) | ADR / superseded 正文**不可变** ⇒ 仅登记 | 各自 owner 追加 SU |
| L-5 | **§2.2「6 层」标题 vs 7 行表**口径差 | cosmetic | 后续批 |
| L-6 | **§2.2 表无状态列** —— Core·TrafficGen 与 Fabric&Transport Plane 两层无 CEP 自有运行时载体 | 已由 SU 读法边界第 5 条承担;**加列反违反状态单一真源** ⇒ 不做 | — |

## 7. 关联

- `docs/architecture/07-coherence-exploration-platform.md`(SU:2026-11-14 架构文档对齐订正)
- `docs/architecture/cep/{protocol-engine,directory-ordering,simulation-and-experiment}.md`
- `ADR-CEP-12`(「16 = 下界」理由句)· `ADR-CEP-17`(Ruby 族可行性判定)· `ADR-CEP-04 §13`(R-1 裁决)
- `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(M1 冻结面 86 产物)
- `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`(`CEP_ALIGNMENT_ANCHORS` 块)
- 台账 `backlog: cep-architecture-doc-alignment-2026-11-14` · `backlog: cep-sharer-tracker-5-residual-2026-11-14`
