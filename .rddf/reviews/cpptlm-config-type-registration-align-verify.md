# 验收报告 —— cpptlm-config-type-registration-align（R-5 · 阶段 B）

> **判据来源**：`openspec/changes/cpptlm-config-type-registration-align/proposal.md` 的 `## 验收标准` 段（AC-01~AC-10 + G-1/G-2）
> **执行面**：**本地验收** —— 分流条件 ② 不满足：`.rddf/state/iteration.json` **不存在** ⇒ P3 archive gate 未触发 ⇒ 不走 `rdd-verifier` skill
> **已用轮次**：**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3`）
> **验收人**：verifier（**只验不改** —— 本轮零实现改动）
> **日期**：2026-11-18

---

## 0. 结论

**PASS（AC-08 按 blocked-by 如实交付为空，不判失败）** —— 其余 9 条 AC + G-1/G-2 全部通过。

| 项 | 判定 | 备注 |
|---|:--:|---|
| AC-01 ~ AC-07, AC-09, AC-10 | ✅ 通过 | 见 §1~§2 |
| **AC-08** | ⚠️ **未交付（blocked-by 如实登记）** | 判据本身要求「未落地则标 `blocked-by` 并显式登记」⇒ **该义务已履行** ⇒ 不判失败（见 §3） |
| G-1 / G-2 | ✅ 通过 | 见 §4 |

---

## 1. 机制与判据（AC-01 ~ AC-04）✅

```bash
$ ./build/bin/cpptlm_tests "[type-registry]" -r compact
All tests passed (146 assertions in 4 test cases)
```

| AC | 判据 | 证据 |
|:--:|---|---|
| **AC-01** 正向不误伤 | `instantiateAll == true` + 实例存在 | case 1（`REQUIRE(factory.getInstance("cpu") != nullptr)`） |
| **AC-02** 负向 fail-fast + 零实例残留 | `== false` + **实例表全空** | case 2：`CHECK(getInstance("cpu") == nullptr)` —— **连合法项也不该被建**（这是「预扫描在创建之前」的直接可观测后果）;另实测注入 2 个假类型 ⇒ `stderr` 报出全部 2 条 |
| **AC-03** 可重入 | 同 factory 失败一次后再跑合法 config ⇒ `true` 且实例完好 | case 3 |
| **AC-04** 差集为空 | 运行时注册表 ∩ 配置 type = ∅;3 份受影响配置逐个可装配 | case 4 + 闸输出 `64 份配置 / 44 个 type / 注册表 63 个` ⇒ **未注册 0** |

⚠️ 判据**未断言日志**（AC-02 的「⛔ 不断言日志」已遵守）—— 错误消息只用 `stderr` 作人工诊断出口。

## 2. 生成器一致性 / 闸判别力 / 零回归 / 归档 / 文档（AC-05~AC-07, AC-09, AC-10）✅

### AC-05 生成器一致 ✅
```bash
$ python3 examples/generate_apu_soc.py --all --output-dir configs
$ python3 examples/generate_library_configs.py --all --out-dir configs
$ git diff --exit-code configs/     # rc=0
```
⇒ 生成器与生成物**逐字节一致**（这是「改 JSON 会回漂」的必要条件）。

### AC-06 闸有判别力（**双向**）✅
```bash
# 注入假 type 到真实配置
$ # configs/library_cpu_demo.json += {"name":"vfy_bogus","type":"ZzVerifyBogus"}
$ bash scripts/test/config_type_registration_check.sh   # rc=1
     type=ZzVerifyBogus  ← configs/library_cpu_demo.json
# 恢复
$ bash scripts/test/config_type_registration_check.sh   # rc=0
$ git status --short configs/                            # 空
```
闸**自带反向自测**也在每次运行中执行：注入 `ZzSelfTestFakeType_*` ⇒ 判据识别;已注册类型（`ApuSoC`）⇒ 不被误报。

### AC-07 零回归 ✅
```bash
$ ./build/bin/cpptlm_tests
All tests passed (883422 assertions in 2383 test cases)
```
基线（实施日重测）= 883279 / 2379 ⇒ **+143 断言 / +4 case**。
⚠️ 判据原文要求「⛔ 不得写死数字」，`proposal.md` 的 AC-07 已是「实施日重测基线」措辞（`rddf_stage_gate` 的 P-6 也守这条）。

### AC-09 文档与台账同步 ✅

| 载体 | 证据 |
|---|---|
| `docs/implementation/module-param-baseline.md` §2bis.2 | 改为「对齐层 ✅ / 机制层 ✅ / 判据层 ✅」三行 + **嵌套段仍未受保护**的显式警示 |
| `scripts/README.md` | `config_type_registration_check.sh` 行（含「零豁免」「反向自测」「ctest 注册」） |
| `AGENTS.md` | 「运行时类型注册表」命令段（`--list-types` / `--list-types --json` / 闸命令） |
| `test/CMakeLists.txt` | `config_type_registration_check` + `config_type_registration` 两个 ctest |
| 台账 | Entry #486 |

### AC-10 设计类型归档 ✅
```bash
$ ./build/bin/cpptlm_sim --list-types --json | (检查四类型)
  GPUTLM: ✓ 不在   SharedMemoryTLM: ✓ 不在
  MemoryClusterTLM: ✓ 不在   GpuSocTLM: ✓ 不在
