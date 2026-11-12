# tasks —— `cpptlm-cep-b3b2-archive-form`

> 顺序 = **A0(授权门) → A1(闸升级 + 重定向表) → A2(`07 号` 指针) → A3(归档态段) → A4(新闸) → A5(ctest + 守门表) → A6(0 回归) → A7(冻结链 + 归档) → A8(KPI 回填)**。
> 门:**G1 = A0** · **G2 = A4**(新闸有牙)· **G3 = A6**(0 回归)· **G4 = A7**(冻结链 + 归档)。

## A0 · 授权门(G1)
- [ ] A0.1 台账 Entry #384 授权实施(占 1 slot,KPI **0/3 → 1/3**)
- [ ] A0.2 improvement + architect 决策已落盘
- [ ] A0.3 四件套已建 + `openspec validate --strict` rc=0

## A1 · 闸升级 + 重定向表
- [ ] A1.1 `LEDGER` ⇒ 根集合(表 + 环境变量 + 缺省,去重)
- [ ] A1.2 锚匹配 ⇒ **行首标题匹配**
- [ ] A1.3 `test/cep_b3b2_ledger_redirect.json`(roots[0] = 台账现路径)
- [ ] A1.4 既有 4 条 deferrals 仍绿 + JSON 零 diff

## A2 · `07 号` 指针(冻结件)
- [ ] A2.1 §12 追加**单向指针**一句(⛔ 不复写规则)

## A3 · 归档态段
- [ ] A3.1 台账**追加** `## §14` 段:§14.1~§14.7 + §14.7.1 撤回登记(空占位)
- [ ] A3.2 段首显式声明「形态定义,非归档已发生」

## A4 · 新闸(G2)
- [ ] A4.1 `scripts/test/cep_b3b2_archive_form_check.sh`:V1~V6
- [ ] A4.2 M1(破坏 deferrals ⇒ `ledger_entry` 红)· M2(降级锚解析 ⇒ V2 判红)
- [ ] A4.3 真实仓字节指纹前后相等

## A5 · ctest + 守门表
- [ ] A5.1 `test/CMakeLists.txt` +1 ⇒ ctest **174 → 175**
- [ ] A5.2 `scripts/README.md` 守门表 **35 → 36**

## A6 · 0 回归(G3)
- [ ] A6.1 `ctest --test-dir build -j4` **175/175**
- [ ] A6.2 **41** 个 `scripts/test/cep_*.sh` 逐个 rc=0
- [ ] A6.3 format / docs_sync / doc_ref / agents_md_lint 全 rc=0

## A7 · 冻结链 + 归档(G4)
- [ ] A7.1 builder 台账 Entry(授权号)→ 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验
- [ ] A7.2 `openspec archive`(✅ 已完成归档语义)⇒ delta **ADDED** 合并进 `openspec/specs/cep-b3b2-archive-form/`
- [ ] A7.3 归档件 tasks 回填(逐项核对实际文本后再勾)

## A8 · KPI 回填
- [ ] A8.1 活口径 **7 处** **1/3 → 0/3**

## ⛔ 明确不做
- ⛔ **执行归档** · ⛔ 改 `B3b-2` 计数 · ⛔ 改 deferrals JSON 内容 · ⛔ 回改既有 Entry / SU · ⛔ 动 `§4.0` · ⛔ 新建 ADR
