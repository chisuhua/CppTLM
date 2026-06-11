# rtl-hybrid_cache 微架构文档

> **类别**: RTL > HybridCache
> **状态**: ✅ 已实施（仅当 `BUILD_RTL=ON`）
> **Header**: `include/rtl/hybrid_cache_component.hh` + `include/rtl/hybrid_cache_wrapper.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:65`，`HYBRID_CACHE_WRAPPER_REGISTER_RTL` 守卫宏，`BUILD_RTL=OFF` 退化为 no-op）
> **蓝图来源**: gem5 RTL 风格（C++20 CppHDL），canonical pattern 来自 `docs/example_rtl_modules.md:20-66`
> **首版 commit**: v2.1 RTL 子树（2026-06-07 附近）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §1.3

---

## 1. 设计目标

`cpptlm::rtl::HybridCacheComponent` + `cpptlm::rtl::HybridCacheWrapper` 是 **TLM↔RTL 桥接**——前者是纯 C++20 CppHDL RTL 组件，后者是 PIMPL 包装把它暴露为标准 ChStream 模块。**与 gem5 对位**: gem5 的 RTL SimObject（简化版，2 态 FSM）。

**核心特性**（来自 `hybrid_cache_component.hh:36-52` + `hybrid_cache_wrapper.hh`）：
- 纯 C++20 CppHDL 组件，**2 个 `ch_stream<Bundle>` 端口**（不是 15 个标量端口）
- `Bundle` 自带 fragment 元数据（`parent_id`/`fragment_id`/`fragment_total`/`first`/`last`）
- 2 态 FSM：`IDLE → PROCESS → RESPOND`
- 单拍、总是 hit、echo `addr` 作为 `data`（spike 范围）
- PIMPL 包装：通过 `FragmentMapper` 做 TLM↔RTL beat 转换

## 2. 架构概览

### 2.1 双层结构

```
   TLM 侧 (ChStream 协议)                 RTL 侧 (CppHDL ch_stream 协议)
   ──────────────────                  ─────────────────────
                                          ┌──────────────────────────┐
   InputStreamAdapter<CacheReqBundle>      │ HybridCacheComponent     │
   ───────────────────────────────────►    │  (ch::Component)         │
                                          │  2 ch_stream ports:      │
                                          │   - req_in               │
                                          │   - resp_out             │
                                          │  2 态 FSM                │
                                          │  IDLE → PROCESS → RESPOND│
   OutputStreamAdapter<CacheRespBundle>   │                          │
   ◄───────────────────────────────────   │  跨拍状态: ch_reg<>      │
                                          └──────────────────────────┘
              ▲                                          ▲
              │                                          │
              └─────────── HybridCacheWrapper (PIMPL) ──┘
                              │
                              ▼
                      FragmentMapper (TLM ↔ RTL beat)
```

### 2.2 HybridCacheComponent 端口

```
__io(
    ch_stream<bundles::CacheReqBundleRTL>  req_in;
    ch_stream<bundles::CacheRespBundleRTL> resp_out;
);
```

**对比 TLM 端**：
- TLM `InputStreamAdapter<CacheReqBundle>` ↔ RTL `ch_stream<CacheReqBundleRTL>`
- TLM `OutputStreamAdapter<CacheRespBundle>` ↔ RTL `ch_stream<CacheRespBundleRTL>`

**注**: RTL Bundle (`cache_bundles_rtl.hh`) 与 TLM Bundle (`cache_bundles_tlm.hh`) **字段对称**——FragmentMapper 做字段级转换。

## 3. 接口（Public API）

### 3.1 HybridCacheComponent (RTL 侧)

```cpp
namespace cpptlm::rtl {
class HybridCacheComponent : public ch::Component {
public:
    __io(
        ch_stream<bundles::CacheReqBundleRTL>  req_in;
        ch_stream<bundles::CacheRespBundleRTL> resp_out;
    );

    HybridCacheComponent(ch::Component* parent = nullptr,
                       const std::string& name = "hybrid_cache");

    void create_ports() override {
        new (this->io_storage_) io_type;
    }
    void describe() override;
};
}
```

### 3.2 HybridCacheWrapper (TLM 侧, PIMPL)

```cpp
class HybridCacheWrapper : public ChStreamModuleBase {
public:
    HybridCacheWrapper(const std::string& name, EventQueue* eq);
    ~HybridCacheWrapper() override;

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();
    cpptlm::StreamAdapterBase* get_adapter() const;

    tlm_stats::StatGroup* get_stats_group() override;
    std::string get_stats_path() const override;
};
```

## 4. 行为流程

### 4.1 HybridCacheComponent FSM（v0 spike 范围）

```cpp
void HybridCacheComponent::describe() {
    // 2 态 FSM:
    //   IDLE: 等待 req_in.valid && req_in.ready
    //         → 锁存首拍 (ch_reg<>)
    //   PROCESS: 输出 resp_out.valid
    //         → 等待 resp_out.ready
    //         → 释放锁存, 回 IDLE
}
```

**v0 spike 行为**：
- 单拍（无 fragment 处理）
- 总是 hit
- echo `addr` 作为 `data`（`req_in.addr` → `resp_out.data`）
- 跨拍状态用 `ch_reg<>` 锁存

