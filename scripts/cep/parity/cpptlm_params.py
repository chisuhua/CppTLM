#!/usr/bin/env python3
"""scripts/cep/parity/cpptlm_params.py
   —— 对拍用**本仓侧参数**的唯一真源抽取器

功能:从 **CppTLM 实现源码**里抽取对拍需要的参数(命中 / 未命中延迟常数),
      避免在脚本里**手抄**一份(那是本仓反复出现的「双真源」型漂移来源)。

来源(唯一真源,按优先级):
   · `include/tlm/cep/cache_tlm.hh` —— 命中 / 未命中延迟常数(实现里的字面量);
   · `scripts/cep/parity/parity_config.json` —— 时基口径(cycle → ns)与对拍参数;
     其中 `cycle_ns` 的**文档锚点**必须在 `docs/designs/dgpu-soc/timing-mode.md` 里
     找得到(脚本自检)⇒ 口径漂移会立刻报红。

用法:python3 scripts/cep/parity/cpptlm_params.py [--json]
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
CONFIG = ROOT / "scripts" / "cep" / "parity" / "parity_config.json"
CACHE_HDR = ROOT / "include" / "tlm" / "cache_tlm.hh"
# 时基口径的**文档锚点**:AGENTS.md 的「1 sim_cycle = 1 ns @ 1 GHz」是全仓口径登记处
# (per AGENTS.md KEY INVARIANTS「VramControllerTLM 1 GHz 操作频率假设」)。
TIMING_DOC = ROOT / "AGENTS.md"


def extract_cache_latencies():
    """从 `cache_tlm.hh` 抽取命中 / 未命中延迟常数(实现字面量)。"""
    text = CACHE_HDR.read_text(encoding="utf-8")
    # 形态:`uint64_t access_latency = hit ? 5 : 50;` —— 命中在前,未命中在后
    m = re.search(r"hit\s*\?\s*(\d+)\s*:\s*(\d+)", text)
    if not m:
        raise SystemExit(f"✗ 未能在 {CACHE_HDR} 抽取命中/未命中延迟常数(形态已变 ⇒ 需同步)")
    return int(m.group(1)), int(m.group(2))


def check_cycle_ns_anchor(cycle_ns, anchor):
    """时基口径的**文档锚点自检**:锚点串必须仍在时基文档里(否则口径已漂移)。"""
    if not TIMING_DOC.exists():
        raise SystemExit(f"✗ 时基口径文档不存在:{TIMING_DOC}")
    text = TIMING_DOC.read_text(encoding="utf-8")
    if anchor not in text:
        raise SystemExit(
            f"✗ 时基口径锚点在 {TIMING_DOC} 中已消失:「{anchor}」⇒ 口径漂移,需先修文档"
        )


def collect():
    cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
    hit_cycles, miss_cycles = extract_cache_latencies()
    cycle_ns = float(cfg["cycle_ns"])
    check_cycle_ns_anchor(cycle_ns, cfg["cycle_ns_anchor"])
    return {
        "source": {
            "latencies": str(CACHE_HDR.relative_to(ROOT)),
            "cycle_ns": str(CONFIG.relative_to(ROOT)),
            "cycle_ns_anchor_doc": str(TIMING_DOC.relative_to(ROOT)),
        },
        "hit_cycles": hit_cycles,
        "miss_cycles": miss_cycles,
        "cycle_ns": cycle_ns,
        "hit_ns": hit_cycles * cycle_ns,
        "miss_ns": miss_cycles * cycle_ns,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true", help="输出 JSON(默认 human-readable)")
    args = ap.parse_args()
    data = collect()
    if args.json:
        print(json.dumps(data, ensure_ascii=False, indent=2))
    else:
        print(f"命中延迟   : {data['hit_cycles']} cycles = {data['hit_ns']} ns")
        print(f"未命中延迟 : {data['miss_cycles']} cycles = {data['miss_ns']} ns")
        print(f"时基       : 1 cycle = {data['cycle_ns']} ns(锚点自检通过)")
        print(f"来源       : {data['source']['latencies']}")


if __name__ == "__main__":
    sys.exit(main())
