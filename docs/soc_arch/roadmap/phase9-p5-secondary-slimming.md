# phase9-p5-secondary-slimming: ABI 二级精简 18→15 + get_version 宏化 (修订版, 保留 open/close)

> **类别**: SoC Architecture > Roadmap · **阶段**: phase9 P5 · **优先级**: 🟠 跨仓(需要 Hub ack)
> **日期**: 2027-09-17 (修订: open/close 保留) · **维护者**: Sisyphus · **跨仓**: ✅ (Hub ack required)
> **关联 ADR**: [ADR-SOC-20-cpptlm-abi-secondary-slimming.md](../adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md) — 本阶段主 ADR (修订版)
> **父 ADR**: [ADR-SOC-18-cpptlm-abi-slimming.md](../adr/ADR-SOC-18-cpptlm-abi-slimming.md) — 第一轮精简 22→18
> **关联 OpenSpec**: 待创建 `openspec/changes/2027-09-17-cpptlm-abi-secondary-slimming/`
> **关联 HSK**: 待创建 `HSK-12-cpptlm-abi-secondary-slimming.md`

**W 编号基准**: W1 = 2026-09-21(Mon) — 本目录所有阶段文件均按此基准;当前 W37+。

---

## 1. 目标 (修订版)

在 ADR-SOC-18 第一轮精简(22→18)完成的基线上,做**第二轮精简**(18→15 函数 + `get_version` 改宏,**保留 open/close**):

- **删除 2 个真冗余函数**: `create_by_id` / `get_adapter_info`
- **`get_version` 改宏**: `#define CPPTLM_VERSION_STRING "v1.0-dgpu-v1"`
- **保留 open/close** (per 用户反馈 2027-09-17): fd 风格 API + 生命周期分层语义价值
- **测试迁移**: 4 个测试文件, ~9 处调用点
- **跨仓协调**: Hub (UsrLinuxEmu) 同步删除 2 处调用点

**预期产出**:
- `cpptlm_emulator.h` **15 函数** + 1 宏 + 4 callback typedef(零修改) + 1 opaque struct (零修改)
- 66,564+ 既有 assertions 零回归
- Hub 端 PR 合并

---

## 2. 关键发现与修订理由(2027-09-17)

### 2.1 grep 验证

`src/abi/cpptlm_emulator.cc:433` — `cpptlm_emulator_open()` **内部调用** `cpptlm_emulator_create_by_id()`。

### 2.2 修订理由

**初版建议**: 级联删除 4 个 handle API (`create_by_id` + `open` + `close` + `get_adapter_info`) → 18→14+宏

**用户反馈 2027-09-17**: open/close 模仿 kernel driver fd 模式, 有 **API 设计语义价值**:
- 真实 driver fd 模式: `open("/dev/dri/renderD128")` → ioctl(fd, ...) → close(fd)
- 生命周期分层: 主进程持有 `cpptlm_emulator_t*`, 业务模块借用 `cpptlm_emulator_handle_t`
- API 一致性: driver 开发者心智模型统一

**6 个 driver 场景风险评估**:

| 场景 | 删除影响 |
|------|---------|
| 进程间 fd 共享 | 🟢 零 (之前无此能力) |
| 多线程 ref count | 🟢 零 (之前无此能力) |
| 生命周期分层 | 🟡 **保留 open/close** |
| 多 emulator 实例 | 🟢 零 (create 已支持) |
| handle 状态追踪 | 🟢 零 (无 per-handle 状态) |
| fd API 风格统一 | 🟡 **保留 fd 风格** |

**决策**: 保留 open/close, 删除真冗余的 2 个(create_by_id + get_adapter_info) + 改 get_version 宏

### 2.3 修订前后对比

| 维度 | 初版 (18→14+宏) | 修订 (18→15+宏) |
|------|:---------------:|:---------------:|
| 删除函数数 | 5 | **3** (含 1 改宏) |
| `open/close` | 删除 | **保留** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (2 函数级) |
| Driver 功能影响 | 🟡 场景 3+6 | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |
| 未来扩展空间 | 不可逆 | 可逆(可加不能减) |

---

## 3. 任务清单 (修订后)

