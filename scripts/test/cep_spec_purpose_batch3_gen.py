#!/usr/bin/env python3
"""R-1 批 3 · 归因载体生成器（⛔ 不用 spec 的 TBD 行 —— 批 1 实测其名 ≠ 归档目录名）。

从 `openspec/changes/cpptlm-spec-purpose-batch3/design.md` §D-1 表格**机器提取**
14 条 canonical Purpose 草稿 + 从同文件的「可追性核对」表提取 `implementation_anchors`
⇒ 生成 `test/spec_purpose_batch3_attribution.json` 的**原始输入**。

⚠️ `checks` 字段**不由本脚本填写**（自证无判别力）—— 由
   `scripts/test/cep_spec_purpose_batch3_check.py` 在校验时重算并回写。

用法:
  python3 scripts/test/cep_spec_purpose_batch3_gen.py        # 生成归因载体
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

DESIGN = 'openspec/changes/cpptlm-spec-purpose-batch3/design.md'
OUT = 'test/spec_purpose_batch3_attribution.json'

# spec → (claim_kind, evidence_kind, 归档目录后缀键 / 固定 git ref, source_change 名)
META = {
    'dgpu-4ip-port-contract': ('current', 'archived-change-proposal', 'dgpu-realip-pin-contract', None),
    'dgpu-board-abi-hygiene': ('current', 'archived-change-proposal', 'cpptlm-dgpu-board-abi-hygiene', None),
    'dgpu-board-adapter-info': ('current', 'archived-change-proposal', 'dgpu-board-adapter-info-extension', None),
    'dgpu-soc-timing': ('current', 'git-history-proposal', None, 'cpptlm-dgpu-soc-timing-mvp'),
    'driver-visible-minimal-soc': ('current', 'git-history-proposal', None, 'cpptlm-driver-visible-minimal-soc'),
    'gmmu-pte-wf': ('current', 'archived-change-proposal', 'cpptlm-gmmu-pte-bus-correction', None),
    'lwc-bus-hub-mvp': ('current', 'archived-change-proposal', 'cpptlm-lwc-bus-hub-mvp', None),
    'minimal-dgpu-soc-abi-landing': ('current', 'archived-change-proposal', 'cpptlm-minimal-dgpu-soc-v1-landing', None),
    'minimal-dgpu-soc-architecture': ('current', 'archived-change-proposal', 'cpptlm-dgpu-soc-v1-architecture', None),
    'minimal-dgpu-soc-test-coverage': ('current', 'archived-change-proposal', 'cpptlm-minimal-dgpu-soc-v1-test-coverage', None),
    'sdma-engine-tlm': ('current', 'archived-change-proposal', 'cpptlm-dgpu-sdma-engine', None),
    'soc-internal-noc': ('current', 'archived-change-proposal', 'cpptlm-dgpu-soc-internal-noc-minimal', None),
    'soc-internal-noc-data-flow': ('current', 'archived-change-proposal', 'cpptlm-dgpu-soc-internal-noc-e1', None),
    'vram-controller-functional-mode': ('current', 'archived-change-proposal', 'cpptlm-vram-controller-functional-integration', None),
}

# git-history-proposal 的**固定** ref（⛔ 不可写 HEAD —— 会随 commit 漂移）
GIT_REF = {
    'cpptlm-dgpu-soc-timing-mvp': '0345d283',
    'cpptlm-driver-visible-minimal-soc': '689752ef',
}


def resolve_archive(key):
    """归档目录（名字带**双日期前缀** ⇒ 由 glob 解析，⛔ 不硬编码全名）。
    ⚠️ 必须**唯一命中** —— 命中 0 或 ≥2 即报错退出。"""
    hits = sorted(h for h in glob.glob(os.path.join(ROOT, 'openspec/changes/archive', '*' + key))
                  if os.path.isdir(h))
    if len(hits) != 1:
        raise SystemExit(f'  ❌ 来源 change 归档目录必须唯一命中，实得 {len(hits)}: *{key}*')
    return os.path.basename(hits[0])


def read_design():
    text = open(DESIGN, encoding='utf-8').read()
    sec = text[text.find('## D-1 ·'):text.find('## D-1 末 ·')]
    drafts, anchors = {}, {}
    for line in sec.split('\n'):
        if not line.startswith('| ') or line.startswith('|---'):
            continue
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        if len(cells) < 4:
            continue
        m = re.fullmatch(r'`([a-z0-9\-]+)`', cells[1])
        if m and cells[2]:
            drafts[m.group(1)] = cells[2]
    # 可追性核对表：`| # | spec | anchors | 实测 |`
    tail = text[text.find('可追性核对'):]
    for line in tail.split('\n'):
        if not line.startswith('| ') or line.startswith('|---'):
            continue
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        if len(cells) < 4:
            continue
        m = re.fullmatch(r'`([a-z0-9\-]+)`', cells[1])
        if not m:
            continue
        toks = re.findall(r'`([^`]+)`', cells[2])
        if toks:
            anchors[m.group(1)] = toks
    return drafts, anchors


def main():
    drafts, anchors = read_design()
    print(f'  从 design.md §D-1 提取到 {len(drafts)} 条草稿 / {len(anchors)} 条锚点')
    missing = set(META) - set(drafts)
    extra = set(drafts) - set(META)
    if missing or extra:
        raise SystemExit(f'  ❌ 缺: {sorted(missing)} · 多: {sorted(extra)}')

    items = []
    for spec, (kind, ev_kind, key, chg) in META.items():
        if ev_kind == 'archived-change-proposal':
            arch_dir = resolve_archive(key)
            # ⚠️ 核心名 = **去日期前缀**（归档目录名带双日期前缀；proposal 里只有核心名）
            chg = re.sub(r'^(?:\d{4}-\d{2}-\d{2}-)+', '', arch_dir)
            ev = f'openspec/changes/archive/{arch_dir}/proposal.md'
            ref = None
        else:
            chg = chg or key
            ev = f'openspec/changes/{chg}/proposal.md'
            ref = GIT_REF[chg]
            # 证据必须可由 git show 复现
            import subprocess
            r = subprocess.run(['git', 'show', f'{ref}:{ev}'], capture_output=True, text=True)
            if r.returncode != 0:
                raise SystemExit(f'  ❌ {spec}: git show {ref}:{ev} 不可达')
            if chg not in r.stdout[:2000]:
                raise SystemExit(f'  ❌ {spec}: 核心名 {chg} 未命中证据前 2000 字符')
        sp = f'openspec/specs/{spec}/spec.md'
        t = open(sp, encoding='utf-8').read()
        reqs = re.findall(r'(?m)^### Requirement: (.+)$', t)
        anc = anchors.get(spec)
        if not anc:
            raise SystemExit(f'  ❌ {spec}: design.md 可追性核对表未给 implementation_anchors')
        item = {
            'spec': spec,
            'claim_kind': kind,
            'evidence_kind': ev_kind,
            'source_change': chg,
            'source_evidence': ev,
            'requirement_titles': reqs,
            'implementation_anchors': anc,
            'purpose_text': drafts[spec],
            'waiver': 'single-requirement' if len(reqs) == 1 else None,
        }
        if ref:
            item['source_evidence_ref'] = ref
        if ev_kind == 'archived-change-proposal':
            item['source_archive_dir'] = arch_dir
        items.append(item)

    doc = {
        'schema': 'spec-purpose-attribution/v2',
        'batch': 'batch3',
        'archive_semantics': 'completed-direct-main-spec',
        'note': 'checks 字段由 scripts/test/cep_spec_purpose_batch3_check.py 重算写入，⛔ 不由人手填写。'
                'source_evidence_ref 必须是固定 sha（⛔ 不可写 HEAD）。',
        'items': items,
    }
    with open(OUT, 'w', encoding='utf-8') as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2)
        fh.write('\n')
    print(f'  ✅ 写入 {OUT}（{len(items)} 条）')
    for it in items:
        n = len(it['purpose_text'])
        flag = '✅' if 50 <= n <= 160 else '⚠️ '
        print(f"    {flag} {it['spec']:44} len={n:4} reqs={len(it['requirement_titles']):2} "
              f"anchors={len(it['implementation_anchors'])} {it['evidence_kind']}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
