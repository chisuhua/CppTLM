# 验收报告 · `cpptlm-cep-instance-assembly-contract-alignment`

- **验收对象**:change `cpptlm-cep-instance-assembly-contract-alignment`(闭合 critic 复核的 1 BLOCKER + 2 SHOULD_FIX + 3 NIT)
- **角色**:verifier(2026-11-07)
- **判据来源**:change `proposal.md` AC-1 ~ AC-6 + delta spec(MODIFIED `cep-instance-assembly`)
- **结论**:**PASS**(0 失败项 · 0 SHOULD_FIX · 1 轮)

## 一、AC 逐条判定

| AC | 判定 | 证据 |
|---|---|---|
| AC-1 spec ③ 与 IAG-01/02 与实现逐条一致 | ✅ | delta spec(Requirement ③ + 两条 Scenario)与 `instance_assembly_gate.hh:172-194` 逐条对齐;`grep required_plane_instances` 在 delta 中命中 |
| AC-2 `ADR-CEP-12` 追加 SU 记录修正 | ✅ | 末尾 SU(Entry #261)含 ① 修正内容 / ② 修正理由(假阴性)/ ③ required 权威归属 / ④ 契约面指向;**前段 SU(Entry #258)未回改** |
| AC-3 coverage rubric 两档 + MOESI-GPU 行标注 + TileLink 行未回改 | ✅ | rubric 增 `L4-direct` / `L4-wire` 两行 + 说明段;MOESI-GPU 行 = ◐(direct ✅ / wire ❌);TileLink-C 行**未**改(backlog) |
| AC-4 完备性负控 +3 按 tag 变红 + MGE-01b 真跑 instantiateAll | ✅ | `[cep][assembly]` 负控覆盖 `family` 缺失→`[IAG-03]` · `modules` 空→`[IAG-06]` · `bridge_id` 缺失→`[IAG-07]`;MGE-01b `ModuleFactory::instantiateAll(cfg)` **真跑通过**(独立探针实测 `instantiateAll=true`) |
| AC-5 0 回归 + 闸族 + 冻结面 | ✅ | `ctest --test-dir build -j4` ⇒ **156/156**;`docs_sync` / `doc_ref` / `decl_alignment` 全绿;`cep_freeze_check` ⇒ **41 产物零 diff** + provenance 通过(两步授权 per Entry #261) |
| AC-6 台账 + 归档 + KPI 回填 | ✅ | 归档为 ✅ 已完成归档语义(delta MODIFIED 合并进 `openspec/specs/cep-instance-assembly/spec.md`,实测 **~ 1 modified**);main spec 的 Requirement ③ 与 IAG-01/02 已含「null/空 = 不启用 collective(合法)」+「required 清单的权威」;KPI **1/3 → 0/3**(5 处活文档),`openspec list --json` = **0 active**;Entry #262 已落 |

## 二、审查

- critic 补复核 **REJECT**(1 BLOCKER + 2 SHOULD_FIX + 3 NIT)⇒ 本 change 闭合;
- critic **复审** ⇒ **APPROVE · mergeable: yes · scope_exceeded: false**(0 BLOCKER;2 NIT:append-only 内生的旧表述先后顺序、proposal 日期 —— 日期已修)。

## 三、失败项与路由

**无失败项** ⇒ ⛔ 无需回 builder / planner。

## 四、已用轮次

第 **1** 轮(2026-11-07)⇒ **PASS**。
