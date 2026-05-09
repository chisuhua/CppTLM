# CppTLM Technical Debt Remediation Plan

**Created:** 2026-05-09
**Status:** Ready for Execution
**Based on:** 4-parallel analysis (Architecture, Duplication, Technical Debt, C++ Modernization) + Oracle调研 + Momus审查

---

## Executive Summary

| 维度 | 关键发现 |
|------|---------|
| 架构对齐 | 3处 CRITICAL layer violations，1处 circular dependency |
| 代码重复 | 2个死代码 Python 文件，linter/validator 功能重叠 |
| 技术债务 | 2个 HIGH 内存泄漏，1个 HIGH 功能缺失(latency)，12个已知失败测试 |
| C++ 现代化 | 债务较轻，1个 HIGH（`routing_algo_` raw pointer） |

**总债务:** 21 verified findings，分布在 15+ 文件中

---

## P0 — Immediate (Fix This Week)

> 所有 P0 项可并行执行，无相互依赖。

### P0.1: 修复 PortPair 内存泄漏

**文件:** `src/core/module_factory.cc:752, 934, 941`

**问题:** 3处 `new PortPair(...)` 无对应 `delete`，每次仿真运行都会泄漏。

**操作:**
```cpp
// src/core/module_factory.cc:752
// BEFORE:
new PortPair(src_port, dst_port);
// AFTER:
auto pp = std::make_unique<PortPair>(src_port, dst_port);
```

**依赖项:** 无
**预估工时:** 1-2 小时
**验证:**
```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
# ASan build:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
# 运行后确认无 "Direct leak" for PortPair
```

---

### P0.2: 删除死代码 Python 文件（2个）

**文件:** `python/noc_mesh.py`, `python/noc_builder.py`

**问题:**
- `noc_mesh.py` 引用未定义符号 `VcRouter`, `TerminalNode`, `build_mesh_connections`, `connect_terminals`
- `noc_builder.py` 唯一消费者是 `noc_mesh.py`，删除后者即孤化前者

**操作:**
```bash
git rm python/noc_mesh.py python/noc_builder.py
```

**依赖项:** 无
**预估工时:** 15 分钟
**验证:**
```bash
python -c "from python import noc_builder"  # 应抛出 ImportError
python -c "from python import noc_mesh"    # 应抛出 ImportError
```

---

### P0.3: 修复 wildcard.hh 空 catch 块

**文件:** `include/utils/wildcard.hh:28`

**问题:** `catch(...)` 吞噬所有异常，调试时无法定位 `std::regex_error`。

**操作:**
```cpp
// BEFORE:
} catch (...) {
    return pattern == str;
}

// AFTER:
} catch (const std::regex_error& e) {
    DPRINTF(WILDCARD, "[WARN] Invalid regex pattern '%s': %s\n",
            pattern_str.c_str(), e.what());
    return false;
} catch (const std::exception& e) {
    DPRINTF(WILDCARD, "[WARN] Unexpected error in wildcard match: %s\n",
            e.what());
    return false;
}
```

**依赖项:** 无（仅 `wildcard.hh` 内部修改）
**预估工时:** 30 分钟
**验证:**
```bash
# 添加测试用例：传入 "[invalid" 等非法正则
./build/bin/cpptlm_tests "[wildcard]" --output-on-failure
```

---

## P1 — This Sprint (Fix Within 2 Weeks)

> 预估总工时: 8-14 小时

### P1.1: routing_algo_ 裸指针 → unique_ptr

**文件:**
- `include/tlm/router_tlm.hh:253` — 声明
- `src/tlm/router_tlm.cc:56, 67, 103-107` — 使用处

**问题:** `RoutingAlgorithm*` 手动 `new`/`delete`，违反现代 C++ 所有权规范。

**TDD 步骤:**

**RED:** 编写测试——创建 RouterTLM，注入自定义 algorithm，析构时验证无泄漏
```cpp
TEST_CASE("RouterTLM custom routing algo no leak") {
    auto router = std::make_unique<RouterTLM>(/* ... */);
    router->set_routing_algorithm(std::make_unique<XYRouting>());
    router.reset();  // 触发析构
    // ASan 验证无泄漏
}
```

**GREEN:** 修改代码
```cpp
// router_tlm.hh:253
// BEFORE:
RoutingAlgorithm* routing_algo_ = nullptr;
// AFTER:
std::unique_ptr<RoutingAlgorithm> routing_algo_;

// router_tlm.cc:103-107
// BEFORE:
void RouterTLM::set_routing_algorithm(RoutingAlgorithm* algo) {
    delete routing_algo_;
    routing_algo_ = algo;
}
// AFTER:
void RouterTLM::set_routing_algorithm(std::unique_ptr<RoutingAlgorithm> algo) {
    routing_algo_ = std::move(algo);
}
```

