#!/usr/bin/env bash
# docs/research/oswald/materialize_papers.sh
# 论文原文物化器 —— 把仓外 mynotes 仓的 4 份 PDF 原文物化到本目录 .papers/（gitignored）并按 sha256 校验
#
# 用途:让「论文原文」在本仓**可寻址 + 可校验**，而不把 PDF 二进制 vendor 进 git
#      （per docs/research/AGENTS.md §5「第三方原文不存放」—— 避免版权 + 体积问题）。
#      ⇒ 仓内可复核的是「抽取件 + 行级锚点」（见 paper-claims-index.md），
#        PDF 同一性由本脚本的 sha256 校验保证。
#
# 形态:
#   materialize_papers.sh            # 物化全部（缺失即报 SKIP，不失败）
#   materialize_papers.sh --verify   # 只校验已存在的 .papers/（不复制）
#   materialize_papers.sh --list     # 只打印清单
#
# 诚实边界:
#   · 源路径在**仓外**（/workspace/main/mynotes/00_Raw_Sources/）⇒ 本仓无该路径时只能 SKIP，
#     此时「原文同一性」无法在本机验证（但抽取件与锚点仍可机检）。
#   · 本脚本**不写** .papers/ 以外的任何路径；.papers/ 已在 .gitignore 内。
#
# 配套闸: scripts/test/cep_oswald_provenance_check.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MANIFEST="$HERE/source-manifest.json"
PAPERS_DIR="$HERE/.papers"
MODE="${1:-materialize}"

RED=$'\033[1;31m'; GRN=$'\033[1;32m'; YEL=$'\033[1;33m'; DIM=$'\033[2m'; RST=$'\033[0m'

[ -f "$MANIFEST" ] || { echo "${RED}[FAIL]${RST} 缺 manifest: $MANIFEST"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "${RED}[FAIL]${RST} 需 python3"; exit 1; }

python3 - "$MANIFEST" "$PAPERS_DIR" "$MODE" <<'PY'
import hashlib, json, os, shutil, sys

manifest, papers_dir, mode = sys.argv[1], sys.argv[2], sys.argv[3]
m = json.load(open(manifest, encoding="utf-8"))

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

if mode == "--list":
    for p in m["papers"]:
        print(f"{p['id']:10} {p['venue']:34} sha256={p['sha256'][:16]}  {p['materialized_path']}")
    sys.exit(0)

os.makedirs(papers_dir, exist_ok=True)
n_copied = n_ok = n_skip = 0
for p in m["papers"]:
    dst = os.path.join(papers_dir, os.path.basename(p["materialized_path"]))
    origin = p["origin_path"]
    if mode != "--verify" and os.path.isfile(origin):
        shutil.copy2(origin, dst)
        n_copied += 1
    elif not os.path.isfile(dst):
        print(f"  [SKIP] {p['id']:10} 源不可达（{origin}）且未物化 ⇒ 原文同一性本机不可验", flush=True)
        n_skip += 1
        continue
    got = sha256(dst)
    if got == p["sha256"]:
        print(f"  [ OK ] {p['id']:10} sha256={got[:16]}  {os.path.basename(dst)}", flush=True)
        n_ok += 1
    else:
        print(f"  [FAIL] {p['id']:10} sha256 漂移：期望 {p['sha256'][:16]} 实得 {got[:16]}", flush=True)
        sys.exit(1)

print(f"\n  物化={n_copied} 校验通过={n_ok} 跳过={n_skip}", flush=True)
if n_skip and mode != "--verify":
    print("  提示: 仓外源不可达时，原文同一性无法本机验证；抽取件与行级锚点仍由仓内闸覆盖。", flush=True)
PY
