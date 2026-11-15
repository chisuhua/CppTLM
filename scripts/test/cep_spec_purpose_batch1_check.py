#!/usr/bin/env python3
"""R-1 批 1 · Purpose 判据校验器（C-1 ~ C-7）。

用法:
  python3 scripts/test/cep_spec_purpose_batch1_check.py --apply   # 落地：把归因载体的
                                                                  # purpose_text 写进 16 份主 spec
  python3 scripts/test/cep_spec_purpose_batch1_check.py           # 校验：跑 C-1~C-7 并回写 checks

判据来源：openspec/changes/cpptlm-spec-purpose-batch1/design.md §D-3
⛔ 本脚本**只**改 `## Purpose` 段；`## Requirements` 段逐字节不动。
"""
import json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

ATTR = 'test/spec_purpose_batch1_attribution.json'
SPEC_DIR = 'openspec/specs'

# ── C-4 硬禁：过程叙述 / change 引用 / 日期 ───────────────────────────────
FORBIDDEN_NARRATIVE = [
    r'被审查确认', r'早期提案', r'越界做法', r'本规范覆盖', r'本 change',
    r'v\d+\.\d+', r'Phase\s+\d', r'Task\s+\d+', r'Entry\s*#\d+',
    r'\b20\d{2}-\d{2}-\d{2}\b', r'\bper\b', r'proposal', r'归档载体',
]
# ── C-5 硬禁：技术字面量（属 Requirement 的活）────────────────────────────
FORBIDDEN_LITERAL = [r'>>', r'&\s*0x', r'0x[0-9a-fA-F]+', r'[A-Za-z_]+::[A-Za-z_]+']
# ── C-4 附加：纯「Requirement 摘要」信号 ────────────────────────────────
FORBIDDEN_ENUM = [r'本规范覆盖', r'分别见', r'以下', r'包括[^，。]{0,10}等\b']

# 领域缩写白名单（C-6 兜底：这些是**领域术语**，仓内可 grep 但大小写/形态各异）
DOMAIN_TOKENS = {
    'BAR', 'MMIO', 'AXI', 'ABI', 'PCIE', 'PF', 'VF', 'EP', 'TLP', 'ACS', 'P2P',
    'REBAR', 'PM', 'L1', 'D0', 'D3', 'MSI', 'X', 'SOC', 'FPGA', 'ID', 'IP',
    'AXI4', 'UART', 'DMA', 'SR', 'IOV', 'VM', 'PF0', 'UE',
}

STOP_PUNCT = re.compile(r'[\s\*`~，,。、；;：:（）()「」『』"\'\[\]【】<>《》/\\|—\-_.!？?；;]+')


def norm(s):
    """规范化：去 markdown 强调/代码围栏/空白/标点，仅留字母数字与 CJK。"""
    s = re.sub(r'\*\*|`+', '', s)
    return STOP_PUNCT.sub('', s)


def cjk_only(s):
    return ''.join(re.findall(r'[\u4e00-\u9fff]', s))


def words_of(s):
    return [w.lower() for w in re.findall(r'[A-Za-z][A-Za-z0-9_\-]{2,}', s)]


def lcs_word_seq(a, b):
    """最长公共**连续词序列**长度（词）。"""
    best = 0
    for i in range(len(a)):
        for j in range(i + best + 1, len(a) + 1):
            seq = a[i:j]
            if any(seq == b[k:k + len(seq)] for k in range(len(b) - len(seq) + 1)):
                if len(seq) > best:
                    best = len(seq)
            else:
                break
    return best


def lcs_len(a, b):
    """最长公共子串长度（DP，O(n·m)）。"""
    if not a or not b:
        return 0
    prev = [0] * (len(b) + 1)
    best = 0
    for i in range(1, len(a) + 1):
        cur = [0] * (len(b) + 1)
        ai = a[i - 1]
        for j in range(1, len(b) + 1):
            if ai == b[j - 1]:
                cur[j] = prev[j - 1] + 1
                if cur[j] > best:
                    best = cur[j]
        prev = cur
    return best


