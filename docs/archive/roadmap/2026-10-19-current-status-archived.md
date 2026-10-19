# CppTLM 实时状态看板

> **🚧 历史快照 — 已停止实时维护 🚧**
> **原始最后更新**: 2026-07-03 · **原始基线**: ✅ 764/764 (15547 assertions)
>
> ⚠️ **本文档于 2027-02-09 由 Oracle 评审判定 FAIL 并标记为历史快照**。原内容描述 2026-07-03 时点项目状态,7 个月未更新。当前权威实施状态请参阅:
> - [`docs/soc_arch/architecture/00-overview.md`](../soc_arch/architecture/00-overview.md) — v1.0 总架构蓝图
> - [`docs/roadmap/README.md`](./README.md) — 本目录历史快照(已标注失效)
> - [`openspec/changes/`](../../../openspec/changes/) — 11 个活跃 changes + archive
> - **当前实测基线**: 1241 cases / 47634 assertions / 2 known fail (Phase 8 e2e `_config` + `_bar` Minor, per AGENTS.md)

---

## 🎯 当前下一步任务(2027-02-09 视角)

| 优先级 | 任务 | 工期 | 入口条件 | 阻塞项 |
|:---:|------|:---:|------|:---:|
| 🟡 P1 | ADR-SOC-09 D5 修订(基于 UsrLinuxEmu ADR-088 KFD 反馈) | TBD | UsrLinuxEmu ADR-088 反馈 | 跨仓依赖 |
| 🟡 P1 | ADR-SOC-14 D5 修订(live migration 节奏) | TBD | UsrLinuxEmu ADR-089 v5.5+ 更新 | 跨仓依赖 |
| 🟡 P1 | Phase 1-3 PCIe 子链路 3 项 ADR(Link Layer / 130b Encoding / PHY Digital) | 4 hr | 已交付实施 | 文档化 |
| 🟢 P2 | 9 类 SimModule 容器单独 ADR(拆分 ADR-SOC-10) | 2 hr | ADR-SOC-10 实施后 | 可选 |
| 🟢 P2 | docs_sync_check 加 dGPU SoC 路径 | 1 hr | 路径已纳入 | 可选 |
| 🟢 P2 | 历史 roadmap 整体归档或重写 | 1-2 hr | Oracle FAIL | ✅ 已降级为快照 |

---

## 📋 Kanban 看板

### 🚀 IN PROGRESS (2027-02-09)

_无进行中任务(全部 2027-02 v1.0 战略交付已完成归档)_

### ⏳ READY (入口条件满足)

| 任务 | 工期 | 入口条件状态 | 前置任务 |
|------|:---:|------|------|
| ADR-SOC-09 D5 修订(KFD 路径) | TBD | [ ] UsrLinuxEmu ADR-088 KFD 反馈 | 跨仓依赖 |
| ADR-SOC-14 D5 修订(live migration) | TBD | [ ] UsrLinuxEmu ADR-089 v5.5+ | 跨仓依赖 |
| Phase 1-3 PCIe 子链路 ADR | 4 hr | [x] 实施已完成 | 文档化 |
| 9 类 SimModule 容器 ADR | 2 hr | [x] ADR-SOC-10 已签发 | 可选拆分 |

### ✅ DONE (2026-09..2027-02 v1.0 战略全部交付)

