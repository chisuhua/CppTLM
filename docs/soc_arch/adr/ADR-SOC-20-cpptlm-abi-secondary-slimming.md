# ADR-SOC-20: CppTLM ABI 二级精简 (18 → 14 函数 + 宏化 get_version)

# ADR-SOC-20: CppTLM ABI 二级精简 (18 → 15 函数 + 宏化 get_version)

> **状态**: 📋 Proposed (修订自 2027-09-17 用户反馈)
> **日期**: 2027-09-17
> **影响**: ABI 函数从 18 精简到 15, `get_version` 改 `#define` 宏; **保留** `open/close`(语义价值); Hub (UsrLinuxEmu) 需要同步删除 3 个调用点
> **类别**: SoC 架构 / ABI 表面治理 / 跨仓协调
> **拆分自**: [ADR-SOC-18-cpptlm-abi-slimming.md](./ADR-SOC-18-cpptlm-abi-slimming.md) (Phase 9+ 第一轮精简 22→18)
> **关联 ADR**:
> - [`ADR-SOC-18-cpptlm-abi-slimming.md`](./ADR-SOC-18-cpptlm-abi-slimming.md) — **第一轮精简**(22→18)
> - [`ADR-SOC-17-pcie-mock-ip.md`](./ADR-SOC-17-pcie-mock-ip.md) — **Mock IP 边界**(backdoor 行为通过 mock 路径实现)
> - [`ADR-SOC-19-axi-master-outbound-bridge.md`](./ADR-SOC-19-axi-master-outbound-bridge.md) — **AXI Master/Slave 边界**(Phase 9+ 评估缺口)
> **关联 OpenSpec**: 待创建 `openspec/changes/2027-09-17-cpptlm-abi-secondary-slimming/`
> **关联 HSK**: 待创建 `HSK-12-cpptlm-abi-secondary-slimming.md`(取代 HSK-11 §部分)
> **关联 Roadmap**: [`phase9-p5-secondary-slimming.md`](../roadmap/phase9-p5-secondary-slimming.md) — P5 实施阶段

---

## 1. 背景

### 1.1 第一轮精简后状态 (per ADR-SOC-18)

Phase 9+ 第一轮 ABI 精简 (`2026-09-16-cpptlm-abi-slimming`) 已完成, ABI 从 **22 函数精简到 18 函数**(删除 4 个 backdoor/lookup 函数)。Hub (UsrLinuxEmu) ack 后实际行为验证通过——驱动仅依赖 ~12 核心 ABI。

**18 个函数表** (per `include/abi/cpptlm_emulator.h:64-104`):

| # | 函数 | 调用频率 | 设计用途 |
|---|------|---------|---------|
| 1 | `cpptlm_emulator_get_version` | 启动 1 次 | 返回版本字符串 |
| 2 | `cpptlm_emulator_get_device_count` | 启动 1 次 | 设备枚举 |
| 3 | `cpptlm_emulator_get_device_info` | 启动 N 次 | 设备元数据 |
| 4 | `cpptlm_emulator_create` | 启动 1 次 | 创建 emulator (按 profile_path) |
| 5 | `cpptlm_emulator_create_by_id` | 启动 1 次 | 创建 emulator (按 dev_id) |
| 6 | `cpptlm_emulator_destroy` | 关闭 1 次 | 销毁 emulator |
| 7 | `cpptlm_emulator_mmio_write` | 频繁 | MMIO 写 |
| 8 | `cpptlm_emulator_mmio_read` | 频繁 | MMIO 读 |
| 9 | `cpptlm_emulator_pcie_config_write` | 中频 | PCIe Config 写 |
| 10 | `cpptlm_emulator_pcie_config_read` | 中频 | PCIe Config 读 |
| 11 | `cpptlm_emulator_msix_init` | 启动 1 次 | MSI-X 初始化 |
| 12 | `cpptlm_emulator_msix_update_pending` | 频繁 | MSI-X 触发 |
| 13 | `cpptlm_emulator_msix_clear_pending` | 中频 | MSI-X 清除 |
| 14 | `cpptlm_emulator_register_callbacks` | 启动 1 次 | 注册 4 类回调 |
| 15 | `cpptlm_emulator_register_dma_translate_cb` | 启动 1 次 | 注册 DMA 翻译回调 |
| 16 | `cpptlm_emulator_open` | 启动 1 次 | 打开句柄 (返回 handle) |
| 17 | `cpptlm_emulator_close` | 关闭 1 次 | 关闭句柄 |
| 18 | `cpptlm_emulator_get_adapter_info` | 启动 1 次 | 通过 handle 查询设备信息 |

