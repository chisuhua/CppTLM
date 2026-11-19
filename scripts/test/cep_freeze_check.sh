#!/usr/bin/env bash
# scripts/test/cep_freeze_check.sh -- CEP Stage 0 产物冻结校验(机制 M1)
#
# 机制 M1「产物冻结先于解读」(per .rddf/roadmap/objectives/obj-cep-platform.md §9.2):
#   "4 件套由 CI 产出并带 hash;冻结后**不可改**;解读与冻结分离 ——
#    即使同一人,也不能改已冻结的产物"
#
# Stage 0 尚无实验 4 件套(那要 Stage 1 才有),故 M1 在 Stage 0 的对应物是
# **Stage 0 的规范性契约产物**冻结:
#   ① ADR-CEP-*.md(全量)           (真源 3:invariant 契约;成员数以冻结清单为准,⛔ 此处不写数字)
#   ② docs/architecture/07-*.md    (真源 1:CEP 总表)
#   ③ 2 份 schema                   (能力声明 + metrics)
#   ④ 3 个 tools/*.py               (4 件套契约的实现)
#   ⑤ umbrella 的 spec.md           (15 个 requirement)
#
# ⚠️ **两个 checker 脚本自身不冻结**:
#   `cep_freeze_check.sh`(本脚本)与 `cep_docs_sync_check.sh` —— checker 必须可修,
#   否则一次 checker bug 就锁死整个体系;且 checker 不承载契约语义,
#   承载语义的是**被它检查的产物**。
#
# 用法:
#   bash scripts/test/cep_freeze_check.sh                      # 校验(漂移即非零退出)
#   bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"
#   bash scripts/test/cep_freeze_check.sh --self-test          # 授权闸反向自测(10 条)
#
# ── 授权面(2026-11-19 收紧)──────────────────────────────────────────
#   · 合法 entry id **只认** `### Entry #<数字>`(台账 Entry 正文里的 `###` 级子标题
#     如「授权明细」「验收」**不**是合法 id —— 可被直接冒充)。
#   · **移出**冻结面(文件被删 / 范围收窄)必须按声明行登记:
#       `- 冻结面移出 `<相对路径>`:<理由>`
#     ⚠️ 不得以「Entry 正文里出现该路径」当作授权(台账正文大量引用全路径)。
#   · **排除**(glob 命中但豁免冻结)同样按声明行登记:
#       `- 冻结面排除 `<相对路径>`:<理由>`
#   · 两种声明行都写在**授权 Entry 正文**里 ⇒ 台账 = 唯一授权源,零第二真源。
#   · 未获授权的移出 ⇒ `--update` **拒绝写盘**(冻结面不可被无声缩小)。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

# ⚠️ **测试用环境变量覆盖**(2026-11-19):默认行为**完全不变**;仅供 `--self-test`
#    在**临时清单 / 临时台账**上做负向变异(绝不动真实清单)。
#    · `CEP_FREEZE_MANIFEST` —— 清单路径
#    · `CEP_FREEZE_LEDGER`   —— 台账路径
MANIFEST="${CEP_FREEZE_MANIFEST:-docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json}"
FROZEN_GLOBS=(
  "docs/adr/ADR-CEP-*.md"
  "docs/architecture/07-coherence-exploration-platform.md"
  # 2026-11-10(per 裁定表 v3.2 §五 批 0.6 ①):批 3 按层拆分 → `docs/architecture/cep/*.md`
  #   8 篇子文档必须与其主纲同受 M1 冻结面约束 ⇒ 提前扩 glob。
  #   按 Entry #324 C-2′ 的「glob 全量 + 显式排除表(当前为空)」口径 ⇒ 新文件自动入集,
  #   无需逐条登记「未冻结」。⚠️ 当前 `docs/architecture/cep/` 不存在 ⇒ 本 glob 现 0 匹配,
  #   冻结产物数 69 → 69 不变(批 3 建目录后自动纳入)。
  "docs/architecture/cep/*.md"
  "include/tlm/cep/protocol_instance.schema.json"
  "include/tlm/cep/fabric.schema.json"
  # 2026-10-25(Entry #56 B1'):**实例数据**也纳入冻结 ——
  #   `fabric.json` 是 ArchForge 的**派生副本**(`derived_from` = 22-nsa §0.5.4.1 + 32 号 §6.0.2),
  #   但它被 S3 spike 的**顶点/边集导出**与 ctest #85 `cep_decl_alignment_check` 的
  #   **第 ⑤ 项双向比对**消费 → 静默改它能翻转回归门的 verdict ⇒ 它事实上承载契约语义。
  #   冻结它 = 把「改前必须先确认 ArchForge 已更新」的自陈纪律**机器化**。
  #   ⚠️ 它是**派生刷新型**成员:上游 ArchForge 更新时须走 `--update --authorized-by`
  #   走同一授权流程(上游刷新协议见 `obj §11` Entry #56)。
  "include/tlm/cep/fabric.json"
  # 2026-10-25(Entry #64):队列 #4 归档 → 3 份 main spec 进入 M1 冻结集(34 → 37)。
  #   ⚠️ **只**匹配 main spec 路径(归档后的权威形态);归档件里的 delta 副本**不**入集 ——
  #   否则同一能力会有两份"冻结产物",语义重复且漂移面翻倍。
  "openspec/specs/cep-sharer-tracker-base/spec.md"
  "openspec/specs/cep-ordering-point-checker/spec.md"
  "openspec/specs/cep-channel-dependency-gate/spec.md"
  # 2026-10-26(Entry #70):队列 #5 归档 → 2 份 main spec 进入 M1 冻结集(37 → 39)。
  #   ⚠️ **只**匹配 main spec 路径(归档后的权威形态);归档件里的 delta 副本**不**入集
  #     —— 否则同一能力会有两份「冻结产物」,语义重复且漂移面翻倍(per Entry #64 第 1 步口径)。
  "openspec/specs/cep-collective-plane-base/spec.md"
  "openspec/specs/cep-collective-plane-contract/spec.md"
  "experimental/metrics.schema.json"
  # 2026-11-05(Entry #218;per ADR-CEP-06 SU#2 ② 门禁①):**预登记 out-of-scope 集合的机器投影**
  #   —— 真源 = `objective §9.1` ② 表(评审过的设计面);本文件是 tools 判据④ 的读本。
  #   ⚠️ 它是**派生刷新型**成员:真源(§9.1 ② 表)与本文件**必须逐假设集合相等**
  #   ⇒ 由 drift 校验强制(先例: cep_decl_alignment_check)⇒ 单改一侧即红。
  #   ⚠️ 冻结它 = 把「预登记不可事后自由扩张」的纪律**机器化**
  #   —— 否则「not-collected 冒充预声明」这条自欺升级通道形同虚设(per SU#1 §④)。
  "experimental/preregistration.json"
  "tools/render_report.py"
  "tools/run_sweep.py"
  "tools/ab_compare.py"
  # ── 2026-11-10(C-2′ / Entry #324):spec 侧 **glob 全量**(取代逐条枚举)──────
  "openspec/specs/cep-*/spec.md"
  # 2026-10-23(Entry #30 修正):umbrella 已归档 → delta spec 路径随迁。
  # ⚠️ **不得**用 `openspec/changes/**/specs/cep-core-contracts/spec.md`:
  #    `**` 可跨 0 层 → 未来任何 change 复用本 capability 名即被当作「新增未登记的
  #    冻结产物」而误报冻结漂移(前向陷阱)。故**只**匹配已归档的 umbrella。
  "openspec/changes/archive/*cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md"
  # ══════════════════════════════════════════════════════════════════════
  # 2026-11-10(台账 Entry #324,**C-2′ 裁决** ⇒ **取代** Entry #307 C-2 的「枚举式」):
  #   **spec 侧改为 glob 全量 + 显式排除表(当前为空)**。
  #   判据:冻结对象 = `openspec/specs/` 下**任何** capability spec ——
  #     理由 = **目录语义**由归档命令保证(⏸ 暂存归档**必须** `--skip-specs`
  #     ⇒ 该目录只可能容纳**已交付**能力的义务文本)。
  #   ⇒ 「记录类 main spec」属**分类错误**(判决记录在 `changes/archive/` 与测试输出,
  #      **不在** `openspec/specs/`)⇒ **排除表当前为空**。
  #   ⚠️ **Purpose = TBD 不是排除信号** —— 实测 8 份 TBD 中 **3 份已在冻结集**
  #      (`cep-poc-spike-verification` / `cep-stub-cache-core` / `cep-vc-mapping-contract`)
  #      且正文均为 SHALL 义务文本 ⇒ 拿它当闸门会**追溯宣判 3 个已冻结成员不合规**。
  #      ⇒ Purpose/Scenario 达标度是**正交轴**(独立 remediation 批次),⛔ 不阻断冻结。
  #   ⚠️ 为何 glob 而非逐条枚举:⭐ `--update` 是**全量重算**,`artifacts` 里无论如何都有
  #      逐文件 sha256 ⇒ 枚举的唯一收益只剩「逐文件 opt-out 粒度」,而这用一个空的
  #      `exclusions` 数组即可零成本买到;且枚举有**僵尸登记盲区**(下方反向校验段)。
  #   唯一可排除情形**须显式登记**(如 `ADR-CEP-NUMBER-MAP` 那种「每新增 ADR 必加行」
  #      的例行维护型载体 —— ⚠️ 它走 **ADR glob**,与 spec glob 是**两个独立面**)。
  # 归档后**系统契约**落在 main spec —— 同样纳入冻结(否则契约面可被静默改写)
  "openspec/specs/cep-core-contracts/spec.md"
  # 2026-10-25(Entry #55):队列 #2 归档 → delta capability 落在 main spec —— 同样纳入冻结
  "openspec/specs/cep-poc-spike-verification/spec.md"
  # 2026-10-25(Entry #61):队列 #3 归档 → 2 个 delta capability 落在 main spec —— 同样纳入冻结
  #   (同 Entry #56 先例:契约面被静默改写 = 判据退化,必须走授权闸)
  "openspec/specs/cep-vc-mapping-contract/spec.md"
  "openspec/specs/cep-stub-cache-core/spec.md"
  # 2026-11-09(Entry #312):`cpptlm-cep-stub-cache-chstream-surface` 轨 A 已归档(Entry #305)
  #   ⇒ 新建 main spec `cep-stub-cache-chstream-surface` 属**已交付能力的契约面** ⇒ 纳入冻结集。
  #   ⚠️ 先例:同类 `openspec/specs/cep-stub-cache-core/spec.md` 已在集内 ⇒ 不纳入则「治理不对等」永久化。
  #   ⚠️ **只增这一条**:其余 26 份 `cep-*` main spec 的「未纳入」登记属 scope registry(T3),⛔ 不在此扩。
  #   ⚠️ 冻结前该 spec 已**达标化**(Purpose 由归档占位补齐 + 悬空「本 delta」自引用与悬空路径锚点修正,
  #      per Entry #312 ②)⇒ 冻结的是**成品契约面**,⛔ 不是占位半成品。
  "openspec/specs/cep-stub-cache-chstream-surface/spec.md"
)

