#!/usr/bin/env bash
# scripts/test/config_type_registration_check.sh
# change `cpptlm-config-type-registration-align`（R-5）的 T3 判据闸
#
# 判据：配置里出现的每个模块 `type` SHALL 存在于**运行时注册表**中。
#   违反后果不是「报错」而是**静默丢模块** —— `instantiateAll` 对未注册类型只打
#   `DPRINTF` 然后继续（Release 下连日志都没有）⇒ 拓扑缺模块 ⇒ 仿真结果静默失真。
#
# 判据集合**只从运行时导出派生**：
#   `cpptlm_sim --list-types --json` ⇒ `ModuleFactory::getRegisteredTypes()`
#   ⛔ 不做源码解析、⛔ 不硬编码任何类型名 —— 否则会与实现漂移，并漏掉最难的两个案例
#      （`TmuDispatchProcessorTLM` 有类但无基类 / `ArbiterTLM` 是模板基类）。
#
# 扫描域：`configs/**/*.json`（排除 `configs/param_rules/**`）
#   ⚠️ 类型集合是**构建相关**的（`HybridCacheWrapper` 仅 `BUILD_RTL` 注册）
#      ⇒ 本闸必须**按构建运行**，⛔ 禁静态白名单。
#
# 用法：
#   bash scripts/test/config_type_registration_check.sh              # 正常检查
#   bash scripts/test/config_type_registration_check.sh --self-test  # 含反向自测（默认也跑）
#   bash scripts/test/config_type_registration_check.sh --bin <path> # 指定 cpptlm_sim
#
# 退出码：0 = 通过（且反向自测通过）;1 = 有未注册类型 / 自测失败;2 = 工具不可用
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT" || exit 2

