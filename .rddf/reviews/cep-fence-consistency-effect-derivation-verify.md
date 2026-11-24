# 验收报告：cep-fence-consistency-effect-derivation（G4-a · fence 一致性效果的派生声明面）

> **角色**：verifier（角色内建验收 · 只验不改）· **日期**：2026-11-24
> **变更集**：`4421df37`（立项 commit）→ `bda3aabb`（HEAD）· 4 个 commit：`f7576e64` 产物 · `b002c7c8` 冻结清单 · `bda3aabb` 五步链回执
> **结论**：**PASS 16/16**（⛔ 0 项失败）· 另有 **1 条 SHOULD_FIX**（**不阻塞**归档）

---

## 0. 判据来源

| 优先级 | 来源 | 本次取用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 无额外（用户指令「同意你的建议」⇒ 裁决 = **G4-a**） |
| 2 | `openspec/changes/cep-fence-consistency-effect-derivation/proposal.md` 的 `## 验收标准` | **16 条 AC**（AC-1 ` AC-16）· **本报告的主判据** |
| 3 | 本次变更自己声明的行为 | `design.md` D-3 判据骨架（J-1 ` J-8）+ 台账 **Entry #544** ② 规则表 R1–R7 |
| 4 | ADR 的 Verification 章 | 本单元**零 ADR 正文变更** ⇒ 不适用 |

## 0.1 分流判定

| 条件 | 判定 | 证据 |
|---|---|---|
| `openspec/changes/<name>/` 存在且 proposal 含 `## 验收标准` | ✅ 满足 | proposal.md 存在 · 含该段（16 条） |
| rdd-builder **P3 archive gate** 已触发（`iteration.json`） | ❌ **不满足** | `.rddf/state/iteration.json` **不存在** ⇒ 信号**不可检测** |
| `rdd-verifier` skill 可加载 | ✅ 满足 | 但**前置条件 2 不成立** ⇒ **不调用** |

⇒ **走本地验收（契约 1–5 步）**；本地额外门（`iteration.json` 的 `tasks_done == tasks_total`）在本项目**不适用**（文件缺失）⇒ 以 **change 的 `tasks.md` 实施段全勾 + 16 条 AC 全量取证**代替（如实标注 **N/A**，非「跳过」）。

---

## 1. 验证顺序与总结果

| # | 步 | 结果 | 关键证据 |
|:--:|---|---|---|
| 1 | 编译 / 静态检查 | ✅ | `ninja -j4` rc=0 · `openspec validate --changes --strict` = **1 passed / 0 failed** · `--specs --strict` ⇒ `✓ spec/cep-fence-consistency-effect-derivation` |
| 2 | 单元测试 | ✅ | `cpptlm_tests "[cep][fence_effects]"` = **198 assertions / 6 test cases** 全通过（含 22 条 static_assert） |
| 3 | 集成 / 端到端 | ✅ | **全量 ctest 207/207 rc=0**（⛔ 无 `-R` 过滤）· **53/53** `cep_*.sh` rc=0 |
| 4 | 边界与错误路径 | ✅ | **2 条反向 fixture** 各按自己的红因签名变红 + **死分支合成行正控**（TSO / PLO / RC 单方向 / R1 优先级 / 跨族 / 未知标签）+ **JSON 缺键必红**（verifier 侧独立实测） |
| 5 | 遗留清扫复核 | ✅ | 三个新文件零 TODO / FIXME / 调试输出（§4） |

---

## 2. 16 条 AC 逐条判定

