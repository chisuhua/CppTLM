# 人工审查材料 · CEP 架构文档 + 决策文档（architect / planner 段）

- **审查范围**:本会话（`5f744d16` → `08c5d683`/`ec31a0db`）**architect 段 + planner 段**的产出
- **产出者**:architect（决策与文档）+ planner（盘点与路径图）
- **审查者**:用户（人工审查）　**生成日期**:2026-11-08
- ⚠️ **审查前必读**:① 本材料给出**精确行号**（基于 `ec31a0db`）② 两份主文档在 **M1 冻结面**内 ⇒ **append-only** ⇒ 发现要改的内容**只能追加新段或走授权链**，⛔ 不能回改已签发段落 ③ 台账 `obj-cep-platform.md` 的 Entry 段同样 append-only

---

## 〇 · 一页速览（本次会话到底做了什么）

```
36 个提交 · 3 个阶段 · 4 类产出
├ architect 段:12 条决策统一 + 架构文档/决策文档落盘 + 三方复核(oracle→metis→critic)
├ planner 段  :剩余任务盘点 + 任务路径图(双轨 + 3 门 + 4 里程碑)
└ 附带        :架构文档质量审查与评分(48/60→49/60=82%)+ KPI 行去堆积 + CI 红线清理
```

**关键状态**:
- `ctest` **164/164** · 6 个 CEP 闸全绿 · M1 冻结面 **41 产物零 diff** + provenance 归位 · KPI **0/3**
- **`B3b-2` 维持 0/4**（终态 DoD Go 挂起）—— 本会话**未**改变任何计数

---

## 〇-b · 2026-11-08 裁决落地与发现回填（per 台账 **Entry #291**）

- **用户已裁 2 条**（本材料包 §六.4 的三条质疑中）：① **F-05** —— `CohIfResponderTLM` = **真实 home 的简化模型**（承载**最小协议语义**）⇒ 弃用「协议中立替身」措辞；② **F-11** —— **拓扑 B 保留为独立拓扑**（⛔ 不降为 A 的变体）⇒ 代价 = 两套 config ⇒ 须新增**机器可判**的 **J-9 拓扑分档判据**。第三条质疑（**D-9 产物 sha256** = F-12）**仍未裁**。
- **第二轮审查发现清单（14 条）**：`.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md` ⇒ **F-02 / F-03 / F-06 / F-09 / F-10** 已在 planner 域**就地修正**（§10 **路径图修订 ①** + 讨论纪要 **§八 回填** + 本材料包行号与期望值更正）；**F-01 / F-04 / F-05 落地 / F-08 / F-11 落地 / F-12** 属 **architect**（须**追加 SU**，⛔ 不回改已签发段）；**F-07 / U-4 兜底** 属 **builder**。
- ⚠️ **本材料包的行号与 append-only 期望已按实测更正**（§一 第 2 行 + §二 2️⃣ 表 + §五 ④）；**台账 Entry #290 ② 载有同款旧行号**，因 §11 append-only ⇒ ⛔ 不回改，更正登记在 **Entry #291**。
- **后续追加（2026-11-08 第二、三批）**：architect 已落 **Entry #292**（4 项决策面勘误：F-01 状态追认 / F-04 `:71` 理由 / F-05 D-0 定位改写 / F-11 拓扑 B + **J-9**）与 **Entry #293**（两项裁定：**声明面 = 甲** + **F-12** 台账元数据 **7 字段**）⇒ 路径图同步于 **§10 修订 ① / ②**（台账 Entry #291 / #294）⇒ ⇒ **本材料包的 §四 未闭合项表已部分过时**：**U-1**（`:56` MODIFIES）仍未做、**U-3**（三键测量归属）仍待定，但**声明面甲/乙**已由 Entry #293 裁定为**甲**、**D-9 的元数据字段**已由裁定 2 精确化。

---

## 一 · 文件清单（按审查优先级排序）

### P0 · 必审（决策面）

