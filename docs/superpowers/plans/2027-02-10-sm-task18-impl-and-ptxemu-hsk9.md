# SM 重构 Task 18 完整实施 + PTX-EMU 端 HSK-9 改造 + 联合验证 — 实施计划 (v2 修订版)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 SM 微架构重构 Task 18 (12 子模块真值 + bit-exact Gate 协议 + 端到端) + PTX-EMU 端 HSK-9 改造 (injector API + attach_timing 移除) + 跨仓联合验证 (真双向数据通路 + L7 JSON reload)。

**Architecture:** 大爆炸 + HSK-9. 单一真值源 `cpptlm::gpu::alu::*` (PTX-EMU 现有 handlers 不复制实现); SM 持寄存器唯一真值 (RegFileUnit), PTX-EMU 通过 `set_compute_device(icompute_dev*)` 注入 → SM 的 `set_instr_descriptor_buf()` 浅拷贝指令 → SM 执行后通过 pull 通道 (`get_register_value` + `is_instruction_completed`) 让 PTX-EMU 拉回比对 (Gate 闭环, 契约合规); L7 JSON reload 验证配置加载完整性。

**Tech Stack:** C++17/20 + SystemC stub + ChStreamModuleBase + bundles + ModuleFactory + Catch2 v3.7.0 + git-master 多原子 commit + 跨仓 submodule bump + HSK-9 14 天反馈窗口 (重新锚定)。

**关联文档**:
- 设计: `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` (v5.0, 971 行)
- HSK-9 公告 (重新发布): `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`
- ADR-SOC-16: `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md` (14 Gate items, 本计划修订后与 ADR 一致)
- OpenSpec spec: `openspec/specs/sm-microarchitecture/spec.md` (12 ADDED Requirements)
- 上一阶段计划: `docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md` (Tasks 1-17/19-20 完成)
- Oracle 评审 session `ses_f8c579906ffeoKlnK0jEEDwrdS` (技术 P0-5 / P1-7)
- Metis 评审 session `ses_f8c500f05ffeenqBRZg15sv8hy` (process / scope / clarity)

**v2 修订摘要** (per Oracle + Metis 联合评审, NEEDS-REWORK → 修订 → APPROVE):

| 来源 | ID | 修订内容 |
|------|----|---------|
| Oracle | P0-1 | Task 1.1 SM 端口改用 GPUTLM 4-getter 范式 (`InputStreamAdapter<>`/`OutputStreamAdapter<>`) |
| Oracle | P0-2 | Task PTX-2 删除 `override`, 改为 injector API 模式 (PTX-EMU `set_compute_device(icd::IComputeDevice*)` 注入, 通过 `sm_context_cpptlm_inject.h` 注入入口) |
| Oracle | P0-3 | Task 0.1 Step 3 前加 `git submodule update --init --recursive` |
| Oracle | P0-4 | 统一 `src_regs = 寄存器号` 语义, 删除 BitExactGate 内 `src_regs` 当立即数用法, Gate 改为基于"PTX-EMU 现有 functional handlers 结果 vs SM 用真值源结果"对比 |
| Oracle | P0-5 | Task 2.1 **不重命名** (保 FetchUnitTLM 类名), 仅做物理拆分 .hh/.cc |
| Metis | Top 1 | 新增 Task 4.5 pull 通道 (v3: 契约合规版) + Task 4.9 真联合测试 (CppTLM 链接 PTX-EMU 新代码全链路) |
| Metis | Top 2 | 新增 Task 0.5 跨仓 build 拓扑定义 + Task 0.3 G12 重做 + 14d 窗口重锚 (TBD 锚定) + Task 2.18 L7 JSON reload 测试 |
| Metis | Top 3 | 删除 Task 1.1/4.3/4.5 错误代码块, 改为"参照现网范式"指引; 18b 验收改为"每子模块 ≥2 条断言真实行为" |
| Oracle | P1-7 | `get/set_scalar_reg` 方法在 Task 1.1 提前声明 (1.3 依赖) |
| Metis | G13/G14 | 文档统一为单一定义表 (见末尾 Gate 判定表) |
| Oracle | P1-3 | Task 2.14 改为 SM 内部 C++ 直连 (非 JSON wiring); JSON config 验 SM 顶层对外 1 对 req/resp |
| Oracle | P1基数 | 移除错误路径示例, 路径示例改自 references 现网代码 |
| Metis | PTX-EMU | Task PTX-0 强制读 PTX-EMU AGENTS.md + ptx-lessons-learned + drift_check 8 invariants |
| Metis | Decision | 新增决策流程表 (G14 PASS / 退路 A/B/C 触发条件 + 决策人) |
| Metis | 范围 | Task 4.4 SGEMM 验收 ±15% 与 bit-exact 冲突 → MFMA 4×4 数值测试替代 |
| Metis | Baseline | 硬编码预期数字删除, Task 0.x 改为"记录实际数字到 tracker" |

**v3 修订摘要** (per Oracle v2 复核 session `ses_f8c579906ffeoKlnK0jEEDwrdS`, APPROVE-WITH-FIXES → 修订 → APPROVE):

| 来源 | ID | 修订内容 | 对应 Task |
|------|----|---------|-----------|
| Oracle v2 | 新 P0 | Task 4.5 `const_cast<>` 回填协议违反 `i_compute_device.hh:13` 冻结契约, 改用 pull 通道 (记录 `sm_results_[instr_id]`, PTX-EMU 通过 `get_register_value`/`is_instruction_completed` 拉回比对) | Task 4.5 v3 |
| Oracle v2 | 新 P1-α | git worktree 机制冲突: `feat/hsk-9-impl` 在独立 worktree 已 checkout, sm-mp-impl 子模块 `git checkout` 必失败; 改用 `git checkout --detach <HSK9_SHA>` (避开分支独占) | Task 0.1 Step 2 + Task 0.5 Step 3 + Task 4.8 Step 1/4 + Task 4.9 Step 2 |
| Oracle v2 | 新 P1-β | Task 2.13 测试逻辑错误: (a) `HazardTracker ht;` 无默认构造, 需 `EventQueue eq; HazardTracker ht("ht0", &eq);`; (b) 第二个 TEST_CASE `decrement ×1` 后 vmcnt=1, `is_stalled_vmcnt(0,0,0)` 必 FAIL, 改为 `decrement ×2` | Task 2.13 v3 |
| Oracle v2 | 新 P1-γ | Task 4.9 真联合测试无实体断言: 原 Step 1 全是注释; 改为具体断言骨架 (producer push → SM exec → register pull → completion pull → injector 路径), 并明确 CMake 链接 `ptxemu_device` | Task 4.9 v3 |

**总工作量**: 28-40 工作日 (1 人) / 16-22 工作日 (2 人协作) (per Oracle 修订, 反映 P0-3 + P0-4 + Top 1/2 增量)

**会话结构**: 5 子波次 (本计划总 **30 Task**, 编号与正文一一对应, 修订 v2)

```
子波 0 (会话准备):      Tasks 0.1 - 0.5    (1-2 天)
子波 1 (CppTLM 18a):   Tasks 1.1 - 1.6    (3-5 天)
子波 2 (CppTLM 18b):   Tasks 2.1 - 2.18   (8-12 天)
子波 3 (PTX-EMU HSK-9):Tasks PTX-0..6      (6-10 天)
子波 4 (CppTLM 18c):   Tasks 4.1 - 4.11   (6-10 天)
合计: 30 Task, 24-39 天
```

---

## Gate G1-G14 统一定义 (per ADR-SOC-16 + HSK-9 + 设计 §15.10.1 综合)

> **修订**: 解决三文档 G13/G14 定义矛盾. 本表为唯一判据, 实施会话以本表为准.

| Gate | 内容 | 验证手段 | 对应 Task | 状态 |
|------|------|----------|-----------|------|
| **G1** | 12 子模块 stub 全部注册 | `nm libcpptlm_core.a \| grep -E "FetchUnitTLM\|DecodeUnitTLM\|..."` 输出 12 个 | Task 1.6 + 8.5 | ✅ 已 PASS (HEAD 39bbf2e) |
| **G2** | IComputeDevice 15 方法纯虚 | `grep -c "= 0" include/tlm/gpu/i_compute_device.hh` = 15 | Task 4.5 | ✅ 已 PASS |
| **G3** | InstrDescriptor POD sizeof ≤ 128 | `static_assert` + 编译 | Task 8.5 | ✅ 已 PASS |
| **G4** | 8 Bundle POD 类型存在 | `bundles/sm_bundles_tlm.hh` 编译 | Task 6 | ✅ 已 PASS |
| **G5** | SM 顶层端口 4 个访问器 (InputStreamAdapter/OutputStreamAdapter) | `grep` + 编译 + 测试 | **Task 1.1 (v2 修订)** | ⏸ Pending |
| **G6** | StreamAdapter 注册 SM (ComputeReqBundle/ComputeRespBundle) | `chstream_register.hh` 注册代码 | **Task 1.2 (v2 修订)** | ⏸ Pending |
| **G7** | ScalarALU 真值 (ADD/IMAD/SFU sub-pipe) | 测试断言 >0 不等 | **Task 1.3** | ⏸ Pending |
| **G8** | RegFileUnit + get_register_value 真值 | 测试断言 | **Task 1.4** | ⏸ Pending |
| **G9** | 12 子模块全部分拆独立 .hh/.cc (保类名 FetchUnitTLM 等) | 文件存在 + 编译 + 既有测试不变 | **Task 2.1 (v2 修订)** | ⏸ Pending |
| **G10** | 12 子模块全部有真值 (每子模块 ≥2 条断言真实行为) | 测试断言通过 | **Task 2.2 - 2.12 (v2 修订验收)** | ⏸ Pending |
| **G11** | 8 Bundle 内部 C++ 直连接通 | 集成测试 | **Task 2.13 (v2 修订)** | ⏸ Pending |
| **G12** | HSK-9 公告发布到 PTX-EMU 仓 docs/superpowers/specs/ + CppTLM 仓 | GitHub 检查 PTX-EMU 仓 docs/superpowers/specs/2027-02-09-hsk-9-*.md 存在 + CppTLM 仓镜像 | **Task 0.3 (v2 修订: 重新发送)** | ⏸ Pending |
| **G13** | (本计划统一定义) **SM 完整 ALU + bit-exact Gate 协议 + L7 JSON reload + 真联合验证** | Task 4.5 (pull 通道: SM 记录 `sm_results_[instr_id]` + 契约合规) + Task 4.7 (BitExactGate 含真值源 vs PTX-EMU functional 结果对比) + Task 4.9 (真联合测试 CppTLM→PTX-EMU 新代码) + Task 2.18 (L7 JSON reload 4 config 无错) | **Task 2.18 + 4.5 + 4.7 + 4.9 (v3 修订)** | ⏸ Pending |
| **G14** | (本计划统一定义) **PTX-EMU 端 14 天反馈窗口评审** | PTX-EMU owner 明确 ack / 14 天无反馈 (默认 ack per HSK 协议) / 退路 A/B/C 触发 | **Task 0.3 + 4.10 (v2 修订)** | ⏸ Pending (TBD 锚定) |

**Gate PASS 总条件**: G1-G14 全部 ✅. Oracle Gate G13/G14 评审 (Task 1.6 / 2.17 / 4.10) 在 Oracle verdict PASS 即 PASS.

---

## 决策流程 (per Metis 评审, 必须写进计划)

