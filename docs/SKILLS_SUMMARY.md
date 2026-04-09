# CppTLM v2.0 技能总结

> **版本**: 1.0  
> **创建日期**: 2026-04-10  
> **目的**: 沉淀 Phase 0-4 实施经验，供后续开发参考

---

## 一、核心技能清单

### 1.1 C++17/20 技能

| 技能 | 应用场景 | 实践示例 |
|------|---------|---------|
| **智能指针** | PortManager 独占所有 | `std::unique_ptr<PortManager>` |
| **右值引用** | Extension 克隆 | `std::move()` 优化 |
| **constexpr** | 编译期常量 | `static constexpr int NUM_PORTS = 4;` |
| **模板元编程** | 泛型 Stream Adapter | `template<typename BundleT>` |
| **结构化绑定** | 遍历 map | `for (const auto& [tid, record] : transactions_)` |

### 1.2 设计模式

| 模式 | 应用位置 | 说明 |
|------|---------|------|
| **单例模式** | TransactionTracker, DebugTracker | 全局唯一追踪器 |
| **工厂模式** | PacketPool, ModuleFactory | 对象创建集中管理 |
| **扩展模式** | TLM Extension | 动态附加元数据 |
| **观察者模式** | Event 调度 | 事件通知机制 |

### 1.3 TLM/SystemC 技能

| 技能 | 应用场景 |
|------|---------|
| **tlm_generic_payload** | 标准事务承载 |
| **tlm_extension** | 元数据扩展 |
| **clone()/copy_from()** | 深拷贝实现 |
| **get_extension/set_extension** | Extension 操作 |

---

## 二、Phase 0-4 经验总结

### Phase 0: 原型验证

**关键学习**:
- TLM Extension clone/copy 必须实现深拷贝
- 命名空间必须明确 (`tlm::tlm_extension*`)
- 测试先行避免后期调试困难

**踩过的坑**:
1. 忘记命名空间导致编译错误
2. 测试文件依赖问题未及时发现
3. 构建验证延迟导致技术债务积累

**最佳实践**:
```cpp
// 正确实现 clone
tlm_extension* clone() const override {
    return new TransactionContextExt(*this);
}

// 正确实现 copy_from
void copy_from(const tlm_extension_base& ext) override {
    const auto& other = static_cast<const TransactionContextExt&>(ext);
    // 逐字段复制
}
```

### Phase 2: SimObject 扩展

**关键学习**:
- 层次化复位需要递归调用子模块
- 快照功能使用 JSON 便于序列化
- 向后兼容：默认空实现

**设计决策**:
```cpp
// 层次化复位流程
void reset(const ResetConfig& config) {
    if (config.hierarchical) {
        for (auto* child : children_) {
            child->reset(config);  // 递归子模块
        }
    }
    do_reset(config);  // 调用子类实现
}
```

### Phase 3: 交易处理架构

**关键学习**:
- transaction_id 必须与 stream_id 同步
- Packet 扩展方法委托到 Extension
- Tracker 单例需要线程安全考虑（v2.1）

**API 设计模式**:
```cpp
// 委托模式
uint64_t Packet::get_transaction_id() const {
    if (payload) {
        TransactionContextExt* ext = nullptr;
        payload->get_extension(ext);
        if (ext) return ext->transaction_id;
    }
    return stream_id;  // 回退
}
```

### Phase 4: 错误处理架构

**关键学习**:
- 分层错误分类便于查询和统计
- ErrorContextExt 与 TransactionContextExt 共享框架
- 严重错误检测需要快速路径

**错误处理流程**:
```
检测错误 → 设置 error_code → 附加 ErrorContextExt 
→ DebugTracker 记录 → 可选停止仿真
```

---

## 三、构建与测试技能

### 3.1 CMake 最佳实践

