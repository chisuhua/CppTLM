# CppTLM 后续工作指导手册

> **生成日期**: 2026-05-25 (修订版 v3，Oracle 审查后)  
> **审查日期**: 2026-05-25 (基于实际代码 HEAD `c8eacba` 交叉验证)  
> **来源**: 整合自 `reports/` 下 5 份文档 + 实际代码库审计  
> **用途**: 统一指导后续开发、测试、债务清理工作  
> **⚠️ 准确性说明**: 本指南已通过实际代码验证。源文档创建时间跨度大 (2026-04-10 ~ 2026-05-25)，部分早期数据已过时，见第九章。

---

## 一、项目当前状态总览

### 1.1 总体进度

```
Phase 0-2: Core 基础             ✅
Phase 3.1: 配置增强               ✅ (2026-05-07)
Phase 3.2: 端口类型系统           ✅ (2026-05-09, C++ 端完成)
Phase 3.3: 参数推导系统           ✅ 核心已完成 (C++ 端 + Python 端)
Phase 3.4: 工具链验证             🟡 部分完成 (validator 已实现)
Phase 4:   ErrorContext + Debug   ✅
Phase 5:   Modules V2             ✅
Phase 6:   端到端集成             ✅
Phase 7:   交易生命周期           ✅
Phase 8:   统计/性能/压力         ✅
P0 债务:                         全部完成 ✅
P1 债务:                         P1.2 ✅ 已完成(worktree,待合并) | P1.4 🟡 待处理
P2 债务:                         待清理
```

### 1.2 活跃工作流

| 项目 | 状态 |
|------|------|
| **OpenSpec Change**: tgms-phase-3-plus | ✅ P1.2 latency bug 修复完成 (worktree，待合并 main) |
| **Worktree**: `.zcf/tgms-phase-3-plus-wt` | 分支 `openspec/tgms-phase-3-plus` |
| **Workflow 阶段**: execute → status_archive → cleanup | ⏳ 进行中 |

### 1.3 测试现状 (截至 2026-05-25)

| 指标 | 数值 |
|------|------|
| 测试文件 (`test/*.cc`) | **68** |
| Python 测试文件 | **8** |
| 测试用例总数 | **531** |
| 断言总数 | **14,505** |
| 运行状态 | **✅ 全部通过** |
| 测试标签 | **173 个** |

**按 Phase 分布**:

| Phase | 用例数 | 状态 |
|-------|--------|------|
| Phase 0 (核心) | 6 | ✅ |
| Phase 2 (Core 扩展) | 12 | ✅ |
| Phase 3 (交易上下文) | 32 | ✅ |
| Phase 3.2 (端口管理) | 46 | ✅ |
| Phase 3.3 (参数推导) | 12 | ✅ |
| Phase 4 (错误上下文) | 6 | ✅ |
| Phase 5 (模块 V2) | 6 | ✅ |
| Phase 6 (端到端) | 16 | ✅ |
| Phase 7 (交易生命周期) | 2 | ✅ |
| Phase 8 (统计/压力) | 16 | ✅ |

**关键域分布**:

| 域 | 用例数 |
|----|--------|
| [TLM] | 72 |
| [cache] | 32 |
| [chstream] | 31 |
| [config] | 28 |
| [crossbar] | 23 |
| [e2e] | 32 |
| [error] | 35 |
| [nic] | 15 |
| [param] | 16 |
| [port_types] | 12 |
| [port_compat] | 21 |
| [stats] | 35+ |
| [stress] | 17 |

---

## 二、代码审查发现

摘自 `CODE_REVIEW_2026-05-25.md` (当日审查，已验证准确)。

### 🔴 P0 — 必须修复

| # | 问题 | 文件 | 影响 | 修复方案 |
|---|------|------|------|---------|
| 1 | **RouterTLM 流水线状态未清零** | `include/tlm/router_tlm.hh` | 高流量下丢包/乱序 | 每周期开始时保存上周期等待状态，不要直接覆盖 `pipe_reg_` |
| 2 | **PacketPool::acquire() 引用计数** | `include/core/ext/packet_pool.hh` | 多线程共享 PacketPool 时隐患 | 使用 `atomic<uint32_t>` 替代手动 ref_count |

> **审计注**: 当前 `add_ref()` 为**私有死代码**（未被任何调用方使用），`remove_ref()` 仅在持有锁的 `release()` 内调用。若未来多线程场景下共享 PacketPool 实例，此设计会导致竞态。触发条件：多线程共享 PacketPool 时必须修复。

### 🟡 P1 — 高优先级

