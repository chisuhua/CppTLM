#!/usr/bin/env python3
"""参数效果盘点（change `cpptlm-module-param-effect-landing` 的 T0.1 / AC-01 判据）

遍历 `configs/**/*.json`（排除 `configs/param_rules/**`），输出：

1. 每个「类型 × params 键」的出现次数与出现位置；
2. 该类型在其头文件里**是否有配置读取面**（`on_config_loaded` / `set_config` 覆写）——
   无读取面 ⇒ design.md D-1.2 的「类别 ①」（零配置读取面）；
3. 汇总：类型数 / 键 × 类型对数 / 有读取面的类型数。

⚠️ 本脚本只做**盘点**，不判定「读到的值是否真的生效」（`ADR-X.17` D-6 明示该判定
不可机械完成）⇒ 逐键三态裁决仍以 `design.md` D-1 为准。

用法：
    python3 scripts/test/param_effect_inventory.py            # 人类可读
    python3 scripts/test/param_effect_inventory.py --json     # 机读（供 AC-01 对拍）
    python3 scripts/test/param_effect_inventory.py --check    # 与 design.md D-1.5 统计对拍
"""

import argparse
import json
import os
import re
import sys
from collections import defaultdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONFIG_DIR = os.path.join(REPO, "configs")
INCLUDE_DIR = os.path.join(REPO, "include")
EXCLUDE_PREFIX = os.path.join("configs", "param_rules")

# design.md D-1.6.2 的统计基准（--check 用）；实施期新增键时同步更新本常量与 design.md
EXPECTED_TYPE_COUNT = 12
EXPECTED_KEY_TYPE_PAIRS = 22


def iter_configs():
    for root, dirs, files in os.walk(CONFIG_DIR):
        rel_root = os.path.relpath(root, REPO)
        if rel_root.startswith(EXCLUDE_PREFIX):
            continue
        dirs[:] = [d for d in dirs if not os.path.join(rel_root, d).startswith(EXCLUDE_PREFIX)]
        for f in sorted(files):
            if not f.endswith(".json"):
                continue
            path = os.path.join(root, f)
            try:
                with open(path, encoding="utf-8") as fh:
                    yield os.path.relpath(path, REPO), json.load(fh)
            except (json.JSONDecodeError, OSError) as exc:  # 坏配置不该让盘点崩掉
                print(f"WARN: 跳过无法解析的配置 {path}: {exc}", file=sys.stderr)


def collect_modules(node, out):
    """递归收集 {"type": ..., "name": ..., "params": {...}} 形态的模块条目。"""
    if isinstance(node, dict):
        t = node.get("type")
        if isinstance(t, str) and isinstance(node.get("params"), dict):
            out.append((t, node.get("name", "?"), node["params"]))
        for key in ("modules", "children", "submodules"):
            if key in node:
                collect_modules(node[key], out)
        for v in node.values():
            if isinstance(v, (dict, list)):
                collect_modules(v, out)
    elif isinstance(node, list):
        for item in node:
            collect_modules(item, out)


_HEADER_CACHE = {}
_LITERAL_INDEX = None


def _literal_index():
    """`include/**` + `src/**` 里出现的**字符串字面量** → 文件集合。

    用途：判断某参数键是否被**本类型之外**的代码读取（板级直读 / 父模块转发）。
    ⚠️ 只做「键名字符串是否出现」的粗筛，不判语义；命中 ⇒ 不能直接判为零效果。
    """
    global _LITERAL_INDEX
    if _LITERAL_INDEX is not None:
        return _LITERAL_INDEX
    lit = re.compile(r'"([^"\\\n]{1,80})"')
    index = defaultdict(set)
    for base in (INCLUDE_DIR, os.path.join(REPO, "src")):
        for root, _dirs, files in os.walk(base):
            for f in files:
                if not f.endswith((".hh", ".h", ".cc", ".cpp")):
                    continue
                path = os.path.join(root, f)
                try:
                    with open(path, encoding="utf-8", errors="ignore") as fh:
                        text = fh.read()
                except OSError:
                    continue
                rel = os.path.relpath(path, REPO)
                for m in lit.finditer(text):
                    index[m.group(1)].add(rel)
    _LITERAL_INDEX = index
    return index


