# Pre-existing Issues — cpptlm-cleanup Boulder 遗留

> **来源**: cpptlm-cleanup boulder (2026-06-08) 执行期间发现
> **状态**: 全部为 **out of plan scope**（plan "Must NOT do" 显式禁止修改），记录供后续 PR 处理
> **优先级**: 中（不阻塞 cpptlm-cleanup 主目标，但影响 .clang-format CI 检查、example 编译、validate_topology combined target）

---

## Issue #1: `.clang-format:26` 含已弃用 key

**类型**: 配置文件问题（pre-existing）
**严重度**: 中（导致 `./scripts/format.sh --check` 报告 "Error reading .clang-format: Invalid argument"）
**位置**: `/workspace/project/CppTLM/.clang-format:26`

### 症状
```
$ ./scripts/build/format.sh --check
Error reading /workspace/project/CppTLM/.clang-format: Invalid argument
.clang-format:26:1: error: unknown key 'IndentNamespaces'
IndentNamespaces: true
^~~~~~~~~~~~~~~~
```

### 根因
clang-format 22.1.6 已弃用 `IndentNamespaces` key（被 `NamespaceIndentation: All` 替代 — 项目 .clang-format 同一文件 line 34 已正确使用新 key）。

### 修复方案
1. **方案 A**（推荐）: 删除 line 26 的 `IndentNamespaces: true`
2. **方案 B**: 同时使用 `NamespaceIndentation: All`（已存在 line 34，可能有冲突）
3. **方案 C**: 保留 key + 升级 clang-format 到更新版本

### 影响范围
- `format.sh --check` CI 步骤（`.github/workflows/ci.yml:code-format` job）已失败
- 不影响 C++ 编译/测试

---

## Issue #2: `examples/example_basic_transaction.cc:38` Packet 私有构造

**类型**: 源码 bug（pre-existing，example 文件）
**严重度**: 中（导致 `example_basic_transaction` target 无法编译）
**位置**: `/workspace/project/CppTLM/examples/example_basic_transaction.cc:38`

### 症状
```
/workspace/project/CppTLM/examples/example_basic_transaction.cc:38:50: error:
  'Packet::Packet(tlm::tlm_generic_payload*, uint64_t, PacketType)' is private within this context
```

### 根因
`Packet` 构造函数在 `include/core/packet.hh:249` 声明为 `private`，仅 `friend class PacketPool` 可访问。example 文件用 `new Packet(&payload, 0, PKT_REQ)` 直接构造，违反封装。

### 修复方案
1. **方案 A**（推荐）: 改用 `PacketPool::get().acquire()` API：
   ```cpp
   Packet* pkt = PacketPool::get().acquire();
   pkt->payload = &payload;  // 或在 acquire 后通过 pool 注入
   pkt->set_transaction_id(tid);
   ```
2. **方案 B**: 改用 `PacketPool` 的工厂方法（如果存在）
3. **方案 C**: 把 example_basic_transaction 移到 `docs-archived/examples-orphaned/`（参考 simple1/simple_hier 归档模式）

### 影响范围
- `example_basic_transaction` target 编译失败
- cpptlm_sim 主功能不受影响

---

## Issue #3: `examples/example_error_handling.cc:88,96` Packet 私有构造（同 #2）

**类型**: 源码 bug（pre-existing，example 文件）
**严重度**: 中（导致 `example_error_handling` target 无法编译）
**位置**: `/workspace/project/CppTLM/examples/example_error_handling.cc:88` 和 `:96`

### 症状
```
/workspace/project/CppTLM/examples/example_error_handling.cc:88:52: error:
  'Packet::Packet(tlm::tlm_generic_payload*, uint64_t, PacketType)' is private within this context
```

### 根因
同 Issue #2。example_error_handling.cc 有两处（`:88` 和 `:96`）使用错误 API。

### 修复方案
同 Issue #2 方案 A。

### 关联
同文件 line 134 已有正确用法：`PacketPool::get().release(pkt1);` —— 说明 release 路径正确，acquire 路径未更新。

---

## Issue #4: `examples/example_error_handling.cc:127` CoherenceState 无 operator<<

**类型**: 源码 bug（pre-existing，example 文件）
**严重度**: 中（导致 `example_error_handling` target 编译失败）
**位置**: `/workspace/project/CppTLM/examples/example_error_handling.cc:127`

### 症状
```
/workspace/project/CppTLM/examples/example_error_handling.cc:127:29: error:
  no match for 'operator<<' (operand types are 'std::basic_ostream<char>' and 'const CoherenceState')
```

