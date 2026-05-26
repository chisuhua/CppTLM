# GUIDE_FOR_FUTURE_WORK.md 审查报告

> **审查日期**: 2026-05-25  
> **方法**: 实际代码交叉验证 (git log, 测试运行, 文件读取, AST 搜索)  
> **状态**: **发现多处严重过时/不准确问题**

---

## 一、审查方法

| 方法 | 数量 |
|------|------|
| Git log 验证 (commit hashes) | 11 commits |
| 文件存在性检查 | 20+ 文件 |
| 代码内容验证 | 8 个关键文件详细阅读 |
| 测试运行 | 531 用例, 14505 assertions, 全部通过 |
| 标签统计 | 173 个标签 |
| 源文档日期交叉检查 | 5 份源报告 |

---

## 二、🔴 严重不准确

### 1. 测试规模严重低估

| 指标 | 指南声明 | 实际 | 偏差 |
|------|---------|------|------|
| 测试文件数 | 43 | **68** (`test/*.cc`) | +58% |
| Python 测试 | 未提及 | **8 个** | — |
| 测试用例数 | 63 (34+29) | **531** | **+743%** |
| 断言数 | 未提及 | **14,505** | — |
| 测试状态 | 54% 覆盖率 | **全部通过** | — |

**结论**: 指南引用的是 2026-04-10 的 `TEST_PLAN_v2.md`，距今已有 223 个 commits，该计划已是严重过时的历史文档。

### 2. Phase 7/8 完全缺失

| Phase | 指南状态 | 实际状态 | 测试用例 |
|-------|---------|---------|---------|
| Phase 7 (交易生命周期) | **未提及** | ✅ 已完成 | 2 cases `[phase7]` |
| Phase 8 (统计/性能/压力) | **未提及** | ✅ 已完成 | 16 cases `[phase8]` |

**证据**:
- `test_phase7_transaction_lifecycle.cc` — 存在
- `test_phase7_benchmark.cc` — 存在
- `test_phase8_performance_stress.cc` — 存在
- `test_phase8_stress.cc` — 存在

### 3. P1.2 latency 注入状态矛盾

| 指南章节 | 声明 | 实际 |
|---------|------|------|
| 第三章 "剩余工作" | ⏳ 待完成, 4-6h | 修复在 `tgms-phase-3-plus` worktree 中 ✅ 已完成 |
| 第一章 "活跃工作流" | ✅ P1.2 latency bug 修复完成 | worktree 中已实现 `int latency = conn.value("latency", 0)` |

**结论**: 同一文档内状态矛盾。P1.2 已在 worktree 中完成，只是尚未合并到 main。

### 4. mem_exts.hh 路径错误

| 指南声明 | 实际路径 |
|---------|---------|
| `include/utils/mem_exts.hh` | `include/ext/mem_exts.hh` |

---

## 三、🟡 中等不准确

### 5. Phase 进度评估偏保守

| 指南声明 | 实际 |
|---------|------|
| Phase 3.3 "部分完成" | 已有 4 个 param 测试文件 + **12** 个 `[phase3.3]` 用例通过 |
| Phase 3.4 "部分完成" | 已有 `validator.py`, `topology_validator.py` 并正常运行 |

**证据**: 标签统计显示：
- `[phase3.3]`: 12 test cases
- `[param]`: 16 test cases
- `[param_errors]`: 2 test cases
- `[parser]`: 9 test cases
- `[derive]`: 1 test case

### 6. ConnectionResolver 文件已重构

| 指南声明 | 实际 |
|---------|------|
| "`connection_resolver.cc:44` latency 被 `(void)` 压制" | 当前 `connection_resolver.cc` (133 行) 已被重构，**不含 `latency` 参数** |
| 修复方案 | 补丁在 worktree 中已应用 (line 41: `conn.value("latency", 0)`) |

### 7. TASK_PLAN_OVERVIEW 中的任务已部分完成

| 指南中的"待实施"任务 | 实际状态 |
|---------------------|---------|
| T3.3-01: `param_rules.hh` | ✅ **已存在** |
| T3.3-02: `param_parser.hh` | ✅ **已存在** |
| T3.3-03: `param_errors.hh` | ✅ **已存在** |
| T3.3-11: 参数系统 C++ 测试 | ✅ **已存在** (4 个测试文件) |

---

## 四、🟢 轻微问题

### 8. CI/CD 工作流章节过时

指南引用的 CI 配置描述与 `.github/workflows/ci.yml` 实际内容需要确认是否一致。

### 9. 测试标签分类过时

指南的第六~九章 (测试补充计划 T7-T12) 基于 April 10 的 `TEST_PLAN_v2.md`。其中许多测试已经实现，不应再列为"待补充"。

### 10. 部分代码审查问题需重新评估

