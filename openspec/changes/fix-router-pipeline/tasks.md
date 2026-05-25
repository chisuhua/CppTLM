# Tasks: Fix RouterTLM Pipeline State Clearing

## 1. Analysis

- [ ] 1.1 定位 `router_tlm.hh` 中的 `pipe_reg_` 定义
- [ ] 1.2 分析流水线处理方法的调用时序
- [ ] 1.3 识别需要跨周期保持的状态字段

## 2. Implementation

- [ ] 2.1 在流水线处理方法开始时保存上周期状态
- [ ] 2.2 修改状态更新逻辑，使用合并而非直接覆盖
- [ ] 2.3 确保等待状态（VC 仲裁结果）正确保持

## 3. Testing

- [ ] 3.1 添加或更新 RouterTLM 流水线测试
- [ ] 3.2 验证高流量场景不丢包
- [ ] 3.3 验证多 VC 场景状态正确

## 4. Verification

- [ ] 4.1 运行 `[router]` 测试通过
- [ ] 4.2 运行 `[tlm]` 测试通过
- [ ] 4.3 构建验证通过