# ── 台账 entry id 的唯一口径(2026-11-19 收紧)────────────────────────────
#   ⚠️ 原口径 `grep "^### "` 把台账 Entry **正文里的 `###` 级子标题**也当成合法授权条目
#      (实测台账 681 个 `###` 标题中仅 504 个是 `### Entry #`,另 177 个混入可选集合),
#      例如 `### 验收（AC-1 ~ AC-8）` / `### 授权明细(逐处枚举,超出即越权)` ——
#      标题本身就带「授权 / 验收」字样 ⇒ `--authorized-by "验收（AC-1 ~ AC-8）"`
#      可直接冒充授权条目 ⇒ 授权闸形同虚设(per 台账 Entry #506「顺带发现」)。
#   ⇒ 只认 `### Entry #<数字>` 这一种形态。**shell / embedded python / ADR 结构闸三处同口径**。
LEDGER="${CEP_FREEZE_LEDGER:-.rddf/roadmap/objectives/obj-cep-platform.md}"

entry_ids_of() {  # $1 = 台账路径
  grep -E "^### Entry #[0-9]+ " "$1" | sed 's/^### //; s/ *—.*$//'
}

is_authorized_entry() {  # $1 = 台账路径, $2 = 候选 id
  entry_ids_of "$1" | grep -qxF "$2"
}

# 反向自测(只验绿 = 没验):闸必须拒绝「冒充 / 静默缩小 / 纸面排除」
if [ "${1:-}" = "--self-test" ]; then
  _st_base="${TMPDIR:-/tmp/opencode}"; mkdir -p "$_st_base" 2>/dev/null || true
  _st_dir="$(mktemp -d "$_st_base/cep_freeze_selftest.XXXXXX")" || exit 2
  trap 'rm -rf "$_st_dir"' EXIT
  # ── 假台账 A:真 entry + 2 个正文子标题(冒充用)──────────────────────
  cat > "$_st_dir/ledger_a.md" <<'STEOF'
## 台账
### Entry #1 — 真 entry(授权记录)
正文
### 验收（AC-1 ~ AC-8）
正文
### 授权明细(逐处枚举,超出即越权)
正文
STEOF
  # ── 假台账 B:含**排除声明行**(D-1a 的声明源)────────────────────────
  cat > "$_st_dir/ledger_b.md" <<'STEOF'