$ ls include/tlm/gpu/gpu_tlm.hh          # No such file
$ ls include/tlm/gpu/shared_memory_tlm.hh # No such file
```
（该 AC 的实现归 T7，本轮只做验收 —— 归档动作见 Entry #481）

## 3. AC-08 —— **交付为空，blocked-by 如实登记** ⚠️

**判据原文**：
> 外层 SimModule 内联 `modules` 含未知类型 ⇒ 外层 `instantiateAll == false`;⚠️ **blocked-by** `cpptlm-module-param-migrate-batch2` T0.2 ⇒ 未落地则标 `blocked-by` 并显式登记,**⛔ 不许静默跳过**

**blocked-by 前置的状态**（实测）：
```bash
$ grep -n "T0.2" openspec/changes/cpptlm-module-param-migrate-batch2/tasks.md
9:- [ ] T0.2 ⚠️ **嵌套失败传播**（design D-4）：`SimModule::simulate_instantiate` 的...
```
⇒ **仍未勾选** ⇒ AC-08 的实现在本 change 不可能完成。

**登记核对**（判据要求的义务是否履行）：
```bash
$ grep -n "blocked-by" proposal.md
74:| 1 | **嵌套**（SimModule 内联 modules）的失败传播 | **blocked-by** ... ⇒ 本 change 只在 AC-8 显式标注依赖 |
102:| **AC-08** | ... ⚠️ **blocked-by** `cpptlm-module-param-migrate-batch2` T0.2 ⇒ 未落地则标 `blocked-by` 并显式登记,⛔ 不许静默跳过 |
```
+ 「## 遗留与 blocked-by」表的第 1 行（AC-08 = ⛔ blocked-by，写明「嵌套段交付为空」）

**行为实测**（确认现状与登记一致，⛔ 不把「检测到」误读成「已保护」）：
```bash
$ cat /tmp/nested.json   # ApuSoC 内联 modules 含 "ZzNestedBogus"
$ ./build/bin/cpptlm_sim /tmp/nested.json --cycles 1
[ModuleFactory] ERROR: 1 module(s) reference a type that is NOT in the runtime registry:
  type="ZzNestedBogus"  instance="inner_bogus"
$ echo $?      # 0
```
⇒ **检测到了（`stderr` 有报），但不传播**（外层仍成功）—— 这正是 `simulate_instantiate` 丢弃返回值的后果，也正是 AC-08 被 blocked 的原因。

**判定**：AC-08 的**判据要求的是「未落地则显式登记」**（而非「必须落地」）⇒ 该义务**已履行** ⇒ **不判失败**。
⚠️ 但归档时**不得**在台账或 spec 里写「嵌套路径已受保护」。

## 4. 护栏（G-1 / G-2）✅

```bash
$ ./build/bin/cpptlm_tests "[param]"           → 312 / 61
$ ./build/bin/cpptlm_tests "[config][schema]"  → 14 / 14
$ ./build/bin/cpptlm_tests "[cep]"             → 12413 / 540
$ ./build/bin/cpptlm_tests "[e2e]"             → 7442 / 150
$ python3 -m pytest test/python/ -q            → 232 passed
$ ctest --test-dir build -j4                   → 184/184
```

## 5. 边界与错误路径（verifier 顺序第 4 步）

| 边界 | 实测 |
|---|---|
| 空 `modules` | `exit=0`（空拓扑可装配，不误判为未注册） |
| `modules` 缺 `type` 字段 | `exit=1`（既有 schema 校验拦截，预扫描跳过该条 —— 不崩） |
| 多个未注册类型（含重复名） | 注入 3 条（2 个同名 + 1 个异名）⇒ **3 条全部报出**（「一次报全部」成立） |
| 部分失败时的零残留 | AC-02 断言**连合法项**都未创建（`getInstance("cpu") == nullptr`） |
| 用例过滤器 | 判据只用**返回值 + 实例表**，不依赖日志（日志在 Release 下不存在） |

## 6. 遗留清扫复核（verifier 顺序第 5 步）

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / 临时诊断日志 | **零命中**（本 change 触达的 6 个源/测试/脚本文件） |
| 被 skip 的测试 | **零命中** |
| 未替换的 mock / 临时 fallback | **零命中** |
| `DPRINTF` 作为报错路径 | ⚠️ builder 已发现并修 1 处（实例创建循环的 `else` 分支 → `stderr` + `return false` 作防御）;**`simulate_instantiate` 的 `DPRINTF` 刻意保留**（属 C2 / AC-08 范围）—— 判定**可接受**：该处对应 blocked 项，已显式登记 |
| 生成器漂移 | **已消除**（AC-05 双向验证） |

## 7. 失败项与路由

**无失败项。**

⚠️ 归档前必须处理的两件（不属验收范围）：
1. **T6.4 冻结面裁定** —— 新 main spec `config-type-registration-align` **不带 `cep-` 前缀** ⇒ 按 `cep_freeze_check.sh` 的 glob 口径**默认不入** M1 冻结集 ⇒ 须在台账**显式裁定**（同 C1 的 Entry #485 先例）
2. **T6.2 KPI 回填** 7 处（`3/3 → 2/3`）

## 8. 已用轮次

| 轮次 | 结果 |
|---|---|
| 1 / 3 | ✅ **PASS**（AC-08 按 blocked-by 如实交付，不判失败） |

**结论**：**允许归档**（`openspec archive`，⛔ 不用 `--skip-specs` —— 实现已落地）。

**关联**：`design.md` D-1~D-8 · 台账 Entry #486 · `docs/implementation/module-param-baseline.md` §2bis
