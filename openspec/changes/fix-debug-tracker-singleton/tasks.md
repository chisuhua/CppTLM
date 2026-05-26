# Tasks: Fix DebugTracker Singleton

## 1. Analysis

- [x] 1.1 定位 `if (initialized_) return;`

## 2. Implementation

- [x] 2.1 使用 std::call_once 或 mutex

## 3. Testing

- [x] 3.1 多线程测试

## 4. Verification

- [x] 4.1 构建验证