## 台账
### Entry #2 — 排除授权(测试用)
- 冻结面排除 `openspec/specs/cep-core-contracts/spec.md`:反向自测用排除
STEOF
  _st_rc=0

  # ── ① 真 entry id 被接受 ───────────────────────────────────────────
  if is_authorized_entry "$_st_dir/ledger_a.md" "Entry #1"; then
    echo "  ✓ 自测①:真 entry id 'Entry #1' 被接受"
  else
    echo "  ✗ 自测失败①:真 entry id 'Entry #1' 未被接受(闸误伤)"; _st_rc=1
  fi
  # ── ② 正文子标题**不得**冒充授权条目 ───────────────────────────────
  for _fake in "验收（AC-1 ~ AC-8）" "授权明细(逐处枚举,超出即越权)"; do
    if is_authorized_entry "$_st_dir/ledger_a.md" "$_fake"; then
      echo "  ✗ 自测失败②:正文子标题 '$_fake' 被当成合法授权条目(闸无牙)"; _st_rc=1
    else
      echo "  ✓ 自测②:正文子标题 '$_fake' 被拒绝"
    fi
  done
  # ── ③ 真实台账的 id 形态合规 ───────────────────────────────────────
  if [ -f "$LEDGER" ]; then
    _st_real="$(entry_ids_of "$LEDGER")"
    if [ -z "$_st_real" ]; then
      echo "  ✗ 自测失败③:真实台账解析不到任何 Entry id"; _st_rc=1
    elif printf '%s\n' "$_st_real" | grep -qvE '^Entry #[0-9]+$'; then
      echo "  ✗ 自测失败③:真实台账解析出非 'Entry #<数字>' 形态的 id"; _st_rc=1
    else
      echo "  ✓ 自测③:真实台账 $(printf '%s\n' "$_st_real" | wc -l) 个 id 全部为 'Entry #<数字>' 形态"
    fi
  fi

  # ── ④ C-1b:未获授权的**移出冻结面** ⇒ 拒绝写盘 ─────────────────────
  _st_man4="$_st_dir/man4.json"
  python3 - "$_st_man4" <<'STEOF'
import json, sys
json.dump({"globs": [], "artifact_count": 1, "exclusions": [],
           "artifacts": {"docs/adr/ADR-CEP-DOES-NOT-EXIST.md": "0" * 64}},
          open(sys.argv[1], "w"), indent=2)
STEOF
  _st_sha_before="$(sha256sum "$_st_man4" | cut -d' ' -f1)"
  if CEP_FREEZE_MANIFEST="$_st_man4" CEP_FREEZE_LEDGER="$_st_dir/ledger_a.md" \
       bash "$0" --update --authorized-by "Entry #1" > "$_st_dir/r4.log" 2>&1; then
    echo "  ✗ 自测失败④:未获授权的移出被放行(闸无牙)"; _st_rc=1
  else
    _st_sha_after="$(sha256sum "$_st_man4" | cut -d' ' -f1)"
    if grep -q "拒绝写盘" "$_st_dir/r4.log" && [ "$_st_sha_before" = "$_st_sha_after" ]; then
      echo "  ✓ 自测④:未获授权的移出被拒,且清单未被写盘"
    else
      echo "  ✗ 自测失败④:拒绝理由或「不写盘」未成立"; _st_rc=1
      tail -4 "$_st_dir/r4.log" | sed 's/^/      /'
    fi
  fi

  # ── ⑤ D-1a:声明行排除 ⇒ **差分**地少一件 + 排除表落一条 ───────────
  #   ⚠️ 用**差分**断言(critic NIT 2:原断言在假清单 `globs:[]` 下是空真):
  #   同一份脚本 globs,跑两次 --update —— 无排除 vs 有排除 ⇒ 产物集恰差 1 件,
  #   且差的那一件正是被排除的路径。
  _st_man5a="$_st_dir/man5a.json"; _st_man5b="$_st_dir/man5b.json"
  for _f in "$_st_man5a" "$_st_man5b"; do
    python3 - "$_f" <<'STEOF'
import json, sys
json.dump({"globs": [], "artifact_count": 0, "exclusions": [], "artifacts": {}},
          open(sys.argv[1], "w"), indent=2)
STEOF
  done
  CEP_FREEZE_MANIFEST="$_st_man5a" CEP_FREEZE_LEDGER="$_st_dir/ledger_a.md" \
    bash "$0" --update --authorized-by "Entry #1" > "$_st_dir/r5a.log" 2>&1
  if CEP_FREEZE_MANIFEST="$_st_man5b" CEP_FREEZE_LEDGER="$_st_dir/ledger_b.md" \
       bash "$0" --update --authorized-by "Entry #2" > "$_st_dir/r5.log" 2>&1; then
    if python3 - "$_st_man5a" "$_st_man5b" <<'STEOF'
import json, sys
a = json.load(open(sys.argv[1])).get("artifacts", {})
b = json.load(open(sys.argv[2]))
ex = b.get("exclusions", [])
p = "openspec/specs/cep-core-contracts/spec.md"
only_a = set(a) - set(b.get("artifacts", {}))
ok = (len(ex) == 1 and ex[0].get("path") == p and ex[0].get("ledger_entry") == "Entry #2"
      and only_a == {p} and p not in b.get("artifacts", {})
      and b.get("artifact_count") == len(b.get("artifacts", {}))
      and b.get("artifact_count") == len(a) - 1)
sys.exit(0 if ok else 1)
STEOF
    then
      echo "  ✓ 自测⑤:声明行排除生效(排除表落 1 条 · 产物集**差分**恰少该件 · 计数自洽)"
    else
      echo "  ✗ 自测失败⑤:声明行排除未按预期生效"; _st_rc=1
      tail -4 "$_st_dir/r5.log" | sed 's/^/      /'
    fi
  else
    echo "  ✗ 自测失败⑤:带排除声明的 --update 报错"; _st_rc=1
    tail -4 "$_st_dir/r5.log" | sed 's/^/      /'
  fi

  # ── ⑥ C-1a:清单的 globs ≠ 脚本的 globs ⇒ 红(缩小范围须显式可见)────
  if [ -f "$MANIFEST" ]; then
    _st_man6="$_st_dir/man6.json"
    python3 - "$MANIFEST" "$_st_man6" <<'STEOF'
import json, sys
m = json.load(open(sys.argv[1]))
m["globs"] = list(m.get("globs", []))[:-1]   # 少一条 ⇒ 与脚本当前数组不一致
json.dump(m, open(sys.argv[2], "w"), indent=2)
STEOF
    if CEP_FREEZE_MANIFEST="$_st_man6" bash "$0" > "$_st_dir/r6.log" 2>&1; then
      echo "  ✗ 自测失败⑥:清单 globs 与脚本不一致仍放行(缩小范围无牙)"; _st_rc=1
    elif grep -q "冻结范围与清单不一致" "$_st_dir/r6.log"; then
      echo "  ✓ 自测⑥:清单 globs 与脚本不一致 ⇒ 报红(缩小范围须显式可见)"
    else
      echo "  ✗ 自测失败⑥:报红理由非预期"; _st_rc=1
      tail -4 "$_st_dir/r6.log" | sed 's/^/      /'
    fi
  fi

  # ── ⑦ D-1a:排除项**仍在冻结集里** ⇒ 红(纸面排除不算排除)──────────
  if [ -f "$MANIFEST" ]; then
    _st_man7="$_st_dir/man7.json"
    python3 - "$MANIFEST" "$_st_dir/ledger_a.md" "$_st_man7" <<'STEOF'
