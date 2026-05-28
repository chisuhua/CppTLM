# CppTLM 路线图 — Phase 3+ 实施规划

> **版本**: v1.1
> **编制日期**: 2026-05-07
> **基于文档**: ADR-X.9, ADR-X.10, ADR-X.11, ADR-X.12
> **状态**: 🗄️ 已归档 (2026-05-28) — Phase 3.1-3.4 路线图归档至本文件，TGMS v4.0 请参考 docs/implementation/11-tgms-v4-implementation-plan.md

---

## 一、总体进度

```
Phase 3.1 ✅ 已完成 (2026-05-07, commit 458a9d7)
├── DEF-01~05 缺陷修复
├── 配置继承 (extends)
├── 变量引用 ($ref)
├── 参数默认值 (RouterTLM)
└── NI PE-side 连接验证

Phase 3.2 ✅ 已完成 (2026-05-09, C++ 端全部完成)
├── port_types.hh (C++) — 25+ 测试通过
├── port_compatibility.hh (C++) — 兼容性矩阵工作正常
├── 端口别名系统 — router.NORTH → router.0 解析正确
├── DEF-04 WARNING 日志 — 非法端口索引产生 WARNING
└── Python 工具链集成 — RouterPort/NICPort 枚举、SemVer、pyproject.toml

Phase 3.3 🟡 部分完成 (2026-05-09)
├── param_rules.json — ✅ 已存在（configs/param_rules/router_tlm.json, nic_tlm.json）
├── 参数推导表达式 — ✅ derive_expr 已实现（router_tlm.json: "(mesh_x >= 4) ? 8 : 4"）
├── Credit Flow 自动计算 — 📋 待实施
└── 双重验证 (Python + C++) — 📋 待实施

Phase 3.4 🟡 部分完成 (2026-05-09)
├── cpptlm_config/ Python 包 — ✅ 已存在（builder.py, validator.py, models.py, types.py）
├── topology_validator.py — ✅ validator.py 已实现基础验证
├── 连通性验证 — 📋 待实施
├── 静态负载分析 — 📋 待实施
└── 配置 lint 工具 — 📋 待实施
```

---

## 二、Phase 3.2 详情

**前置条件**: Phase 3.1 完成 + 测试通过

### 2.1 C++ 端任务

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:---:|
| T3.2-01 | 创建 `port_types.hh` — PortSpec, PortRole, BundleType | 1d | 📋 提案 |
| T3.2-02 | 创建 `port_compatibility.hh` — 三层兼容性检查 | 1d | 📋 提案 |
| T3.2-03 | ModuleFactory 集成 port_specs JSON 反序列化 | 2d | 📋 提案 |
| T3.2-04 | 端口方向检查 (L1) | 1d | 📋 提案 |
| T3.2-05 | Bundle 类型匹配检查 (L2) | 1d | 📋 提案 |
| T3.2-06 | 数据宽度检查 (L3) | 0.5d | 📋 提案 |
| T3.2-07 | 端口别名系统 | 1.5d | 📋 提案 |
| T3.2-08 | NICTLM 端口组支持 | 1.5d | 📋 提案 |
| T3.2-09 | DEF-04 WARNING 日志 | 0.5d | 📋 提案 |
| T3.2-14 | 端口类型单元测试 | 2d | 📋 提案 |

### 2.2 里程碑

| 里程碑 | 日期 | 验收标准 |
|--------|------|---------|
| M1: 端口类型基础完成 | Week 1 | port_types.hh + port_compatibility.hh 可用 |
| M2: ModuleFactory 集成完成 | Week 2 | port_specs JSON 解析 + 三层检查矩阵工作正常 |
| M3: Python 集成完成 | Week 3 | RouterPort/NICPort/SemVer/pyproject.toml 完成 |
| M4: Phase 3.2 发布 | Week 4 | 所有测试通过 (50+)，文档完整 |

---

## 三、Phase 3.3 详情

**前置条件**: Phase 3.2 完成 + 50+ 新测试通过

### 3.1 与 Phase 3.1 已实现代码的关系