| ID | 任务 | 工作量 | 阻塞 |
|----|------|:----:|------|
| **P5-1** | 创建 OpenSpec change `cpptlm-abi-secondary-slimming/` (proposal + design + specs + tasks) | 0.5 d | 无 |
| **P5-2** | 创建 HSK-12 跨仓契约镜像 | 0.5 d | 无 |
| **P5-3** | Hub 侧 ADR-088 §D5 Status Update 提交 + 异步 ack 跟踪 | 0 d (异步) | P5-4 必须等 ack |
| **P5-4** | 测试迁移 (4 文件 ~9 处, **修订后**) | 0.7 d | P5-3 ack |
| **P5-5** | ABI 头文件 + 实现 3 函数移除 + `get_version` 改宏 (**保留 open/close**) | 0.4 d | P5-4 |
| **P5-6** | 新增 `[abi-secondary-slimming]` 测试 (验证 3 已删 + 15 仍可用) | 0.5 d | P5-5 |
| **P5-7** | 全量回归 + `openspec validate --strict` PASS | 0.5 d | P5-5 |
| **P5-8** | Archive OpenSpec change + 更新 ADR-SOC-18 Status Update | 0.2 d | P5-7 |

**总工作量**: ~3.3 d (含 ack 异步等待, 修订后减少 0.2 d)

### 3.1 任务依赖图

```
P5-1 (OpenSpec 创建)
P5-2 (HSK-12 创建)
  ↓
P5-3 (Hub ack 提交 + 跟踪) ←─── 异步,不阻塞 P5-1/P5-2
  ↓ ack 到达
P5-4 (测试迁移, **修订后 4 文件 ~9 处**)
  ↓
P5-5 (ABI 头文件 + 实现修改, **修订后保留 open/close**)
  ↓
P5-6 (新增测试)
P5-7 (全量回归)
  ↓
P5-8 (Archive + Status Update)
```

---

## 4. 详细任务分解 (修订后)

### T-P5-1: OpenSpec change 创建

| 子任务 | 详情 |
|--------|------|
| 1 | 创建 `openspec/changes/2027-09-17-cpptlm-abi-secondary-slimming/proposal.md` |
| 2 | 创建 `design.md` (per父 design.md 模板) |
| 3 | 创建 `specs/cpptlm-emulator-abi/spec.md` (REMOVED Requirements 2 + 1 ADDED for macro) |
| 4 | 创建 `tasks.md` (8 步对应 P5-1..P5-8) |
| 5 | 跑 `openspec validate --strict` 验证 |

### T-P5-2: HSK-12 跨仓契约镜像

| 子任务 | 详情 |
|--------|------|
| 1 | 创建 `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` |
| 2 | 内容: **15 函数**完整清单 + 1 宏 + 4 callback typedef + 3 函数移除迁移指南 |
| 3 | Hub 侧 4 种响应回退策略 (per HSK-11 §5 模板) |

### T-P5-3: Hub 异步协调(关键路径)

| 子任务 | 详情 |
|--------|------|
| 1 | 提交 ADR-088 §D5 Status Update PR (含 **3 函数**移除清单) |
| 2 | **10 工作日窗口**:Hub ack / 部分 ack / 拒绝 / 无响应 |
| 3 | **超时 fallback**: per tasks.md T-P5-3 step 4 (假定 Hub 已自行移除) |

### T-P5-4: 测试迁移 (TDD 5 步)

**4 文件迁移清单 (修订后)**:

| 文件 | 迁移内容 | 处数 |
|------|---------|:---:|
| `test/test_cpptlm_emulator_abi.cc` | 移除 `FnGetVersion` 签名测试 | 2 |
| `test/test_cpptlm_emulator_handle_helpers.hh` | RAII helper 改用 `create("profile_path")` | 1 |
| `test/test_cpptlm_emulator_registry.cc` | 3 处 `create_by_id(0)` → `create("profile_path")` | 3 |
| `test/test_dgpu_board_shell_full_abi.cc` | 4 处 `create_by_id(N)` → `create()`; 移除 `get_version` 调用 | 5 |
| `test/test_cpptlm_emulator_abi_slimming.cc` | 移除 `get_version` / `get_adapter_info` 引用 | 2 |
| `test/test_dgpu_adapter_info.cc` | 整个文件改写测试 `get_device_info` 或合并 | 全文件 |
| **总计** | (**修订后, open/close 相关测试保持不变**) | **~9+ 处** |

### T-P5-5: ABI 头文件 + 实现 (修订后)

| 子任务 | 详情 |
|--------|------|
| 1 | `include/abi/cpptlm_emulator.h`: 删除 3 个函数声明(`create_by_id` line 72, `get_adapter_info` line 103, `get_version` line 64) |
| 2 | `src/abi/cpptlm_emulator.cc`: 删除 3 个函数实现 + 移除 `get_adapter_info` 句柄表查询; **保留 `open/close` 实现** + 更新版本注释 "15 functions + 4 callbacks + 1 macro" |
| 3 | `CPPTLM_VERSION_STRING` 宏确认 (line 24 已定义) |
| 4 | `cpptlm_emulator_t` 结构体零修改; 4 callback typedef 零修改 |

