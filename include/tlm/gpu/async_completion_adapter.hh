// include/tlm/gpu/async_completion_adapter.hh
// AsyncCompletionAdapter: D1-Full P2 #C5 占位实现
// 功能: Phase 9+ TMA async 回调预留接口, Phase 8.B 仅存回调不触发
// 作者 CppTLM Team / 日期 2026-07-16
// 参考:
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/spec.md §cpptlm-async-completion
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §4 IAsyncCompletion 占位
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §Phase 3
#ifndef TLM_GPU_ASYNC_COMPLETION_ADAPTER_HH
#define TLM_GPU_ASYNC_COMPLETION_ADAPTER_HH

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace tlm {

/// IAsyncCompletion: TMA async 完成回调接口（Phase 9+ 使用）
///
/// **Phase 8.B 语义**: 接口已定义但 Adapter 实现为占位（fire_completion 不触发 callback）
/// **Phase 9+ 语义**: Adapter 真实调用 callback，用于 TMA async copy 完成通知
///
/// 调用方契约:
///   - PTX-EMU 端通过 `sm_context.set_async_completion(adapter*)` 注入
///   - nullptr = 独立模式 (PTX-EMU 走原有同步路径, 零退化)
///   - 调用方负责 lifecycle: PTX-EMU 持有 Adapter 实例, CppTLM ModuleFactory 销毁时析构
class IAsyncCompletion {
public:
    virtual ~IAsyncCompletion() = default;

    /// 注册 async 完成回调（id 用于 fire_completion 时匹配）
    /// @param id  PTX-EMU 生成的唯一 ID（来自 TMA 操作）
    /// @param cb  完成时调用的回调函数
    /// @note Phase 8.B: 仅存储, 不立即调用
    virtual void register_completion_callback(uint64_t id, std::function<void()> cb) = 0;

    /// 触发 async 完成（id 匹配的 callback 应被调用）
    /// @param id  PTX-EMU 生成的 ID（应与 register 时相同）
    /// @note **Phase 8.B 占位**: 不调用 callback, 仅递增 fire_completion_count_
    /// @note **Phase 9+ 实施**: 真正调用 callback 并 erase map entry
    virtual void fire_completion(uint64_t id) = 0;
};

/// AsyncCompletionAdapter: IAsyncCompletion 的 Phase 8.B 占位实现
///
/// **设计原则**:
///   - 非线程安全（F12b-LD 阶段单线程假设, Phase 9+ 添加 mutex）
///   - callback 用 std::function<void()> 存储 (支持 lambda + 捕获)
///   - fire_completion_count_ 原子计数器供监控 / 测试使用
///   - 独立模式 (KernelLaunchTLM::async_completion_ == nullptr) 不影响本类
///
/// **Phase 9+ 替换路径**:
///   1. 重写 `fire_completion()`: 调用 callback + erase
///   2. 考虑添加 thread-safety (mutex 或 atomic map)
///   3. 添加 callback 超时机制 (避免永久 hang)
class AsyncCompletionAdapter : public IAsyncCompletion {
public:
    AsyncCompletionAdapter() = default;
    ~AsyncCompletionAdapter() override = default;

    /// 注册 callback（Phase 8.B 仅存储）
    void register_completion_callback(uint64_t id, std::function<void()> cb) override {
        pending_callbacks_[id] = std::move(cb);
    }

    /// 触发完成（Phase 8.B 占位: 仅计数, 不调用 callback）
    void fire_completion(uint64_t id) override {
        ++fire_completion_count_;
        // Phase 8.B: 故意不调用 it->second()
        // Phase 9+: 取消注释:
        //   if (auto it = pending_callbacks_.find(id); it != pending_callbacks_.end()) {
        //       it->second();
        //       pending_callbacks_.erase(it);
        //   }
        (void)id;  // 避免 unused 警告
    }

    /// 监控接口: 返回累计 fire_completion 调用次数
    /// @note 测试 + 监控使用, 非接口方法
    uint64_t fire_completion_count() const {
        return fire_completion_count_;
    }

    /// 监控接口: 返回当前 pending callback 数量 (Phase 8.B 一直增长, 不 erase)
    /// @note 测试 + 监控使用, 非接口方法
    size_t pending_callback_size() const {
        return pending_callbacks_.size();
    }

private:
    std::unordered_map<uint64_t, std::function<void()>> pending_callbacks_;
    uint64_t fire_completion_count_ = 0;
};

}  // namespace tlm

#endif  // TLM_GPU_ASYNC_COMPLETION_ADAPTER_HH