| # | 文件 | 本次改动 | 段 | 行号定位 |
|---|---|---|---|---|
| **1** | `.rddf/roadmap/objectives/obj-cep-platform.md` | **+604 行** | planner | §10 路径图 **L995~1044**；台账 Entry **L15843~16380**（#269~#289 共 21 条） |
| **2** | `docs/architecture/07-coherence-exploration-platform.md` | **+366 行 / −1 行** | architect | **SU·七 L1837** · **SU·八 L1913** · **SU·九 L1975** · **SU·十 L2097** · **SU·十一 L2128** · §末索引 **L2008~2093**(⚠️ **不在文末** —— 其后追加了 SU·十 / SU·十一) |
| **3** | `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md` | **+84 行** | architect | **SU 2026-11-08 L360~441** |
| **4** | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | **+109 行** | architect | **SU 2026-11-07（强度裁决）L801~907** |

### P1 · 宜审（依据与评分）

| # | 文件 | 本次改动 | 段 | 行号定位 |
|---|---|---|---|---|
| **5** | `.rddf/reviews/cep-ualink-e2e-framework-discussion.md` | **+132 行**（新建） | planner/architect | 12 条决策的**逐条讨论依据**（含每轮取证与裁定） |
| **6** | `.rddf/reviews/cep-arch-doc-review-2026-11-08.md` | **+86 行**（新建） | architect | 架构文档**质量评分 49/60=82%** + 5 个扣分项（F-2~F-7） |
| **7** | `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` | **+3 行** | architect | L38（`TileLink-C` 行 L4 档误标修正：wire 档 → direct 档） |

### P2 · 可扫（活文档同步）

| # | 文件 | 本次改动 |
|---|---|---|
| 8 | `.rddf/roadmap/phases/phase-10.md` · `.rddf/roadmap.md` · `.rddf/improvements/cpptlm-cep-umbrella.md` · `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` · `.rddf/improvements/cpptlm-cep-ualink-coherence-engine.md` · `.rddf/improvements/cpptlm-cep-dcoh-nvlink-refkind-migration.md` | KPI baseline 行**去堆积**（6.3~8.8K → 254~343 字符）+ improvement 状态同步 |
| 9 | `.rddf/improvements/cpptlm-cep-b3b2-d4-trigger-probe.md` | 新建（E1 探测协议规格） |
| 10 | `scripts/README.md` | CEP 闸表**全量对齐**（+11 行）+ 「季度复审脚本」新小节 |
| 11 | `scripts/test/cep_doc_ref_check.sh` | glob 豁免 + 行号守卫 + 2 条 `VIRTUAL_PATHS` |
| 12 | `scripts/test/cep_b3b2_d4_probe_check.sh` · `cep_b3b2_d4_probe_validate.py` · `cep_b3b2_d4_quarterly_review.sh` | 新建（探测清单闸 + 校验器 + cadence 脚本） |
| 13 | `test/cep_b3b2_d4_probe.json` · `test/cep_b3b2_d4_probe_runs.jsonl` · `test/test_cep_refkind_whitelist_contract.cc` · `test/test_cep_ualink_engine.cc` | 新建（判据/数据） |
| 14 | `include/tlm/cep/ualink_protocol_engine.hh` · `include/chstream_register.hh` · `configs/cep_ualink_engine_pe_minimal.json` | 新建（引擎 + 注册 + 拓扑） |
| 15 | `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json` | 冻结清单（授权链产物，⛔ 勿手改） |

---

## 二 · 逐份审查要点（每份：**看哪里 / 判什么 / 已知待裁项**）

### 1️⃣ `obj-cep-platform.md` —— 决策台账 + 路径图（planner）

| 看 | 判什么 |
|---|---|
| **§10 的「2026-11-08 任务路径图」L995~1044** | ① 轨 A 的 **10 个任务**出口判据是否**可机器判** ② **3 个门**（G1 授权 / G2 完成者未通不得宣布全绿 / G3 种子未入快照不得作 B4 证据）是否齐 ③ **4 个里程碑**的依赖顺序 ④ 轨 B 是否真**不占** KPI slot（B1/B2 不建 change 目录） |
| **§10 的「2026-11-07 后续任务实施路径」L944~994** | 盘点是否可信：`B3b-2` 内部可推进量 = 0 的**逐族逐路径**论证；遗留 5 项（F-1~F-5）的归属与估时 |
| **台账 Entry #269~#289（L15843~16380）** | ① **Entry #269**（引擎强度 = 甲 + oracle 4 落点 + 张力消解）② **#275**（批 1）③ **#277**（E1 协议 + metis 9 条处置）④ **#278**（清 backlog）⑤ **#279**（闸修复 + **我的验证遗漏**登记）⑥ **#280~#288**（4 条裁决 + 12 条决策 + oracle/metis/critic 三方复核）⑦ **#289**（路径图） |
| **§5.2 KPI baseline（L470）** | 去堆积后是否仍能定位历史（⇒ §11 台账） |

