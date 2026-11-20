#!/usr/bin/env python3
"""cep_instance_golden_check.py —— 13 键仓内 golden 的**双向比对**闸

per Q-5 裁决（`.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §2.5）：
    canonical = 仓内 `test/cep_instance_golden.json`（**派生副本**）
    真源        = `include/tlm/cep/protocol_instances.json`
    ⇒ 闸必须做**双向比对**（沿用 `cep_decl_alignment_check.sh` 的双向对账范式）
      ⇒ **单改一侧即红** ⇒ ⛔ 不得把 golden 当独立真源。

比对方向（两个方向都查，缺一即红）：
    ① golden → 真源：golden 里的每个实例 / 每个键，真源必须有且**取值相等**
    ② 真源 → golden：`_scope` 内每个实例，golden 必须覆盖且**键集合相等**
       （键缺失 / 键多余 / 取值不等 都判红）

用法:
    python3 scripts/test/cep_instance_golden_check.py
退出码: 0 = 绿;1 = 红(方向与差异逐条打印)
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "test" / "cep_instance_golden.json"
SOURCE = ROOT / "include" / "tlm" / "cep" / "protocol_instances.json"

KEYS = (
    "id",
    "family",
    "adapter",
    "status",
    "protocol_version",
    "flow_support",
    "perm",
    "atomic",
    "consistency_labels",
    "ordering_assumption",
    "vc_binding",
    "sharer_tracker",
    "combination_capabilities",
)


def main() -> int:
    if not GOLDEN.exists():
        print(f"  ✗ 缺 golden: {GOLDEN.relative_to(ROOT)}", file=sys.stderr)
        return 1
    if not SOURCE.exists():
        print(f"  ✗ 缺真源: {SOURCE.relative_to(ROOT)}", file=sys.stderr)
        return 1

    golden = json.loads(GOLDEN.read_text(encoding="utf-8"))
    src = json.loads(SOURCE.read_text(encoding="utf-8"))

    scope = list(golden.get("_scope", []))
    g_inst = golden.get("instances", {})
    s_inst = {i["id"]: i for i in src.get("instances", [])}

    problems = []

    # ── 范围自洽：golden 覆盖的实例集合必须**恰等于** _scope ─────────────────
    if sorted(g_inst) != sorted(scope):
        problems.append(
            f"golden 的实例集合与 `_scope` 不一致: golden={sorted(g_inst)} _scope={sorted(scope)}"
        )
    # ⛔ 不得把 ArchForge 权威的实例纳入（否则造出第二真源）
    owned = set(src.get("repo_owned_instance_ids", []))
    leaked = sorted(set(scope) & owned)
    if leaked:
        problems.append(
            f"`_scope` 含 repo-owned 实例 {leaked} ⇒ 其权威在 ArchForge ⇒ ⛔ 不得纳入 golden"
        )

    # ── 方向 ①: golden → 真源 ────────────────────────────────────────────
    for iid, g_vals in sorted(g_inst.items()):
        if iid not in s_inst:
            problems.append(f"[golden→源] golden 有实例 {iid}，真源无此实例")
            continue
        s_vals = s_inst[iid]
        for k in sorted(set(g_vals) | set(KEYS)):
            if k not in KEYS:
                problems.append(f"[golden→源] {iid} 的键 {k} 不在 13 键表内")
                continue
            if k not in g_vals:
                problems.append(f"[golden→源] {iid} 缺键 {k}")
                continue
            if k not in s_vals:
                problems.append(f"[golden→源] {iid}.{k} 在真源中不存在")
                continue
            if g_vals[k] != s_vals[k]:
                problems.append(f"[golden→源] {iid}.{k} 取值不等: golden={g_vals[k]!r} 源={s_vals[k]!r}")

    # ── 方向 ②: 真源 → golden ────────────────────────────────────────────
    for iid in scope:
        if iid not in s_inst:
            problems.append(f"[源→golden] `_scope` 内的 {iid} 在真源中不存在")
            continue
        if iid not in g_inst:
            problems.append(f"[源→golden] {iid} 未出现在 golden 中")
            continue
        s_keys = set(s_inst[iid])
        g_keys = set(g_inst[iid])
        missing = sorted(s_keys - g_keys)
        extra = sorted(g_keys - s_keys)
        if missing:
            problems.append(f"[源→golden] {iid} 在 golden 中缺键 {missing}(真源有)")
        if extra:
            problems.append(f"[源→golden] {iid} 在 golden 中多出键 {extra}(真源无)")

    if problems:
        for p in problems:
            print(f"  ✗ {p}", file=sys.stderr)
        print(f"  ✗ golden ↔ 真源 双向比对失败:{len(problems)} 处", file=sys.stderr)
        return 1

    n_keys = sum(len(v) for v in g_inst.values())
    print(
        f"  ✓ 13 键 golden ↔ 真源 双向一致:"
        f" {len(g_inst)} 实例 / {n_keys} 键(逐键取值相等 · 键集合相等)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