**REFACTOR:** 运行 router 相关测试
```bash
./build/bin/cpptlm_tests "[router]" --output-on-failure
```

**依赖项:** 无
**预估工时:** 3-4 小时
**验证:** ASan build + router tests 全部通过

---

### P1.2: 实现 connection_resolver.cc 延迟注入

**文件:** `src/core/connection_resolver.cc:44`

**问题:** `latency` 参数被 `(void)` 压制，JSON 配置中的连接延迟完全失效。

**TDD 步骤:**

**RED:** 编写测试——连接配置中设置 `"latency": 5`，断言端口延迟等于 5
```bash
# 临时创建测试配置 configs/test_latency_5.json
# 运行仿真，验证数据包延迟
```

**GREEN:** 修改代码
```cpp
// 1. 在 PortCreationInfo 结构体中添加 latency 字段（如果尚未存在）
// 2. 将 latency 传播到 createPortFunc lambda
// 3. 在 module_factory.cc 中调用 setDelay(latency)
```

**REFACTOR:** 确认默认行为不变（latency=0 时现有测试仍通过）

**依赖项:** 理解 `module_factory.cc` 中 `createPortFunc` 的使用（lines 622-640）
**预估工时:** 4-6 小时
**验证:**
```bash
./build/bin/cpptlm_tests "[connection]" --output-on-failure
```

---

### P1.3: 修复 link_tlm.cc 硬编码 src_port = 0

**文件:** `src/tlm/link_tlm.cc:69`

**问题:** `df.src_port = 0` 破坏 credit 返回路由——所有 credit 都返回到上游端口 0。

**Oracle 决策: Option A** — 在 `NoCFlitBundle` 添加 `src_port` 字段

**TDD 步骤:**

**RED:** 编写测试——多端口拓扑中，LinkTLM 接收来自不同 src_port 的 flit，验证 credit 返回到正确的上游端口
```bash
# 测试场景：Router0 端口1 → Link0 → Router1 端口3
# 验证 credit 返回到 Router0 的端口1（而非端口0）
```

**GREEN:** 修改代码

**Step 1:** 在 `NoCFlitBundle` 添加字段
```cpp
// include/bundles/noc_bundles_tlm.hh
// 在 src_node/dst_node 之后添加：
ch_uint<8>  src_port;   // 上游 Router 的输出端口索引
```

**Step 2:** RouterTLM 发送时填充 `src_port`
```cpp
// src/tlm/router_tlm.cc — 发送 flit 时
flit.src_port = out_port;  // 已发送的端口编号
resp_out[out_port]->push(flit);
```

**Step 3:** LinkTLM 读取而非硬编码
```cpp
// src/tlm/link_tlm.cc:69
// BEFORE:
df.src_port = 0;  // TODO: 从 flit 中获取源端口信息
// AFTER:
df.src_port = flit.src_port.read();
```

**向后兼容:** 旧 flit 无 `src_port` 字段时默认 0（等同于当前行为）

**依赖项:** 无（纯加字段，向后兼容）
**预估工时:** 2-3 小时
**验证:**
```bash
./build/bin/cpptlm_tests "[link]" --output-on-failure
```

---

### P1.4: 清理冗余配置文件

**文件:** `configs/` 下的 3 对重复文件

| 应删除 | 保留 |
|--------|------|
| `configs/mesh_2x2.json` | `configs/mesh_2x2_tlm.json` |
| `configs/hierarchical_2x2.json` | `configs/hierarchical_2x2_tlm.json` |
| `configs/ring_8.json` | `configs/ring_8_tlm.json` |

**Oracle 决策:** `_tlm` 后缀是迁移标记，非永久特性。在 v2.1 中去后缀化为规范名。

**操作:**
```bash
# 1. 确认 _tlm 版本是超集
diff configs/mesh_2x2.json configs/mesh_2x2_tlm.json

# 2. 删除非 _tlm 版本
git rm configs/mesh_2x2.json configs/hierarchical_2x2.json configs/ring_8.json

# 3. 更新引用（非 _tlm 文件无测试引用，可确认安全）
git grep -l "mesh_2x2.json" -- test/ configs/
```

**依赖项:** 无
**预估工时:** 1-2 小时
**验证:**
```bash
./build/bin/cpptlm_tests --output-on-failure
```

---

## P2 — Next Sprint (Thematic Buckets)