def read_spec(spec):
    p = os.path.join(SPEC_DIR, spec, 'spec.md')
    return open(p, encoding='utf-8').read()


def split_sections(text):
    """⇒ (purpose_body, requirements_body)；缺段 ⇒ (None, None)"""
    m = re.search(r'(?s)^## Purpose\s*\n(.*?)\n## Requirements\s*\n(.*)\Z', text, re.M)
    if not m:
        return None, None
    return m.group(1), m.group(2)


def apply_purpose(spec, purpose_text):
    """只替换 ## Purpose 段正文；## Requirements 段逐字节不动。"""
    p = os.path.join(SPEC_DIR, spec, 'spec.md')
    text = open(p, encoding='utf-8').read()
    new, n = re.subn(r'(?s)(^## Purpose\s*\n).*?(\n## Requirements\s*\n)',
                     lambda m: m.group(1) + purpose_text + m.group(2), text, count=1, flags=re.M)
    if n != 1:
        return False
    open(p, 'w', encoding='utf-8').write(new)
    return True


def repo_has_symbol(tok):
    """仓内 include/ src/ configs/ 是否可 grep 到该标识符（大小写不敏感）。"""
    r = subprocess.run(['grep', '-rIl', '--include=*.hh', '--include=*.h', '--include=*.cc',
                        '--include=*.json', '--include=*.py', '-i', tok,
                        'include', 'src', 'configs'],
                       capture_output=True, text=True)
    return bool(r.stdout.strip())


