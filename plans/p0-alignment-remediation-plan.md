# CppTLM 文档—代码对齐修复计划 (P0 Alignment Remediation)

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **状态**: ⏳ 待开始 (checkbox) / 🟡 部分已完成 (实测) — **2026-06-09 状态审计**
> **创建日期**: 2026-06-07
> **创建者**: Sisyphus (AI Architect) + Oracle 验证
> **关联审查**: 本仓库 4 项 P0 + 4 项新风险（详见审查报告）
> **预计工期**: 5-6 个工作日（单人）
> **破坏性**: 中（改 4 个 P0 + 1 个测试，必触发回归但零 .disabled 约束）
>
> ⚠️ **2026-06-09 审计警告**: 本计划创建于 2026-06-07,checkbox 0/101 未更新。但经实测代码,P0-#1 (字符串小写) 已于某时间点落地;P0-#2 (v3 归档) 已于 2026-06-09 由本文操作 (commit 即将生成) 落地;P0-#3/#4 状态待重新审计。详见下方"2026-06-09 状态审计"章节。

---

## Goal

修复 CppTLM 项目文档—代码对齐审查中识别的 **4 项 P0 不一致**与 **2 项新风险**，使项目从"声称端到端联通"变为"实际端到端联通"，并把 v3 hybrid 设计文档归档、冻结 RTL Spike 状态。

**Success Criteria（必须全部满足）**:
1. ✅ `./build/bin/cpptlm_tests` 全绿（baseline 数量 + 至少 +1 新 E2E 测试，baseline 动态获取：`./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1`）
2. ✅ 新增 TEST_CASE "Phase 6: E2E data flow cache→xbar→mem" PASS
3. ✅ `port_types.hh` JSON 字符串与 ADR-X.9 §决策 1 一致（小写）
4. ✅ CrossbarTLM/ArbiterTLM2/ArbiterTLM4 通过 `bind_port_pair` 真正绑定端口（非 WARN 后放空）
5. ✅ P0-#2 决策落地（ch_stream 验证或回退 v3 拆分）
6. ✅ `hybrid_tlm_cppHDL_design_v3.md` 归档到 `docs-archived/`
7. ✅ ADR-X.9/6/13 + chppHDL_api_verification.md 状态同步

---

## Architecture Context

### 现状（待修复）

| 组件 | 现状 | 目标 |
|---|---|---|
| PortRole/BundleType 字符串 | 大写 `"INITIATOR"` 等 | 小写 `"initiator"`（对齐 ADR-X.9） |
| CrossbarTLM 内部 | 双指针数组 + Step 7 不调用 bind | 单指针 + Step 7 用 dynamic_cast 真正 bind |
| Packet::reset() | 不清理 Extension | 显式 `release_extension<>`（防御性） |
| HybridCacheComponent | `ch_stream<Bundle>` 作 `__io` 端口 | **待 smoke test 决定**（B 选项） |
| test_phase6_integration.cc | 纯烟雾测试（零事务断言） | 新增真 E2E 测试覆盖 |
| docs/architecture/examples/hybrid/v3 | 仍在主目录 | 归档到 `docs-archived/` |

### 关键依赖链

```text
P0-#1 (字符串小写) ──→ P0-#3 (Step 7 多端口) ──→ P0-#2 (ch_stream 验证)
   │                       │                          │
   │                       ▼                          │
   │                  新 E2E 测试 PASS                │
   │                       │                          │
P0-#4 (reset 清理) ───────┘                          │
                                                    │
                              若 P0-#2 B 失败 → 选 A (回退 v3)
```

### 2026-06-09 状态审计（Sisyphus 重新验证）

> **审计人**: Sisyphus (本会话)  
> **审计方法**: 逐项 grep 实际代码 + git log 验证 + 文件存在性检查  
> **结论**: checkbox 严重失真,本计划**实际进度约 1/4 P0 项已落地**, 需 Sisyphus 重新评估

| 编号 | 计划声称"待修复" | 实际代码状态 | 验证命令 | 评估 |
|---|---|---|---|---|
| **P0-#1** | `port_types.hh` PortRole 字符串大写 | **已小写** | `grep -E '\{PortRole::' include/core/port_types.hh` → `{INITIATOR, "initiator"}` | ✅ **已完成**(commit author 待追溯) |
| **P0-#1** | `port_types.hh` BundleType 字符串大写 | **已小写** | `grep -E '\{BundleType::' include/core/port_types.hh` → `{CACHE_REQ, "cache_req"}` | ✅ **已完成** |
| **P0-#2** | `hybrid_tlm_cppHDL_design_v3.md` 需归档到 `docs-archived/` | **未归档** | `ls docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v3.md` → 仍在主目录 | ❌ **本审计同时归档** (本会话) |
| **P0-#2** | smoke test `test/rtl/smoke_chstream_rtl.cc` | **未创建** | `ls test/rtl/smoke_chstream_rtl.cc` → 不存在 (test/rtl/ 仅含 CMakeLists.txt) | ⏳ **未开始** |
| **P0-#3** | CrossbarTLM 单指针化 + Step 7 dispatch | **未修复** | `include/tlm/crossbar_tlm.hh` 仍 `cpptlm::StreamAdapterBase* adapter[NUM_PORTS]` (双指针); `module_factory.cc:594` 仍 `[WARN] Multi-port module uses set_stream_adapter(array)` | ⏳ **未开始** |
| **P0-#4** | `Packet::reset()` 显式 release Extension | **未修复** | `sed -n '224,250p' include/core/packet.hh` → reset() 内**无** `release_extension<>` 调用 | ⏳ **未开始** |
| **新风险#1** | test_phase6_integration.cc 直接驱动 | **未删除** | `grep -n "xbar.req_in\[0\]" test/test_phase6_integration.cc` → 待验证 | ⏳ **未开始** |
| **新风险#1** | 新增 E2E 测试 "Phase 6: E2E data flow cache→xbar→mem" | **未添加** | `grep -r "E2E data flow" test/` → 无匹配 | ⏳ **未开始** |

**审计建议**:

1. ✅ **P0-#1 已完成** — 但 plan checkbox 未更新 (T1.1.1/T1.1.2),应立即勾选
2. ✅ **P0-#2 v3 归档** — 本审计会话已归档 (commit 待生成)
3. ⏳ **P0-#3/P0-#4/新风险#1** — 仍待实施
4. 🟡 **Sprint 状态** — 7 天 Sprint 实际仅完成 2/7 项 (28%),worktree 仍在 Day 0 阶段

**Sisyphus 推荐**: 本计划 checkbox 失真问题比"完成度低"更危险,新 boulder 若读取此 plan 会误判 0/101 工作量。建议:

- (本会话) 勾选 T1.1.1/T1.1.2 (P0-#1 字符串小写)
- (本会话) 勾选 T6.2.2 (v3 归档)
- 保留其余 99 项未完成状态,等待后续 boulder 实施

### 修改文件清单

| 类别 | 文件 | 改动行数 | P0 编号 |
|---|---|---|---|
| 修改 | `include/core/port_types.hh` | ~10 行 | P0-#1 |
| 修改 | `include/tlm/crossbar_tlm.hh` | ~10 行 | P0-#3 |
| 修改 | `include/tlm/arbiter_tlm.hh` | ~8 行 | P0-#3 |
| 修改 | `src/core/module_factory.cc` | ~25 行 | P0-#3 |
| 修改 | `include/core/packet.hh` | ~5 行 | P0-#4 |
| 修改 | `include/rtl/hybrid_cache_component.hh` | ~22 行 (若 B 失败) | P0-#2 |
| 修改 | `src/rtl/hybrid_cache_component.cc` | ~30 行 (若 B 失败) | P0-#2 |
| 修改 | `test/test_phase6_integration.cc` | -20 行 + +45 行 | 新风险#1 |
| 新增 | `test/rtl/smoke_chstream_rtl.cc` | ~30 行 (D4) | P0-#2 验证 |
| 移动 | `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v3.md` | → `docs-archived/` | 文档清理 |

---

## Tech Stack

- **C++**: C++17/20 混合（CppHDL 部分需 C++20）
- **测试**: Catch2 v3.7.0（73 个测试文件，baseline 数量动态获取：`./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1`）
- **构建**: CMake 3.16+ + Ninja
- **CppHDL**: `external/CppHDL` 子模块（C++20 + LLVM-22 + ASan）
- **JSON**: nlohmann/json

---

## Pre-Conditions (Day 0 — 验证)

> ⚠️ **必须在 Day 1 之前完成**。任一条件不满足则 Sprint 失败。

- [ ] **PC-1** Ubuntu 22.04+ 或等效 Linux 发行版
  ```bash
  cat /etc/lsb-release  # DISTRIB_DESCRIPTION 含 "22.04" 或更新
  ```

- [ ] **PC-2** C++17 编译器 (g++ 11+ / clang++ 14+)
  ```bash
  g++ --version  # g++ 11.x.x 或更新
  ```

- [ ] **PC-3** 项目能 build 出基线测试
  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests 2>&1 | tail -5
  # 期望: "All tests passed (...)"
  ```

- [ ] **PC-4** 记录当前 baseline 测试数
  ```bash
  ./build/bin/cpptlm_tests 2>&1 | grep -E "All tests passed|test cases" | tail -3
  # 期望: $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) cases / 14714 assertions (2026-06-06 基准)
  ```

- [ ] **PC-5** CppHDL 子模块存在（若 D4 选 B 才需要）
  ```bash
  readlink -f external/CppHDL  # 期望: /workspace/project/CppHDL
  ls external/CppHDL/include/ch.hpp  # 期望: 文件存在
  ```

- [ ] **PC-6** 没有未提交的本地修改
  ```bash
  git status --porcelain  # 期望: 空输出
  ```

**若 PC-3 / PC-4 失败** → 先恢复基线（git stash + git reset + 重新 build），不进入 Day 1。

---

## Day 1 (P0-#1 + 新风险#4 收尾)

### Task 1.1 — P0-#1: 修复 PortRole/BundleType 字符串大小写 ⏱️ 30 min

**文件**: `include/core/port_types.hh:23-44`

**问题**: ADR-X.9 §决策 1 行 70-83 声明小写，代码用大写。JSON 配置按 ADR 写小写会反序列化失败。

**操作**:
```bash
# 1. 备份原行（行号可能因 commit 略有偏移）
sed -n '23,44p' include/core/port_types.hh
```

- [x] **T1.1.1** 修改 `port_types.hh:23-29` PortRole 映射（小写） ✅ **2026-06-09 审计: 已完成**
  ```diff
  -NLOHMANN_JSON_SERIALIZE_ENUM(PortRole, {
  -    {PortRole::INITIATOR, "INITIATOR"},
  -    {PortRole::TARGET, "TARGET"},
  -    {PortRole::BI_DIRECTIONAL, "BI_DIRECTIONAL"},
  -    {PortRole::NETWORK, "NETWORK"},
  -    {PortRole::PE, "PE"},
  -})
  +NLOHMANN_JSON_SERIALIZE_ENUM(PortRole, {
  +    {PortRole::INITIATOR, "initiator"},
  +    {PortRole::TARGET, "target"},
  +    {PortRole::BI_DIRECTIONAL, "bi_directional"},
  +    {PortRole::NETWORK, "network"},
  +    {PortRole::PE, "pe"},
  +})
  ```
  > **审计证据**: `grep -E '\{PortRole::' include/core/port_types.hh` 输出 `{INITIATOR, "initiator"}` 等 5 条,全部小写。完成时间 commit 追溯待补 (可能由 `architecture-debt-cleanup` boulder 或其他 commit 顺带完成)。

- [x] **T1.1.2** 修改 `port_types.hh:39-44` BundleType 映射（小写） ✅ **2026-06-09 审计: 已完成**
  ```diff
  -NLOHMANN_JSON_SERIALIZE_ENUM(BundleType, {
  -    {BundleType::CACHE_REQ, "CACHE_REQ"},
  -    {BundleType::CACHE_RESP, "CACHE_RESP"},
  -    {BundleType::NOC_FLIT, "NOC_FLIT"},
  -    {BundleType::GENERIC, "GENERIC"},
  -})
  +NLOHMANN_JSON_SERIALIZE_ENUM(BundleType, {
  +    {BundleType::CACHE_REQ, "cache_req"},
  +    {BundleType::CACHE_RESP, "cache_resp"},
  +    {BundleType::NOC_FLIT, "noc_flit"},
  +    {BundleType::GENERIC, "generic"},
  +})
  ```
  > **审计证据**: `grep -E '\{BundleType::' include/core/port_types.hh` 输出 4 条,全部小写。

- [ ] **T1.1.3** 同步检查 `port_types.hh:53-57` PortGroupBundleType 映射
  ```bash
  sed -n '53,57p' include/core/port_types.hh
  # 期望已是大写（SINGLE/BUNDLE_MASTER/BUNDLE_SLAVE），按 ADR 是否需改小写？
  ```
  - 若 ADR-X.9 表格仅描述 PortRole/BundleType，**PortGroupBundleType 保持大写**（内部用，无 ADR 约束）
  - 在 `port_types.hh:53-57` 旁加注释：`// PortGroupBundleType 内部使用，无 ADR 字符串约束`

- [ ] **T1.1.4** 全项目搜索大写字符串硬编码（防御性）
  ```bash
  grep -rn '"INITIATOR"\|"TARGET"\|"BI_DIRECTIONAL"\|"NETWORK"\|"PE"' \
      --include="*.cc" --include="*.hh" --include="*.json" src/ include/ test/ configs/ 2>/dev/null
  # 期望: 仅 port_types.hh 自身（已改）+ ADR/文档注释（不算）
  ```
  - 若发现 `configs/*.json` 写了大写 → 同步改为小写
  - 若发现 `*.cc` 硬编码比较 → 改为读 enum

**验证**:
- [ ] **T1.1.V1** 重新编译
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -10
  # 期望: 无错误
  ```
- [ ] **T1.1.V2** 跑端口类型 + Phase 3 测试
  ```bash
  ./build/bin/cpptlm_tests "[phase3]" "[port_types]" 2>&1 | tail -5
  # 期望: All tests passed
  ```
- [ ] **T1.1.V3** 跑全量回归
  ```bash
  ./build/bin/cpptlm_tests 2>&1 | tail -3
  # 期望: 总数 ≥ $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) baseline
  ```

**回滚**: `git checkout include/core/port_types.hh`

---

### Task 1.2 — 新风险#4 收尾：port_types.hh 全文审计 ⏱️ 30 min

- [ ] **T1.2.1** 读 `port_types.hh:79-93` `to_json`/`from_json` 函数
  ```bash
  sed -n '79,150p' include/core/port_types.hh
  ```
  - 确认 `to_json` 与 `NLOHMANN_JSON_SERIALIZE_ENUM` **双向一致**
  - 确认 `from_json` 处理缺失字段（用 `.value()` + 默认值）
  - 确认 `ModulePortSpec::from_json` 行 142-148 对可选字段（`module_name`/`port_groups`/`aliases`）正确处理

- [ ] **T1.2.2** 检查 `module_factory.cc:42` `get_default_port_specs()` 函数
  ```bash
  grep -n "get_default_port_specs\|PortRole::\|BundleType::" src/core/module_factory.cc | head -20
  ```
  - 确认默认 specs 与 `port_types.hh` 字符串一致（**T1.1 后**应全部小写）
  - 若发现硬编码大写字符串 → 同步改为 enum 值（`PortRole::INITIATOR` 而非 `"INITIATOR"`）

- [ ] **T1.2.3** 检查 `port_compatibility.cc` 字符串
  ```bash
  grep -n '"initiator"\|"target"\|"INITIATOR"\|"TARGET"' src/core/port_compatibility.cc
  ```
  - 期望 T1.1.4 之后无大写命中
  - 若有，列为 T1.1.4 的扩展

**验证**: 在 plan.md 末尾的"审计报告"章节记录 `port_types.hh` + `module_factory.cc` + `port_compatibility.cc` 三处审计结果（OK / Issue 列表）

---

### Day 1 结束产物

- [ ] **D1.DONE** 全部 T1.1.* + T1.2.* check 完成
- [ ] **D1.VERIFY** `./build/bin/cpptlm_tests` 全绿

---

## Day 2 (测试基础设施改写：删直接驱动 + 加 E2E)

### Task 2.1 — 删除 test_phase6_integration.cc 直接驱动（新风险#1）⏱️ 30 min

**文件**: `test/test_phase6_integration.cc:142-164`

**问题**: 该 TEST_CASE 直接操作 `xbar.req_in[0]`，**绕过** StreamAdapter/MasterPort/SlavePort/PortPair，**永远漏过** P0-#3 检测。

- [ ] **T2.1.1** 读 `test_phase6_integration.cc` 完整内容（行 1-250）
  ```bash
  sed -n '1,250p' test/test_phase6_integration.cc
  ```
  - 定位行 142-164 的 TEST_CASE（名称含 "CrossbarTLM tick routes request" 或类似）
  - 记录其前后 TEST_CASE 边界

- [ ] **T2.1.2** 删除该 TEST_CASE（含前后空行）
  ```bash
  # 记录起始/结束行号
  START=142
  END=164
  sed -i "${START},${END}d" test/test_phase6_integration.cc
  ```
  - 若实际行号偏移，用 grep 定位：
    ```bash
    grep -n "CrossbarTLM tick\|xbar.req_in\[0\]" test/test_phase6_integration.cc
    ```

- [ ] **T2.1.3** 确认编译过
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -5
  # 期望: 无错误（删除后应干净）
  ```

**验证**: 测试文件可编译；被删测试名在 grep 中无残留
```bash
grep -n "xbar.req_in\[0\]\.consume\|memcpy.*xbar.req_in" test/test_phase6_integration.cc
# 期望: 无输出
```

**回滚**: `git checkout test/test_phase6_integration.cc`

---

### Task 2.2 — 新增真 E2E 测试 ⏱️ 1.5 h

**文件**: `test/test_phase6_integration.cc` 末尾（接在最后一个 TEST_CASE 后）

**目标**: 新测试在 P0-#3 未修时**必失败**，修复后**必通过**

- [ ] **T2.2.1** 在 `test_phase6_integration.cc` 末尾追加新 TEST_CASE
  ```cpp
  TEST_CASE("Phase 6: E2E data flow cache→xbar→mem (P0-#3 regression)",
            "[phase6][e2e][dataflow][regression]") {
      registerChStreamModules();
      EventQueue eq;
      ModuleFactory factory(&eq);

      json config = R"({
          "modules": [
              {"name": "cache", "type": "CacheTLM"},
              {"name": "xbar",  "type": "CrossbarTLM"},
              {"name": "mem",   "type": "MemoryTLM"}
          ],
          "connections": [
              {"src": "cache",     "dst": "xbar.0", "latency": 1},
              {"src": "xbar.0",    "dst": "mem",    "latency": 2}
          ]
      })"_json;

      factory.instantiateAll(config);
      factory.startAllTicks();

      auto* cache = dynamic_cast<CacheTLM*>(factory.getInstance("cache"));
      auto* xbar  = dynamic_cast<CrossbarTLM*>(factory.getInstance("xbar"));
      auto* mem   = dynamic_cast<MemoryTLM*>(factory.getInstance("mem"));
      REQUIRE(cache != nullptr);
      REQUIRE(xbar  != nullptr);
      REQUIRE(mem   != nullptr);

      // 构造请求: tid=0xDEAD, addr=0x1234 → xbar 路由 dst=1 → xbar.0 → mem
      bundles::CacheReqBundle req;
      req.transaction_id.write(0xDEAD);
      req.address.write(0x1234);
      req.is_write.write(0);
      req.data.write(0xBEEF);
      req.size.write(8);

      // 注入包到 cache.req_in
      Packet* pkt = PacketPool::get().acquire();
      pkt->type = PKT_REQ;
      bundles::serialize_bundle(req, pkt->payload->get_data_ptr(),
                                pkt->payload->get_data_length());
      cache->req_in().process(pkt);

      // 推进 5 周期: cache 处理 + xbar 路由 + mem 响应 + 回传
      for (int i = 0; i < 5; i++) {
          eq.run(1);
      }

      // 关键断言: tid 0xDEAD 在 cache.resp_out 出现（透传贯穿 cache→xbar→mem）
      INFO("After 5 cycles, cache.resp_out.valid() should be true "
           "(proves StreamAdapter bind is correct for multi-port xbar)");
      REQUIRE(cache->resp_out().valid());
      auto resp = cache->resp_out().data();
      REQUIRE(resp.transaction_id.read() == 0xDEAD);

      PacketPool::get().release(pkt);
  }
  ```

- [ ] **T2.2.2** 确认 `serialize_bundle` 函数签名
  ```bash
  grep -n "serialize_bundle" include/bundles/bundle_serialization.hh | head -5
  # 期望签名: bool serialize_bundle(const T& bundle, uint8_t* dst, size_t dst_size)
  ```
  - 若签名不同，按实际签名调整 T2.2.1

- [ ] **T2.2.3** 编译 + 跑新测试（**预期 FAIL**，因为 P0-#3 未修）
  ```bash
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests "Phase 6: E2E data flow" 2>&1 | tail -30
  # 期望: REQUIRE(cache->resp_out().valid()) 失败
  ```

**验证**:
- [ ] **T2.2.V1** 测试编译通过
- [ ] **T2.2.V2** 测试 FAIL（`resp_out.valid() == false`），证明测试**真的能 catch P0-#3**
- [ ] **T2.2.V3** 其他 phase6 测试无回归

**回滚**: `git checkout test/test_phase6_integration.cc`

---

### Day 2 结束产物

- [ ] **D2.DONE** T2.1.* + T2.2.* 全完成
- [ ] **D2.VERIFY** 新 E2E 测试编译过 + **预期 FAIL**（P0-#3 未修）

---

## Day 3 (P0-#3 修复 + P0-#4 顺手做)

### Task 3.1 — P0-#3 修复 #1: CrossbarTLM 单指针化 ⏱️ 1 h

**文件**: `include/tlm/crossbar_tlm.hh`

- [ ] **T3.1.1** 读 `crossbar_tlm.hh:34, 62-67, 108-110, 126` 当前实现
  ```bash
  sed -n '34p;62,67p;108,110p;126p' include/tlm/crossbar_tlm.hh
  ```

- [ ] **T3.1.2** 检查 `get_adapter(unsigned idx)` 是否被外部调用
  ```bash
  grep -rn "get_adapter" --include="*.cc" --include="*.hh" include/ src/ test/ | grep -v crossbar_tlm.hh
  # 期望: 无外部使用（若有 → 保留向后兼容重载）
  ```

- [ ] **T3.1.3** 简化 `CrossbarTLM::set_stream_adapter`
  ```diff
  -    cpptlm::StreamAdapterBase* adapter[NUM_PORTS] = {nullptr};
  +    cpptlm::StreamAdapterBase* adapter_ = nullptr;
       bool port_busy_[NUM_PORTS] = {false};
  ```

  ```diff
  -    void set_stream_adapter(cpptlm::StreamAdapterBase*) override {}
  -    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override {
  -        for (unsigned i = 0; i < NUM_PORTS; i++) {
  -            adapter[i] = adapters[i];
  -        }
  -    }
  +    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
  +        adapter_ = adapter;
  +    }
  ```

  ```diff
  -        for (unsigned i = 0; i < NUM_PORTS; i++) {
  -            if (adapter[i]) adapter[i]->tick();
  -        }
  +        if (adapter_) adapter_->tick();
  ```

  ```diff
  -    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const { return adapter[idx]; }
  +    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }
  ```

- [ ] **T3.1.4** 编译验证
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -10
  # 期望: 编译过（即使 link 错，syntax 应 OK）
  ```

**回滚**: `git checkout include/tlm/crossbar_tlm.hh`

---

### Task 3.2 — P0-#3 修复 #2: ArbiterTLM 清理双指针 ⏱️ 30 min

**文件**: `include/tlm/arbiter_tlm.hh`

- [ ] **T3.2.1** 读 `arbiter_tlm.hh:18, 81-84` 当前实现
  ```bash
  sed -n '18p;81,84p' include/tlm/arbiter_tlm.hh
  ```

- [ ] **T3.2.2** 检查 `adapters_[N]` 是否被使用
  ```bash
  grep -n "adapters_\[" include/tlm/arbiter_tlm.hh
  ```

- [ ] **T3.2.3** 清理冗余（仅保留 `single_adapter_`）
  ```diff
  -    cpptlm::StreamAdapterBase* adapters_[N_PORTS] = {nullptr};
       cpptlm::StreamAdapterBase* single_adapter_ = nullptr;
  ```
  ```diff
  -    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
  -        single_adapter_ = adapter;
  -    }
  -    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override {
  -        for (unsigned i = 0; i < N_PORTS; i++) {
  -            adapters_[i] = adapters[i];
  -        }
  -    }
  +    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
  +        single_adapter_ = adapter;
  +    }
  ```
  ```diff
  -        for (unsigned i = 0; i < N_PORTS; i++) {
  -            if (adapters_[i]) adapters_[i]->tick();
  -        }
           if (single_adapter_) single_adapter_->tick();
  ```

**回滚**: `git checkout include/tlm/arbiter_tlm.hh`

---

### Task 3.3 — P0-#3 修复 #3: module_factory.cc 多端口 dispatch ⏱️ 1.5 h

**文件**: `src/core/module_factory.cc:548-562`

- [ ] **T3.3.1** 读当前 `else if (is_multi)` 分支
  ```bash
  sed -n '548,567p' src/core/module_factory.cc
  ```

- [ ] **T3.3.2** 替换为完整 dispatch
  ```diff
   } else if (is_multi) {
       if (type == "RouterTLM") {
           auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<tlm::RouterTLM,
               bundles::NoCFlitBundle, tlm::RouterTLM::NUM_PORTS>*>(adapter);
           for (unsigned i = 0; i < n_ports; i++) {
               bi_adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i], resp_out_vec[i], req_in_vec[i]);
           }
  +    } else if (type == "CrossbarTLM") {
  +        using XbarAdapterT = cpptlm::MultiPortStreamAdapter<CrossbarTLM,
  +            bundles::CacheReqBundle, bundles::CacheRespBundle, 4>;
  +        if (auto* xa = dynamic_cast<XbarAdapterT*>(adapter)) {
  +            for (unsigned i = 0; i < 4; i++) {
  +                xa->bind_port_pair(i, req_out_vec[i], resp_in_vec[i], resp_out_vec[i], req_in_vec[i]);
  +            }
  +        } else {
  +            DPRINTF(MODULE, "[ERROR] CrossbarTLM adapter type mismatch\n");
  +        }
  +    } else if (type == "ArbiterTLM2") {
  +        using Arb2AdapterT = cpptlm::MultiPortStreamAdapter<ArbiterTLM<2>,
  +            bundles::CacheReqBundle, bundles::CacheRespBundle, 2>;
  +        if (auto* aa = dynamic_cast<Arb2AdapterT*>(adapter)) {
  +            for (unsigned i = 0; i < 2; i++) {
  +                aa->bind_port_pair(i, req_out_vec[i], resp_in_vec[i], resp_out_vec[i], req_in_vec[i]);
  +            }
  +        }
  +    } else if (type == "ArbiterTLM4") {
  +        using Arb4AdapterT = cpptlm::MultiPortStreamAdapter<ArbiterTLM<4>,
  +            bundles::CacheReqBundle, bundles::CacheRespBundle, 4>;
  +        if (auto* aa = dynamic_cast<Arb4AdapterT*>(adapter)) {
  +            for (unsigned i = 0; i < 4; i++) {
  +                aa->bind_port_pair(i, req_out_vec[i], resp_in_vec[i], resp_out_vec[i], req_in_vec[i]);
  +            }
  +        }
       } else {
  -        DPRINTF(MODULE, "[WARN] Multi-port module '%s' uses set_stream_adapter(array) "
  -                "instead of bind_port_pair(). If this module uses BidirectionalPortAdapter, "
  -                "the binding may be incorrect. Please report this issue.\n",
  -                type.c_str());
  +        DPRINTF(MODULE, "[WARN] Multi-port module '%s' has no dispatch handler, "
  +                "ports may be unbound\n", type.c_str());
       }
       ch_mod->set_stream_adapter(adapter);
  ```

- [ ] **T3.3.3** 编译
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -10
  ```
  - 期望: 无错误