import json, sys
m = json.load(open(sys.argv[1]))
p = sorted(m.get("artifacts", {}))[0]
m["exclusions"] = [{"path": p, "reason": "反向自测用", "ledger_entry": "Entry #1"}]
json.dump(m, open(sys.argv[3], "w"), indent=2)
STEOF
    if CEP_FREEZE_MANIFEST="$_st_man7" CEP_FREEZE_LEDGER="$_st_dir/ledger_a.md" \
         bash "$0" > "$_st_dir/r7.log" 2>&1; then
      echo "  ✗ 自测失败⑦:纸面排除(仍在冻结集里)未报红"; _st_rc=1
    elif grep -q "仍在冻结集里" "$_st_dir/r7.log"; then
      echo "  ✓ 自测⑦:纸面排除(产物仍在冻结集里)⇒ 报红"
    else
      echo "  ✗ 自测失败⑦:报红理由非预期"; _st_rc=1
      tail -4 "$_st_dir/r7.log" | sed 's/^/      /'
    fi
  fi

  # ── ⑧ SHOULD_FIX 1:清单注入**无声明行**的排除项 ⇒ 红 ────────────────
  #   清单本身不在冻结集内 ⇒ 手工改清单注入排除项可绕过其余判据
  #   ⇒ 必须验「授权 Entry 正文里确有该路径的排除声明行」。
  if [ -f "$MANIFEST" ]; then
    _st_man8="$_st_dir/man8.json"
    python3 - "$MANIFEST" "$_st_man8" <<'STEOF'
import json, sys
m = json.load(open(sys.argv[1]))
# 挑一份真实存在、且在 glob 范围内的 cep-* spec
p = sorted(k for k in m.get("artifacts", {}) if k.startswith("openspec/specs/cep-"))[0]
m["artifacts"].pop(p, None)                      # 移出冻结集(否则触发「仍在冻结集里」而非本条)
m["artifact_count"] = len(m["artifacts"])
m["exclusions"] = [{"path": p, "reason": "反向自测:无声明行的注入", "ledger_entry": "Entry #1"}]
json.dump(m, open(sys.argv[2], "w"), indent=2)
STEOF
    if CEP_FREEZE_MANIFEST="$_st_man8" bash "$0" > "$_st_dir/r8.log" 2>&1; then
      echo "  ✗ 自测失败⑧:清单注入无声明行的排除项未报红"; _st_rc=1
    elif grep -q "没有对应的排除声明行" "$_st_dir/r8.log"; then
      echo "  ✓ 自测⑧:清单注入无声明行的排除项 ⇒ 报红(纸面排除堵死)"
    else
      echo "  ✗ 自测失败⑧:报红理由非预期"; _st_rc=1
      tail -4 "$_st_dir/r8.log" | sed 's/^/      /'
    fi
  fi

  # ── ⑨ SHOULD_FIX 2:Entry 正文**仅顺带提及**路径 ⇒ 不得授权移出 ──────
  #   台账正文大量引用 ADR / spec 全路径 ⇒ 子串「点名」= 意外授权。
  _st_led9="$_st_dir/ledger_c.md"
  cat > "$_st_led9" <<'STEOF'
## 台账
### Entry #3 — 顺带提及(非声明)
本次处理了 docs/adr/ADR-CEP-DOES-NOT-EXIST.md 这个文件的移除问题,顺带记一笔。
STEOF
  _st_man9="$_st_dir/man9.json"
  python3 - "$_st_man9" <<'STEOF'
import json, sys
json.dump({"globs": [], "artifact_count": 1, "exclusions": [],
           "artifacts": {"docs/adr/ADR-CEP-DOES-NOT-EXIST.md": "0" * 64}},
          open(sys.argv[1], "w"), indent=2)
STEOF
  if CEP_FREEZE_MANIFEST="$_st_man9" CEP_FREEZE_LEDGER="$_st_led9" \
       bash "$0" --update --authorized-by "Entry #3" > "$_st_dir/r9.log" 2>&1; then
    echo "  ✗ 自测失败⑨:仅顺带提及路径即被当作移出授权(意外授权)"; _st_rc=1
  else
    echo "  ✓ 自测⑨:仅顺带提及路径不构成授权(须按移出声明行)"
  fi

  # ── ⑩ SHOULD_FIX 2 的正向面:按移出声明行 ⇒ 放行 ────────────────────
  _st_led10="$_st_dir/ledger_d.md"
  cat > "$_st_led10" <<'STEOF'
## 台账
### Entry #4 — 移出授权
- 冻结面移出 `docs/adr/ADR-CEP-DOES-NOT-EXIST.md`:该文件已删除
STEOF
  _st_man10="$_st_dir/man10.json"
  python3 - "$_st_man10" <<'STEOF'
import json, sys
json.dump({"globs": [], "artifact_count": 1, "exclusions": [],
           "artifacts": {"docs/adr/ADR-CEP-DOES-NOT-EXIST.md": "0" * 64}},
          open(sys.argv[1], "w"), indent=2)
STEOF
  if CEP_FREEZE_MANIFEST="$_st_man10" CEP_FREEZE_LEDGER="$_st_led10" \
       bash "$0" --update --authorized-by "Entry #4" > "$_st_dir/r10.log" 2>&1; then
    echo "  ✓ 自测⑩:按移出声明行的授权 ⇒ 放行(授权通道仍通)"
  else
    echo "  ✗ 自测失败⑩:带移出声明行的 --update 被拒(授权通道被堵死)"; _st_rc=1
    tail -4 "$_st_dir/r10.log" | sed 's/^/      /'
  fi

  # ── ⑪ 语法**示例行**不得产生排除项(2026-11-19 实测事故)────────────
  #   台账 Entry 里常会**讲解**声明语法(示例行长得像真声明)⇒ 解析器若照单全收,
  #   清单会被塞进 `<相对路径>` 这类假排除项。⇒ 声明须「形态合法 + 落在相关集合内」。
  _st_led11="$_st_dir/ledger_e.md"
  cat > "$_st_led11" <<'STEOF'
## 台账
### Entry #5 — 仅讲解语法(无真实声明)
授权面有两种声明行,格式如下(这是**示例**,不是真实声明):
- 冻结面移出 `<相对路径>`:<理由>     # 该路径离开冻结面
- 冻结面排除 `<相对路径>`:<理由>     # 该路径不进冻结面
STEOF
  _st_man11="$_st_dir/man11.json"
  python3 - "$_st_man11" <<'STEOF'
import json, sys
json.dump({"globs": [], "artifact_count": 0, "exclusions": [], "artifacts": {}},
          open(sys.argv[1], "w"), indent=2)
STEOF
  if CEP_FREEZE_MANIFEST="$_st_man11" CEP_FREEZE_LEDGER="$_st_led11" \
       bash "$0" --update --authorized-by "Entry #5" > "$_st_dir/r11.log" 2>&1; then
    if python3 - "$_st_man11" <<'STEOF'
