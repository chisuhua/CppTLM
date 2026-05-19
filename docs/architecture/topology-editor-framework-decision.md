# 拓扑编辑器前端框架决策

## 1. 背景

TD-004: Alpine.js 用于拓扑编辑器（能力不足）

## 2. 候选框架

| 框架 | 包体积 | 状态管理 | 拖拽库 | 备注 |
|------|--------|---------|--------|------|
| Svelte | ~20KB | 内置Store | svelte-dnd-action | ⭐ 推荐 |
| React | ~40KB | Zustland/Redux | react-dnd, @dnd-kit | 生态最大 |
| Vue 3 | ~30KB | Pinia | vue-draggable | 渐进式 |
| Alpine.js | ~0KB | 不足 | alpine-dnd | ❌ 不适用 |

## 3. 推荐方案

**推荐: Svelte**

理由：
1. 包体积最小（~20KB gzip）
2. 编译时优化，无运行时开销
3. 内置响应式 Store，适合复杂状态
4. 语法简洁，类似 Vue 但更简洁

## 4. 实施计划

1. 创建 `cpptlm/visualization/editor/` 目录
2. 使用 Vite 创建 Svelte 项目
3. 集成 svelte-dnd-action
4. 实现核心编辑器组件

## 5. 风险

- Svelte 生态较小，招聘可能较难
- 团队可能需要学习 Svelte 语法