```cmake
# 1. 显式列出源文件（不用 GLOB）
set(CORE_SOURCES
    core/connection_resolver.cc
    core/module_factory.cc
)

# 2. 目标化包含目录
target_include_directories(cpptlm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 3. 条件编译
if(USE_SYSTEMC)
    target_compile_definitions(cpptlm_core PUBLIC USE_SYSTEMC)
endif()
```

### 3.2 Catch2 测试模式

```cpp
// BDD 风格测试
TEST_CASE("Feature description", "[tag]") {
    GIVEN("前置条件") {
        WHEN("执行操作") {
            THEN("验证结果") {
                REQUIRE(...);
            }
        }
    }
}

// 参数化测试（未来）
TEST_CASE_METHOD(FixtureClass, " ... ", "[tag]") {
    // 使用共享 fixture
}
```

### 3.3 调试技能

| 工具 | 用途 | 命令 |
|------|------|------|
| **ccache** | 加速编译 | `ccache -s` 查看统计 |
| **clang-tidy** | 静态分析 | `clang-tidy src/*.cc` |
| **gdb** | 运行时调试 | `gdb ./test` |
| **valgrind** | 内存检查 | `valgrind --leak-check=full ./test` |

---

## 四、代码质量技能

### 4.1 零债务原则实施

**检查清单**:
- [ ] 无 TODO/FIXME 注释
- [ ] 所有测试通过
- [ ] LSP 无 ERROR
- [ ] 构建 0 错误
- [ ] .gitignore 清洁

### 4.2 代码审查要点

| 检查项 | 标准 |
|--------|------|
| **命名** | 类名 CamelCase，函数 camelCase，变量 snake_case |
| **注释** | 中文注释，复杂算法 >5 行必须注释 |
| **缩进** | 2 空格，禁止 Tab |
| **行宽** | <= 100 字符 |
| **错误处理** | 不忽略返回值，不空 catch |

### 4.3 重构技能

**安全重构流程**:
1. 确保测试覆盖
2. 小步提交
3. 每步验证构建
4. 使用 LSP 重命名

---

## 五、常见错误与解决

### 5.1 TLM Extension 错误

**错误**: `copy_from` 参数类型不匹配

```cpp
// 错误
void copy_from(const tlm_extension& ext) override {
    // 编译错误
}

// 正确
void copy_from(const tlm_extension_base& ext) override {
    const auto& other = static_cast<const MyExtension&>(ext);
}
```

### 5.2 单例初始化顺序

**问题**: 多个单例互相依赖导致未定义行为

**解决**: 使用局部静态变量（C++11 保证线程安全）

```cpp
static TransactionTracker& instance() {
    static TransactionTracker tracker;  // 首次调用时初始化
    return tracker;
}
```

### 5.3 内存泄漏

**检测**:
```bash
valgrind --leak-check=full ./test
```

**常见原因**:
- Extension 未删除
- Packet 未释放到 Pool

**解决**:
```cpp
// 正确：使用 Pool
Packet* pkt = PacketPool::get().acquire();
// 使用后
PacketPool::get().release(pkt);
```

---

## 六、性能优化技能

### 6.1 编译优化

```cmake
# Debug: -g -O0
# Release: -O2 -g
# 启用 ccache 加速重复编译
```

### 6.2 运行时优化

| 优化点 | 技术 | 预期收益 |
|--------|------|---------|
| **内存分配** | PacketPool 对象池 | 减少 malloc 调用 |
| **缓存友好** | 结构体紧凑布局 | 提高缓存命中率 |
| **分支预测** | likely/unlikely 提示 | 减少分支误判 |

---

## 七、下一步技能发展

### 7.1 Phase 5 需要的技能

- **模块设计**: Cache/Crossbar/Memory 实现
- **状态机**: 一致性协议的状态转换
- **性能分析**: 延迟/吞吐量测量

### 7.2 未来技能需求

- **并行仿真**: 多线程 Event 调度
- **分布式追踪**: 跨节点交易追踪
- **机器学习**: 性能预测模型

---

**维护**: CppTLM 开发团队  
**版本**: 1.0  
**最后更新**: 2026-04-10
