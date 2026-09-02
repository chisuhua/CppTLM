# axi4-mapper 微架构文档

> **类别**: Framework > AXI Mapper · **状态**: 🔵 Implemented (per Phase 6 + ADR-SOC-13)
> **Header**: `include/framework/axi4_mapper.hh` (113 行)
> **类**: `cpptlm::Axi4Mapper`（独立模块,非 ChStreamModuleBase / 非 SimModule,可被 CrossbarTLM / CacheTLM 复用）
> **命名空间**: `cpptlm::`（per `include/AGENTS.md` 约定）
> **蓝图来源**: Phase 6 AXI4Mapper（per `openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/`）
> **关联 ADR**:
> - [`ADR-SOC-13-axi-stream-adapter-mapper.md`](../adr/ADR-SOC-13-axi-stream-adapter-mapper.md) D3 — Axi4Mapper 独立模块 + 与 PcieEndpointIP 解耦 + JSON 可选注入
> - [`ADR-SOC-10-module-factory-topology.md`](../adr/ADR-SOC-10-module-factory-topology.md) D4 — JSON `axi4_mapper_inject: true` 可选注入
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](../adr/ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 不内嵌 Mapper（解耦）
> **关联 OpenSpec**: [`openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/`](../../../openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/)
> **首版 commit**: `2026-12-22` T-MAP-1 + `8b92bfb..fe4d745` (Phase 6 实施, Oracle PASS) · **最近更新**: 2027-02-09 (Phase 8 整合 + ADR-SOC-13 同步)
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 框架基础: [`include/framework/AGENTS.md`](../../../include/framework/AGENTS.md) (StreamAdapter 接口契约)
> - Bundle 定义: [`include/bundles/axi4_bundles_tlm.hh`](../../../include/bundles/axi4_bundles_tlm.hh) (Axi4Bundle 17 字段)
> - 配对组件: [`axi4-stream-adapter.md`](./axi4-stream-adapter.md) (三端口 valid/ready 握手)
> - L1 Host Interface 子系统架构: [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md)

---

## 1. 设计目标

`cpptlm::Axi4Mapper` 是 **AXI4 ↔ Bundle Mapper**,在 [`Axi4Bundle`](../../../include/bundles/axi4_bundles_tlm.hh) 之上提供 **outstanding 事务跟踪**与 **out-of-order completion 调度**。

**核心特征**:
- **outstanding 跟踪**: 读写独立 ID 空间(`awid`/`arid` 各自计数),容量上限 N,N+1 拒绝新发出
- **OOO completion**: 通过 `rid` 把乱序 `rdata` 关联回原事务(核心机制)
- **独立模块**: 与 PcieEndpointIP 解耦,可被 CrossbarTLM / CacheTLM 复用
- **只做跟踪/调度**: 不内嵌 AXI 通道数据路径(数据路径经 [`Axi4StreamAdapter`](./axi4-stream-adapter.md))

---

## 2. 架构概览

```
   issue_write(req) / issue_read(req)
   ┌─────────────────────────────┐
   │ Axi4Mapper(capacity=N)      │
   │ - awid_count_/arid_count_   │ ◄──── 读写独立 ID 空间
   │ - write_oor_queue_[N]       │ ◄──── outstanding 写请求
   │ - read_oor_map_[N]          │ ◄──── outstanding 读请求(rid→原事务)
   │ - match_read_resp(resp)     │ ◄──── OOO completion 核心
   └─────────────────────────────┘
                  │
                  ▼
       Axi4StreamAdapter(数据路径,valid/ready 反压)
```

---

## 3. 关键接口

| 接口 | 签名 | 作用 |
|------|------|------|
| 构造 | `explicit Axi4Mapper(std::size_t capacity = 16)` | outstanding 容量上限(读写各 N) |
| 登记写请求 | `bool issue_write(const Axi4Bundle& req)` | 按 awid 计数;返回 true 接受;false 表示容量满(N+1 拒绝) |
| 登记读请求 | `bool issue_read(const Axi4Bundle& req)` | 按 arid 计数,存储原事务供 OOO 关联 |
| 写完成 | `void complete_write(const Axi4Bundle& resp)` | 按 bid 匹配并释放槽位 |
| 读完成 | `bool match_read_resp(const Axi4Bundle& resp)` | 按 rid 匹配乱序 rdata;返回 true 表示匹配成功并消耗 |
| 容量查询 | `size_t outstanding_writes() / outstanding_reads()` | 查询当前 outstanding 数 |

---

## 4. OOO Completion 核心机制

**乱序完成不影响其他 outstanding（不匹配即不消耗）**:
1. 读请求 A 发出(`arid=A`)
2. 读请求 B 发出(`arid=B`)
3. 读响应 R_B 先到达(`rid=B`),`match_read_resp(R_B)` 返回 true,移除 B 的 outstanding
4. 读响应 R_A 后到达(`rid=A`),`match_read_resp(R_A)` 返回 true,移除 A 的 outstanding
5. **不匹配**: 若 rid 不在 outstanding map 中,`match_read_resp` 返回 false(忽略或上报)

---

## 5. 解耦与复用

- **独立于 PcieEndpointIP**: Axi4Mapper 是无状态组件,无硬编码耦合
- **可被 CrossbarTLM 复用**: CrossbarTLM 的多端口事务调度可集成 Axi4Mapper 做 OOO
- **可被 CacheTLM 复用**: CacheTLM 的 miss 路径返回数据可能乱序,可集成 Axi4Mapper 做 ID 关联
- **JSON 可选注入**: 通过 `axi4_mapper_inject: true` 配置启用(per ADR-SOC-10 D4 + `examples/dgpu_soc_with_pcie_ip.json`)

---

## 6. 与 Axi4StreamAdapter 的边界

| 维度 | Axi4StreamAdapter | Axi4Mapper |
|------|-------------------|------------|
| 职责 | valid/ready 反压 + 三端口数据路径 | outstanding 跟踪 + OOO completion 调度 |
| 数据路径 | ✅ 内嵌 | ❌ 仅跟踪,不内嵌 |
| 状态 | 通道 valid/ready 信号 + outstanding ID 槽 | 读写 ID map |
| 复用 | AXI 边界组件 | 任何需 OOO 调度的 AXI 链路 |

---

## 7. 已知限制

- **容量上限 N**: N+1 拒绝（与 Q12 CompletionTracker 语义一致）
- **读写独立 ID 空间**: awid 和 arid 不共享计数器
- **Phase 8 M1 Cfg 地址编码简化**: 当前用 `awaddr` 当 offset（per ADR-SOC-13 D5）

---

## 8. 测试覆盖

- `test/test_axi4_mapper_*.cc` (Phase 6): outstanding 跟踪 + OOO completion + N+1 拒绝 + 读写独立
- `test/test_pcie_endpoint_ip_full_e2e.cc` (Phase 8): 通过 `axi4_mapper_inject: true` 注入,3 TEST_CASE 全链路 PASS
