# Tasks: Fix PacketPool Reference Counting

## 1. Analysis

- [x] 1.1 定位 `packet_pool.hh` 中的 ref_count 定义
- [x] 1.2 分析引用计数的使用场景
- [x] 1.3 确认 add_ref() 是死代码

## 2. Implementation

- [x] 2.1 将 `Packet::ref_count` 改为 `std::atomic<uint32_t>`
- [x] 2.2 删除 `add_ref()` 私有方法（死代码）
- [x] 2.3 确保 `remove_ref()` 逻辑正确

## 3. Testing

- [ ] 3.1 添加多线程安全测试
- [ ] 3.2 运行现有 pool 相关测试

## 4. Verification

- [ ] 4.1 运行 `[packet_pool]` 测试通过
- [ ] 4.2 运行 `[pool]` 测试通过
- [ ] 4.3 构建验证通过
