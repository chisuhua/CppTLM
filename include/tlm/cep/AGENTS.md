# include/tlm/cep/ — CEP 平台实现层

**状态**: v1（2026-11-19 建档）· **最后更新**: 2026-11-19
**域**: Coherence Exploration Platform 的 **header-only** 实现（50 个 `.hh` + 9 个 JSON/schema 声明源）

## 职责

跨协议一致性探索平台的实现面。**只放头文件**（.cc 在本目录不存在，测试在 `test/test_cep_*.cc`）。
平台总纲与分层见 `docs/architecture/07-coherence-exploration-platform.md`；决策见 `docs/adr/ADR-CEP-*.md`。

## 文件索引

### 引擎骨架（协议中立，⛔ 不得出现任何协议名）

| 文件 | 职责 |
|---|---|
| `protocol_engine_base.hh` / `protocol_family.hh` | 引擎基类 + 协议族枚举 |
| `collective_plane.hh` / `collective_plane_contract.hh` / `fabric_collective_plane.hh` | 集合通信面与契约 |
| `sharer_tracker.hh` / `ordering_point_checker.hh` | 共享者跟踪 + 定序点检查 |
| `stub_cache_core.hh` / `address_resolver.hh` | 桩缓存核 + 地址解析 |
| `channel_dependency_graph.hh` | 通道依赖图 |
| `engine_capabilities.hh` | 协议引擎承载能力声明面（G1 · 现状登记） |
| `hop_timing_capabilities.hh` | 协议语义层的 hop / 时序建模声明面（G10 · 现状登记） |

### 协议适配（每协议一组：engine / adapter / state_spec）

`chi_rnf_*` · `cxl_*` + `cxl_dcoh_*` · `mesif_*` · `moesi_gpu_*` · `nvlink_*` + `nvlink_sharp_state_spec.hh` · `ualink_*` + `ualink_inc_state_spec.hh` · `cohif_*` · `tilelink_*`

### 装配与闸

`instance_assembly_gate.hh` / `instance_assembly_loader.hh` / `adapter_contracts_loader.hh` / `adapter_schema_contracts.hh` / `bridge_module_membership.hh` / `cep_mesh_interconnect.hh`

### 声明源（JSON + 同 schema 锁）

`fabric.json` / `collective_planes.json` / `perm_upgrades.json` / `protocol_instances.json` —— 每个都有同名 `.schema.json`。

## 本目录铁律

1. **header-only**：新增实现一律 .hh；⛔ 不要在此加 .cc（构建显式列源，加了也不会被编译）。
2. **协议中立**：通用层（集合通信 / 跟踪器 / 地址解析 / 依赖图）**不得**出现具体协议名；协议差异只允许出现在 `*_adapter.hh` / `*_state_spec.hh` / `*_protocol_engine.hh` 三类文件里。
3. **JSON 与 schema 成对**：新增声明源必须同批提交同名 `.schema.json`，否则装配闸抓不到。
4. **契约闸先跑**：改任何契约面（引擎接口 / 声明源 / schema）后必跑 `scripts/test/cep_*.sh`（数量以 `ls scripts/test/cep_*.sh | wc -l` 为准）。
5. **章节锚点**：引用 CEP 章节前必查 `docs/adr/ADR-CEP-00A-real-source-anchor-map.md`，禁凭记忆写节号。
6. **冻结面**：`docs/adr/ADR-CEP-*.md`、`docs/architecture/07-*.md`、`docs/architecture/cep/*.md`、`openspec/specs/cep-*` 均在 M1 冻结清单内 —— 改动走**五步授权链**（`docs/guide/cep-usage.md` §12.6），⛔ 不要直接改。

## 常见坑

- **别在通用层加协议分支**：`collective_plane.hh` 里出现 `CHI` / `CXL` 字样即破坏协议中立性（ADR-CEP-03 的判据）。
- **新增协议 ≠ 新增一份引擎**：先看 `protocol_family.hh` 的枚举与 base 契约，能复用就不新建。
- **CEP 测试标签**：`[cep]`；单跑用 `./build/bin/cpptlm_tests "[cep]"`。

## 关联

`docs/architecture/07-coherence-exploration-platform.md`（总纲）· `docs/architecture/cep/`（9 篇子文档）· `docs/adr/ADR-CEP-00-platform-charter-and-governance.md`（治理宪章）· `scripts/test/cep_*.sh`（契约闸）· `test/test_cep_*.cc`
