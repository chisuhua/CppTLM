#!/usr/bin/env bash
# scripts/test/cep_freeze_check.sh -- CEP Stage 0 产物冻结校验(机制 M1)
#
# 机制 M1「产物冻结先于解读」(per .rddf/roadmap/objectives/obj-cep-platform.md §9.2):
#   "4 件套由 CI 产出并带 hash;冻结后**不可改**;解读与冻结分离 ——
#    即使同一人,也不能改已冻结的产物"
#
# Stage 0 尚无实验 4 件套(那要 Stage 1 才有),故 M1 在 Stage 0 的对应物是
# **Stage 0 的规范性契约产物**冻结:
#   ① 22 份 ADR-CEP-*.md           (真源 3:invariant 契约)
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
#   bash scripts/test/cep_freeze_check.sh            # 校验(漂移即非零退出)
#   bash scripts/test/cep_freeze_check.sh --update   # 重新生成冻结清单
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

MANIFEST="docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json"
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

if [ "${1:-}" = "--update" ]; then
  # ⚠️ **授权闸门**(2026-10-22 代码复审 B5):`--update` 无条件重算全部 hash 并写回,
  #    等于「一次命令即可把任何未授权的冻结产物修改吸收为已冻结」。
  #    故要求提供台账 entry id 并核对其确实存在于台账 §11 —— 台账 entry 即**授权记录**。
  # ⚠️ **必须提供且必须精确命中 entry 标题**(2026-10-22 复审 NB2):
  #    初版「缺省只警告 + grep 子串匹配」可被 `--authorized-by "a"` 伪造 → 闸门形同虚设。
  AUTH=""
  if [ "${2:-}" = "--authorized-by" ]; then AUTH="${3:-}"; fi
  LEDGER=".rddf/roadmap/objectives/obj-cep-platform.md"
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
  #   (entry id = '### ' 之后、' — ' 之前的部分;标题的日期/类型后缀不算)
  ENTRY_IDS="$(grep "^### " "$LEDGER" | sed 's/^### //; s/ *—.*$//')"
  if ! printf '%s\n' "$ENTRY_IDS" | grep -qxF "$AUTH"; then
    echo "  ✗ 授权校验失败:台账 $LEDGER 中没有 id 为 '$AUTH' 的 entry"
    echo "    · 现有 entry id:"
    printf '%s\n' "$ENTRY_IDS" | sed 's/^/      /'
    exit 1
  fi
  echo "  ✓ 授权校验通过:台账 entry '$AUTH'"
  python3 - "$MANIFEST" "$ROOT" "$AUTH" "${FROZEN_GLOBS[@]}" << 'PYEOF'
import glob, hashlib, json, os, subprocess, sys

manifest_path, root, auth = sys.argv[1], sys.argv[2], sys.argv[3]
globs = sys.argv[4:]

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

payload = {
    "frozen_at": frozen_at or "",
    "frozen_at_commit": head,
    "authorized_by": auth or "",
    "mechanism": "M1 产物冻结先于解读(per obj-cep-platform §9.2)",
    "note": ("Stage 0 冻结的是**规范性契约产物**(ADR + 07 号 + schema + tools + spec);"
             "两个 checker 脚本自身不冻结(checker 必须可修,且不承载契约语义)。"
             "**fabric 载体按「契约面 / 数据面」拆分**:`fabric.schema.json`(**类型描述** = 契约,"
             "故冻结 —— 否则 `vc_count.maximum` 可被静默放宽);`fabric.json`(**实例数据** = 派生副本,"
             "权威在 ArchForge,故**不冻结**,Stage 1 首个 implemented 实例跑过后再纳)。"
             "冻结后若需修改,须走 §11 台账 scope-change / adr-amendment entry;"
             "`--update` 需带 `--authorized-by <台账 entry id>` 才有授权记录。"),
    "artifact_count": len(files),
    "globs": list(globs),
    "artifacts": {rel: sha256(rel) for rel in files},
    # ══ 2026-11-10(C-2′ / Entry #324):**排除表**(唯一可豁免「glob 全量冻结」的通道)══
    #   语义:冻结面 = `openspec/specs/cep-*/spec.md` **全集**;本数组**当前为空**。
    #   条目形态:`{"path": <相对路径>, "reason": <为何不冻结>, "ledger_entry": <台账 Entry #N>}`
    #   ⇒ 校验见 `cep_freeze_check.sh` 的 exclusion-integrity 段(**空数组 = 全量冻结**)
    "exclusions": [],
}
with open(os.path.join(root, manifest_path), "w", encoding="utf-8") as f:
    json.dump(payload, f, ensure_ascii=False, indent=2, sort_keys=True)
    f.write("\n")
print(f"[cep_freeze_check] 已重新生成冻结清单: {len(files)} 个产物 -> {manifest_path}")
PYEOF
  exit $?
fi

if [ ! -f "$MANIFEST" ]; then
  echo "  ✗ 冻结清单不存在: $MANIFEST"
  echo "    生成: bash scripts/test/cep_freeze_check.sh --update"
  exit 1
fi

python3 - "$MANIFEST" "$ROOT" << 'PYEOF'
import hashlib, json, os, sys

manifest_path = sys.argv[1]
root = sys.argv[2]
with open(os.path.join(root, manifest_path), encoding="utf-8") as f:
    man = json.load(f)

def sha256(rel):
    h = hashlib.sha256()
    with open(os.path.join(root, rel), "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

arts = man.get("artifacts", {})
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
        if rel not in arts:
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
    for _ln in open(_LEDGER_ABS, encoding="utf-8"):
        if _ln.startswith("### "):
            LEDGER_IDS.add(_re.sub(r" *—.*$", "", _ln[4:]).strip())
EXCL = man.get("exclusions", []) if isinstance(man, dict) else []
_excl_errors = []
_spec_paths = sorted(_glob.glob(os.path.join(root, "openspec/specs/cep-*/spec.md")))
_spec_rels = {os.path.relpath(p, root) for p in _spec_paths}
for e in EXCL:
    if not isinstance(e, dict):
        _excl_errors.append(f"排除表条目不是对象: {e!r}")
        continue
    p, why, led = e.get("path"), e.get("reason"), e.get("ledger_entry")
    if not p or not why or not led:
        _excl_errors.append(f"排除表条目缺 `path`/`reason`/`ledger_entry`: {p or '<no path>'}")
        continue
    if p not in _spec_rels:
        _excl_errors.append(f"排除表条目指向**不存在**的 spec: {p}")
    if led not in LEDGER_IDS:
        _excl_errors.append(f"排除表条目的 `ledger_entry` 在台账中解析不到: {led}")
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