⚠️ **重点核对**：**Entry #279 §④** 诚实登记了我的验证遗漏；**Entry #286 §①** 登记了取证方法的假阳性 ⇒ 这两处是自审质量的证据。

### 2️⃣ `07 号` SU·七~十一（architect）—— 5 个连续追加段

| 段 | 行 | 判什么 | 已知待裁/待办 |
|---|---|---|---|
| **SU·七** | 1837 | 「未装配」= **既有显式延期**（⛔ 不是缺口/偏离）的论证；`ADR-CEP-19` 决策 9 的引证 | 4 条待裁决点（已在 SU·八 闭合） |
| **SU·八** | 1913 | 4 条裁决的**可执行文本**；`:71` MODIFIES 的**撤销**（勘误）；**实施前不得宣称已落地**的纪律 | 实施期另立 Entry 授权 `:56` |
| **SU·九** | 1975 | 桥路径**目录更正**（`include/tlm/cep/` 非 `include/framework/`）+ 红线落点**两条并存** | — |
| **SU·十** | 2097 | §5.1.1 状态列「✅」的**维度**澄清；「弱档」使用边界 | — |
| **SU·十一** | 2128 | **12 条决策登记表** + ①-b「经网络」定义钉死 + ①-c 桥挂载结构 + ② 实施前不得宣称 + ③ 计数禁令句 + ④ 冻结面清单与**两链分离** + 拓扑准入判据 | 3 处**实施期**待办（seed setter / 三键测量归属 / `:56` MODIFIES） |
| **§末索引** | 2008~2093 | 41 一级节 + 13 SU + 5 关键表指针（脚本生成） | ⚠️ **已过期且不在文末**（其后追加了 SU·十 / SU·十一 ⇒ 实为 43 节 / 15 SU）⇒ 增删章节后须重生成 |

⚠️ **重点核对**：SU·十一 **①-b**（若「经网络」定义不严 ⇒ 验收假绿）与 **④ 两链分离**（spec MODIFIES **不在**本轮授权内）。

### 3️⃣ `ADR-CEP-19` SU 2026-11-08（architect）

| 看 | 判什么 |
|---|---|
| **① 决策 9 四条理由逐条核对** | ① 仍成立⇒改显式 MODIFIES / ② 部分解除 / ③ stub 纯类**刻意保全** / ④ 已闭合 —— 逐条是否站得住 |
| **② 实施形态** | 「组合式 wrapper」是否真的**保全**了理由 ③ |
| **③ MODIFIES 预告** | **`:56` 单条**（`:71` 撤销的理由：其 WHEN 限定命中路径） |
| **④ / ④-a / ④-b** | 两拓扑 + 完成者角色 + **拓扑准入判据**（用户逐字）+ 「经网络」定义 |
| **⑤ 计数纪律** | 禁令句是否**逐字**准确 |

### 4️⃣ `ADR-CEP-12` SU 2026-11-07（architect）—— 引擎强度裁决

① 强度裁决（记账级专用承载）② **字面禁令清单**（不得使用/只许使用两栏 + 2 条触发式禁令）③ 三分法观测面 + 4 条不变量 ④ completion 不依赖有序性（含 order-coupling mutant 的鉴别力与**边界诚实登记**）⑤ `snoop_resp` 四句不对称。

⚠️ **重点核对**：④ 的「鉴别力边界」—— 文档**主动声明**了哪些 mutant 抓不到 ⇒ 这是否可接受，还是需要更强载体？

### 5️⃣ 讨论纪要（依据面）

12 条决策的**逐条讨论依据** + 每轮取证 + oracle/metis/critic 的复核痕迹（含 3 处被纠正的事实错误）。

### 6️⃣ 评分报告

架构文档质量 **49/60=82%**（D1 9 · D2 9 · D3 9 · D4 9 · **D5 5** · D6 8）+ 5 个扣分项（F-2~F-7）+ **取证方法假阳性**的自更正记录。

---

## 三 · 决策索引（12 条 → 落点行号）