| AC | 判据 | 判定 | 证据（工具 + 输出摘要） |
|:--:|---|:--:|---|
| **AC-1** | 推导函数存在性（编译期） | ✅ | `test_cep_fence_effects.cc` 内 **8 条** `static_assert(kFenceEffectCapabilities[i].effect == derive_fence_effect(…))`，失败签名逐行含**实例 id**（实测提取：row0→chi_rnf … row7→ualink_4） |
| **AC-2** | 派生正确性（**独立 oracle**） | ✅ | 测试内 **硬编码期望表** `kExpectedEffects[8]`（逐行 {id, FenceEffect} · ⛔ **不由** `derive_fence_effect` 生成）⇄ 编译出的派生值 ⇒ 8/8 相等；闸另有 **oracle 存在性护栏**（「✓ AC-2 独立 oracle 在位（测试内硬编码期望表 8 行）」）⇒ oracle 被删 ⇒ 必红（`fence-oracle-missing`） |
| **AC-3** | 输入副本漂移 | ✅ | 闸「✓ AC-3 输入副本与 JSON 逐字段一致」（头文件副本 (read, write, barrier) ⇄ `protocol_instances.json` 逐实例逐字段）；测试内运行期同判；**反向**：改拷贝根 JSON 标签而副本不动 ⇒ 红 `fence-input-copy-drift`（fixture ②） |
| **AC-4** | 域外必红 | ✅ | 闸「✓ AC-4 全部行在推导域内」；**反向**：注入域外标签组合 ⇒ 红 `fence-derivation-out-of-domain`（fixture ①）；编译期合成行：跨族（SC/TSO）与未知标签（XYZ）⇒ `Invalid` |
| **AC-5** | JSON 缺键必红 | ✅ | 闸「✓ AC-5 JSON 键齐备」；**verifier 侧独立实测**：删 `mesif.consistency_labels` ⇒ 红 `fence-json-key-missing: mesif`（⛔ 未静默跳过） |
| **AC-6** | 实例集双向零 orphan | ✅ | 闸「✓ AC-6 正向 / ✓ AC-6 反向」；测试用例「实例集双向零 orphan（AC-6）」 |
| **AC-7** | basis 非空 + id 唯一 + 粒度声明 | ✅ ⚠ | basis 非空 ✓（闸 + 测试逐行）；id 唯一 ✓（闸 + 谓词负控）；**粒度声明**：常量 `kFenceEffectGranularityIsPerInstance` 在位 + 文件头诚实边界段声明「⛔ 不区分 barrier 发生在哪个网络侧」⇒ **声明面整体已声明**；⛔ **但 per-row basis 未逐行复述该限定** ⇒ 见 §3 的 **F-1**（SHOULD_FIX） |
| **AC-8** | 死分支合成行正控 | ✅ | 实测提取 **7 条**合成行断言：TSO⇒FullBarrier · PLO⇒Partial · RC-acquire-only⇒AcquireOnly · SC(+barrier)⇒None · PLO(+无barrier)⇒None（R1 优先级）· 跨族⇒Invalid · 未知标签⇒Invalid（⛔ 合成行**不进**实例表 ⇒ 不破坏 AC-6） |
| **AC-9** | 诚实边界（机器可读） | ✅ | **6 个常量齐备**（闸「✓ AC-7 诚实边界常量齐备（6 个）」）：`kFenceEffectIsDeclarationOnly` · `kFenceEffectIsNotCrossDomainGuarantee` · `kBarrierAbsenceMeansNoFenceEffect` · `kScModelMeansNoExtraFenceEffect` · `kRcEffectIsPaperUpperBoundSubset` · `kFenceEffectGranularityIsPerInstance`；措辞「**声明效果**」在位 |
| **AC-10** | 通用层零协议名 / 零 include + 与 G1/G10 正交 | ✅ | 闸「✓ 零协议类名 / ✓ 零 include 协议头 / ✓ 与 G1 / G10 正交（无 egress/in_flight/hop/latency）」 |
| **AC-11** | 两条反向 fixture（各自红因签名） | ✅ | `--selftest`：**基线拷贝根 rc=0** + 变异① `fence-derivation-out-of-domain` + 变异② `fence-input-copy-drift` ⇒ **2/2 按预期红因变红**（⛔ 非「rc≠0」冒充）；跑在 **mktemp 拷贝根**（唯一临时名）⇒ 零工作树污染（⛔ 不与两个变异闸共享工作树文件） |
| **AC-12** | main spec + 五步链 | ✅ | main spec 在位（6 Requirement · 含**规则表 R1–R7**）· delta 由主 spec **提取生成** · `openspec validate --changes --strict` = valid · 冻结清单 `artifact_count` **93 → 94** · 含新 spec = **True** · 五步链 commit 链 `4421df37` → `f7576e64`（产物）→ `b002c7c8`（清单**单独** commit）→ `bda3aabb`（回执）· 无参 `cep_freeze_check.sh` rc=0（94 产物 hash 一致） |
| **AC-13** | 计数同步面（**命令实测**，⛔ 不写死） | ✅ | 实测：主 spec **141**（`cep-*` **50**）· `cep_*.sh` **53** + `cep_*.py` **14** = **67** · `test_cep_*.cc` **80** · active **1** · 登记册 §0.1 四行取值逐字一致 ⇒ `cep_registry_baseline_check.py`「**5 项文档取值 == 实测**」rc=0；README 守门表新增行命中 1 · `include/tlm/cep/AGENTS.md` 索引含 `fence_effects.hh`（头文件数 **51**） |
| **AC-14** | 零回归 · **全量口径** + 冻结件零残留 | ✅ | `ctest --test-dir build -j4`（⛔ **不按 `-R` 过滤**）⇒ **207 / 207 passed, 0 failed** rc=0；**53 / 53** `cep_*.sh` rc=0；**非 `cep` 只读闸 9 / 9** rc=0（`archive_verify` / `config_type_registration` / `docs_adr_index` / `docs_index_consistency` / `markdown_link` / `openspec_version` / `test_sh_arg_parsing` / `rddf_stage_gate` / `cep_docs_sync_check --strict`）；**冻结件零残留**（`git status -- include/tlm/cep/ docs/adr/ openspec/specs/` ⇒ 空） |
| **AC-15** | 零 schema / 零 instances / 零 ADR 正文 | ✅ | `git diff --name-only 4421df37..HEAD -- include/tlm/cep/protocol_instance.schema.json include/tlm/cep/protocol_instances.json docs/adr/ test/cep_invariant_assertion_map.json` ⇒ **空** |
| **AC-16** | 零 invariant 新增 | ✅ | `cep_invariant_map_check.py` ⇒「**66/66** = 66 项有牙载体 + 0 项 ⭭-unmodelled + 0 项 ⭭-pending」；`git diff 4421df37..HEAD -- test/cep_invariant_assertion_map.json` ⇒ **空** |