### 1.2 进一步精简空间分析 (用户调研 2027-09-17)

通过 grep 验证每个函数的实际调用方 + 内部耦合关系,发现 **3 个候选精简模式**:

#### 候选 A: `cpptlm_emulator_get_version` → 宏

- 当前: 函数调用返回 `const char*`
- 替代: `#define CPPTLM_VERSION_STRING "v1.0-dgpu-v1"` (该宏已存在于 line 24, 仅被函数覆盖)
- 影响: **零跨仓风险**(无函数签名移除, 改为宏常量)
- 实施成本: < 0.1 人天

#### 候选 B: `cpptlm_emulator_create_by_id` + `cpptlm_emulator_get_adapter_info` 删除

**关键发现 (per `src/abi/cpptlm_emulator.cc:433`)**: `cpptlm_emulator_open()` 内部调用 `cpptlm_emulator_create_by_id(dev_id)`。

**原计划**: 级联删除 4 个 handle API (open / create_by_id / close / get_adapter_info)

**修订理由 (per 用户反馈 2027-09-17)**: `open/close` 模仿 fd 模式, 有 **API 设计语义价值**:
- 真实 driver fd 模式: `open("/dev/dri/renderD128")` → ioctl(fd, ...) → close(fd)
- CppTLM 模拟器分层: `create`(设备实体) / `open`(会话句柄) / `close`(关闭会话) / `destroy`(销毁设备)
- **生命周期分层语义**: 主进程持有 `cpptlm_emulator_t*`, 业务模块借用 `cpptlm_emulator_handle_t`
- **API 设计一致性**: 与 kernel driver fd 风格匹配, driver 开发者心智模型统一

**风险评估** (per 6 个 driver 场景):
- 进程间 fd 共享: ❌ 之前无此能力,删除后仍无
- 多线程 ref count: ❌ 之前无此能力,删除后仍无
- 生命周期分层: ✅ 有合法价值, **保留 open/close**
- 多 emulator 实例: ✅ 通过 create 支持,不受影响
- handle 状态追踪: ❌ 之前无 per-handle 状态
- fd API 风格统一: ✅ 有 API 设计价值, **保留 open/close**

**结论**: **保留 open/close**,仅删除真正冗余的 2 个:

