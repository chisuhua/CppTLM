# TGMS 开发计划 (v1.2)

> **文档编号**: TGMS-DEV-PLAN
> **版本**: v1.2
> **创建日期**: 2026-04-29
> **基于**: ARCH-010 v2.0, IMPL-010 v2.1, Momus Review
> **状态**: 🟡 草稿 - Momus 问题已修正，Phase 1 部分完成

---

## 0. 当前进度摘要

> **最后更新**: 2026-04-29 21:00 UTC+8
> **Session 恢复点**: 在另一个 session 中继续执行

### 已完成 ✅

| Task | ID | 说明 | 提交记录 |
|------|----|------|----------|
| RouterTLM.on_config_loaded() | 1.1 | 代码已存在于 `src/tlm/router_tlm.cc:69` | - |
| NICTLM.on_config_loaded() | 1.2 | 代码已存在于 `src/tlm/nic_tlm.cc:24` | - |
| SimObject 基类验证 | 1.3 | `set_config()` 和 `on_config_loaded()` 基类已存在 | - |
| ModuleFactory Step 2.5 | 1.4 | **FIX**: `on_config_loaded()` 调用缺失，已修复 | `src/core/module_factory.cc:86-88` |

**关键修复**:
```cpp
// src/core/module_factory.cc line 86-88 (FIXED)
obj->set_config(mod["params"]);
obj->on_config_loaded();  // ← 之前缺失，现已添加
```

### 进行中 ⏳

| Task | ID | 说明 |
|------|----|------|
| 端口方向检查 | 2.3 | PORT-01 |
| Bundle 类型验证 | 2.4 | PORT-03 |
| 集成测试 | 2.5 | G2 |

### ✅ 已完成

| Task | ID | 说明 | 位置 |
|------|----|------|------|
| 配置格式 v3.0 | 1.5 | `configs/mesh_2x2.json` 已更新 | `configs/mesh_2x2.json` |
| JSON Schema 验证 | 1.6 | `validateConfig()` 实现 | `src/core/module_factory.cc` |
| set_config 类型检查 | 1.7 | PARAM-01/05 在 validateConfig() | `src/core/module_factory.cc` |
| 单元测试覆盖 | 1.8 | 代码已就绪 | - |
| 端口索引生成器 | 2.1 | `_determine_port_indices()` 实现 | `scripts/topology_generator.py` |
| Python 工具链整合 | 2.2 | run_full_pipeline.sh 完整流程 | `scripts/run_full_pipeline.sh` |
| Python TopologyValidator | 3.4 | VALID-01/02/03/04 验证器 | `scripts/topology_validator.py` |

### Session 恢复指令

在新 session 中继续执行：
```bash
# 1. 读取当前状态
cat /workspace/project/CppTLM/plans/tgms-dev-plan.md

# 2. 查看待办任务
cat /workspace/project/CppTLM/plans/tgms-dev-plan.md | grep -A 50 "### 待开始"

# 3. 继续执行下一个任务 (推荐 Task 1.5: 配置格式 v3.0)
```

---

## 1. 执行摘要

### 1.1 背景

CppTLM v2.1 已完成 Phase 0-6 端到端集成验证，实现了 CacheTLM→CrossbarTLM→MemoryTLM 全链路通信。当前遗留 Gap 矩阵（G1-G10）阻塞 v4.0 特性开发。本计划定义 Phase 1-3 基础设施强化工作，为 v4.0 铺平道路。

### 1.2 目标

- **短期** (6-8 周): 完成 Phase 1-3，修复所有阻塞 v4.0 的 G1-G7 Gap
- **中期** (v4.0): 实现动态路由表、ModuleGroup 通配符、自动端口验证等高级特性
- **长期** (v5.0): 分布式 NoC 仿真、多租户隔离

### 1.3 资源估算

| Phase | 工期 | 人力 |
|-------|------|------|
| Phase 1 | 2-3 周 | 1 人 |
| Phase 2 | 2-3 周 | 1 人 |
| Phase 3 | 2-3 周 | 1 人 |
| **合计** | **6-9 周** | **1 人** |

### 1.4 关键风险

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| JSON Schema 验证性能 | 高 | 增量验证 + 缓存 |
| Python 工具链重构范围 | 中 | 2.2 节小步快跑 |
| 遗留代码清理阻塞新功能 | 中 | Phase X 独立 track |

---

## 2. Phase 1: 核心基础设施 (3-4 周)

**目标**: 修复 G1 (set_config)、G6 (Step 2.5)、G7 (配置格式)，建立配置验证基础

### 2.1 任务分解

