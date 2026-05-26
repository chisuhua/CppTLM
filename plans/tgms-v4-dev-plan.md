# TGMS v4.0 开发计划

> **文档编号**: TGMS-V4-PLAN
> **版本**: v1.0
> **创建日期**: 2026-05-27
> **基于**: IMPL-010-v4.0, docs/implementation/11-tgms-v4-implementation-plan.md
> **状态**: 🟡 规划中

---

## 0. 当前进度摘要

> **最后更新**: 2026-05-27

### TGMS v3.0 ✅ 已完成 (Phase 1-3)

| 任务 | 说明 | 状态 |
|------|------|------|
| Phase 1 | 核心基础设施 (set_config, ModuleFactory Step 2.5) | ✅ 完成 |
| Phase 2 | Python 工具链 (端口索引生成, 工具链整合) | ✅ 完成 |
| Phase 3 | 验证与示例 (2x2/4x4 Mesh, TopologyValidator) | ✅ 完成 |

### TGMS v4.0 ❌ 未开始 (Phase 4-6)

| Phase | 任务 | 状态 |
|-------|------|------|
| Phase 4 | 层次核心 (Hierarchy tree, CoherenceDomain) | 🟡 规划中 |
| Phase 5 | 协议桥 (ProtocolBridge) | 🟡 规划中 |
| Phase 6 | 多集群 SoC 验证 | 🟡 规划中 |

---

## 1. Phase 4: 层次核心 (3-4 周)

### 1.1 任务分解

| Task | Gap | 工作量 | 依赖 | 并行机会 | 状态 |
|------|-----|--------|------|----------|------|
| 4.1 Hierarchy tree parser | G8 | 3 天 | Phase 1 | - | 🟡 |
| 4.2 CoherenceDomain C++ module | G9 | 5 天 | 4.1 | - | 🟡 |
| 4.3 Domain boundary validation | G9 | 2 天 | 4.2 | - | 🟡 |
| 4.4 Snoop routing logic | G9 | 3 天 | 4.2 | 与 4.3 并行 | 🟡 |
| 4.5 Directory protocol stub | G9 | 5 天 | 4.2 | 与 4.3/4.4 并行 | 🟡 |
| 4.6 Python hierarchy generator | G8 | 5 天 | 4.1-4.3 | - | 🟡 |

### 1.2 执行顺序

```
Week 1:
  Day 1-3: 4.1 Hierarchy tree parser

Week 2:
  Day 1-5: 4.2 CoherenceDomain C++ module

Week 3:
  Day 1-2: 4.3 Domain boundary validation
  Day 3-5: 4.4 Snoop routing logic (并行)

Week 4:
  Day 1-5: 4.5 Directory protocol stub (并行 with 4.4)
  Day 1-5: 4.6 Python hierarchy generator (可在 4.1 后独立开始)
```

### 1.3 验收标准

- [ ] `hierarchy` JSON 对象正确解析为树结构
- [ ] CoherenceDomain 类支持 MESI/MOESI 协议
- [ ] 跨域连接无桥接时被拒绝
- [ ] Snoop 消息到达所有域成员
- [ ] 基于目录的主节点查找正确
- [ ] Python hierarchy generator 输出正确的 v4.0 JSON

---

## 2. Phase 5: 协议桥 (2-3 周)

### 2.1 任务分解

| Task | Gap | 工作量 | 依赖 | 并行机会 | 状态 |
|------|-----|--------|------|----------|------|
| 5.1 ProtocolBridge C++ module | G10 | 5 天 | Phase 4 | - | 🟡 |
| 5.2 Address translation engine | G10 | 3 天 | 5.1 | - | 🟡 |
| 5.3 Protocol conversion logic | G10 | 5 天 | 5.1 | 与 5.2 并行 | 🟡 |
| 5.4 Cross-protocol validation | G10 | 3 天 | 5.1-5.3 | - | 🟡 |
| 5.5 Python bridge config generator | G10 | 3 天 | 5.1-5.4 | - | 🟡 |

### 2.2 执行顺序

