#!/usr/bin/env python3
"""scripts/cep/parity/main_tier_collect.py
   —— 主对拍档:把各条流的结果汇总成**数组** JSON

⚠️ 早期版本直接在 bash 里字符串拼接 ⇒ 产生「Extra data」的**非法 JSON**;
   ⇒ 改为「每条流一个独立文件 + 这里读成数组」。

用法:python3 main_tier_collect.py <stream_dir> <out.json>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) < 3:
        raise SystemExit("用法:python3 main_tier_collect.py <stream_dir> <out.json>")
    src_dir, out = sys.argv[1], sys.argv[2]
    recs = []
    for p in sorted(pathlib.Path(src_dir).glob("stream_*.json")):
        recs.append(json.loads(p.read_text(encoding="utf-8")))
    pathlib.Path(out).write_text(
        json.dumps(recs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[collect] 汇总 {len(recs)} 条流 → {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