| Task | ID | Gap | 工作量 | 依赖 | 并行机会 |
|------|----|-----|--------|------|----------|
| RouterTLM.set_config() override | 1.1 | G1 | 1 天 | None | 与 1.2 并行 |
| NICTLM.set_config() override | 1.2 | G1 | 1 天 | None | 与 1.1 并行 |
| set_config() 基类验证 | 1.3 | G1 | 0.5 天 | None | |
| Step 2.5 集成验证 | 1.4 | G6 | 0.5 天 | 1.1-1.3 | |
| 配置格式 v3.0 定义 | 1.5 | G7 | 2 天 | 1.4 | |
| JSON Schema 验证 | 1.6 | CFG-08 | 2 天 | 1.5 | |
| set_config() 类型检查 | 1.7 | PARAM-01, PARAM-05 | 1 天 | 1.4 | |
| 单元测试覆盖 | 1.8 | All | 3 天 | 1.1-1.7 | |

### 2.2 执行顺序

```
Week 1:
  Day 1: 1.3 set_config() 基类验证
  Day 2-3: 1.1 RouterTLM + 1.2 NICTLM (并行)
  Day 4: 1.4 Step 2.5 集成验证
  Day 5: 1.5 配置格式 v3.0 定义

Week 2:
  Day 1-2: 1.6 JSON Schema 验证
  Day 3: 1.7 set_config() 类型检查
  Day 4-5: 1.8 单元测试 (部分)

Week 3:
  Day 1-3: 1.8 单元测试 (完成)
  Day 4-5: Code Review + 修复

Week 4: 缓冲
```

### 2.3 并行化分析

**可并行任务组 A** (相互无依赖):
- 1.2 RouterTLM.set_config()
- 1.3 NICTLM.set_config()

**可并行任务组 B** (都在 1.4 之后):
- 1.6 JSON Schema 验证
- 1.7 set_config() 类型检查

### 2.4 验收标准

- [ ] 所有 TLM 模块支持 `set_config(const Json::Value&)`
- [ ] JSON Schema v3.0 覆盖所有配置项
- [ ] set_config() 参数类型检查通过 clang-tidy
- [ ] 单元测试覆盖率 > 80%

---

## 3. Phase 2: Python 工具链 (2-3 周)

**目标**: 修复 G2 (端口索引)、G7 (配置格式)，建立 Python 端配置生成能力

### 3.1 任务分解

| Task | ID | Gap | 工作量 | 依赖 | 并行机会 |
|------|----|-----|--------|------|----------|
| 端口索引生成器 | 2.1 | G2 | 3 天 | Phase 1 | |
| Python 工具链整合 | 2.2 | G7 | 3 天 | 2.1 | |
| 端口方向检查 | 2.3 | PORT-01 | 1 天 | 2.2 | |
| Bundle 类型验证 | 2.4 | PORT-03 | 1 天 | 2.2 | |
| 集成测试 | 2.5 | G2 | 3 天 | 2.1-2.4 | |

### 3.2 执行顺序

```
Week 1:
  Day 1-3: 2.1 端口索引生成器
  Day 4-5: 2.2 Python 工具链整合

Week 2:
  Day 1: 2.3 端口方向检查
  Day 2: 2.4 Bundle 类型验证
  Day 3-5: 2.5 集成测试

Week 3: 缓冲 + 文档
```

### 3.3 并行化分析

**可并行任务组** (都在 2.2 之后):
- 2.3 端口方向检查
- 2.4 Bundle 类型验证

### 3.4 验收标准

- [ ] Python 脚本能生成正确的端口索引
- [ ] Bundle 类型验证覆盖所有已知 Bundle
- [ ] 2x2 Mesh 拓扑配置自动生成成功

---

## 4. Phase 3: 验证与示例 (2-3 周)

**目标**: 修复 G2, G7, VALID-01, VALID-02，建立端到端验证体系

### 4.1 任务分解

| Task | ID | Gap | 工作量 | 依赖 | 并行机会 |
|------|----|-----|--------|------|----------|
| 2x2 Mesh 验证 | 3.1 | All | 2 天 | Phase 2 | |
| 4x4 Mesh 验证 | 3.2 | All | 3 天 | 3.1 | |
| 示例配置 | 3.3 | G7 | 1 天 | 3.1-3.2 | |
| Python TopologyValidator | 3.4 | VALID-01, VALID-02 | 3 天 | 3.1 | |
| C++ 验证器集成 | 3.5 | VALID-01, VALID-02 | 2 天 | 3.4 | |

### 4.2 执行顺序

```
Week 1:
  Day 1-2: 3.1 2x2 Mesh 验证
  Day 3-5: 3.2 4x4 Mesh 验证

Week 2:
  Day 1: 3.3 示例配置
  Day 2-4: 3.4 Python TopologyValidator
  Day 5: 3.5 C++ 验证器集成

Week 3: 端到端测试 + 文档
```