import json, sys
m = json.load(open(sys.argv[1]))
sys.exit(0 if not m.get("exclusions") else 1)
STEOF
    then
      echo "  ✓ 自测⑪:语法示例行**未**产生排除项(声明须形态合法 + 落在相关集合内)"
    else
      echo "  ✗ 自测失败⑪:语法示例行被当成真声明 ⇒ 清单被污染"; _st_rc=1
      tail -4 "$_st_dir/r11.log" | sed 's/^/      /'
    fi
  else
    echo "  ✗ 自测失败⑪:仅含示例行的 --update 报错"; _st_rc=1
    tail -4 "$_st_dir/r11.log" | sed 's/^/      /'
  fi

  [ "$_st_rc" -eq 0 ] && echo "  ✓ 授权闸反向自测通过" || echo "  ✗ 授权闸反向自测失败"
  exit "$_st_rc"
fi

if [ "${1:-}" = "--update" ]; then
  # ⚠️ **授权闸门**(2026-10-22 代码复审 B5):`--update` 无条件重算全部 hash 并写回,
  #    等于「一次命令即可把任何未授权的冻结产物修改吸收为已冻结」。
  #    故要求提供台账 entry id 并核对其确实存在于台账 §11 —— 台账 entry 即**授权记录**。
  # ⚠️ **必须提供且必须精确命中 entry 标题**(2026-10-22 复审 NB2):
  #    初版「缺省只警告 + grep 子串匹配」可被 `--authorized-by "a"` 伪造 → 闸门形同虚设。
  AUTH=""
  if [ "${2:-}" = "--authorized-by" ]; then AUTH="${3:-}"; fi
  if [ -z "$AUTH" ]; then
    echo "  ✗ 缺少 --authorized-by <台账 entry 标题> —— **拒绝写盘**"
    echo "    · 改冻结产物必须先在台账 §11 追加 entry(授权记录),再 --update"
    echo "    · 用法: bash scripts/test/cep_freeze_check.sh --update --authorized-by \"Entry #N\""
    exit 1
  fi
  if [ ! -f "$LEDGER" ]; then
    echo "  ✗ 找不到台账 $LEDGER —— 无法核验授权"
    exit 1
  fi
  # 精确匹配:台账里必须存在 **entry id 完全等于** $AUTH 的 entry
  #   (entry id = '### Entry #' + 数字;Entry 正文里的 `###` 级子标题**不**是合法 id)
  if ! is_authorized_entry "$LEDGER" "$AUTH"; then
    echo "  ✗ 授权校验失败:台账 $LEDGER 中没有 id 为 '$AUTH' 的 entry"
    echo "    · 现有 entry id:"
    entry_ids_of "$LEDGER" | sed 's/^/      /'
    exit 1
  fi
  echo "  ✓ 授权校验通过:台账 entry '$AUTH'"
  python3 - "$MANIFEST" "$ROOT" "$AUTH" "$LEDGER" "${FROZEN_GLOBS[@]}" << 'PYEOF'
import glob, hashlib, json, os, re, subprocess, sys

manifest_path, root, auth, ledger_rel = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
globs = sys.argv[5:]

# ── 授权 Entry 的正文(C-1b / D-1a 的判据面)──────────────────────────
#   台账 entry 即**授权记录**(本闸既有原则)⇒ 授权范围也由它界定:
#   · 「哪些产物被移出冻结面」必须在该 Entry 正文里**按声明行**登记;
#   · 「哪些产物被**排除**」必须在该 Entry 正文里**按声明行**登记。
def entry_body(entry_id):
    lp = os.path.join(root, ledger_rel)
    if not os.path.isfile(lp):
        return ""
    txt = open(lp, encoding="utf-8").read()
    m = re.search(r"^### " + re.escape(entry_id) + r"\b.*?(?=^### |\Z)", txt, re.S | re.M)
    return m.group(0) if m else ""

BODY = entry_body(auth)

# 声明行格式(严格):
#   · `- 冻结面排除 `<相对路径>`:<理由>` ⇒ 该路径**不进**冻结集(glob 命中但豁免)
#   · `- 冻结面移出 `<相对路径>`:<理由>` ⇒ 该路径**离开**冻结面(文件被删 / 范围收窄)
#     ⚠️ **不得**用「Entry 正文里出现该路径」当作授权(2026-11-19 critic SHOULD_FIX 2):
#        台账正文大量引用 ADR / spec 全路径 ⇒ 顺带提及即构成授权 ⇒ 意外放行。
#        「移出」与「排除」同形态声明 ⇒ 与 entry-id 侧的精确度对等。
#   ⚠️ **声明必须「形态合法 + 落在相关集合内」才生效**(2026-11-19 实测事故):
#      Entry 正文里**讲解语法用的示例行**(如 `- 冻结面排除 `<相对路径>`:<理由>`)
#      会被同一个正则当成真声明 ⇒ 清单被塞进假排除项。
#      ⇒ ① 路径形态须合法(不含尖括号/空格,须含 `/`);② 排除声明的路径须**在当前
#         glob 展开集内**;③ 移出声明的路径须**在本次 `removed` 集内**。
#         不满足 ⇒ **不生效**(声明不授予任何权限 ⇒ fail-safe)。
DECL_EXCL_RE = re.compile(r"^-\s*冻结面排除\s*`([^`]+)`\s*[:：]\s*(\S.*)$", re.M)
DECL_OUT_RE = re.compile(r"^-\s*冻结面移出\s*`([^`]+)`\s*[:：]\s*(\S.*)$", re.M)
PATH_SHAPE = re.compile(r"^[A-Za-z0-9._][A-Za-z0-9._/-]*$")
DECLARED = {p: reason.strip() for p, reason in DECL_EXCL_RE.findall(BODY)}
DECLARED_OUT = {p: reason.strip() for p, reason in DECL_OUT_RE.findall(BODY)}

files = []
for g in globs:
    files += glob.glob(os.path.join(root, g), recursive=True)
files = sorted(set(os.path.relpath(f, root) for f in files))

