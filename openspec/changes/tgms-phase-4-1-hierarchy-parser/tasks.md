# Tasks: TGMS v4.0 Phase 4.1 - Hierarchy Tree Parser

## 1. Analysis

- [ ] 1.1 分析 v4.0 hierarchy JSON 格式
- [ ] 1.2 设计 TopologyNode 类结构
- [ ] 1.3 规划 ModuleFactory 集成方案

## 2. Implementation

- [ ] 2.1 创建 `include/core/topology_node.hh` (TopologyNode 类)
- [ ] 2.2 实现 `parse_hierarchy_tree()` 函数
- [ ] 2.3 在 ModuleFactory 添加 Step 0 (hierarchy 解析)
- [ ] 2.4 添加 coherence_domains 解析支持

## 3. Testing

- [ ] 3.1 单元测试: TopologyNode 基本功能
- [ ] 3.2 单元测试: hierarchy JSON 解析
- [ ] 3.3 集成测试: ModuleFactory hierarchy 解析

## 4. Verification

- [ ] 4.1 构建验证
- [ ] 4.2 所有测试通过

## 参考文档

- `docs/implementation/11-tgms-v4-implementation-plan.md` - TGMS v4.0 详细实现计划
- `plans/tgms-v4-dev-plan.md` - TGMS v4.0 开发计划