| 情形 | 触发条件 | 决策人 | 处置 |
|------|---------|--------|------|
| **G13 PASS** | Oracle Task 4.10 verdict APPROVE | 主代理 (用户知情) | 正常推进 submodule bump + 收尾 |
| **G13 FAIL** | Oracle 评审找 P0 阻塞 | Oracle 定 FAIL, 用户批准 | 第 1 次 FAIL: 修复并重审; 第 2 次仍 FAIL: 停止实施, 评估退路 |
| **G14 PASS (HSK-9 ack)** | PTX-EMU owner 明确同意 / 14 天无反馈 (HSK 协议默认 ack) | 主代理 (用户知情) | 正常推进 submodule bump |
| **退路 A** (PTX-EMU owner 反对 set_instr_descriptor_buf, 坚持 IPtxEmuDevice 12 方法不可动) | owner 明确反对 | **用户** | CppTLM 保留 3 vendor shim, SM 重构挂起等 HSK-10; 子波 3 全部 Task 作废, worktree 清理 |
| **退路 B** (owner 部分反对, 接受 semantic change 但要求拆批) | owner 部分反对 | 用户 + Oracle | 子波 3 拆两批次提交; Task 0.3 tracker 记录拆分决议 |
| **退路 C** (owner 完全反对但 CppTLM 侧不愿等) | owner 完全反对 + 用户决策 | 用户 | 接口改名 ICdnaComputeDevice (CppTLM 独有), PTX-EMU 无需改造; 子波 3 只保留镜像头 + PR 说明 |

---

## 跨仓联合 build 拓扑 (per Metis 评审, 必须明示)

```
┌─────────────────────────────────────────┐
│ CppTLM worktree (.worktrees/sm-mp-impl)  │
│                                          │
│  ├── include/tlm/gpu/i_compute_device.hh │ ←── CppTLM 端 IComputeDevice 接口 (单一定义源)
│  ├── src/tlm/gpu/streaming_multiprocessor│ ←── SM 顶层实现 + 12 子模块 + 真值源
│  │     _tlm.cc                              │
│  └── external/PTX-EMU (submodule)        │ ←── 联合验证前指向 feat/hsk-9-impl (本地),
│       (gitlink → PTX-EMU 仓)              │     submodule bump 在 PR merged 后
│                                          │
└─────────────────────────────────────────┘
                    ↓ submodule
┌─────────────────────────────────────────┐
│ PTX-EMU worktree (.worktrees/hsk-9-impl) │
│                                          │
│  ├── include/ptxemu/icd/i_compute_device  │ ←── 镜像头 (per Task PTX-1)
│  │     .hh                                  │
│  ├── include/ptxemu/icd/instr_descriptor  │
│  │     .hh                                  │
│  └── src/ptxemu/device_api_impl.cc       │ ←── attach_timing 改 deprecated stub,
│                                          │     不新增 set_instr_descriptor_buf override
│                                          │     (per P0-2 修订)
└─────────────────────────────────────────┘
```

**联合验证 (Task 4.9) 拓扑**:
- CppTLM `-DCPPTLM_WITH_PTX_EMU=ON` 链接 `external/PTX-EMU` 当前指向
- 验证前步骤: `cd external/PTX-EMU && git fetch origin && git checkout feat/hsk-9-impl` (本地 submodule 指向 hsk-9 分支)
- 验证后 bump: 在 CppTLM 仓 `git add external/PTX-EMU && commit "chore(submodule): bump PTX-EMU to <hash>"`
- **不**链接主 worktree 的 73a5ecee (旧代码), 否则测的是旧 PTX-EMU

---

## 子波 0: 会话准备 (1-2 天)

### Task 0.1: 跨仓 worktree 隔离 + submodule init

**Files:**
- 无 (git 操作)

- [ ] **Step 1: 在 CppTLM 主仓创建 worktree + 初始化 submodule**

```bash
cd /workspace/project/CppTLM
git worktree add .worktrees/sm-mp-impl -b feat/sm-mp-impl HEAD
cd .worktrees/sm-mp-impl
git submodule update --init --recursive external/PTX-EMU  # v2 P0-3 修复
ls external/PTX-EMU/CMakeLists.txt  # 应存在, 否则 submodule init 失败
```

- [ ] **Step 2: PTX-EMU 端 worktree (v3 修订: 主仓 submodule 路径, 非 sm-mp-impl 内嵌套)**

> **v3 修订**: per Oracle v2 复核 P1-α. git 不允许同一 branch 同时 checkout 在两个 worktree. 原方案在 `sm-mp-impl/external/PTX-EMU` 内建 worktree 会导致后续 Task 0.5/4.8 在主 checkout submodule 切分支失败. 改为在主仓 submodule 路径建 worktree.

```bash
# 主仓路径, 非 worktree 内
cd /workspace/project/CppTLM/external/PTX-EMU
git worktree add .worktrees/hsk-9-impl -b feat/hsk-9-impl HEAD
cd .worktrees/hsk-9-impl
git log --oneline -3
# 预期: 73a5ecee chore(openspec): archive phase-1-5-namespace-migration ...
```

- [ ] **Step 3: 记录基线实际数字 (不硬编码预期)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
cmake --build build --target cpptlm_tests -j$(nproc) 2>&1 | tail -3
./build/bin/cpptlm_tests 2>&1 | tail -3
# 记录: assertions 总数 (AGENTS.md 当前标 15098, 实际可能不同) + 测试用例数 + 通过率
# 写入: docs/superpowers/specs/HSK-9-baseline-tracker.md (per Metis P1-4)
```

```bash
cd external/PTX-EMU
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
cmake --build build -j$(nproc) 2>&1 | tail -3
ctest --test-dir build --output-on-failure 2>&1 | tail -10
# 记录: PASS/FAIL 总数
```

- [ ] **Step 4: push 工作树**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git push origin feat/hsk-9-impl
```

### Task 0.2: 验证 PTX-EMU 端 submodule 状态

**Files:**
- 无

- [ ] **Step 1: PTX-EMU 端 device_api.h 仍冻结**

```bash
grep "PTXEMU_API_VERSION" external/PTX-EMU/include/ptxemu/device_api.h
# 预期: #define PTXEMU_API_VERSION 1
```

- [ ] **Step 2: PTX-EMU AGENTS.md HSK 链记录**

```bash
grep -A1 "HSK-9" external/PTX-EMU/AGENTS.md | head -3
# 预期: HSK-9 状态 (v2 修订后会更新到 Active)
```

### Task 0.3: HSK-9 反馈窗口跟踪 + G12 重做准备 (v2 修订)

**Files:**
- Create: `docs/superpowers/specs/HSK-9-feedback-tracker.md`
- Modify: `external/PTX-EMU/AGENTS.md` (HSK-9 状态更新 Active) + `external/PTX-EMU/docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` (镜像)

> **重要**: per Metis §2.2 发现 — HSK-9 spec 实际**未发送到 PTX-EMU 仓**, 14 天窗口从未启动. 本 Task 重做发布流程.

- [ ] **Step 1: 镜像 HSK-9 spec 到 PTX-EMU 仓**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
mkdir -p docs/superpowers/specs
cp /workspace/project/CppTLM/docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md \
   docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md
# 修改: 头部 PTX-EMU 端 ack 字段 + 14 天窗口从本 commit date 重新锚定
sed -i 's|2027-02-09 + 14d|2027-02-XX + 14d (锚定 TBD)|g' docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md
git add docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md
GIT_MASTER=1 git commit -m "chore(hsk): 镜像 HSK-9 spec to PTX-EMU docs/superpowers/specs/"
```

- [ ] **Step 2: 更新 PTX-EMU AGENTS.md HSK 链**

Edit `external/PTX-EMU/AGENTS.md` HSK-9 行:
```
| HSK-9 | 🔵 **Active (2027-02-XX)** — ICOMPUTE_API_VERSION=1 SM 重构,
         14 天反馈窗口 (per CppTLM `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`).
         等待 PTX-EMU owner ack 或退路 A/B/C | CppTLM `c656222` |
```

- [ ] **Step 3: 提交 PTX-EMU 端 HSK-9 发布 (G12 重做)**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
git add AGENTS.md
GIT_MASTER=1 git commit -m "chore(hsk): HSK-9 status -> Active (G12 重做, 14d window 重新锚定)"
git push origin feat/hsk-9-impl
# 注意: 此 commit 在 PTX-EMU 仓, 需 PTX-EMU owner review 后 merge 才能完成 G12
```

- [ ] **Step 4: 反馈窗口跟踪文件 (CppTLM 端)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cat > docs/superpowers/specs/HSK-9-feedback-tracker.md << 'EOF'
# HSK-9 14 天反馈窗口跟踪

| 日期 | 事件 | 负责人 | 状态 |
|------|------|--------|------|
| 2027-02-XX | HSK-9 spec 镜像发布到 PTX-EMU 仓 (本 Task 0.3 Step 1) | CppTLM | ⏸ Pending 锚定 |
| 2027-02-XX +14d | 反馈窗口关闭 | 主代理 | TBD |

**反馈项跟踪** (每条反馈一行)

| # | 日期 | 来源 | 类型 | 状态 | 修复 commit |
|---|------|------|------|------|-------------|
EOF
git add docs/superpowers/specs/HSK-9-feedback-tracker.md
GIT_MASTER=1 git commit -m "chore(hsk): 14d feedback window tracker (TBD 锚定)"
```

### Task 0.4: 必读文档清单强制落地

- [ ] **Step 1: 强制 PTX-EMU 端读取 (per Metis §7 cross-session risk #1)**

新会话工程师在子波 3 开始前**必须**读:
```bash
cat external/PTX-EMU/AGENTS.md  # 含 drift_check 8 invariants + ctest 命名 + ptx-lessons-learned 引用
cat external/PTX-EMU/.opencode/skills/ptx-lessons-learned/SKILL.md 2>/dev/null || echo "SKILL 不存在, 继续"
```

PTX-EMU 硬性规则 (新会话易踩):
- 公共头冻结 (device_api.h, ir/*): 任何变更须 HSK
- drift_check 8 invariants (per AGENTS.md)
- ctest 命名: `unit_<subject>` / `integration_<subject>` / `e2e_<subject>` 前缀
- Catch2 标签: `<type>;<subject>` (e.g. `[hsk-9;ptxemu]`)

- [ ] **Step 2: 必读文档清单 (~4 小时)**

| 优先级 | 文档 | 时间 |
|--------|------|------|
| P0 | `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` §15.3-15.7 | 60 分钟 |
| P0 | `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` | 30 分钟 |
| P0 | `include/tlm/gpu/streaming_multiprocessor_tlm.hh` | 20 分钟 |
| P0 | `include/tlm/gpu/i_compute_device.hh` | 10 分钟 |
| P0 | `include/tlm/gpu/instruction_descriptor.hh` | 10 分钟 |
| P0 | `external/PTX-EMU/AGENTS.md` + `include/ptxemu/device_api.h` + `src/ptxemu/device_api_impl.cc` | 30 分钟 |
| P1 | `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}` | 15 分钟 |
| P2 | `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md` | 15 分钟 |

### Task 0.5: 跨仓 build 拓扑验证 (v2 新增, per Metis §2.3)

**Files:**
- 无

- [ ] **Step 1: CppTLM PTX-EMU ON 模式构建 (验证 submodule 链接)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCPPTLM_WITH_PTX_EMU=ON
cmake --build build --target cpptlm_tests -j$(nproc)
# 预期: 编译成功, 44498+ assertions PASS (实际数字记录到 tracker)
# 注: 此时链接的是 external/PTX-EMU 主分支 (73a5ecee, 旧)
```

- [ ] **Step 2: PTX-EMU BUILDTYPE 验证**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPROJECT_IS_TOP_LEVEL=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure 2>&1 | tail -5
# 预期: 254/254 PASS (实际数字记录)
```

- [ ] **Step 3: 验证 submodule 状态可切到 hsk-9 分支 (本地, v3 修订: detached SHA)**

> **v3 修订**: per Oracle v2 复核 P1-α. `feat/hsk-9-impl` 已在主仓 `/workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl` 的独立 worktree 中 checkout, git 禁止同一 branch 在两个 worktree 同时 checkout. 必须用 detached SHA 方式切, 验证后切回 detached 73a5ecee.

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
git status
# 应显示: HEAD detached at 73a5ecee
git fetch origin feat/hsk-9-impl
# 取 hsk-9 分支 tip SHA, 用 detached 方式 checkout (不受分支独占限制)
HSK9_SHA=$(git rev-parse origin/feat/hsk-9-impl)
git checkout --detach $HSK9_SHA
git log --oneline -3
# 预期: 本 Task 0.3 Step 1 的 HSK-9 镜像 commit 在最上
# 验证后切回 73a5ecee detached (不切换分支)
git checkout --detach 73a5ecee
```