### Bucket A: Python 代码库清理

| # | 任务 | 文件 | 操作 |
|---|------|------|------|
| A.1 | 删除 noc_builder.py | `python/noc_builder.py` | 已于 P0.2 同步删除 |
| A.2 | 合并 linter 独有检查 → validator | `scripts/linter.py` → `cpptlm_config/validator.py` | 迁移 W001(self-loop)、W002(duplicate-connection) 到 validator，再删除 linter.py |

**A.2 详细步骤:**

1. 在 `cpptlm_config/validator.py` 中添加 linter 独有检查：
   - `E001`: 模块缺少 name（validator 已有 VALID-01/02）
   - `E002`: 模块缺少 type
   - `W001`: 自环检测（`src == dst`）
   - `W002`: 重复连接检测
2. 验证 `python scripts/topology_validator.py configs/mesh_2x2_tlm.json` 输出不变
3. 删除 `scripts/linter.py`

**依赖:** A.2 须在删除 linter.py 之前完成
**预估工时:** 2-3 小时

---

### Bucket B: C++ 现代化

| # | 任务 | 文件 | 操作 | 风险 |
|---|------|------|------|------|
| B.1 | `typedef` → `using` | `include/ext/mem_exts.hh` (6处) | 纯机械转换，无行为变更 | 低 |
| B.2 | C 风格数组 → `std::array` | `include/core/cmd.hh:46,47,74` | `uint8_t data[64]` → `std::array<uint8_t, 64>` | 低 |
| B.3 | 测试中 PortPair new → unique_ptr | `test/test_*.cc` (6处) | 参照 P1.1 模式 | 低 |

**SKIP (受约束):**
- `packet_pool.hh` 手动引用计数（已尝试 unique_ptr 并回退，AGENTS.md 约束）
- `tlm_stub.hh` 裸指针（SystemC ABI 约束）

---

### Bucket C: Legacy 模块正式废弃

| # | 任务 | 文件 | 操作 |
|---|------|------|------|
| C.1 | 审计 modules_v2.hh 使用者 | `include/modules/legacy/modules_v2.hh` | `git grep -l "CrossbarV2\|MemoryV2\|CacheV2"` 确认无消费者 |
| C.2 | 添加 `[[deprecated]]` | `include/modules/legacy/modules_v2.hh` | 确认无消费者后标记 |
| C.3 | 更新 README | `include/modules/legacy/README.md` | 添加映射表：CacheV2→CacheTLM, MemoryV2→MemoryTLM, CrossbarV2→CrossbarTLM |

**依赖:** C.2 须在 C.1 确认无消费者后执行
**预估工时:** 2 小时

---

### Bucket D: 日志一致性

| # | 任务 | 文件 | 操作 | 风险 |
|---|------|------|------|------|
| D.1 | `printf` → `DPRINTF` | `src/core/module_factory.cc` (~20处) | 替换为 `DPRINTF(CONFIG, ...)` | 低 |
| D.2 | 移除/实现占位符 main | `src/main_hierarchy.cpp`, `src/main_layout.cpp` | 删除或实现真实功能 | 低 |
| D.3 | `std::cout` → `DPRINTF` | `src/utils/dynamic_loader.cc` | 替换为 `DPRINTF(LOADER, ...)` | 低 |

**依赖:** 无
**预估工时:** 3-4 小时

---

## P3 — Backlog (Future Sprints)

### P3.1: 12个已知失败测试分类

**现状:** Pool/Wildcard/Connection 相关测试失败——Phase 0-6 未修改这些代码，历史遗留。

**操作:**
1. 运行完整测试套件，统计具体失败数量和名称
2. 按根因分类：实现 bug vs 架构设计缺陷 vs 测试本身损坏
3. 对每个类别的测试打标签：`[.skip:gh#XXX]` 或标记为已知失败

**Oracle 需要决定:** 哪些测试应修复（< 1天）vs 哪些需要架构级修改

---

### P3.2: core/framework 跨层循环依赖

**现状:** `framework/bidirectional_port_adapter.hh:12` → `core/chstream_port.hh:9` → `framework/stream_adapter.hh` 形成循环。

**Oracle 需要决定:**
- Option A: 将 `StreamAdapterBase` 接口移到 `core/`（framework 实现 core 接口）
- Option B: 引入 `core/stream_adapter_interface.hh` 纯虚基类，前向声明打破循环
- Option C: 接受现状并记录架构约束

**预估工时:** 待 Oracle 决策后估算

---

### P3.3: 在 CI 中启用 ASan

