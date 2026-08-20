HANDOFF CONTEXT
===============

USER REQUESTS (AS-IS)
---------------------
- 请把方案A， B， C都写入到handoff文档里，把所有需要的工作背景知识都写清楚，我想在新的session里讨论方案
- 前序请求: "同意建议，请按建议内容执行" (P0全套)
- 前序请求: "2" (选项B - P0全套)
- 前序请求: "Oracle 进行调研，给出最佳方案" (Oracle失败, fallback到自分析)

GOAL
----
在新session中讨论P0全套（3个修复）的具体实施方案：方案A（镜像）/ 方案B（端口表重构）/ 方案C（不修复），并实施选定方案以解锁P3 connectCPU helper + P5 incorporate_parent真实wiring + CoherentXBarTLM snoop。

WORK COMPLETED
--------------
- 我完整执行了 SimModule Complex Hierarchies 计划（5 Phase + Final + Coverage 补全）：
  - P1（API 修复）: 修 D.4 (findInternalPath 递归) + D.5 (默认 tick 递归) + 3 层 JSON E2E
  - P2（GPU 端 4 类）: ComputeCluster / TpcCluster / GpcCluster / GpuCluster + JSON 蓝图模板复用 + REGISTER_MODULE 参数化重构
  - P3（ChStream helper）: connectBus / connectCPUSideBus / connectMemSideBus（connectCPU 因依赖 D.1 延后）
  - P4（基础设施 3 类）: CacheCluster / MemoryCluster / GpuNoC
  - P5（顶层）: ApuSoC + incorporate_parent 钩子
  - Final: 6 文档同步 + 综合验收
  - Coverage 补全: 14 新 TEST_CASE（6 helper safety + 8 边界）+ 6 get_module_type 断言 + run_all_tests.sh E2E 循环加 2 个新 JSON
- 完成度: 24 commits, 673/673 C++ tests + 222/222 Python tests pass, 10 SimModule 类全部注册, run_all_tests.sh E2E PASS 全配置
- 关键决策: 3 轮 Momus 审查（每次发现 plan spec 错误）+ TDD 捕获 4 个关键 bug（P2-T2.5 暴露 SimModule::simulate_instantiate 缺 virtual + cfg schema 错 + JSON 路径错 + ComputeCluster 多次 instantiateAll 触发 replace 语义）

CURRENT STATE
--------------
- 工作树 clean（无 uncommitted changes）
- Branch: main
- HEAD: 888440c (docs(handoff): save P0 discussion handoff context for new session)
- 全部 25 commits 已落地 main 分支（24 plan commits + 1 handoff commit）
- Build 环境: cmake --build build -j$(nproc) 成功, cpptlm_tests 二进制 196MB
- 659/659 → 673/673 (P1+P2+P3 coverage 补全新增 14 用例)
- Plan 文件: docs-archived/superpowers/plans/2026-06-19-simmodule-complex-hierarchies.md (1561 行, 3 轮 Momus PASS)
- Spec 文件: docs-archived/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md (860 行)

PENDING TASKS
--------------
P0 全套（用户已选 B，未实施）：
1. **D.1 修复** - `getInternalOutputPort` 对 ChStream 模块永远返回 nullptr
2. **CoherentXBarTLM 实现** - 新类用于 APU 顶层跨域 snoop 广播（当前 apu_soc_v1.json 用普通 CrossbarTLM 占位）
3. **死代码清理** - connectBus/connectCPUSideBus/connectMemSideBus 的 "port not found" throw 路径（lazy registration 死代码）

P1 阶段（待 P0 完成后）：
4. incorporate_parent 真实 late-binding 语义（当前 ApuSoC 是空递归）
5. cpptlm.library Python 高级工厂函数（cpu_nested_cluster, memory_cluster_hierarchical）
6. compute_unit_v1.json 蓝图升级（Phase 7.B 接入 GpuComputeUnitTLM 后）

