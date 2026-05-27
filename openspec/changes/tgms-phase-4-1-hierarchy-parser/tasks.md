# Tasks: TGMS v4.0 Phase 4.1 - Hierarchy Tree Parser

> **Status**: 🔄 进行中
> **Last Updated**: 2026-05-27
> **Worktree**: `.zcf/tgms-phase-4-1-hierarchy-parser-wt`

## 1. Analysis

- [x] 1.1 分析 v4.0 hierarchy JSON 格式
- [x] 1.2 设计 TopologyNode 类结构
- [x] 1.3 规划 ModuleFactory 集成方案

## 2. Implementation

- [x] 2.1 创建 `include/core/topology_node.hh` (TopologyNode 类)
- [x] 2.2 创建 `include/core/topology_parser.hh` (声明 parse_hierarchy_tree 等)
- [x] 2.3 实现 `src/core/topology_parser.cc` (parse_hierarchy_tree_with_validation)
- [x] 2.4 添加 coherence_domains 解析支持
- [ ] 2.5 在 ModuleFactory 添加 Step 0 (hierarchy 解析调用)
- [ ] 2.6 在 ModuleFactory 添加 coherence_domain 关联

## 3. Testing

- [ ] 3.1 单元测试: TopologyNode 基本功能
- [ ] 3.2 单元测试: hierarchy JSON 解析 (parse_hierarchy_tree)
- [ ] 3.3 单元测试: coherence_domains 解析
- [ ] 3.4 单元测试: 循环引用检测

## 4. Verification

- [ ] 4.1 构建验证 (cmake --build build)
- [ ] 4.2 所有测试通过 (ctest --output-on-failure)

## 5. Documentation

- [ ] 5.1 更新 docs/implementation/11-tgms-v4-implementation-plan.md

## 参考文档

- `docs/implementation/11-tgms-v4-implementation-plan.md` - TGMS v4.0 详细实现计划
- `plans/tgms-v4-dev-plan.md` - TGMS v4.0 开发计划

## 实现说明

### 已完成
- TopologyNode 类: `include/core/topology_node.hh`
- TopologyParser 声明: `include/core/topology_parser.hh`
- TopologyParser 实现: `src/core/topology_parser.cc`
  - parse_hierarchy_tree()
  - parse_hierarchy_tree_with_validation()
  - parse_coherence_domains_array()
  - detect_circular_reference()

### 待完成
- ModuleFactory 集成 (Step 0)
- 单元测试
- 构建验证