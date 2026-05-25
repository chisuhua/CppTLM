# Tasks: Fix RouterTLM Pipeline State Clearing

## 1. Analysis

- [x] 1.1 定位 `router_tlm.hh` 中的 `pipe_reg_` 定义
- [x] 1.2 分析流水线处理方法的调用时序
- [x] 1.3 识别需要跨周期保持的状态字段

## 2. Implementation

- [x] 2.1 添加 `waiting_for_credit` 标志到 `RouterStageState`
- [x] 2.2 扩展 `PendingFlit` 结构包含完整状态
- [x] 2.3 实现 `pipe_reg_` 保存等待 credit 的 flit
- [x] 2.4 修改 `stage_switch_traversal()` 检查下游 credit
- [x] 2.5 实现等待 flit 跨周期恢复逻辑

## 3. Testing

- [ ] 3.1 添加或更新 RouterTLM 流水线测试
- [ ] 3.2 验证高流量场景不丢包
- [ ] 3.3 验证多 VC 场景状态正确

## 4. Verification

- [ ] 4.1 运行 `[router]` 测试通过
- [ ] 4.2 运行 `[tlm]` 测试通过
- [ ] 4.3 构建验证通过
