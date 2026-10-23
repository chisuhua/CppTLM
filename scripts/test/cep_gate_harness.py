#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cep_gate_harness.py —— 独立跑 `cep_docs_sync_check.sh` 的**第 11 组(状态一致性闸)**。

## 为什么需要它

测「闸本身的行为」时,**不能用整脚本的 rc**:`ADR-CEP-00` 等是 **M1 冻结面 30 成员**,
**任何**对它们的编辑都会让第 7 组(冻结校验)报红 → 整脚本 rc=1 **必然**发生,
与第 11 组无关。2026-10-24 曾因此把「闸没 fire」误判成「闸 fire 了」。

本 harness 把第 11 组的 Python **原样抽出**,跑在**真拷贝**的临时根上:
- `.rddf` / `docs` / `scripts` / `AGENTS.md` → **真拷贝**(改副本不写穿仓库)
- `openspec` → **symlink**(闸只做 glob 与 `openspec list --json`)

## 用法

```bash
python3 scripts/test/cep_gate_harness.py                 # 基线:6 条断言全绿,rc=0
python3 scripts/test/cep_gate_harness.py --mutate '<python 代码>'   # 先变异临时根,再跑
python3 scripts/test/cep_gate_harness.py --mutate-file /path/to/mut.py
```

**只看本进程的 rc**(0 = 全绿,1 = 有断言报红),不要看整脚本 rc。
"""
import os
import shutil
import subprocess
import sys

R = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# 临时根:默认用 `tempfile.mkdtemp()`(尊重 TMPDIR;**不**用 /tmp/opencode ——
#        那是 agent 会话目录约定,本脚本已在 CI 的 ctest 里跑)。
#        `CEP_GATE_TMP` 可显式指定(调试取证用),此时不自动清理。
_EXPLICIT_TMP = os.environ.get("CEP_GATE_TMP")
if _EXPLICIT_TMP:
    RT = _EXPLICIT_TMP
else:
    import tempfile

    RT = tempfile.mkdtemp(prefix="cep_gate_root_")

FILES = [
    ".rddf/roadmap/objectives/obj-cep-platform.md",
    ".rddf/roadmap/phases/phase-10.md",
    ".rddf/improvements/cpptlm-cep-umbrella.md",
    ".rddf/roadmap.md",
    "AGENTS.md",
    "docs/adr/ADR-CEP-00-platform-charter-and-governance.md",
    "docs/adr/ADR-CEP-NUMBER-MAP.md",
    "docs/ONBOARDING.md",
    "scripts/README.md",
    "docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json",
]

CHECKER = "scripts/test/cep_docs_sync_check.sh"


def cleanup_root():
    """退出时清理临时根(仅当它是自动创建的)。"""
    if not _EXPLICIT_TMP:
        shutil.rmtree(RT, ignore_errors=True)


def build_root():
    shutil.rmtree(RT, ignore_errors=True)
    for rel in FILES:
        dst = os.path.join(RT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy(os.path.join(R, rel), dst)
    # openspec 只读 → symlink(闸做 glob + openspec list)
    link = os.path.join(RT, "openspec")
    if not os.path.exists(link):
        os.symlink(os.path.join(R, "openspec"), link)


def extract_block():
    """把第 11 组的 python 块原样抽出。"""
    src = open(os.path.join(R, CHECKER), encoding="utf-8").read()
    start = src.index("python3 - \"$PROJECT_ROOT\" << 'PYEOF'")
    end = src.index("\nPYEOF", start)
    body = src[start:end].split("\n", 1)[1]
    return body


def main():
    build_root()
    if "--mutate" in sys.argv:
        code = sys.argv[sys.argv.index("--mutate") + 1]
        # prelude:变异代码统一可用 `sub()`(锚点唯一性断言 → 漂移即响亮失败)
        prelude = (
            "import os\n"
            "def sub(text, old, new):\n"
            "    assert text.count(old) == 1, '锚点漂移:变异未生效 → 本用例会**假绿**'\n"
            "    return text.replace(old, new, 1)\n"
        )
        p = subprocess.run(["python3", "-c", prelude + code],
                           env={**os.environ, "RT": RT, "CEP_GATE_SRC": R})
        # ⚠️ 变异失败必须**响亮失败**:否则「变异没生效」会被读成「期望 GREEN」→ **假绿**
        if p.returncode != 0:
            print(f"❌ 变异脚本失败 rc={p.returncode} —— 变异未生效,本次结果**不可信**")
            return 2
    elif "--mutate-file" in sys.argv:
        mf = sys.argv[sys.argv.index("--mutate-file") + 1]
        p = subprocess.run(["python3", mf], env={**os.environ, "RT": RT, "CEP_GATE_SRC": R})
        if p.returncode != 0:
            print(f"❌ 变异脚本失败 rc={p.returncode} —— 变异未生效,本次结果**不可信**")
            return 2
    blk = os.path.join(RT, "_gate_block.py")
    open(blk, "w", encoding="utf-8").write(extract_block())
    p = subprocess.run(["python3", blk, RT], capture_output=True, text=True)
    print(p.stdout.rstrip())
    if p.stderr.strip():
        print("STDERR:", p.stderr.strip()[:600])
    print(f"→ 第 11 组 rc = {p.returncode} (0 = 全绿;1 = 有断言报红)")
    return p.returncode


if __name__ == "__main__":
    import atexit

    atexit.register(cleanup_root)
    sys.exit(main())