| # | 问题 | 文件 | 确认状态 | 修复方案 |
|---|------|------|---------|---------|
| 3 | **CrossbarTLM 总线冲突未检测** | `include/tlm/crossbar_tlm.hh:86` | ✅ 确认存在 — `resp_out[dst].write(resp)` 无冲突检测 | 添加 `port_busy[]` 检测数组 |
| 4 | **TransactionTracker::record_hop Extension 同步缺失** | `include/framework/transaction_tracker.hh:133` | ✅ 确认存在 — `(void)event` 注释 | 补全 extension 同步逻辑 |

### 🟢 P2-P3 — 中等优先级

| # | 问题 | 文件 | 确认状态 | 修复方案 |
|---|------|------|---------|---------|
| 5 | **PayloadToPacket 哈希函数错误** | `include/core/ext/payload_to_packet.hh:31` | ✅ 确认 — `std::hash<uint64_t>` 用于 pair key | 自定义 `PairHash` |
| 6 | **DebugTracker 单例初始化时序** | `include/framework/debug_tracker.hh:99` | ✅ 确认 — `if (initialized_) return;` 锁外检查 | `std::call_once` 或 mutex 保护 |
| 7 | **RouterTLM VC 分配失败未处理** | `src/tlm/router_tlm.cc:364` | ✅ 确认 — `return NUM_VCS` 后调用处不检查 | 增加调用处检查 |
| 8 | **PortManager 模板静态断言过于严格** | `include/core/port_manager.hh` | 待确认 | 放宽 `is_base_of_v<SimObject>` 约束 |

---

## 三、剩余工作清单

### 3.1 P1 债务

#### P1.2 — latency 注入修复 [worktree 中已完成]

| 字段 | 内容 |
|------|------|
| **状态** | ✅ 在 `tgms-phase-3-plus` worktree 中已完成，**待合并到 main** |
| **文件** | `src/core/connection_resolver.cc` (worktree 版本) |
| **修复内容** | line 41: `int latency = conn.value("latency", 0)` — 从 JSON 提取 latency 并传播到 `PortCreationInfo` |
| **合并阻碍** | 需要从 worktree 合并到 main 分支 |
| **验证** | `./build/bin/cpptlm_tests "[connection]" --output-on-failure` |

#### P1.4 — 清理冗余配置文件 [🟡 中优先]

| 应删除 | 保留 |
|--------|------|
| `configs/mesh_2x2.json` | `configs/mesh_2x2_tlm.json` |
| `configs/hierarchical_2x2.json` | `configs/hierarchical_2x2_tlm.json` |
| `configs/ring_8.json` | `configs/ring_8_tlm.json` |

**验证**: `./build/bin/cpptlm_tests --output-on-failure`  
**工时**: 1-2h

### 3.2 P2 债务

| ID | 任务 | 文件/范围 | 工时 | 优先级 |
|----|------|-----------|------|--------|
| **P2.A** | linter 合并到 validator | `scripts/linter.py` → `cpptlm_config/validator.py` | 2-3h | 🟡 |
| **P2.B** | typedef → using 现代化 | `include/ext/mem_exts.hh` 等 | 1-2h | 🟢 |
| **P2.C** | Legacy modules 审计 + deprecation 标记 | `include/modules/legacy/` | 1-2h | 🟢 |
| **P2.D** | DPRINTF 日志一致性 | 全局文件 | 1-2h | 🟢 |

### 3.3 TODO 注释清理

- `src/traffic_main.cpp`: 流量生成模块相关 TODO，待 v2.1 架构决策

### 3.4 工时汇总

> **人力基准**: 所有工时估计基于 **1 FTE**（单人全职）。若投入 2 FTE，时间约减半。

| 类别 | 工时 (1 FTE) |
|------|------|
| P1.4 (配置文件清理) | 1-2h |
| P2.A (linter 合并) | 2-3h |
| P2.B-D | 4-6h |
| TODO 清理 | 1h |
| **技术债务小计** | **约 8-12h** |
| P1.2 (合并 worktree → main) | < 1h |
| P0/P1 代码审查修复 | 3-5 天 |
| Phase 3.4 剩余开发 | 约 3 周 |
| Phase 3.3 Python 端 | 约 1 周 |

---

## 四、Phase 3.3 — 参数推导系统

**审计状态**: 核心 C++ 端已完成，Python 端部分完成。

### C++ 端 (已完成 ✅)

| ID | 任务 | 状态 | 实际文件 |
|----|------|------|---------|
| T3.3-01 | 创建 `param_rules.hh` | ✅ 已完成 | `include/core/param_rules.hh` |
| T3.3-02 | 创建 `param_parser.hh` | ✅ 已完成 | `include/core/param_parser.hh` |
| T3.3-03 | 创建 `param_errors.hh` | ✅ 已完成 | `include/core/param_errors.hh` |
| T3.3-04 | ModuleFactory 加载 ParamRule | ✅ 已完成 | |
| T3.3-05 | set_config() 异常处理 | ✅ 已完成 | |
| T3.3-06 | 全局参数规则文件 | ✅ 已完成 | `configs/param_rules/*.json` |
| T3.3-10 | 参数推导表达式解析器 | ✅ 已完成 | 三元表达式支持 |
| T3.3-11 | 参数系统单元测试 (25+ 测试) | ✅ 已完成 | `test/test_param_rules.cc`, `test_param_parser.cc`, `test_param_defaults.cc`, `test_param_errors.cc` |

