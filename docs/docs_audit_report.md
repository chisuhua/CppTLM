# CppTLM 文档审计报告 (Snapshot)

> **生成时间**: 2026-06-14
> **工具**: `scripts/test/docs_sync_check.sh`
> **状态**: ✅ 365/365 路径引用有效

## 检查范围

| 文档 | 路径 |
|------|------|
| 项目入口 | `AGENTS.md` |
| 新人指南 | `docs/ONBOARDING.md` |
| 路线图 | `roadmap.md` |
| 脚本索引 | `scripts/README.md` |

## 路径引用统计

| 指标 | 值 |
|------|---|
| 文档数 | 4 |
| 扫描路径引用总数 | 365 |
| 缺失路径数 | **0** |
| 误报（VIRTUAL_PATHS 排除） | 27（已删除/归档/规划中文件 + gem5 参考路径） |

## VIRTUAL_PATHS（已删除/归档文件，仅在文档中说明删除原因时引用）

| 路径 | 状态 | 引用上下文 |
|------|------|-----------|
| `src/noc/` 等 | 设计示意 | v2.1 实际使用 `src/tlm/` 和 `src/rtl/` |
| `cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp` | v2.1 删除 | USE_SYSTEMC 选项移除 |
| `cpu_cluster.cc` | 归档到 samples-orphaned | v1 legacy 模块 |
| `ext/packet_to_payload.hh` / `ext/payload_to_packet.hh` | 归档到 docs-archived | packet_to_payload 桥接路径修正 |
| `modules_v2.hh` | 归档 | v2 模块定义被 TLM 模块取代 |

## scripts/ 子目录一致性

- ✅ `scripts/CMakeLists.txt` 存在
- ✅ 所有 5 子目录（build/, test/, pipeline/, topology/, stats/）均含 `.gitkeep`

## 历史快照对比

| 日期 | 路径引用 | 缺失 | 备注 |
|------|----------|------|------|
| 2026-06-10（清理前） | 367 | 182 | 大量误报（路径前缀补全缺失） |
| 2026-06-10（智能路径补全） | 354 | 76 | 误报减少 58% |
| 2026-06-10（修复 ONBOARDING 过期引用） | 354 | 13 | 仅剩余"说明删除原因"引用 |
| 2026-06-10（VIRTUAL_PATHS 完善） | 354 | 0 | 零误报 |
| 2026-06-11（Phase 7 GPU APU 规划） | **361** | **0** | 新增 Phase 7 路径白名单（13 规划中文件 + 3 gem5 参考）；调研报告 [`research-cpptlm-gpu-fused-soc-survey.md`](research-cpptlm-gpu-fused-soc-survey.md) 入库 |
| 2026-06-14（Phase 7.A 文档收尾） | **365** | **0** | CHANGELOG.md 新增 `[Unreleased]` 段记录 7.A 8 项落地；roadmap.md 7.A 行 Pending → ✅；VIRTUAL_PATHS 移除已落地的 `compute_bundles_tlm.hh`/`gpu_tlm.hh`/`gpu_standalone.json`（30 → 27） |

## 维护流程

每次 `src/include/scripts` 结构变更时，开发者必须：

1. **本地运行** `./scripts/test/docs_sync_check.sh --strict`，确保零误报
2. **更新文档** AGENTS.md / ONBOARDING.md / roadmap.md / scripts/README.md
3. **提交前** pre-commit hook 自动运行此检查（如已启用 `pre-commit install`）
4. **CI 兜底**（未来可扩展） `.github/workflows/ci.yml` 中可加 docs-sync-check job

## 扩展点（未来工作）

- [ ] 添加更多文档到扫描列表（如 `docs/architecture/01-*.md`）
- [ ] 检查 Markdown 链接（`[text](path.md)` 形式）
- [ ] 添加 include/AGENTS.md 路径验证（注册宏引用一致性）
- [ ] 添加 .clang-format 与代码风格一致性检查