| 决策 | 内容摘要 | 落点 |
|---|---|---|
| D-0 | 完成者角色 `CohIfResponderTLM`（独立 ChStream 派生 / ⛔ 非 ProtocolEngineBase / 对称 CohIf 端口 / **进 JSON 拓扑**） | `07 号` SU·十一 ① 表 D-0 行；`ADR-CEP-19` ④-a |
| D-1 | 可观测面：latency 测**发起端(wrapper)**；wrapper **只暴露原始事实**，聚合由 harness | `07 号` SU·十一 ① 表；纪要 §二 |
| D-2 | 双向映射：**命中也回 CPU 侧响应**；响应侧归 wrapper；⚠️「承载层回 NoD 响应 ≠ 协议引擎发射」 | `07 号` SU·十一 ① 表；`ADR-CEP-19` ② |
| D-3 | 公开观测 API；不适用 ⇒ `not-collected` + **显式登记降格**；第一批 = **CHI RN-F** | `07 号` SU·十一 ① 表 |
| D-4 | 不新建 Sink（后置 `ab_compare` baseline）+ wrapper **单一模块双角色** | `07 号` SU·十一 ① 表 |
| D-5 | **两拓扑并存**：A 直连=全绿载体；B=互联可达+桥投影，⛔ 不承载全绿 | `07 号` SU·十一 ① 表 + ①-b/①-c |
| D-6 | 冻结 spec **`:56` 单条** MODIFIES（`:71` 撤销；`:63` 不动） | `07 号` SU·十一 ① 表 + ④；`ADR-CEP-19` ③ |
| D-7 | `SEQUENTIAL` + `RANDOM`（固定种子入 config 快照）；⛔ `TRACE` | `07 号` SU·十一 ① 表 |
| D-8 | 判据 J-1~J-8 | `07 号` SU·十一 ① 表；纪要 §二 |
| D-9 | 产物规范（入台账的是**元数据**；`preregistration.json` 不动） | `07 号` SU·十一 ① 表 |
| D-10 | 计数纪律禁令句 | `07 号` SU·十一 ③；`ADR-CEP-19` ⑤ |
| **D-11** | **「经网络」定义钉死**（= ChStream 连接延迟；桥 = 装配期投影登记器；⛔ 「事务穿过桥」是假绿） | `07 号` SU·十一 ①-b；`ADR-CEP-19` ④-b |
| **拓扑准入判据** | 「JSON 拓扑代表真实物理存在的；只为测试而创建的元素不进 JSON 拓扑；否则要进」 | `07 号` SU·十一 ④；`ADR-CEP-19` ④-a |
| **桥挂载结构** | B-1 + 语义降级（一个 entry 三端点 = 同一 CohIf 投影语境；⛔ 桥不产连通性） | `07 号` SU·十一 ①-c；`ADR-CEP-19` ④-a |

---

## 四 · 已知未闭合项（人工审查时请重点判断是否可接受）

| # | 未闭合项 | 归属 | 现状 | 是否阻塞实施 |
|---|---|---|---|---|
| **U-1** | `cep-stub-cache-core` 的 **`:56` MODIFIES** | builder（实施期） | **未做** ⇒ 需**另立台账 Entry** + 两步授权（Entry #288 **不预授权**） | ⚠️ **是**（G1 门） |
| **U-2** | `TrafficGenTLM` 的 **seed / 地址段 / mode setter**（现 `rng_(42)` 硬编码） | builder | 未做 ⇒ 「种子入 config 快照」**今无落点** | ⚠️ **是**（G3 门，影响可复现性） |
| **U-3** | **三键测量归属**（`coherence_stalls` / `collective_stalls` / `directory_misses` 的来源与「真实 0」口径） | builder | 未定 ⇒ 若填 0 须**有观测依据**（⛔ 否则落入 `ADR-CEP-06` SU#1 ③ 的「实测 0 与未测量同形」自欺面） | ⚠️ **是**（影响 J-4） |
| **U-4** | **Step 7.5 重挂检测**（同一模块属多个 entry ⇒ 失败） | builder | **独立 backlog**（B-1 形态天然规避） | ⛔ 否 |
| **U-5** | **`ADR-CEP-12` 边界句**：wrapper 观测面**不是**能力映射 basis 分档的输入 | architect | critic NIT，**可后置** | ⛔ 否 |
| **U-6** | **F-3** 17 份 ADR「起草中」逐批转签发 | architect | 轨 B·B1 ⇒ ⛔ 不可批量 | ⛔ 否 |
| **U-7** | **F-2** 14/22 ADR 缺 `Verification` | builder | 轨 B·B2（带 deferral 登记的闸） | ⛔ 否 |
| **U-8** | **`07 号` §5.1.1 状态列 / `ADR-CEP-19` 的实施后状态更新** | architect | ⛔ **实施 + verifier 验收后**才做（SU·十一 ②） | ⛔ 否 |

