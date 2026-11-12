# D-2 验收报告 —— `cpptlm-cep-iag-gate-execution-arm`

> **判据来源（按优先级）**
> 1. 用户口令「按建议推进直到完成所有任务」（= architect 梳理 → planner 盘点 → builder 实施 → verifier 验收）
> 2. `openspec/changes/archive/2026-11-12-cpptlm-cep-iag-gate-execution-arm/proposal.md` 的 **7 条 AC**
> 3. architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md` 的 **6 条判据 + 4 条决策**
> 4. 台账 **Entry #379**（立项 + 4 条裁决 + 起草期纠正）· **#380**（实施）· **#381**（归档）
>
> **验收轮次**：**第 1 轮**（`RDDF_VERIFIER_MAX_LOOPS=3`）
> **结论**：**PASS** —— 7 条 AC **全部满足**；**0 BLOCKER**；**2 条已登记的实施期实测订正**（见 §五）
> **角色边界**：本报告**未修改任何实现**（只读 + 运行验证命令）。

---

## 一 · 逐条判定（每条均有工具证据）

| AC | 判定 | 证据（命令 → 实测） |
|---|:--:|---|
| **AC-1** 装配路径臂有牙 | ✅ | `cpptlm_tests "[cep][assembly][iag]"` ⇒ **3 cases / 22 assertions** 全绿;反控 = bridge `protocol_instance` ⇒ 未声明 id ⇒ `instantiateAll` 返回 false 且输出**含 `[IAG-02]`**（dup2 捕获 stdout,⛔ 非 rc≠0）;还原 ⇒ 绿。**有鉴别力的第二形态**:桥引模块不在拓扑 ⇒ 按 `[IAG-06]` 红（无臂时会红于**模块解析**,错误签名不同）⇒ 证明臂**先于**模块创建 |
| **AC-2** 离线臂有牙 + 两臂同签名 | ✅ | 同一破坏 config ⇒ `cep_instance_assembly_gate` ⇒ rc=1 且输出含 **`[IAG-02]`** ⇒ 与装配路径臂**同签名** ⇒ 两臂共享同一判据实现;还原 ⇒ rc=0 绿 |
| **AC-3** 两臂同绿 · 9 份 config | ✅ | CLI 逐个实跑:8 **PASS** + 1 **SKIP**(`cep_chi_rnf_e2e_a.json` = `cep` 段无 `bridges` 键 ⇒ 判据自身 SKIP)⇒ **无一被新执行臂判红**;与 architect 预判**完全一致**。装配路径侧:`[cep][assembly][iag]` 正控绿 |
| **AC-4** 挂点真跑（Q1 教训） | ✅ | `cmake --build build --target validate_topology` ⇒ **rc=0** 且输出内 IAG CLI **实跑 9 次**(8 PASS + 1 SKIP)⇒ ⛔ 非「挂点存在但命令面没调」;变异 M2（摘掉 `${CPPTLM_CEP_VALIDATION_COMMANDS}`)⇒ 对账判据 **C6 红**(`NO_IAG_SEGMENT`)⇒ 还原 ⇒ 绿 |
| **AC-5** 禁双实现 + 判据只读 | ✅ | `module_factory.cc` 调 `run_instance_assembly_gate`(=2 处)· CLI 调同函数(=3 处)· 两处均调 `load_instance_assembly_inputs`(=2 文件)⇒ **判据实现 + 加载规则双共用**;`git diff --quiet -- include/tlm/cep/instance_assembly_gate.hh` ⇒ **零 diff** ✔ |
| **AC-6** 0 回归 | ✅ | `ctest --test-dir build -j4` ⇒ **174/174**;`scripts/test/cep_*.sh` ⇒ **40/40 rc=0**;`format.sh --check` / `docs_sync_check.sh --strict` / `cep_doc_ref_check.sh --strict` / `agents_md_lint.py` ⇒ **全 rc=0**;`cep_freeze_check.sh` 无参 rc=0（**80 产物** · provenance `4780deb3`） |
| **AC-7** 范围纪律 | ✅ | `git diff --stat 49e8b5e7..HEAD` = **18 文件 / +859 / −25**;逐条核对:判据本体 **0 行** · `protocol_instances.json` **0 行** · `collective_planes.json` **0 行** · `docs/adr/` **0 个变更文件** · `B3b-2 = 0/4` 行仍在(1 命中) · 未加「体量数字」断言 |

---

## 二 · 判据有牙性复核（不只看 rc）

| 判据 | 是否只靠 `rc≠0` | 反证 |
|---|:--:|---|
| 装配路径臂 | ❌ | `[IAG-02]` 与 `[IAG-06]` **按签名**断言（dup2 捕获 stdout）;且 `[IAG-06]` 形态与「模块解析错误」**签名不同** ⇒ 有鉴别力 |
| 离线臂 | ❌ | 破坏 ⇒ 含 `[IAG-02]` 才判红;还原 ⇒ 绿 |
| 挂点真跑 | ❌ | M2 摘掉 IAG 段 ⇒ C6 红(`NO_IAG_SEGMENT`)⇒ 还原 ⇒ 绿 |
| 禁双实现 | ❌ | 对账闸 C1~C4（grep + `git diff --quiet`）⇒ 任一臂改用另一份实现即红 |

---

## 三 · 遗留清扫复核（本次范围内的自查项）

| 项 | 实测 |
|---|---|
| `TODO` / `FIXME` / `XXX`（4 个新增文件） | **0** |
| 被注释掉的代码 | **0** |
| `skip` / 跳过的测试 | **0** |
| 临时日志 / 调试输出残留 | **0**（`mktemp` 的破坏 config 用完即 `rm -f`;影子树在 `/tmp/opencode/`） |
| 临时调试分支（`pdb` / `breakpoint()`） | **0** |
| 未替换的 mock / 临时 fallback | **0** |

---

## 四 · 失败项与路由（**0 BLOCKER**）

**无失败项。**

---

## 五 · 已登记的实施期实测订正（⛔ 非缺陷掩盖，均可在仓内复核）

| # | 订正 | 登记处 |
|:--:|---|---|
| ① | **装配臂落点 Step 0.75 → Step 0.26**:architect 决策 D-A1 的**理由**（「IAG-06 需读 topology 模块表 ⇒ 必须等 hierarchy 解析」）**实测不成立** —— `instance_assembly_gate.hh` 的 `module_declared()` 读的是 **config 的 `modules` 段**(静态声明),⛔ 不需要已创建的对象 ⇒ IAG 族**纯 config 依赖** ⇒ 可更早。决策**目的**(早失败 + 零副作用)**不变**;对账闸 **C7** 以「臂行号(200) < `// 2. 创建所有模块实例` 行号(337)」机械保证 | design §D-6 · Entry #380 ③-a |
| ② | **ctest 用例数 173 → 174**:离线 CLI **自身**也注册了 ctest ⇒ 实为 **+2**(对账闸 + CLI)⇒ 判据**未放宽**,只订正记录值 | design §D-7 · Entry #380 ③-b |

