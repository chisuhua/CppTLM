# scripts/ — CppTLM 工具脚本

按用途分 5 个子目录组织：

## 根目录便捷入口（推荐用法）

| 命令 | 用途 |
|------|------|
| `./build.sh` | 独立构建入口（薄包装转 `scripts/build/build.sh`）。`--rebuild` 干净构建；`--ptx-emu` 启用 PTX-EMU 集成；`--debug` Debug 模式；默认 `-j2` OOM-safe |
| `./test.sh` | 测试入口。**默认只跑测试**（binary 须已构建），显式 `./test.sh --build` 一次性 build+test。`--verify-fix` 跑 `verify_test_seam_fix.sh` 一键回归（Oracle Option C + classify_naive fallback 验证）|

## 子目录结构

| 目录 | 内容 | 用途 |
|------|------|------|
| `build/` | `build.sh`, `format.sh`, `build_ptx_emu.sh` | 构建与代码格式化（`build_ptx_emu.sh` 启用 PTX-EMU 集成，dGPU/APU SoC 默认路径）。根 `build.sh` 是薄包装，委托此目录 |
| `test/` | `test_off.sh`, `test_ptx_emu.sh`, `verify_test_seam_fix.sh`, `run_all_tests.sh`, `ci_e2e_test.sh`, `docs_sync_check.sh`, `markdown_link_check.sh`, `gen_wire_format_snapshot.sh`, `check_pcie_ip_facade.sh`, `cep_docs_sync_check.sh`, `cep_doc_ref_check.sh`, `cep_freeze_check.sh`, `cep_archforge_alignment_check.sh`, `cep_decl_alignment_check.sh`, `cep_decl_reverse_verify.sh`, `cep_cohif_reverse_verify.sh`, `cep_protocol_engine_reverse_verify.sh`, `cep_protocol_bridge_reverse_verify.sh`, `cep_chstream_bidirectional_macro_reverse_verify.sh`, `cep_mesif_state_spec_reverse_verify.sh`, `generate_mesif_recognition_checklist.sh` | 分模式测试与兼容入口。根 `test.sh --verify-fix` 委托 `verify_test_seam_fix.sh`；其他仍按模式分派。**`cep_*` 11 个 = CEP 守门脚本**(契约同步 / 对外引用 / M1 冻结 / 跨仓锚点 / 声明映射双向 + 队列 #7 的 4 个反向 fixture 脚本),详见 §CEP 契约守门脚本 |
| `pipeline/` | `run_full_pipeline.sh` | 完整仿真流程（生成→仿真→可视化） |
| `topology/` | `topology_validator.py`, `topology_generator.py`, `analyzer.py`, `path_tracer.py`, `credit_flow.py` | 拓扑生成/分析/验证 |
| `stats/` | `stats_annotator.py`, `stats_watcher.py`, `layout_manager.py`, `derive_expr.py` | 性能统计与可视化 |

## CEP 契约守门脚本（`scripts/test/cep_*.sh`）

| 脚本 | 作用 | 状态 |
|------|------|------|
| `cep_docs_sync_check.sh` | CEP 契约面同步检查（21 份 ADR 锚点 + invariant 计数单一真源 + P 编号口径 + 传输层 `CATEGORY_*` 分层纪律），共 **10 组** | ✅ 生效 |
| `cep_doc_ref_check.sh` | 对外文档引用可达性（节号引用目标节存在 + 语义断言 + 负向断言 + 跨仓 `path:line` 锚点） | ✅ 生效（ctest 注册 `--strict`） |
| `cep_freeze_check.sh` | M1 冻结面授权闸（成员数以清单为准）（`--update --authorized-by "Entry #N"` + 两步 commit） | ✅ 生效 |
| `cep_archforge_alignment_check.sh` | 跨仓（ArchForge）对齐锚点守门，33 锚点 | ✅ 生效 |
| `cep_decl_alignment_check.sh` | 协议实例声明 ↔ schema 双向映射（判据 C-4） | ✅ 生效 |
| `cep_decl_reverse_verify.sh` | 上者的**反向验证**（防自证） | ✅ 生效 |
| `cep_cohif_reverse_verify.sh` | CohIf 载体（CC-01 / CC-02）的**反向验证**（静态扫描 + 编译期锁毒化副本） | ✅ 生效（ctest 注册） |
| `cep_protocol_engine_reverse_verify.sh` | ProtocolEngine 端口面（`INV-CEP-07-IC2`）的反向验证 | ✅ 生效（ctest 注册） |
| `cep_protocol_bridge_reverse_verify.sh` | ProtocolBridge 特化桥（CC-03 / CC-05 / CC-06）的反向验证 | ✅ 生效（ctest 注册） |
| `cep_chstream_bidirectional_macro_reverse_verify.sh` | `REGISTER_CHSTREAM_BIDIRECTIONAL` 的**编译期锁**反向验证（N=1 必红且原因命中 `static_assert`） | ✅ 生效（ctest 注册） |
| `cep_mesif_state_spec_reverse_verify.sh` | MESIF 规约判据（队列 #9 `cpptlm-cep-mesif-state-spec` 的 MS-01 ~ MS-05）的反向验证（8 类毒化副本：依据清空 / kind↔pending 错配 / 无锚点引用 / 共享态写突变 / 本地事件秩下降 / 转换表挖洞 / 偏离不真） | ✅ 生效（ctest 注册） |

> 配套数据文件：`cep_doc_ref_aliases.tsv`（节号别名）/ `cep_doc_ref_registry.tsv`（语义 + 负向断言登记）。
> **本表 11 个脚本**全部已注册进 `ctest`（`ctest -R cep`）。
> ⚠️ 目录内另有 `cep_freeze_reverse_verify.sh` 与 `cep_gate_selftest.sh` 两个脚本**未列入本表**（预存登记缺口,非本次引入）。

### 反向 fixture 登记（队列 #7 `cpptlm-cep-cohif-carrier`）

> **纪律**：每条新判据必须有**可执行**的反向 fixture，且**必须登记**在此表
> （无登记 ⇒ fixture 可能只存在于某次会话里 ⇒ CI 跑不到 ⇒ 等于没有）。
> **登记三要素**：判据 ID · fixture 形态 · **登记位置**（ctest 名 / Catch2 标签）。

| 判据 | 反向 fixture（形态） | 登记位置 |
|------|----------------------|----------|
| **CC-01** ① | 加第 6 个 `flow_id` ⇒ 静态扫描红 | `cep_cohif_reverse_verify.sh`（ctest `cep_cohif_reverse_verify`）+ `[cep][cohif-carrier]` |
| **CC-01** ② | 混入网络层字段 ⇒ 静态扫描红 | 同上 |
| **CC-01** ③ | 域外流名进入 `flow_id` 域 ⇒ 静态扫描红 | 同上（扫描器含「域外流名」判定） |
| **CC-02** ② | 加虚函数 ⇒ `is_trivially_copyable` 编译失败 | `cep_cohif_reverse_verify.sh` + `[cep][cohif-carrier]`「CC-02 反向 fixture ④」 |
| **CC-02** ③ | 字段宽度 +1 ⇒ 宽度锁编译失败 | `cep_cohif_reverse_verify.sh` + `[cep][cohif-carrier]`「CC-02 反向 fixture ③」 |
| **CC-02** ③ | 某字段漏加宽度锁 ⇒ 锁表扫描红 | `[cep][cohif-carrier]`「CC-02 ③ 反向 fixture」 |
| **CC-03** | 篡改字段映射 ⇒ round-trip 必红 | `cep_protocol_bridge_reverse_verify.sh`（ctest `cep_protocol_bridge_reverse_verify`）+ `[cep][protocol-bridge]`「反向 fixture ④」 |
| **CC-04** ① | 断链 ⇒ 端到端断言必红 | `[cep][json-assembly]` / `[cep][cohif-carrier]`「CC-04 反向 fixture」（**结局枚举分类** ⇒ 防「因错误的原因红」） |
| **CC-04** ③ | `N = 1` ⇒ 宏 `static_assert` 编译红（原因命中） | `cep_chstream_bidirectional_macro_reverse_verify.sh`（ctest）+ `[cep][cohif-carrier]`「CC-04 ③」 |
| **CC-05** ① | 只注册不消费 ⇒ 死桥清单非空 | `cep_protocol_bridge_reverse_verify.sh` + `[cep][protocol-bridge]`「反向 fixture ①」 |
| **CC-05** ④ | 重复 `bridge_id_` ⇒ 判红 | 同上「反向 fixture ②」 |
| **CC-05** ④ | `protocol_id_` 与 `family` 不符 ⇒ 判红 | 同上「反向 fixture ③」 |
| **CC-06** | 映射产出超出 3 类值域 ⇒ 判红 | `[cep][protocol-bridge]`「CC-06」（**引用**队列 #6 的 `category_value_domain()`，不重立第二份判据） |

### 反向 fixture 登记（队列 #8 `cpptlm-cep-chi-rnf-subset`）

> 同上纪律。本批的**判据实现分布**决定了 fixture 的落点：实例/状态机/引擎面在 C++（判据本体零改动，
> 复用 AC-01~05 + 本 change 新增的引擎观测面），声明源闸面在 Python 守门脚本 ⇒ **不复制第二份实现**。

| 判据 | 反向 fixture（形态） | 登记位置 |
|------|----------------------|----------|
| **CR-01** ① | 删 `chi_rnf` 的**任一** `required` 键（13 个**穷举**）⇒ 判据必红 | `[cep][chi-rnf][cr-01]`（谓词 `schema_violations()`，缺键**记违规不抛异常**） |
| **CR-01** ③ | `protocol_version` 写成锁定表外的值（`chi-r3`）⇒ 必红 | 同上 |
| **CR-01** ② | `perm` 少一个值（4 值锁被破）⇒ 必红 | 同上 |
| **CR-02** ④ | `vc_binding` 的 `vc_id` 越界（3→9）⇒ 装配闸必 FAIL | `[cep][chi-rnf][cr-02]`（内联声明**必须**带边表，否则「假绿」） |
| **CR-02** ⑤ | 删 `vc_binding` 的某消息类 ⇒ 边端点失去绑定 ⇒ 闸必 FAIL | 同上 |
| **CR-02** ② | 消息类写成 6 类词表外的值 ⇒ 红 | `cep_decl_reverse_verify.sh`「破坏 5」 |
| **CR-02** ③ | 删 `ordering_assumption` 的某网络键（键不对称）⇒ 红 | `cep_decl_reverse_verify.sh`「破坏 4」 |
| **CR-03** ① | 删 `snoop` 映射行（仍声明使用）⇒ AC-04 必红 | `[cep][chi-rnf][cr-03]` |
| **CR-03** ② | 映射目标写成第 4 类 ⇒ AC-04 必红 | 同上 |
| **CR-03** ④ | 全表 == 方向默认表且**无** `identity_reason` ⇒ AC-04 必红 | 同上 |
| **CR-04** ② | 让某稳定态「有为稳定态但无投影」⇒ AC-05 必红 | `[cep][chi-rnf][cr-04]` |
| **CR-04** ③ | 改掉 `UC + Store → UD` 的目标态（4 ≠ 3）⇒ AC-05 必红 | 同上 |
| **CR-05** ① | `instance_id` 改成不存在的 id ⇒ AC-02 必红 | `[cep][chi-rnf][cr-05]` |
| **CR-05** ④ | 改成 `UNIQUE → READ`（降权限）⇒ AC-02 必红 | 同上 |
| **CR-06** ② | `impl = 不参与` 但 `caps.scope = SYSTEM` ⇒ 谓词必红 | `[cep][chi-rnf][cr-06]` |
| **CR-07** ③ | 加**未登记**的额外实例 ⇒ ⑥ 必红 | `cep_decl_reverse_verify.sh`「破坏 16」 |
| **CR-07** ④ | 把 `moesi_gpu` 登记进 `repo_owned_instance_ids[]`（撞名）⇒ 必红 | 同上「破坏 18」 |
| **CR-08** ① | 本仓域实例违 schema（`vc_binding` 越出 6 类词表）⇒ ① 扩面必红 | 同上「破坏 17」 |
| **CR-08** ⑤ | 登记 id 指向不存在的实例 ⇒ 登记键自身必红 | 同上「破坏 19」 |
| **CR-07** ② | 仅 `status` 差异 ⇒ 仍**绿**（正控：豁免边界） | 同上「破坏 15 / 20」 |
| **CR-09** ④ | 改 `Status Update` 段 ⇒ hash 校验红 / 跳过 `--update` ⇒ provenance 红 | `cep_freeze_check.sh`（ctest `cep_freeze_check`） |
| **CR-10** ② | 引擎换成 `CohIfNullProtocolEngine` ⇒ 发射谓词必红（Null 零发射） | `[cep][chi-rnf][cr-10]`（正/反向并置：`deliver_read_bundles` 1 vs 0） |
| **CR-10** ③ | 同拓扑把 `type` 换成 Null ⇒ 类型判据 + 抵达判据**双红** | `[cep][chi-rnf][cr-10][json-assembly]` |
| **CR-11** ① | **逐通道**屏蔽（6 条）：该通道观测点 == 0 且汇聚判据红 | `[cep][chi-rnf][cr-11]` |
| **CR-12** ① | 旁路 credit 门控 ⇒ 耗尽态 `emitted` 不再是 0（同款驱动 + 充足 credit 作对照） | `[cep][chi-rnf][cr-12]` |
| **CR-13** ① | 强制在途重复 `transaction_id` ⇒ 必被拒且**不增长在途集** | `[cep][chi-rnf][cr-13]` |
| **CR-14** ① | 抑制 CompAck / 重复完成 ⇒ `comp_ack_sent` 不再是**恰好 1** | `[cep][chi-rnf][cr-14]` |
| **subset 排除项** | 9 个排除 opcode 注入 ⇒ 全拒 + 三条后果（无通道事件 / 不占 TxnID / 不进发射队列） | `[cep][chi-rnf][subset-exclusions]` |
| **消费方覆盖** | 从声明里摘掉 `chi_rnf` ⇒ 装配闸 `per_instance` 缺失该实例 | `[cep][chi-rnf][consumers]` |

## 路径引用注意

- `scripts/CMakeLists.txt` 调度 `topology/topology_validator.py`
- 各 bash 脚本用 `BASH_SOURCE` 解析自身目录，所有跨子目录引用用相对路径 `../`
- Python demo 导入用 `sys.path.insert(0, ...)` 显式添加

## 验证

```bash
# 推荐流程: 先构建再测试
./build.sh && ./test.sh

# 干净重建 + 跑测试
./build.sh --rebuild && ./test.sh

# 一键回归验证 ([dgpu_framebuffer] 修复 + 4 commit)
./test.sh --verify-fix

# 其他用法
./test.sh --e2e
./test.sh --quick
./test.sh --mode ptx-emu  # 仅 PTXIR image H2D DMA，不执行 kernel/CuTe

# CMake target 应仍工作
cmake --build build --target validate_topology

# 文档路径同步检查（pre-commit hook 也会自动运行）
./scripts/test/docs_sync_check.sh --strict
```

## 文档同步检查 (`scripts/test/docs_sync_check.sh`)

- 扫描 `AGENTS.md` / `docs/ONBOARDING.md` / `roadmap.md` / `scripts/README.md` 中所有 `path/to/file.ext` 反引号引用
- 通过 CMake `INCLUDE_PATH_PREFIXES` 智能补全（处理无 `core/` 前缀的简写形式如 `<file>.hh`）
- 通过 `VIRTUAL_PATHS` 数组排除已删除/归档文件（仅在文档中说明删除原因时引用）
- `--strict` 模式发现任何缺失返回非零退出码（用于 pre-commit hook）
- 当前快照：[`docs/docs_audit_report.md`](../docs/docs_audit_report.md)

## 测试入口与范围

- 根目录 `test.sh` 是统一测试入口（默认只 test），支持 `--build`、`--mode auto|off|ptx-emu|both`、`--build-only`、`--test-only`、`--verify-fix`、`--quick`、`--python-only`、`--e2e` 和 `--ctest`。
- 根目录 `build.sh` 是独立构建入口（薄包装转 `scripts/build/build.sh`），支持 `--rebuild`、`--ptx-emu`、`--debug`、`--no-tests`、`--no-examples`、`--jobs N`。
- `scripts/test/test_off.sh` 承担 OFF 路径回归；`scripts/test/test_ptx_emu.sh` 承担 PTX-EMU 路径验证。
- PTX-EMU 验证范围严格限于 PTXIR image H2D DMA；kernel 执行与 CuTe 程序执行均明确排除。
- `run_all_tests.sh` 和 `ci_e2e_test.sh` 保留为兼容包装，不是新的 CI 主入口。
- `verify_test_seam_fix.sh` 是 `[dgpu_framebuffer]` 修复（commit `ba32fe97`）+ 4 commit 回归一键验证（7 步：git 状态/构建/fix target/9 核心标签/全量/稳定性/cpptlm-debug 4 件套）。可通过 `test.sh --verify-fix` 或 `scripts/test/verify_test_seam_fix.sh` 直接调用。
