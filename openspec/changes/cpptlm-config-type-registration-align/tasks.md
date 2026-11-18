# tasks: cpptlm-config-type-registration-align

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（「读取即声明」原则平移到类型面）;实施形态见本 change 的 `design.md`（D-1~D-7）。
> 共 **T0~T6**;标 ⚠️ 的条目为 metis gap 分析新增的强制项。

> **执行顺序纪律**:**T1（对齐）必须先于 T2（上闸）** —— 否则 `test/test_param_alignment.cc`（会实例化含已准入类型的配置）与 `test/test_dgpu_pcie_device_perspective.cc` 会同时红，无法区分是本 change 的回归还是既有债（design D-4）。

## T0 — 前置与基线

- [x] T0.1 固化**基线事实**（写入 design D-1 的证据列）⇒ **2026-11-18 实测**：运行时注册表 **63 个类型**（`./build/bin/cpptlm_sim --list-types --json` 的 `count`;⚠️ 立项期写 67 —— T7.1~T7.3 **类型归档 −4**（`GPUTLM`/`SharedMemoryTLM`/`MemoryClusterTLM`/`GpuSocTLM`）后为 63）;4 个未注册类型 / **3 份配置**（`ArbiterTLM`(1,`library_memory_demo.json`) · `ComputeUnitTLM`(4,`apu_soc_full.json`) · `TCCTLM`(**1**,`apu_soc_full.json` —— ⚠️ 原文写「×2」是笔误，实测 1 处) · `TmuDispatchProcessorTLM`(1,`dgpu_board_v1.json`)）;`DPRINTF` 仅 Debug 生效（`include/core/sim_core.hh:8-16` 的 `#ifdef DEBUG_PRINT` + `CMakeLists.txt:70-72` 仅 `CMAKE_BUILD_TYPE STREQUAL "Debug"` 加 `-DDEBUG_PRINT`）;3 份配置 `cpptlm_sim --cycles 10` **全部 exit 0**、且 **grep `unknown|not registered|ComputeUnit|TCCTLM` 零命中**（= 静默丢弃的实证）
- [x] T0.2 ⚠️ 记录**基线数字** ⇒ **2026-11-18 实测**：`cpptlm_tests` **883279 / 2379**（立项期 883336/2393 ⇒ T7.7 归档降至 883243/2371 ⇒ C1 阶段 A +36/+8）;`pytest` **232 passed**;`ctest` **182/182**。⚠️ **AC-07 已改为「实施日重测基线」**（`proposal.md` 不写死数字 —— 类型归档会删测试，硬编码下限必然失效）
- [x] T0.3 ⚠️ 核对**未注册类型是否还有第 5 个** ⇒ **无第 5 个**：以**运行时注册表**（63 类型）对比 `configs/**/*.json`（**64 份**，排除 `param_rules/**`，含 `templates/`）中出现的 **48 个 type**（含嵌套 `modules[]`）⇒ 差集恰为上述 4 个。⚠️ 本轮为**静态**扫描（未展开 `extends`/`include`/`${var}`）—— 展开后的判据由 T3.2 的闸负责（T3.6）

## T1 — 对齐层（4 处漂移 + 生成器 + 断言）