| 任务 | commit | 工期 | 状态 |
|------|--------|:---:|:---:|
| PCIe EP Phase 1 Link Layer | `e9b1b..` | 3 mo | ✅ Oracle PASS |
| PCIe EP Phase 2 128b/130b Encoding | `8d1f1d5` `b21d290` | 1 mo | ✅ Oracle PASS |
| PCIe EP Phase 3 PHY Digital Ctrl | `05be913..bac9267` | 1 mo | ✅ Oracle 9/10 PASS |
| PCIe EP Phase 4 SR-IOV VF Pool | `4e9564e..6ebbd7d` | 2 mo | ✅ Oracle PASS |
| PCIe EP Phase 5 AXI Stream Adapter | `6223534..b1811703` | 2 mo | ✅ Oracle M1/M2/M3 PASS |
| PCIe EP Phase 6 AXI4Mapper | `ae8ecd7..f2540e8` | 1 mo | ✅ Oracle PASS |
| PCIe EP Phase 7 Host Bypass + RC | `a771253..91e515f` | 2 mo | ✅ Oracle 有条件 PASS |
| PCIe EP Phase 8 整合交付 | `13ee09d..429327d` | 2 mo | ✅ 整合完成 |
| dGPU SoC v1.0 总架构蓝图 | `c568a5a` | 1 day | ✅ Oracle v3.0/v3.1 PASS |
| ADR-SOC-09..14 新增 | `164224f` | 1 day | ✅ Oracle PASS-WITH-CONDITIONS |
| ADR-SOC-01..08 Status Update | `a478d7b` | 1 day | ✅ Oracle PASS |
| 54 份模块微架构同步 | `fd19a7a` | 1 day | ✅ |
| AGENTS.md v3.1 (SoC) 迁移 | `3511922` | 0.5 day | ✅ |
| 4 份新模块微架构(host-bypass等) | `f559bc5` | 0.5 day | ✅ |
| ADR 修订规划 OpenSpec change | `ef78907` `dcd4598` | 1 day | ✅ Oracle validate PASS |

### 🔴 BLOCKED (跨仓依赖)

| 任务 | 工期 | 缺失入口条件 | 阻塞原因 |
|------|:---:|------|------|
| ADR-SOC-09 D5 修订(KFD 路径) | TBD | UsrLinuxEmu ADR-088 KFD 反馈 | 跨仓承诺 |
| ADR-SOC-14 D5 修订(live migration) | TBD | UsrLinuxEmu ADR-089 v5.5+ 更新 | 跨仓承诺 |

### 📜 历史看板(2026-07-03 视角,已过时)

以下为原始 2026-07-03 时点看板,仅供历史对比:

| 任务 | 原始状态 | 当前实际状态 |
|------|:---:|------|
| Phase 8.B Task 9-14 | ⏳ READY | ✅ 已交付 + 注入 PTX-EMU |
| F4 (Phase 7.C CoherentXBarTLM) | 🔴 待启动 | ✅ skeleton + tests + 归档 change |
| F12b-LD (PTX-EMU 集成) | 🔴 blocked | ✅ 已实施归档 (`0f0136e`, 2026-07-16) |
| Python Lib Phase 0/1 | ⏳ READY | ✅ cpptlm/ Python 包已就位 |
| Phase 8.A Oracle + Archive | ✅ | ✅ |

---

## 📊 测试基线演进

| 时点 | cases | assertions | 状态 |
|:---:|:---:|:---:|------|
| 2026-07-03 | 764 | 15547 | ✅ 快照(原始基线) |
| 2026-08-31 | ~870 | ~18000 | ✅ v0.5.0-MVP tag 已打 |
| 2027-02-09 | **1241** | **47634** | ⚠️ 2 known fail (Phase 8 e2e `_config` + `_bar` Minor) |

---

## 🔧 已知问题 (Known Issues)

### Major
- 无

### Minor
- `test_pcie_endpoint_ip_full_e2e.cc:_config` + `_bar` — Phase 8 e2e 已知 Minor,AXI↔PCIe Cfg 地址编码简化(直接用 `awaddr` 当 offset,需未来细化按 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset`)
- `ch_uint<512>` 数据宽度实为 64-bit 存储(per `include/bundles/cpphdl_types.hh`)

---

## 📅 维护历史

| 日期 | 版本 | 维护者 | 说明 |
|------|------|--------|------|
| 2026-07-03 | v1.0 | (原维护者) | 初版,764/764 基线,Phase 8.B 活跃中 |
| 2027-02-09 | v2.0-historical | Sisyphus | Oracle 评审 FAIL → 降级为历史快照,顶部加横幅,新增 ✅ DONE 段列出 2026-09..2027-02 v1.0 战略全部交付 |