### Python 端 (4.5 天)

| ID | 任务 | 工时 | 状态 |
|----|------|------|------|
| T3.3-07 | Credit Flow 自动计算 | 1.5d | 📋 待实施 |
| T3.3-08 | Credit Flow JSON Schema 扩展 | 0.5d | 📋 待实施 |
| T3.3-09 | 两阶段验证集成 (Pydantic + C++) | 1.5d | 📋 待实施 |
| T3.3-12 | Python 参数系统测试 (15+ 测试) | 1d | 📋 待实施 |

### 集成与文档 (2 天)

| ID | 任务 | 工时 | 状态 |
|----|------|------|------|
| T3.3-13 | 端到端集成测试 — Credit Flow | 1d | 📋 待实施 |
| T3.3-14 | 更新架构文档 | 0.5d | 📋 待实施 |
| T3.3-15 | 更新用户指南 | 0.5d | 📋 待实施 |

---

## 五、Phase 3.4 — 拓扑验证与工具链

### Python 端 (11.5 天)

| ID | 任务 | 工时 | 状态 |
|----|------|------|------|
| T3.4-01 | 创建 `validator.py` — TopologyValidator | 2d | ✅ 已存在 |
| T3.4-02 | 孤立节点检测 | 0.5d | 📋 待实施 |
| T3.4-03 | BFS 可达性检测 | 0.5d | 📋 待实施 |
| T3.4-04 | 路由器端口方向验证 | 1d | 📋 待实施 |
| T3.4-05 | Bundle 类型兼容性验证 | 1d | 📋 待实施 |
| T3.4-06 | 两阶段验证集成 | 0.5d | 📋 待实施 |
| T3.4-07 | 静态负载分析 | 1.5d | 📋 待实施 |
| T3.4-08 | 连接路径追踪 | 1.5d | 📋 待实施 |
| T3.4-09 | 配置 lint 工具 | 1d | 📋 待实施 |
| T3.4-10 | 拓扑分析工具测试 (25+ 测试) | 1.5d | 📋 待实施 |
| T3.4-14~17 | pyproject.toml/SemVer/端口组/可视化 | 2d | 📋 待实施 |

### 集成与文档 (1.5 天)

| ID | 任务 | 工时 | 状态 |
|----|------|------|------|
| T3.4-11 | 端到端集成测试 | 0.5d | 📋 待实施 |
| T3.4-12 | 更新架构文档 | 0.5d | 📋 待实施 |
| T3.4-13 | 更新用户指南 | 0.5d | 📋 待实施 |

---

## 六、测试补充计划 (Gap Analysis)

> **⚠️ 重要说明**: 本指南最初基于 2026-04-10 的 `TEST_PLAN_v2.md` (距今 223 commits)。该计划中的 T7-T12 多数测试已在 Phase 7/8 中实现。以下为基于 **531 当前用例** 的差距分析。

### 6.1 现有但需关注的测试区域

| 区域 | 当前用例数 | 评估 |
|------|-----------|------|
| 完整交易流程 (原 T7) | ✅ 已覆盖在 `[phase7]` (2例) + `[transaction]` (12例) | 基本覆盖 |
| 错误传播 (原 T8) | ✅ 已覆盖在 `[error]` (35例) + `[phase4]` (6例) | 覆盖良好 |
| 端到端集成 (原 T9) | ✅ 已覆盖在 `[e2e]` (32例) + `[integration]` (19例) | 覆盖良好 |
| 压力测试 (原 T10) | ✅ 已覆盖在 `[stress]` (17例) + `[phase8]` (16例) | 覆盖良好 |
| 框架层 (原 T11) | ✅ 已覆盖在 `[chstream]` (31例) + `[framework]` (6例) | 覆盖良好 |
| Bundle 层 (原 T12) | ✅ 已覆盖在 `[bundle]` (14例) + `[noc]` (20例) | 覆盖良好 |

### 6.2 潜在缺口

| 缺口 | 说明 | 优先级 |
|------|------|--------|
| ADR-X.1 ID 分配端到端 | 虽被 `[transaction]` 覆盖，但专用测试可能不足 | 🟡 |
| 多模块级联 (3+ 模块) | 部分 `[e2e]` 测试覆盖，但专用级联测试可能缺失 | 🟡 |
| 长时间稳定性 (100K+ cycles) | 需要确认 `[stress]` 测试的运行时长 | 🟢 |
| ASan 内存泄漏检测 | CI 中是否启用 ASan 运行测试？ | 🟡 |

