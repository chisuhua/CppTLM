# CppTLM 任务总体规划

> **生成日期**: 2026-05-23
> **基于**: docs/implementation/, docs/adr/, docs/plan/, docs/superpowers/plans/
> **版本**: v1.0

---

## 一、总体进度概览

```
Phase 3.1 ✅ 已完成 (2026-05-07, commit 458a9d7)
├── DEF-01~05 缺陷修复
├── 配置继承 (extends)
├── 变量引用 ($ref)
├── 参数默认值 (RouterTLM)
└── NI PE-side 连接验证

Phase 3.2 ✅ C++ 端已完成 (2026-05-09)
├── port_types.hh — 25+ 测试通过
├── port_compatibility.hh — 兼容性矩阵
├── 端口别名系统 — router.NORTH → router.0
├── DEF-04 WARNING 日志
└── Python 工具链集成 (RouterPort/NICPort/SemVer)

Phase 3.3 🟡 部分完成 (2026-05-09)
├── param_rules.json — ✅ 已存在
├── 参数推导表达式 — ✅ derive_expr 已实现
├── Credit Flow 自动计算 — 📋 待实施
└── 双重验证 (Python + C++) — 📋 待实施

Phase 3.4 🟡 部分完成 (2026-05-09)
├── cpptlm_config/ Python 包 — ✅ 已存在
├── topology_validator.py — ✅ 基础验证已实现
├── 连通性验证 — 📋 待实施
├── 静态负载分析 — 📋 待实施
└── 配置 lint 工具 — 📋 待实施
```

---

## 二、Phase 3.2 端口管理系统 — 已完成 ✅

| 任务 ID | 任务描述 | 状态 |
|---------|---------|:----:|
| T3.2-01 | 创建 `port_types.hh` — PortSpec, PortRole, BundleType | ✅ |
| T3.2-02 | 创建 `port_compatibility.hh` — 三层兼容性检查 | ✅ |
| T3.2-03~06 | ModuleFactory 集成 L1/L2/L3 兼容性检查 | ✅ |
| T3.2-07 | 端口别名系统 — deprecated_names + resolve_port_alias() | ✅ |
| T3.2-08 | NICTLM 端口组支持 — port_groups JSON 解析 | ✅ |
| T3.2-09 | DEF-04 WARNING 日志 — 非法端口索引时打印警告 | ✅ |
| T3.2-14 | 端口类型单元测试 — C++ 端 25+ 测试 | ✅ |
| T3.2-16 | 端到端集成测试 — mesh_2x2/4x4 配置通过 | ✅ |
| T3.2-17~18 | 更新架构文档/用户指南 | ⚠️ 部分完成 |

---

## 三、Phase 3.3 配置能力增强 — 部分完成 🟡

### 3.1 C++ 端任务 (10.5 天)

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:----:|
| T3.3-01 | 创建 `param_rules.hh` — ParamRule + nlohmann/json 宏 | 1d | 📋 |
| T3.3-02 | 创建 `param_parser.hh` — 类型转换 + derive_expr 评估 | 2d | 📋 |
| T3.3-03 | 创建 `param_errors.hh` — ParamValidationError 异常 | 0.5d | 📋 |
| T3.3-04 | ModuleFactory 加载 ParamRule 验证 | 2d | 📋 |
| T3.3-05 | set_config() 异常处理 — 验证失败抛出异常 | 1d | 📋 |
| T3.3-06 | 全局参数规则文件 — `configs/param_rules/*.json` | 0.5d | 📋 |
| T3.3-10 | 参数推导表达式解析器 — 三元表达式 `(cond) ? v1 : v2` | 1.5d | 📋 |
| T3.3-11 | 参数系统单元测试 — C++ 端 25+ 测试 | 2d | 📋 |

### 3.2 Python 端任务 (4.5 天)

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:----:|
| T3.3-07 | Credit Flow 自动计算 — credit_capacity + credit_return_latency | 1.5d | 📋 |
| T3.3-08 | Credit Flow JSON Schema 扩展 | 0.5d | 📋 |
| T3.3-09 | 两阶段验证集成 — Python Pydantic + C++ ModuleFactory | 1.5d | 📋 |
| T3.3-12 | Python 参数系统测试 — 15+ 测试 | 1d | 📋 |

