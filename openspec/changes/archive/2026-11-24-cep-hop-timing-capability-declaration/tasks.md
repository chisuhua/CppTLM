## Tasks

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **⚠️ 回归口径（per 台账 Entry #542 ⑪⑫ 教训）**：**零回归 = 全量 ctest（⛔ 不按 `-R` 过滤）+ 全部 `cep_*.sh` + 非 `cep` 只读闸 + `docs_sync --strict`**。
> **时序钉**：主 spec 的**新建**与实现**同批 commit**（delta 由主 spec **提取生成** ⇒ 零漂移）⇒ 五步链 1 次（Entry #543）。

### Task 0: 授权（先于改冻结面）

- [x] 台账 Entry **#543**（立项授权 · 前置闭合 · 层的区分表 · 边界 · 五步链预测 · 回归口径纪律）
- [x] `openspec/changes/cep-hop-timing-capability-declaration/proposal.md`（planner 域 · 17 条 AC）
- [x] `.openspec.yaml` + `design.md`（builder 域）

### Task 1: 声明面（新增 `include/tlm/cep/hop_timing_capabilities.hh`）

- [x] `HopModel` / `TimingModel` 枚举 + `hop_model_name` / `timing_model_name`
- [x] `HopTimingCapability` struct（`id` / `protocol_hop` / `protocol_timing` / `basis`）+ `kHopTimingCapabilities[8]`（逐行 `basis`）
- [x] 自洽谓词 `hop_timing_capabilities_are_consistent`（J-2 + J-3 现状锁）
- [x] SFINAE 探针 `has_hop_api_v` / `has_timing_api_v`（优先级重载 · 协议中立）
- [x] **3 个诚实边界常量**（`tick()` ≠ 时序模型 · 仅对协议语义层成立 · 现状锁）
- [x] 文件头「层的三分」段 + 判据骨架 J-1~J-8

### Task 2: 测试（新增 `test/test_cep_hop_timing_capabilities.cc`）

- [x] 编译期否定断言（5 个实例引擎 × hop / 时序；失败签名含实例 id）
- [x] 探针**正控 / 负控**（合成类型 ⇒ 探针无牙可被抓）
- [x] 运行期 J-1（双向零 orphan）· J-2（自洽）· J-3（恒 None）
- [x] 载体层事实复述（J-6 · 3 处）
- [x] 谓词变异副本负控（basis 空 / id 重复 / 非 None / 空表）

### Task 3: 闸（新增 `scripts/test/cep_hop_timing_capabilities_check.sh`）

- [x] 12 条判据（AC-1/2/3/4/6/7/8/9/10）
- [x] `--root` / `--selftest`（拷贝根 · 零工作树污染）
- [x] **3 条反向 fixture**（各按自己的红因签名）+ 前置失败显式失败

### Task 4: 契约面（**冻结面** · 与实现同批）

- [x] main spec `openspec/specs/cep-hop-timing-capability-declaration/spec.md`（6 Requirement / 10 Scenario）
- [x] delta **由主 spec 提取生成**（`## ADDED Requirements` ⇒ 零漂移）
- [x] `openspec validate --changes --strict` rc=0 · `openspec validate --specs --strict` 本 spec 绿

### Task 5: 计数同步面（AC-13）

- [x] `test/CMakeLists.txt` 注册 2 个 ctest 条目（203 → 204）
- [x] `scripts/README.md` 守门表新增行
- [x] 登记册 `obj-cep-platform-remaining.md` §0.1「闸族 **52 + 14 = 66**」
- [x] `include/tlm/cep/AGENTS.md` 文件索引新增本头（**并补登 I1 漏掉的 `engine_capabilities.hh`**）
- [x] `cep_registry_baseline_check.py` rc=0

### Task 6: 五步授权链（Entry #543）

- [x] 步 1：台账 Entry #543 先落 ✅
- [x] 步 2：产物 commit
- [x] 步 3：`cep_freeze_check.sh --update --authorized-by "Entry #543"` ⇒ 期望「**新增 1 · 移出 0 · hash 变更 0**」· `artifact_count` **92 → 93**
- [x] 步 4：冻结清单**单独** commit
- [x] 步 5：无参复验 `cep_freeze_check.sh`（rc=0 · 93 产物 hash 一致）

### Task 7: 验收与归档

- [x] 本地验收（verifier）：17 条 AC 逐条取证 · 遗留清扫
- [x] **全量** ctest（⛔ 不按 `-R` 过滤）· 全部 `cep_*.sh` · 非 `cep` 只读闸 · `docs_sync --strict`
- [x] 归档 `openspec archive cep-hop-timing-capability-declaration --yes` ⇒ 验证主 spec 的 Purpose **非占位**
- [x] 计数 / KPI 回填（active 1 → 0 · KPI 1/3 → 0/3）
- [x] 台账 Entry #543 收口 + 验收报告落 `.rddf/reviews/`
