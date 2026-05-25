# PacketPool Reference Count Fix — Design

## Problem Detail

### Current Implementation

```cpp
// Packet 内的 ref_count
class Packet {
    int ref_count = 0;  // 手动计数，非原子
};

// PacketPool 私有方法
void add_ref(Packet* pkt) {
    if (pkt) pkt->ref_count++;  // 私有死代码
}

void remove_ref(Packet* pkt) {
    if (!pkt || pkt->ref_count <= 0) return;
    pkt->ref_count--;
    if (pkt->ref_count == 0) release(pkt);
}
```

### Issues

1. `add_ref()` 是私有死代码 — 从未被任何调用方使用
2. `ref_count` 是普通 `int`，多线程下非线程安全
3. 即使 `remove_ref()` 有锁保护，如果多线程同时 `acquire()` 并修改 ref_count，仍可能竞态

### Proposed Fix

1. 将 `Packet::ref_count` 改为 `std::atomic<uint32_t>`
2. 移除或标记 `add_ref()` 为 delete（死代码）
3. 保持 `remove_ref()` 的锁逻辑（内部已加锁）

## Implementation

### Step 1: 修改 Packet class

```cpp
class Packet {
    std::atomic<uint32_t> ref_count{0};  // 原子计数
    // ...
};
```

### Step 2: 清理死代码

```cpp
// 删除 add_ref() 私有方法（死代码）
// void add_ref(Packet* pkt) { ... } // 已删除
```

### Step 3: 确保 release() 正确

`release()` 已有 `std::lock_guard<std::mutex>` 保护，无需额外修改。

## Verification

1. `./build/bin/cpptlm_tests "[packet_pool]"` 通过
2. `./build/bin/cpptlm_tests "[pool]"` 通过
3. 多线程测试验证
