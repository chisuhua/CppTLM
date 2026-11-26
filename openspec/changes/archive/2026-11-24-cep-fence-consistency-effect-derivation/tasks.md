## Tasks

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **⚠️ 回归口径（per 台账 Entry #542 ⑫ + #543 ⑥ + #544 ⑦）**：**零回归 = 全量 ctest（⛔ 不按 `-R` 过滤）+ 全部 `cep_*.sh` + 非 `cep` 只读闸 + `docs_sync --strict`** ⇒ **并附冻结件零残留核对**。

### Task 0: 授权（先于改冻结面）

- [x] 台账 Entry **#544**（立项授权 + 推导规则表 R1–R7 + metis 12 条 finding 处置 + 边界）
- [x] improvement v2（16 条 AC · 状态「已批准·待立项」）
- [x] metis 1 轮（P0×4 / P1×4 / P2×3 · 全部处置）
- [x] `.openspec.yaml` + `design.md`（builder 域）

### Task 1: 派生声明面（新增 `include/tlm/cep/fence_effects.hh`）

- [x] `FenceEffect` 枚举（7 值 · 方向化）+ `fence_effect_name()`
- [x] `ConsistencyFamily` + `consistency_family_of()`（唯一映射点）
- [x] `derive_fence_effect()`（constexpr · R1–R7 · 唯一推导入口）
- [x] `make_fence_effect_capabilities()` ⇒ 8 行表（**由函数生成**）+ 自洽谓词
- [x] **6 个诚实边界常量** + 文件头规则表 + 判据骨架

### Task 2: 测试（新增 `test/test_cep_fence_effects.cc`）

- [x] 编译期：逐行 `effect == derive(行输入)`（8 条 static_assert · 失败签名带实例 id）
- [x] **独立 oracle**（硬编码期望表 ⇄ 派生值 · ⛔ 不由推导函数生成）
- [x] **死分支合成行正控**（TSO / PLO / RC-acquire-only / R1 优先级 / 跨族 / 未知标签）
- [x] 运行期：输入副本漂移 ⇄ JSON（逐实例逐字段）+ JSON 缺键必红
- [x] 实例集双向零 orphan + 边界常量 + 谓词变异副本负控

### Task 3: 闸（新增 `scripts/test/cep_fence_effects_check.sh`）

- [x] 8 条判据（J-1 ~ J-8 · 含 oracle 存在性护栏 + 规则表契约面核对）
- [x] `--root` / `--selftest`（拷贝根 · 零工作树污染）
- [x] **2 条反向 fixture**（各按自己的红因签名）+ 前置失败显式失败
- [x] **检查器自身崩溃 ⇒ 必红**（⛔ 不得因「无输出」误判为绿 —— 假绿纪律）

### Task 4: 契约面（**冻结面** · 与实现同批）

- [x] main spec（6 Requirement · 含规则表 R1–R7）
- [x] delta **由主 spec 提取生成**（`## ADDED Requirements` ⇒ 零漂移）
- [x] `openspec validate --changes --strict` rc=0 · `--specs --strict` 本 spec 绿

### Task 5: 计数同步面（AC-13）

- [x] `test/CMakeLists.txt` 注册 2 个 ctest 条目
- [x] `scripts/README.md` 守门表新增行
- [x] 登记册 §0.1 四行（闸族 / CEP 测试 / 主 spec / OpenSpec）
- [x] `include/tlm/cep/AGENTS.md` 文件索引新增本头（头文件数 +1）
- [x] `cep_registry_baseline_check.py` rc=0

### Task 6: 五步授权链（Entry #544）

- [x] 步 1：台账 Entry #544 先落 ✅
- [x] 步 2：产物 commit
- [x] 步 3：`cep_freeze_check.sh --update --authorized-by "Entry #544"` ⇒ 期望「**新增 1 · 移出 0 · hash 变更 0**」
- [x] 步 4：冻结清单**单独** commit
- [x] 步 5：无参复验 `cep_freeze_check.sh`（rc=0 · 产物 hash 一致）

### Task 7: 验收与归档

- [x] 本地验收（verifier）：16 条 AC 逐条取证 · 遗留清扫
- [x] **全量** ctest（⛔ 不按 `-R` 过滤）· 全部 `cep_*.sh` · 非 `cep` 只读闸 · `docs_sync --strict` · 冻结件零残留
- [x] 归档 `openspec archive cep-fence-consistency-effect-derivation --yes` ⇒ 验证主 spec 的 Purpose **非占位**
- [x] 计数 / KPI 回填（active 1 → 0 · KPI 1/3 → 0/3）
- [x] 台账 Entry #544 收口 + 验收报告落 `.rddf/reviews/`