**判定汇总：PASS 16 / 16（失败 0）**

---

## 3. 发现（不阻塞归档）

| # | 级别 | 发现 | 证据 | 建议 / 路由 |
|:--:|:--:|---|---|---|
| **F-1** | SHOULD_FIX | **per-row `basis` 未复述「实例粒度 · 不区分网络侧」**：AC-7 要求 basis 声明该限定，但当前 8 行 basis 只写规则编号与结论（「R1 · 无 barrier 流 ⇒ …」）；该限定只在**文件头诚实边界段**与**常量注释**里声明 ⇒ 只看**单行** basis 的读者可能过度解读为「该实例的 fence 保证」 | §2 AC-7 实测：`grep -c "不区分" include/tlm/cep/fence_effects.hh` = **1**（仅在常量注释 / 头段，非 per-row） | 路由 **builder**（注释级 · 零行为变更 · 头文件**不在冻结集** ⇒ 无需五步链）：给 8 行 basis 追加统一短缀「（实例粒度 · 不区分网络侧）」。**不阻塞**归档 |

---

## 4. 遗留清扫复核（本次范围 = 3 个新文件 + 1 个新 main spec + change 工件）

| 类别 | 检查 | 结果 |
|---|---|---|
| TODO / FIXME / XXX / HACK | 三个新文件全量 grep | **零命中** |
| 半截实现 | 声明面：枚举 / 族映射 / 推导函数 / 表生成 / 谓词 / 6 常量 **均实现完整** | ✅ |
| 临时调试输出 | grep printf / std::cerr / DPRINTF | **零命中** |
| 注释掉的代码 | 行尾注释调用形态 + `#if 0` | **零命中** |
| 被 skip 的测试 | `[cep][fence_effects]` 6 个用例全部**实跑**（198 断言）· 无新建 `.disabled` | ✅ |
| 未替换的 mock | 本单元**零 mock** | ✅ |
| 临时 fallback 分支 | 闸的 `--selftest` 走 **mktemp 拷贝根**（唯一临时名）⇒ **零工作树污染** | ✅ |
| **自证风险（metis 重点）** | ① 表行**由函数生成** ⇒ 行值与规则不可能不一致（但也**不构成**独立证据）② 派生正确性由**测试内硬编码 oracle** 承担（**独立**路径）③ 闸**不重算**派生值 ⇒ 闸与测试不共享推导路径 ④ 闸另有 **oracle 存在性护栏**（oracle 被删 ⇒ 必红） | ✅ 四层构造到位 |
| **副本第二真源风险** | 头文件内含 labels / barrier 镜像副本 ⇒ 由 **J-3 逐字段比对**兜底（红因 `fence-input-copy-drift`）+ **反向 fixture ②** 证明该判据有牙 | ✅ |
| 范围外历史遗留 | **不清理**（本单元不趁机改动）· 仅记录 | ✅ |