P2 阶段（覆盖率冲刺 95%+）：
7. 5 项残留 △ 补测（TpcCluster 内部 cu 计数, routing="FIFO", l1_size/l2_size 透传, channel_size 透传, 性能 metric 断言）
8. P0 死代码清理后, 删除 3 个 "port not found" REQUIRE_THROWS 测试用例（改成 lazy registration 验证）

P3 阶段（生产化）：
9. .clang-format 自动修复（test_chstream_helpers.cc 等有 16 个格式违规）
10. CHANGELOG.md 时间戳格式统一
11. 当 incorporate_parent 真实语义确定后, 写 ADR 到 docs/superpowers/specs/

KEY FILES
---------
- include/core/sim_module.hh - SimModule 基类（getInternalOutputPort L140, tick 默认递归 L165, incorporate_parent L175, findInternalPath 递归 L149）
- src/core/module_factory.cc - ModuleFactory Step 5/7/8（807 行, ChStream 端口创建在 L637-649）
- include/core/port_manager.hh - PortManager 接口（addDownstreamPort/addUpstreamPort 需检查是否支持带名字重载）
- include/tlm/cluster/{compute,tpc,gpc,gpu}_cluster.{hh,cc} - P2 4 个 GPU cluster
- include/tlm/cluster/{cache,memory,gpu_noc}_cluster.{hh,cc} - P4 3 个基础设施 cluster
- include/tlm/cluster/apu_soc.{hh,cc} - P5 顶层容器（含 incorporate_parent override）
- include/modules_cluster.hh - 9 个 REGISTER_MODULE 集中注册（含 P2-T2.4 重构后的 `const bool _reg_x = (REGISTER_MODULE(X), true)` hack）
- src/tlm/{cache,crossbar}_tlm.cc - P3 helper 方法实现（含 lazy registration + "port not found" 死代码）
- test/test_simmodule_*.cc - 5 个 P1-P5 新测试文件 + 1 个递归 bug 测试
- docs-archived/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md - Spec（860 行）

IMPORTANT DECISIONS
-------------------

### 当前 P0 设计阶段已确认（用户选 B，需选 A/B/C 子方案）

**P0 全套 3 项**（用户选 B 待实施）：

#### 1. D.1 修复（影响 P3 connectCPU + P5 incorporate_parent + CoherentXBarTLM snoop）

**根本原因**（已读 src/core/module_factory.cc:637-649 确认）：
```cpp
// Step 7 创建 ChStream 端口:
req_out_vec[i] = new cpptlm::ChStreamInitiatorPort(
    name + ".req_out" + (n_ports > 1 ? suffix : ""), event_queue);
ch_initiator_ports_.emplace_back(req_out_vec[i]);  // ← 仅存于工厂成员
// 子模块 PortManager 不知道这些端口存在
```
结果: `getInternalOutputPort("cpu0.req_out")` 返回 nullptr, 因为 `cpu0.PortManager` 里没有名为 `req_out` 的 MasterPort。
现有测试已承认限制: test_simmodule_nested.cc:195-206 用 WARN（不是 REQUIRE）捕获 nullptr。

**方案 A (推荐)：镜像方案**
- 位置: src/core/module_factory.cc:649 后插入
- 实施: Step 7 创建 ChStream 端口后, 同步向子模块 PortManager 注册"镜像"
- **关键事实（已验证）**: `ChStreamInitiatorPort IS-A MasterPort` (include/core/chstream_port.hh:17) - 原理可行
- **PortManager API 约束（已验证）**: 现有 addDownstreamPort 签名是 `addDownstreamPort(Owner*, sizes, priorities, label)`, 创建 NEW DownstreamPort<Owner>, 不支持注册已有指针
- **需新增 API** 或修改方案 A 代码:
  ```cpp
  // 方案 A 修订实现:
  // 1) include/core/port_manager.hh 新增 registerMirrorPort 方法:
  void registerMirrorPort(const std::string& label, MasterPort* port) {
      downstream_map[label] = port;  // 直接注册已有指针, 不创建新 port
  }
  void registerMirrorInputPort(const std::string& label, SlavePort* port) {
      upstream_map[label] = port;
  }
  // 2) src/core/module_factory.cc:649 后插入:
  if (ch_mod->hasPortManager()) {
      auto& pm = ch_mod->getPortManager();
      pm.registerMirrorPort(name + ".req_out" + suffix, req_out_vec[i]);
      pm.registerMirrorInputPort(name + ".resp_in" + suffix, resp_in_vec[i]);
      pm.registerMirrorInputPort(name + ".req_in" + suffix, req_in_vec[i]);
      pm.registerMirrorPort(name + ".resp_out" + suffix, resp_out_vec[i]);
  }
  ```