def sha256(rel):
    h = hashlib.sha256()
    with open(os.path.join(root, rel), "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

# ── 读旧清单(C-2:让「本次吸收了什么」**可见**)────────────────────────
#   ⚠️ 旧清单此前**从不读** ⇒ `--update` 的输出只有一行计数 ⇒ 操作者看不到
#      「刚刚吸收了哪几个文件 / 丢了哪几个文件」。
old_arts = {}
old_excl = []
try:
    with open(os.path.join(root, manifest_path), encoding="utf-8") as f:
        _old = json.load(f)
    old_arts = _old.get("artifacts", {}) or {}
    old_excl = _old.get("exclusions", []) or []
except Exception:
    old_arts = {}
    old_excl = []

# ── 排除表(D-1a:声明 → 减法 → 真被排除)──────────────────────────────
#   ⚠️ 旧实现:`exclusions` 是**只写字段**(`--update` 恒写 `[]`),从无声明源、
#      也从不参与产物集计算 ⇒ 文档宣称的「唯一可豁免通道」实际不存在。
#   ⇒ 声明源 = **授权 Entry 正文的声明行**(台账 = 唯一授权源,零第二真源);
#      条目形态沿用 Entry #324 已定的 `{path, reason, ledger_entry}`。
#   ⚠️ **排除表每次重算**(不盲目累积):
#      · 旧条目中路径**仍在当前 glob 展开集内** ⇒ 保留(此前已获授权的长期豁免);
#      · 旧条目中路径已不存在 ⇒ **丢弃**(文件已消失 ⇒ 豁免已无意义);
#      · 本次 Entry 的**有效**排除声明 ⇒ 追加。
#      ⇒ 避免「一条无效排除项永久沉淀在清单里」(2026-11-19 实测事故的修复面)。
SCOPE = set(files)
EXCL = {}
for e in old_excl:
    if isinstance(e, dict) and e.get("path") and e["path"] in SCOPE:
        EXCL[e["path"]] = {"path": e["path"], "reason": e.get("reason", ""),
                           "ledger_entry": e.get("ledger_entry", "")}
for p, reason in DECLARED.items():
    if p in SCOPE and PATH_SHAPE.match(p):
        EXCL[p] = {"path": p, "reason": reason, "ledger_entry": auth}
    else:
        print(f"  ! 排除声明**未生效**(路径不在当前冻结范围内,或形态非法): {p}")

# 减法:被排除的产物**不进**冻结集
files = [f for f in files if f not in EXCL]

new_arts = {rel: sha256(rel) for rel in files}
added = sorted(set(new_arts) - set(old_arts))
removed = sorted(set(old_arts) - set(new_arts))
changed = sorted(r for r in (set(old_arts) & set(new_arts)) if old_arts[r] != new_arts[r])

# ── C-1b:**拒绝静默丢产物** ──────────────────────────────────────────
#   清单的 `artifacts` 是**快照**:`--update` 重算 ⇒ 移出范围的产物**静默失去冻结**
#   且无任何判据报红(verify 只遍历 `artifacts`)。
#   ⇒ 移出必须被**授权 Entry 显式声明**,二选一:
#      · 「移出声明行」(`- 冻结面移出 \`<路径>\`:<理由>`)—— 该路径就此离开冻结面;
#      · 「排除声明行」(`- 冻结面排除 \`<路径>\`:<理由>`)—— 该路径改为长期豁免。
#      ⚠️ 不得用「正文里出现该路径」当授权(critic SHOULD_FIX 2)—— 台账正文大量引用
#         ADR / spec 全路径,顺带提及即构成授权。
#      ⚠️ 移出声明**只对本次 `removed` 里的路径生效**(形态合法 ∧ 确在移出集内)
#         ⇒ 讲解语法用的示例行不授予任何权限(2026-11-19 实测事故)。
DECLARED_OUT_EFF = {p for p in DECLARED_OUT if p in removed and PATH_SHAPE.match(p)}
unauthorized = [r for r in removed if r not in EXCL and r not in DECLARED_OUT_EFF]

try:
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root,
                                    stderr=subprocess.DEVNULL).decode().strip()
except Exception:
    head = ""

try:
    frozen_at = subprocess.check_output(["git", "log", "-1", "--format=%ad", "--date=short"],
                                         cwd=root, stderr=subprocess.DEVNULL).decode().strip()
except Exception:
    frozen_at = ""

# ── 差集回显(C-2)────────────────────────────────────────────────────
print(f"[cep_freeze_check] 清单差集(相对旧清单):新增 {len(added)} · 移出 {len(removed)} · hash 变更 {len(changed)}")
for r in added:
    print(f"    + 新增冻结产物: {r}")
for r in removed:
    tag = "已声明排除" if r in EXCL else ("已声明移出" if r in DECLARED_OUT_EFF else "⚠️ 未获授权")
    print(f"    - 移出冻结面: {r}  [{tag}]")
for r in changed:
    print(f"    ~ hash 变更: {r}")

if unauthorized:
    print("  ✗ 拒绝写盘:以下产物将**移出冻结面**,但授权 Entry 未按声明行登记:")
    for r in unauthorized:
        print(f"      · {r}")
    print(f"    · 授权 entry = {auth}")
    print("    · 修法:在该 Entry 正文里加声明行:")
    print("             `- 冻结面移出 `<相对路径>`:<理由>`")
    print("    · 缩小冻结面 = 撤销既有保护 ⇒ 必须留授权痕迹")
    sys.exit(1)

payload = {
    "frozen_at": frozen_at or "",
    "frozen_at_commit": head,
    "authorized_by": auth or "",
    "mechanism": "M1 产物冻结先于解读(per obj-cep-platform §9.2)",
    # ⚠️ **只述结构,不述成员**(2026-11-19 C-4):旧 note 手写了「fabric.json 不冻结」
    #   等**成员级结论**,而 `fabric.json` 早已入冻结集 ⇒ 每次 `--update` 都把错话
    #   重新写回清单。成员级事实一律以本清单 `globs`/`artifacts` 为准。
    "note": ("冻结面 = 本清单的 `globs`(范围)× `artifacts`(逐件 sha256);"
             "**成员级结论一律以本清单为准**,此处不复述(复述即漂移源)。"
             "两个 checker 脚本自身不冻结(checker 必须可修,且不承载契约语义)。"
             "冻结后若需修改,须走 §11 台账 scope-change / adr-amendment entry;"
             "`--update` 需带 `--authorized-by <台账 entry id>` 才有授权记录。"),
    "artifact_count": len(files),
    "globs": list(globs),
    "artifacts": new_arts,
    # ══ 2026-11-10(C-2′ / Entry #324):**排除表**(唯一可豁免「glob 全量冻结」的通道)══
    #   语义:冻结面 = `openspec/specs/cep-*/spec.md` **全集**;本数组**当前为空**。
    #   条目形态:`{"path": <相对路径>, "reason": <为何不冻结>, "ledger_entry": <台账 Entry #N>}`
    #   ⇒ 声明源 = 授权 Entry 正文的声明行;校验见 `cep_freeze_check.sh` 的
    #     exclusion-integrity 段(**空数组 = 全量冻结**)
    "exclusions": [EXCL[p] for p in sorted(EXCL)],
}

with open(os.path.join(root, manifest_path), "w", encoding="utf-8") as f:
    json.dump(payload, f, ensure_ascii=False, indent=2, sort_keys=True)
    f.write("\n")
print(f"[cep_freeze_check] 已重新生成冻结清单: {len(files)} 个产物 -> {manifest_path}"
      + (f"（排除 {len(EXCL)} 项）" if EXCL else ""))
PYEOF
  exit $?
fi

if [ ! -f "$MANIFEST" ]; then
  echo "  ✗ 冻结清单不存在: $MANIFEST"
  echo "    生成: bash scripts/test/cep_freeze_check.sh --update"
  exit 1
fi

python3 - "$MANIFEST" "$ROOT" "${FROZEN_GLOBS[@]}" << 'PYEOF'
import hashlib, json, os, sys

manifest_path = sys.argv[1]
root = sys.argv[2]
script_globs = sys.argv[3:]
with open(os.path.join(root, manifest_path), encoding="utf-8") as f:
    man = json.load(f)