def _iter_with_base(type_name):
    """`ArbiterTLM4` 是 `ArbiterTLM<4>` 的注册名 ⇒ 退到基类名去定位定义。"""
    yield type_name
    m = re.match(r"^(.*?)\d+$", type_name)
    if m and m.group(1):
        yield m.group(1)


def find_headers(type_name):
    """在 include/ 下定位**定义**该类型的文件（含前向声明，供调用方筛）。

    返回 (实际匹配名, [(相对路径, 内容)])。
    ⚠️ 必须返回全部命中：同一类型常在聚合头里被前向声明（`class X;`），只取第一个
    命中会把定义它的头文件漏掉 ⇒ 读取面被误判为「无」。
    """
    if type_name in _HEADER_CACHE:
        return _HEADER_CACHE[type_name]
    for name in _iter_with_base(type_name):
        pat = re.compile(
            r"^\s*(?:\[\[[^\]]*\]\]\s*)?(?:class|struct)\s+" + re.escape(name) + r"\b",
            re.MULTILINE,
        )
        found = []
        for root, _dirs, files in os.walk(INCLUDE_DIR):
            for f in sorted(files):
                if not f.endswith((".hh", ".h")):
                    continue
                path = os.path.join(root, f)
                try:
                    with open(path, encoding="utf-8", errors="ignore") as fh:
                        text = fh.read()
                except OSError:
                    continue
                if pat.search(text):
                    found.append((os.path.relpath(path, REPO), text))
        if found:
            _HEADER_CACHE[type_name] = (name, found)
            return _HEADER_CACHE[type_name]
    _HEADER_CACHE[type_name] = (type_name, [])
    return _HEADER_CACHE[type_name]