### 3.3 集成与文档 (2 天)

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:----:|
| T3.3-13 | 端到端集成测试 — Credit Flow 验证 | 1d | 📋 |
| T3.3-14 | 更新架构文档 — 参数框架设计 | 0.5d | 📋 |
| T3.3-15 | 更新用户指南 — 参数配置指南 | 0.5d | 📋 |

---

## 四、Phase 3.4 拓扑验证与工具链 — 部分完成 🟡

### 4.1 Python 端任务 (11.5 天)

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:----:|
| T3.4-01 | 创建 `cpptlm_config/validator.py` — TopologyValidator 整合 | 2d | 📋 |
| T3.4-02 | 孤立节点检测 — VALID-01 | 0.5d | 📋 |
| T3.4-03 | BFS 可达性检测 — VALID-02 | 0.5d | 📋 |
| T3.4-04 | 路由器端口方向验证 — PORT-01 | 1d | 📋 |
| T3.4-05 | Bundle 类型兼容性验证 — PORT-03 | 1d | 📋 |
| T3.4-06 | 两阶段验证集成 — build() 后自动调用 validator | 0.5d | 📋 |
| T3.4-07 | 静态负载分析 — VALID-05 | 1.5d | 📋 |
| T3.4-08 | 连接路径追踪 — TOOL-07 | 1.5d | 📋 |
| T3.4-09 | 配置 lint 工具 — TOOL-08 | 1d | 📋 |
| T3.4-14~17 | pyproject.toml/SemVer/端口组/可视化元数据验证 | 2d | 📋 |
| T3.4-10 | 拓扑分析工具测试 — 25+ 测试 | 1.5d | 📋 |

### 4.2 集成与文档 (1.5 天)

| 任务 ID | 任务描述 | 工作量 | 状态 |
|---------|---------|:---:|:----:|
| T3.4-11 | 端到端集成测试 | 0.5d | 📋 |
| T3.4-12 | 更新架构文档 — 验证工具链设计 | 0.5d | 📋 |
| T3.4-13 | 更新用户指南 — 验证工具使用指南 | 0.5d | 📋 |

---

## 五、技术债务 (P1/P2) — 待清理 ⏳

| ID | 任务 | 文件 | 工时 | 优先级 |
|----|------|------|------|--------|
| **P1.2** | latency 注入 bug | `src/core/connection_resolver.cc:44` | 4-6h | 🔴 |
| **P1.4** | 清理冗余配置文件 | `configs/` (mesh_2x2.json 等) | 1-2h | 🟡 |
| **P2.A** | linter 合并到 validator | `scripts/linter.py` → `cpptlm_config/` | 2-3h | 🟡 |
| **P2.B** | typedef→using 现代化 | `include/utils/mem_exts.hh` | 1-2h | 🟢 |
| **P2.C** | Legacy modules 审计 | `include/modules/legacy/` | 1-2h | 🟢 |
| **P2.D** | DPRINTF 日志一致性 | 全局 | 1-2h | 🟢 |

---

## 六、推荐执行计划

```
Week 1-2: Phase 3.3 C++ 端 (T3.3-01~06, T3.3-10~11)
Week 3:   Phase 3.3 Python 端 (T3.3-07~09, T3.3-12)
Week 4:   Phase 3.3 集成测试 + 文档 (T3.3-13~15)
Week 5-6: Phase 3.4 Python 端 (T3.4-01~10)
Week 7:   Phase 3.4 集成测试 + 文档 (T3.4-11~13)
Week 8:   技术债务清理 (P1.2, P1.4, P2.A~D)
```

---

## 七、OpenSpec Change 拆分建议

| 新 Change | 包含任务 | 预估工时 |
|-----------|---------|---------|
| `phase-3-3-param-system` | T3.3-01~15 (C++ + Python + 测试) | 4 周 |
| `phase-3-4-validation-toolchain` | T3.4-01~17 (验证器 + 工具链 + 测试) | 3 周 |
| `phase-3-tech-debt` | P1.2, P1.4, P2.A~D | 1 周 |

---

*文档生成时间: 2026-05-23*
*基于: docs/plan/ROADMAP.md, docs/plan/phase3.*-plan.md, docs/implementation/*, docs/adr/ADR-X.*, docs/REMAINING_WORK.md*
