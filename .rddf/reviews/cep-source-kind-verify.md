# 验收报告 · `cpptlm-cep-basis-source-kind-and-cell-free`(P1 全量)

- **验收对象**:change `cpptlm-cep-basis-source-kind-and-cell-free`(open + implementation)
- **验收角色**:verifier(2026-11-06,用户口令「继续切 verifier」)
- **判据来源**:① `.rddf/improvements/cpptlm-cep-basis-source-kind-and-cell-free.md` **AC-1 ~ AC-6**(用户已批准的设计)② change `proposal.md` 的 Acceptance 段 ③ `specs/cep-basis-source-kind/spec.md` 的 SRK-01 ~ SRK-05
- **分流判定**:`openspec/changes/<name>/proposal.md` **有** `## Acceptance` 段 ✅,但 `.rddf/state/iteration.json` **不存在** ⇒ P3 archive gate **未触发** ⇒ ⛔ 不走 `rdd-verifier` skill,按本地验收 1~5 步执行
- **结论**:**PASS**(0 失败项 · 2 条 SHOULD_FIX 遗留 · 已用 **1** 轮)

---

## 一、验证顺序与结果

| 步 | 项 | 命令 | 结果 |
|---|---|---|---|
| 1 | 编译 | `cmake --build build -j4 --target cpptlm_tests` | **rc=0**,零新增警告 |
| 2 | 单测(本 change) | `./build/bin/cpptlm_tests "[cep][source-registry]"` | **9 cases / 417 assertions 全 PASS** |
| 2 | 单测(CEP 全量) | `./build/bin/cpptlm_tests "[cep]"` | **451 cases / 10779 assertions 全 PASS** |
| 3 | 集成 / 全量 | `ctest --test-dir build -j4` | **156 / 156 全 PASS**(270.43s;新增 `cep_source_registry` + `cep_source_effect_check`) |
| 4 | 决定性变异 | `bash scripts/test/cep_source_effect_check.sh` | **6 PASS / 0 FAIL**(签名钉死) |
| 5 | 遗留清扫 | 见 §四 | 无 TODO / 半截实现 / 临时日志 / skip 测试 / 未替换 mock |

## 二、AC 逐条判定

### AC-1 —— 逐格来源可解析 + 注册表双载体一致 ⇒ **PASS**

- 证据 1:`[cep][source-registry]` **SRK-01a~e** 逐载体遍历( tilelink 主矩阵 12 格 + T3 11 行 · cxl 68 格 · mesif 35+5 格 · ualink 11+4 格),每格 `resolve_ref(...).registered == true` ⇒ **未登记 = 0**;
- 证据 2:**SRK-03** 读 `include/tlm/cep/sources.json`(经 `CPPTLM_SOURCE_DIR`)与 `kSources[]` 逐条比对 id / kind / title + 条数 ⇒ 一致;
- 证据 3:闸脚本 clean 态输出 `consistent: 8 sources`;
- 证据 4(独立取证,非测试代码):`/tmp/opencode/check_src_kind`(自建遍历程序)输出 **`BAD=0`**。

### AC-2 —— 按载体 `basis ↔ ref_kind` 映射 0 违规 ⇒ **PASS**

- 证据 1:**SRK-02** 遍历 4 载体全部格,`basis_ref_kind_consistent()` 全部为真;
- 证据 2(**鉴别力**):**SRK-04b** —— 同一条 `D1` 依据,标 `PublicDirect` ⇒ 一致;改标 `ArchforgeAnchor` ⇒ **必须不一致**(实测 `REQUIRE_FALSE` 通过)⇒ 判据非恒真;
- 证据 3(变异):影子头把 `tilelink-c` 降级为 `PublicNonnormative` ⇒ 探针 **rc=1**,签名 `SRK-02 inconsistency: TLC-M-01`;
- 证据 4:按载体映射规则实测(tilelink `Direct`⇒`spec_clause`;cxl `PublicDirect`⇒`public_nonnormative`;mesif `Primary`⇒`repo_derived` 且 ref 非空;ualink `SelfDerived`⇒`repo_derived` / `Pending`⇒`public_nonnormative`)与 design D-2 逐条一致。

### AC-3 —— `CellKind::Free` 四态互斥 + 消费端分支 ⇒ **PASS**

- 证据 1:**SRK-04** —— tilelink 三态计数和 = 12、`Free` = **0**;cxl `count_cells(Free) == 0` 且四态和 = 68;`transition_is_free()` 与 `kind == Free` **逐格相等**;`Free` 不使 `transition_is_known/unknown` 为真;枚举值互异(防 `Free` 别名化);
- 证据 2(**决定性变异,verifier 补做**):影子树把 `cxl_cache_adapter.hh` 的 `transition_is_free` 改为 `kind == CellKind::Unknown` ⇒ probe **rc=1**,输出 **`FREE-KIND mismatch s=0 e=3`**;clean 影子 rc=0 输出 `FREE clean`;真源 sha256 未变(`fd0c62e0…`)。
- ⚠️ 判据边界(诚实声明):真实 `Free` 格 = **0** ⇒ 本 AC 验的是**机制**(枚举 + 互斥 + 分支 + 鉴别力),**不是**真实数据。

