# CppTLM v2.0 开发者指南

> **版本**: 1.0  
> **创建日期**: 2026-04-10  
> **适用对象**: CppTLM 框架开发者、模块开发者

---

## 一、快速开始

### 1.1 环境要求

```bash
# 必需
- CMake >= 3.16
- C++17 兼容编译器 (GCC 9+, Clang 10+)
- ccache (可选，推荐)

# 可选
- Ninja (加速构建)
- SystemC (用于混合仿真)
```

### 1.2 构建流程

```bash
# 1. 配置
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON

# 2. 构建
cmake --build . -j4

# 3. 测试
ctest --output-on-failure
```

---

## 二、核心概念

### 2.1 SimObject - 所有模块的基类

```cpp
class MyModule : public SimObject {
public:
    MyModule(const std::string& name, EventQueue* eq)
        : SimObject(name, eq) {}
    
    void tick() override {
        // 每个周期执行的逻辑
    }
    
    void do_reset(const ResetConfig& config) override {
        // 复位逻辑（可选重写）
    }
};
```

**关键特性**:
- `tick()`: 纯虚函数，必须实现
- `reset()`: 层次化复位（支持递归子模块）
- `save_snapshot()`: 状态快照支持

### 2.2 TLM Extension 机制

**创建 Extension**:

```cpp
struct MyExtension : public tlm::tlm_extension<MyExtension> {
    int value;
    
    tlm_extension* clone() const override {
        return new MyExtension(*this);
    }
    
    void copy_from(const tlm_extension_base& ext) override {
        const auto& other = static_cast<const MyExtension&>(ext);
        value = other.value;
    }
};
```

### 2.3 错误码体系

| 类别 | 错误码范围 | 示例 |
|------|----------|------|
| TRANSPORT | 0x01xx | 无效地址、超时 |
| RESOURCE | 0x02xx | 缓冲区满 |
| COHERENCE | 0x03xx | 状态违例、死锁 |
| PROTOCOL | 0x04xx | ID 冲突、乱序 |

---

## 三、开发规范

### 3.1 测试先行原则

```cpp
// 1. 先写测试
TEST_CASE("MyFeature", "[feature]") {
    // 测试代码
}

// 2. 构建确认失败
cmake --build . && ctest

// 3. 实现功能

// 4. 构建验证通过
cmake --build . && ctest
```

### 3.2 错误处理模式

```cpp
if (error_condition) {
    pkt->set_error_code(ErrorCode::COHERENCE_STATE_VIOLATION);
    
    DebugTracker::instance().record_error(
        pkt->payload,
        ErrorCode::COHERENCE_STATE_VIOLATION,
        "State mismatch",
        get_name()
    );
}
```

---

## 四、调试技巧

### 4.1 启用 DebugTracker

```cpp
auto& tracker = DebugTracker::instance();
tracker.initialize(true, true, false);

// 查询错误
auto errors = tracker.get_errors_by_category(ErrorCategory::COHERENCE);
```

### 4.2 交易追踪

```cpp
auto& txn = TransactionTracker::instance();
uint64_t tid = txn.create_transaction(&payload, "cpu", "READ");
txn.record_hop(tid, "crossbar", 1, "hopped");
txn.complete_transaction(tid);
```

---

## 五、资源链接

| 资源 | 位置 |
|------|------|
| 架构文档 | `docs-pending/02-architecture/` |
| ADR 决策 | `docs-pending/03-adr/` |
| 实施计划 | `plans/implementation_plan_v2.md` |

---

**版本**: 1.0  
**最后更新**: 2026-04-10