def _strip_comments_and_strings(text):
    """去掉注释与字符串字面量 —— 否则注释里的 `{` / `}` 会破坏花括号配对。"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif c in ("'", '"'):
            quote = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            out.append(" ")
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _class_body(text, type_name):
    """截取 `class/struct <type_name>` 的**类体**（花括号配对）。

    ⚠️ 不能对整个文件做子串匹配：聚合头里相邻类的 `on_config_loaded` 会让本类
    被误判为「有读取面」（实测 `GpuMeshNoC` 曾因此被误判）。
    ⚠️ 前向声明（`class X;`）没有类体 ⇒ 遇到 `;` 先于 `{` 即跳过，否则会错抓到
    下一个无关类的花括号。
    """
    clean = _strip_comments_and_strings(text)
    pat = re.compile(
        r"^\s*(?:\[\[[^\]]*\]\]\s*)?(?:class|struct)\s+" + re.escape(type_name) + r"\b",
        re.MULTILINE,
    )
    for m in pat.finditer(clean):
        brace = clean.find("{", m.end())
        semi = clean.find(";", m.end())
        if brace < 0 or (0 <= semi < brace):
            continue  # 前向声明
        depth = 0
        for i in range(brace, len(clean)):
            if clean[i] == "{":
                depth += 1
            elif clean[i] == "}":
                depth -= 1
                if depth == 0:
                    return clean[brace : i + 1]
    return ""


def has_read_surface(body):
    return ("on_config_loaded" in body) or re.search(r"\bset_config\s*\(", body) is not None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true", help="输出机读 JSON")
    ap.add_argument("--check", action="store_true", help="与 design.md D-1.6.2 统计对拍")
    args = ap.parse_args()

    pairs = defaultdict(lambda: {"count": 0, "files": set(), "names": set()})
    type_instances = defaultdict(int)
    for rel, doc in iter_configs():
        mods = []
        collect_modules(doc, mods)
        for type_name, name, params in mods:
            type_instances[type_name] += 1
            for key in params:
                rec = pairs[(type_name, key)]
                rec["count"] += 1
                rec["files"].add(rel)
                rec["names"].add(name)

    surface = {}
    for type_name in type_instances:
        matched, headers = find_headers(type_name)
        body_hits = [
            (rel, text) for rel, text in headers if has_read_surface(_class_body(text, matched))
        ]
        primary = body_hits[0][0] if body_hits else (headers[0][0] if headers else None)
        surface[type_name] = (primary, bool(body_hits), matched)

    lit_index = _literal_index()
    rows = []
    for (type_name, key) in sorted(pairs):
        rec = pairs[(type_name, key)]
        header, has_surface, matched_name = surface[type_name]
        refs = sorted(lit_index.get(key, set()))
        others = [r for r in refs if r != header]
        if header is None:
            kind = "undefined-type"
        elif has_surface:
            kind = "read-surface"
        elif others:
            kind = "external-ref"  # 无自有读取面,但键名在本类型之外被引用 ⇒ 须人工核实
        else:
            kind = "zero-ref"
        rows.append(
            {
                "type": type_name,
                "key": key,
                "instances": rec["count"],
                "type_instances": type_instances[type_name],
                "files": sorted(rec["files"]),
                "header": header,
                "resolved_from": matched_name if matched_name != type_name else None,
                "read_surface": has_surface if header else None,
                "external_refs": others,
                "kind": kind,
            }
        )

    type_names = sorted({r["type"] for r in rows})
    undefined_types = sorted(t for t, (h, _has, _m) in surface.items() if h is None)
    zero_ref_pairs = sorted({(r["type"], r["key"]) for r in rows if r["kind"] == "zero-ref"})
    external_pairs = sorted({(r["type"], r["key"]) for r in rows if r["kind"] == "external-ref"})
    zero_ref_types = sorted({t for t, _k in zero_ref_pairs})

    if args.json:
        print(
            json.dumps(
                {
                    "type_count": len(type_names),
                    "key_type_pairs": len(rows),
                    "no_read_surface_types": zero_ref_types,
                    "no_read_surface_type_count": len(zero_ref_types),
                    "no_read_surface_pairs": [list(p) for p in zero_ref_pairs],
                    "no_read_surface_pair_count": len(zero_ref_pairs),
                    "no_surface_but_external_ref_pairs": [list(p) for p in external_pairs],
                    "undefined_types": undefined_types,
                    "rows": rows,
                },
                ensure_ascii=False,
                indent=2,
            )
        )
        return 0

    print(f"盘点范围: configs/**/*.json（排除 {EXCLUDE_PREFIX}/**）")
    print()
    print(f"{'类型':32s} {'键':24s} {'实例':>5s}  判定")
    print("-" * 80)
    tags = {
        "read-surface": "有读取面",
        "zero-ref": "无读取面·零引用",
        "external-ref": "无读取面·他处引用",
        "undefined-type": "类型未定义",
    }
    for r in rows:
        print(f"{r['type']:32s} {r['key']:24s} {r['instances']:5d}  {tags[r['kind']]}")
    print("-" * 80)
    print(f"类型数              : {len(type_names)}")
    print(f"键 × 类型对数        : {len(rows)}")
    print(f"类别 ①（零效果强候选）: {len(zero_ref_types)} 类 / {len(zero_ref_pairs)} 键×类型对")
    print(f"  {', '.join(zero_ref_types) if zero_ref_types else '(无)'}")
    print(
        f"无自有读取面但键被他处引用 : {len(external_pairs)} 键×类型对"
        "（板级直读 / 父模块转发 ⇒ 须人工核实，见台账表 A）"
    )
    if undefined_types:
        print(f"⚠️ 配置引用了未定义类型 : {', '.join(undefined_types)}（整份配置不可装配）")

    if args.check:
        ok = True
        if len(zero_ref_types) != EXPECTED_TYPE_COUNT:
            print(
                f"✗ 类别 ① 类型数 {len(zero_ref_types)} != design.md D-1.6.2 的 {EXPECTED_TYPE_COUNT}"
            )
            ok = False
        if len(zero_ref_pairs) != EXPECTED_KEY_TYPE_PAIRS:
            print(
                f"✗ 类别 ① 键×类型对数 {len(zero_ref_pairs)} != design.md D-1.6.2 的 "
                f"{EXPECTED_KEY_TYPE_PAIRS}"
            )
            ok = False
        if ok:
            print("✓ 与 design.md D-1.6.2 统计一致")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
