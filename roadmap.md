# CppTLM Roadmap

> **Version**: 2.0
> **Last Updated**: 2026-06-10
> **Status**: ✅ v2.1 Released (2026-06-08)

## 已完成里程碑 (v2.0 → v2.1)

### v2.1.0 — 2026-06-08
**架构**: 分层融合 v2.1 — CacheTLM / CrossbarTLM / MemoryTLM 端到端全链路验证  
**Build**: TLM stub 默认启用，移除 USE_SYSTEMC 依赖  
**Extension**: tlm_stub 多扩展支持（tlm_extension_registry + tlm_array + release_extension API）  
**Codebase**: 21 项 cleanup（备份/孤儿头/openspec 残留/根目录过期 notes 已清理）

---

## Phase 4: Hierarchy Core（已完成 4/6）

| 子任务 | 状态 | 实现 |
|--------|:----:|------|
| 4.1 Hierarchy Tree Parser | ✅ | `include/core/topology_node.hh`, `src/core/topology_parser.cc` |
| 4.2 CoherenceDomain C++ Module | ✅ | `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc` |
| 4.3 Domain Boundary Validation | ✅ | `src/core/module_factory_validate.cc:445 validate_domain_boundary()` |
| 4.4 Snoop Routing Logic | ✅ | `CoherenceDomain::get_snoop_targets()` + snoop fanout |
| 4.5 Directory Protocol Stub | ⏳ | `CoherenceDomain::lookup_home_node()` 已实现 address mapping；独立 `Directory` 类未拆分（暂不必要） |
| 4.6 Python Hierarchy Generator | ✅ | `cpptlm/topo/layer.py` 含 `HierarchicalTopologyGenerator` |

**评估**: 4.5 的核心功能已被 `CoherenceDomain::lookup_home_node()` 覆盖，独立 `Directory` 类不是阻塞项。

---

## Phase 5: Protocol Bridge (待启动)

- 5.1 ProtocolBridge C++ module
- 5.2 Address translation engine
- 5.3 Protocol conversion logic
- 5.4 Cross-protocol validation
- 5.5 Python bridge config generator

---

## Phase 6: Multi-Cluster SoC Validation (待启动)

- 6.1 2x CPU Cluster + GPU Cluster config
- 6.2 Cross-cluster coherence validation
- 6.3 Protocol bridge integration test
- 6.4 Full SoC example (4 CPU + 1 GPU)

---

## 当前活跃工作（2026-06-10）

### docs/cleanup boulder（已完成归档）

详细归档清单见 [`docs-archived/omo-archived/INDEX.md`](docs-archived/omo-archived/INDEX.md)：
- 18 个已完成的 boulder plans
- 4 个 notepad 子目录（tlm-stub-multi-extension-and-cleanup、dashboard-implementation、p0-p1-architecture-debt-fix-v2、phase-0-tag）
- 第一批 + 第二批共 28 个文件

### Python 包双轨制（已形式化）

- **状态**: 双包共存架构（2026-06-10 决策）
- **结构**:
  - `cpptlm_config/` （PyPI 名 `cpptlm-config`，715 行）— 核心依赖：ConfigBuilder、TopologyValidator、TopologyAdapter
  - `cpptlm/` （PyPI 名 `cpptlm`）— 用户友好 facade：cli、simulation、topo、visualization
- **依赖关系**: `pyproject.toml` (cpptlm) 显式声明 `cpptlm-config>=0.1.0` 作为依赖
- **运行时**: 通过 `sys.path.insert(0, ...)` 引用 monorepo 内 `cpptlm_config/` 目录

### 文档维护机制（已建立）

- `scripts/test/docs_sync_check.sh` — 验证 4 个核心文档中路径引用
- `.pre-commit-config.yaml` — 提交前自动运行 `--strict` 模式
- `docs/docs_audit_report.md` — 当前快照（354/354 路径有效）
- AGENTS.md §"文档维护原则" — 手动规范

### 待办（未来）

- [ ] Phase 5 / 6 启动时同步 roadmap.md v3.0
- [ ] 考虑扩展 docs_sync_check.sh 扫描范围（Markdown 链接、include/AGENTS.md 一致性）

---

## Metadata

- **Architecture Reference**: `docs/architecture/01-hybrid-architecture-v2.1.md`
- **Implementation Plan**: `docs/implementation/11-tgms-v4-implementation-plan.md`
- **Archived Roadmap (v1.0)**: `docs-archived/omo-archived/plans/` 下的 `cpptlm-cleanup.md` 与 `architecture-debt-cleanup.md`
- **Project Overview**: [`AGENTS.md`](AGENTS.md)
