---
name: cpptlm-cep-b3b2-d4-trigger-probe
priority: P2
phase: phase-10
依赖:
  - ADR-CEP-12
  - ADR-CEP-04
状态: ✅ **已实施**(2026-11-09,per 台账 Entry #309 —— 探测协议 + 闸 per Entry #277;**首次真实执行** 2026-11-09:8/8 槽位全覆盖,**4 条 external + 4 条 internal 全部 `miss`** ⇒ **D-4 未对任何族触发** ⇒ 受限族 T1-T4 维持未达成 ⇒ **`B3b-2` 维持 0/4**;cadence 闸由 `CADENCE-NEVER-RUN` 转 `CADENCE-OK`。历史:🟡 **提案**(2026-11-07,per 台账 Entry #274 §④ E1 —— **已按 metis gap 分析改写**,待实施))
created: 2026-11-07
---

# improvement: cpptlm-cep-b3b2-d4-trigger-probe

**主题**: Coherence Exploration Platform(CEP)—— `B3b-2` 受限族的 **D-4 触发探测协议**

> ⚠️ **本文件已于 2026-11-07 按 metis gap 分析(1 轮)改写** —— 4 处 P0 缺陷 + 1 处建模错误已修(逐条对照见 §「metis 处置对照」)。**metis 轮次已用尽** ⇒ 改写后**无独立复核** ⇒ 残余风险已登记(per 台账 Entry #277)。

## 架构依据

- `07 号` **SU 2026-11-06 · 四** **D-4**(升级路径,⛔ 不自动)—— ① **规范可读化** ② **人类成员条款摘要** ③ **来源类型机制**(✅ 已落地)④ **`CellKind::Free`**(✅ 已落地);
- 台账 **Entry #274** §④ 登记周期项 **E1** = 「D-4 ① / ② 的**触发探测**」⇒ ⚠️ **原先未定义探测协议** ⇒ 季度复审会**凭记忆检索** ⇒ **漏检** + **误判**双重风险;
- ⚠️ **`07 号` D-2 的双轴约束**(本条的核心硬约束):**触发轴**(T1–T4,**合取**)∧ **交付轴**(运行时引擎 + L4 E2E)⇒ **两轴齐备**才可改判 `B3b-2` 计数。

## 范围

**In-Scope**:

1. **探测清单**(机器可查 JSON):以 **source event** 为枚举单位(CXL 2.0 / 3.0 **共享**同一联盟许可事件 ⇒ 挂 `family_effect[]`,⛔ 不重复检索);每个 (族 × 路径) 槽位 **≥1 条**探测项;
2. **两类探测的形态区分**(⚠️ metis P0-2 的框架修正):
   - **① 外部检索类**(`check_type: external_metadata`):只访问 **元数据位置**(公告页 / 下载索引 / 版本列表 / 许可页 / 成员门户);
   - **② 仓内产物类**(`check_type: internal_artifact`):⛔ **不是**检索 ⇒ 判「指定落盘路径下是否出现带签署 + 逐格定位的摘录文件」⇒ **闸直接扫文件**;
3. **判定分层**(⚠️ metis G2-6):**「可读化命中」⛔ ≠ 「T1 命中」** —— ① 只判**许可状态与文档类型**;「够不够 T1」由**重判**决定 ⇒ 两个判据**必须分开**;
4. **合规边界**(有牙形态,⛔ 非自证声明):**「投喂正文」在 schema 里没有合法落点**(key 白名单)+ **4 个决定性变异**证伪;
5. **命中后的动作链**(⚠️ metis P0-1 的双轴修正):`命中 ⇒ 重判 T1–T4(触发轴)⇒ 若触发轴全过 ⇒ 立项(引擎 + L4 E2E,交付轴)⇒ **两轴齐备后**才可改判 B3b-2 计数 ⇒ 再评终态 Go`;⚠️ **触发轴过但交付轴未过 ⇒ 计数仍 0/4,只改 `reason`**。

**Out-of-Scope(明确不做)**:

- ⛔ **不做**探测执行本身(外部检索在季度窗口执行;本条只固化**协议**);
- ⛔ **不改** B3b-2 计数(**仍 0/4**)⇒ 本条**不改**任何 DoD 结论;
- ⛔ **不下载 / 不投喂 / 不引用**任何规范正文(含 Evaluation Copy)⇒ 探测只取**元数据**;
- ⛔ **不**为 4 族补 T1–T4 载体(无依据 ⇒ 推理填空);
- ⛔ **不建 OpenSpec change / 不新增 main spec**(⚠️ metis P2-9 的瘦身建议)⇒ 产物 = `.rddf` 文档 + JSON + 闸 ⇒ **不占 KPI slot**。

## Why

`B3b-2` 的解除**只可能由外部事件触发**(per Entry #274 §①)⇒ 本仓**内部可推进量 = 0** ⇒ 唯一的自主动作是「**不漏检 + 不误判**」。而当前 E1 只有一句话 ⇒ 季度复审时「搜什么、搜到算什么」全凭当场判断 ⇒ ① 可能**漏**(没搜到该搜的地方 / ② 类根本不是搜的东西)② 可能**误判**(把白皮书当规范 / 把 Evaluation Copy 当可读化)③ 可能**投喂越界**(检索工具自动返回正文片段,Entry #265 **已实发**)④ 可能**误改计数**(跳过交付轴)⇒ 四种失败模式都会**污染**已签发的 DoD 结论。

## What Changes

| 变更 | 文件 |
|---|---|
| 探测清单(机器可查) | `test/cep_b3b2_d4_probe.json`(新) |
| 校验闸(结构 + 覆盖 + 合规 + 4 决定性变异) | `scripts/test/cep_b3b2_d4_probe_check.sh`(新) |
| **闸注册**(⚠️ metis P0-4:不注册的闸 = 不存在) | `test/CMakeLists.txt`(+ `add_test`)· `scripts/README.md`(闸表登记) |
| 协议正文 | 本文件 + `.rddf/roadmap/objectives/obj-cep-platform.md` 台账 Entry #277 |

## Acceptance

- [ ] **AC-1（覆盖谓词,非条数）**:JSON 的 `probes[]` 满足 —— **4 族 × 2 路径 = 8 个槽位**逐格 **≥1 条**探测项;每条含 `must_all[]`(**≥2 个**子条件);① 类每条 `search_sources[]` **≥2 个且 `page_type` 互异**(防单页迁移漏检);⚠️ **CXL 2.0 / 3.0 共享同一 `source_event_id`** ⇒ 闸须报 `coverage: 8/8 slots` 且 **不重复检索**
- [ ] **AC-2（无合法落点 + 决定性变异,⛔ 非自证声明）**:probe JSON 与 run-record 的 **key 白名单**中**不存在** `body` / `snippet` / `excerpt` / `content` / `text`(未知 key 直接红);`metadata_source.url` 不得匹配 `\.pdf(\?|$)` / `/sites/default/files/` / `/wp-content/uploads/`;`page_type ∈ {official_announcement, download_index, version_list, license_page, member_portal}` ⇒ **4 个决定性变异**(加 `body_excerpt` 字段 / 把 source 换成 `.pdf` URL / 删一格覆盖 / 把判据改成主观措辞)分别按**固定签名**变红
- [ ] **AC-3（判定语法化 + 可判定）**:每条判据展开为 `{field, op ∈ {eq,ne,in,regex,gte}, value}`;**banned token** = `视情况|酌情|确认可读|适当|必要时|等|相关|已确认`;检索目标不得出现**结论词**(只允许事实词)⇒ **「以为命中其实没命中」**的 6 类反例逐条被排除(白皮书 / Evaluation Copy / 二手报道 / 自动附带正文 / 两级转述 / 「有 § 编号就算条款级」)
- [ ] **AC-4（防漂移 + 不改计数）**:每条 probe 带 `deferral_trigger_sha256` = 该族 `trigger_condition` 字符串的 sha256 ⇒ 闸比对 `test/cep_b3b2_deferrals.json`,**不一致即红**(⛔ 不修改 deferrals 文件);⚠️ 新增文件中不得出现 `1/4|2/4|3/4|4/4` 形态的 B3b-2 计数;`obj §6.2` 的 B3b-2 行 sha256 不变
- [ ] **AC-5（具名 + 注册）**:闸**必须**在 `test/CMakeLists.txt` 加 `add_test` 且在 `scripts/README.md` 闸表登记 ⇒ **ctest 实测能跑到**;冻结面项按 `cep_freeze_check.sh` 的**实际清单**具名(新增文件不在冻结集 ⇒ 该项改为「冻结清单 sha256 未变」)
- [ ] **AC-6（三态 + 零 run-record 区分）**:探测结果为 **三态** `hit | miss | unknown` ⇒ ⛔ **站点宕机 / 登录墙 / 重定向到 PDF / 仅 archive 快照** ⇒ `unknown`(**不得**写成 `miss`,否则漏检被合法化);⚠️ **零 run-record** 与「全部 miss」必须可区分(前者 = 漏检)⇒ 闸须能报「**从未执行过**」
- [ ] **AC-7（0 回归）**:`ctest` 全绿 + 4 闸 + 冻结面清单未变 + `test/cep_b3b2_deferrals.json` sha256 未变

## Capabilities

- **不漏检**:季度复审有**可执行清单**而非记忆;漏检可定位到具体 `probe_id` / 槽位;
- **不误判**:命中判据**语法化 + 可观测**⇒ 「搜到了」与「算数」分离;
- **不越界**:协议**在 schema 层无正文落点** + 4 变异证伪 ⇒ 不是「声明不投喂」;
- **不改计数**:双轴约束写进协议 ⇒ 触发轴过 ≠ 计数可改。

## Impact

- **公共面**:新增 1 份 JSON + 1 个闸脚本 + 2 处注册(测试 / 脚本表);
- **冻结面**:0 改动(新增文件不在 M1 冻结集 ⇒ 冻结清单 sha256 不变);
- **KPI**:**不占 slot**(不建 OpenSpec change);
- **⚠️ 不改 B3b 计数**:B3b-2 仍 **0/4**;终态 DoD Go 维持**挂起**。

## metis 处置对照(2026-11-07 · 1 轮 · 逐条)

| metis 结论 | 本文件的处置 |
|---|---|
| **P0-1** 动作链违反 D-2 双轴 | ✅ §「In-Scope」5 改为**双轴**链 + 「触发轴过但交付轴未过 ⇒ 计数仍 0/4,只改 reason」 |
| **P0-2** ②「人类摘要」不是外部检索 | ✅ §「In-Scope」2 区分 `check_type: external_metadata` / `internal_artifact`;② = **仓内产物检查**(闸直接扫文件) |
| **P0-3** AC-2 是自证声明字段 | ✅ **AC-2** 换为「**key 白名单无合法落点** + URL pattern + `page_type` 枚举 + **4 个决定性变异**」 |
| **P0-4** 漏 `test/CMakeLists.txt` / `scripts/README.md` 注册 | ✅ **「What Changes」** 显式列出 2 处注册 + **AC-5** 要求 ctest 实测跑到 |
| **G1** 枚举单位错(应 source event)+ 合取须拆 | ✅ **In-Scope 1** 以 source event 为单位 + `family_effect[]` + **AC-1** 的 `must_all[] ≥2` |
| **G2** 命中判据 6 类反例 | ✅ **AC-3** 语法化 + banned token + 6 类反例逐条排除 |
| **G3** 「只取元数据」需可判 | ✅ **AC-2** 的三层结构(key 白名单 / URL pattern / page_type 枚举)+ **诚实声明**(闸保证**协议不被用作正文通道**,⛔ **不保证**执行者未在其他渠道接触正文) |
| **G4** 与既有 deferral 闸的关系 | ✅ **保持分离** + **单向引用**(probe → deferrals,带 `deferral_trigger_sha256`)+ 闸的族名单**从 deferrals JSON 读**,⛔ **不硬编码** |
| **G5** AI slop(AC-2/AC-3/AC-5 装饰性) | ✅ **AC-2/3/5** 均改为可机器判;**AC-1** 由「8 条齐全」改为「**覆盖谓词**」 |
| **P2-9** 降低 spec 重量 | ✅ **不建 OpenSpec change / 不新增 main spec** ⇒ 不占 KPI slot |

## ⚠️ 残余风险(metis 轮次已用尽 ⇒ 改写后无独立复核)

1. **「检索工具自动返回正文」是协议无法阻止的物理事实**(Entry #265 已实发)⇒ 只能靠 run-record 的 `incident: auto_body_contact` 登记 + 季度人工审计 ⇒ ⛔ **不得**写成「闸守住了只取元数据」;
2. **cadence 断言不进 ctest** ⇒ 「≤100 天未执行即红」若放 ctest ⇒ 100 天后**所有无关 PR 都红** ⇒ 放季度复审脚本;
3. **本条改写未经第二轮 metis**(轮次已用尽)⇒ 改写内容 = metis 建议的**直接落地**,但**无独立复核** ⇒ 登记为残余风险。
