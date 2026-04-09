# CppTLM v2.0 实施计划（修订版）

> **版本**: 2.0  
> **创建日期**: 2026-04-10  
> **状态**: 🚀 实施中  
> **原则**: 零技术债务，测试先行，持续验证

---

## 一、实施原则（世界级工程师标准）

### 1. 零债务原则
- 每个 Phase 完成后立即验证
- 不积累 TODO、FIXME 注释
- 代码与文档同步更新
- 每个提交都可独立编译测试

### 2. 测试先行原则
- 每个功能先写测试
- 测试覆盖率 >80%
- 测试作为文档的一部分
- 回归测试自动化

### 3. 架构纯洁性
- 严格遵循 ADR 决策
- 不引入未批准的变更
- 保持向后兼容
- 清晰的模块边界

### 4. 持续验证
- 每个文件修改后执行 clang-check
- 每个 Phase 完成后运行测试
- 保持 main 分支始终可编译
- 失败立即回滚

---

## 二、实施阶段总览

```
┌─────────────────────────────────────────────────────────────┐
│  Phase 0: 原型验证 (1 天) ✅ 进行中                          │
│  - TransactionContextExt clone/copy 验证                    │
│  - Extension 与 Packet 同步验证                             │
│  - Tracker 单例基本功能验证                                 │
├─────────────────────────────────────────────────────────────┤
│  Phase 1: 构建系统改进 (1 天) ⏳ 待开始                      │
│  - CMakeLists.txt 重构 + ccache                             │
│  - PortPair 连接机制                                        │
├─────────────────────────────────────────────────────────────┤
│  Phase 2: 核心基础扩展 (2 天) ⏳ 待开始                      │
│  - SimObject::reset() + snapshot                            │
│  - ErrorCode 定义                                            │
│  - ModuleRegistry                                           │
│  - ResetCoordinator                                         │
├─────────────────────────────────────────────────────────────┤
│  Phase 3: 交易处理架构 (2 天) ⏳ 待开始                      │
│  - TransactionContextExt                                    │
│  - Packet::transaction_id 扩展                              │
│  - TransactionTracker                                       │
│  - StreamAdapter 泛型                                       │
├─────────────────────────────────────────────────────────────┤
│  Phase 4: 错误处理架构 (2 天) ⏳ 待开始                      │
│  - ErrorContextExt                                          │
│  - DebugTracker                                             │
│  - FragmentMapper                                           │
├─────────────────────────────────────────────────────────────┤
│  Phase 5: 模块升级 (2 天) ⏳ 待开始                          │
│  - CacheV2 (1 天)                                            │
│  - CrossbarV2 (0.5 天)                                       │
│  - MemoryV2 (0.5 天)                                        │
├─────────────────────────────────────────────────────────────┤
│  Phase 6: 测试与示例 (2 天) ⏳ 待开始                        │
│  - 4 个新测试                                                │
│  - 2 个示例程序                                             │
│  - 迁移指南文档                                             │
└─────────────────────────────────────────────────────────────┘

总计：12 天
```

---

## 三、当前状态

### 基线信息

| 项目 | 值 |
|------|-----|
| **Git 分支** | main |
| **最新提交** | f3a1615 chore: 添加 .gitignore 文件 |
| **构建系统** | CMake 3.28 + Make |
| **C++ 标准** | C++17 |
| **测试框架** | Catch2 v3.5.0 (FetchContent) |

### Phase 0 探索任务

| 任务 | 状态 | Session ID |
|------|------|------------|
| SimObject 现状分析 | 🔄 进行中 | ses_28cdf2467ffesGdLJysMiNr7k2 |
| Packet 现状分析 | 🔄 进行中 | ses_28cdf2435ffe6QX5GFFqKL3zWB |
| TLM Extension 现状分析 | 🔄 进行中 | ses_28cdf2412ffeDe2FG2idG9GYek |
| CMakeLists.txt 现状分析 | 🔄 进行中 | ses_28cdf23f4ffe9QBvot9E0dJXo3 |

---

## 四、验收清单

### Phase 0 验收清单

- [ ] P0.1: TransactionContextExt 可正确 clone/copy
- [ ] P0.2: Extension 与 Packet 同步机制验证通过
- [ ] P0.3: Tracker 单例可正常创建和访问
- [ ] P0.4: 原型测试代码通过编译

### Phase 1 验收清单

- [ ] P1.1: CMakeLists.txt 重构完成，项目名改为 CppTLM
- [ ] P1.2: ccache 自动检测并启用
- [ ] P1.3: CI 配置完成
- [ ] P1.4: 所有现有代码编译通过

### Phase 2 验收清单

- [ ] P2.1: SimObject 扩展 reset/snapshot 完成
- [ ] P2.2: ErrorCode 定义完成
- [ ] P2.3: ModuleRegistry 实现完成
- [ ] P2.4: ResetCoordinator 实现完成
- [ ] P2.5: 所有单元测试通过

### Phase 3 验收清单

- [ ] P3.1: TransactionContextExt 实现完成
- [ ] P3.2: Packet 扩展完成
- [ ] P3.3: TransactionTracker 实现完成
- [ ] P3.4: StreamAdapter 泛型实现完成
- [ ] P3.5: 所有单元测试通过