---

## 5. 失败项与路由建议

- **失败项：0**（16/16 PASS）⇒ 无需路由回 builder 修实现。
- **建议路由（不阻塞）**：**F-1** 走 builder 注释级补写（可在本单元归档前顺带做，零行为变更）。
- **归档前置**：归档与计数回填属 **builder** 职责（`openspec archive` + KPI/计数回填 + 台账 Entry #544 收口）⇒ 需切回 builder 执行。

---

## 6. 已用轮次

| 轮次 | 内容 | 结果 |
|:--:|---|---|
| 1 | 本地验收（16 AC 全量取证 + 全量回归复核 + 遗留清扫 + **verifier 侧独立变异实测**：JSON 缺键路径） | **PASS 16/16** · 1 条 SHOULD_FIX |

**下一步（builder）**：① F-1 注释级补写（可选）② `openspec archive cep-fence-consistency-effect-derivation --yes` ⇒ 验证主 spec 的 Purpose **非占位** ⇒ 计数 / KPI 回填（active 1→0 · KPI 1/3→0/3 · 归档 175→176 · CEP 59→60 · 主 spec **141** / 闸 **53** / 测试 **80** / 冻结 **94**）⇒ 台账 Entry **#544** 收口（附本报告指针）⇒ 归档后**复跑** AC-13 / AC-14（⛔ 回填本身会改 7 处活口径 ⇒ 必须复验）。

## 7. 复现命令（本次全部证据）

```bash
# 契约闸 + 反向 fixture
bash scripts/test/cep_fence_effects_check.sh
bash scripts/test/cep_fence_effects_check.sh --selftest
# 判据测试
./build/bin/cpptlm_tests "[cep][fence_effects]" -r compact
# 全量回归（⛔ 不按 -R 过滤）
for s in scripts/test/cep_*.sh; do bash "$s" >/dev/null 2>&1 || echo "FAIL $s"; done
ctest --test-dir build -j4
# 非 cep 只读闸（9 个）
for c in archive_verify_check.sh config_type_registration_check.sh docs_adr_index_check.sh \
         docs_index_consistency_check.sh markdown_link_check.sh openspec_version_check.sh \
         test_sh_arg_parsing.sh rddf_stage_gate.sh cep_docs_sync_check.sh; do bash scripts/test/$c; done
# 冻结链
bash scripts/test/cep_freeze_check.sh
# 零改动硬查
git diff --name-only 4421df37..HEAD -- docs/adr/ include/tlm/cep/protocol_instance.schema.json \
  include/tlm/cep/protocol_instances.json test/cep_invariant_assertion_map.json
```
