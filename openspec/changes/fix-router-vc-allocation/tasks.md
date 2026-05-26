# Tasks: Fix RouterTLM VC Allocation

## 1. Analysis

- [x] 1.1 定位 `allocate_vc()` 调用
- [x] 1.2 确认未检查返回值

## 2. Implementation

- [x] 2.1 添加返回值检查
- [x] 2.2 处理分配失败情况

## 3. Testing

- [x] 3.1 添加 VC 分配测试

## 4. Verification

- [x] 4.1 运行 [router] 测试
- [x] 4.2 构建验证
