# Archived CppTLM Framework ADRs

> **创建日期**: 2026-08-19
> **范围**: 本目录存放**已被替代 / 反转 / 撤销**的 CppTLM 框架层 ADR(`docs/adr/` 的归档区)
> **关系**: 活跃 ADR 位于 `docs/adr/`;SoC 应用层 ADR 位于 `docs/soc_arch/adr/`

---

## 归档清单

| ADR | 议题 | 归档原因 | 替代者 |
|-----|------|---------|--------|
| [ADR-X.15-cpptlm-v3-dgpu-extract.md](./ADR-X.15-cpptlm-v3-dgpu-extract.md) | cpptlm-v3-dgpu-extract(角色反转 + v3.0.0 BREAKING bump + 11 项删除清单) | 2026-08-19 v0.5 redo 反转(per Status Update 块);11 项删除清单全部撤销 | [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) |
| [ADR-X.16-cpptlm-v05-redo.md](./ADR-X.16-cpptlm-v05-redo.md) | cpptlm-v05-redo(Net-new 取代 v3.0-extract,PTX-EMU submodule + adapter + per-warp precision,8 项决策) | 2026-08-19 MVP 切片(12 周 → 4 阶段 6-10 周);8 项决策保留为 MVP 决策依据 | [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) |

---

## 归档原则

1. **不可变原则**:归档 ADR 的正文内容**保持不变**(per AGENTS.md "ADR 不可变"硬规则)
2. **历史决策追溯**:即使被替代,归档 ADR 是重要的决策史 + Status Update 审计追溯
3. **恢复方法**:如需复活归档 ADR:
   ```bash
   git mv docs-archived/adr/ADR-X.N-*.md docs/adr/
   ```
   并在 `docs/adr/README.md` 索引表追加条目 + 更新 `## Status Update` 段

---

## 状态说明

| 状态 | 含义 |
|------|------|
| 🟢 Accepted | 已批准或已实施(位于 `docs/adr/`) |
| 📋 Proposed | 起草中(位于 `docs/adr/`) |
| 🚫 Archived | 已被替代 / 反转 / 撤销(位于本目录) |

---

## 维护

- 新增归档: `git mv docs/adr/ADR-X.N-*.md docs-archived/adr/` + 在本 README 登记
- 复活归档: 同上反向 + 更新 `docs/adr/README.md` 索引表

---

**维护**: CppTLM Team · **最后更新**: 2026-08-19