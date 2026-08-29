# v0.5.0-MVP release-gate: validate_topology + CHANGELOG + git tag

> **状态**: 📋 Proposed — 2026-08-31 · **日期**: 2026-08-31 · **Owner**: CppTLM Team (Sisyphus)
> **承接**: 从 `2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/` 迁移 T-s3-4 / T-s3-5
> **关联**: ADR-SOC-06-cpptlm-v05-mvp.md (D5)
> **依赖**: 
> - `2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/` 必须已 archive (T-s3-1/2/3 ✅)
> - `2026-08-26-cpptlm-dgpu-board-soc-split/` 必须已 archive
> - `2026-08-26-cpptlm-dgpu-abi-export/` 必须已 archive
> - UsrLinuxEmu 集成 smoke PASS

---

## Why

T-s3-4 (validate_topology + ≥850 baseline) + T-s3-5 (CHANGELOG + v0.5.0-MVP tag) 是发布动作而非代码动作。将其从 s3 拆出独立 change:
- s3 archive 不被发布动作阻塞 (39/53 即可 archive)
- tag 语义自洽:v0.5.0-MVP tag 在 board-soc-split + abi-export + UsrLinuxEmu 集成全绿后打

## What Changes

### 1. 验证门禁
- `cmake --build build --target validate_topology` PASS
- 全量测试 PASS (当前 Oracle 实测 978/978)
- 无 regression

### 2. 文档
- `CHANGELOG.md` 顶部新增 `## [v0.5.0-MVP] - 2026-XX-XX`
- `docs/soc_arch/modules/README.md` 同步 7 模块
- `scripts/test/docs_sync_check.sh --strict` PASS

### 3. 版本号论证 (per Oracle m3)
v0.5.0-MVP 是 feature branch 命名而非 semver 顺序,与 v3.0.0 dGPU extract (per ADR-X.15) 并列。正式 GA 应为 v0.5.0 (MVP 验证 + bugfix 之后,M11+ 规划)。

### 4. Git tag
- `git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → SQ → CudaCore + PTX-EMU functional/timing split"`

## Acceptance Gate
- [ ] G-RG-1: validate_topology PASS
- [ ] G-RG-2: 全量 ≥978 测试 PASS
- [ ] G-RG-3: CHANGELOG.md 记录 v0.5.0-MVP
- [ ] G-RG-4: docs_sync_check --strict PASS
- [ ] G-RG-5: git tag v0.5.0-MVP 创建