SIM_BIN="./build/bin/cpptlm_sim"
RUN_SELFTEST=1
while [ $# -gt 0 ]; do
    case "$1" in
        --bin) SIM_BIN="$2"; shift 2 ;;
        --no-self-test) RUN_SELFTEST=0; shift ;;
        --self-test) RUN_SELFTEST=1; shift ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[0;33m'; RST=$'\033[0m'
fail() { echo -e "${RED}  ✗ $1${RST}"; FAILED=$((FAILED + 1)); }
ok()   { echo -e "${GRN}  ✓ $1${RST}"; }
warn() { echo -e "${YLW}  ⚠ $1${RST}"; }
FAILED=0

if [ ! -x "$SIM_BIN" ]; then
    echo -e "${RED}工具不可用：$SIM_BIN 不存在或不可执行${RST}" >&2
    echo "  先构建：./build.sh" >&2
    exit 2
fi

TMPDIR_SELF="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_SELF"' EXIT

# ────────────────────────────────────────────────────────────────────────────
# 1. 导出运行时注册表（唯一真源）
# ────────────────────────────────────────────────────────────────────────────
TYPES_JSON="$("$SIM_BIN" --list-types --json 2>/dev/null)"
if [ -z "$TYPES_JSON" ]; then
    echo -e "${RED}--list-types --json 无输出${RST}" >&2
    exit 2
fi
echo "$TYPES_JSON" > "$TMPDIR_SELF/types.json"

# ────────────────────────────────────────────────────────────────────────────
# 2. 扫描配置并求差集
#    ⚠️ 遍历**嵌套** `modules[]`（`dgpu_board_v1.json` 的 `tmu` 就在 soc 内联模块里）
#    ⚠️ 扫描 `extends` / `include` / `${var}` 展开**之后**的结果 —— 展开前会漏掉
#       `${var}` 注入的类型
# ────────────────────────────────────────────────────────────────────────────
python3 - "$TMPDIR_SELF/types.json" "$TMPDIR_SELF/result.txt" <<'PY'
import json
import os
import sys

types_path, out_path = sys.argv[1], sys.argv[2]
registered = set(json.load(open(types_path, encoding="utf-8"))["types"])

# 极简 extends / $include / ${var} 展开：只用于**收集 type**，不追求与 C++ 侧完全等价。
# ⚠️ 若某文件展开失败，**不静默跳过** —— 记为 ERROR 行（漏扫比误扫危险）。
MAX_DEPTH = 8


def load_expanded(path, depth=0, seen=None):
    if seen is None:
        seen = set()
    if depth > MAX_DEPTH:
        raise RuntimeError(f"include depth > {MAX_DEPTH}")
    real = os.path.realpath(path)
    if real in seen:
        return {}
    seen = seen | {real}
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except Exception as e:  # noqa: BLE001
        raise RuntimeError(f"parse failed: {e}") from e

    if not isinstance(data, dict):
        return {}

    # `extends`: 父配置打底，子配置覆盖
    parent = data.pop("extends", None)
    if parent:
        parent_path = parent if os.path.isabs(parent) else os.path.join(os.path.dirname(path), parent)
        if not os.path.exists(parent_path):
            parent_path = os.path.join(os.getcwd(), parent)
        if os.path.exists(parent_path):
            base = load_expanded(parent_path, depth + 1, seen)
            data = {**base, **data}
            if "modules" in base and "modules" in data and base["modules"] is not data["modules"]:
                data["modules"] = list(base["modules"]) + list(data["modules"])
        else:
            raise RuntimeError(f"extends target not found: {parent}")

    return data


def collect(modules, src, out):
    for m in modules or []:
        if not isinstance(m, dict):
            continue
        t = m.get("type")
        if isinstance(t, str) and t:
            out.setdefault(t, set()).add(src)
        collect(m.get("modules"), src, out)


files = []
for root, _dirs, fs in os.walk("configs"):
    if os.path.normpath(root).startswith(os.path.normpath("configs/param_rules")):
        continue
    for f in fs:
        if f.endswith(".json"):
            files.append(os.path.join(root, f))
files.sort()

seen_types = {}
errors = []
for path in files:
    try:
        data = load_expanded(path)
    except Exception as e:  # noqa: BLE001
        errors.append(f"{path}: {e}")
        continue
    collect(data.get("modules"), path, seen_types)

missing = {t: v for t, v in seen_types.items() if t not in registered}

with open(out_path, "w", encoding="utf-8") as f:
    f.write(f"# configs={len(files)} types={len(seen_types)} registered={len(registered)}\n")
    for e in errors:
        f.write(f"ERROR\t{e}\n")
    for t in sorted(missing):
        for src in sorted(missing[t]):
            f.write(f"MISSING\t{t}\t{src}\n")

print(f"  扫描：{len(files)} 份配置 / {len(seen_types)} 个 type / 注册表 {len(registered)} 个")
if errors:
    print(f"  ⚠️ 展开失败 {len(errors)} 份（不计入通过）：")
    for e in errors[:5]:
        print(f"     {e}")
PY

# ────────────────────────────────────────────────────────────────────────────
# 3. 判据（零豁免）
# ────────────────────────────────────────────────────────────────────────────
# ⚠️ `grep -c` 无匹配时**输出 0 但 exit 1** ⇒ 不能写 `|| echo 0`（会得到 "0\n0"）
MISSING_N="$(grep -c '^MISSING' "$TMPDIR_SELF/result.txt" 2>/dev/null)"
ERROR_N="$(grep -c '^ERROR' "$TMPDIR_SELF/result.txt" 2>/dev/null)"
[ -z "${MISSING_N:-}" ] && MISSING_N=0
[ -z "${ERROR_N:-}" ] && ERROR_N=0

if [ "$ERROR_N" -gt 0 ]; then
    # ⚠️ 展开失败的配置**不判 pass** —— 漏扫比误扫危险（同 verifier 的「取证不可用 ⇒ 不判 pass」）
    fail "有 $ERROR_N 份配置无法展开/解析（⛔ 不判通过）"
    grep '^ERROR' "$TMPDIR_SELF/result.txt" | head -5 | sed 's/^ERROR\t/     /'
fi

if [ "$MISSING_N" -eq 0 ]; then
    ok "配置中的每个模块 type 都在运行时注册表内（零豁免）"
else
    fail "发现 $MISSING_N 处引用未注册类型"
    grep '^MISSING' "$TMPDIR_SELF/result.txt" | head -20 |
        awk -F'\t' '{printf "     type=%s  ← %s\n", $2, $3}'
    echo "     ⇒ 这些模块会被**静默丢弃**（装配成功但拓扑缺模块）。"
    echo "     ⇒ 处置：改用已注册类型 / 从配置删除该条目（⛔ 不要放宽本闸）。"
fi

# ────────────────────────────────────────────────────────────────────────────
# 4. 反向自测（只验绿 = 没验）
#    向临时 fixture 注入一个假 type ⇒ 闸的判据**必须**报出它。
# ────────────────────────────────────────────────────────────────────────────
if [ "$RUN_SELFTEST" -eq 1 ]; then
    FAKE_TYPE="ZzSelfTestFakeType_$(date +%s)"
    FIXTURE="$TMPDIR_SELF/selftest.json"
    cat > "$FIXTURE" <<EOF
{ "name": "selftest", "modules": [ {"name": "f", "type": "$FAKE_TYPE"} ], "connections": [] }
EOF
    # 用同一套 Python 判据（复用逻辑 ⇒ 自测验的是真判据，不是另一份实现）
    SELFTEST_HIT="$(python3 - "$TMPDIR_SELF/types.json" "$FIXTURE" "$FAKE_TYPE" <<'PY'
import json
import sys

types_path, fixture_path, fake = sys.argv[1], sys.argv[2], sys.argv[3]
registered = set(json.load(open(types_path, encoding="utf-8"))["types"])
cfg = json.load(open(fixture_path, encoding="utf-8"))
found = [m["type"] for m in cfg.get("modules", []) if m.get("type") not in registered]
print("HIT" if fake in found else "MISS")
PY
)"
    if [ "$SELFTEST_HIT" = "HIT" ]; then
        ok "反向自测：注入未注册类型 '$FAKE_TYPE' ⇒ 判据能识别（闸有牙）"
    else
        fail "反向自测失败：注入的未注册类型未被判据识别 ⇒ 闸没有牙"
    fi

    # 反向的另一半：已注册类型**不得**被误报
    REAL_TYPE="$(python3 -c "
import json,sys
d=json.load(open('$TMPDIR_SELF/types.json',encoding='utf-8'))
print(sorted(d['types'])[0])
")"
    SELFTEST_MISS="$(python3 - "$TMPDIR_SELF/types.json" "$REAL_TYPE" <<'PY'
import json
import sys

types_path, real = sys.argv[1], sys.argv[2]
registered = set(json.load(open(types_path, encoding="utf-8"))["types"])
print("OK" if real in registered else "BAD")
PY
)"
    if [ "$SELFTEST_MISS" = "OK" ]; then
        ok "反向自测：已注册类型 '$REAL_TYPE' 不被误报"
    else
        fail "反向自测失败：已注册类型被误报"
    fi
fi

echo ""
if [ "$FAILED" -gt 0 ]; then
    echo -e "${RED}配置类型注册校验失败：$FAILED 项${RST}"
    exit 1
fi
echo -e "${GRN}配置类型注册校验通过${RST}"
exit 0