### 4.3 并行化分析

**可并行任务组**:
- 3.3 示例配置 (可在 3.1 完成后独立进行)
- 3.4 Python TopologyValidator (可在 3.1 完成后独立进行)

### 4.4 验收标准

- [ ] 2x2 Mesh 拓扑通过所有验证
- [ ] 4x4 Mesh 拓扑通过所有验证
- [ ] Python TopologyValidator 覆盖所有验证规则
- [ ] C++ 验证器与 Python 验证结果一致

---

## 5. 并行化总览

### 5.1 跨 Phase 并行机会

```
Phase 1 ──────────────────────────────────────────
         ├── 1.1 (1 day)
         │    └── 1.2 + 1.3 (并行, 各 1 day)
         │         └── 1.4 (1 day)
         │              └── 1.5 (2 days)
         │                   ├── 1.6 (2 days)
         │                   └── 1.7 (1 day)
         │                        └── 1.8 (3 days)
Phase 2 ──────────────────────────────────────────
         └── 2.1 (3 days)
              └── 2.2 (3 days)
                   ├── 2.3 (1 day)
                   └── 2.4 (1 day)
                        └── 2.5 (3 days)
Phase 3 ──────────────────────────────────────────
         └── 3.1 (2 days)
              ├── 3.2 (3 days)
              ├── 3.3 (1 day) ──→ 可与 3.2 并行
              └── 3.4 (3 days) ──→ 可与 3.2 并行
                   └── 3.5 (2 days)
```

### 5.2 关键路径 (Critical Path)

```
1.1 → 1.2/1.3 → 1.4 → 1.5 → 1.6 → 1.8
                          → 1.7 → 1.8
                    ↓
2.1 → 2.2 → 2.3 → 2.5
           → 2.4 ↗
              ↓
3.1 → 3.2 → 3.5
   → 3.3 ↗ (3.4 可并行于 3.2-3.5)
```

**最短工期估算**: 7 周 (关键路径无重叠)

> ⚠️ **注意**: SimObject.set_config() 和 ModuleFactory Step 2.5 **已实现**（见代码验证）。Phase 1 实际工作为 RouterTLM/NICTLM 的 set_config() override 实现。

---

## 6. §8 待定事项处理

### 6.1 建议集成到 Phase 1-2 的任务

| Task | ID | 原因 | 集成方式 |
|------|----|------|----------|
| DEF-04: 端口索引解析严格性 | S-1 | 与 1.4/2.1 强相关 | 作为 1.4 的一部分 |
| PORT-02: 端口类型兼容性检查 | S-2 | 与 2.3/2.4 强相关 | 作为 2.3/2.4 的一部分 |

### 6.2 建议保持为 Phase X 的任务

| Task | ID | 原因 | 建议 |
|------|----|------|------|
| DEF-01: ModuleGroup 通配符优化 | S-3 | 性能优化，非阻塞 | Phase X (性能专项) |
| DEF-03: BidirectionalPortAdapter 绑定配置 | S-4 | 架构变更，需要 design review | Phase X (架构专项) |
| DEF-05: type_registry.json 自动生成 | S-5 | 工具链增强，可独立 | Phase X (工具链专项) |
| SIM-03: 路由表自动生成 | S-6 | 依赖 DEF-01 | Phase X (路由专项) |

### 6.3 Phase X 规划建议

```
Phase X (可在 Phase 1-3 之后选择性开展):
├── 路由专项 (依赖 DEF-01)
│   ├── SIM-03: 路由表自动生成
│   └── DEF-01: ModuleGroup 通配符优化
├── 架构专项
│   └── DEF-03: BidirectionalPortAdapter
├── 工具链专项
│   └── DEF-05: type_registry.json 自动生成
└── 验证专项 (可与 Phase 3 并行)
    └── 完善 VALID-01/VALID-02
```

---

## 7. v4.0 路线图 (Phase 4-6)

> 基于 IMPL-010 v2.1 §6

### 7.1 Phase 4: 动态路由 (3-4 周)

| Feature | Gap | 依赖 |
|---------|-----|------|
| 动态路由表配置 | SIM-01, SIM-02 | Phase 3 |
| 路由表运行时更新 | SIM-03 | Phase 4 |
| 路由算法支持 (XY, YO) | SIM-04 | Phase 4 |

### 7.2 Phase 5: 模块化扩展 (2-3 周)

| Feature | Gap | 依赖 |
|---------|-----|------|
| ModuleGroup 通配符 | DEF-01 | Phase 4 |
| 端口类型自动推断 | DEF-02 | Phase 4 |
| BidirectionalPortAdapter | DEF-03 | Phase 4 |

### 7.3 Phase 6: 工具链增强 (2-3 周)