| 函数 | 决策 | 理由 |
|------|------|------|
| `cpptlm_emulator_create_by_id` (#5) | ❌ 删除 | 与 `cpptlm_emulator_create` (按 profile_path) 重叠;驱动可由 pkg-config 获得 profile_path |
| `cpptlm_emulator_open` (#16) | ✅ **保留** | fd 风格 API + 生命周期分层语义价值 |
| `cpptlm_emulator_close` (#17) | ✅ **保留** | fd 风格 API 配对 |
| `cpptlm_emulator_get_adapter_info` (#18) | ❌ 删除 | 与 `cpptlm_emulator_get_device_info` (#3) 重叠(后者按 dev_id, 无句柄依赖) |

#### 候选 C: `cpptlm_emulator_register_dma_translate_cb` → 合并到 `register_callbacks`

- 当前: `cpptlm_emulator_register_dma_translate_cb(emu, cb)` 单独注册 DMA 翻译
- 替代: `cpptlm_emulator_register_callbacks(...)` 第 5 个回调参数
- 风险: 🟡 中(需 Hub 侧调用方适配 + 测试迁移)
- 暂**不采纳**(留给后续轮次; 当前优先删除明显冗余)

### 1.3 实际精简目标 (修订后)

**综合 A + B (修订)**:
- 18 → 15 函数表(删除 2 个真冗余)
- `get_version` → 宏(0 函数表占用)
- **总效果**: ABI 表面从 18 函数 → 15 函数 + 1 宏 = **16 符号**

**18 → 15 函数, 占 22 函数原始表面的 68%**; 总符号 26 → 20 = 77%

**修订前后对比**:
| 维度 | 原计划 (18→14+宏) | 修订后 (18→15+宏) |
|------|:-----------------:|:------------------:|
| 函数删除数 | 5 | **3** |
| `open/close` | 删除 | **保留** |
| Hub ack 风险 | 🟡 中 (5 函数级) | 🟢 低 (3 函数级) |
| Driver 功能影响 | 🟡 低 (场景 3+6) | 🟢 **零** |
| fd API 风格 | 丢失 | **保留** |
| 未来扩展空间 | 不可逆 | 可逆(可加不能减)|

---

## 2. 决策 (修订版)

✅ **决策 1 (修订)**: 删除 2 个真正冗余函数 (#5 create_by_id, #18 get_adapter_info)
- **理由**:
  - `create_by_id` 与 `create` (按 profile_path) 重叠; driver 可由 pkg-config 获得 profile_path
  - `get_adapter_info` 与 `get_device_info` (按 dev_id, 无句柄依赖) 重叠
- **保留 open/close**: fd 风格 + 生命周期分层语义价值
- **Hub 协调**: 必须, 需 UsrLinuxEmu 同步删除 2 处调用点

✅ **决策 2**: `cpptlm_emulator_get_version` 改 `#define CPPTLM_VERSION_STRING` 宏
- **理由**: 返回常量字符串, 改为预处理器宏零开销 + 零跨仓影响
- **Hub 协调**: 不需要, 头文件宏 + ABI 函数兼容 (驱动仍能调函数, 仅调用方改成宏)

✅ **决策 3**: **不采纳** 候选 C (`register_dma_translate_cb` 合并)
- **理由**: 当前 2 个注册函数不算"明显冗余", 风险/收益比不优
- **保留作为未来轮次评估项**

✅ **决策 4**: `cpptlm_emulator_t` 结构体 + 4 callback typedef **零修改**

### 2.1 精简后 ABI 表(15 函数 + 1 宏)

| # | 函数 | 决策 | 保留/移除理由 |
|---|------|------|-------------|
| 1 | `cpptlm_emulator_get_device_count` | ✅ 保留 | 设备枚举入口 |
| 2 | `cpptlm_emulator_get_device_info` | ✅ 保留 | 设备元数据 |
| 3 | `cpptlm_emulator_create` | ✅ 保留 | 创建 emulator (按 profile_path) |
| 4 | `cpptlm_emulator_destroy` | ✅ 保留 | 销毁 |
| 5 | `cpptlm_emulator_mmio_write` | ✅ 保留 | MMIO 数据面 |
| 6 | `cpptlm_emulator_mmio_read` | ✅ 保留 | MMIO 数据面 |
| 7 | `cpptlm_emulator_pcie_config_write` | ✅ 保留 | PCIe Config 数据面 |
| 8 | `cpptlm_emulator_pcie_config_read` | ✅ 保留 | PCIe Config 数据面 |
| 9 | `cpptlm_emulator_msix_init` | ✅ 保留 | MSI-X 初始化 |
| 10 | `cpptlm_emulator_msix_update_pending` | ✅ 保留 | MSI-X 触发 |
| 11 | `cpptlm_emulator_msix_clear_pending` | ✅ 保留 | MSI-X 清除 |
| 12 | `cpptlm_emulator_register_callbacks` | ✅ 保留 | 注册 4 类回调 |
| 13 | `cpptlm_emulator_register_dma_translate_cb` | ✅ 保留 (暂) | DMA 翻译回调 |
| 14 | `cpptlm_emulator_open` | ✅ **保留** (修订) | fd 风格 + 生命周期分层 |
| 15 | `cpptlm_emulator_close` | ✅ **保留** (修订) | fd 风格配对 |
| ~~16~~ | ~~`cpptlm_emulator_get_adapter_info`~~ | ❌ 删除 | 与 `get_device_info` 重叠 |
| ~~5~~ | ~~`cpptlm_emulator_create_by_id`~~ | ❌ 删除 | 与 `create` 重叠 |
| ~~1~~ | ~~`cpptlm_emulator_get_version`~~ | 🔄 改宏 | 返回常量字符串 |
| - | `CPPTLM_VERSION_STRING` (宏) | 🆕 新增 | 版本号常量 |

**核心 12 函数** = 设备管理(2) + 数据面(4) + MSI-X(3) + 回调(2) + DMA(1) — 接近实测 ~12 真实驱动需求。

### 2.2 移除函数清单 (修订后)

| 函数 | 现有调用方 | 迁移路径 |
|------|----------|---------|
| `cpptlm_emulator_create_by_id` | `src/abi/cpptlm_emulator.cc:181` (定义) + 内部使用 (在 `cpptlm_emulator_open` 中) · `test_cpptlm_emulator_abi.cc:105` (签名测试) · `test_cpptlm_emulator_handle_helpers.hh:16` (RAII helper) · `test_cpptlm_emulator_registry.cc:30, 37, 108` · `test_dgpu_board_shell_full_abi.cc:15, 35, 50, 70` | **调用方**全部改 `cpptlm_emulator_create("profile_path")`; **内部实现**: `open()` 内部保留对 `create_by_id` 的调用(因 `open` 自身不删除) |
| `cpptlm_emulator_get_adapter_info` | `test_cpptlm_emulator_abi_slimming.cc:54` (签名测试) · `test_dgpu_adapter_info.cc:11, 24, 38` (整个文件) | 全部改 `cpptlm_emulator_get_device_info(dev_id, ...)` |
| `cpptlm_emulator_get_version` | `test_cpptlm_emulator_abi.cc:27, 103` · `test_cpptlm_emulator_abi_slimming.cc:28` · `test_dgpu_board_shell_full_abi.cc:61` | 全部改用 `CPPTLM_VERSION_STRING` 宏 |

**测试文件迁移总计**: ~9 处(4 个测试文件)
- `test_cpptlm_emulator_abi.cc`: 2 处 (get_version + create_by_id 签名测试)
- `test_cpptlm_emulator_handle_helpers.hh`: 1 处 (RAII helper)
- `test_cpptlm_emulator_registry.cc`: 3 处
- `test_dgpu_board_shell_full_abi.cc`: 5 处 (4 × create_by_id + 1 × get_version)
- `test_cpptlm_emulator_abi_slimming.cc`: 2 处 (get_version + get_adapter_info 签名测试)
- `test_dgpu_adapter_info.cc`: 整个文件改写测试 get_device_info

---

## 3. 实施

### 3.1 文件清单 (修订后)

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/abi/cpptlm_emulator.h` | 改 | 删除 3 个函数声明 (`create_by_id`, `get_adapter_info`, `get_version`);确认 `CPPTLM_VERSION_STRING` 宏; `open/close` **保留** |
| `src/abi/cpptlm_emulator.cc` | 改 | 删除 3 个函数实现; 移除 `get_adapter_info` 句柄表逻辑; **保留 `open/close` 句柄表**;更新版本注释 "15 functions + 4 callbacks + 1 macro" |
| `test/test_cpptlm_emulator_abi.cc` | 改 | 移除 `get_version` / `create_by_id` 签名测试; 仅保留 15 函数签名测试 |
| `test/test_cpptlm_emulator_handle_helpers.hh` | 改 | RAII helper 改用 `cpptlm_emulator_create` 替代 `create_by_id` |
| `test/test_cpptlm_emulator_registry.cc` | 改 | 3 处 `create_by_id(0)` → `create("profile_path")` |
| `test/test_dgpu_board_shell_full_abi.cc` | 改 | 4 处 `create_by_id(N)` → `create("profile_path_N")`; 移除 `get_version` 调用 |
| `test/test_cpptlm_emulator_abi_slimming.cc` | 改 | 移除 `get_version` / `get_adapter_info` 引用 |
| `test/test_dgpu_adapter_info.cc` | 改/删 | 整个文件改写测试 `get_device_info`, 或合并到 `test_cpptlm_emulator_abi.cc` |
| `docs/cross_repo/HSK-12-cpptlm-abi-secondary-slimming.md` | 新 | 跨仓契约镜像(取代 HSK-11 §已删除函数清单) |
| `docs/soc_arch/adr/ADR-SOC-20-cpptlm-abi-secondary-slimming.md` | 改 | **本 ADR** (修订版) |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | 改 | §9.3 ABI 状态 18 → 15 |
| `docs/soc_arch/roadmap/phase9-p5-secondary-slimming.md` | 改 | P5 实施阶段文件(修订后 3 函数删除) |
| `docs/soc_arch/roadmap/phase9-post-phase8-roadmap.md` | 改 | 5 梯队 → 6 梯队 (+ P5) |
| `docs/soc_arch/adr/ADR-SOC-18-cpptlm-abi-slimming.md` | 改 (追加 ## Status Update) | 标注后续 ADR-SOC-20 二级精简 |

### 3.2 验收标准 (DoD, 修订后)

- [ ] `cpptlm_emulator.h` 剩余 **15 个 C 函数 + 1 宏 + 4 callback typedef** (`open/close` 保留)
- [ ] 4 callback typedef 零修改
- [ ] `cpptlm_emulator_t` 结构体零修改
- [ ] 15 函数签名零修改(保留的 15 个)
- [ ] 所有测试迁移完成(call-site grep 门禁 0 命中被删函数)
- [ ] `[abi-secondary-slimming]` 新增测试 PASS(验证 3 个已删 + 15 个仍可用)
- [ ] Hub (UsrLinuxEmu) 同步删除 3 处调用点(确认 PR 合并)
- [ ] 既有 `[pcie]` 测试零回归(66,564+ assertions)
- [ ] `openspec validate cpptlm-abi-secondary-slimming --strict` PASS

---

## 4. 跨仓协调

### 4.1 Hub (UsrLinuxEmu) 同步事项

| 同步项 | 实施方 | 时间窗 |
|--------|-------|:------:|
| 删除 `cpptlm_emulator_create_by_id` 调用 | Hub | Week 0-1 |
| 删除 `cpptlm_emulator_open` 调用 | Hub | Week 0-1 |
| 删除 `cpptlm_emulator_close` 调用 | Hub | Week 0-1 |
| 删除 `cpptlm_emulator_get_adapter_info` 调用 | Hub | Week 0-1 |
| `cpptlm_emulator_get_version()` → `CPPTLM_VERSION_STRING` 宏 | Hub(可选) | Week 0-2 |

### 4.2 协调策略 (per HSK-12 §5)

| Hub 响应 | 回退策略 |
|----------|---------|
| ack | 当前 plan 推进(删除 4 函数 + 改宏) |
| 部分 ack | 调整保留部分函数 |
| 拒绝 | 全保留 5 函数, ABI 不变 |
| 无响应(当前 Phase 9+ 经验) | 假定 Hub 侧已自行移除, 继续推进 |

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| Hub 集成断裂(如有未声明依赖) | 🟡 中 | 跨仓阻塞 | 提前 HSK-12 通知 + 10 工作日窗口 |
| handle API 在某些场景必需 | 🟢 低 | 真实驱动用 `create/destroy` 即可 | 实测 ~12 核心 ABI 远超 14 |
| `get_version` 改宏破坏 driver 编译 | 🟢 低 | 头文件宏兼容 | 同时保留函数实现作为 deprecated(可选) |
| 4 个级联函数移除顺序错 | 🟢 低 | 编译失败 | 一次性 commit 删除全部 4 个 + 测试同步 |
| `register_dma_translate_cb` 与 `register_callbacks` 重叠未清理 | 🟢 低 | 长期 ABI 膨胀 | 留给未来轮次(本次不动) |

---

## 6. 备选方案(已拒绝 + 修订)

### 备选 1: 18→10 激进精简(同时移除 register_dma_translate_cb)

**拒绝理由**: 当前 15 函数已接近真实需求 12, 进一步精简收益递减; `register_dma_translate_cb` 与 `register_callbacks` 重叠度有争议(语义不同: dma_translate 是函数指针 vs callbacks 是 4 个回调); 风险递增

### 备选 2: 18→17(仅改宏 get_version, 不删函数)

**拒绝理由**: 收益仅 1 个函数, 跨仓协调成本仍需承担; 不如做完整清理

### 备选 3: 不动 18 ABI, 维持现状

**拒绝理由**: 用户调研 2027-09-17 已明确"还可以继续精简"; 真冗余(create_by_id, get_adapter_info)已识别

### 备选 4 (修订拒绝): 18→14+宏 (删除 open/close) ← **本 ADR 初版**

**拒绝理由 (per 用户反馈 2027-09-17)**: open/close 模仿 fd 模式, 有 **API 设计语义价值**:
- 真实 driver fd 模式: `open("/dev/dri/renderD128")` → ioctl(fd, ...) → close(fd)
- 生命周期分层语义: 主进程持有 `cpptlm_emulator_t*`, 业务模块借用 `cpptlm_emulator_handle_t`
- API 设计一致性: 与 kernel driver fd 风格匹配

**风险评估** (per 6 个 driver 场景):
- 进程间 fd 共享: ❌ 之前无此能力, 删除后仍无 → 🟢 零影响
- 多线程 ref count: ❌ 之前无此能力 → 🟢 零影响
- 生命周期分层: ✅ 有合法价值 → **保留 open/close**
- 多 emulator 实例: ✅ 通过 create 支持 → 🟢 不受影响
- handle 状态追踪: ❌ 之前无 per-handle 状态 → 🟢 零影响
- fd API 风格统一: ✅ 有 API 设计价值 → **保留 fd 风格**

**决策修订**: 保留 open/close, 改为 **删除 3 函数路径 (18→15+宏)** (本 ADR 当前修订版)

**修订时间线**: 初版 2027-09-17 (路径 18→14+宏) → 用户反馈 2027-09-17 (open/close 语义价值) → 修订 2027-09-17 (路径 18→15+宏)

---

## 7. 参考文献

- 第一轮精简: [ADR-SOC-18-cpptlm-abi-slimming.md](./ADR-SOC-18-cpptlm-abi-slimming.md)
- P5 实施路线: [phase9-p5-secondary-slimming.md](../roadmap/phase9-p5-secondary-slimming.md)
- 模块文档: [dgpu-soc-pcie-slice.md §9.3](../modules/dgpu-soc-pcie-slice.md)
- 现有 ABI: `include/abi/cpptlm_emulator.h` (line 64-104 当前 18 函数表)
- 实现: `src/abi/cpptlm_emulator.cc` (line 181, 433, 468)
- 测试 grep 证据: 9 处调用方分布(详见 §2.2 表)

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Proposed — 等待 Hub ack + 测试迁移

## Status Update

No updates yet (initial version, 2027-09-17).