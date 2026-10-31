# Tasks — cpptlm-test-sh-arg-parsing-timeout v1.0(TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-10-31)· **来源**: H7 对拍收尾轮暴露的既有缺陷(台账 Entry #120 §⑨)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/ci-test-entrypoint/spec.md`](specs/ci-test-entrypoint/spec.md)
> **估时**: 0.5 天(3 个 Phase)· **冻结面**: 零改动 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **用户裁决**: U2 =「建独立 change 修」(2026-10-31)

---

## 执行进度(待 builder 阶段回填)

| Phase | 状态 | 说明 |
|---|------|------|
| 0 治理前置 | ✅ **已完成**(2026-10-31) | change 目录 4 件套已建 + `openspec validate --strict` rc=0 + 台账归因 dated 更正已落 + 历史残留构建树已清理 |
| 1 修法实施 | ⬜ 待实施 | 第 3 项检查改用「已存在的文件」+ 新增第 4 项反向 fixture |
| 2 验收 + 归档 | ⬜ 待实施 | 全量 ctest 不排除任何测试即全绿 + 门禁 + 归档 |

---

## Phase 0 — 治理前置 ✅

- [x] 0.1 起草 4 件套(proposal / design / tasks / delta spec `ci-test-entrypoint`)
- [x] 0.2 `openspec validate --strict` rc=0
- [x] 0.3 **归因实测证伪** —— `BUILD_DIR=/tmp/opencode/__probe_bd bash -x ./test.sh --mode off --build`
      ⇒ 输出 `[1/459] Building CXX object ...` ⇒ 证明超时在**编译阶段** ⇒ **「递归」不成立**
- [x] 0.4 台账 Entry #120 §⑨ 加 **dated 更正**(append-only,不改写原文)
- [x] 0.5 清理历史残留构建树(`/tmp/opencode/__no_such_build_dir__`)

## Phase 1 — 修法实施(TDD 5-step)

> **TDD 纪律**:先写**红**的判据(反控),再改实现,再跑绿。

- [ ] 1.1 **先复现红**:在**当前**实现下跑 `scripts/test/test_sh_arg_parsing.sh`
      ⇒ 记录第 3 项检查的实际耗时(预期 **> 1500s** 或被外部 timeout 打断)
      ⇒ 留档为「修复前基线」
- [ ] 1.2 **写反控(红)**:在 `scripts/test/test_sh_arg_parsing.sh` 新增第 4 项检查 ——
      「`BUILD_DIR` 指向**已存在的文件** ⇒ 秒级失败且**不产生**构建产物」
      ⇒ 此时该检查应**因「产生了构建产物」而红**(现状:第 3 项检查用「不存在的目录」⇒ 会创建目录)
      ⇒ ⚠️ 断言须指向**具体原因**(「产生了构建产物」),不是「超时」
- [ ] 1.3 **改实现(绿)**:第 3 项检查的 `BUILD_DIR` 从「不存在的目录」改为「**已存在的文件**」
      ⇒ 用 `mktemp` 造临时文件(避开仓库内路径,免污染)
      ⇒ 同步更新脚本注释:说明**为什么**是「文件」(cmake 在配置阶段失败 ⇒ 秒级)
- [ ] 1.4 **跑绿**:`bash scripts/test/test_sh_arg_parsing.sh` ⇒ 全绿
      ⇒ 且第 3 + 第 4 项检查合计耗时 **< 30s**
- [ ] 1.5 **鉴别力自检(反控 ②)**:临时把 `test.sh` 的互斥检查改回「`--build` 恒报互斥错」
      ⇒ 断言第 3 项检查**因互斥字样而红** ⇒ **立即还原**
      ⇒ ⚠️ 确认还原后仍绿
- [ ] 1.6 `git diff` 复核:改动**仅限** `scripts/test/test_sh_arg_parsing.sh`
      ⇒ **零生产代码改动**(`test.sh` 未动)

## Phase 2 — 验收 + 归档

- [ ] 2.1 `ctest --test-dir build -j4`(**不排除任何测试**)⇒ **全绿**、**零 Timeout**(AC-5)
- [ ] 2.2 `ctest --test-dir build -R test_sh_arg_parsing` ⇒ 单条 **Passed**(耗时 < 30s)
- [ ] 2.3 门禁:`bash scripts/test/cep_docs_sync_check.sh` · `cep_doc_ref_check.sh --strict` ·
      `cep_freeze_check.sh` · `cep_gate_selftest.sh` · `bash scripts/test/docs_sync_check.sh --strict`
      · `bash scripts/build/format.sh --check` ⇒ 全 rc=0
- [ ] 2.4 台账追加 Entry(**append-only**):本 change 的完成登记 + 归因更正的落点指针
- [ ] 2.5 `openspec validate --strict` rc=0 + `openspec archive cpptlm-test-sh-arg-parsing-timeout`
      ⇒ delta spec 合并进 `openspec/specs/ci-test-entrypoint/spec.md`
- [ ] 2.6 `git commit`(**不 push**)

---

## 非目标(Non-Goals)

| # | 不做 | 理由 |
|---|---|---|
| 1 | 不改 `test.sh`(生产脚本) | 生产路径不夹带测试专用钩子(design D-β) |
| 2 | 不删第 3 项检查 | 它是 `6a990316` 那个真 bug 的**回归锁** ⇒ 删掉 = 丢保护 |
| 3 | 不把该测试从 ctest 移除 | 会让「全量 ctest 全绿」变成排除法的假象(design D-δ) |
| 4 | 不改 H7 对拍 change 的任何产物 | 边界见 design D-ε |
| 5 | 不 push | 仓内已有 32 个未推送 commit |
