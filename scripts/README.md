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
| `test/` | `test_off.sh`, `test_ptx_emu.sh`, `verify_test_seam_fix.sh`, `run_all_tests.sh`, `ci_e2e_test.sh`, `docs_sync_check.sh`, `markdown_link_check.sh`, `gen_wire_format_snapshot.sh`, `check_pcie_ip_facade.sh`, `cep_docs_sync_check.sh`, `cep_doc_ref_check.sh`, `cep_freeze_check.sh`, `cep_archforge_alignment_check.sh`, `cep_decl_alignment_check.sh`, `cep_decl_reverse_verify.sh`, `cep_cohif_reverse_verify.sh`, `cep_protocol_engine_reverse_verify.sh`, `cep_protocol_bridge_reverse_verify.sh`, `cep_chstream_bidirectional_macro_reverse_verify.sh` | 分模式测试与兼容入口。根 `test.sh --verify-fix` 委托 `verify_test_seam_fix.sh`；其他仍按模式分派。**`cep_*` 10 个 = CEP 守门脚本**(契约同步 / 对外引用 / M1 冻结 / 跨仓锚点 / 声明映射双向 + 队列 #7 的 4 个反向 fixture 脚本),详见 §CEP 契约守门脚本 |
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

> 配套数据文件：`cep_doc_ref_aliases.tsv`（节号别名）/ `cep_doc_ref_registry.tsv`（语义 + 负向断言登记）。
> 10 个脚本全部已注册进 `ctest`（`ctest -R cep`）。

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
