# cpu-cpusim_legacy 微架构文档

> **类别**: CPU > Legacy
> **状态**: ⚠️ Legacy（v2.1 已弃用）
> **Header**: `include/modules/legacy/cpu_sim.hh`
> **注册**: `REGISTER_OBJECT`（`include/modules.hh:30`）— **仅当 `BUILD_LEGACY_MODULES=ON`**
> **蓝图来源**: gem5 v1 风格的 Port-pair 通信
> **首版 commit**: v1 路径（具体追溯 `git log -- include/modules/cpu_sim.hh`）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team (legacy 维护)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §1.3
> - Legacy 归档: [`include/modules/legacy/README.md`](../../modules/legacy/README.md)

---

## 1. 设计目标（**归档说明，非新功能**）

`CPUSim` 是 v1 legacy 时代的 CPU 仿真器，**使用 PortPair/PortManager 通信模型**（区别于 v2.1 ChStream/StreamAdapter）。

**v2.1 弃用原因**（来自 `cpu_sim.hh:63-74` 文件尾 DEPRECATION NOTICE）：
- 架构：ChStream/StreamAdapter 是 v2.1 标准
- 性能：v2.1 TLM 模块是周期精确且端口可组合
- 测试：legacy 模块与 v2.1 插件有已知兼容性问题

**迁移路径**：
- 缓存仿真 → `include/tlm/cache_tlm.hh`
- 内存仿真 → `include/tlm/memory_tlm.hh`
- 新模块 → 基于 `ChStreamModuleBase` + StreamAdapter 模式

## 2. 架构概览

```
   ┌──────────────────────┐
   │      CPUSim          │
   │  (Port-based legacy)│
   │                      │
   │  tick():            │
   │   if (rand()%20==0) │──► handleDownstreamResponse()
   │     if (inflight<4)  │
   │       sendReq()      │
   │                      │
   │  handleDownstream    │
   │  Response():        │
   │   erase inflight     │
   └──────────────────────┘
             │
             ▼
   tlm::tlm_generic_payload (v1 SystemC TLM 1.0 风格)
             │
             ▼
   MasterPort (legacy) ──► SlavePort (下游模块)
```

## 3. 接口（Public API）

```cpp
class CPUSim : public SimObject {
public:
    CPUSim(const std::string& n, EventQueue* eq);

    bool handleDownstreamResponse(Packet* pkt, int src_id,
                                   const std::string& src_label) override;
    void tick() override;

private:
    std::unordered_map<uint64_t, Packet*> inflight_reqs;
    uint64_t next_addr = 0x1000;
};
```

**关键依赖**（v1 SystemC TLM 1.0）：
- `tlm::tlm_generic_payload`（SystemC 1.0 风格，区别于 TLM 2.0）
- `Packet` / `PacketPool`（来自 v1 `core/packet.hh` / `core/ext/packet_pool.hh`）
- `MasterPort`（来自 `core/master_port.hh`）
- `getPortManager().getDownstreamPorts()`（legacy Port 模式）

**注册条件**：
- `BUILD_LEGACY_MODULES=ON`（默认 OFF）：`REGISTER_OBJECT` 展开为有效注册
- `BUILD_LEGACY_MODULES=OFF`（默认）：`REGISTER_OBJECT` 退化为 `/* no-op */`，CPUSim 不在 ModuleFactory 注册表中

## 4. 行为流程

### 4.1 tick() 主循环

```cpp
void CPUSim::tick() {
    auto& pm = getPortManager();
    if (pm.getDownstreamPorts().empty()) return;

    if (inflight_reqs.size() < 4 && rand() % 20 == 0) {
        auto* trans = new tlm::tlm_generic_payload();
        trans->set_command(tlm::TLM_READ_COMMAND);
        trans->set_address(next_addr);
        trans->set_data_length(4);
        trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        Packet* pkt = PacketPool::get().acquire();
        pkt->payload = trans;
        pkt->src_cycle = event_queue->getCurrentCycle();
        pkt->type = PKT_REQ;
        MasterPort* port = pm.getDownstreamPorts()[next_addr % pm.getDownstreamPorts().size()];
        pkt->vc_id = 0;

        if (port->sendReq(pkt)) {
            inflight_reqs[next_addr] = pkt;
            next_addr += 4;
        } else {
            PacketPool::get().release(pkt);
        }
    }
}
```

### 4.2 关键设计取舍（v1 legacy 特征）

- **`rand()` 无种子**——确定性不可控
- **端口选择**：`next_addr % downstream.size()` 哈希到不同端口（无明确语义）
- **TLM 1.0 风格**：`set_command(TLM_READ_COMMAND)` / `set_response_status(TLM_INCOMPLETE_RESPONSE)`——与 v2.1 TLM 2.0 扩展不兼容
- **无 setter / 无 StatGroup / 无 on_config_loaded**——纯 v1 hardcode

## 5. Bundle 字段使用

**v1 SystemC TLM 1.0 字段**（区别于 v2.1 ChStreamBundle）：
- `address` / `command`（READ/WRITE）/ `data_length` / `response_status`
- 通过 `Packet::payload` 包装（`tlm_generic_payload*`）
- 无 `transaction_id` / 无 `kernel_id` 等 GPU 扩展字段

## 6. 统计

**无 StatGroup**（v1 时代未集成 tlm_stats 框架）。

## 7. 蓝图（**无新功能**，仅维护）

按 Legacy README `include/modules/legacy/README.md`：
- ❌ **No new features** will be added
- ✅ **Critical bug fixes only** will be merged
- 📦 **Will be removed** in v3.0 (planned)

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **TLM 1.0 → 2.0 不兼容**——与 v2.1 ChStream 模块无法直接通信 | 高 | 中 | 迁移到 CPUTLM（v2.1） |
| R2 | **`rand()` 无种子**——单元测试不稳定 | 中 | 中 | 改用 `std::mt19937` + 固定种子（v3.0 移除前可选择性修复） |
| R3 | **与 v2.1 插件兼容性问题**（legacy README 明确指出） | 高 | 中 | BUILD_LEGACY_MODULES=OFF 默认避免 |
| R4 | **TLM 1.0 内存所有权语义**——`new tlm_generic_payload()` 需手动 delete | 中 | 中 | `PacketPool` + `release()` 模式（已实现但易出错） |
| R5 | **v3.0 移除**——所有依赖此模块的代码需迁移 | 中 | 中 | 提前规划 CPUTLM 替代方案 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（仅 BUILD_LEGACY_MODULES=ON） | ✅ | `cmake -DBUILD_LEGACY_MODULES=ON` 通过 |
| 单测覆盖 | ⚠️ 有限 | `test_legacy_e2e_simulation.cc`（仅 BUILD_LEGACY_MODULES=ON 编译） |
| 端到端 | ⚠️ 旧 config | `samples/simple1/cpu_cluster.cc`（v1 legacy，已归档至 docs-archived） |
| TLM 1.0 通信 | ✅ | `sendReq()` 链路通 |
| v2.1 兼容 | ❌ 不兼容 | 与 ChStream 不能直接通信 |

## 10. 修订历史

- **v1 (legacy)**: 初版——v1 Port-pair 模式
- **v2.0 → v2.1**: 标记为 DEPRECATED，文件尾添加 DEPRECATION NOTICE
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本归档微架构文档创建（B1 批次）
- **未来 v3.0**: 计划移除
