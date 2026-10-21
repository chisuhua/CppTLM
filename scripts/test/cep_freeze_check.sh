#!/usr/bin/env bash
# scripts/test/cep_freeze_check.sh -- CEP Stage 0 产物冻结校验(机制 M1)
#
# 机制 M1「产物冻结先于解读」(per .rddf/roadmap/objectives/obj-cep-platform.md §9.2):
#   "4 件套由 CI 产出并带 hash;冻结后**不可改**;解读与冻结分离 ——
#    即使同一人,也不能改已冻结的产物"
#
# Stage 0 尚无实验 4 件套(那要 Stage 1 才有),故 M1 在 Stage 0 的对应物是
# **Stage 0 的规范性契约产物**冻结:
#   ① 21 份 ADR-CEP-*.md           (真源 3:invariant 契约)
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
  "include/tlm/cep/protocol_instance.schema.json"
  "experimental/metrics.schema.json"
  "tools/render_report.py"
  "tools/run_sweep.py"
  "tools/ab_compare.py"
  "openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md"
)

if [ "${1:-}" = "--update" ]; then
  python3 - "$MANIFEST" "$ROOT" "${FROZEN_GLOBS[@]}" << 'PYEOF'
import glob, hashlib, json, os, subprocess, sys

manifest_path = sys.argv[1]
root = sys.argv[2]
globs = sys.argv[3:]

files = []
for g in globs:
    files += glob.glob(os.path.join(root, g))
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

payload = {
    "frozen_at": "2026-10-21",
    "frozen_at_commit": head,
    "mechanism": "M1 产物冻结先于解读(per obj-cep-platform §9.2)",
    "note": ("Stage 0 冻结的是**规范性契约产物**(ADR + 07 号 + schema + tools + spec);"
             "两个 checker 脚本自身不冻结(checker 必须可修,且不承载契约语义)。"
             "冻结后若需修改,须走 §11 台账 scope-change / adr-amendment entry。"),
    "artifact_count": len(files),
    "globs": list(globs),
    "artifacts": {rel: sha256(rel) for rel in files},
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
    for p in glob.glob(os.path.join(root, g)):
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

print(f"  \033[0;32m✓\033[0m M1 产物冻结校验通过:{len(arts)} 个产物 hash 全部一致"
      f"(冻结于 {man.get('frozen_at', '?')}, commit {str(man.get('frozen_at_commit', ''))[:12]})")
PYEOF
exit $?