def check_item(item):
    spec = item['spec']
    checks = {}
    detail = []
    text = read_spec(spec)
    pur, reqs = split_sections(text)

    if pur is None:
        return {'ok': False, 'checks': {'structure': False}, 'detail': ['无法定位 ## Purpose / ## Requirements 段']}

    # ── C-1 非占位（限段）───────────────────────────────────────────────
    bad = re.findall(r'TBD|TODO', pur)
    checks['no_placeholder'] = not bad
    if bad:
        detail.append(f'C-1 占位残留: {bad}')

    # ── C-2 长度 + 单物理行 ─────────────────────────────────────────────
    lines = pur.split('\n')
    checks['ge_50_chars'] = len(pur.strip()) >= 50
    checks['le_160_chars'] = len(pur.strip()) <= 160
    checks['single_line'] = len([l for l in lines if l.strip()]) == 1
    if not checks['ge_50_chars']:
        detail.append(f'C-2 长度 {len(pur.strip())} < 50')
    if not checks['le_160_chars']:
        detail.append(f'C-2 长度 {len(pur.strip())} > 160')
    if not checks['single_line']:
        detail.append('C-2 Purpose 非单物理行')

    # ── C-3 不复述 Requirement ────────────────────────────────────────
    #  ⚠️ 判据订正(2026-11-15 实施时实测):初稿用「含标识符的 LCS < 12」——
    #    实测**假阳性**:合法产品名 `PcieEndpointIP`(14 字符)与 Requirement 标题
    #    相同即触发 ⇒ 度量把「同名标识符」误判为「复述正文」。
    #  ⇒ 改为两项:**CJK-only LCS**(中文表述是否复述)+ **英文词序列 LCS**
    #    (是否逐字搬运英文句子);标识符同名由 C-6(可追性)单独把关。
    np_ = norm(pur)
    nr = norm(reqs)
    l_cjk = lcs_len(cjk_only(np_), cjk_only(nr))
    l_word = lcs_word_seq(words_of(np_), words_of(nr))
    thr = 16 if item.get('waiver') == 'single-requirement' else 12
    checks['lcs_below_threshold'] = l_cjk < thr and l_word < 5
    if l_cjk >= thr:
        detail.append(f'C-3 CJK-LCS={l_cjk} ≥ 阈值 {thr}（复述 Requirement 中文表述）')
    if l_word >= 5:
        detail.append(f'C-3 英文词序列公共长度={l_word} ≥ 5（逐字搬运 Requirement 英文句）')

    # ── C-4 无过程叙述 ─────────────────────────────────────────────────
    hits = [p for p in FORBIDDEN_NARRATIVE if re.search(p, pur)]
    hits += [p for p in FORBIDDEN_ENUM if re.search(p, pur)]
    checks['no_process_narrative'] = not hits
    if hits:
        detail.append(f'C-4 过程叙述/摘要信号: {hits}')

    # ── C-5 无技术字面量 ───────────────────────────────────────────────
    lits = [p for p in FORBIDDEN_LITERAL if re.search(p, pur)]
    checks['no_technical_literal'] = not lits
    if lits:
        detail.append(f'C-5 技术字面量: {lits}')

    # ── C-6 不声称不存在的能力 ─────────────────────────────────────────
    #  ① claim_kind 与 spec 内 Superseded 标记互斥
    superseded = bool(re.search(r'Superseded|物理删除|SUPERSEDED', text))
    checks['claim_kind_consistent'] = not (superseded and item['claim_kind'] == 'current')
    if superseded and item['claim_kind'] == 'current':
        detail.append('C-6 spec 含 Superseded 标记但 claim_kind=current')
    #  ② Purpose 里的英文标识符必须可追（Requirement 标题 ∪ 仓内符号 ∪ 领域词）
    toks = set(re.findall(r'[A-Za-z][A-Za-z0-9_\-]{2,}', pur))
    untraceable = []
    for t in sorted(toks):
        if t.upper() in DOMAIN_TOKENS or t in DOMAIN_TOKENS:
            continue
        if t.lower() in re.sub(r'[^a-z0-9\-]', '', ' '.join(item['requirement_titles']).lower()):
            continue
        if t.lower() in nr.lower():
            continue
        if repo_has_symbol(t):
            continue
        untraceable.append(t)
    checks['symbols_traceable'] = not untraceable
    if untraceable:
        detail.append(f'C-6 不可追标识符: {untraceable}')

    # ── C-7 归因闭合 ───────────────────────────────────────────────────
    ev = item['source_evidence']
    ev_ok = os.path.isfile(ev)
    kw_ok = False
    if ev_ok:
        body = open(ev, encoding='utf-8').read()
        # 关键字命中：来源 change 的**核心名**（去掉日期前缀）须出现在证据文件前 2000 字符内
        core = re.sub(r'^(?:\d{4}-\d{2}-\d{2}-)+', '', item['source_change'])
        kw_ok = core in body[:2000]
    checks['evidence_exists'] = ev_ok
    checks['evidence_keyword_hit'] = kw_ok
    if not ev_ok:
        detail.append(f'C-7 证据文件不存在: {ev}')
    elif not kw_ok:
        detail.append(f'C-7 证据文件未命中来源 change 核心名: {item["source_change"]}')

    # ── 单 Requirement 豁免必须起句 ────────────────────────────────────
    if item.get('waiver') == 'single-requirement':
        checks['waiver_lead'] = pur.strip().startswith('为什么需要这条交付判据')
        if not checks['waiver_lead']:
            detail.append('C-3 单 Requirement 豁免未以「为什么需要这条交付判据」起句')

    ok = all(checks.values())
    return {'ok': ok, 'checks': checks, 'detail': detail}


def main():
    doc = json.load(open(ATTR, encoding='utf-8'))
    apply_mode = '--apply' in sys.argv

    if apply_mode:
        n = 0
        for it in doc['items']:
            ok = apply_purpose(it['spec'], it['purpose_text'])
            print(f"  {'✅' if ok else '❌'} {it['spec']}")
            n += ok
        print(f'  ⇒ 已落地 {n}/{len(doc["items"])} 份')
        return 0 if n == len(doc['items']) else 1

    results = []
    for it in doc['items']:
        r = check_item(it)
        results.append((it['spec'], r))
        it['checks'] = r['checks']
    with open(ATTR, 'w', encoding='utf-8') as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2)
        fh.write('\n')

    fails = 0
    for spec, r in results:
        if r['ok']:
            print(f'  ✅ {spec}')
        else:
            fails += 1
            print(f'  ❌ {spec}')
            for d in r['detail']:
                print(f'       · {d}')
    print(f'\n  ⇒ 通过 {len(results)-fails}/{len(results)}')
    return 0 if fails == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
