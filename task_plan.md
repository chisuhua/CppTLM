# Task Plan: Phase 7 NoC/NIC 代码修复

**项目**: CppTLM Phase 7 NoC/NIC
**版本**: 1.0
**创建日期**: 2026-04-24
**最后更新**: 2026-04-24

---

## 目标

修复 Phase 7 评审报告中发现的代码缺陷，确保代码可编译、可运行。

---

## 优先级总览

| 优先级 | 类型 | 问题数 | 状态 |
|--------|------|--------|------|
| P0 | 编译错误 | 1 | 🔴 pending |
| P1 | 正确性严重 | 4 | 🔴 pending |
| P2 | 精度/设计一致性 | 3 | 🔴 pending |
| P3 | 代码质量 | 3 | 🟡 pending |

---

## P0: 编译错误（阻塞性）

### P0-1: BidirectionalPortAdapter tick() 调用不存在的 req_out()

**文件**: `include/framework/bidirectional_port_adapter.hh`
**行**: 108
**问题**: `module_->req_out()[i].clear_valid()` — RouterTLM 没有 `req_out()` 方法

**修复方案**: 删除 `clear_valid()` 调用，`send()` 内部已处理

**状态**: 🔴 pending

---

## P1: 正确性严重问题

### P1-1: VC ID 硬编码为 0

**文件**: `src/tlm/router_tlm.cc`
**行**: 122
**问题**: `unsigned vc = 0;` 忽略 bundle 中的 vc_id

**修复方案**: 从 bundle 读取 VC ID
```cpp
unsigned vc = req_adapter.data().vc_id.read();
if (vc >= NUM_VCS) vc = 0;
```

**状态**: 🔴 pending

### P1-2: Credit Return 永不通路

**文件**: `src/tlm/router_tlm.cc`
**问题**: `receive_credit()` 从未被调用，credit 永久耗尽导致阻塞

**修复方案**: 添加 `implicit_credit_return()` 机制，每 BUFFER_DEPTH 周期自动恢复 credit

**状态**: 🔴 pending

### P1-3: NICTLM reassemble flits 数组未初始化

**文件**: `src/tlm/nic_tlm.cc`
**行**: 122
**问题**: `std::array<NoCFlitBundle, FLITS_PER_PACKET>` 使用默认构造，可能包含垃圾值

**修复方案**: 使用 `{}` 显式初始化

**状态**: 🔴 pending

### P1-4: NICTLM reassemble 非 HEAD flit 处理缺失

**文件**: `src/tlm/nic_tlm.cc`
**行**: 108-119
**问题**: 当 `it == pending_packets_.end()` 且 flit 不是 HEAD 时，应该忽略但当前会丢失

**修复方案**: 添加 else 分支处理非 HEAD 情况

**状态**: 🔴 pending

---

## P2: 精度/设计一致性

### P2-1: LT 阶段空实现

**文件**: `src/tlm/router_tlm.cc`
**行**: 290-294
**问题**: 六阶段流水线 LT 阶段为空，每跳 1 周期链路延迟未建模

**修复方案**: 添加 pending_output 队列，LT 阶段才真正发送 flit

**状态**: 🔴 pending

### P2-2: VC Allocation 重复分配

**文件**: `src/tlm/router_tlm.cc`
**行**: 184-202
**问题**: 每个周期为所有 active flit 重新分配 VC，应该只在 RC 阶段后分配一次

**修复方案**: 添加 `vc_allocated` 标记，只对 HEAD flit 首次进入 VA 时分配

**状态**: 🔴 pending

### P2-3: Switch Allocation 只仲裁一个 flit

**文件**: `src/tlm/router_tlm.cc`
**行**: 234
**问题**: `return` 语句导致每周期只转发一个 flit

**修复方案**: 移除 return，收集所有仲裁结果，批量处理多个 flit

**状态**: 🔴 pending

---

## P3: 代码质量

### P3-1: NICTLM packetize 无 backpressure 检查

**文件**: `src/tlm/nic_tlm.cc`
**行**: 95
**问题**: 无条件 `net_req_out_.write()`，不检查下游 ready 信号

**修复方案**: 添加 `ready()` 检查，仅在下游可接收时写入

**状态**: 🟡 pending

### P3-2: test_nic_tlm.cc inject_req 实现可疑

**文件**: `test/test_nic_tlm.cc`
**问题**: consume/set_valid 顺序错误，memcpy 用法不安全

**修复方案**: 修正为正确顺序，直接赋值替代 memcpy

**状态**: 🟡 pending

### P3-3: 缺少真正的 NoC/NIC 集成测试

**文件**: `test/test_phase7_integration.cc`
**问题**: 该文件测试 CPUTLM/ArbiterTLM，与 RouterTLM/NICTLM 无关

**修复方案**: 新增 `test_noc_integration.cc`，测试 Router 多跳转发

**状态**: 🟡 pending

---

## 错误记录

| 错误 | P# | 尝试 | 解决 |
|------|----|:----:|------|
| (无) | - | - | - |

---

## 决策记录

| 决策 | 日期 | 理由 |
|------|------|------|
| Credit Return 采用隐式方案 | 2026-04-24 | 简化实现，避免大规模架构修改 |
| LT 延迟用 pending queue 方案 | 2026-04-24 | 不改变 tick() 调用顺序，最小侵入 |

---

## 下一步行动

**P0-1**: 修复 BidirectionalPortAdapter tick() 编译错误