| 代码审查问题 | 实际评估 |
|-------------|---------|
| P0.1: PacketPool ref_count 竞态 | `add_ref()` 是私有方法且**未被调用**，实际风险比描述低。`remove_ref()` 仅在持有锁的 `release()` 内调用 |
| P1: CrossbarTLM 总线冲突 | ✅ **确认存在** — `crossbar_tlm.hh:86` `resp_out[dst].write(resp)` 无冲突检测 |
| P1: TransactionTracker hop 同步 | ✅ **确认存在** — `transaction_tracker.hh:133-135` `(void)event` 注释说明未实现 |

---

## 五、✅ 已验证为准确的内容

### 5.1 所有声称的 Commit 均存在

```
fb94bc9 ✅ fix(memory): use unique_ptr for PortPair
7841631 ✅ chore(python): remove dead noc_builder.py and noc_mesh.py  
8f71654 ✅ fix(wildcard): log regex errors instead of swallowing
c78f7fe ✅ refactor(router): routing_algo_ to unique_ptr
fdc7375 ✅ fix(link): propagate src_port from flit bundle
ee3180f ✅ feat(dashboard): FastAPI server with SSE real-time streaming
4977724 ✅ refactor(dashboard): extract static HTML files
fe983bd ✅ feat(editor): add Svelte-based topology editor
09614ce ✅ chore: update .gitignore
4819184 ✅ fix(router): use = default for destructor (Rule of Zero)
458a9d7 ✅ fix(modules): 多处小修复
```

### 5.2 所有关键文件存在性

```
✅ include/core/port_types.hh
✅ include/core/port_compatibility.hh
✅ include/tlm/router_tlm.hh
✅ include/tlm/crossbar_tlm.hh
✅ include/core/ext/packet_pool.hh
✅ include/core/ext/payload_to_packet.hh
✅ include/framework/transaction_tracker.hh
✅ include/framework/debug_tracker.hh
✅ scripts/linter.py
✅ cpptlm_config/validator.py
✅ include/modules/legacy/
✅ src/traffic_main.cpp
```

### 5.3 代码审查问题确认 (7/7)

| # | 问题 | 确认 |
|---|------|------|
| 1 | RouterTLM 流水线状态 | 存在六阶段流水线实现 |
| 2 | PacketPool ref_count | 设计存在隐患，但 add_ref 为死代码 |
| 3 | CrossbarTLM 冲突 | ✅ 确认 — `resp_out[dst].write(resp)` 无冲突检测 |
| 4 | record_hop 同步缺失 | ✅ 确认 — `(void)event` 注释 |
| 5 | PayloadToPacket 哈希错误 | ✅ 确认 — line 31 `std::hash<uint64_t>` 用于 pair |
| 6 | DebugTracker 初始化 race | ✅ 确认 — line 99 锁外检查 |
| 7 | VC 分配失败未处理 | ✅ 确认 — line 364 返回 NUM_VCS |

---

## 六、根本原因分析

指南的主要不准确源于**源文档严重过时**：

| 源文档 | 创建日期 | 距今 Commits | 问题 |
|--------|---------|-------------|------|
| `TEST_PLAN_v2.md` | 2026-04-10 | 223 commits | 测试数据已完全变化 |
| `MIGRATION_GUIDE.md` | 2026-04-10 | 223 commits | API 可能已变化 |
| `REMAINING_WORK.md` | 2026-05-19 | 较少 | P1.2 状态矛盾 |
| `TASK_PLAN_OVERVIEW.md` | 2026-05-23 | 近 | 部分任务实际已实现 |
| `CODE_REVIEW_2026-05-25.md` | 2026-05-25 | 当日 | 有效，代码审查准确 |

---

## 七、修复建议

### 立即修复 (指南自身)

1. **更新测试数据**: 531 用例 / 14505 assertions / 全部通过
2. **补充 Phase 7/8**: 添加已完成的 Phase 7 (交易生命周期) 和 Phase 8 (统计/性能/压力)
3. **修正 P1.2 状态**: 标记为"worktree 中已完成，待合并"
4. **修正 mem_exts.hh 路径**: `include/ext/mem_exts.hh`
5. **删除"测试补充计划" (第六章)**: 大部分测试已实现，应重新基于当前 531 用例做 gap analysis
6. **更新 Phase 3.3/3.4 进度**: 部分任务实际已完成

### 建议流程改进

1. **指南应基于动态数据** — 测试结果应直接从 `cpptlm_tests --list-tests` 获取，而非手动维护
2. **源文档版本标记** — 每份源文档应标记"最后验证日期"和"最后验证 commit"
3. **P1.2 优先合并** — worktree 中的 latency 修复应尽快合并到 main，消除状态矛盾
4. **定期更新 CODE_REVIEW** — 代码审查报告很及时 (2026-05-25)，应保持

---

## 八、验证命令汇总

```bash
# 最新测试状态
./build/bin/cpptlm_tests

# 按标签查看
./build/bin/cpptlm_tests --list-tests "[phase3.3]"

# 查看所有标签
./build/bin/cpptlm_tests --list-tags

# 验证 git 历史
git log --oneline -30

# 检查文件存在性
ls -la include/ext/mem_exts.hh
```

---

*审查人: Sisyphus | 审查日期: 2026-05-25 | 基于实际代码库 HEAD c8eacba*
