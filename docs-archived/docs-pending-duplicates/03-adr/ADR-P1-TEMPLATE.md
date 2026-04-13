# ADR-P1: 混合仿真架构 P1 级决策

> **版本**: 1.1  
> **日期**: 2026-04-09  
> **状态**: ✅ 已确认  
> **关联**: PRD-002, 架构 v2.0

---

## P1 级 ADR 议题列表

| ADR 编号 | 议题 | 优先级 | 状态 |
|---------|------|--------|------|
| **ADR-P1.1** | Bundle 共享策略 | P1 | ✅ 已确认 |
| **ADR-P1.2** | 多端口声明方式 | P1 | ✅ 已确认 |
| **ADR-P1.3** | Adapter 泛型设计 | P1 | ✅ 已确认 |
| **ADR-P1.4** | Fragment/Mapper 层设计 | P1 | ✅ 已确认 |
| **ADR-P1.5** | 双并行模式支持策略 | P1 | ✅ 已确认 |

---

## ADR-P1.1: Bundle 共享策略

### 问题

TLM 模块和 RTL 模块需要交换数据，Bundle 定义应该：
- A) TLM/RTL 各自定义，通过 Mapper 转换
- B) 统一共享一份定义，TLM/RTL 共用

### 选项对比

| 选项 | 优点 | 缺点 |
|------|------|------|
| A) 各自定义 | TLM/RTL 可独立演进 | 需要 Mapper 转换，重复定义 |
| **B) 统一共享** ✅ | 一份定义，减少重复 | TLM/RTL 耦合，变更需协调 |

### 决策

✅ **选项 B) 统一共享**

**理由**:
- 减少代码重复
- TLM/RTL 接口一致性保证
- Bundle 变更时自动同步

**实施**: 所有 Bundle 定义放在 `include/bundles/`，TLM/RTL 共用。

---

## ADR-P1.2: 多端口声明方式

### 问题

Crossbar/NoC Switch 等模块需要多个输入/输出端口，如何声明 `ch_stream<T>`？

### 选项对比

| 选项 | 示例 | 优点 | 缺点 |
|------|------|------|------|
| **A) 数组方式** ✅ | `std::array<ch_stream<T>, N> req_in;` | 简洁，可迭代访问 | 端口数量固定 |
| B) 逐个声明 | `ch_stream<T> req_in_0, req_in_1, ...;` | 每个端口独立命名 | 代码冗长，不易迭代 |
| C) 动态映射表 | `std::map<std::string, ch_stream<T>*> ports;` | 端口数量动态 | 运行时开销，类型不安全 |

### 决策

✅ **选项 A) 数组方式**

**理由**:
- 简洁，支持迭代访问
- 编译时类型安全
- 端口数量固定符合硬件特性

**实施**: 所有多端口模块使用 `std::array<ch_stream<T>, N>` 声明。

---

## ADR-P1.3: Adapter 泛型设计

### 问题

`StreamAdapter` 需要支持多种 Bundle 类型（Cache/NoC/AXI4），如何设计？

### 选项对比

| 选项 | 设计 | 优点 | 缺点 |
|------|------|------|------|
| A) 模板特化 | 每个 Bundle 类型一个特化 | 编译时类型安全 | 代码重复，新增类型需新代码 |
| B) 类型擦除 | `void* stream_ptr_` + `std::function` | 支持动态 Bundle 类型 | 运行时开销，调试困难 |
| **C) 混合方案** ✅ | 泛型基类 + 模板特化 | 平衡灵活性与性能 | 复杂度增加 |

### 决策

✅ **选项 C) 混合方案**

**理由**:
- 常用 Bundle（Cache/NoC）用模板特化，性能优先
- 罕见 Bundle 用类型擦除，灵活性优先
- `ModuleRegistry` 根据 Bundle 类型自动选择

**实施**: 
- `GenericInputStreamAdapter` / `GenericOutputStreamAdapter` 使用模板 + 类型擦除
- 常用 Bundle 的 decode/encode 函数模板特化

---

## ADR-P1.4: Fragment/Mapper 层设计

### 问题

NoC 分片传输（Fragment）和协议映射（AXI4/CHI）是否需要独立 Mapper 层？

### 选项对比

| 选项 | 设计 | 优点 | 缺点 |
|------|------|------|------|
| A) 无 Mapper 层 | 模块内部直接处理分片/协议 | 简单，无额外抽象 | 模块逻辑复杂，重复代码 |
| **B) 独立 Mapper 层** ✅ | FragmentMapper/AXI4Mapper 独立 | 职责分离，可复用 | 增加一层抽象 |
| C) 可选 Mapper | 模块可选择是否使用 Mapper | 灵活性高 | 复杂度增加 |

### 决策

✅ **选项 B) 独立 Mapper 层**

**理由**:
- 职责分离：模块专注业务，Mapper 专注转换
- 可复用：FragmentMapper 可被多个 NoC 模块使用
- 可测试：Mapper 独立测试

**实施**: 
- `include/mapper/` 目录
- `FragmentMapper`, `AXI4Mapper`, `CHIMapper` 独立实现

---

## ADR-P1.5: 双并行模式支持策略

### 问题

双并行模式（COMPARE/SHADOW）是否在 v2.0 中实现？

### 选项对比

| 选项 | 策略 | 优点 | 缺点 |
|------|------|------|------|
| A) v2.0 实现 | 在 ModuleRegistry 中支持 impl_type 配置 | 功能完整 | 复杂度增加，延迟 v2.0 发布 |
| **B) v2.1 实现** ✅ | v2.0 仅支持 TLM/RTL，v2.1 添加 COMPARE/SHADOW | v2.0 快速发布 | 功能不完整 |
| C) 框架外实现 | 双并行由外部测试框架处理 | 框架简化 | 需要额外工具 |

### 决策

✅ **选项 B) v2.1 实现**

**理由**:
- v2.0 聚焦核心架构（ch_stream 统一 + Bundle 共享）
- COMPARE/SHADOW 是增值功能，非核心需求
- v2.1 可在稳定架构上添加

**实施**: 
- v2.0: 实现 TLM/RTL 核心架构
- v2.1: 在 `include/framework/impl_mode.hh` 添加 COMPARE/SHADOW 支持

---

## 决策汇总

| ADR | 推荐选项 | 确认状态 | 状态 |
|-----|---------|---------|------|
| P1.1 Bundle 共享 | B) 统一共享 | ✅ 已确认 | ✅ 已实施 |
| P1.2 多端口声明 | A) 数组方式 | ✅ 已确认 | ✅ 已实施 |
| P1.3 Adapter 泛型 | C) 混合方案 | ✅ 已确认 | ✅ 已实施 |
| P1.4 Fragment/Mapper | B) 独立 Mapper 层 | ✅ 已确认 | ✅ 已实施 |
| P1.5 双并行模式 | B) v2.1 实现 | ✅ 已确认 | ✅ 已实施 |

---

**下一步**: 
1. ✅ P1 级议题已全部确认
2. 📋 检查是否还有其他议题需要讨论
3. 📋 准备 ADR-P2 级议题（置信度评分、回归测试、验收标准）