- [x] T1.1 `configs/dgpu_board_v1.json` ⇒ **已落地**：删 `tmu` 条目（`TmuDispatchProcessorTLM` 的类无基类 ⇒ 不可注册） + 删 3 条指向它的悬空连接;**R-1 已登记**（`docs/implementation/module-param-baseline.md` §2bis.1）：`CP → TMU → SQ` 派发路径**今日本就是断的**（该模块从未被实例化 ⇒ `cp.dispatch` 从来没有下游 / `sq.cta_in` 从来没有上游）⇒ 删条目只是让配置**承认**事实，⛔ 不得据此推断派发能力退化。`cpptlm_sim configs/dgpu_board_v1.json --cycles 10` exit 0
- [x] T1.2 `configs/library_memory_demo.json` ⇒ **已落地**：`ArbiterTLM` → **`ArbiterTLM4`**（`channels=4`）;`n_ports` **已停发**（原登记为「新零效果键」—— 实测 `ArbiterTLM<N>::on_config_loaded` 已对 `n_ports`/`ports` 态② 拒绝，per C1 ⇒ 生成器停发即可，无需新增登记）
- [x] T1.3 `cpptlm/library/standard.py` ⇒ **已落地**：按 `channels` emit `ArbiterTLM2`/`ArbiterTLM4`，其它值 **`ValueError`**（⛔ 不静默兜底）;同时停发 `mem_type`（`MemoryTLM` 已态②）与 `memory_type`（`MemoryCluster` 无消费面 —— 生成器漂移，emit 会使生成物装配失败）;`examples/generate_library_configs.py` 无需改（它只调 factory）
- [x] T1.4 `configs/apu_soc_full.json` + 生成器 ⇒ **已落地**：移除 `ComputeUnitTLM` ×4 + `TCCTLM` ×**1**（⚠️ 原文写「×2」为笔误，实测 1 处）+ 对应连接、hierarchy 节点、`coherence_domains.members` 的 `tcc`、description 的「not yet registered」表述;CLI `--num-cu` 一并删除（⛔ 不留「声称支持但无实现」的开关 —— 与 D-5 的「配置不声明做不到的事」同一条纪律）;生成物：16 → **11** modules / 15 → **10** connections
- [x] T1.5 `test/test_apu_soc_from_config.cc` ⇒ **已订正（未删 case）**：case 名从「aspirational」改为 **`可装配`**，标签 `[aspirational]` → **`[loadable]`**;断言改为 `modules==11` / `connections==10` + **反向断言** `ComputeUnitTLM==0` / `TCCTLM==0`（守住不回归）+ 新增 **`REQUIRE(factory.instantiateAll(config))`**（原 NOTE 的「不调 instantiateAll」已删除 —— 配置现在真能装配，这正是本 change 的核心成果）
- [x] T1.6 ⚠️ 核对 `test/test_cep_contracts_crosscheck.cc:334-351` ⇒ **仍绿**（本 change 未删任何配置文件）⇒ 无越界
- [x] T1.7 ⚠️ AC-05 取证：重跑 `python3 examples/generate_apu_soc.py --all --output-dir configs` + `python3 examples/generate_library_configs.py --all --out-dir configs` ⇒ **无新增 diff**（生成器与生成物一致）

## T2 — 机制层（未注册类型 fail-fast）

- [ ] T2.1 `src/core/module_factory.cc` 新增 **Step 0.3 预扫描**（位置：插件加载 `:328-338` **之后**、实例创建 `:350` **之前**）⇒ 遍历顶层 `modules[]`,逐 `type` 查运行时注册表
- [ ] T2.2 未注册 ⇒ **`return false`** + `stderr` 报「**类型名 + 实例名 + 来源文件**」;⛔ **不走 `DPRINTF`**（Release 下无输出）;⛔ **不加 `--strict` 开关**;⛔ **不硬编码任何类型名**
- [ ] T2.3 ⚠️ 一次报**全部**违规（不逐个 return）⇒ 避免「修一个再跑一次」
- [ ] T2.4 ⚠️ 失败路径**零实例残留**（预扫描在创建之前 ⇒ 由**位置**保证，不靠事后清理）;核对 `ModuleGroup::registerInstance` 静态表未被污染
- [ ] T2.5 ⚠️ `src/core/module_factory.cc` 其余部分**零 diff**（该函数 528 行,承载 CEP 双闸 / `kCompositeOnly` / StreamAdapter 注入）⇒ 提交前逐行复核 diff
- [ ] T2.6 ⛔ **不碰** `include/core/sim_module.hh` 的 `simulate_instantiate` 签名（归 C2）

## T3 — 判据层（导出 + 差集闸 + 反向自测）

