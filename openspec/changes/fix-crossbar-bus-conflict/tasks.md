# Tasks: Fix CrossbarTLM Bus Conflict

## 1. Analysis

- [x] 1.1 定位 `resp_out[dst].write()` 调用
- [x] 1.2 分析冲突场景

## 2. Implementation

- [x] 2.1 添加 port_busy[] 检测
- [x] 2.2 实现冲突检测逻辑

## 3. Testing

- [x] 3.1 添加总线冲突测试
- [x] 3.2 运行 crossbar 相关测试

## 4. Verification

- [x] 4.1 运行 [crossbar] 测试通过
- [x] 4.2 构建验证