### 根因
`CoherenceState`（定义于 `include/core/...`）未实现 `operator<<(std::ostream&, CoherenceState)`。example 尝试 `std::cout << snap.from_state` 但无重载。

### 修复方案
1. **方案 A**（推荐）: 在 `include/core/...` 添加：
   ```cpp
   inline std::ostream& operator<<(std::ostream& os, CoherenceState s) {
       return os << coherence_state_to_string(s);
   }
   ```
   需要 `coherence_state_to_string()` 辅助函数（如果不存在需新建）
2. **方案 B**: 在 example 文件本地用 `switch` 或 `if/else` 转换为字符串
3. **方案 C**: 改为 C++23 `std::format`（`{:^}` style）

### 影响范围
- `example_error_handling` target 编译失败
- 核心库不受影响

---

## Issue #5: `configs/ring_8_tlm.json` VALID-02 Unreachable Terminals

**类型**: 配置文件问题（pre-existing）
**严重度**: 低（导致 `validate_topology` combined target 失败，但 4 个独立 target 中 3 个 PASS）
**位置**: `/workspace/project/CppTLM/configs/ring_8_tlm.json`

### 症状
```
[TopologyValidator] /workspace/project/CppTLM/configs/ring_8_tlm.json
  FAIL: [VALID-02] Unreachable terminals from node_0: ['node_1', 'node_2', 'node_3', 'node_4', 'node_5', 'node_6', 'node_7']
  Suggestion: Check connectivity graph
  VALIDATION FAILED
```

### 根因
`ring_8_tlm.json` 配置的 ring 拓扑（8 节点环形）应该每个节点可达其他节点（BFS 验证）。但拓扑定义可能有向边方向设置错误或连接缺失。

### 修复方案
1. **方案 A**（推荐）: 修复 ring_8_tlm.json 的 connections 段，确保每个节点有 to/from 邻居
2. **方案 B**: 重新用 `topology_generator.py --type ring --nodes 8` 生成
3. **方案 C**: 暂时把 ring_8 从 validate_topology combined target 移除

### 影响范围
- `cmake --build build --target validate_topology` combined target 失败
- 4 个独立 target（`validate_mesh_2x2`、`validate_mesh_4x4`、`validate_hierarchical`、`validate_ring_8`）仍可单独跑

---

## 后续 PR 建议

### 单 PR 修复（推荐）
**PR 标题**: `fix: address pre-existing example/config/format issues from cpptlm-cleanup boulder`

**范围**:
1. 修复 `.clang-format:26` 删除 `IndentNamespaces`（Issue #1）
2. 修复 `examples/example_basic_transaction.cc:38` 用 `PacketPool::get().acquire()`（Issue #2）
3. 修复 `examples/example_error_handling.cc:88,96` 同上（Issue #3）
4. 在 `include/core/coherence_state.hh` 或 example 本地添加 `operator<<`（Issue #4）
5. 修复 `configs/ring_8_tlm.json` 拓扑连接或重新生成（Issue #5）

**风险评估**:
- Issue #1 风险极低（删除已弃用 key）
- Issue #2/3 风险中（需确认 PacketPool::acquire() 返回的 Packet 行为与 new Packet 相同 — 主要是 payload 绑定）
- Issue #4 风险低（添加小段 operator<< 重载）
- Issue #5 风险低（修改/重新生成 config）

**预计 commit 数**: 5 atomic commits（按 issue 一对一）

### 文档同步
AGENTS.md "WHERE TO LOOK" 表 "samples/" 段可添加：
- example_*.cc 编译前提（PacketPool 替代 new Packet）
- 配置文件有效性保证（topology_generator 重新生成）

---

## 验证（修复后）

```bash
# 全部 5 个 issue 修复后：
./scripts/build/format.sh --check            # Issue #1 — 应通过
cmake --build build --target example_basic_transaction example_error_handling -j$(nproc)  # Issue #2/3/4 — 应成功
cmake --build build --target validate_topology  # Issue #5 — 4/4 PASS
./build/bin/examples/example_basic_transaction  # 应输出"=== CppTLM v2.0 基础交易追踪示例 ==="
./build/bin/examples/example_error_handling     # 应输出"=== 示例完成 ==="
```

---

## 参考

- cpptlm-cleanup plan: `/workspace/project/CppTLM/.omo/plans/cpptlm-cleanup.md`
- Boulder outcome: `/workspace/project/CppTLM/.omo/boulder.json` (cpptlm-cleanup-cbb2cf0e)
- 原始 evidence: `/workspace/project/CppTLM/.omo/evidence/task-10-verification.txt`, `task-17-verification.txt`
- 18 commits: `git log --oneline 68fce6c^..HEAD` in main branch
