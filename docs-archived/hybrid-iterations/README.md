# hybrid-iterations — 归档的 Hybrid TLM+CppHDL 混合仿真设计文档

本目录包含从 `docs/architecture/examples/hybrid/` 归档的早期版本设计文档,记录 Hybrid TLM+CppHDL 混合仿真架构的迭代过程。

## 归档原因

`docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v{N}.md` 是项目早期的设计文档,每个版本代表一次 Oracle 评审后的修订。最终版本 v4 完整重写并通过 FragmentMapper 验证 (17/17 测试),实现了 TransactionContextExt 作为真值源的关键设计决策。

由于 v1/v2/v3 已被 v4 完全取代,继续保留在主文档目录会造成:
1. 读者困惑(不知道该看哪个版本)
2. 文档搜索噪音(v3 838 行 / v2 722 行 / v1 1204 行 / v4 1029 行)
3. 维护负担(每次评审都要解释"看 v4,不要看 v3")

## 归档时间

| 版本 | 原始位置 | 归档时间 |
|------|----------|----------|
| v1 | `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design.md` | 2026-06-07 |
| v2 | `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v2.md` | 2026-06-07 |
| v3 | `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v3.md` | 2026-06-09 |

## 归档清单

| 文件 | 大小 | 状态 | 关键决策 |
|------|------|------|----------|
| `v1-hybrid_tlm_cppHDL_design.md` | 1204 行 (49059 字节) | 🗄️ 已废止 | Oracle Round 1 评审发现 10 项 CRITICAL API 误用 |
| `v2-hybrid_tlm_cppHDL_design.md` | 722 行 (28302 字节) | 🗄️ 已废止 | Oracle Round 2 评审发现 1C+2H+3M+1L=7 项未完全修复 |
| `v3-hybrid_tlm_cppHDL_design.md` | 838 行 (30282 字节) | 🗄️ 已废止 (D-Path 验证失败) | 17 项修正完成但遗漏 TransactionContextExt,被 v4 完整重写取代 |

## 当前活跃版本

**[`docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md`](../../architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md)** (1029 行)

关键特性:
- ✅ FragmentMapper 验证 (17/17 测试通过, 593/593 全部回归)
- ✅ TransactionContextExt 作为 tid 真值源
- ✅ 复用 `ChStreamAdapterFactory::registerAdapter` 而非自建
- ✅ Phase 7 RTL Spike 设计冻结

## 状态

- **保留方式**: `git mv` (保留完整 git 历史)
- **不维护**: 归档版本不再接受修订,所有引用应指向 v4
- **不删除**: 保留以供历史参考、决策追溯、潜在类似设计的参考
- **可恢复**: 若需重新激活某版本,`git mv` 即可

## 恢复方法

```bash
# 从归档恢复 v3 (示例)
git mv docs-archived/hybrid-iterations/v3-hybrid_tlm_cppHDL_design.md \
       docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v3.md
```

## 引用关系(本归档前)

| 引用方 | 引用目标 |
|--------|----------|
| `docs/adr/ADR-X.8-fragment-handling.md` | `hybrid_tlm_cppHDL_design_v4.md §3` (v4 only) |
| `docs/architecture/01-hybrid-architecture-v2.1.md §4.5` | `hybrid_tlm_cppHDL_design_v4.md` (v4 only) |
| `hybrid_tlm_cppHDL_design_v4.md §0.1` | v1/v2/v3 演进史 (自引用) |

✅ **归档后无悬空引用**: 所有外部文档只引用 v4,不再引用 v1/v2/v3 的原路径。
