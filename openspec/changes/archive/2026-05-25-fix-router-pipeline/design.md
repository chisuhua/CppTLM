# RouterTLM Pipeline State Fix — Design

## Problem Detail

### Current Behavior

在 `router_tlm.hh` 的流水线处理逻辑中，每周期开始时直接覆盖 `pipe_reg_`：

```cpp
// 当前行为（推测）
pipe_reg_ = new_state;  // 直接覆盖，上周期状态丢失
```

### Desired Behavior

在每周期开始时保存上周期状态，处理完当前请求后合并状态：

```cpp
// 正确的行为
auto prev_wait_state = pipe_reg_;  // 保存上周期状态
// 处理当前请求...
pipe_reg_ = merge(prev_wait_state, current_result);  // 合并状态
```

## Implementation Approach

### Step 1: Identify Pipeline State

定位 `router_tlm.hh` 中的 `pipe_reg_` 和相关等待状态变量。

### Step 2: Analyze State Dependencies

理解哪些状态需要跨周期保持（等待状态），哪些可以覆盖（新的传输请求）。

### Step 3: Implement State Preservation

在 `router_tlm.cc` 或 `router_tlm.hh` 的流水线处理方法中：

1. 添加临时变量保存上周期状态
2. 修改状态更新逻辑，使用合并而非覆盖
3. 确保等待状态（如 VC 仲裁结果）不会被错误清除

### Step 4: Add Tests

添加或更新测试验证：
- 高流量场景下不会丢包
- 多 VC 场景下状态保持正确
- 流水线握手信号正确传播

## Verification

1. `./build/bin/cpptlm_tests "[router]"` 通过
2. `./build/bin/cpptlm_tests "[tlm]"` 通过
3. 新增特定测试验证修复