# ── 冻结面**范围一致性**(C-1a,2026-11-19)─────────────────────────────
#   脚本当前的 `FROZEN_GLOBS` 与清单记录的 `globs` 必须逐条相等。
#   ⚠️ 为何必须有:清单的 `artifacts` 是**快照** —— 若有人改了脚本的 glob 数组
#      (缩小冻结范围)而**未** `--update`,清单仍按旧范围判定(绿);而一旦 `--update`,
#      旧 manifest **从不读** ⇒ 新清单按新范围重算 ⇒ 被移出范围的产物**静默失去冻结**,
#      且**没有任何判据**会报红(verify 只遍历 `artifacts`)。
#   ⇒ 范围变更必须先在这里**显式变红**:缩小范围 = 须授权 + 须 `--update` 重生成清单。
man_globs = man.get("globs", [])
if set(script_globs) != set(man_globs):
    for g in sorted(set(script_globs) - set(man_globs)):
        print(f"  \033[0;31m✗\033[0m 脚本**新增**冻结范围(清单未记录): {g}")
    for g in sorted(set(man_globs) - set(script_globs)):
        print(f"  \033[0;31m✗\033[0m 脚本**移除**冻结范围(清单仍记录): {g}")
    print(f"  \033[0;31m✗\033[0m 冻结范围与清单不一致(脚本 {len(script_globs)} 条 vs 清单 {len(man_globs)} 条)")
    print("    · 范围变更须先在台账登记授权,再重生成清单:")
    print("      bash scripts/test/cep_freeze_check.sh --update --authorized-by \"Entry #N\"")
    sys.exit(1)
print(f"  \033[0;32m✓\033[0m 冻结范围一致性:脚本 {len(script_globs)} 条 glob == 清单记录")