**操作:** 在 `.github/workflows/ci.yml` Debug build 中添加 `-fsanitize=address`

**收益:** 每次 PR 自动检测内存泄漏
**风险:** ~2x CI  slowdown，仅 Debug 构建受影响，可接受

---

## 依赖关系图

```
P0.1 ──┐
P0.2 ──┤──► P0 (全部并行)
P0.3 ──┘

P1.1 ──────┐
P1.2 ──────┼──► P1 (内部并行，P1.2/3 依赖 P1.1 理解路由模式)
P1.3 ──────┘
     │
P1.4 ──────► 独立

P2.A.2: linter checks 迁移 ──► P2.A: 删除 linter.py
P3.1, P3.2: Oracle 决策 ──► 执行
```

---

## 原子提交策略

```bash
# === P0 提交 ===
git commit -m "fix(memory): use unique_ptr for PortPair in module_factory

3处 new PortPair 无对应 delete，每次仿真泄漏。
ASan verified clean.
Refs: module_factory.cc:752,934,941"

git commit -m "chore(python): remove dead noc_builder.py and noc_mesh.py

两个文件互相引用但均无其他消费者。
noc_mesh.py 引用未定义符号 (VcRouter, TerminalNode)。
Refs: python/noc_builder.py, python/noc_mesh.py"

git commit -m "fix(wildcard): log regex errors instead of swallowing

catch(...) 静默吞噬 std::regex_error。
添加 DPRINTF 日志输出具体错误信息。
Refs: include/utils/wildcard.hh:28"

# === P1 提交 ===
git commit -m "refactor(router): routing_algo_ to unique_ptr

消除手动 new/delete 对。set_routing_algorithm
改用 move 语义。ASan verified + router tests passing.
Refs: router_tlm.hh:253, router_tlm.cc:56,67,105"

git commit -m "feat(connection): implement latency in ConnectionResolver

latency 参数从 JSON connections传播至端口延迟。
新增 latency=5 测试验证。现有测试 latency=0 行为不变。
Refs: connection_resolver.cc:44"

git commit -m "fix(link): propagate src_port from flit bundle

NoCFlitBundle 新增 src_port 字段。
LinkTLM 从 flit 读取而非硬编码 0。
向后兼容：旧 flit 默认 src_port=0。
Refs: link_tlm.cc:69, noc_bundles_tlm.hh"

git commit -m "chore(configs): remove legacy non-tlm variants

删除 mesh_2x2.json, hierarchical_2x2.json, ring_8.json。
保留 _tlm 版本作为规范。v2.1 将去后缀化。
Refs: configs/"

# === P2 提交 ===
git commit -m "refactor(linter): merge W001/W002 into TopologyValidator

self-loop 和 duplicate-connection 检查迁移至
cpptlm_config/validator.py。scripts/linter.py 删除。
Refs: scripts/linter.py, cpptlm_config/validator.py"

git commit -m "style(c++): modernize typedef to using in mem_exts

6处 typedef ReadCmd data_t 改为 using data_t = ReadCmd。
无行为变更，纯风格统一。
Refs: include/ext/mem_exts.hh"

git commit -m "style(cmd): convert C arrays to std::array in cmd.hh

uint8_t data[64] 改为 std::array<uint8_t, 64>。
启用 .fill(0) 和 .at() 边界检查。
Refs: include/core/cmd.hh:46,47,74"
```

---

## 验证命令汇总

```bash
# 完整构建 + 测试
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# 分模块测试
./build/bin/cpptlm_tests "[router]"      # P1.1
./build/bin/cpptlm_tests "[link]"        # P1.3
./build/bin/cpptlm_tests "[connection]"   # P1.2
./build/bin/cpptlm_tests "[wildcard]"     # P0.3

# Python 测试
python -m pytest test/python/ -v
```

---

## 执行检查点

- [ ] P0.1: PortPair 内存泄漏修复 + ASan 验证
- [ ] P0.2: Python 死代码文件删除
- [ ] P0.3: wildcard 日志修复 + 测试通过
- [ ] P1.1: routing_algo_ unique_ptr + router tests 通过
- [ ] P1.2: latency 注入实现 + connection tests 通过
- [ ] P1.3: src_port 传播 + link tests 通过
- [ ] P1.4: 冗余配置文件清理 + 全量测试通过
- [ ] P2.A: linter 合并 + validator tests 通过
- [ ] P2.B: C++ 风格现代化（低风险）
- [ ] P2.C: Legacy modules 审计 + deprecation 标记
- [ ] P2.D: 日志一致性修复
- [ ] P3: Oracle 决策后执行
