# 验收报告 · `cpptlm-cep-refkind-carrier-whitelist-contract`

- **验收对象**:change `cpptlm-cep-refkind-carrier-whitelist-contract`(doc-only 契约对齐)
- **角色**:verifier(2026-11-07)
- **判据来源**:change `proposal.md` **AC-1 ~ AC-5** + delta spec 的 **RKC-04**
- **结论**:**PASS**(0 失败项 · 1 轮)
- **分流说明**:`.rddf/state/iteration.json` **不存在** ⇒ 未触发 `rdd-verifier` 的 P3 archive gate;`proposal.md` 的 AC 段为 `## Acceptance`(⛔ 非 `## Acceptance Criteria`)⇒ 走**本地 1–5 步验收**

## 一、AC 逐条判定

| AC | 判定 | 证据 |
|---|---|---|
| **AC-1** 契约面含「唯一白名单」子段 + RKC-04 | ✅ | `openspec/specs/cep-refkind-coverage/spec.md` 实测含「载体自足性的**唯一白名单**(准入条件与登记)」子段 + **RKC-04** Scenario;**3 条准入条件**(纯 `constexpr` / 只依赖标准库 / 不引入)+「单一真源」+「同步」义务齐 ⇒ `test/test_cep_refkind_whitelist_contract.cc` **WLC-01** PASS |
| **AC-2** 白名单**唯一**(多一个必红) | ✅ | **WLC-02**:两处探针的**白名单条目集合**(C++ 字面量中的转义形式)各 **== 1** 且含 `source_registry.hh`;**WLC-04 负控**:构造「多一个成员」的探针文本 ⇒ 集合 == 2 ⇒ 唯一性判据**必红**(判据有牙) |
| **AC-3** **三处一致** | ✅ | **WLC-03**:契约面文本 + `ualink_inc` 探针(UIS-10)+ `nvlink_sharp` 探针(CP-05)指向**同一**路径;且契约面自身**只登记 1 个**成员(`contract_cep_paths().size() == 1`)⇒ 「唯一」在契约面侧也成立 |
| **AC-4** 0 回归 + 冻结面零 diff | ✅ | `ctest --test-dir build -j4` ⇒ **160/160**;`[cep]` **477 cases / 11116 assertions**;4 闸全绿;`cep_freeze_check.sh` 无参复验 ⇒ 41 产物 hash 全部一致(⚠️ `cep-refkind-coverage` **不在** M1 冻结集 ⇒ 本 change **不触发** `--update --authorized-by` ⇒ 冻结清单**未**改动) |
| **AC-5** 台账 + 归档 + KPI 回填 | ✅ | Entry #272(实施)· #273(归档);delta **MODIFIED** 合并进 main spec(实测含白名单子段 + RKC-04);归档件强制入库;KPI **1/3 → 0/3**(5 处活文档);`openspec list --json` = **0 active** |

## 二、⚠️ 实施期的 2 处**判据实现**修正(非判据退化)

| # | 问题 | 修正 |
|---|---|---|
| 1 | 扫描口径过粗:初稿扫**普通**形式 `"tlm/cep/*.hh"` ⇒ **误报**探针自身的 `#include` 与待检载体路径(`ualink_inc_state_spec.hh` / `collective_plane.hh` / `nvlink_sharp_state_spec.hh`) | 改为**两侧口径分开**:探针侧扫**转义**形式(`\"tlm/cep/...\"` = C++ 字面量中的白名单条目)+ 契约面侧扫**普通**形式(markdown 文本) |
| 2 | 契约面取值错误:初稿取「第一个**存在**的候选」⇒ **归档前** main spec 仍存在但**尚未合并**该登记 ⇒ 读到旧内容而红 | 改为取「第一个**含白名单登记**的候选」(main spec / delta / 归档后 delta 三候选)⇒ 归档前后皆绿;若三处皆无 ⇒ **FAIL** ⇒ 断言**不弱化** |

⇒ 归档前后断言数 27 → 25(归档后 delta 候选消失 ⇒ helper 落到 main spec)⇒ **断言等价**。

## 三、遗留清扫复核(本次范围)

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK | **0 命中** |
| 调试残留(`printf` / `cerr` / `cout` / `fopen`) | **0 命中** |
| 被 skip 的测试(`SKIP(` / `#if 0`) | **0 命中** |
| 注释掉的代码行 / 临时 fallback | **0 命中** |
| `clang-format` | 本次新增 `test/test_cep_refkind_whitelist_contract.cc` **已格式化**(0 replacements) |

⚠️ **范围外遗留**:仓内仍有 **13 个历史遗留**文件需格式化(含上一轮 Entry #266 新增的 `test_cep_refkind_coverage.cc`)⇒ 已登记为**独立清理项**(per 台账 Entry #271 §6),⛔ 不在本 change 范围。

## 四、失败项与路由

**无失败项** ⇒ ⛔ 无需回 builder / planner。

## 五、已用轮次

第 **1** 轮(2026-11-07)⇒ **PASS**。
