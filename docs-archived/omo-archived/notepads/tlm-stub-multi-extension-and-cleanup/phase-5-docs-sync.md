# Phase 5: 一致性审查 + ADR-X.13 + CHANGELOG + 文档同步

**Date**: 2026-06-06
**Status**: ✅ Complete
**Scope**: docs/adr/, CHANGELOG.md, 5+ AGENTS.md, README.md audit

## 1. 完成内容

### 1.1 ADR-X.13 创建

`docs/adr/ADR-X.13-stub-multi-extension.md` (新文件, ~80 行):
- 状态: ✅ 已实施
- 章节: 背景 / 决策 / 实施 / 兼容性 / 参考
- 5 commits 引用 (833da21, c9e9340, adfcc5b, a15e2fc, 93f4a81)
- Accellera SystemC 官方链接 + 行号参考

### 1.2 ADR-X.5 更新

`docs/adr/ADR-X.5-build-system.md`:
- L5 状态: `📋 待确认` → `✅ 已实施`
- Q2/Q3 决策: "可选" → "USE_SYSTEMC_STUB stub 实现" / "C) stub 桩实现"
- L629 CI 矩阵: `use-systemc: [ON, OFF]` → `# stub 路径唯一（USE_SYSTEMC option 已删除）`
- L558-580 build.sh: 删除 USE_SYSTEMC 环境变量 + `-DUSE_SYSTEMC` 配置行
- L784: 删除 `-DUSE_SYSTEMC=ON` 构建命令示例
- L767 决策汇总: "可选启用（USE_SYSTEMC 选项）" → "USE_SYSTEMC option 已删除，仅 stub 路径（USE_SYSTEMC_STUB 桩）"
- 保留对 `USE_SYSTEMC_STUB` 的描述

### 1.3 ADR-X.6 更新

`docs/adr/ADR-X.6-transaction-integration.md:234`:
- 删除 "长期可能需要改进" 注释
- 改写为引用 Phase 1c 修复（commit adfcc5b + 93f4a81）

### 1.4 CHANGELOG.md 重写

`CHANGELOG.md` (8 → 53 行):
- Added: tlm_stub multi-extension, registry, array, release_extension, deep_copy_from, test_tlm_multi_extension.cc, ADR-X.13, CI code-format job
- Changed: BREAKING tlm_extension<T>::ID 迁移, TransactionContextExt 显式 release, ~tlm_generic_payload 循环清理, cmd_exts.hh 简化
- Removed: BREAKING USE_SYSTEMC option, external/systemc/, src/sc_main.cpp, packet_to_payload/payload_to_packet, mock_modules.hh 重复 nesting, 16 文件 #ifdef blocks
- Fixed: MemoryV2 error path 不再静默销毁, Type ID 跨 TU 一致性
- v2.1.0 (2026-06-05) baseline tag

### 1.5 AGENTS.md 更新 (5 文件)

- `AGENTS.md` (root): L96 use-systemc matrix → "仅 stub 路径（USE_SYSTEMC option 已删除）"
- `include/tlm/AGENTS.md`: tlm_stub.hh 描述（94 → 283 行, 多 extension API 列表）
- `include/AGENTS.md`: 注意事项新增 ext/ 多 extension 并存条目
- `include/core/AGENTS.md`: core/ext/ 段说明 Phase 1c 后状态（packet_to_payload/payload_to_packet 已移除）
- `include/ext/AGENTS.md`: 约定段新增 4 个 multi-ext bullet (registry / set 返回旧指针 / release / get 非空处理)

### 1.6 README.md (项目根)

**不存在** (per plan: 不创建)。`docs/README.md` 和 `test/README.md` 存在但不在 plan 范围。

## 2. 一致性审计结果

### 2.1 活跃构建/CI 路径 (0 命中 - clean)

`CMakeLists.txt`, `*.cmake`, `.github/**/*.yml`, `scripts/*.sh`, `scripts/*.py`: 0 hits

### 2.2 其他命中分类

| 类别 | 文件 | 原因 |
|------|------|------|
| 故意文档 | `CHANGELOG.md` (L27, L29) | 记录 USE_SYSTEMC 删除 |
| 故意文档 | `AGENTS.md` (L96) | "USE_SYSTEMC option 已删除" |
| 故意文档 | `docs/adr/ADR-X.5-build-system.md` (L358-447, L629, L694, L767) | 历史 CMake 示例 + 我的更新 |
| 合法 stub | `USE_SYSTEMC_STUB` (CHANGELOG L33-35, AGENTS.md L213, include/tlm/AGENTS.md L41, docs/ONBOARDING.md L24, docs/skills/SKILLS_SUMMARY.md L140, docs/requirements/PRD-001/002) | stub 宏是当前路径 |
| 历史计划 | `docs/superpowers/plans/*`, `plans/phase7-completion-plan.md`, `plans/archive/tgms-handoff.md` | 删除前计划 (超出范围) |
| 历史文档 | `findings.md`, `reports/MIGRATION_GUIDE.md`, `docs/adr/ADR-X-SUMMARY.md` | v2.0 文档 (不重写非 X.2/X.5/X.6/X.13 ADR) |

### 2.3 计划期望 vs 现实

Plan 期望 0 命中, 在 Phase 5 范围不可达:
- 4-6 月的删除前历史计划记录了 `-DUSE_SYSTEMC=OFF` 旧状态
- ADR-X-SUMMARY.md 聚合所有 X-系列 ADR (修改会牵涉 X.1, X.3, X.4, X.7, X.8 — 超出范围)
- ADR-X.5 中的 CMake 示例代码块是 v2.0 设计的历史参考

关键检查 (活跃 build/CI/CMake 路径) 已 clean。后续 phase 可处理历史计划归档。

## 3. 验证

- Build: `ninja: no work to do` (仅文档变更; 构建已为最新)
- Tests: `test cases: 581 | 579 passed | 2 failed` — **579/581** 匹配 plan 期望
- 2 失败: 已知 NIC 注册测试 (test_phase0_stats_registration.cc:133), 预先存在 0 回归

## 4. Commit

单一原子 commit (type `docs:` per plan template).