### 4.2 HybridCacheWrapper tick() 流程（PIMPL）

```cpp
void HybridCacheWrapper::tick() {
    // 1. TLM 端: consume req_in
    if (req_in_.valid() && req_in_.ready()) {
        auto& req = req_in_.data();
        // 2. TLM → RTL beat 转换 (FragmentMapper)
        CacheReqBeatRTL beat = FragmentMapper::serialize_req(pkt);
        // 3. 推入 RTL 组件
        component_->req_in.write(beat);
        req_in_.consume();
    }

    // 4. RTL 端: tick CppHDL 组件
    component_->tick();

    // 5. RTL → TLM beat 转换
    if (component_->resp_out.valid() && resp_out_.ready()) {
        CacheRespBeatRTL resp_beat = component_->resp_out.read();
        CacheRespBundle resp = FragmentMapper::write_resp(pkt, resp_beat);
        resp_out_.write(resp);
        component_->resp_out.consume();
    }
}
```

## 5. Bundle 字段使用

### 5.1 TLM 端（`CacheReqBundle` / `CacheRespBundle`）

| 字段 | Wrapper 使用 |
|------|------------|
| `transaction_id` | 透传到 RTL beat（`tid` 字段） |
| `address` | 透传 + 镜像为 RTL resp `data`（spike） |
| `is_write` / `data` | 透传 |
| `fragment_id` / `fragment_total` / `parent_id` | 通过 `TransactionContextExt` 提取到 RTL beat |
| `first` / `last` | 同上 |

### 5.2 RTL 端（`CacheReqBundleRTL` / `CacheRespBundleRTL`）

通过 `cache_bundles_rtl.hh` 定义，**字段与 TLM 端对称**（`ch_uint<64>` / `ch_bool`），与 `cache_bundles_tlm.hh` 通过 `FragmentMapper` 双向转换。

## 6. 统计

| 指标 | 类型 | 含义 |
|------|------|------|
| Wrapper 挂载 StatGroup | (v0 未挂载) | — |

**路径**: (Wrapper 实际路径由 `get_stats_path()` 决定——v0 桩)

## 7. 蓝图（未来演进）

### 7.1 真实延迟（spike 范围升级）

- v0: 单拍即返回
- v2.2: 多周期延迟（hit/miss 区分）
- v2.2: 多拍 fragment 处理（beat 序列）

### 7.2 Hit/Miss 决策

- v0: 总是 hit，echo addr 作为 data
- v2.2: 真实 set-associative cache（set index / tag compare / way select）
- v2.2: replacement policy（LRU/LFU）

### 7.3 与 CacheTLM 集成

- 当前：Wrapper 是**独立 RTL 桥接模块**，可作为 CacheTLM 的**RTL 后端替代**
- v2.2: CacheTLM 改为 polymorphic，TLM 后端用 `std::map`，RTL 后端用 HybridCacheComponent

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **v0 spike 总是 hit**——非真实 cache 行为 | 高 | 中 | v2.2 真实 cache 模型 |
| R2 | **单拍**——fragment 处理未实现 | 高 | 中 | v2.2 多拍支持 |
| R3 | **`echo addr as data`**——`data` 字段语义被覆盖 | 中 | 低 | v0 显式标注"spike" |
| R4 | **`create_ports` PIMPL 模式**——RTL 组件内存由 wrapper 管理 | 中 | 低 | 现有 PIMPL 模式已成熟 |
| R5 | **`describe()` 实际未给出**（v0 stub） | 高 | 中 | v2.2 真实 CppHDL FSM 描述 |
| R6 | **依赖 `ch.hpp` / `chlib/stream.h`**——额外 CppHDL 依赖 | 中 | 低 | 已通过 `BUILD_RTL=OFF` 退化为 no-op；构建系统隔离 |
| R7 | **`fragment_total > 1` 时 FragmentMapper 行为未测试** | 中 | 中 | v2.2 真实 fragment 测试 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（仅当 BUILD_RTL=ON） | ✅ | `cmake -DBUILD_RTL=ON` 通过 |
| 编译（默认 BUILD_RTL=OFF） | ✅ | 守卫宏退化为 no-op |
| 2 端口 CppHDL | ✅ | `__io(ch_stream<...> req_in/resp_out)` |
| 2 态 FSM | ✅ | IDLE → PROCESS → RESPOND |
| Fragment 转换 | ✅ | `FragmentMapper::serialize_req/serialize_beat_at/write_resp` |
| PIMPL 包装 | ✅ | `HybridCacheWrapper` 隐藏 RTL 细节 |
| **真实 hit/miss** | ❌ 总是 hit | 见 R1 |
| **多拍 fragment** | ❌ 单拍 | 见 R2 |

## 10. 修订历史

- **2026-06-07**: HybridCacheComponent 初版（CppHDL 2 端口 + 2 态 FSM）
- **2026-06-07**: HybridCacheWrapper PIMPL 包装
- **2026-06-07**: FragmentMapper 集成（`cache_bundles_rtl.hh`）
- **2026-06-08**: v2.1 Release 标签（含 BUILD_RTL 守卫）
- **2026-06-11**: 本微架构文档创建（B2 批次）
