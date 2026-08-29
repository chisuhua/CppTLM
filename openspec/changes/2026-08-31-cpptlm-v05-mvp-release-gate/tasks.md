# v0.5.0-MVP release-gate: Tasks

> **承接**: s3 T-s3-4 + T-s3-5 从 2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/ 迁移

## W1 (2026-09-XX)
### T-RG-1: validate_topology + baseline
- [x] `cmake --build build --target validate_topology` PASS
- [x] `build/bin/cpptlm_tests` 全 PASS (130/131, 1 pre-existing SIGSEGV deferred 非阻塞)
- [x] 无 regression

### T-RG-2: CHANGELOG + 版本论证
- [x] `CHANGELOG.md` 顶部 `## [v0.5.0-MVP] - 2026-XX-XX` 新增
- [x] 子章节:`### 新增 (Features)` / `### 修复 (Fixes)` / `### 测试 (Tests)`
- [x] 每个 bullet 用 `**Component::method**` 格式
- [x] MVP 标注:在标题后注释 "MVP slice — pre-release, 不保证 GA" (per Oracle m3)

### T-RG-3: docs 同步
- [x] `docs/soc_arch/modules/README.md` 同步 7 模块 (CP/PM4/TMU/SQ/CQ/CudaCore/PtxEmu)
- [x] `scripts/test/docs_sync_check.sh --strict` PASS

### T-RG-4: git tag
- [x] `git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: ..."`
- [x] tag annotation 引用本 proposal.md

## Acceptance Checklist
- [x] T-RG-1 ~ T-RG-4 完成
- [x] board-soc-split archive (T-bs-1 已落地 5e47446)
- [x] abi-export archive
- [ ] UsrLinuxEmu 集成 smoke PASS