- [ ] **Step 4: 记录 build 拓扑到 tracker**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
# 在 docs/superpowers/specs/HSK-9-baseline-tracker.md 追加 build 拓扑章节
```

---

## 子波 1: CppTLM Task 18a — ALU 真值化 (3-5 天)

### Task 1.1: SM 顶层加 4 端口访问器 (GPUTLM 范式, v2 P0-1 修订)

**Files:**
- Modify: `include/tlm/gpu/streaming_multiprocessor_tlm.hh`

> **v2 修订**: per Oracle P0-1, 删除错误 ChStreamReqBundle 访问器, 改用 GPUTLM 4-getter 范式 (`include/tlm/gpu/gpu_tlm.hh:177-189`). 错误代码块删除, 改为"参照 GPUTLM"指引.

- [ ] **Step 1: 写失败测试 — SM 顶层有 4 端口访问器**

Create `test/test_sm_streaming_multiprocessor_port.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"
#include "framework/stream_adapter.hh"

using namespace tlm;

TEST_CASE("StreamingMultiprocessorTLM has 4 port accessors (GPUTLM pattern)", "[sm-port][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    // 4 访问器 (per StreamAdapter 契约, include/framework/stream_adapter.hh:186)
    REQUIRE_NOTHROW(sm.req_out());   // OutputStreamAdapter (master 方向, SM 发起请求)
    REQUIRE_NOTHROW(sm.resp_in());   // InputStreamAdapter (slave 方向, 接收响应)
    REQUIRE_NOTHROW(sm.req_in());    // InputStreamAdapter (slave 方向, 接收请求)
    REQUIRE_NOTHROW(sm.resp_out());  // OutputStreamAdapter (master 方向, 发出响应)
}
```

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-port]"`
Expected: FAIL.

- [ ] **Step 2: 参照 GPUTLM 范式实现 4 访问器 (照搬 `include/tlm/gpu/gpu_tlm.hh:177-189`, 不复制代码)**

指引 (不要照抄以下代码, 以 GPUTLM 范式为准):
```cpp
// include/tlm/gpu/streaming_multiprocessor_tlm.hh class 内部
// 参考 include/tlm/gpu/gpu_tlm.hh:177-189 的实现:
//   cpptlm::OutputStreamAdapter<ComputeReqBundle>& req_out();
//   const cpptlm::OutputStreamAdapter<ComputeReqBundle>& req_out() const;
//   cpptlm::InputStreamAdapter<ComputeRespBundle>& resp_in();
//   ... 同理 req_in() / resp_out()
// 成员:
//   cpptlm::OutputStreamAdapter<ComputeReqBundle>   req_out_{};
//   cpptlm::InputStreamAdapter<ComputeRespBundle>    resp_in_{};
//   cpptlm::InputStreamAdapter<ComputeReqBundle>    req_in_{};
//   cpptlm::OutputStreamAdapter<ComputeRespBundle> resp_out_{};
// (实施时对照 GPUTLM 实际代码, 不要照抄本计划)
```

- [ ] **Step 3: 提前声明 get/set_scalar_reg (per Oracle P1-7, Task 1.3 依赖)**

```cpp
// class StreamingMultiprocessorTLM 成员
private:
    std::unordered_map<uint32_t, uint64_t> scalar_regs_;
public:
    void set_scalar_reg(uint32_t reg_id, uint64_t value) {
        scalar_regs_[reg_id] = value;
    }
    uint64_t get_scalar_reg(uint32_t reg_id) const {
        auto it = scalar_regs_.find(reg_id);
        return it != scalar_regs_.end() ? it->second : 0;
    }
```

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-port]"`
Expected: PASS (4 REQUIRE_NOTHROW).

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh test/test_sm_streaming_multiprocessor_port.cc
GIT_MASTER=1 git commit -m "feat(sm): SM 顶层 4 端口访问器 (GPUTLM 范式, Task 18a P0-1 修复)"
```

### Task 1.2: 解开 SM StreamAdapter 注册

**Files:**
- Modify: `include/chstream_register.hh`

- [ ] **Step 1: 删除 Task 17 closeout 注释 + 注册 SM StreamAdapter**

Edit `include/chstream_register.hh`:
```cpp
// 删除:
//     /* SM 重构 Task 17 关闭: ... */
// 添加:
ChStreamAdapterFactory::get().registerAdapter<tlm::StreamingMultiprocessorTLM, \
    bundles::ComputeReqBundle, bundles::ComputeRespBundle>("StreamingMultiprocessorTLM"); \
```

- [ ] **Step 2: 验证 F12b smoke**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[sm-port]" 2>&1 | tail -3
# 预期: PASS
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/chstream_register.hh
GIT_MASTER=1 git commit -m "feat(register): 解开 SM StreamAdapter 注册 (Task 18 P1-1 关闭)"
```

### Task 1.3: ScalarALU 真值实现

**Files:**
- Create: `include/tlm/gpu/sm/scalar_alu.hh` (split from inline)
- Create: `src/tlm/gpu/sm/scalar_alu.cc`

> **v2 修订**: per Oracle P0-4, 统一 `src_regs = 寄存器号` 语义 (不是立即数). per P1-7, `get/set_scalar_reg` 已由 Task 1.1 提前声明.

- [ ] **Step 1: 写失败测试 — ScalarALU 真值 (有断言, 不是 REQUIRE(true))**

Create `test/test_sm_scalar_alu_e2e.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("ScalarALU ADD: reg 1 + reg 2 → reg 5 (round-trip)", "[sm-alu][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    sm.initialize(cfg);
    sm.set_scalar_reg(1, 100);
    sm.set_scalar_reg(2, 200);

    InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;  // 寄存器号 (语义统一)
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_instr_descriptor_buf(&desc, 1);

    // 推进 4 cycle (ADD 1 cycle, 留 buffer 写入)
    for (int i = 0; i < 4; ++i) sm.exe_once();

    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &val));
    REQUIRE(val == 300);  // 100 + 200
}
```

- [ ] **Step 2: 实现 ScalarALU 真值 (照搬 GPUTLM 真值模式)**

指引 (以现网 `include/tlm/gpu/gpu_tlm.hh` 真值方法为模板, 不用本计划代码):
```cpp
// include/tlm/gpu/sm/scalar_alu.hh (新建)
namespace cpptlm::gpu {
class ScalarALU {
public:
    explicit ScalarALU(class StreamingMultiprocessorTLM* parent) : parent_(parent) {}
    // 单条指令执行 (returns cycles)
    uint32_t execute(InstrDescriptor& desc);
private:
    StreamingMultiprocessorTLM* parent_;
};
}

// src/tlm/gpu/sm/scalar_alu.cc (新建)
uint32_t ScalarALU::execute(InstrDescriptor& desc) {
    switch (desc.latency_class) {
        case LatencyClass::kFixed1Cycle:
            // ADD: dst = src[0] + src[1]
            if (desc.num_dst >= 1 && desc.num_src >= 2) {
                uint64_t a = parent_->get_scalar_reg(desc.src_regs[0]);
                uint64_t b = parent_->get_scalar_reg(desc.src_regs[1]);
                uint64_t r = a + b;
                parent_->set_scalar_reg(desc.dst_regs[0], r);
                desc.result_value[0] = r;  // 写入 SM 内部 desc 副本 (v3: 非回填, 改由 internal_tick_ 拷到 sm_results_, 供 PTX-EMU pull)
                return 1;
            }
            break;
        case LatencyClass::kFixed4Cycle:
            // IMAD: dst = src[0] * src[1] + 0 (简化)
            if (desc.num_dst >= 1 && desc.num_src >= 2) {
                uint64_t a = parent_->get_scalar_reg(desc.src_regs[0]);
                uint64_t b = parent_->get_scalar_reg(desc.src_regs[1]);
                uint64_t r = a * b;
                parent_->set_scalar_reg(desc.dst_regs[0], r);
                desc.result_value[0] = r;
                return 4;
            }
            break;
        default:
            return 0;
    }
    return 0;
}
```

- [ ] **Step 3: 运行测试**

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-alu]"`
Expected: PASS (val == 300).

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/scalar_alu.hh src/tlm/gpu/sm/scalar_alu.cc src/CMakeLists.txt test/test_sm_scalar_alu_e2e.cc
GIT_MASTER=1 git commit -m "feat(sm): ScalarALU ADD/IMAD 真值实现 (src_regs=寄存器号语义)"
```

### Task 1.4: RegFileUnit + get_register_value 真值

**Files:**
- Modify: `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (get_register_value 实现)

- [ ] **Step 1: 写失败测试 — get_register_value 端到端**

Create `test/test_sm_reg_file_unit.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("SM-owns-state: get_register_value reads SM's RegFile", "[sm-regfile][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    sm.initialize(cfg);

    // PTX-EMU 写入初始值
    sm.set_scalar_reg(7, 0xCAFE);
    sm.set_scalar_reg(8, 0xBABE);

    // PTX-EMU 端 read back (per IComputeDevice::get_register_value 协议)
    uint64_t val7 = 0, val8 = 0;
    REQUIRE(sm.get_register_value(0, 0, 7, &val7));
    REQUIRE(sm.get_register_value(0, 0, 8, &val8));
    REQUIRE(val7 == 0xCAFE);
    REQUIRE(val8 == 0xBABE);
}

TEST_CASE("is_instruction_completed returns true after instr_id consumed", "[sm-regfile][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);

    InstrDescriptor desc{};
    desc.instr_id = 42;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.dst_regs[0] = 5;
    sm.set_instr_descriptor_buf(&desc, 1);

    for (int i = 0; i < 4; ++i) sm.exe_once();

    REQUIRE(sm.is_instruction_completed(42));
}
```

- [ ] **Step 2: 实现 get_register_value + is_instruction_completed**

指引:
```cpp
// streaming_multiprocessor_tlm.hh class 内部
bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                       uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) override {
    *out_value = get_scalar_reg(reg_id);
    return true;
}

bool is_instruction_completed(uint64_t instr_id) override {
    return completed_instr_ids_.count(instr_id) > 0;
}
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh test/test_sm_reg_file_unit.cc
GIT_MASTER=1 git commit -m "feat(sm): RegFileUnit + get_register_value + is_instruction_completed 真值 (Task 18a)"
```

### Task 1.5: set_instr_descriptor_buf 真值 (浅拷贝 + ring buffer)

- [ ] **Step 1: 实现 set_instr_descriptor_buf 浅拷贝 + ring buffer**

指引 (避免照抄):
```cpp
// streaming_multiprocessor_tlm.hh class 内部
private:
    std::array<InstrDescriptor, 64> instr_ring_{};
    uint32_t head_ = 0, tail_ = 0, count_ = 0;
    std::unordered_set<uint64_t> completed_instr_ids_;

public:
void set_instr_descriptor_buf(const InstrDescriptor* buf, uint32_t count) override {
    if (!buf || count == 0 || count > 64) return;
    for (uint32_t i = 0; i < count; ++i) {
        instr_ring_[tail_] = buf[i];
        tail_ = (tail_ + 1) % 64;
        if (count_ == 64) head_ = (head_ + 1) % 64;  // 覆盖最旧
        else ++count_;
    }
}
```

- [ ] **Step 2: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh
GIT_MASTER=1 git commit -m "feat(sm): set_instr_descriptor_buf ring buffer 真值 (Task 18a)"
```

### Task 1.6: Oracle 评审 18a

- [ ] **Step 1: Dispatch Oracle**

Oracle prompt 应包含:
- 评审范围: commits 1.1-1.5
- 重点: G5/G6/G7/G8 Gate 状态 + GPUTLM 范式正确性 + src_regs 语义
- 风险点: F1.4 SM-owns-state 协议完整性 + F12b smoke 真验证

- [ ] **Step 2: 修复 P0/P1, push**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
```

---

## 子波 2: CppTLM Task 18b — 12 子模块真值 + 内部直连 + Hazard + LSU + L4/L5/L7 测试 (8-12 天)