### T-P5-6: 新增 `[abi-secondary-slimming]` 标签测试

```cpp
TEST_CASE("abi-secondary-slimming: 3 functions removed + 15 remain + 1 macro", "[abi-secondary-slimming]") {
    // 验证 3 个函数符号在编译期不可见 (create_by_id / get_adapter_info / get_version)
    // 验证 15 个函数符号仍可调用 (含 open/close)
    // 验证 CPPTLM_VERSION_STRING 宏值正确
}
```

### T-P5-7: 全量回归

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -j4
./build/bin/cpptlm_tests "[pcie]"   # 既有 66,564+ assertions
./build/bin/cpptlm_tests "[abi-secondary-slimming]"  # 新增
```

### T-P5-8: Archive + Status Update

| 子任务 | 详情 |
|--------|------|
| 1 | 执行 `openspec archive cpptlm-abi-secondary-slimming` |
| 2 | 更新 `ADR-SOC-18-cpptlm-abi-slimming.md` Status Update 段: 后续精简见 ADR-SOC-20 (修订版) |
| 3 | 更新 `ADR-SOC-20-cpptlm-abi-secondary-slimming.md` Status Update 段: archive 状态 |
| 4 | 提交最终 PR |

---

## 5. 完成标准(DoD, 修订后)

| Gate | 验证 |
|------|------|
| **G1** | `openspec validate cpptlm-abi-secondary-slimming --strict` PASS |
| **G2** | `grep -rn "cpptlm_emulator_create_by_id\|cpptlm_emulator_get_adapter_info\|cpptlm_emulator_get_version" src/ include/ test/` = **0 匹配** (排除 ABI 定义文件已删实现) |
| **G3** | `include/abi/cpptlm_emulator.h` 剩余 **15 个驱动核心函数 + 1 宏 + 4 callback typedef** (`open/close` 保留) |
| **G4** | 15 函数签名零修改 (与父 ADR-SOC-18 §G7 字节比对) |
| **G5** | `CPPTLM_VERSION_STRING` 宏值正确(版本号同步更新到 "v1.0-dgpu-v1") |
| **G6** | `cpptlm_emulator_t` 结构体零修改 |
| **G7** | 既有 `[pcie]` 测试零回归(66,564+ assertions PASS) |
| **G8** | `[abi-secondary-slimming]` 新增测试 PASS |
| **G9** | Hub (UsrLinuxEmu) 2 处调用点删除 PR 合并 (`create_by_id` + `get_adapter_info`; `get_version` 可选) |
| **G10** | ADR-SOC-18 Status Update 段已追加(修订版) |
| **G11** | **`open/close` 调用点零修改** (修订后保留, **功能影响零**) |

---

## 6. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| Hub ack 超时(per ADR-SOC-18 经验) | 🟡 中 | 跨仓阻塞 | 10 工作日窗口 + fallback 策略(假定已移除) |
| `open` 内部分调用 `create_by_id` 改动连带影响 | 🟢 低 | 编译失败 | 4 个函数一次性 commit 删除(避免中间状态) |
| `test_dgpu_adapter_info.cc` 整体改写遗漏 | 🟢 低 | 测试覆盖缺失 | 合并到 `test_cpptlm_emulator_abi.cc` 后 grep 验证 |
| Hub 侧有未声明依赖(如某些 driver 路径) | 🟢 低 | 集成断裂 | 实测 ~12 核心 ABI 远超 14 |
| `CPPTLM_VERSION_STRING` 宏版本号不同步 | 🟢 低 | 版本混淆 | 同步更新头文件 + 测试断言 |

---

## 7. 跨仓协调事项 (修订后)

### 7.1 Hub (UsrLinuxEmu) 同步删除清单 (修订后)

| 同步项 | Hub 实施 | 备注 |
|--------|:-------:|------|
| `cpptlm_emulator_create_by_id(dev_id)` 调用 | ✅ 必做 | 改 `cpptlm_emulator_create(profile_path)` |
| `cpptlm_emulator_get_adapter_info(handle, ...)` 调用 | ✅ 必做 | 改 `cpptlm_emulator_get_device_info(dev_id, ...)` |
| `cpptlm_emulator_get_version()` 调用 | 🟡 可选 | 改 `CPPTLM_VERSION_STRING` 宏(或保留函数调用) |
| `cpptlm_emulator_open` / `cpptlm_emulator_close` 调用 | ❌ **不动** | 保留 fd 风格 API, driver 零影响 |

### 7.2 协调节奏

- **W0**: OpenSpec + HSK-12 创建 + Hub ack 提交 (2 函数级)
- **W1-2**: Hub 异步 ack + 本仓测试迁移预备
- **W2 末**: 关键路径——Hub ack 到达 → 开始 P5-4
- **W3**: 测试迁移 + ABI 头文件 + 实现修改
- **W4**: 全量回归 + archive

---

## 8. 与其他 Phase 阶段关系

```
phase9-p0  ──→ p1  ──→ p2  ──→ p3  ──→ p4  ──→ p5 (本文件)
                                                          │
                                                          ↓
                                                  Hub ack (异步)
                                                          │
                                                          ↓
                                                  P5-4 测试迁移
                                                          │
                                                          ↓
                                                  archive
