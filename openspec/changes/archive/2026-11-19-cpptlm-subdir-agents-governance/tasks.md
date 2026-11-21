# tasks — cpptlm-subdir-agents-governance

- [x] G1 `agents_md_lint.py` 增 `--all`（体量 ≤10 KiB / 单行 ≤1000 / **未标注**失效引用 / 登记对账）+ `--root` + `--selftest`（6 变异）+ 6 段模板自述
- [x] G2 修调用点：`docs_sync_check.sh` 传 `--all --strict`（原先未传 `--strict` ⇒ 闸无牙）
- [x] G3 修 6 处失效/矛盾：`include/core`（legacy 类树 + `core/ext` 段）、`configs`（拓扑表 3 份改名 + 归档 change 引用）、`cpptlm` / `cpptlm_config`（归档 change 引用 + 校验命令路径）、`src/core`（"暂不编译"）、`test`（"文档待清理"）
- [x] G4 补 3 份主线 AGENTS.md：`include/tlm/{cep,pcie,gpu}/AGENTS.md`
- [x] G5 根文件：登记清单 15→18 + 分工纪律 5 条 + `external/PTX-EMU` 声明
- [x] G6 `cep_freeze_check.sh`：授权 id 提取 `grep "^### "` → `grep "^### Entry #"`
- [x] G7 ctest 注册 `agents_md_lint_all` / `agents_md_lint_selftest`

## 验收记录（2026-11-19）

| 判据 | 结果 |
|---|---|
| AC-1 | `agents_md_lint.py --all --strict` rc=0（**18 份**子目录 AGENTS.md） |
| AC-2 | `--selftest` **7/7**（基线 + 6 变异），含反向变异「标注后放行」 |
| AC-3 | `docs_sync_check.sh --strict` rc=0，且已传 `--all --strict` |
| AC-4 | 6 处失效全部闭合；残留命中经逐条核对**均为「说明已删除」的正确语境**（`AGENTS.md:262` ANTI-PATTERNS / `include/core:68` 说明删除） |
| AC-5 | 3 份新文件 3.4 / 3.3 / 3.9 KiB，均 ≤10 KiB，已登记 |
| AC-6 | 用子标题冒充：`--update --authorized-by "验收(AC-1 ~ AC-7,全部机器可判)"` ⇒ **拒绝**且 manifest 无改动；可用 id 由 **682 条降到 505 条**（剔除 177 条伪造入口） |
| AC-7 | `cep_freeze_check.sh`（无参）rc=0，89 件 hash 一致 |
| AC-8 | 全部文档闸 + `ctest` 全绿 |

**实施期发现**：`agents_md_lint.py` 的 STRUCTURE 树检查原按 **cwd** 解析相对路径 ⇒ 从 `build/` 或拷贝根运行会整片误报；已在 `main()` 固定 `os.chdir(ROOT)`（`--root` 因此可用，selftest 才有意义）。