### AC-4 —— 决定性变异 3 条按正确原因红 ⇒ **PASS**(第 3 条由 verifier 补做)

| 变异 | 期望签名 | 实测 |
|---|---|---|
| ① 删注册表条目 | 缺 id | `missing in json: cxl-d1` ✅ |
| ② 弱来源配 `Direct` | 映射不一致 | `SRK-02 inconsistency: TLC-M-01` ✅ |
| ③ `Free` 误当 `Unknown` | 四态断言失败 | `FREE-KIND mismatch s=0 e=3`(rc=1)✅ **verifier 影子树补做** |

⚠️ ③ **未**并入 `cep_source_effect_check.sh`(脚本只含 ①②)⇒ 见 §三 SHOULD_FIX-1。

### AC-5 —— 0 回归 + 冻结面零 diff + 闸族全绿 ⇒ **PASS**

- `ctest --test-dir build -j4` ⇒ **156/156**;
- `cep_freeze_check.sh` ⇒ **41 产物 hash 全部一致**(冻结于 commit `d8a29f37`)+ provenance 自检通过 ⇒ **零 diff**;
- `cep_docs_sync_check.sh` / `cep_doc_ref_check.sh` / `cep_decl_alignment_check.sh` ⇒ 全绿;
- `openspec validate cpptlm-cep-basis-source-kind-and-cell-free` ⇒ **valid**;
- `openspec list --json` ⇒ active = **1**(KPI 1/3,与文档一致)。

### AC-6 —— 遗留登记(architect 追加 SU 更新 D-1)⇒ **PASS**

- 证据:台账 **Entry #252 §⑥ 1** 明确登记「`07 号` SU · 四 D-1 的『不可机器判』声明需 architect 追加 SU 更新(builder 不改 ADR / 冻结面)」;
- ⚠️ 该遗留**未闭合**(属 architect 域)⇒ 见 §三 SHOULD_FIX-2。

## 三、SHOULD_FIX(不阻塞,登记遗留)

1. **AC-4③ 未进脚本** —— `Free` 误当 `Unknown` 的决定性变异仅由 verifier 在影子树补做,`cep_source_effect_check.sh` 仍只有 ①② ⇒ **建议**下一轮把该态并入脚本(否则该判据在 CI 中无自动化有牙载体)。
2. **`UIS-10` 白名单未进契约** —— 本次为「载体只 include 标准库」新增 1 条白名单(`"tlm/cep/source_registry.hh"`),**主测试与反向验证探针已同步**,但 `openspec/specs/cep-ualink-inc-state-spec/spec.md` 的 **UIS-10 场景未写该白名单** ⇒ 存在**契约漂移**风险(测试已放宽、契约未跟上)⇒ **建议**以 delta 形式登记(归档时合并),⛔ verifier 不改 main spec。

## 四、遗留清扫复核(本 change 范围)

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK | **0**(新文件全文扫描) |
| 临时调试日志(`printf` / `cerr`) | **0 残留** —— 仅 probe 内 3 处 `std::printf` 属**探针输出机制**(签名取证用),非调试残留 |
| 被 skip / disabled 的测试 | **0** |
| 注释掉的代码块 | **0** |
| 未替换的 mock | **0**(本 change 无 mock) |
| 临时 fallback 分支 | **0**(`CellKind::Free` 分支为**契约内**机制,非 fallback) |
| 范围外的历史遗留 | 未触碰(仅登记):`ADR-CEP-11` G-3 残余风险、`CellKind::Free` 真实格 = 0、4 族 T1/T4 缺口 |

## 五、失败项与路由

**无失败项**(0 项)⇒ ⛔ 无需回 builder、无需回 planner。

## 六、已用轮次

- 第 **1** 轮(2026-11-06)⇒ **PASS**;未触发重试上限(`RDDF_VERIFIER_MAX_LOOPS=3`)。

## 七、证据命令汇总(可复算)

```bash
cmake --build build -j4 --target cpptlm_tests
./build/bin/cpptlm_tests "[cep][source-registry]"     # 9 cases / 417 assertions
./build/bin/cpptlm_tests "[cep]"                        # 451 cases / 10779 assertions
ctest --test-dir build -j4                              # 156 / 156
bash scripts/test/cep_source_effect_check.sh            # 6 PASS / 0 FAIL
bash scripts/test/cep_freeze_check.sh                  # 41 产物零 diff
bash scripts/test/cep_docs_sync_check.sh
bash scripts/test/cep_doc_ref_check.sh
bash scripts/test/cep_decl_alignment_check.sh
```