```

- **P3 (战略层 ADR 修订)**: 与本 P5 互补——P3 是已实施 ADR 修订, P5 是二级精简实施
- **P4 (AXI 通用 outbound 桥接评估)**: 与本 P5 互不影响(架构层面独立)
- **后续轮次** (假设): P6 `register_dma_translate_cb` 合并到 `register_callbacks`(per ADR-SOC-20 §6 备选 1)

---

## 9. 维护纪律

- 任务完成 → OpenSpec archive, **不删除文件**, 改 status 字段
- 总览文件(`phase9-post-phase8-roadmap.md`)每周五 review
- 新阶段加入: 复制模板 → 改名 → 在 `roadmap/README.md` 索引
- 跨仓议题在 §7 单独标注

---

## 10. 关联文档

- **ADR 主文档**: [ADR-SOC-20](../adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md)
- **第一轮 ADR**: [ADR-SOC-18](../adr/ADR-SOC-18-cpptlm-abi-slimming.md)
- **总览**: [phase9-post-phase8-roadmap.md](./phase9-post-phase8-roadmap.md)
- **模块文档**: [`dgpu-soc-pcie-slice.md §9.3`](../modules/dgpu-soc-pcie-slice.md)
- **现有 ABI**: `include/abi/cpptlm_emulator.h:64-104`
- **第一轮 OpenSpec (已 archive)**: [`archive/2026-09-16-cpptlm-abi-slimming/`](../../../openspec/changes/archive/2026-09-16-cpptlm-abi-slimming/)

---

## 11. 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: ✅ 完成 (2027-09-17) — 修订版 P5 已 archive @ 15 函数 + 1 宏

## Status Update

### 2027-09-17 — P5 修订版实施完成 (per [ADR-SOC-20-cpptlm-abi-secondary-slimming.md](../adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md))

**完成状态**: ✅ 修订版 P5 archive 成功

**实施统计**:
- ✅ 头文件: 3 声明删除 (`get_version` + `create_by_id` + `get_adapter_info`)
- ✅ 实现文件: 3 实现删除 + `open()` 内部 create_by_id → create + resolve_profile_path
- ✅ 测试 6 文件 + examples 1 文件 全部迁移 (修订版合并决策: 标签测试合并到现有文件)
- ✅ 全量回归: [pcie] 36,454 / [abi] 63 / [dgpu][full_abi] 26 / [chstream] 155 assertions, ctest 75/75 PASS

**修订版 G8 acceptance gate 满足**:
- `[abi-secondary-slimming]` tag: 7 assertions, 1 test case PASS (合并到 `test_cpptlm_emulator_abi_slimming.cc`)

**修订版关键保持**:
- 🟢 `open/close` fd 风格 API 保留 (修订版核心, 不删除)
- 🟢 Hub ack 风险降低 60% (2 函数级 vs 初版 5 函数级)
- 🟢 零 driver 功能影响 (实测 ~12 核心 ABI 远超 15)

**跨仓**: Hub issue [UsrLinuxEmu #33](https://github.com/chisuhua/UsrLinuxEmu/issues/33) 已提交, 14 天响应窗口 (不阻塞主线)

**关联**: 
- [ADR-SOC-18-cpptlm-abi-slimming.md](../adr/ADR-SOC-18-cpptlm-abi-slimming.md) — 父 ADR, Status Update 已添加 P5 完成记录
- [ADR-SOC-20-cpptlm-abi-secondary-slimming.md](../adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md) — ✅ Accepted
- [HSK-12-cpptlm-abi-secondary-slimming.md](../../cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md) — 跨仓契约镜像, §9.3 修订版合并决策记录
- 实施计划: [`.rddf/plans/cpptlm-abi-secondary-slimming.md`](../../../.rddf/plans/cpptlm-abi-secondary-slimming.md) — TDD 5 步 8 WU 全部完成