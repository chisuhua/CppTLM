#!/usr/bin/env python3
# scripts/test/cep_b3b2_d4_probe_validate.py
#   —— `B3b-2` D-4 触发探测清单的**校验器**(可对任意 JSON 文件运行)
#
# 校验 5 组(per 台账 Entry #274 §④ E1 + metis gap 分析的 P0-1~P0-4 / G1~G5):
#   ① **key 白名单**:probe / source / criterion 的键名必须在白名单内
#      (⇒ **「投喂正文」在 schema 里没有合法落点** —— `body` / `snippet` / `excerpt` /
#      `content` / `text` 一律不存在)
#   ② **URL pattern**:元数据来源的 URL 不得命中「正文直链」pattern
#   ③ **page_type 枚举** + **banned token** + **判据语法**({field, op, value} + op 白名单)
#   ④ **覆盖谓词**:4 族 × 2 路径 = 8 槽位逐格 ≥1 条探测项(族名单**从 deferrals JSON 读**,
#      ⛔ 不硬编码);`must_all` ≥2 子条件;① 类 `search_sources` ≥2 且 `page_type` 互异
#   ⑤ **防漂移**:每条 probe 的 `deferral_trigger_sha256` == 该族 `trigger_condition` 的 sha256
#
# 输出:每个失败项一行「签名: 详情」;rc=0 表示全绿。
# ⚠️ 本校验器**不**改任何文件(纯读)。
import hashlib
import json
import re
import sys

