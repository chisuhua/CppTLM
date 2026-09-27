# D-AXI T1 实施进度 (Phase 0)

> **状态**: ⏳ Phase 0 进行中（2/3 完成, 1 项需 Phase 10 收编）
> **关联**: `openspec/changes/cpptlm-driver-visible-minimal-soc/` (v1.5)
> **总计划**: `docs/superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md`

## Phase 0 完成度

| 子任务 | B-code | 状态 | Commit |
|--------|--------|------|--------|
| **Phase 0.1** PacketPool::acquire_with_min_size | B17 | ✅ 完成 | `118ead3f` |
| **Phase 0.2** GMMU dummy `resp_out()`/`req_in()` | B21 | ⚠️ 跳过（见下） | — |
| **Phase 0.3** MemoryTLM::on_config_loaded wires capacity_gb | B18 | ✅ 完成 | `4317b30c` |

## Phase 0.2 跳过理由

T1 子计划 Phase 0.2 假设 `GmmuTLM` 已切到 `ChStreamModuleBase`，dummy 方法需求成立。**实际代码状态**：

- `include/tlm/gpu/gmmu_tlm.hh:20`: `class GmmuTLM : public ::SimModule`
- 注册路径: `REGISTER_MODULE(GmmuTLM)`（`include/modules_cluster.hh:61`），非 `REGISTER_CHSTREAM`

**结论**: dummy `resp_out()`/`req_in()` 方法是 `ChStreamModuleBase` 派生类的 `registerAdapter` 模板契约要求。`SimModule` 派生类走 `ModuleFactory::registerModule` 路径，不需要 dummy 方法。

**Phase 0.2 真实工作** = 把 GMMU 从 `SimModule` 切到 `ChStreamModuleBase` + 加 AXI MasterPort + 异步状态机 (N2) + pending_iova_ 匹配 + 1-outstanding 契约。**这是 Phase 10 范畴的工作** (GMMU async translate + iova match, B2/B3/B21 的实质落地)。

**延期收编**: B21 在 v1.5 P0 阻塞修正清单中作为"前置框架修复"列出，但在实施时发现实际依赖 Phase 10 的 GMMU 架构改造。Phase 10 T1 子任务原计划已含此改造, B21 与 B2/B3 同步在 Phase 10 实施。

## Phase 0.1 + 0.3 实施证据

### Phase 0.1 (B17) PacketPool::acquire_with_min_size

**问题**: AxiMemBundle ≈ 4136B > PacketPool 默认 256B 上限，导致 `serialize_bundle` 静默失败（v1.3 评审 Oracle 二次审查发现）

**实施**: `include/core/ext/packet_pool.hh:101-130` 新增 `acquire_with_min_size(uint64_t min_bytes)` 方法：
- 返回 packet 的 payload capacity >= `max(min_bytes, 256)`
- 复用现有 freelist + new_payload 路径

**测试**: `test/test_packet_pool.cc` 加 `[d-axi][B17]` TEST_CASE 3 SECTION，8 assertions PASS

**验证**: `./build/bin/cpptlm_tests "[packet][pool][d-axi]"` → 8/8 assertions

### Phase 0.3 (B18) MemoryTLM::on_config_loaded wires capacity_gb

**问题**: v1.4 §13 声称 `capacity_gb=1` → `set_size_bytes(1GB)`，但 `on_config_loaded()` 当前是 no-op（`memory_tlm.hh:95-99`），JSON `capacity_gb` 永远不被读取 → "1GB cap 不存在" 谎言

**实施**: `include/tlm/memory_tlm.hh:95-104` 重写 `on_config_loaded()`：
```cpp
void on_config_loaded() override {
    const auto& cfg = get_config();
    if (cfg.contains("capacity_gb") && cfg["capacity_gb"].is_number()) {
        const uint64_t gb = cfg["capacity_gb"].get<uint64_t>();
        set_size_bytes(gb * (1ULL << 30));
    }
}
```

**测试**: `test/test_memory_tlm_backing.cc` 加 2 个 `[d-axi][B18]` TEST_CASE，4 assertions PASS

**验证**: `./build/bin/cpptlm_tests "[B18]"` → 4/4 assertions; `[memory_backing]` → 7/7 test cases 33/33 assertions

## 实施期发现（向后反馈给 Phase 10）

1. **8GB heap alloc 失败**: `std::vector<uint8_t> backing(8GB)` 在测试中触发 SIGKILL（OOM），但 `unique_ptr<uint8_t[]>(new uint8_t[8GB])` 是 Linux overcommit（lazy commit，无 RSS 提交）。**Phase 10 B7 实施确认**: 必须用 `unique_ptr` + default-init, 禁 `vector::resize` 或 `vector` 作 backing（per v1.5 B7 文档化）。

2. **MemoryTLM v2.2 backing 路径无 OUT_OF_RANGE 检查** (`memory_tlm.hh:115-117`)：当前只检查 `addr + sz > cap`，但 v2.2 路径内对 `addr + sz > backing_size_` 检查缺失（B12 范畴）。**Phase 10 B12 实施确认**: `memory_read/write` 需加 `if (addr + sz > backing_size_) return -EINVAL`。

3. **B17 测试中的 8GB 测试需删除** (已精简到 8KB): Phase 0.1 测试仅覆盖 typical 4KB bundle size；8GB BAR2 路径由 Phase 10 B7 + mmap 单独验证。

## 全部 baseline 验证（66951 assertions 全绿）

| 套件 | 测试数 | assertions | 结果 |
|------|--------|------------|------|
| 全部 `~[d-axi]` | 1587 | 66951 | ✅ ALL PASS |
| `[gmmu]` | 11 | 19 | ✅ |
| `[sdma]` | 53 | 12875 | ✅ |
| `[pcie-memory]` | 24 | 67 | ✅ |
| `[dgpu][shell]` | 33 | 89 | ✅ |
| `[packet][pool][d-axi]` (B17) | 1 | 8 | ✅ |
| `[B18]` (capacity_gb) | 2 | 4 | ✅ |
| `[memory_backing]` 全部 | 7 | 33 | ✅ |

## 后续行动

| 选项 | 内容 |
|------|------|
| **(a) 启动 Phase 1-9**（dual-storage 消灭 + 边界同步 + Fault Path） | 跑 9 个中等风险 Phase, 大约 1-2 工作日 |
| **(b) 启动 Phase 10**（PcieMemoryDevice 重构） | B7-B12 实施核心, 大约 2 工作日 |
| **(c) 暂停 + 团队评审** | 当前 2 commits + 66883 PASS 是阶段性里程碑 |
| **(d) Phase 0.2 评估替代方案** | 是否将 GMMU 切 ChStream 提前到独立子任务 |

我的建议: 选 **(a)** - Phase 1-9 是清理债务阶段, 把 v1.5 P0 阻塞清单逐项清除后再做 Phase 10 重构会大幅降低后续风险。
