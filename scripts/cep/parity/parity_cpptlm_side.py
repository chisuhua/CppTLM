#!/usr/bin/env python3
"""scripts/cep/parity/parity_cpptlm_side.py
   —— 常数档的 **CppTLM 侧**结果 JSON 生成器

常数档(辅助档)与参数同源档不同:它**没有**独立的 trace 回放 ⇒ CppTLM 侧的
「测量结果」就是**从实现源码抽取的参数**(命中 / 未命中延迟常数 + cycle↔ns 换算),
由 `cpptlm_params.py` 负责抽取(避免手抄副本)。

⚠️ **P-5**(per `14 号` §4.4 / `ADR-CEP-06 §3`):必须带 `verification_mode` +
   `verification_scope` ⇒ 取值从 `parity_config.json` 的 `verification` 段注入
   (唯一真源 ⇒ 两侧必然一致)。

用法:python3 scripts/cep/parity/parity_cpptlm_side.py <out.json>
"""
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import cpptlm_params  # noqa: E402


def main():
    if len(sys.argv) < 2:
        raise SystemExit(
            "用法:python3 scripts/cep/parity/parity_cpptlm_side.py <out.json>")
    out_path = pathlib.Path(sys.argv[1])
    out_path.parent.mkdir(parents=True, exist_ok=True)

    cfg = json.loads((HERE / "parity_config.json").read_text(encoding="utf-8"))
    ver = cfg.get("verification", {})

    d = cpptlm_params.collect()
    d["tool"] = "cpptlm"
    d["mode"] = "constant_tier"
    n = int(cfg["gem5"]["tester"]["num_accesses"])
    d["num_accesses"] = n
    d["accesses"] = n
    # P-5:verification 声明(两侧同源 ⇒ 取值一致)
    d["verification_mode"] = ver.get("verification_mode")
    d["verification_scope"] = ver.get("verification_scope")
    # 命中/未命中计数在常数档**不参与判定**(该档只比延迟常数)⇒ 留空
    d["hits"] = None
    d["misses"] = None
    d["miss_rate"] = None
    d["num_reads"] = None
    d["num_writes"] = None

    out_path.write_text(json.dumps(d, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8")
    print(f"[parity] CppTLM 侧结果已写出:{out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