**回滚**: `git checkout src/core/module_factory.cc`

---

### Task 3.4 — P0-#4 修复: Packet::reset() 显式 release Extension ⏱️ 30 min

**文件**: `include/core/packet.hh:226-244`

- [ ] **T3.4.1** 读 `reset()` 函数
  ```bash
  sed -n '226,244p' include/core/packet.hh
  ```

- [ ] **T3.4.2** 在 `reset()` 末尾追加 release
  ```diff
   void reset() {
       if (payload && !isCredit()) {
           payload->reset();
       }
  +    // ADR-X.6 §5.1 缓解: reset() 显式 release 已知 extension,
  +    // 防止 PacketPool reuse 时 stale extension 残留
  +    if (payload) {
  +        payload->template release_extension<TransactionContextExt>();
  +        payload->template release_extension<ErrorContextExt>();
  +    }
       stream_id = 0;
       ...
   }
  ```

- [ ] **T3.4.3** 编译验证
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -5
  ```

**回滚**: `git checkout include/core/packet.hh`

---

### Task 3.5 — Day 3 全量验证 ⏱️ 30 min

- [ ] **T3.5.1** 跑新 E2E 测试（**预期现在 PASS**）
  ```bash
  ./build/bin/cpptlm_tests "Phase 6: E2E data flow" 2>&1 | tail -10
  # 期望: All tests passed
  ```

- [ ] **T3.5.2** 跑 chstream + phase6 + crossbar + arbiter 子集
  ```bash
  ./build/bin/cpptlm_tests "[chstream]" 2>&1 | tail -3
  ./build/bin/cpptlm_tests "[phase6]" 2>&1 | tail -3
  ./build/bin/cpptlm_tests "[crossbar]" 2>&1 | tail -3
  ./build/bin/cpptlm_tests "[arbiter]" 2>&1 | tail -3
  # 期望: 全绿
  ```

- [ ] **T3.5.3** 全量回归
  ```bash
  ./build/bin/cpptlm_tests 2>&1 | tail -3
  # 期望: 总数 ≥ $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) + 1 (baseline + 1 新 E2E)
  ```

### Day 3 结束产物

- [ ] **D3.DONE** T3.1.* + T3.2.* + T3.3.* + T3.4.* 全完成
- [ ] **D3.VERIFY** 新 E2E 测试 PASS, 全量 ≥ $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) + 1 测试通过

---

## Day 4 (P0-#2 决策点：Smoke Test → A 或 C)

> ⚠️ **本 Day 决定 RTL Spike 命运**。若 smoke test 崩，回退到 v3 拆分（选项 A）；若通过，保留 ch_stream + 更新文档（选项 C）。

### Task 4.1 — 准备 smoke test 文件 ⏱️ 30 min

- [ ] **T4.1.1** 创建 `test/rtl/smoke_chstream_rtl.cc`
  ```bash
  mkdir -p test/rtl
  touch test/rtl/smoke_chstream_rtl.cc
  ```

- [ ] **T4.1.2** 写最小 smoke test
  ```cpp
  // test/rtl/smoke_chstream_rtl.cc
  // P0-#2 决策依据: 验证 ch_stream<Bundle> 是否能作为 __io 端口
  // 与 chppHDL_api_verification.md §2.4 实证结论对照
  // 作者 Sisyphus / 日期 2026-06-07
  #include <catch2/catch_all.hpp>
  #include "ch.hpp"
  #include "component.h"
  #include "device.h"
  #include "simulator.h"
  #include "rtl/hybrid_cache_component.hh"

  TEST_CASE("CppHDL: ch_stream<bundles::CacheReqBundleRTL> as __io port",
            "[cpphdl][ch_stream_io][p0_2_smoke]") {
      using namespace ch;

      ch_device<cpptlm::rtl::HybridCacheComponent> dev;
      Simulator sim(dev.context(), false);

      // 1 周期推进不应 SEGV（chppHDL_api_verification §2.5 实证 SEGV 路径）
      REQUIRE_NOTHROW(sim.tick());

      SUCCEED("ch_stream<Bundle> 作 __io 端口 1 tick 不崩溃");
  }
  ```

- [ ] **T4.1.3** 确认 `test/rtl/CMakeLists.txt` 存在
  ```bash
  ls test/rtl/CMakeLists.txt
  ```
  - 若不存在，参考根 `test/CMakeLists.txt` 创建
  - 若 `cpptlm_tests` 已用 GLOB 自动发现 `test/rtl/*.cc`，无需额外配置

- [ ] **T4.1.4** 编译 + 跑 smoke test
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -10
  ./build/bin/cpptlm_tests "ch_stream.*__io" 2>&1 | tail -30
  ```
  - 记录输出到 `plans/p0-alignment-remediation-plan.md` 的"smoke test 结果记录"章节

### Task 4.2 — 根据结果分支决策 ⏱️ 0-8 h

- [ ] **T4.2.B1**（**若 smoke test PASS**）→ 走选项 C
  - **T4.2.B1.1** 更新 `docs/architecture/examples/hybrid/chppHDL_api_verification.md §2.4`
    - 标题改为："`ch_stream<T>` 作为 `__io` 端口（待 §2.4 验证）"
    - 加 §2.6 "ch_stream<Bundle> 作 __io 端口的实证结论" 章节
  - **T4.2.B1.2** 在 `hybrid_tlm_cppHDL_design_v4.md §0.2` 表格加一行：
    ```
    | 10 | ch_stream<Bundle> 作 __io 端口 | 实证 PASS（D4 smoke test）| ✅ 已验证 |
    ```
  - **T4.2.B1.3** 更新 v4 §0.2 测试数据（"14/14" → 实际跑出数字）
  - **估时**: 1 h

- [ ] **T4.2.A1**（**若 smoke test FAIL / SEGV**）→ 走选项 A（回退 v3 拆分）
  - **T4.2.A1.1** 改 `include/rtl/hybrid_cache_component.hh:38-41` 到 v3 拆分范式
    ```cpp
    __io(
        ch_in<ch_uint<64>>  req_tid;
        ch_in<ch_uint<64>>  req_parent;
        ch_in<ch_uint<8>>   req_frag_id;
        ch_in<ch_uint<8>>   req_frag_total;
        ch_in<ch_uint<64>>  req_addr;
        ch_in<ch_uint<8>>   req_size;
        ch_in<ch_bool>      req_is_write;
        ch_in<ch_uint<64>>  req_data;
        ch_in<ch_bool>      req_valid;
        ch_out<ch_bool>     req_ready;
        ch_out<ch_uint<64>> resp_tid;
        ch_out<ch_uint<64>> resp_parent;
        ch_out<ch_uint<8>>  resp_frag_id;
        ch_out<ch_uint<8>>  resp_frag_total;
        ch_out<ch_uint<64>> resp_data;
        ch_out<ch_bool>     resp_is_hit;
        ch_out<ch_uint<8>>  resp_error_code;
        ch_out<ch_bool>     resp_first;
        ch_out<ch_bool>     resp_last;
        ch_out<ch_bool>     resp_valid;
        ch_in<ch_bool>      resp_ready;
    );
    ```
  - **T4.2.A1.2** 改 `src/rtl/hybrid_cache_component.cc:28, 33-55, 60` 全部用 ch_in/ch_out 而非 `io().req_in.payload.*`
  - **T4.2.A1.3** 改 `src/rtl/hybrid_cache_wrapper.cc:50-138` 把 `io().req_in.payload.*` 改回标量端口 `io().req_addr` 等
  - **T4.2.A1.4** `git revert ba994c7 7448447 5d696e1` （3 个 ch_stream 重构 commit 整体回退）
  - **T4.2.A1.5** 更新 `hybrid_tlm_cppHDL_design_v4.md §0.2`：标注 "9 → 10 项"（ch_stream 决策为 A：回退 v3）
  - **T4.2.A1.6** 更新 `chppHDL_api_verification.md §2.4` 强化结论："v4 验证后确认 ch_stream<Bundle> 不可作 __io 端口，团队切回 v3 拆分"
  - **估时**: 6-8 h（若 Wrapper 改动大）

- [ ] **T4.2.3** 无论 A 或 C，重新跑 smoke test
  ```bash
  ./build/bin/cpptlm_tests "ch_stream.*__io" 2>&1 | tail -10
  # 期望: 0 SEGV, 1 PASS
  ```

**回滚**:
- 选项 C: `git checkout include/rtl/hybrid_cache_component.hh src/rtl/hybrid_cache_component.cc`
- 选项 A: `git revert <commit>`

### Day 4 结束产物

- [ ] **D4.DONE** T4.1.* + T4.2.* 全完成
- [ ] **D4.VERIFY** smoke test 1 PASS + 0 SEGV
- [ ] **D4.DECISION** 在 `plans/p0-alignment-remediation-plan.md` 记录 A 或 C 决策及理由

---

## Day 5 (全量回归 + 文档同步)

### Task 5.1 — 全量回归测试 ⏱️ 1 h

- [ ] **T5.1.1** Release 模式全量
  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests 2>&1 | tail -3
  # 期望: 总数 ≥ $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) + 1 (baseline + 1 新 E2E)
  ```

- [ ] **T5.1.2** Debug 模式全量（验证 ASan/零泄漏）
  ```bash
  cmake -S . -B build_debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
  cmake --build build_debug -j$(nproc)
  ./build/bin/cpptlm_tests 2>&1 | tail -3
  # 期望: All tests passed (no leak)
  ```

- [ ] **T5.1.3** 各 phase 子集回归
  ```bash
  ./build/bin/cpptlm_tests "[phase0]" 2>&1 | tail -1
  ./build/bin/cpptlm_tests "[phase1]" 2>&1 | tail -1
  ./build/bin/cpptlm_tests "[phase2]" 2>&1 | tail -1
  ./build/bin/cpptlm_tests "[phase3]" 2>&1 | tail -1
  ./build/bin/cpptlm_tests "[phase4]" 2>&1 | tail -1
  ./build/bin/cpptlm_tests "[phase5]" 2>&1 | tail -1
  ./build/bin/cpptlm_tests "[phase6]" 2>&1 | tail -1
  # 期望: 全绿
  ```

### Task 5.2 — 文档同步 ⏱️ 1 h

- [ ] **T5.2.1** 更新 `docs/adr/ADR-X.9-port-type-system.md`
  - 在文件头加变更记录：
    ```
    | 4.0 | 2026-06-07 | 实施对齐修复：port_types.hh 字符串统一小写（与决策 1 一致）|
    ```
  - 在 §决策 1 行 70-83 后加注释："**实施注**: 实际代码 `port_types.hh` 现已对齐为小写字符串（2026-06-07 修复 P0-#1）"

- [ ] **T5.2.2** 更新 `docs/adr/ADR-X.6-transaction-integration.md`
  - §5.1 表格中 PacketPool Extension 清理 行从 "⚠️" 改为 "✅"
  - 加引用："实施修复见 `include/core/packet.hh:reset()`（2026-06-07 P0-#4）"

- [ ] **T5.2.3** 更新 `docs/adr/ADR-X.13-stub-multi-extension.md`（若存在）
  - 记录"显式 release+set 模式"已在 FragmentMapper v4 中应用
  - 文件路径：`docs/adr/ADR-X.13-stub-multi-extension.md`（chppHDL v4 §0.2 引用）

- [ ] **T5.2.4** 更新 `docs/architecture/01-hybrid-architecture-v2.1.md §4.5`
  - "Transaction/Debug 架构（完整对齐）" → 4 处 ✅ 标记为 "✅ 2026-06-07 验证对齐"

### Task 5.3 — git commit 准备 ⏱️ 30 min

- [ ] **T5.3.1** 检查 git 状态
  ```bash
  git status
  # 期望: 列出本次所有修改
  ```

- [ ] **T5.3.2** 决定 commit 粒度（推荐 atomic 6 commits）
  ```bash
  # Commit 1: P0-#1 字符串修复
  git add include/core/port_types.hh
  git commit -m "fix(core): align PortRole/BundleType JSON strings to ADR-X.9 lowercase (P0-#1)"
  
  # Commit 2: P0-#3 Step 7 多端口 dispatch
  git add include/tlm/crossbar_tlm.hh include/tlm/arbiter_tlm.hh src/core/module_factory.cc
  git commit -m "fix(factory): bind CrossbarTLM/ArbiterTLM multi-port adapters (P0-#3)"
  
  # Commit 3: P0-#4 reset 清理
  git add include/core/packet.hh
  git commit -m "fix(packet): explicit extension release in reset() (P0-#4)"
  
  # Commit 4: 测试改写 + 新 E2E
  git add test/test_phase6_integration.cc
  git commit -m "test(phase6): add E2E data flow test, remove bypass fixture"
  
  # Commit 5: P0-#2 决策（smoke test + A/C 分支）
  git add test/rtl/smoke_chstream_rtl.cc include/rtl/hybrid_cache_component.hh src/rtl/hybrid_cache_component.cc src/rtl/hybrid_cache_wrapper.cc 2>/dev/null || true
  git commit -m "fix(rtl): [A or C] ch_stream<Bundle> as __io port decision (P0-#2)"
  
  # Commit 6: 文档同步
  git add docs/adr/ADR-X.6-transaction-integration.md docs/adr/ADR-X.9-port-type-system.md docs/architecture/01-hybrid-architecture-v2.1.md
  git commit -m "docs(adr): sync P0-#1/#3/#4 alignment status"
  ```

- [ ] **T5.3.3** 最终全量回归
  ```bash
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests 2>&1 | tail -3
  # 期望: 全绿（commit 不应引入新问题）
  ```

### Day 5 结束产物

- [ ] **D5.DONE** T5.1.* + T5.2.* + T5.3.* 全完成
- [ ] **D5.VERIFY** Release + Debug 双模式全绿，6 个 atomic commit 已提交到本地

---

## Day 6 (RTL Spike 冻结 + 文档归档)

### Task 6.1 — 创建 Spike 决策记录 ⏱️ 30 min

- [ ] **T6.1.1** 在 `docs/architecture/examples/hybrid/` 创建 `CHANGELOG.md`
  ```bash
  touch docs/architecture/examples/hybrid/CHANGELOG.md
  ```

- [ ] **T6.1.2** 写入决策记录
  ```markdown
  # Hybrid TLM+CppHDL 设计决策历史

  | 版本 | 日期 | 状态 | 关键决策 |
  |------|------|------|----------|
  | v1 | 2026-06-05 | 🗄️ 已废止 | Oracle 发现 10 个 CRITICAL API 误用 |
  | v2 | 2026-06-05 | 🗄️ 已废止 | Oracle 发现 1C+2H+3M+1L = 7 项 |
  | v3 | 2026-06-06 | 🗄️ 已废止（已归档）| 17 项修正完成，但未跑 smoke test |
  | v4 | 2026-06-06 | 📋 现行 | 12 项 v3→v4 修正 + 复用 chppHDL 现有设施 |
  | **D4 smoke test** | **2026-06-07** | **✅ PASS / ❌ FAIL** | **ch_stream<Bundle> 决策：A 回退 / C 保留** |
  ```

- [ ] **T6.1.3** 在 `hybrid_tlm_cppHDL_design_v4.md` 头部加 STATUS 横幅
  ```markdown
  > ⚠️ **STATUS (2026-06-07)**: D4 smoke test [PASS/FAIL] → 决策 [A/C] 已落地
  > 详见 `docs/architecture/examples/hybrid/CHANGELOG.md`
  ```

### Task 6.2 — 归档 v3 文档 ⏱️ 15 min

- [ ] **T6.2.1** 确认 `docs-archived/` 目录存在
  ```bash
  ls docs-archived/  # 期望: 已有目录
  ```

- [x] **T6.2.2** 移动 v3 ✅ **2026-06-09 完成: 归档至 `docs-archived/hybrid-iterations/v3-*.md`** (与 v1/v2 命名一致)
  ```bash
  # 实际执行 (2026-06-09):
  git mv docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v3.md \
         docs-archived/hybrid-iterations/v3-hybrid_tlm_cppHDL_design.md
  ```
  > **决策变更说明**: 原计划指定 `docs-archived/hybrid-design-history/`,但项目已存在 `docs-archived/hybrid-iterations/` 目录存放 v1/v2。本次归档沿用 v1/v2 命名约定 (`v{N}-*.md`),保持一致性。归档目标已记录在本文件"2026-06-09 状态审计"章节。

- [ ] **T6.2.3** 提交移动
  ```bash
  git add -A
  git commit -m "docs(hybrid): archive v3 design to docs-archived/"
  ```

- [ ] **T6.2.4** 在 `hybrid_tlm_cppHDL_design_v4.md` §0.1 演进史表加一行
  ```markdown
  | v3 | 2026-06-06 | 🗄️ 已归档（2026-06-07）| 全部 17 项修正，但未跑 smoke test |
  ```

### Task 6.3 — 关闭 RTL Spike ⏱️ 15 min

- [ ] **T6.3.1** 在 `hybrid_tlm_cppHDL_design_v4.md` 头部加 "SPIKE CLOSED" 横幅
  ```markdown
  > 🔒 **SPIKE CLOSED (2026-06-07)**: 决策落地后，HybridCacheWrapper + FragmentMapper
  > 进入生产代码；Day 2+ 路线图（多拍 / compare / shadow）转入正式项目路线图。
  > **不允许**新增 HybridCacheComponent 重构 commit（除非重新跑 smoke test）。
  ```

- [ ] **T6.3.2** 提交
  ```bash
  git add docs/architecture/examples/hybrid/
  git commit -m "docs(hybrid): close RTL Spike (D4 decision + v3 archive)"
  ```

### Day 6 结束产物

- [ ] **D6.DONE** T6.1.* + T6.2.* + T6.3.* 全完成
- [ ] **D6.VERIFY** v3 已归档 + CHANGELOG 存在 + SPIKE CLOSED 横幅到位

---

## Day 7 (缓冲日：回归 + PR 准备)

### Task 7.1 — 完整回归 ⏱️ 1 h

- [ ] **T7.1.1** Release 模式全量
  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests 2>&1 | grep -E "test cases|assertions" | tail -1
  # 期望: ≥ $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) + 1 cases
  ```

- [ ] **T7.1.2** Debug + ASan 模式
  ```bash
  cmake -S . -B build_asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
  cmake --build build_asan -j$(nproc)
  ./build/bin/cpptlm_tests 2>&1 | tail -3
  # 期望: All tests passed (no leak)
  ```

- [ ] **T7.1.3** 格式检查
  ```bash
  ./scripts/format.sh --check
  # 期望: 无格式错误
  ```

### Task 7.2 — PR 准备 ⏱️ 1 h

- [ ] **T7.2.1** 推送分支
  ```bash
  git checkout -b fix/p0-alignment-remediation
  git push -u origin HEAD
  ```

- [ ] **T7.2.2** 创建 PR（用 gh CLI）
  ```bash
  gh pr create --title "fix: P0 alignment remediation (4 P0s + 2 new risks)" \
               --body "## Summary
  - P0-#1: PortRole/BundleType JSON strings aligned to ADR-X.9 lowercase
  - P0-#3: CrossbarTLM/ArbiterTLM multi-port adapters properly bound
  - P0-#4: Packet::reset() explicit extension release (X.6 §5.1 mitigation)
  - P0-#2: ch_stream<Bundle> __io decision (A or C per D4 smoke test)
  - New E2E test: 'Phase 6: E2E data flow cache→xbar→mem'
  - Archive v3 hybrid design to docs-archived

  ## Test Results
  - $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) + 1 cases (baseline + 1 new E2E)
  - All Phase 0-6 subtests green
  - ASan clean

  ## Docs
  - ADR-X.6, ADR-X.9, ADR-X.13 status synchronized
  - chppHDL_api_verification.md updated per D4 decision
  - v3 hybrid design archived"
  ```

- [ ] **T7.2.3** 等待 CI 通过（手动监控或 `gh pr checks --watch`）

- [ ] **T7.2.4** 合并（squash 或 merge commit，看项目约定）
  ```bash
  gh pr merge --squash  # 或 --merge
  ```

### Task 7.3 — Sprint 闭环报告 ⏱️ 30 min

- [ ] **T7.3.1** 在本 plan 文件末尾追加"完成报告"章节
  - 测试数变化
  - 各 P0 状态
  - 实际工期
  - 遇到的意外
  - D4 决策（A 或 C）+ 理由

### Day 7 结束产物

- [ ] **D7.DONE** 全部 Sprint 任务完成
- [ ] **D7.VERIFY** CI 通过 + PR merged + 闭环报告写入

---

## Audit Report (Day 1 输出)

> **填写人**: _____________ **日期**: _____________

| 文件 | 范围 | 状态 | Issue 列表 |
|---|---|:---:|---|
| `port_types.hh:1-150` | 全部 | ☐ OK ☐ Issue | |
| `module_factory.cc:42` `get_default_port_specs` | 函数 | ☐ OK ☐ Issue | |
| `port_compatibility.cc:1-N` | 全部 | ☐ OK ☐ Issue | |

**审计结论**:
```
[在此粘贴你的发现]
```

---

## Smoke Test 结果记录 (Day 4 输出)

> **D4 smoke test 输出**:
```
[在此粘贴测试输出 — PASS 或 SEGV 堆栈]
```

> **D4 决策**:
- [ ] **A**: 回退 v3 ch_in/ch_out 拆分
- [ ] **C**: 保留 ch_stream<Bundle> + 更新 chppHDL_api_verification.md §2.4

> **决策理由**:
```
[在此粘贴 3 句话理由]
```

---

## Risk & Rollback Strategy

### 风险登记表

| 风险 | 概率 | 缓解 |
|---|---|---|
| Day 1 P0-#1 破坏现有 JSON config | 🟡 中 | T1.1.4 全项目搜索；测试 `[json]` 跑全 |
| Day 3 P0-#3 改 CrossbarTLM 头破坏外部 | 🟡 中 | T3.1.2 grep `get_adapter` 外部使用；若无则安全 |
| Day 3 P0-#3 dispatch 漏掉新多端口类型 | 🟡 中 | T3.3.2 else 分支保留 WARN |
| Day 4 P0-#2 选项 A 需大改 Wrapper | 🟡 中 | 选项 B 先验证再决定，避免赌博 |
| Day 5 commit 粒度选错 | 🟢 低 | 6 atomic commits 单独可 revert |
| Day 6 归档后引用断裂 | 🟢 低 | v3 文件 git 仍可查 |

### 回滚策略

| 阶段 | 回滚命令 |
|---|---|
| Day 1 后 | `git revert <last_commit>` 即可 |
| Day 3 后 | `git revert <p0-3-commit>` 保留 P0-#1/#4 |
| Day 4 后 A 选项失败 | `git revert <a-decision-commit>` 回退到 C 选项 |
| Day 5 PR 前 | `git reset --hard origin/main` 全清 |
| Day 6 后 | `git mv docs-archived/hybrid-design-history/hybrid_tlm_cppHDL_design_v3.md docs/architecture/examples/hybrid/` |

### Definition of Done

- [ ] 7 个 atomic commit 全部在 main 分支
- [ ] `./build/bin/cpptlm_tests` Release 模式 ≥ $(./build/bin/cpptlm_tests 2>&1 | grep -E "test cases" | tail -1) + 1 cases 全绿
- [ ] `./build/bin/cpptlm_tests` Debug + ASan 模式全绿
- [ ] 新 E2E 测试 "Phase 6: E2E data flow cache→xbar→mem" 在主分支 PASS
- [ ] v3 hybrid design 在 `docs-archived/hybrid-design-history/`
- [ ] CHANGELOG.md 记录 D4 决策（A 或 C）
- [ ] 至少 3 个 ADR 文档状态更新
- [ ] CI 通过
- [ ] Sprint 闭环报告写入本 plan 末尾

---

## Effort Estimate Summary

| Task | 估时 | 实际 |
|---|---:|---:|
| Day 0 Pre-conditions | 30 min | |
| Day 1 P0-#1 + 审计 | 1 h | |
| Day 2 测试改写 | 2 h | |
| Day 3 P0-#3 + P0-#4 | 4 h | |
| Day 4 P0-#2 决策 | 2-8 h | |
| Day 5 回归 + 文档 + commit | 2.5 h | |
| Day 6 归档 + 冻结 | 1 h | |
| Day 7 缓冲 + PR | 2.5 h | |
| **总计** | **15-20 h** | |

**对应 5-6 个工作日（按 4h/天有效执行）**。

---

## References

- 审查报告：本仓库 `plans/doc_alignment_findings.md`（前置）+ 本轮 Oracle 验证
- 关联 ADR：`docs/adr/ADR-X.{1,6,9,13}-*.md`
- 关联设计：`docs/architecture/01-hybrid-architecture-v2.1.md` + `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md`
- Oracle 验证：5 轮问答（Q1 验证 + Q2 依赖 + Q3 patch + Q4 E2E + Q5 Sprint）
- 现有计划参考：`plans/debt-remediation-plan.md`（已闭环模板）+ `.omo/plans/rtl-spike-implementation.md`（架构上下文）

---

**维护**: Sisyphus (AI Architect)
**最后更新**: 2026-06-07
**状态**: ⏳ 待开始 → 7 天 Sprint 闭环后改 ✅
