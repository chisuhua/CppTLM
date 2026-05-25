# Tasks: Fix CrossbarTLM Bus Conflict

## 1. Analysis

- [ ] 1.1 定位 `resp_out[dst].write()` 调用
- [ ] 1.2 分析冲突场景

## 2. Implementation

- [ ] 2.1 添加 port_busy[] 检测
- [ ] 2.2 实现冲突检测逻辑

## 3. Testing

- [ ] 3.1 添加总线冲突测试
- [ ] 3.2 运行 crossbar 相关测试

## 4. Verification

- [ ] 4.1 运行 [crossbar] 测试通过
- [ ] 4.2 构建验证