def sha256(rel):
    h = hashlib.sha256()
    with open(os.path.join(root, rel), "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

arts = man.get("artifacts", {})
# 排除表路径集(D-1a:排除项**必须真被排除** ⇒ drift 判定须豁免它们,
#   否则「已声明排除」会被 drift 判成「新增未登记的冻结范围内产物」⇒ 自相矛盾)
_excl_paths = {e.get("path") for e in (man.get("exclusions") or [])
               if isinstance(e, dict) and e.get("path")}
drift, missing, changed = [], [], []
for rel, want in sorted(arts.items()):
    full = os.path.join(root, rel)
    if not os.path.exists(full):
        missing.append(rel)
        continue
    got = sha256(rel)
    if got != want:
        changed.append((rel, want[:12], got[:12]))

# 新增但未登记的冻结范围内产物(例如新加了一份 ADR)
import glob
globs = man.get("globs", [])
for g in globs:
    for p in glob.glob(os.path.join(root, g), recursive=True):
        rel = os.path.relpath(p, root)
        if rel not in arts and rel not in _excl_paths:
            drift.append(rel)

if missing or changed or drift:
    for r in missing:
        print(f"  \033[0;31m✗\033[0m 冻结产物**缺失**: {r}")
    for r, w, g in changed:
        print(f"  \033[0;31m✗\033[0m 冻结产物**已被修改**: {r}  (期望 {w}…, 实得 {g}…)")
    for r in drift:
        print(f"  \033[0;31m✗\033[0m 新增未登记的冻结范围内产物: {r}")
    print(f"  \033[0;31m✗\033[0m M1 冻结校验失败:{len(missing)} 缺失 / {len(changed)} 被改 / {len(drift)} 未登记")
    sys.exit(1)

# ── 冻结范围 **exclusion-integrity**(反向校验,**2026-11-10 C-2′ / 台账 Entry #324**)──
#   ⚠️ **本段已反转**:C-2.3(Entry #313)原口径 = 「新增 main spec **未登记** ⇒ 红」
#      —— 那是在**枚举式**冻结下的「知情未冻结」机制。
#   ⇒ C-2′(Entry #324)改为 **glob 全量**后,方向反转:
#      ① `openspec/specs/cep-*/spec.md` **全部**必须在 manifest(由 glob 保证)⇒
#         「未纳入冻结」**不再是默认状态**;
#      ② 唯一可豁免的路径 = **排除表**(manifest 的 `exclusions[]`),且每条**必须**
#         带 `path` + `reason` + `ledger_entry`,且 `ledger_entry` 须能在台账解析;
#      ③ 排除表条目指向不存在的 spec ⇒ 红(防僵尸);
#      ④ 排除表**当前为空** —— 出现新条目须走五步链(review diff 可见)。
#   ⚠️ **为何要反转**:枚举式下有一个**活缺陷** —— 旧段的两个集合差
#      (`spec_rels - frozen_specs - registered` / `registered - spec_rels`) **不含**
#      `registered ∩ frozen_specs` ⇒ 「**登记为未冻结、实际已冻结**」**不会被判红**
#      ⇒ 渐进枚举的中间态会产生**僵尸登记**(而 registry `:3` 明言本表存在的意义
#      是「杜绝双真源」)⇒ glob 化后 registry 退役 ⇒ 盲区**自然消失**。
#   ⚠️ 脚本自身**明确不冻结**(见文件头 `:16-19`,checker 必须可修)⇒
#      **范围记录**由 `manifest.globs` 承担(`--update` 已写入)⇒ 范围变更进入 review diff。
import glob as _glob
import re as _re
# 台账 entry id 解析(与 shell 侧的 `ENTRY_IDS` 同口径 —— 精确 id = `### ` 之后、` — ` 之前)
_LEDGER_REL = ".rddf/roadmap/objectives/obj-cep-platform.md"
_LEDGER_ABS = os.path.join(root, _LEDGER_REL)
LEDGER_IDS = set()
if os.path.exists(_LEDGER_ABS):
    # 与 shell 侧 `entry_ids_of` **同口径**:只认 `### Entry #<数字>` ——
    #   Entry 正文里的 `###` 级子标题(「授权明细」「验收」等)**不**是合法 entry id
    #   (per 台账 Entry #506「顺带发现」:旧写法 `startswith("### ")` 让 177 个子标题
    #    混入可选集合 ⇒ 可被 `--authorized-by` 直接冒充)。
    for _ln in open(_LEDGER_ABS, encoding="utf-8"):
        _m = _re.match(r"^### (Entry #[0-9]+)\b", _ln)
        if _m:
            LEDGER_IDS.add(_m.group(1))
EXCL = man.get("exclusions", []) if isinstance(man, dict) else []
_excl_errors = []
_spec_paths = sorted(_glob.glob(os.path.join(root, "openspec/specs/cep-*/spec.md")))
_spec_rels = {os.path.relpath(p, root) for p in _spec_paths}
# ── 排除路径域 = **当前 globs 展开集**(不限于 `cep-*` spec)────────────
#   ⚠️ critic SHOULD_FIX 3:`--update` 的减法对**全部** glob 产物生效,但旧口径把
#      排除项限定在 `cep-*` spec ⇒ glob 数组注释里点名的场景(如走 ADR glob 的
#      `ADR-CEP-NUMBER-MAP` 类)一旦真声明,会「update 放行 + verify 判死」= 最差组合。
#   ⇒ 域放宽到 globs 展开集(仍保留「防僵尸」:路径必须真实存在于冻结范围内)。
_frozen_scope = set()
for _g in man.get("globs", []):
    for _p in _glob.glob(os.path.join(root, _g), recursive=True):
        _frozen_scope.add(os.path.relpath(_p, root))

# ── 排除表条目的**授权痕迹**(critic SHOULD_FIX 1,2026-11-19)──────────
#   ⚠️ 清单本身**不在冻结集内**(既有设计)⇒ 手工改清单注入一条
#      `{path, reason, ledger_entry: "<任一真实 entry id>"}` 可通过其余全部机器判据
#      ⇒ 一条**从未在任何 Entry 正文声明过**的排除会永久生效
#      = D-1a 堵掉的「纸面排除」换个入口复活。
#   ⇒ 必须验:**该 Entry 正文里确有该路径的排除声明行**(与 `--update` 同口径正则)。
def _entry_body(entry_id):
    if not os.path.exists(_LEDGER_ABS):
        return ""
    _txt = open(_LEDGER_ABS, encoding="utf-8").read()
    _m = _re.search(r"^### " + _re.escape(entry_id) + r"\b.*?(?=^### |\Z)", _txt, _re.S | _re.M)
    return _m.group(0) if _m else ""

_DECL_EXCL_RE = _re.compile(r"^-\s*冻结面排除\s*`([^`]+)`\s*[:：]\s*(\S.*)$", _re.M)

for e in EXCL:
    if not isinstance(e, dict):
        _excl_errors.append(f"排除表条目不是对象: {e!r}")
        continue
    p, why, led = e.get("path"), e.get("reason"), e.get("ledger_entry")
    if not p or not why or not led:
        _excl_errors.append(f"排除表条目缺 `path`/`reason`/`ledger_entry`: {p or '<no path>'}")
        continue
    if p not in _frozen_scope:
        _excl_errors.append(f"排除表条目指向**不在冻结范围内**的路径: {p}")
    if led not in LEDGER_IDS:
        _excl_errors.append(f"排除表条目的 `ledger_entry` 在台账中解析不到: {led}")
    elif p not in {m[0] for m in _DECL_EXCL_RE.findall(_entry_body(led))}:
        _excl_errors.append(f"排除表条目在授权 Entry 正文里**没有对应的排除声明行**: {p} (entry {led})")
    # ⚠️ D-1a:排除必须**真被排除** —— 旧实现里 `exclusions` 是只写字段(恒 `[]`),
    #   从不参与产物集计算 ⇒ 「排除」只是纸面声明,产物其实仍被冻结。
    if p in arts:
        _excl_errors.append(f"排除表条目指向的产物**仍在冻结集里**(排除未生效): {p}")
if _excl_errors:
    for m in _excl_errors:
        print(f"  \033[0;31m✗\033[0m {m}")
    print(f"  \033[0;31m✗\033[0m 排除表完整性校验失败({len(_excl_errors)} 项)")
    sys.exit(1)
print(f"  \033[0;32m✓\033[0m 冻结范围 exclusion-integrity:glob 展开 {len(_spec_rels)} 份 `cep-*` main spec · 排除表 {len(EXCL)} 条(空 = 全量冻结 ✓)")

# ── provenance 自检(2026-10-22 代码复审 B4)──────────────────────────────
# 清单的 hash 与工作区一致 **不等于** 「这批产物 = frozen_at_commit 的内容」。
# 后者才是 provenance 的真命题 —— 逐个比对,不一致即**响亮报出**(不阻断:
# 「先 commit 再 --update」的两步流程会让 frozen_at_commit 天然落后 HEAD 一个 commit)。
import subprocess as _sp
try:
    head = _sp.check_output(["git", "rev-parse", "HEAD"], cwd=root,
                             stderr=_sp.DEVNULL).decode().strip()
except Exception:
    head = ""
fc = man.get("frozen_at_commit", "")
if head and fc and fc != head:
    print(f"  \033[1;33m!\033[0m 冻结清单的 frozen_at_commit({fc[:12]}) ≠ 当前 HEAD({head[:12]})")
    print(f"    · 若刚 commit 过冻结产物,须再跑一次: bash scripts/test/cep_freeze_check.sh --update --authorized-by \"Entry #N\"")
if fc:
    _bad = []
    for rel, want in sorted(arts.items()):
        try:
            blob = _sp.check_output(["git", "show", f"{fc}:{rel}"], cwd=root,
                                    stderr=_sp.DEVNULL)
        except Exception:
            _bad.append(rel)
            continue
        if hashlib.sha256(blob).hexdigest() != want:
            _bad.append(rel)
    if _bad:
        print(f"  \033[1;33m!\033[0m provenance 警告:{len(_bad)} 个产物的 hash 与 commit {fc[:12]} 的内容**不一致**")
        for r in _bad[:5]:
            print(f"      - {r}")
        print("    · 含义:清单描述的不是该 commit 的内容(通常因 --update 在 commit 之前跑)")
    else:
        print(f"  \033[0;32m✓\033[0m provenance 自检通过:{len(arts)} 个产物的 hash 与 commit {fc[:12]} 的内容一致")

# ⚠️ 变量名必须与上面 provenance 块内的 `_bad` 一致(2026-10-22 复审 S5 修)
#    —— 初版写成 `_prov_bad`,NameError 被吞 → 误走「✓ 全通过」分支
_prov_bad = _bad if fc else []
_prov_ok = not _prov_bad
if _prov_ok:
    print(f"  \033[0;32m✓\033[0m M1 产物冻结校验通过:{len(arts)} 个产物 hash 全部一致"
          f"(冻结于 {man.get('frozen_at', '?')}, commit {str(man.get('frozen_at_commit', ''))[:12]})")
else:
    print(f"  \033[1;33m!\033[0m M1 产物 hash 校验通过:{len(arts)} 个产物与工作区一致"
          f"(冻结于 {man.get('frozen_at', '?')}, commit {str(man.get('frozen_at_commit', ''))[:12]})")
    print(f"  \033[1;33m!\033[0m **但 provenance 不成立**:{len(_prov_bad)} 个产物的 hash 与该 commit 的内容不一致"
          f" —— 须在 commit 产物后重跑 --update")
    # ⚠️ 2026-10-26(Entry #71 R-6):provenance 不成立原本只 warn ⇒ rc 仍为 0
    #    ⇒ 两步授权的**第 3 步**「再 commit 清单」**无机器保证**(清单可停在错误 commit 上)。
    #    「清单描述的不是该 commit 的内容」是**真缺陷**(不是常态)⇒ 阻断。
    #    `--update` 模式**天然豁免**:它在 153 行 `exit $?` 提前结束 —— 此刻产物尚未提交,
    #    provenance 必然不成立 ⇒ 不豁免会把「commit 产物 → --update」流程本身卡死。
    sys.exit(1)
PYEOF
exit $?