### Phase 4 验收清单

- [ ] P4.1: ErrorContextExt 实现完成
- [ ] P4.2: DebugTracker 实现完成
- [ ] P4.3: FragmentMapper 实现完成
- [ ] P4.4: 所有单元测试通过

### Phase 5 验收清单

- [ ] P5.1: CacheV2 实现完成
- [ ] P5.2: CrossbarV2 实现完成
- [ ] P5.3: MemoryV2 实现完成
- [ ] P5.4: 模块升级测试通过

### Phase 6 验收清单

- [ ] P6.1: 4 个新增测试完成
- [ ] P6.2: 2 个示例程序完成
- [ ] P6.3: 迁移指南文档完成
- [ ] P6.4: 所有测试通过率 100%

---

## 五、文件变更清单

### 新建文件（按 Phase 分类）

| Phase | 文件 | 内容 |
|-------|------|------|
| P0 | `test/test_extension_prototype.cc` | 原型验证测试 |
| P1 | `.github/workflows/ci.yml` | CI 配置 |
| P2 | `include/framework/error_category.hh` | ErrorCode 定义 |
| P2 | `include/framework/module_registry.hh` | ModuleRegistry |
| P2 | `include/framework/reset_coordinator.hh` | ResetCoordinator |
| P3 | `include/ext/transaction_context_ext.hh` | TransactionContextExt |
| P3 | `include/framework/transaction_tracker.hh` | TransactionTracker |
| P3 | `include/framework/stream_adapter.hh` | StreamAdapter |
| P4 | `include/ext/error_context_ext.hh` | ErrorContextExt |
| P4 | `include/framework/debug_tracker.hh` | DebugTracker |
| P4 | `include/mapper/fragment_mapper.hh` | FragmentMapper |
| P5 | `include/modules/cache_v2.hh` | CacheV2 |
| P5 | `include/modules/crossbar_v2.hh` | CrossbarV2 |
| P5 | `include/modules/memory_v2.hh` | MemoryV2 |
| P6 | `docs/MIGRATION_GUIDE.md` | 迁移指南 |

### 修改文件（按 Phase 分类）

| Phase | 文件 | 变更 |
|-------|------|------|
| P1 | `CMakeLists.txt` | 重构为 CppTLM 项目 |
| P1 | `src/CMakeLists.txt` | 更新目标名 |
| P2 | `include/core/sim_object.hh` | 添加 reset/snapshot |
| P3 | `include/core/packet.hh` | 添加 transaction_id/error_code |
| P5 | `include/modules/cache_sim.hh` | 升级为 CacheV2 |
| P5 | `include/modules/crossbar.hh` | 升级为 CrossbarV2 |
| P5 | `include/modules/memory_sim.hh` | 升级为 MemoryV2 |

---

## 六、风险评估与缓解

| 风险 | 概率 | 影响 | 缓解措施 | 状态 |
|------|------|------|---------|------|
| Extension 机制复杂度高 | 中 | 高 | Phase 0 原型验证 | ✅ 处理中 |
| 向后兼容破坏 | 中 | 高 | 每个 Phase 运行现有测试 | ✅ 已计划 |
| 测试覆盖率不足 | 低 | 中 | 测试先行政策 | ✅ 已计划 |
| Phase 依赖链断裂 | 低 | 高 | 每个 Phase 独立可验证 | ✅ 已计划 |

---

## 七、决策日志

### 2026-04-10 决策

| 时间 | 决策 | 理由 |
|------|------|------|
| 09:00 | 使用 Make 而非 Ninja | 系统未安装 ninja，apt 无法获取 |
| 09:00 | 启动 4 个并行探索任务 | 充分理解现有代码后再实施 |
| 09:00 | 创建实施计划文档 | 保持文档与实施同步 |

---

## 八、下一步行动

### 当前：等待探索任务完成

等待以下任务完成（预计 5-10 分钟）：
- SimObject 现状分析
- Packet 现状分析
- TLM Extension 现状分析
- CMakeLists.txt 现状分析

### 探索完成后：

1. **审查探索结果** - 确认理解正确
2. **开始 Phase 0** - 原型验证
   - 创建 TransactionContextExt 原型
   - 编写原型测试
   - 验证 clone/copy 机制
3. **验证 Phase 0** - 运行测试，确认通过

---

## 九、质量指标

### 代码质量

- [ ] clang-tidy 检查通过
- [ ] clang-format 格式正确
- [ ] 无编译警告
- [ ] 无内存泄漏（ASan 验证）

### 测试质量

- [ ] 单元测试覆盖率 >80%
- [ ] 所有现有测试通过
- [ ] 新增测试通过
- [ ] CI 自动化

### 文档质量

- [ ] 每个类有 Doxygen 注释
- [ ] 每个方法有参数说明
- [ ] 每个 Phase 有完成报告
- [ ] 架构文档更新

---

**版本**: 1.0  
**最后更新**: 2026-04-10  
**下次更新**: Phase 0 完成后