| 组件 | Phase 3.1 (已实现) | Phase 3.3 (计划) |
|------|-------------------|-----------------|
| ParamRule 结构体 | ✅ `include/core/param_rules.hh` | 升级为 JSON 序列化 |
| get_param_rules() | ✅ RouterTLM | 扩展到所有模块 |
| 默认值应用 | ✅ 已实现 | 扩展 derive_expr |
| 参数验证 | ❌ 未实现 | 实现两阶段验证 |
| 全局规则文件 | ❌ 未实现 | `configs/param_rules/*.json` |

### 3.2 C++ 端任务

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:---:|
| T3.3-01 | 创建 `param_rules.hh` JSON 序列化版本 | 1d | 📋 提案 |
| T3.3-02 | 创建 `param_parser.hh` — 类型转换 + derive_expr | 2d | 📋 提案 |
| T3.3-03 | 创建 `param_errors.hh` — ParamValidationError | 0.5d | 📋 提案 |
| T3.3-04 | ModuleFactory ParamRule 验证 | 2d | 📋 提案 |
| T3.3-05 | set_config() 异常处理 | 1d | 📋 提案 |
| T3.3-06 | 全局参数规则文件 | 0.5d | 📋 提案 |
| T3.3-10 | 参数推导表达式解析器 | 1.5d | 📋 提案 |
| T3.3-11 | 参数系统单元测试 | 2d | 📋 提案 |

### 3.3 Python 端任务

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:---:|
| T3.3-07 | Credit Flow 自动计算 | 1.5d | 📋 提案 |
| T3.3-08 | Credit Flow JSON Schema 扩展 | 0.5d | 📋 提案 |
| T3.3-09 | 两阶段验证集成 | 1.5d | 📋 提案 |
| T3.3-12 | Python 参数系统测试 | 1d | 📋 提案 |

---

## 四、Phase 3.4 详情

**前置条件**: Phase 3.3 完成 + 90+ 新测试通过

### 4.1 前置条件说明

> **注意**: Phase 3.4 依赖 `cpptlm_config/` Python 包。
> ✅ `cpptlm_config/` 目录已存在（builder.py, validator.py, models.py, types.py, topology_adapter.py）。

### 4.2 Python 端任务

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:---:|
| T3.4-01 | 创建 `cpptlm_config/validator.py` | 2d | 📋 提案 |
| T3.4-02 | 孤立节点检测 | 0.5d | 📋 提案 |
| T3.4-03 | BFS 可达性检测 | 0.5d | 📋 提案 |
| T3.4-04 | 路由器端口方向验证 | 1d | 📋 提案 |
| T3.4-05 | Bundle 类型兼容性验证 | 1d | 📋 提案 |
| T3.4-06 | 两阶段验证集成 | 0.5d | 📋 提案 |
| T3.4-07 | 静态负载分析 | 1.5d | 📋 提案 |
| T3.4-08 | 连接路径追踪 | 1.5d | 📋 提案 |
| T3.4-09 | 配置 lint 工具 | 1d | 📋 提案 |

---

## 五、参考文档

| 文档 | 说明 |
|------|------|
| `docs/plan/phase3.1-defect-fixes-plan.md` | ✅ 已完成 — Phase 3.1 详细计划 |
| `docs/plan/phase3.2-port-management-plan.md` | ✅ 已完成 — Phase 3.2 详细计划（505 测试通过） |
| `docs/plan/phase3.3-config-enhancement-plan.md` | 📋 路线图 — Phase 3.3 详细计划 |
| `docs/plan/phase3.4-validation-toolchain-plan.md` | 📋 路线图 — Phase 3.4 详细计划 |
| `docs/adr/ADR-X.11-config-inheritance-and-fixes.md` | ✅ 已实施 — 配置继承决策 |
| `docs/adr/ADR-X.9-port-type-system.md` | 待实施 — 端口类型系统 ADR |
| `docs/adr/ADR-X.10-parameter-framework.md` | 待实施 — 参数框架 ADR |
| `docs/adr/ADR-X.12-python-config-generator.md` | 待实施 — Python 配置生成器 ADR |

---

## 六、变更日志

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.1 | 2026-05-09 | 文档同步审计：Phase 3.2 标记为完成，Phase 3.3/3.4 标记为部分完成，更新 cpptlm_config 状态 |
| v1.0 | 2026-05-07 | 初始版本，整合 Phase 3.1-3.4 计划为统一路线图 |

---

**维护**: CppTLM 开发团队
**最后更新**: 2026-05-07