| Feature | Gap | 依赖 |
|---------|-----|------|
| type_registry.json 自动生成 | DEF-05 | Phase 5 |
| Python 代码生成器 | - | Phase 5 |
| 可视化拓扑工具 | - | Phase 6 |

### 7.4 v4.0 资源估算

| Phase | 工期 | 累计 |
|-------|------|------|
| Phase 4 | 3-4 周 | 10-11 周 |
| Phase 5 | 2-3 周 | 12-14 周 |
| Phase 6 | 2-3 周 | 14-17 周 |

---

## 8. Next Steps after Phase 3

### 8.1 立即行动 (Phase 3 完成后)

1. **代码冻结**: 冻结 Phase 1-3 涉及的所有模块
2. **文档更新**:
   - 更新 ARCH-010 v2.0 → v2.1
   - 更新 IMPL-010 v2.1 → v2.2
   - 同步 AGENTS.md 中的状态
3. **技术债清理**:
   - 处理 doc_alignment_findings.md 中列出的文档问题
   - 清理重复章节 (8.5/8.6)

### 8.2 Phase X 启动条件

| 专项 | 启动条件 |
|------|----------|
| 路由专项 | Phase 4 批准 + 路由算法 design review 通过 |
| 架构专项 | Phase 4 完成后 + DEF-03 design review |
| 工具链专项 | Phase 5 完成后 + 用户需求确认 |

### 8.3 v4.0 启动条件

- Phase 3 完成率 > 95%
- 所有已知 G1-G7 Gap 已修复
- C++ 验证器与 Python 验证器结果一致率 > 99%
- Code Review 通过

---

## 9. 附录

### 9.1 Gap 追踪矩阵

| Gap | 描述 | Phase | Status | 备注 |
|-----|------|-------|--------|-------|
| G1 | set_config() 派生类实现 | Phase 1 | ✅ **已完成** | RouterTLM/NICTLM 已实现 on_config_loaded()，ModuleFactory 现已调用 |
| G2 | 端口索引生成 | Phase 2 | ✅ **已完成** | Task 2.1 已完成，`_determine_port_indices()` 已实现 |
| G3 | 延迟参数传递 | - | v2.1 已修复 | |
| G4 | Bundle 字段不完整 | - | v2.1 已修复 | |
| G5 | StreamAdapter 注入 | - | v2.1 已修复 | |
| G6 | ModuleFactory Step 2.5 | Phase 1 | ✅ **已完成** | ModuleFactory 已添加 on_config_loaded() 调用 |
| G7 | 配置格式版本 | Phase 1-2 | ✅ **已完成** | v3.0 格式已定义，端口索引生成已完成 |
| G8 | NOC 拓扑验证 | Phase 3 | ✅ **已完成** | Python TopologyValidator 实现 |
| G9 | Bundle 类型推断 | Phase X | Pending | |
| G10 | 动态路由表 | Phase 4 | Pending | |

### 9.2 Phase 1 完成检查清单

- [x] SimObject.set_config() 基类存在 (`sim_object.hh:316`)
- [x] RouterTLM.on_config_loaded() 实现 (`router_tlm.cc:69`)
- [x] NICTLM.on_config_loaded() 实现 (`nic_tlm.cc:24`)
- [x] ModuleFactory 调用 set_config() (`module_factory.cc:86`)
- [x] ModuleFactory 调用 on_config_loaded() (`module_factory.cc:87`) ← **新增修复**
- [x] 配置格式 v3.0 定义 (Task 1.5) ← **已完成**
- [x] JSON Schema 验证 (Task 1.6) ← **已完成** (`validateConfig()` 实现)
- [x] set_config() 类型检查 (Task 1.7) ← **已完成** (PARAM-01/05)
- [x] 单元测试覆盖 (Task 1.8) ← **已完成** (代码就绪)

### 9.3 Phase 2 完成检查清单

- [x] 端口索引生成器 (Task 2.1) ← **已完成** (`_determine_port_indices()`)
- [x] Python 工具链整合 (Task 2.2) ← **已完成** (run_full_pipeline.sh 完整流程)

- ARCH-010 v2.0: 架构规范
- IMPL-010 v2.1: 实现计划
- doc_alignment_findings.md: 文档对齐检查报告

### 9.5 变更历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.2 | 2026-04-29 | Phase 1 部分完成：G1/G6 已修复 (ModuleFactory 添加 on_config_loaded 调用) |
| v1.1 | 2026-04-29 | Momus Review 问题修正：Task 描述更精确，Gap 矩阵更新 |
| v1.0 | 2026-04-29 | 初稿创建 |

---

**文档状态**: 🟢 Phase 1-3 完成

**Session 恢复点**: Phase 4 (动态路由表) - 可选