---

## 六 · 已用轮次

| 轮 | 结论 | 说明 |
|:--:|---|---|
| **1（本轮）** | **PASS** | 7/7 AC 满足;0 BLOCKER;无需返工 |

**是否打回**：**否**。

---

## 七 · 复核命令清单（可原样重跑）

```bash
# 判据本体(两臂)
./build/bin/cpptlm_tests "[cep][assembly][iag]"
for f in configs/cep_*.json; do ./build/bin/examples/cep_instance_assembly_gate "$f"; done

# 挂点真跑
cmake --build build --target validate_topology 2>&1 | grep -c 'cep instance assembly gate'   # 期望 9

# 对账闸(7 判据 + 2 变异)
bash scripts/test/cep_iag_gate_reconcile_check.sh

# 0 回归
ctest --test-dir build -j4
for g in scripts/test/cep_*.sh; do bash "$g" >/dev/null || echo "FAIL $g"; done
bash scripts/build/format.sh --check
bash scripts/test/docs_sync_check.sh --strict
bash scripts/test/cep_doc_ref_check.sh --strict
python3 scripts/test/agents_md_lint.py
bash scripts/test/cep_freeze_check.sh

# 范围纪律
git diff --numstat 49e8b5e7..HEAD -- include/tlm/cep/instance_assembly_gate.hh \
    include/tlm/cep/protocol_instances.json include/tlm/cep/collective_planes.json
git diff --name-only 49e8b5e7..HEAD -- docs/adr
```