---

## 五 · 可复现的核验命令

```bash
# ① 冻结面 provenance（两份 SU 落定后的 hash 链）
bash scripts/test/cep_freeze_check.sh                    # 期望:41 产物 hash 全一致 + provenance 自检绿
git log --oneline -- docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json | head -5

# ② 逐闸核验
for g in cep_docs_sync_check cep_doc_ref_check cep_decl_alignment_check \
         cep_archforge_alignment_check cep_b3b2_deferral_check cep_b3b2_d4_probe_check; do
  printf "%-34s " "$g"; bash scripts/test/$g.sh >/dev/null 2>&1 && echo "rc=0 ✅" || echo "rc≠0 ❌"
done

# ③ 全量回归
ctest --test-dir build -j4                              # 期望 164/164

# ④ 逐字确认「既有段未回改」(append-only 核验)
git diff 5f744d16..HEAD -- docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md | head -40
git diff 5f744d16..HEAD -- docs/architecture/07-coherence-exploration-platform.md | grep '^-' | grep -v '^---' | head -20
#   ⇒ 期望(实测口径,2026-11-08 更正):`ADR-CEP-19` **零删除行**(纯追加);
#     `07 号` **恰好 1 行删除** = §5.1.1 的 **UALink 行**,系 **Entry #269 授权的就地改写**
#     (⛔ 不是「本会话自己写的行」)⇒ 该文件因此**不是严格 append-only**,核验时勿按「只有新增」判

# ⑤ 计数纪律核验(B3b-2 未被改动)
git diff 5f744d16..HEAD -- test/cep_b3b2_deferrals.json | wc -l   # 期望 0
grep -n 'B3b-2' .rddf/roadmap/objectives/obj-cep-platform.md | grep '维持 0/4' | head -3

# ⑥ 决策与文档的交叉一致性(人工抽查用)
grep -n 'D-0\|D-11\|拓扑准入判据\|B-1 + 显式语义降级' docs/architecture/07-coherence-exploration-platform.md | head -8
```

---

## 六 · 审查建议（按投入产出排序）

1. **先读 §三 决策索引**（12 条 + 2 条附加）⇒ 建立全局认知（~10 min）
2. **再读 `07 号` SU·十一**（L2122~2159）⇒ 这是**决策登记的单一真源**（~15 min）
3. **然后读 `ADR-CEP-19` SU**（L360~441）⇒ 决策的**边界与授权面**（~15 min）
4. **重点挑 3 条质疑**：
   - D-11 的「经网络」定义是否**真的**够严（⇒ 「拓扑 B」是否还值得独立存在？）
   - D-0 的完成者放进**契约面**是否合适（⇒ 它代表 home 的「替身」，会不会被读成「CEP 有 home 模型」？）
   - D-9 的「入台账的是元数据」是否够（⇒ 产物不入库 ⇒ 实验**不可复算** ⇒ 是否该把 `metrics.json` 也登记 sha256？）
5. **再查 §四 的 8 个未闭合项**，逐个判断「是否可接受 / 是否应升级为阻塞」
6. **最后扫 §一 P2 清单**（活文档同步 + 闸脚本 + 判据）⇒ 这批是**执行层**，重点看**闸的判据是否真的能红**（如 `cep_b3b2_d4_probe_check.sh` 的 5 个变异）

---

**关联**:台账 Entry **#288**（12 条决策 + 授权）· **#289**（任务路径图）· **#287**（下一步任务推荐）· **#285**（架构文档审查）· **#282**（4 条 architect 裁决）· `.rddf/reviews/cep-ualink-e2e-framework-discussion.md`（讨论纪要）· `.rddf/reviews/cep-arch-doc-review-2026-11-08.md`（评分报告）