- LOC: ~50 (PortManager 新 API + module_factory.cc 5 行)
- 改动: include/core/port_manager.hh + src/core/module_factory.cc
- 回归: 低（现有 WARN 测试可升级为 REQUIRE 验证修复）
- 解锁: connectCPU + incorporate_parent 真实 wiring + CoherentXBarTLM snoop
- 测试增强: test_simmodule_nested.cc:195-206 升级 WARN→REQUIRE + 新 test_simmodule_d1_chstream_port_visibility.cc（5-8 用例）

**方案 B：端口表重构**
- 位置: src/core/module_factory.cc + .hh + 所有 factory consumer
- 实施: ch_initiator_ports_/ch_target_ports_ 改为 per-module maps 或统一按名查找接口, SimModule 直接查询
- LOC: ~200+
- 改动: module_factory.cc + .hh + 现有 Step 5/7/8 所有消费者
- 回归: 高（可能 break 现有依赖 nullptr 行为的测试）
- 解锁: 同 A + 架构统一
- 测试: 现有所有 chstream 相关测试可能需要更新

**方案 C：不修复，文档化限制**
- 位置: 仅 docs/superpowers/specs/simmodule-d1-limitation.md
- LOC: 0
- 影响: connectCPU helper 不可用, incorporate_parent 真实 wiring 不可达, apu_soc_v1.json 顶层连接需用 factory 路径

#### 2. CoherentXBarTLM 实现

设计草图（用户需在新 session 详细决定）：
- 继承 CrossbarTLM, 加 snoop_broadcast(MasterPort* req) 通道
- MOESI 协议 (per ADR-SOC-01) - 状态表驱动
- 依赖 D.1（需要看到 CacheTLM 的 req_out 才能 snoop）
- 位置: include/tlm/coherent_xbar_tlm.{hh,cc}
- 注册: 加到 modules_cluster.hh: REGISTER_MODULE(CoherentXBarTLM)
- 更新: apu_soc_v1.json 把 "type": "CrossbarTLM" 改为 "CoherentXBarTLM"
- 测试: 新 test_coherent_xbar_tlm.cc（5-8 用例：snoop broadcast, MOESI transitions, response aggregation）

#### 3. 死代码清理

`src/tlm/{cache,crossbar}_tlm.cc` 中:
```cpp
auto* mem_side = getPortManager().getUpstreamPort("mem_side");
// ↑ lazy registration: 如果 port 不存在, 立即 addUpstreamPort(...) 注册空 port
// 因此下面 if (!mem_side) throw 永远不可达
if (!mem_side || !bus_port) {
    throw std::runtime_error("CacheTLM::connectBus: port not found");
}
```

清理选项 (a): 删除 throw 路径, 保留 null bus 抛错（与 lazy 语义一致）
清理选项 (b): 移除 lazy registration, 强制 port 必须预注册（与现有 API 一致但更严格）

### 之前已确认的架构决策（来自 5 Phase 执行）

- **REGISTER_MODULE 宏**: 从无参 statement 重构为参数化模板（T2.4）
  ```cpp
  #define REGISTER_MODULE(T) ModuleFactory::registerModule<T>(#T), true
  // 全局作用域 hack: const bool _reg_x = (REGISTER_MODULE(X), true);
  ```