```
Week 1:
  Day 1-5: 5.1 ProtocolBridge C++ module

Week 2:
  Day 1-3: 5.2 Address translation engine
  Day 1-5: 5.3 Protocol conversion logic (并行)

Week 3:
  Day 1-3: 5.4 Cross-protocol validation
  Day 1-3: 5.5 Python bridge config generator (并行)
```

### 2.3 验收标准

- [ ] ProtocolBridge 类功能完整
- [ ] 地址翻译引擎工作正常
- [ ] 协议转换逻辑正确 (MESI ↔ MOESI)
- [ ] 跨协议验证正常
- [ ] Python bridge config generator 输出正确配置

---

## 3. Phase 6: 多集群 SoC 验证 (2-3 周)

### 3.1 任务分解

| Task | Gap | 工作量 | 依赖 | 并行机会 | 状态 |
|------|-----|--------|------|----------|------|
| 6.1 2x CPU + GPU Cluster config | G8, G9 | 3 天 | Phase 4, 5 | - | 🟡 |
| 6.2 Cross-cluster coherence validation | G9 | 5 天 | 6.1 | - | 🟡 |
| 6.3 Protocol bridge integration test | G10 | 3 天 | 6.1 | 与 6.2 并行 | 🟡 |
| 6.4 Full SoC example (4 CPU + 1 GPU) | All | 5 天 | 6.2-6.3 | - | 🟡 |

### 3.2 执行顺序

```
Week 1:
  Day 1-3: 6.1 2x CPU + GPU Cluster config

Week 2:
  Day 1-5: 6.2 Cross-cluster coherence validation
  Day 1-3: 6.3 Protocol bridge integration test (并行)

Week 3:
  Day 1-5: 6.4 Full SoC example (4 CPU + 1 GPU)
```

### 3.3 验收标准

- [ ] 2x CPU + GPU cluster 配置创建
- [ ] 跨集群一致性验证通过
- [ ] 协议桥集成测试通过
- [ ] 完整 SoC 示例 (4 CPU + 1 GPU) 工作正常

---

## 4. 关键路径 (Critical Path)

```
4.1 → 4.2 → 4.3 → 5.1 → 5.4 → 6.2 → 6.4
                      → 5.2 → 5.3 ↗
                      → 5.5 ↗
               → 6.1 → 6.3 ↗
```

**最短工期估算**: 8-10 周

---

## 5. 新建文件清单

| 文件 | 用途 |
|------|------|
| `include/core/coherence_domain.hh` | CoherenceDomain 类声明 |
| `src/core/coherence_domain.cc` | CoherenceDomain 实现 |
| `include/core/directory.hh` | Directory 类声明 |
| `src/core/directory.cc` | Directory 实现 |
| `include/core/protocol_bridge.hh` | ProtocolBridge 类声明 |
| `src/core/protocol_bridge.cc` | ProtocolBridge 实现 |
| `test/test_coherence_domain.cc` | CoherenceDomain 单元测试 |
| `test/test_protocol_bridge.cc` | ProtocolBridge 单元测试 |

---

## 6. 修改文件清单

| 文件 | 变更 |
|------|------|
| `src/core/module_factory.cc` | 添加 hierarchy 解析 (Step 0), coherence domain 集成 |
| `scripts/topology_generator.py` | 添加 HierarchicalTopologyGenerator, ProtocolBridgeGenerator |

---

## 7. 下一步行动

1. **创建 worktree**: 使用 `using-git-worktrees` skill 创建隔离开发环境
2. **Phase 4.1**: 开始实现 Hierarchy tree parser
3. **更新 ROADMAP**: 同步文档状态

---

## 8. 参考文档

- [11-tgms-v4-implementation-plan.md](../docs/implementation/11-tgms-v4-implementation-plan.md) - 详细实现计划
- [10-topology-generation-management-system.md](../docs/architecture/10-topology-generation-management-system.md) - 架构参考
- [10-tgms-specifications.md](../docs/architecture/10-tgms-specifications.md) - 规格说明