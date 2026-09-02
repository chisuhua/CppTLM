# cpu-cputlm 微架构文档

> **类别**: CPU > CPUTLM · **状态**: ✅ 已实施 + 📋 v1.0 dGPU SoC 战略补充
> **状态**: ✅ 已实施
> **Header**: `include/tlm/cpu_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:33`）
> **蓝图来源**: gem5 `BaseSimpleCPU`（简化为地址发起器）
> **首版 commit**: v2.1 路径同步
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3（黑盒发起器模板）

---

## 1. 设计目标

`CPUTLM` 是 **v0 简化版 CPU 模型**，仅作为地址流量发起器——**不模拟 ISA、不模拟 pipeline、不模拟 cache hierarchy**。**与 gem5 对位**: `BaseSimpleCPU` 的"地址发起器"最小子集。

**核心特性**（来自 `cpu_tlm.hh:12-87`）：
- 单端口 Initiator：`req_out` + `resp_in`（+2 个 dummy 满足 StreamAdapter 接口）
- 硬编码地址范围 `0x1000–0x1100`（256 字节），4 字节步长循环
- **只读**（`is_write = 0`）
- `request_interval_ = 10` 周期（每 10 周期发 1 请求）
- `MAX_INFLIGHT = 4`（最大在飞事务数）

## 2. 架构概览

```
        ┌──────────────┐
        │   CPUTLM     │
        │              │
   tick()  ──► req_out_  ──► (Cache → Xbar → Memory)
        │   ▲
        │   │ resp_in_ ◄──
        └──────┬───────┘
               │ inflight_txns_ (max 4)
               ▼
       cur_addr_ = 0x1000 + 4*i
       0x1100 时绕回 0x1000
```

## 3. 接口（Public API）

```cpp
class CPUTLM : public ChStreamModuleBase {
public:
    explicit CPUTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "CPUTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;

    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>&  resp_in();
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>&  req_out();
    cpptlm::StreamAdapterBase* get_adapter() const;

    // Dummy 适配器（满足 StreamAdapter 接口，不实际收发）
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
};
```

**硬编码常量**：

| 常量 | 值 | 含义 |
|------|----|------|
| `start_addr_` | 0x1000 | 地址起点 |
| `request_interval_` | 10 | 发包周期间隔 |
| `MAX_INFLIGHT` | 4 | 最大在飞事务数 |
| 地址窗口 | 0x1000-0x1100 | 256 字节循环 |

**无 setter**（v0）——所有参数构造函数硬编码。

## 4. 行为流程

### 4.1 tick() 主循环

```cpp
void CPUTLM::tick() {
    // Phase 1: 响应消费
    if (resp_in_.valid() && resp_in_.ready()) {
        auto& resp = resp_in_.data();
        uint64_t txn_id = resp.transaction_id.read();
        inflight_txns_.erase(txn_id);
        resp_in_.consume();
    }

    // Phase 2: 请求发起（节流 + 在飞限制）
    if (inflight_txns_.size() < MAX_INFLIGHT && timer_ == 0) {
        bundles::CacheReqBundle req;
        req.transaction_id.write(next_txn_id_++);
        req.address.write(cur_addr_);
        req.is_write.write(0);  // 只读！
        req.data.write(0);
        req.size.write(4);
        req_out_.write(req);
        inflight_txns_[req.transaction_id.read()] = cur_addr_;
        cur_addr_ += 4;
        if (cur_addr_ >= start_addr_ + 0x100) cur_addr_ = start_addr_;
    }

    // Phase 3: 周期计数（节流）
    timer_ = (timer_ + 1) % request_interval_;

    // Phase 4: Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 关键设计取舍

- **节流 = 10**：`timer_` 0-9 循环，每 10 周期发 1 请求（**节流而非"每周期发 1 请求"**——减少下游压力）
- **in-flight 限流**：`inflight_txns_.size() < 4` 时才发——避免在飞事务无限累积
- **响应仅 erase**：`inflight_txns_.erase(txn_id)`，**未做延迟统计**——v0 无 StatGroup
- **地址无业务含义**：`0x1000-0x1100` 是**仅用于触发下游响应**，无真实指令/数据语义

## 5. Bundle 字段使用

| 字段 | CPUTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——`inflight_txns_` 映射键 |
| `address` | 硬编码 0x1000 + 4*i 循环 |
| `is_write` | **硬编码 0**（只读） |
| `data` | **硬编码 0** |
| `size` | **硬编码 4** |
| 其他 | 忽略 |

## 6. 统计

**无 StatGroup**——CPUTLM **完全不挂载任何统计指标**（`get_stats_group()` 未重写，ModuleFactory Step 8 跳过注册）。

`stats_path` 不存在——`tlm_stats::StatsManager` 中无 CPUTLM 节点。

## 7. 蓝图（未来演进）

### 7.1 Phase 7.B 共享基类

调研 §4 Phase 1 + spec §3：CPUTLM / TrafficGenTLM / GPUTLM v0 三者**大量代码重复**（tick 循环 / inflight 跟踪 / adapter 注入），Phase 7.B 抽出 `compute_unit_base` 共享基类。

### 7.2 蓝图增强

- **setter 化**：暴露 `set_start_addr()` / `set_max_inflight()` / `set_request_interval()`（v0 全部硬编码）
- **写支持**（`is_write=1` 路径）
- **on_config_loaded JSON 解析**：与 TrafficGenTLM / GPUTLM 统一修复（spec §6 R5 + roadmap §Phase 7.B）
- **真实延迟统计**（StatGroup 挂载）
- **多种地址模式**（顺序 / 随机 / 压力 / trace）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **硬编码 0x1000-0x1100 256 字节窗口**——不真实指令流 | 高 | 中 | v0 简化；v2.2 暴露 setter |
| R2 | **只读**（`is_write=0`）——与实际 CPU 不符 | 高 | 中 | v0 简化；v2.2 写支持 |
| R3 | **无 setter**——所有行为固定 | 高 | 中 | 同 R1 |
| R4 | **无 StatGroup**——延迟/吞吐完全不可观察 | 高 | 中 | v0 简化；v2.2 挂载统计 |
| R5 | **JSON params 不读**——`on_config_loaded` 未重写 | 高 | 中 | Phase 7.B 统一修复 |
| R6 | **节流硬编码 10** | 中 | 中 | v2.2 `set_request_interval()` |
| R7 | **MAX_INFLIGHT=4 硬编码** | 中 | 中 | v2.2 `set_max_inflight()` |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ⚠️ 间接 | `configs/cpu_tlm_test.json` + `cpu_group.json` 端到端 |
| 端到端 (CPU→Cache→Xbar→Memory) | ✅ | 4 个独立 config（cpu_group / single_cluster_soc / stress_full / ...） |
| 4 in-flight 限制 | ✅ | `inflight_txns_.size() < 4` |
| 10 周期节流 | ✅ | `timer_` 0-9 循环 |
| **写支持** | ❌ 硬编码只读 | 见 R2 |
| **真实统计** | ❌ 无 | 见 R4 |

## 10. 修订历史

- **2026-04-12**: CPUTLM 初版（v2.1 ChStream 路径）
- **2026-04-15**: Phase 6 集成测试通过
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B1 批次）