- **SimModule::simulate_instantiate 必须 virtual**: P2-T2.5 集成测试发现 (TDD 捕获)
- **cfg schema 必须 `{modules: [...], connections: []}`**: P2-T2.5 集成测试发现
- **JSON 路径用 CWD 相对**: "configs/templates/..." 而非 "./templates/..."
- **ComputeCluster 每次 instantiateAll 单次调用**: avoid factory instances_ map replace 语义

### 测试约定（来自 test/AGENTS.md）

- Catch2 v3.7.0（预编译），使用 `TEST_CASE`/`SECTION`/`CHECK`/`REQUIRE`/`WARN`
- 文件名 `test_*.cc` 自动 GLOB 包含
- 标签大小写不敏感
- 现有 88 个 test_*.cc 文件 + 15265 assertions

EXPLICIT CONSTRAINTS
--------------------
None

CONTEXT FOR CONTINUATION
------------------------

**关键警告**：
1. **Oracle 调研失败**: temperature 模型限制。如需类似深度调研，新 session 应直接读代码 + 自分析
2. **现有测试用 WARN 而非 REQUIRE 捕获 nullptr**: D.1 修复后这些 WARN 可升级为 REQUIRE，但需在 P0 fix 时同步修改测试
3. **port_manager.hh addDownstreamPort/addUpstreamPort API**: 实施方案 A 前需先读 port_manager.hh 确认是否有带名字重载（决定能否直接用 `pm.addDownstreamPort(name, port)`）
4. **lazy registration 与 throw 矛盾**: 已确认 throw 路径永远不可达，清理方案 (a) 推荐
5. **build/test 命令**: 
   - 编译: `cmake --build build -j$(nproc)`
   - 测试: `./build/bin/cpptlm_tests 2>&1 | tail -3` (期望 673/673)
   - 全套: `bash scripts/test/run_all_tests.sh 2>&1 | tail -10` (期望 [SUCCESS])
   - docs: `./scripts/test/docs_sync_check.sh --strict`

**新 session 需要做的关键决策**：
- 选 A/B/C 中的 D.1 方案
- 确认 port_manager.hh API 是否支持镜像
- CoherentXBarTLM 协议选择 (MOESI/MESI/MSI)
- 死代码清理 (a/b 选择)
- 实施顺序（D.1 先于 CoherentXBarTLM，二者可并行/串行）

**关键文档引用**：
- Spec: docs-archived/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.1.2 (D.5 fix), §4.2 (Component design), §15 (Decision log)
- Plan: docs-archived/superpowers/plans/2026-06-19-simmodule-complex-hierarchies.md (1557 行)
- ADR: docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md (CoherentXBarTLM 协议)
- 已有架构: docs/soc_arch/specs/apu-soc-design.md §2.2 (APU 蓝图, CPU/GPU cluster 图表)
- Gem5 参考: /workspace/project/gem5/src/python/gem5/components/cachehierarchies/

**已完成 baseline (24 commits, 50 files)**：
- 8 个新 SimModule 类 + ApuSoC 顶层
- 3 个 ChStream helper
- 1 个 REGISTER_MODULE 宏重构
- D.4 + D.5 + incorporate_parent + virtual 修复
- 5 个新 test_*.cc (16 新 TEST_CASE)
- 3 个新 configs/templates/*.json 蓝图
- 6 文档同步 (AGENTS.md × 4, CHANGELOG.md, configs/AGENTS.md)

---

TO CONTINUE IN A NEW SESSION:

1. Press 'n' in OpenCode TUI to open a new session, or run 'opencode' in a new terminal
2. Paste the HANDOFF CONTEXT above as your first message
3. Add your request: "Continue from the handoff context above. I want to discuss方案 A/B/C for D.1 fix before implementing."