### 6.3 已知问题

| 问题 ID | 描述 | 严重性 | 状态 |
|---------|------|--------|------|
| BUG-001 | MockSimObject.save_snapshot 缺少 type | 低 | 🔧 修复中 |
| BUG-002 | 便捷函数测试双重释放 | 中 | 🔧 修复中 |
| BUG-004 | 旧测试不稳定 (config_loader, layout) | 中 | ⏳ 隔离中 |

---

## 七、推荐执行路线

### 第 1 优先 — 代码审查 P0 修复 (安全修复)

```
1. RouterTLM 流水线状态丢失    [直接影响仿真正确性]
2. PacketPool 死代码清理       [add_ref 未使用, 设计清理]
```

### 第 2 优先 — 合并 worktree + 债务清理 (可并行)

```
P1.2 (合并 worktree → main)    [<1h, 低风险]
P1.4 (配置文件清理)             [1-2h, 简单]
P2.A (linter 合并)              [2-3h, 可并行]
P2.B-D (现代化/审计/日志)       [4-6h, 可并行]
```

### 第 3 优先 — 代码审查 P1/P2 修复

```
CrossbarTLM 冲突检测            [P1]
TransactionTracker 同步         [P1]
PayloadToPacket 哈希            [P2]
DebugTracker 初始化 race        [P2]
VC 分配失败处理                 [P3]
PortManager 静态断言            [P3]
```

### 第 4 优先 — Phase 3.4 验证工具链 (3 周)

```
Week 1-2: Python 端 (T3.4-02~10)
Week 3:   集成测试 + 文档 (T3.4-11~13)
```

### 第 5 优先 — Phase 3.3 Python 端 (1 周)

```
Credit Flow 自动计算 + Python 测试 (T3.3-07~09, T3.3-12~15)
```

### 多线并行策略

```
线 A: P0 安全修复 → P1/P2 修复 (串行, 不可跳过)
线 B: P1.4 + P2.A~D (与线 A 并行)
线 C: Phase 3.4 Python 端开发 (与线 A/B 并行)
线 D: Phase 3.3 Python 端 (在线 C 之后)
```

---

## 八、常用命令速查

```bash
# 编译
cmake --build build -j$(nproc)

# 全量测试 (531 用例)
./build/bin/cpptlm_tests

# 按标签过滤
./build/bin/cpptlm_tests "[connection]"
./build/bin/cpptlm_tests "[phase3.3]"
./build/bin/cpptlm_tests "[crossbar]"

# 查看所有标签
./build/bin/cpptlm_tests --list-tags

# Python 测试
python -m pytest test/python/ -v

# 拓扑验证
python scripts/topology_validator.py configs/mesh_2x2_tlm.json

# 格式化
./scripts/format.sh --check

# 编译 + 测试 (一键)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF \
  && cmake --build build -j$(nproc) \
  && cd build && ctest --output-on-failure
```

---

## 九、来源文档索引 & 准确性声明

| 文档 | 位置 | 创建日期 | 准确性说明 |
|------|------|---------|-----------|
| 代码审查报告 | `reports/CODE_REVIEW_2026-05-25.md` | 2026-05-25 | ✅ 当日审查，代码问题准确 |
| 迁移指南 | `reports/MIGRATION_GUIDE.md` | 2026-04-10 (223 commits ago) | ⚠️ 部分 API 可能已变化，使用前需验证 |
| 剩余工作清单 | `reports/REMAINING_WORK.md` | 2026-05-19 | ✅ 基本准确 (P1.2 状态已修正) |
| 任务总体规划 | `reports/TASK_PLAN_OVERVIEW.md` | 2026-05-23 | ⚠️ T3.3 多项任务实际已完成，见第四章 |
| 测试计划 v2 | `reports/TEST_PLAN_v2.md` | 2026-04-10 (223 commits ago) | 🔴 **严重过时** — 声明 63 用例，实际 531 用例 |
| 审计报告 | `reports/GUIDE_AUDIT_REPORT.md` | 2026-05-25 | 本指南的审计依据 |

### 审计摘要

| 维度 | 准确率 | 说明 |
|------|--------|------|
| 代码审查发现 | ✅ 100% | 7/7 问题代码审查确认准确 |
| 测试数据 | 🔴 严重偏差 | 声 63 用例，实际 531 |
| Phase 进度 | 🟡 偏保守 | Phase 3.3 核心已完成, Phase 7/8 完全缺失 |
| 文件路径 | ✅ 基本准确 | 除 mem_exts.hh 外均正确 |
| 工时估计 | 🟡 需重新评估 | 基于过时数据 |

---

*本手册由实际代码库交叉验证生成。下次更新时建议直接从测试运行结果 (`cpptlm_tests --list-tests`) 获取最新数据。*