- [ ] T3.1 `src/main.cpp` 加 `--list-types --json` ⇒ 复用 `ModuleFactory::getRegisteredTypes()`;输出**零硬编码**
- [ ] T3.2 新增 `scripts/test/config_type_registration_check.sh` ⇒ 读 `--list-types --json` + 扫描 `configs/**/*.json` + `configs/templates/*.json`（排除 `configs/param_rules/**`）⇒ 求差集;⛔ **不扩展** `param_effect_inventory.py` 的名字匹配逻辑（它会漏掉 `TmuDispatchProcessorTLM` 与 `ArbiterTLM` 两个最难案例）
- [ ] T3.3 ⚠️ 闸**带反向自测**：向 fixture config 注入假 type ⇒ 必红;移除 ⇒ 必绿;⛔ 只验绿 = 没验
- [ ] T3.4 ⚠️ 闸**默认零豁免**（首次上线必须零豁免通过）;豁免集合若非空 ⇒ 每条含 `path + reason` 且与台账 §2bis 一一对应
- [ ] T3.5 ⚠️ 闸注册为 **ctest**;⚠️ 类型集合**构建相关**（`HybridCacheWrapper` 仅 `BUILD_RTL` 注册）⇒ 闸按构建运行，⛔ 禁静态白名单
- [ ] T3.6 ⚠️ 闸的扫描作用于 `extends` / `include` / `${var}` **展开之后**的结果（展开前静态扫描会漏 `${var}` 注入的类型）

## T4 — 测试

- [ ] T4.1 新增 `test/test_config_type_registration.cc`：**AC-01** 正向不误伤 + **AC-02** 负向 fail-fast（断言**返回值 + 实例表**,⛔ 不断言日志）
- [ ] T4.2 **AC-03** 失败后可重入（同进程失败一次后再跑合法 config ⇒ 仍 true 且实例完好）;动机 = `ModuleGroup` 静态表裸指针
- [ ] T4.3 **AC-04** 差集为空（脚本输出 + 3 份受影响配置逐个可装配）
- [ ] T4.4 **AC-09** 文档同步：`docs/implementation/module-param-baseline.md` §2bis 改「已闭合」· `scripts/README.md` 新脚本行 · `AGENTS.md`（`--list-types` CLI flag）· `test/CMakeLists.txt`（新测试文件 + ctest 条目登记）

## T5 — 验证

- [ ] T5.1 `cpptlm_tests` 全量绿（≥ 基线 **883336 / 2393**）;`pytest test/python/` **232 passed**;`ctest` 全量绿
- [ ] T5.2 `[param]` / `[config][schema]` / `[cep]` / `[e2e]` 标签全绿（AC-07 / G-1）
- [ ] T5.3 `openspec validate cpptlm-config-type-registration-align --strict` + `cep_docs_sync_check` + `cep_doc_ref_check` + `agents_md_lint` + `docs_sync_check --strict` 全绿
- [ ] T5.4 ⚠️ **AC-08 处置**：若 `cpptlm-module-param-migrate-batch2` 的 T0.2（嵌套失败传播）**未落地** ⇒ 本 AC 标 `blocked-by` 并在 proposal 的 R 表显式登记 ⇒ **⛔ 不许静默跳过**

## T6 — 收口

- [ ] T6.1 台账 Entry（实施记录 + 实测 + 改动面 + 遗留 R-1~R-4）
- [ ] T6.2 KPI 回填（活口径 7 处;本 change 立项时 **2/3 → 3/3**，归档时 **3/3 → 2/3**）
- [ ] T6.3 spec delta 合并（`openspec archive`，⛔ 不用 `--skip-specs` —— 实现已落地）+ 新 main spec 的 Purpose 手写
- [ ] T6.4 ⚠️ **冻结面核查**：新增 main spec 落 `openspec/specs/config-type-registration-align/spec.md` ⇒ **命中**既有 glob `openspec/specs/cep-*/spec.md`？⇒ **不命中**（非 `cep-` 前缀）⇒ 须确认该 spec 是否应入 M1 冻结集，若入则走 `cep_freeze_check.sh --update --authorized-by "Entry #NNN"`
- [ ] T6.5 独立验收（切 verifier;AC-01~AC-09 + G-1/G-2 逐条取证）
- [ ] T6.6 ⚠️ **残留清点**：本 change 范围内不得留 TODO / 半截实现 / 临时日志 / 被 skip 的测试 / 未替换的 mock;⚠️ 特别检查 `DPRINTF` 残留（Release 无输出 ⇒ 等于没报）