ALLOWED_OPS = {"eq", "ne", "in", "regex", "gte"}
BODY_FIELD_NAMES = {"body", "snippet", "excerpt", "content", "text", "raw", "full_text"}
KEY_WHITELIST = {
    "probe": {"id", "family", "path", "check_type", "source_event_id", "family_effect",
              "search_goal", "search_sources", "artifact_glob", "signer_fields", "must_all",
              "on_hit", "deferral_trigger_sha256"},
    "source": {"host", "url", "page_type", "title_hint"},
    "criterion": {"field", "op", "value"},
}


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: cep_b3b2_d4_probe_validate.py <probe.json> <deferrals.json>")
        return 2
    probe_path, deferrals_path = sys.argv[1], sys.argv[2]
    errors: list[str] = []

    try:
        doc = json.load(open(probe_path, encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        print(f"PROBE-JSON-PARSE: {exc}")
        return 1
    try:
        deferrals = json.load(open(deferrals_path, encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        print(f"DEFERRALS-JSON-PARSE: {exc}")
        return 1

    probes = doc.get("probes", [])
    allowed_page_types = set(doc.get("allowed_page_types", []))
    forbidden_patterns = [re.compile(p) for p in doc.get("forbidden_url_patterns", [])]
    banned_tokens = [t for t in doc.get("banned_tokens", []) if t]

    # ── ① key 白名单(逐层)─────────────────────────────────────────────
    for pr in probes:
        pid = pr.get("id", "<no-id>")
        for k in pr:
            if k not in KEY_WHITELIST["probe"]:
                errors.append(f"BODY-FIELD-FORBIDDEN: {pid}: 未知 probe 键 '{k}'"
                              + ("(⛔ 正文类字段名)" if k.lower() in BODY_FIELD_NAMES else ""))
        for src in pr.get("search_sources", []) or []:
            for k in src:
                if k not in KEY_WHITELIST["source"]:
                    errors.append(f"BODY-FIELD-FORBIDDEN: {pid}: 未知 source 键 '{k}'"
                                  + ("(⛔ 正文类字段名)" if k.lower() in BODY_FIELD_NAMES else ""))
        for cr in pr.get("must_all", []) or []:
            for k in cr:
                if k not in KEY_WHITELIST["criterion"]:
                    errors.append(f"BODY-FIELD-FORBIDDEN: {pid}: 未知 criterion 键 '{k}'"
                                  + ("(⛔ 正文类字段名)" if k.lower() in BODY_FIELD_NAMES else ""))

    # ── ② URL pattern(元数据位不得指向正文直链)────────────────────────
    for pr in probes:
        pid = pr.get("id", "<no-id>")
        for src in pr.get("search_sources", []) or []:
            url = str(src.get("url", ""))
            for pat in forbidden_patterns:
                if pat.search(url):
                    errors.append(f"DOC-URL-IN-METADATA-SLOT: {pid}: {url} 命中 {pat.pattern}")

    # ── ③ page_type 枚举 / banned token / 判据语法 ────────────────────
    for pr in probes:
        pid = pr.get("id", "<no-id>")
        for src in pr.get("search_sources", []) or []:
            pt = src.get("page_type")
            if allowed_page_types and pt not in allowed_page_types:
                errors.append(f"PAGE-TYPE-NOT-ALLOWED: {pid}: {pt}")
        # banned token 不得出现在**判据文本**(search_goal / on_hit / criterion value)
        texts = [str(pr.get("search_goal", "")), str(pr.get("on_hit", ""))]
        for cr in pr.get("must_all", []) or []:
            texts.append(str(cr.get("field", "")))
            texts.append(str(cr.get("value", "")))
        for txt in texts:
            for tok in banned_tokens:
                if tok in txt:
                    errors.append(f"SUBJECTIVE-CRITERION: {pid}: 判据文本含主观词 '{tok}'")
        for cr in pr.get("must_all", []) or []:
            op = cr.get("op")
            if op not in ALLOWED_OPS:
                errors.append(f"CRITERION-OP-INVALID: {pid}: op='{op}'")
            if "field" not in cr or "value" not in cr:
                errors.append(f"CRITERION-SYNTAX: {pid}: 判据须含 field / op / value")

    # ── ④ 覆盖谓词(族名单从 deferrals 读)─────────────────────────────
    families = [r["family"] for r in deferrals.get("deferrals", [])]
    paths = {"ext_readable", "internal_excerpt"}
    slots = {(f, p) for f in families for p in paths}
    covered = {(pr.get("family"), pr.get("path")) for pr in probes}
    for fam, pth in sorted(slots):
        if (fam, pth) not in covered:
            errors.append(f"COVERAGE-GAP: {fam}/{pth}")
    for pr in probes:
        pid = pr.get("id", "<no-id>")
        if (pr.get("family"), pr.get("path")) not in slots:
            errors.append(f"COVERAGE-SLOT-UNKNOWN: {pid}: {pr.get('family')}/{pr.get('path')}")
        if len(pr.get("must_all", []) or []) < 2:
            errors.append(f"MUST-ALL-TOO-FEW: {pid}: must_all 少于 2 个子条件")
        if pr.get("check_type") == "external_metadata":
            srcs = pr.get("search_sources", []) or []
            if len(srcs) < 2:
                errors.append(f"SOURCE-TOO-FEW: {pid}: ① 类探测项须 ≥2 个检索源")
            ptypes = [s.get("page_type") for s in srcs]
            if len(set(ptypes)) != len(ptypes):
                errors.append(f"SOURCE-PAGE-TYPE-DUP: {pid}: 检索源的 page_type 须互异")
        elif pr.get("check_type") == "internal_artifact":
            if not pr.get("artifact_glob"):
                errors.append(f"ARTIFACT-GLOB-MISSING: {pid}: ② 类须声明 artifact_glob")
        else:
            errors.append(f"CHECK-TYPE-INVALID: {pid}: check_type='{pr.get('check_type')}'")

    # ── ⑤ 防漂移(deferral_trigger_sha256)──────────────────────────────
    for pr in probes:
        pid = pr.get("id", "<no-id>")
        fam = pr.get("family")
        row = next((r for r in deferrals.get("deferrals", []) if r.get("family") == fam), None)
        if row is None:
            errors.append(f"DEFERRRAL-FAMILY-UNKNOWN: {pid}: {fam}")
            continue
        want = hashlib.sha256(str(row.get("trigger_condition", "")).encode("utf-8")).hexdigest()
        if pr.get("deferral_trigger_sha256") != want:
            errors.append(f"DEFERRRAL-TRIGGER-DRIFT: {pid}: deferral_trigger_sha256 与 "
                          f"deferrals[{fam}].trigger_condition 不一致")

    if errors:
        for e in errors:
            print(e)
        return 1
    print(f"PROBE-CLEAN: {len(probes)} probes / {len(slots)} slots covered")
    return 0


if __name__ == "__main__":
    sys.exit(main())
