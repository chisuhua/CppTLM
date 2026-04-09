# CppTLM (GemSc) — 开始这里

**最后更新**: 2026-04-07 | **版本**: v2.0

---

## 5 分钟了解 CppTLM

### 项目定位

> **CppTLM** 是一个 **Gem5 风格的事务级仿真框架**，目标是演进为 **多层次混合仿真平台**（TLM + RTL 协同仿真）。

```
现在：纯 TLM 仿真框架          目标：TLM + RTL 混合仿真
┌─────────────────────┐       ┌─────────────────────────────┐
│ SimObject 驱动      │       │ SimObject 驱动              │
│ SimplePort 通信     │   →   │ Port<T> 泛型端口            │
│ Packet 封装 TLM     │       │ + TLMToRTL 适配器           │
│ JSON 配置拓扑       │       │ + CppHDL RTL 集成           │
└─────────────────────┘       └─────────────────────────────┘
```

---

## 核心能力

| 能力 | 状态 | 说明 |
|------|------|------|
| **事务级仿真** | ✅ 成熟 | SimObject/EventQueue/PortPair 已实现 |
| **模块库** | ✅ 可用 | CPU/Cache/Memory/Crossbar/TrafficGen |
| **测试覆盖** | ✅ 完善 | 21 个 Catch2 测试用例 |
| **混合仿真** | 📋 设计中 | 5 维适配层方案已评审 |
| **CppHDL 集成** | ⏸️ 待实施 | CppHDL 项目已定位 |

---

## 文档导航

### 🚀 快速开始
- **新开发者**: 阅读 `docs/PROJECT_OVERVIEW.md` → `docs/BUILD_AND_TEST.md`
- **模块开发**: 阅读 `docs/MODULE_DEVELOPMENT_GUIDE.md`
- **API 查询**: 阅读 `docs/API_REFERENCE.md`

### 🏗️ 架构设计（混合仿真）
- **架构概览**: `docs/01-ARCHITECTURE/hybrid-modeling-overview.md`（待创建）
- **交易上下文**: `docs/01-ARCHITECTURE/transaction-context.md`（待创建）
- **适配器设计**: `docs/01-ARCHITECTURE/adapter-design.md`（待创建）

### 📋 实施计划
- **Phase 0**: 环境准备（进行中）
- **Phase 1**: 交易上下文与基础接口（待启动）
- **Phase 2**: 适配层核心（待启动）
- **Phase 3**: 双并行实现（待启动）

### 📚 历史文档
- **v1 设计**: `docs/05-LEGACY/v1-hybrid-design/`
- **v2 改进**: `docs/05-LEGACY/v2-improvements/`

---

## 代码结构

```
/workspace/CppTLM/
├── include/core/           # 核心引擎
│   ├── sim_object.hh       # SimObject 基类
│   ├── simple_port.hh      # SimplePort 接口
│   ├── packet.hh           # Packet 封装
│   ├── event_queue.hh      # 事件调度器
│   └── ...
├── include/modules/        # 标准模块
│   ├── cpu_sim.hh
│   ├── cache_sim.hh
│   └── memory_sim.hh
├── src/                    # 实现
├── test/                   # Catch2 测试
├── configs/                # JSON 配置示例
└── docs/                   # 文档
```

---

## 立即行动

### 开发者
1. 克隆项目
2. 阅读 `docs/BUILD_AND_TEST.md`
3. 运行示例：`./sim configs/cpu_example.json`

### 架构师
1. 阅读 `docs/01-ARCHITECTURE/`（待创建）
2. 审查混合仿真方案
3. 参与架构讨论

### 决策者
1. 阅读 `PROJECT_AUDIT_AND_NEXT_STEPS.md`
2. 确认 Phase 优先级
3. 分配资源

---

## 关键术语

| 术语 | 含义 |
|------|------|
| **TLM** | Transaction-Level Modeling，事务级建模 |
| **RTL** | Register-Transfer Level，寄存器传输级 |
| **SimObject** | 仿真模块基类，每周期调用 tick() |
| **SimplePort** | 端口接口，send/recv 通信 |
| **PortPair** | 双向连接，连接两个 SimplePort |
| **Packet** | 通信负载，封装 tlm_generic_payload |
| **EventQueue** | 事件调度器，驱动仿真时间推进 |

---

## 联系与反馈

- **项目位置**: `/workspace/CppTLM/`
- **CppHDL 位置**: `/workspace/CppHDL/`
- **项目记忆**: `~/.openclaw/workspace-project/CppTLM/session-memory.md`
- **审计报告**: `PROJECT_AUDIT_AND_NEXT_STEPS.md`

---

**准备好开始了吗？选择你的路径 →** 🚀