## T7 — 设计类型归档（用户 2026-11-18 裁定）

> ⚠️ **执行顺序：T7 必须先于 T2（机制）与 T5（验证）** —— 归档会删测试文件，改变回归基线；先归档再上闸，避免「基线在动的时候判红」。
> 判据与逐项影响面见 `design.md` **D-8**。

- [x] T7.1 **`GPUTLM` 归档**：删 `include/tlm/gpu/gpu_tlm.hh`;删 `chstream_register.hh:130` + `:266-267` 的两处注册;删 `configs/apu_soc_phase7a.json` + `configs/gpu_standalone.json` **整份**;`configs/apu_soc_phase7b.json` 去 `compute_unit_0`（含连接 / hierarchy / coherence_domains 成员）;`examples/generate_apu_soc.py` 删 `build_apu_phase7a()` + 7a target + 7b 的 `compute_unit_0`;删 `test/test_gpu_standalone.cc`;`test/test_apu_soc_from_config.cc` 删 phase7a case + 改 phase7b（8→7）+ 改 bulk;`test/test_sm_streaming_multiprocessor_port.cc` 改注释与 adapter sanity;4 个 Python topo 测试的 5 处 `add_module('gpu0','GPUTLM')` 改用仍存在类型;`test/test_cep_contracts_crosscheck.cc:334-335` 的 cfgs 去 `apu_soc_phase7a.json`（`REQUIRE(existing >= 3)` 仍成立）
- [x] T7.2 **`SharedMemoryTLM` 归档**：删 `include/tlm/gpu/shared_memory_tlm.hh` + `src/tlm/gpu/shared_memory_tlm.cc`;删 `src/CMakeLists.txt:43`;删 `chstream_register.hh:190`;删 `test/test_shared_memory_tlm.cc`
- [x] T7.3 **`MemoryClusterTLM` + `GpuSocTLM` 归档（连带）**：删两者的 `.hh` + `.cc`;删 `src/CMakeLists.txt:44` 与 `:50`;删 `chstream_register.hh:191`;删 `modules_cluster.hh:30`（include）+ `:59`（`REGISTER_MODULE(GpuSocTLM)`）;删 `test/test_memory_cluster_tlm.cc` + `test/test_gpu_soc_tlm.cc`
- [x] T7.4 **孤儿模板归档**：删 `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`（实测全仓零引用；删 SM + ClusterTLM 后只剩 2 模块）
- [x] T7.5 ⚠️ **复核保留项未被误删**：`CPUTLM` / `StreamingMultiprocessorTLM` / `GpuMeshNoC` / `CoherentXBarTLM` / `PcieControllerIP` / `PcieMemoryDevice` 均仍在注册表内
- [x] T7.6 ⚠️ **残留扫描**：全仓 `grep -rn` 四处类型名 ⇒ 只应剩**历史叙述**（台账 Entry / archive 件 / 已签发文档），不得剩**活代码引用**
- [x] T7.7 ⚠️ **重新记录基线**（AC-07 的下限以归档后为准）：`cpptlm_tests` **883243 / 2371**（归档前 883353 / 2397 ⇒ -26 case）· `pytest` **232 passed** · `ctest` **181/181**
- [ ] T7.8 ⚠️ **跨仓登记**：ArchForge 侧 `gpu-gputlm.md` / `gpu-shared-memory.md` / `gpu-memory-cluster.md` 的「✅ 已实施」需改为「⏸ 已归档」⇒ 本 change **不做**（跨仓），登记为 **R-5** backlog