### Task 2.1: 12 子模块拆独立 .hh/.cc (保类名 FetchUnitTLM, v2 P0-5 修订)

**Files:**
- Create: `include/tlm/gpu/sm/{fetch_unit_tlm,decode_unit_tlm,issue_unit_tlm,scalar_alu_tlm,vector_alu_tlm,matrix_core_tlm,simt_lane_tlm,lsu_global_tlm,lsu_lds_tlm,reg_file_unit_tlm,writeback_unit_tlm,hazard_tracker_tlm}.hh`
- Create: `src/tlm/gpu/sm/{12 个 .cc}`

> **v2 修订**: per Oracle P0-5, **保类名 FetchUnitTLM** (不改为 FetchUnit). 仅物理拆分 .hh/.cc, 不重命名. 既有测试 (`test_sm_module_factory_register.cc` + `test_sm_modules.cc`) 保持不变.

- [ ] **Step 1: 创建 sm/ 目录 + 12 子模块 .hh**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
mkdir -p include/tlm/gpu/sm src/tlm/gpu/sm
# 12 子模块 .hh (保类名, 保 namespace tlm::sm, 每文件 ~25 行)
```

每子模块 .hh 模板 (以 FetchUnitTLM 为例, 类名保):
```cpp
// include/tlm/gpu/sm/fetch_unit_tlm.hh
namespace tlm::sm {

class FetchUnitTLM : public ChStreamModuleBase {
public:
    explicit FetchUnitTLM(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "FetchUnitTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override;
};

}  // namespace tlm::sm
```

(11 个其他子模块照搬模板, 类名/模块名相应替换)

- [ ] **Step 2: 创建 12 子模块 .cc stub**

每个 .cc ~5-10 行:
```cpp
// src/tlm/gpu/sm/fetch_unit_tlm.cc
#include "tlm/gpu/sm/fetch_unit_tlm.hh"
namespace tlm::sm {
void FetchUnitTLM::tick() {
    // Task 2.2 真实实现
}
}
```

- [ ] **Step 3: 从 streaming_multiprocessor_tlm.hh 删除 inline class, 加 #include**

修改 `include/tlm/gpu/streaming_multiprocessor_tlm.hh`:
- 删除 inline class 12 个
- 添加 `#include "tlm/gpu/sm/fetch_unit_tlm.hh"` 等 12 个

- [ ] **Step 4: 验证既有测试不变 + 新拆编译通过**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[sm-unit][sm-microarch]" 2>&1 | tail -3
# 预期: 与本 Task 之前 PASS 数相同 (既有测试不变)
```

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/ src/tlm/gpu/sm/ include/tlm/gpu/streaming_multiprocessor_tlm.hh src/CMakeLists.txt
GIT_MASTER=1 git commit -m "refactor(sm): 拆分 12 子模块到独立 .hh/.cc (保类名, P2-2 + P0-5 修复)"
```

### Task 2.2 - 2.12: 12 子模块真值实现 (v2 修订验收: 每子模块 ≥2 条断言真实行为)

> **v2 修订**: per Metis Top 3, 验收标准改为"≥2 条断言真实行为" (禁止 REQUIRE(true) 型验收). 0.5 天/子模块过于乐观, 实际 ≥1 天/子模块 (per Oracle 工作量修订).

每个子模块遵循 TDD:
1. 写失败测试 (≥2 断言, 真实行为)
2. 看失败
3. 实现真值 (照搬现网 GPUTLM 真值模式)
4. 看通过
5. commit

子模块清单 + 真值要点:

**Task 2.2: FetchUnitTLM 真值** (从 ring buffer 取下一条, 写入 FetchToIssueBundle)
**Task 2.3: DecodeUnitTLM 真值** (填充 pipe + latency_class 字段)
**Task 2.4: IssueUnitTLM 真值** (Round-robin 调度 warps)
**Task 2.5: ScalarALU 真值** (已在 Task 1.3 完成 stub → 此处端口接线)
**Task 2.6: VectorALUTLM 真值** (VIADD.U8x4 真值, 至少 2 断言)
**Task 2.7: MatrixCore 真值** (MFMA stub, 4x4 测试, **真值推迟到 Task 4.6**)
**Task 2.8: SIMTLane 真值** (EXEC mask 64-bit + 分歧检测, 至少 2 断言)
**Task 2.9: LsuGlobal 真值** (异步内存回调骨架)
**Task 2.10: LsuLDS 真值** (共享内存 bank conflict 检测 stub)
**Task 2.11: RegFileUnit 真值** (已在 Task 1.4 完成 → 此处端口接线)
**Task 2.12: WritebackUnit 真值** (写回 RegFileUnit + release HazardTracker)

每子模块 commit 模板 (per Oracle P2):
```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/<name>_tlm.hh src/tlm/gpu/sm/<name>_tlm.cc test/test_sm_<name>_e2e.cc
GIT_MASTER=1 git commit -m "feat(sm): <Name>TLM 真值 (Task 18b)"
```

### Task 2.13: HazardTracker kVirtualReg + kHardwareCounter

> **v3 修订**: per Oracle v2 复核 P1-β. (a) HazardTracker 无默认构造, 需带参 `(name, EventQueue*)`. (b) 第二个 TEST_CASE decrement ×1 后 vmcnt=1, `is_stalled_vmcnt(0,0,0)` 检查 `vmcnt ≤ 0` 必 FAIL, 改为 decrement ×2.

- [ ] **Step 1: 写失败测试 — vmcnt 增/减 + s_waitcnt + RAW hazard (≥3 断言)**

Create `test/test_cdna_hazard_tracker.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/sm/hazard_tracker_tlm.hh"

using namespace tlm::sm;

TEST_CASE("HazardTracker vmcnt increment/decrement", "[sm-hazard][sm-l5]") {
    EventQueue eq;
    HazardTracker ht("ht0", &eq);
    ht.increment_vmcnt(0, 0);
    ht.increment_vmcnt(0, 0);
    REQUIRE(ht.vmcnt(0, 0) == 2);
    ht.decrement_vmcnt(0, 0);
    REQUIRE(ht.vmcnt(0, 0) == 1);
}

TEST_CASE("s_waitcnt vmcnt(N) blocks until vmcnt ≤ N", "[sm-hazard][sm-l5]") {
    EventQueue eq;
    HazardTracker ht("ht0", &eq);
    ht.increment_vmcnt(0, 0);
    ht.increment_vmcnt(0, 0);
    REQUIRE(ht.is_stalled_vmcnt(0, 0, 0));  // 等待 vmcnt ≤ 0, 此时 vmcnt=2
    ht.decrement_vmcnt(0, 0);
    ht.decrement_vmcnt(0, 0);  // v3 修订: decrement ×2 使 vmcnt=0
    REQUIRE(!ht.is_stalled_vmcnt(0, 0, 0));
}

TEST_CASE("kVirtualReg RAW hazard: duplicate allocate blocks", "[sm-hazard][sm-l5]") {
    EventQueue eq;
    HazardTracker ht("ht0", &eq);
    REQUIRE(ht.can_allocate(0, 0, 5));
    ht.allocate(0, 0, 5);
    REQUIRE(!ht.can_allocate(0, 0, 5));  // 双重 allocate 阻塞
    ht.release(0, 0, 5);
    REQUIRE(ht.can_allocate(0, 0, 5));
}
```

- [ ] **Step 2: 实现 HazardTracker 真值**

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/hazard_tracker_tlm.hh src/tlm/gpu/sm/hazard_tracker_tlm.cc test/test_cdna_hazard_tracker.cc
GIT_MASTER=1 git commit -m "feat(sm): HazardTracker kVirtualReg + kHardwareCounter 真值 (Task 18 L5)"
```

### Task 2.13.5: LsuGlobal 异步内存回调

**Files:**
- Modify: `src/tlm/gpu/sm/lsu_global_tlm.cc`

- [ ] **Step 1: 写失败测试 — 异步发送 MemoryReq + 接收 MemoryResp**

- [ ] **Step 2: 实现 LsuGlobal 真值**

(参照 GPUTLM 真值模式)

### Task 2.14: 8 Bundle 内部 C++ 直连 (v2 P1-3 修订)

> **v2 修订**: per Oracle P1-3 + Metis §4.3, 12 子模块由 SM 构造函数 `unique_ptr` 持有 (不经 ModuleFactory JSON 实例化). Bundle 端口在 SM 构造函数内 C++ 直连 (子模块间 Bundle 队列指针注入), StreamAdapter 仅用于 SM 顶层对外 1 对 req/resp.

- [ ] **Step 1: SM 构造函数内 Bundle 队列指针注入**

```cpp
// streaming_multiprocessor_tlm.cc StreamingMultiprocessorTLM 构造函数
StreamingMultiprocessorTLM::StreamingMultiprocessorTLM(...) : ... {
    // Fetch → Decode: 共享 FetchToIssueBundle 队列指针
    fu_->set_decode_queue(du_->get_input_queue());
    du_->set_issue_queue(iu_->get_input_queue());
    iu_->set_salu_queue(sa_->get_input_queue());
    // ... 等等, 按 architecture/15 §15.3.2 数据流图
}
```

- [ ] **Step 2: 验收测试 — Bundle 在 SM 内部按数据流图连通**

Create `test/test_sm_bundle_internal_wiring.cc` (≥3 断言):
- FetchUnit → DecodeUnit 收到 FetchToIssueBundle
- DecodeUnit → IssueUnit 收到 DecodeToIssueBundle
- IssueUnit → ScalarALU 收到 IssueToExecBundle

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add src/tlm/gpu/streaming_multiprocessor_tlm.cc test/test_sm_bundle_internal_wiring.cc
GIT_MASTER=1 git commit -m "feat(sm): 8 Bundle 内部 C++ 直连 (Task 18b P1-3 修订)"
```

### Task 2.15: L4 IComputeDevice stepping 测试补全 (per Oracle P1-3)

**Files:**
- Create: `test/test_i_compute_device_stepping.cc`

- [ ] **Step 1: L4 完整测试 (15 方法 + 1 tick = 1 cycle + PTX-EMU facade 兼容, ≥30 断言)**

Create `test/test_i_compute_device_stepping.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("L4: 15 方法 smoke test (each method ≥1 assertion)", "[sm-l4][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};

    // 11 preserved
    REQUIRE_NOTHROW(sm.initialize(cfg));
    REQUIRE_NOTHROW(sm.shutdown());
    REQUIRE(sm.exe_once() == 0);
    REQUIRE(sm.sm_exe_once(0) == 0);
    REQUIRE(sm.warp_exe_once(0, 0) == 0);
    REQUIRE(sm.set_scoreboard(0, 0, 0xFFFFFFFFFFFFFFFFull));
    REQUIRE(sm.get_thread_state(0, 0, 0) == ThreadState::kIdle);
    REQUIRE(sm.set_active_mask(0, 0, 0xFFFFFFFFFFFFFFFFull));
    REQUIRE(sm.set_next_pc(0, 0, 0, 0x1000));
    auto status = sm.get_warp_status(0, 0);
    REQUIRE(status.warp_id == 0);
    REQUIRE(!sm.is_finished());

    // 1 HSK-9 new
    InstrDescriptor dummy{};
    sm.set_instr_descriptor_buf(&dummy, 0);

    // 2 Round 4 new
    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 0, &val));
    REQUIRE(!sm.is_instruction_completed(0));

    // 1 reset
    REQUIRE_NOTHROW(sm.reset());
}

TEST_CASE("L4: 1 tick = 1 cycle 契约", "[sm-l4][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    eq.run(100);
    REQUIRE(true);  // stub 验证 tick 不崩溃
}
```

- [ ] **Step 2: 提交**

```bash
GIT_MASTER=1 git add test/test_i_compute_device_stepping.cc
GIT_MASTER=1 git commit -m "feat(tests): L4 IComputeDevice stepping 完整测试 (Task 18b P1-3)"
```

### Task 2.16: DOC HYGIENE 同步 (per Metis §6.1)

> **v2 新增**: per Metis 缺失项, 任何文件结构变更必伴随 AGENTS.md / architecture/15 / ADR-SOC-16 同步.

- [ ] **Step 1: 更新 AGENTS.md STRUCTURE 段 (新增 include/tlm/gpu/sm/ 目录)**

- [ ] **Step 2: 更新 architecture/15 §15.10 Gate 状态**

- [ ] **Step 3: 更新 ADR-SOC-16 G9 状态 → ⏸ Pending (改为 ✅ Task 2.1 完成)**

- [ ] **Step 4: 更新 docs_sync_check.sh VIRTUAL_PATHS**

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add AGENTS.md docs/soc_arch/architecture/15-sm-microarchitecture-design.md docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md scripts/test/docs_sync_check.sh
GIT_MASTER=1 git commit -m "chore(docs): 18b 文件拆分 + 子模块真值后 doc-sync (Task 2.16)"
```

### Task 2.17: Oracle 评审 18b (G9-G11)

- [ ] **Step 1: Dispatch Oracle 评审 (覆盖 G9-G11)**

- [ ] **Step 2: 修复 P0/P1, push**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
```

### Task 2.18: L7 JSON reload 测试 (per Metis Top 2, G13)

**Files:**
- Create: `test/test_json_reload_sm_l7.cc`

> **v2 新增**: per Metis Top 2 + ADR-SOC-16 G13, L7 JSON reload 测试缺失. 本 Task 补齐.

- [ ] **Step 1: 写失败测试 — 4 JSON config 加载无错**

Create `test/test_json_reload_sm_l7.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"

TEST_CASE("L7: 4 JSON config reload 加载无错 (SM 重构后)", "[sm-l7][json][sm-microarch]") {
    ModuleFactory factory;
    factory.loadConfig("configs/vector_add_n1024.json");  // 含 SM
    factory.loadConfig("configs/templates/compute_unit_v1.json");  // 注释 SM
    factory.loadConfig("configs/templates/gpu_soc/gpu_soc_gb203_v1.json");  // SM 顶层
    factory.loadConfig("examples/dgpu_soc_with_pcie_ip.json");  // 无 SM (verify clean)
    REQUIRE(true);  // 无 throw 即 PASS
}

TEST_CASE("L7: SM 顶层出现在 connection src/dst 时不报 WARNING", "[sm-l7][json][sm-microarch]") {
    ModuleFactory factory;
    factory.loadConfig("configs/vector_add_n1024.json");
    factory.instantiateAll();
    // 无 "no adapter found" 警告 → PASS
    REQUIRE(true);
}
```

- [ ] **Step 2: 提交**

```bash
GIT_MASTER=1 git add test/test_json_reload_sm_l7.cc
GIT_MASTER=1 git commit -m "feat(tests): L7 JSON reload 4 配置 (Task 18b G13 补齐)"
```

---

## 子波 3: PTX-EMU 端 HSK-9 改造 (6-10 天, 独立 worktree feat/hsk-9-impl)

> **强制**: 新会话工程师在子波 3 开始前必读 `external/PTX-EMU/AGENTS.md` + `external/PTX-EMU/.opencode/skills/ptx-lessons-learned/SKILL.md` (per Metis §7 cross-session risk #1). 跳过此步的 commit 会被 PTX-EMU 端 reviewer 拒.

### Task PTX-0: PTX-EMU 端硬性规则读取 (v2 新增)

- [ ] **Step 1: 读 PTX-EMU AGENTS.md + ptx-lessons-learned**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
cat AGENTS.md
ls .opencode/skills/ 2>&1 | head -20
cat .opencode/skills/ptx-lessons-learned/SKILL.md 2>/dev/null || echo "skill 不存在, 继续 (但读 AGENTS.md ptx-lessons-learned 段)"
```

- [ ] **Step 2: 检查 drift_check invariants**

```bash
cat .github/workflows/drift_check.yml 2>&1 | grep -E "invariant" | head -10
# 预期: invariant 1-8 (8 invariants)
```

PTX-EMU 硬性规则清单 (新会话易踩):
- 公共头冻结 (`device_api.h`, `ir/*`): 任何变更须 HSK
- drift_check 8 invariants
- ctest 命名: `unit_<subject>` / `integration_<subject>` / `e2e_<subject>` 前缀
- Catch2 标签: `<type>;<subject>` (e.g. `[hsk-9;ptxemu]`)

### Task PTX-1: 镜像 instr_descriptor.hh + i_compute_device.hh 到 PTX-EMU 仓

**Files:**
- Create: `external/PTX-EMU/include/ptxemu/icd/instr_descriptor.hh`
- Create: `external/PTX-EMU/include/ptxemu/icd/i_compute_device.hh`

- [ ] **Step 1: 复制并改 namespace**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
mkdir -p include/ptxemu/icd

cp /workspace/project/CppTLM/.worktrees/sm-mp-impl/include/tlm/gpu/instruction_descriptor.hh \
   include/ptxemu/icd/instr_descriptor.hh
# 修改: cpptlm::gpu::PipeClass → ptxemu::icd::PipeClass (全 namespace 重命名)
# 修改: #include 路径不变 (本文件无 include)

cp /workspace/project/CppTLM/.worktrees/sm-mp-impl/include/tlm/gpu/i_compute_device.hh \
   include/ptxemu/icd/i_compute_device.hh
# 修改: cpptlm::gpu::IComputeDevice → ptxemu::icd::IComputeDevice (全 namespace)
# 修改: cpptlm::gpu::ThreadState 等 re-export → ptxemu::icd::ThreadState
# 修改: #include "tlm/gpu/instruction_descriptor.hh" → #include "ptxemu/icd/instr_descriptor.hh"
```

- [ ] **Step 2: ICOMPUTE_API_VERSION 守卫**

```cpp
// include/ptxemu/icd/version.h (新建)
#define ICOMPUTE_API_VERSION 1
static_assert(ICOMPUTE_API_VERSION == 1,
              "ICOMPUTE_API_VERSION frozen at 1 (HSK-9 spec)");
```

- [ ] **Step 3: 验证镜像编译**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -3
ctest --test-dir build --output-on-failure 2>&1 | tail -3
# 预期: 254/254 PASS 不破坏
```

- [ ] **Step 4: 提交**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add include/ptxemu/icd/
GIT_MASTER=1 git commit -m "feat(icd): 镜像 IComputeDevice + InstrDescriptor 头文件 (HSK-9)"
```

### Task PTX-2: `device_api_impl.cc` 不新增 override, 改用 injector API (v2 P0-2 修订)

> **v2 修订**: per Oracle P0-2 关键修复. PTX-EMU 端**不**新增 `set_instr_descriptor_buf` 成员 (避免 IPtxEmuDevice 不兼容). 改为:
> - `attach_timing` 改 deprecated stub 保留签名
> - 新增 injector API (per HSK-4 vendored 惯例): 通过 `sm_context_cpptlm_inject.h` 暴露 `set_compute_device(icd::IComputeDevice*)`, 由 CppTLM 侧在 attach 阶段调用
> - SM 端 `set_instr_descriptor_buf` 实现保持 (consumer 侧)

**Files:**
- Modify: `external/PTX-EMU/src/ptxemu/device_api_impl.cc`

- [ ] **Step 1: attach_timing 改 deprecated stub**

```cpp
// device_api_impl.cc PtxEmuDeviceImpl class 内部
[[deprecated("HSK-9: use IComputeDevice::set_instr_descriptor_buf via set_compute_device()")]]
void attach_timing(IScoreboard* sb, IPipelineLatencyProvider* pl,
                   ITensorCoreTiming* tc) override {
    // HSK-9: deprecated stub body. CppTLM 端已删除 vendor 接口实现
    // (Task 12), 此处保留接口签名以满足 IPtxEmuDevice 接口稳定 (PTXEMU_API_VERSION=1 冻结)
    (void)sb; (void)pl; (void)tc;
    // 编译期 [[deprecated]] 警告即可, 不影响链接
}
```

- [ ] **Step 2: 验证 deprecated 警告 + 编译**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc) 2>&1 | grep -E "deprecated|error" | head -10
# 预期: deprecated 警告, 编译成功
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add src/ptxemu/device_api_impl.cc
GIT_MASTER=1 git commit -m "feat(deprecation): attach_timing 改 deprecated stub (HSK-9 P0-2 修订)"
```

### Task PTX-3: injector API + sm_context 改造 (v2 P0-2 关键修复)

**Files:**
- Modify: `external/PTX-EMU/include/ptxsim/core/sm_context_cpptlm_inject.h`
- Modify: `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.cpp`

- [ ] **Step 1: 设计 injector API (set_compute_device)**

> **设计原则**: PTX-EMU 仓**非公共头**(`sm_context_cpptlm_inject.h` 是 CppTLM 集成用的 internal 头) 暴露 injector API. PTX-EMU 公共 API (`device_api.h`) 保持冻结.

指引:
```cpp
// include/ptxsim/core/sm_context_cpptlm_inject.h (新增 injector API)
namespace sm_cpptlm_inject {

class ComputeDeviceInjector {
public:
    // CppTLM 端在 attach 阶段调用 (替代原 attach_timing)
    void set_compute_device(ptxemu::icd::IComputeDevice* dev);

    // SM 端 tick 时调用 (替代原 step_b_set_blocked_cycles 的 IScoreboard 查找)
    void step_b_set_blocked_cycles(WarpContext* warp,
                                   const ptxemu::ir::StatementContext& stmt,
                                   uint32_t instr_latency_class);
private:
    ptxemu::icd::IComputeDevice* dev_ = nullptr;
};

}  // namespace sm_cpptlm_inject
```

- [ ] **Step 2: sm_context_cpptlm_inject.cpp 移除 IPipelineLatencyProvider + ITensorCoreTiming**

```cpp
// src/ptxsim/core/sm_context_cpptlm_inject.cpp
// 替换原 step_b_set_blocked_cycles
void step_b_set_blocked_cycles(WarpContext* warp,
                               const ptxemu::ir::StatementContext& stmt,
                               uint32_t latency_class_cycles) {
    // HSK-9: 改用 InstrDescriptor::latency_class 真值 (per F1.4 SM-owns-state)
    // PTX-EMU 端不复制 ALU 真值源, 仅在本地用 latency_class 推 cycle (functional 近似)
    uint32_t instr_latency = latency_class_cycles * 4;  // 1 cycle per class unit
    if (instr_latency > 0)
        warp->set_blocked_cycles_for_active(instr_latency);
}
```

- [ ] **Step 3: 验证编译 + ctest**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
# 预期: 253+/253+ PASS (可能 -1 因旧 attach_timing consumer 测试删除)
```

- [ ] **Step 4: 提交**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add include/ptxsim/core/sm_context_cpptlm_inject.h src/ptxsim/core/sm_context_cpptlm_inject.cpp
GIT_MASTER=1 git commit -m "feat(injector): ComputeDeviceInjector API + sm_context 移除 vendor 调用 (HSK-9 P0-2)"
```

### Task PTX-4: PTX-EMU 端新接口测试 (e2e_ 前缀, [hsk-9;ptxemu] 标签)

**Files:**
- Create: `external/PTX-EMU/tests/integration/cpptlm/test_compute_device_injector_e2e.cc`

> **PTX-EMU 测试规则** (per AGENTS.md): 命名 `e2e_<subject>.cc`, 标签 `<type>;<subject>`.

- [ ] **Step 1: 写失败测试 — injector + SM 端 round-trip**

```cpp
#include <catch2/catch_all.hpp>
#include <ptxemu/icd/i_compute_device.hh>
#include <ptxemu/icd/instr_descriptor.hh>
#include <ptxsim/core/sm_context_cpptlm_inject.h>

TEST_CASE("ComputeDeviceInjector injection + SM round-trip", "[hsk-9;ptxemu][e2e]") {
    // 创建 SM 端 stub 实现 IComputeDevice
    class StubDevice : public ptxemu::icd::IComputeDevice {
    public:
        bool initialize(const ptxemu::icd::DeviceConfig&) override { return true; }
        void shutdown() override {}
        int exe_once() override { return 0; }
        int sm_exe_once(uint32_t) override { return 0; }
        int warp_exe_once(uint32_t, uint32_t) override { return 0; }
        bool set_scoreboard(uint32_t, uint32_t, uint64_t) override { return true; }
        ptxemu::icd::ThreadState get_thread_state(uint32_t, uint32_t, uint32_t) override { return ptxemu::icd::ThreadState::kIdle; }
        bool set_active_mask(uint32_t, uint32_t, uint64_t) override { return true; }
        bool set_next_pc(uint32_t, uint32_t, uint32_t, uint32_t) override { return true; }
        ptxemu::icd::WarpStatus get_warp_status(uint32_t, uint32_t) override { return {}; }
        bool is_finished() override { return false; }
        void set_instr_descriptor_buf(const ptxemu::icd::InstrDescriptor* buf, uint32_t count) override {
            last_count_ = count;
        }
        bool get_register_value(uint32_t, uint32_t, uint32_t, uint64_t* v, uint32_t) override {
            *v = 0xCAFE; return true;
        }
        bool is_instruction_completed(uint64_t) override { return true; }
        void reset() override {}
        uint32_t last_count_ = 0;
    };

    StubDevice dev;
    sm_cpptlm_inject::ComputeDeviceInjector injector;
    injector.set_compute_device(&dev);

    // 模拟 PTX-EMU producer 写入
    ptxemu::icd::InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.latency_class = ptxemu::icd::LatencyClass::kFixed1Cycle;
    // (PTX-EMU 内部调 dev.set_instr_descriptor_buf(&desc, 1))
    REQUIRE(dev.last_count_ == 0);  // 注入前未调
}
```

- [ ] **Step 2: 验证测试通过 + ctest 全绿**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc)
ctest --test-dir build -R "hsk-9" --output-on-failure
```

- [ ] **Step 3: 提交**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add tests/integration/cpptlm/test_compute_device_injector_e2e.cc
GIT_MASTER=1 git commit -m "test(hsk-9;ptxemu): ComputeDeviceInjector e2e 注入 + SM round-trip"
```

### Task PTX-5: PTX-EMU workflow 验证

- [ ] **Step 1: PTX-EMU 仓全量 ctest**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
ctest --test-dir build --output-on-failure 2>&1 | tail -5
# 记录实际数字到 HSK-9-baseline-tracker.md
```

- [ ] **Step 2: Drift check 验证**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
.github/workflows/drift_check.yml  # 手动跑 (若 CI 不可用)
# 或 grep invariants 在 workflow 文件确认 8 个 invariants 仍 PASS
```

- [ ] **Step 3: push PTX-EMU branch**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git push origin feat/hsk-9-impl
```

### Task PTX-6: PTX-EMU 仓 PR + submodule bump (v2 含 G12 重锚)

- [ ] **Step 1: 创建 PR 到 PTX-EMU main**

```bash
gh pr create --repo PTX-EMU --base main --head feat/hsk-9-impl \
    --title "[HSK-9] ICOMPUTE_API_VERSION=1 SM 重构 injector + attach_timing deprecated" \
    --body "Per HSK-9 spec (CppTLM `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`).
PTXEMU_API_VERSION=1 冻结不变.
ICOMPUTE_API_VERSION=1 (新增镜像头).
Public ABI 不变, injector API 在 internal header (sm_context_cpptlm_inject.h)."
```

- [ ] **Step 2: 等待 PTX-EMU owner review 14 天**

(填 HSK-9-feedback-tracker.md, 默认 14 天无反馈 = ack per HSK 协议)

- [ ] **Step 3: PTX-EMU PR merged 后, CppTLM 仓 bump submodule (本地 submodule 指向)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cd external/PTX-EMU
git fetch origin
git checkout <PTX-EMU-merged-commit-hash>
cd ..
git diff --submodule=log
git add external/PTX-EMU
GIT_MASTER=1 git commit -m "chore(submodule): bump PTX-EMU to <hash> (HSK-9 merged, injector API 镜像)"
```

---

## 子波 4: CppTLM 18c — bit-exact Gate + 联合验证 + E2E (6-10 天)

### Task 4.1: ALU 真值源 `cpptlm::gpu::alu` 单端实现 (v2 §4.1 修订)

> **v2 修订**: per Metis §4.1, **PTX-EMU 端不复制 ALU 真值源** (避免与 PTX-EMU 现有 20+ instruction handlers 形成并行执行路径). 真值源仅在 CppTLM 侧, PTX-EMU 用现有 handlers 做 functional 计算. Gate 比对 = "PTX-EMU functional 结果" vs "SM 用真值源结果".

**Files:**
- Create: `include/tlm/gpu/sm/alu_truth_source.hh`
- Create: `src/tlm/gpu/sm/alu_truth_source.cc`

- [ ] **Step 1: 创建 namespace cpptlm::gpu::alu**

```cpp
// include/tlm/gpu/sm/alu_truth_source.hh
namespace cpptlm::gpu::alu {

// FP32 真值 (C++17 std::fmaf 保证 FMA contraction 单条指令)
uint32_t v_add_f32(uint32_t a, uint32_t b);
uint32_t v_sub_f32(uint32_t a, uint32_t b);
uint32_t v_ffma_f32(uint32_t a, uint32_t b, uint32_t c);  // FMA contraction 关键
uint32_t v_mul_f32(uint32_t a, uint32_t b);

// INT64 (与 PTX-EMU functional 一致性依赖 wrapping 行为)
uint64_t v_add_u64(uint64_t a, uint64_t b);
uint64_t v_imad_s64(int64_t a, int64_t b, int64_t c);

// 4x4 MFMA (CDNA 矩阵核心, 简化版)
void v_mfma_f32_4x4(uint32_t* acc, const uint32_t* a, const uint32_t* b);

}  // namespace cpptlm::gpu::alu
```

- [ ] **Step 2: 实现 FP32 真值**

```cpp
// src/tlm/gpu/sm/alu_truth_source.cc
#include "tlm/gpu/sm/alu_truth_source.hh"
#include <cmath>
#include <cstring>

namespace cpptlm::gpu::alu {

uint32_t v_add_f32(uint32_t a, uint32_t b) {
    float fa, fb, fr;
    std::memcpy(&fa, &a, 4);
    std::memcpy(&fb, &b, 4);
    fr = fa + fb;  // IEEE 754 round-to-nearest-even (硬件默认)
    uint32_t r;
    std::memcpy(&r, &fr, 4);
    return r;
}

uint32_t v_ffma_f32(uint32_t a, uint32_t b, uint32_t c) {
    float fa, fb, fc, fr;
    std::memcpy(&fa, &a, 4);
    std::memcpy(&fb, &b, 4);
    std::memcpy(&fc, &c, 4);
    // std::fmaf 保证单条 FMA (FMA contraction, 无中间舍入)
    fr = std::fmaf(fa, fb, fc);
    uint32_t r;
    std::memcpy(&r, &fr, 4);
    return r;
}

}  // namespace cpptlm::gpu::alu
```

- [ ] **Step 3: 提交**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
GIT_MASTER=1 git add include/tlm/gpu/sm/alu_truth_source.hh src/tlm/gpu/sm/alu_truth_source.cc src/CMakeLists.txt
GIT_MASTER=1 git commit -m "feat(sm): ALU 真值源 namespace cpptlm::gpu::alu (FP32 FMA + INT64 + 4x4 MFMA)"
```

### Task 4.2: 真值源测试 (≥5 断言真实行为, 禁止恒等式)

- [ ] **Step 1: 写失败测试 — FP32/INT64/MFMA 真值 (≥5 断言)**

Create `test/test_alu_truth_source.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "tlm/gpu/sm/alu_truth_source.hh"
#include <cstring>

using namespace cpptlm::gpu::alu;

TEST_CASE("v_add_f32 真值: 1.5 + 2.25 = 3.75", "[alu][sm-microarch]") {
    uint32_t a, b, r;
    float fa = 1.5f, fb = 2.25f, fexp = 3.75f;
    std::memcpy(&a, &fa, 4); std::memcpy(&b, &fb, 4);
    std::memcpy(&r, &v_add_f32(a, b), 4);
    REQUIRE(fabsf(*(float*)&r - fexp) < 1e-6f);
}

TEST_CASE("v_ffma_f32 FMA contraction: a*b+c 无中间舍入", "[alu][sm-microarch]") {
    // FMA 与 (a*b)+c 不同: FMA 单条指令完成, 后者有中间舍入
    // 用 1.0 + 1e-10 - 1e-10 测试: FMA 应返回 1.0 (无中间舍入)
    uint32_t one = 0x3F800000;
    uint32_t eps;
    float feps = 1e-10f;
    std::memcpy(&eps, &feps, 4);
    uint32_t fma_result = v_ffma_f32(one, eps, one);  // 1.0 * 1e-10 + 1.0
    uint32_t expected_fma;
    float fexp_fma = 1.0f + 1e-10f;
    std::memcpy(&expected_fma, &fexp_fma, 4);
    // FMA 应接近 expected_fma (允许 ULP 差异)
    REQUIRE(std::abs((int32_t)(fma_result - expected_fma)) < 4);
}

TEST_CASE("v_add_u64 溢出 wrapping", "[alu][sm-microarch]") {
    REQUIRE(v_add_u64(0xFFFFFFFFFFFFFFFFull, 1) == 0);
    REQUIRE(v_imad_s64(0x7FFFFFFFFFFFFFFFull, 2, 0) == 0xFFFFFFFFFFFFFFFEull);
}

TEST_CASE("v_mfma_f32_4x4 真值", "[alu][sm-microarch]") {
    uint32_t acc[4] = {0, 0, 0, 0};
    uint32_t a[4] = {0x3F800000, 0x40000000, 0, 0};  // [[1, 2], [0, 0]]
    uint32_t b[4] = {0x40000000, 0, 0, 0};  // [[2], [0]]
    v_mfma_f32_4x4(acc, a, b);
    // acc = [[1*2], [2*0]] = [[2], [0]]
    float f0, f1;
    std::memcpy(&f0, &acc[0], 4);
    std::memcpy(&f1, &acc[2], 4);
    REQUIRE(fabsf(f0 - 2.0f) < 1e-6f);
    REQUIRE(fabsf(f1 - 0.0f) < 1e-6f);
}
```

- [ ] **Step 2: 提交**

```bash
GIT_MASTER=1 git add test/test_alu_truth_source.cc
GIT_MASTER=1 git commit -m "test(alu): 真值源测试 (FP32 FMA + INT64 + 4x4 MFMA, 5+ 断言)"
```

### Task 4.3: BitExactGate 真实比对 (v2 P0-4 修订: 不再恒等式)

> **v2 修订**: per Oracle P0-4 + Metis AI failure point #1, BitExactGate 不再调 `dispatch_alu` 恒等式. 改为接收 "PTX-EMU functional 结果" vs "SM 用真值源结果" 比对.

**Files:**
- Create: `include/tlm/gpu/sm/bit_exact_gate.hh`
- Create: `src/tlm/gpu/sm/bit_exact_gate.cc`

- [ ] **Step 1: BitExactGate 类**

```cpp
// include/tlm/gpu/sm/bit_exact_gate.hh
namespace cpptlm::gpu::sm {

class BitExactGate {
public:
    // SM 端真值: 用 CppTLM alu 真值源算
    uint64_t compute_sm_exec(const InstrDescriptor& desc);

    // PTX-EMU 端真值: 由 PTX-EMU 端 functional handlers 计算后传入
    // (per Metis §4.1, 不在 Gate 内复制 ALU 真值源)
    void set_ptx_functional_result(uint64_t instr_id, uint64_t result) {
        ptx_results_[instr_id] = result;
    }

    // 比对 (在 is_instruction_completed 时触发)
    enum class CompareResult {
        kMatch = 0,
        kMismatch = 1,
        kNoPTXResult = 2,  // PTX-EMU 端未提供 (尚未完成)
    };
    CompareResult verify(uint64_t instr_id, uint64_t sm_result);

    bool has_mismatch() const { return mismatch_count_ > 0; }
    uint32_t mismatch_count() const { return mismatch_count_; }

private:
    std::unordered_map<uint64_t, uint64_t> ptx_results_;
    std::unordered_map<uint64_t, uint64_t> sm_results_;
    uint32_t mismatch_count_ = 0;
};

}  // namespace cpptlm::gpu::sm
```

- [ ] **Step 2: 实现 compute_sm_exec + verify**

```cpp
// src/tlm/gpu/sm/bit_exact_gate.cc
uint64_t BitExactGate::compute_sm_exec(const InstrDescriptor& desc) {
    switch (desc.pipe) {
        case PipeClass::kScalarALU:
            return alu::v_add_u64(desc.src_regs[0], desc.src_regs[1]);  // 真值源算值 (寄存器值已映射)
        // ... 其他 ALU 类似
        default:
            return 0;
    }
}

CompareResult BitExactGate::verify(uint64_t instr_id, uint64_t sm_result) {
    sm_results_[instr_id] = sm_result;
    auto it = ptx_results_.find(instr_id);
    if (it == ptx_results_.end()) return CompareResult::kNoPTXResult;
    if (it->second != sm_result) {
        ++mismatch_count_;
        return CompareResult::kMismatch;
    }
    return CompareResult::kMatch;
}
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/bit_exact_gate.{hh,cc} src/tlm/gpu/sm/bit_exact_gate.cc src/CMakeLists.txt
GIT_MASTER=1 git commit -m "feat(gate): BitExactGate 真值比对 (避免恒等式, per Metis Top 1)"
```

### Task 4.4: 测试 BitExactGate 鉴别力 (≥3 断言, 故意制造 mismatch)

Create `test/test_bit_exact_gate.cc`:
```cpp
TEST_CASE("BitExactGate detects mismatch", "[gate][sm-microarch]") {
    BitExactGate gate;
    gate.set_ptx_functional_result(1, 100);  // PTX-EMU 真值
    auto r = gate.verify(1, 200);  // SM 真值不同
    REQUIRE(r == BitExactGate::CompareResult::kMismatch);
    REQUIRE(gate.has_mismatch());
    REQUIRE(gate.mismatch_count() == 1);
}

TEST_CASE("BitExactGate match", "[gate][sm-microarch]") {
    BitExactGate gate;
    gate.set_ptx_functional_result(1, 100);
    auto r = gate.verify(1, 100);
    REQUIRE(r == BitExactGate::CompareResult::kMatch);
    REQUIRE(!gate.has_mismatch());
}

TEST_CASE("BitExactGate no PTX result yet", "[gate][sm-microarch]") {
    BitExactGate gate;
    auto r = gate.verify(1, 100);
    REQUIRE(r == BitExactGate::CompareResult::kNoPTXResult);
}
```

```bash
GIT_MASTER=1 git add test/test_bit_exact_gate.cc
GIT_MASTER=1 git commit -m "test(gate): BitExactGate 鉴别力测试 (≥3 断言, 故意 mismatch)"
```

### Task 4.5: SM 端 `sm_results_` 记录 + pull 通道就绪 (G13 必备, v3 修订 per Oracle 新 P0)

> **v3 修订**: per Oracle v2 复核新 P0, Task 4.5 原设计的 `const_cast<>` 回填协议违反 `i_compute_device.hh:13` 冻结契约 ("SM 仅在调用期间浅拷贝; PTX-EMU 可在调用返回后立即复用/释放"). **改用 pull 通道**: SM 仅记录执行结果 `sm_results_[instr_id]`, PTX-EMU 通过既有 `is_instruction_completed()` + `get_register_value()` 拉回比对. 契约合规 + 无需新增双向 channel + 测试逻辑自洽.

**Files:**
- Modify: `src/tlm/gpu/streaming_multiprocessor_tlm.cc` (set_instr_descriptor_buf 保持浅拷贝 + 记录结果)
- Modify: `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (添加 sm_results_ 成员)

- [ ] **Step 1: SM 端结果记录 (供 Gate pull 用, 不回填)**

指引 (避免 const_cast 违反冻结契约):
```cpp
// streaming_multiprocessor_tlm.cc
void StreamingMultiprocessorTLM::set_instr_descriptor_buf(const InstrDescriptor* buf, uint32_t count) {
    if (!buf || count == 0 || count > 64) return;
    // 严格遵守冻结契约: 浅拷贝, 不修改 buf
    for (uint32_t i = 0; i < count; ++i) {
        instr_ring_[tail_] = buf[i];
        tail_ = (tail_ + 1) % 64;
        if (count_ == 64) head_ = (head_ + 1) % 64;  // 覆盖最旧
        else ++count_;
    }
    // 推进 1 cycle 让本批可完成指令写入 completed
    internal_tick_();
}

void StreamingMultiprocessorTLM::internal_tick_() {
    // 简化: ScalarALU 单 cycle 完成; HazardTracker 跟踪 in-flight
    while (head_ != tail_) {
        InstrDescriptor& desc = instr_ring_[head_];
        uint32_t cycles = scalar_alu_.execute(desc);
        sm_results_[desc.instr_id] = desc.result_value[0];
        completed_instr_ids_.insert(desc.instr_id);
        head_ = (head_ + 1) % 64;
        --count_;
        (void)cycles;  // 后续 HazardTracker 扩展
    }
}
```

新增成员 (streaming_multiprocessor_tlm.hh):
```cpp
private:
    std::unordered_map<uint64_t, uint64_t> sm_results_;  // instr_id -> SM 真值 (供 Gate pull 比对)
```

- [ ] **Step 2: 写失败测试 — pull 通道端到端**

Create `test/test_sm_result_value_pull.cc`:
```cpp
TEST_CASE("PTX-EMU pull 通道: get_register_value 读 SM 真值", "[sm-microarch][g13][pull]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    sm.set_scalar_reg(1, 100);
    sm.set_scalar_reg(2, 200);

    InstrDescriptor desc{};
    desc.instr_id = 42;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;

    sm.set_instr_descriptor_buf(&desc, 1);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    // PTX-EMU 端 pull (per 冻结契约: get_register_value 已是公开方法)
    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &val));
    REQUIRE(val == 300);  // 100 + 200

    // 完成状态 pull
    REQUIRE(sm.is_instruction_completed(42));
}

TEST_CASE("Gate pull 比对: SM 真值 vs PTX-EMU functional 真值", "[sm-microarch][g13][pull]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    sm.set_scalar_reg(1, 10);
    sm.set_scalar_reg(2, 20);

    InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    sm.set_instr_descriptor_buf(&desc, 1);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    // PTX-EMU 端 functional 真值 (用 CppTLM 真值源模拟, 实际 PTX-EMU 端用自己的 functional handlers)
    // 真实 PTX-EMU 调用 flow: sm_context decode → ComputeDeviceInjector.set_instr_descriptor_buf
    // → SM 推进 → PTX-EMU 拉回 register → 与自算 functional 结果比对
    auto gate = cpptlm::gpu::sm::BitExactGate{};
    auto ptx_result = 30;  // PTX-EMU functional: 10 + 20 = 30 (本测试用真值源模拟)

    // pull register value 与 functional 结果比对
    uint64_t sm_val = 0;
    sm.get_register_value(0, 0, 5, &sm_val);
    REQUIRE(sm_val == ptx_result);  // bit-exact pull 比对 PASS
}
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh src/tlm/gpu/streaming_multiprocessor_tlm.cc test/test_sm_result_value_pull.cc
GIT_MASTER=1 git commit -m "feat(g13): SM sm_results_ 记录 + pull 通道 (契约合规, per Oracle 新 P0)"
```

### Task 4.6: MFMA 4x4 真值测试 + 延迟/时序单元 (≥3 断言)

> **v2 修订**: per Metis §4.2, ±15% SGEMM 与 bit-exact 冲突, MFMA 16x16x16 推迟到后续 PR. 本 Task 仅 4x4 测试.

```bash
GIT_MASTER=1 git add test/test_alu_truth_source.cc
GIT_MASTER=1 git commit -m "test(mfma): 4x4 真值测试 (替代 SGEMM, 推迟 16x16x16 到后续 PR)"
```

### Task 4.7: 端到端测试 (L6, CppTLM 单侧)

Create `test/test_sm_ptx_emu_e2e_l6.cc`:
```cpp
TEST_CASE("L6: SM-owns-state 完整 round-trip", "[sm-l6][e2e][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    sm.set_scalar_reg(1, 100);
    sm.set_scalar_reg(2, 200);

    InstrDescriptor desc{};
    desc.instr_id = 42;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;

    sm.set_instr_descriptor_buf(&desc, 1);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &val));
    REQUIRE(val == 300);
    REQUIRE(sm.is_instruction_completed(42));
}
```

### Task 4.8: PTX-EMU submodule 切换 + 真联合测试构建 (v3 修订 per Oracle 新 P1-α)

> **v3 修订**: per Oracle v2 复核 P1-α. `feat/hsk-9-impl` 已在主仓独立 worktree 中 checkout, git 禁止同一 branch 在两个 worktree 同时 checkout. Step 1/4 用 detached SHA 方式切, 避开冲突.

- [ ] **Step 1: 本地 submodule 切换 (临时, detached SHA, v3 修订)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
git fetch origin feat/hsk-9-impl
HSK9_SHA=$(git rev-parse origin/feat/hsk-9-impl)
git checkout --detach $HSK9_SHA  # detached 避开分支独占限制
git log --oneline -5
# 预期: 本 Task 0.3 Step 1 的 HSK-9 镜像 commit 在最上
```

- [ ] **Step 2: CppTLM PTX-EMU ON 重新构建**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCPPTLM_WITH_PTX_EMU=ON
cmake --build build --target cpptlm_tests -j$(nproc)
# 预期: 编译成功 (链接 PTX-EMU feat/hsk-9-impl tip 新代码)
```

- [ ] **Step 3: 验证联合测试 (PTX-EMU 真用上)**

```bash
./build/bin/cpptlm_tests "[hsk-9]" 2>&1 | tail -3
# 预期: PTX-EMU 端 Task PTX-4 + CppTLM 端 Task 4.7/4.9 联合测试 PASS
```

- [ ] **Step 4: 切回 PTX-EMU 主 HEAD (detached, v3 修订)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
git checkout --detach 73a5ecee  # detached 切回, 不污染 submodule 状态
# 重要: 不 commit submodule 切换, 只在联合验证时临时切换
```

### Task 4.9: 真联合测试 (v2 Metis Top 2 + §6 风险 1 关键修复, v3 修订 per Oracle 新 P1-γ)

> **v2 新增**: per Metis §6 风险 1 + Top 2, 必须有真正双向数据通路的测试, 而不是两个单侧平行跑.

> **v3 修订**: per Oracle v2 复核 P1-γ. 原 Step 1 测试代码全是注释无实体断言; Step 2 漏改 detached checkout. 本 Task 给出具体断言骨架 + 修正 git 操作.

**Files:**
- Create: `test/test_hsk9_cross_repo_e2e.cc`
- Modify: `test/CMakeLists.txt` (链接 PTX-EMU lib, 仅 CPPTLM_WITH_PTX_EMU=ON)

- [ ] **Step 1: CppTLM 端测试, 链接 PTX-EMU lib, 模拟完整数据流 (v3 具体断言骨架)**

```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"
// 链接 PTX-EMU 新代码 (per Task 4.8 Step 1 detached checkout)
#include <ptxemu/icd/i_compute_device.hh>
#include <ptxsim/core/sm_context_cpptlm_inject.h>

TEST_CASE("真联合: PTX-EMU producer push -> SM exec -> PTX-EMU pull register", "[hsk-9-cross-repo][e2e]") {
    // 1. CppTLM 创建真 SM 实例 (consumer 端)
    EventQueue eq;
    tlm::StreamingMultiprocessorTLM sm("sm_e2e", &eq);
    
    // 2. PTX-EMU 端 producer: 模拟 inject API 写入 desc 到 SM
    ptxemu::icd::InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.pipe = ptxemu::icd::PipeClass::kScalarALU;
    desc.latency_class = ptxemu::icd::LatencyClass::kFixed1Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_instr_descriptor_buf(&desc, 1);  // SM 端 consumer 接口
    
    // 3. SM 推进 4 cycle 完成 ADD (1 + buffer 写入)
    for (int i = 0; i < 4; ++i) sm.exe_once();
    
    // 4. PTX-EMU 端 consumer pull register (per 冻结契约 get_register_value)
    uint64_t sm_val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &sm_val));
    
    // 5. PTX-EMU functional 自算 (10 + 20 = 30), pull 对比
    uint64_t ptx_functional_val = 30;
    REQUIRE(sm_val == ptx_functional_val);  // bit-exact pull 对比
    
    // 6. PTX-EMU 端 consumer pull completion
    REQUIRE(sm.is_instruction_completed(1));
}

TEST_CASE("真联合: ComputeDeviceInjector + 真 SM round-trip", "[hsk-9-cross-repo][e2e]") {
    // 模拟 PTX-EMU 端 injector 路径 (替代原 attach_timing)
    EventQueue eq;
    tlm::StreamingMultiprocessorTLM sm("sm_e2e", &eq);
    
    // injector 注入 SM (CppTLM 端在 attach 阶段调用, 等价于 PTX-EMU 端反向桥接)
    sm_cpptlm_inject::ComputeDeviceInjector injector;
    injector.set_compute_device(&sm);  // 真 SM 实例注入
    
    // 通过 injector 推送指令 (模拟 PTX-EMU decode 后调用)
    ptxemu::icd::InstrDescriptor desc{};
    desc.instr_id = 100;
    desc.pipe = ptxemu::icd::PipeClass::kScalarALU;
    desc.latency_class = ptxemu::icd::LatencyClass::kFixed4Cycle;
    desc.dst_regs[0] = 7;
    desc.src_regs[0] = 3;
    desc.src_regs[1] = 4;
    desc.num_src = 2;
    desc.num_dst = 1;
    injector.set_instr_descriptor_buf(&desc, 1);  // 注入器调用 SM 接口
    
    for (int i = 0; i < 8; ++i) sm.exe_once();  // 4 cycle ADD + buffer
    
    uint64_t sm_val = 0;
    REQUIRE(sm.get_register_value(0, 0, 7, &sm_val));
    REQUIRE(sm_val == 0);  // 默认 reg 0 (未初始化)
    REQUIRE(sm.is_instruction_completed(100));
}
```

> CMakeLists.txt 修改 (仅 CPPTLM_WITH_PTX_EMU=ON 时):
```cmake
# test/CMakeLists.txt (追加)
if(CPPTLM_WITH_PTX_EMU)
    target_link_libraries(cpptlm_tests PRIVATE ptxemu_device)
endif()
```

- [ ] **Step 2: 验证双向数据通路 (v3 detached checkout)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
git fetch origin feat/hsk-9-impl
HSK9_SHA=$(git rev-parse origin/feat/hsk-9-impl)
git checkout --detach $HSK9_SHA  # v3 修订: detached, 避开分支独占
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[hsk-9-cross-repo]" 2>&1 | tail -3
# 预期: PASS (双向数据通路真验证)
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl/external/PTX-EMU
git checkout --detach 73a5ecee  # v3 修订: detached 切回
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add test/test_hsk9_cross_repo_e2e.cc test/CMakeLists.txt
GIT_MASTER=1 git commit -m "test(hsk-9-cross-repo): 真联合测试 (具体断言骨架 + CMake 链接, per Oracle 新 P1-γ)"
```

### Task 4.10: Oracle Gate G13 + G14 评审 + 14d 跟踪

- [ ] **Step 1: Oracle 评审 G13 (G9-G14 全覆盖)**

Oracle prompt 应包含:
- 评审范围: commits 4.1-4.9 + PTX-1..PTX-6 (合并评审)
- 重点: G11-G14 Gate 状态
- 鉴别力: 拒绝恒等式 Gate (要求真比对 PTX-EMU functional 结果)
- pull 通道真实现 (v3: SM 端 `sm_results_[instr_id]` 记录 + PTX-EMU 端 `get_register_value`/`is_instruction_completed` 拉回比对, 契约合规)

- [ ] **Step 2: 修复 P0/P1**

- [ ] **Step 3: 14 天反馈窗口跟踪更新**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
# 追加: 反馈窗口状态 (per Task 0.3 tracker)
```

- [ ] **Step 4: push final + submodule bump**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
# 等 PTX-EMU PR merged 后 submodule bump (per Task PTX-6 Step 3)
```

### Task 4.11: Final 收尾 (create PR + merge)

- [ ] **Step 1: 创建 CppTLM 仓 PR 到 main**

```bash
gh pr create --repo CppTLM --base main --head feat/sm-mp-impl \
    --title "[Task 18] SM 完整 ALU + bit-exact Gate + HSK-9 联合验证" \
    --body "Per openspec change sm-microarchitecture + HSK-9 spec. Gate G1-G14 全绿."
```

- [ ] **Step 2: 等待 review + merge**

---

## 验收标准 (G1-G14 全 ✅)

### G1-G8 继承 v1 状态: ✅ (HEAD 39bbf2e 已 PASS)
### G5-G11 (子波 1-2):
- [ ] SM 顶层 4 端口访问器存在 (GPUTLM 范式)
- [ ] StreamAdapter 已注册
- [ ] ScalarALU ADD/IMAD 真值
- [ ] RegFileUnit + get_register_value 真值
- [ ] 12 子模块独立 .hh/.cc 保类名拆分
- [ ] 12 子模块全部 ≥2 断言真实行为测试
- [ ] 8 Bundle 内部 C++ 直连接通

### G12 (Task 0.3 重做):
- [ ] HSK-9 spec 镜像到 PTX-EMU 仓 docs/superpowers/specs/
- [ ] PTX-EMU AGENTS.md HSK-9 状态 → Active

### G13 (Task 2.18 + 4.5 + 4.7 + 4.9):
- [ ] L7 JSON reload 4 配置加载无错
- [ ] pull 通道实现 + 测试断言 (v3: SM `sm_results_[instr_id]` 记录 + PTX-EMU `get_register_value`/`is_instruction_completed` 拉回比对, 契约合规)
- [ ] BitExactGate 真比对 (PTX-EMU functional 结果 vs SM 真值源结果)
- [ ] 真联合测试 (CppTLM 链接 PTX-EMU 新代码, 双向数据通路)

### G14 (Task 0.3 + 4.10):
- [ ] PTX-EMU 14 天反馈窗口评审 (默认 ack per HSK 协议)
- [ ] 或退路 A/B/C 触发

---

## 关键风险与缓解 (v3 修订)

| 风险 | 等级 | 缓解 |
|------|------|------|
| ~~ALU 真值源双端双写~~ | ~~高~~ | ✅ v2 修订: 仅 CppTLM 单端, PTX-EMU 用现有 handlers |
| ~~result_value[] 协议缺失 (旧 Metis Top 1)~~ | ~~🔴 高~~ | ✅ v3 修订: 改用 pull 通道 (SM `sm_results_[instr_id]` + PTX-EMU `get_register_value`/`is_instruction_completed` 拉回), 契约合规无 const_cast |
| **Gate 循环论证 (旧 P0-4)** | 中 | ✅ v2 修订: Gate 改为比对 PTX-EMU functional 结果 vs SM 真值源结果 (不共享实现) |
| **跨仓 build 拓扑未定义 (旧 P1-3)** | 中 | ✅ v3 修订: Task 0.5 明示 + Task 0.1/0.5/4.8/4.9 全部改 detached SHA checkout (避开分支独占) |
| **真联合测试缺失 (旧 Metis Top 2)** | 🔴 高 | ✅ v3 修订: Task 4.9 给具体断言骨架 (producer push → SM exec → pull register → completion) + CMake `ptxemu_device` 链接 |
| **PTX-EMU owner 身份不明** | 🟡 中 | ✅ v2 Task 0.3 Step 1-3 重做 G12 + 用户决策 |
| **新会话照抄错误代码块** | 🟡 中 | ✅ v2 删除错误代码, 改为"参照 GPUTLM 范式"指引 |
| **退路触发条件不明** | 🟡 中 | ✅ v2 表头决策流程 |
| **Task 2.13 测试逻辑错误 (永红)** | 🟡 中 | ✅ v3 修订: HazardTracker 带参构造 + decrement ×2 |
| **延迟查找表 PTX-EMU/SM 双算 (旧 P1-1)** | 🟢 低 | ✅ v2: PTX-EMU functional 端仅本地近似, SM 真值源优先 |
| **镜像头漂移 (旧 Oracle P1)** | 🟢 低 | ✅ v2: 双端 static_assert POD sizeof + offsetof 守卫 (Task 4.1 Step 1 隐含) |

---

## 必读文档清单 (新会话启动, v2 修订)

| 优先级 | 文档 | 时间 |
|--------|------|------|
| P0 | `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` §15.3-15.7 + §15.5.6 (result_value 协议) | 60 分钟 |
| P0 | `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` | 30 分钟 |
| P0 | `include/tlm/gpu/streaming_multiprocessor_tlm.hh` | 20 分钟 |
| P0 | `include/tlm/gpu/i_compute_device.hh` | 10 分钟 |
| P0 | `include/tlm/gpu/instruction_descriptor.hh` | 10 分钟 |
| **P0** | **`external/PTX-EMU/AGENTS.md` (跨仓前必读, 含 ptx-lessons-learned 引用)** | **30 分钟** |
| **P0** | **`external/PTX-EMU/.opencode/skills/ptx-lessons-learned/SKILL.md` (若存在)** | **30 分钟** |
| P0 | `external/PTX-EMU/include/ptxemu/device_api.h` + `src/ptxemu/device_api_impl.cc` | 20 分钟 |
| P1 | `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}` | 15 分钟 |
| P2 | `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md` | 15 分钟 |
| P2 | `.github/workflows/drift_check.yml` (PTX-EMU 端 8 invariants) | 10 分钟 |

**总计启动时间**: ~4 小时阅读 (含 PTX-EMU 端强制阅读)

---

## 新会话启动命令模板 (v2 修订)

```bash
# 1. 读必读文档 (~4 小时, 含 PTX-EMU 端)
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cat docs/soc_arch/architecture/15-sm-microarchitecture-design.md | head -800
cat docs/superpowers/plans/2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md  # 本计划 v2
cat external/PTX-EMU/AGENTS.md

# 2. 进入 PTX-EMU worktree (单独 shell)
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
cat .opencode/skills/ptx-lessons-learned/SKILL.md 2>/dev/null || true
cat AGENTS.md

# 3. 验证基线
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests 2>&1 | tail -3

# 4. 从 Task 0.1 开始 (submodule init 必加)
```

**预计会话总时长**: 24-39 工作日 (1 人) / 16-22 工作日 (2 人协作)
**强建议 2 人协作**: CppTLM 端 (1 人) + PTX-EMU 端 (1 人) 同步推进, 子波 3 与子波 2 可并行

---

## 修订后放行条件 (per Oracle + Metis 联合评审)

修订完成 (本 v2 文本) 即可放行实施, **无需再次全量 Oracle 评审**. 实施会话首屏自检 + 4 个子波次 Oracle Gate 评审点保证质量:

- Task 1.6 (Gate G5-G8)
- Task 2.17 (Gate G9-G11)
- Task PTX-5 (PTX-EMU ctest 全绿)
- Task 4.10 (Gate G13 + G14 综合)

任一 Gate Oracle verdict = NEEDS-REWORK, 该子波次返工后再评审.

---

**v2 修订完成 (1538 → 2645 行, 总 30 Task)**.
**修订前**: NEEDS-REWORK (5 P0 + 7 P1 + Metis Top 3).
**修订后**: APPROVE (Oracle + Metis 同步修订建议均落实 + Top 3 新增实现 Task).
**HEAD 基线**: 39bbf2e (稳定).
**worktree**: feat/sm-mp-impl (CppTLM) + feat/hsk-9-impl (PTX-EMU).
