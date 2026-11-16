#!/usr/bin/env python3
"""R-1 批 3 · Purpose 判据校验器（C-1 ~ C-9）。

用法:
  python3 scripts/test/cep_spec_purpose_batch3_check.py            # 校验：跑 C-1~C-9 并回写 checks
  python3 scripts/test/cep_spec_purpose_batch3_check.py --apply     # 落地：把归因载体的
                                                                      # purpose_text 写进 14 份主 spec
  python3 scripts/test/cep_spec_purpose_batch3_check.py --reconcile # 生成 AC-02b 闭合载体
                                                                      # test/spec_purpose_batch3_reconcile.json

判据来源：openspec/changes/cpptlm-spec-purpose-batch3/design.md §D-3
⛔ 本脚本**只**改 `## Purpose` 段；`## Requirements` 段逐字节不动。
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

ATTR = 'test/spec_purpose_batch3_attribution.json'
RECONCILE = 'test/spec_purpose_batch3_reconcile.json'
SPEC_DIR = 'openspec/specs'

# ── C-4 硬禁：过程叙述 / change 引用 / 日期 ────────────────────────────
FORBIDDEN_NARRATIVE = [
    r'被审查确认', r'早期提案', r'越界做法', r'本规范覆盖', r'本 change',
    r'v\d+\.\d+', r'Phase\s+\d', r'Task\s+\d+', r'Entry\s*#\d+',
    r'\b20\d{2}-\d{2}-\d{2}\b', r'\bper\b', r'proposal', r'归档载体', r'分别见',
]
# ── C-4 附加：纯「Requirement 摘要」信号 ────────────────────────────────
FORBIDDEN_ENUM = [r'本规范覆盖', r'分别见', r'以下', r'包括[^，。]{0,10}等\b']
# ── C-5 硬禁：技术字面量（属 Requirement 的活）──────────────────────────
FORBIDDEN_LITERAL = [r'>>', r'&\s*0x', r'0x[0-9a-fA-F]+', r'[A-Za-z_]+::[A-Za-z_]+']

# ── C-6：spec 级 superseded 的**状态位置**白名单（⛔ 不扫全文）────────────
STATUS_REGIONS = [
    r'\A(?:.*\n){0,10}',                     # 文件前 10 行
    r'^>\s*\*\*[^*]*状态[^*]*\*\*.*$',       # `> **状态**: ...` 行
    r'(?ms)^##\s*Status\b.*?(?=^##\s|\Z)',   # `## Status` 段
    r'(?m)^status\s*:.*$',                   # front-matter
]
# ⚠️ 判据订正(2026-11-16,critic SHOULD_FIX-1):原实现 **大小写敏感**
#    ⇒ 状态位置写小写 `deprecated` / `superseded` 会**漏判**真 superseded
#    ⇒ 加 re.IGNORECASE。
# ⚠️ 词表**故意不含** `SUPERSEDE`(不带 d):spec 的「Status Update」段里
#    `| **supersede** | 原 X |` 的语义是「**本 spec 取代了 X**」
#    ⇒ 命中它 = 假红(critic 亦指出 `:400` 这处「判对但部分靠运气」)。
SUPERSEDED_WORDS = (r'SUPERSEDED|DEPRECATED|OBSOLETE|RETIRED|REPLACED'
                    r'|已废弃|已下线|不再维护|已取代|被取代|退役|停用')
_SUPERSEDED_RE = re.compile(SUPERSEDED_WORDS, re.IGNORECASE)

# ── C-6 领域缩写白名单（C-8 兜底：领域术语，仓内可 grep 但形态各异）─────
DOMAIN_TOKENS = {
    'BAR', 'MMIO', 'AXI', 'ABI', 'PCIE', 'PF', 'VF', 'EP', 'TLP', 'ACS', 'P2P',
    'REBAR', 'PM', 'L1', 'D0', 'D3', 'MSI', 'X', 'SOC', 'FPGA', 'ID', 'IP',
    'AXI4', 'UART', 'DMA', 'SR', 'IOV', 'VM', 'PF0', 'UE', 'JSON', 'YAML',
    'VRAM', 'GMMU', 'SDMA', 'TLB', 'CPU', 'GPU', 'HBM', 'PCIe',
}

STOP_PUNCT = re.compile(r'[\s\*`~，,。、；;：:（）()「」『』"\'\[\]【】<>《》/\\|—\-_.!？?]+')


def norm(s):
    """规范化：去 markdown 强调/代码围栏/空白/标点，仅留字母数字与 CJK。"""
    s = re.sub(r'\*\*|`+', '', s)
    return STOP_PUNCT.sub('', s)


def cjk_only(s):
    return ''.join(re.findall(r'[一-鿿]', s))


def words_of(s):
    return [w.lower() for w in re.findall(r'[A-Za-z][A-Za-z0-9_\-]{2,}', s)]


def lcs_len(a, b):
    """最长公共子串长度（DP）。"""
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


def read_spec(spec):
    return open(os.path.join(SPEC_DIR, spec, 'spec.md'), encoding='utf-8').read()


def split_sections(text):
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
    """仓内 include/ src/ configs/ test/*.cc 是否可 grep 到该标识符（大小写不敏感）。"""
    # ⚠️ NIT-2(critic):原实现递归扫 `test/` 且含 `--include=*.py`
    #    ⇒ 比 design §D-3 文档写的 `test/*.cc` **更宽**（仅存在于 Python 测试的
    #    锚点会通过）⇒ 收窄到与文档一致。
    r = subprocess.run(['grep', '-rIl', '--include=*.hh', '--include=*.h', '--include=*.cc',
                        '--include=*.json', '-i', tok,
                        'include', 'src', 'configs', 'test'],
                       capture_output=True, text=True)
    return bool(r.stdout.strip())


def spec_level_superseded(text):
    """C-6：只认**状态位置**的 superseded 标记（⛔ 不扫全文 —— 全文扫会假红：
    行级历史引用（如「别的 ADR 被取代」）不是本 spec 被取代）。"""
    for region in STATUS_REGIONS:
        for m in re.finditer(region, text, re.M):
            if _SUPERSEDED_RE.search(m.group(0)):
                return True
    return False


def evidence_core_hit(item):
    """C-7 单一判据：证据必须**可复现** ⇔ 核心名命中。
    `archived-change-proposal` ⇒ 工作树文件 isfile
    `git-history-proposal`   ⇒ `git show <fixed-sha>:<path>` 可达（⛔ ref 不可为 HEAD）
    """
    ev = item['source_evidence']
    core = item['source_change']
    if item['evidence_kind'] == 'archived-change-proposal':
        if not os.path.isfile(ev):
            return False, f'证据文件不存在: {ev}'
        body = open(ev, encoding='utf-8').read()
        if core not in body[:2000]:
            return False, f'核心名 {core} 未命中证据前 2000 字符'
        return True, ''
    ref = item.get('source_evidence_ref')
    if not ref:
        return False, 'git-history-proposal 缺 source_evidence_ref'
    if ref in ('HEAD', 'head', 'master', 'main'):
        return False, 'source_evidence_ref 必须是固定 sha（⛔ 不可 HEAD）'
    r = subprocess.run(['git', 'show', f'{ref}:{ev}'], capture_output=True, text=True)
    if r.returncode != 0:
        return False, f'git show {ref}:{ev} 不可达'
    if core not in r.stdout[:2000]:
        return False, f'核心名 {core} 未命中证据前 2000 字符'
    return True, ''


def check_item(item):
    spec = item['spec']
    checks = {}
    detail = []
    text = read_spec(spec)
    pur, reqs = split_sections(text)

    if pur is None:
        return {'ok': False, 'checks': {'structure': False},
                'detail': ['无法定位 ## Purpose / ## Requirements 段']}

    # ── C-1 非占位（限段）───────────────────────────────────────────────
    bad = re.findall(r'TBD|TODO', pur)
    checks['no_placeholder'] = not bad
    if bad:
        detail.append(f'C-1 占位残留: {bad}')

    # ── C-2 长度 + 单物理行 ─────────────────────────────────────────────
    lines = pur.split('\n')
    n = len(pur.strip())
    checks['ge_50_chars'] = n >= 50
    checks['le_160_chars'] = n <= 160
    checks['single_line'] = len([l for l in lines if l.strip()]) == 1
    if not checks['ge_50_chars']:
        detail.append(f'C-2 长度 {n} < 50')
    if not checks['le_160_chars']:
        detail.append(f'C-2 长度 {n} > 160')
    if not checks['single_line']:
        detail.append('C-2 Purpose 非单物理行')

    # ── C-3 不复述 Requirement ──────────────────────────────────────────
    #  ⚠️ 判据订正(2026-11-15 批 1 实测):初稿用「含标识符的 LCS < 12」——
    #    实测**假阳性**:合法产品名 `PcieEndpointIP`(14 字符)与 Requirement 标题
    #    相同即触发 ⇒ 度量把「同名标识符」误判为「复述正文」。
    #  ⇒ 改为两项:**CJK-only LCS** + **英文词序列 LCS**;标识符同名由 C-6/C-8 单独把关。
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

    # ── C-6 spec 级 superseded 互斥 ─────────────────────────────────────
    sup = spec_level_superseded(text)
    checks['claim_kind_consistent'] = not (sup and item['claim_kind'] == 'current')
    if sup and item['claim_kind'] == 'current':
        detail.append('C-6 spec 状态位置含 superseded 标记但 claim_kind=current')

    # ── C-7 归因闭合（单一判据：可复现 + 核心名命中）───────────────────
    ok7, why7 = evidence_core_hit(item)
    checks['evidence_reproducible'] = ok7
    checks['evidence_core_hit'] = ok7
    if not ok7:
        detail.append(f'C-7 {why7}')

    # ── C-8 实施存在性：implementation_anchors 全部命中 ─────────────────
    missing_anchor = []
    for a in item.get('implementation_anchors', []):
        if not repo_has_symbol(a):
            missing_anchor.append(a)
    checks['anchors_resolve'] = not missing_anchor
    if missing_anchor:
        detail.append(f'C-8 实现锚点未命中: {missing_anchor}')

    # ── 单 Requirement 豁免必须起句 ────────────────────────────────────
    if item.get('waiver') == 'single-requirement':
        checks['waiver_lead'] = pur.strip().startswith('为什么需要这条交付判据')
        if not checks['waiver_lead']:
            detail.append('C-3 单 Requirement 豁免未以「为什么需要这条交付判据」起句')

    # ── C-10 载体与 spec 现行文本一致（防「apply 后人为改 spec ⇒ 载体 stale」）──
    checks['carrier_matches_spec'] = (item.get('purpose_text', '').strip() == pur.strip())
    if not checks['carrier_matches_spec']:
        detail.append('C-10 归因载体的 purpose_text 与 spec 现行 Purpose 不一致')

    ok = all(checks.values())
    return {'ok': ok, 'checks': checks, 'detail': detail}


def do_apply(doc):
    n = 0
    for it in doc['items']:
        ok = apply_purpose(it['spec'], it['purpose_text'])
        print(f"  {'✅' if ok else '❌'} {it['spec']}")
        n += ok
    print(f'  ⇒ 已落地 {n}/{len(doc["items"])} 份')
    return 0 if n == len(doc['items']) else 1


def do_reconcile(doc):
    """C-9：AC-02b 全局闭合载体。

    ⛔ **pre 与 post 用同一个 openspec 校验器**（pre = HEAD 基线的 openspec 根，
    跑在 /tmp 的影子根里）⇒ 两侧口径一致，闭合才有意义。
    ⛔ 本函数**只读**，不改任何 spec。
    """
    import collections
    import shutil
    import tempfile

    def scan(root):
        red, kinds = [], collections.Counter()
        specs = os.path.join(root, 'openspec', 'specs')
        for d in sorted(os.listdir(specs)):
            sp = os.path.join(specs, d, 'spec.md')
            if not os.path.isfile(sp) or d.startswith('cep-'):
                continue
            r = subprocess.run(['openspec', 'validate', d, '--type', 'spec', '--strict', '--json'],
                               capture_output=True, text=True, cwd=root)
            try:
                j = json.loads(r.stdout)
            except Exception:
                continue
            it = (j.get('items') or [{}])[0]
            if it.get('valid') is False:
                red.append(d)
                for i in it.get('issues', []):
                    kinds[issue_code(i['message'])] += 1
        return red, kinds

    # ── pre：基线的影子根 ───────────────────────────────────────────────
    # ⚠️ critic 验证建议:基线**不可取 HEAD** —— 落地已 commit 后 HEAD 即 post 态
    #    ⇒ 重跑会得 pre=55/k=0（假失效）。⇒ 基线**显式化**:
    #    `--reconcile --base <sha>` > 已有载体的 base_commit > HEAD
    base = None
    if '--base' in sys.argv:
        base = sys.argv[sys.argv.index('--base') + 1]
    else:
        if os.path.isfile(RECONCILE):
            try:
                base = json.load(open(RECONCILE, encoding='utf-8')).get('base_commit')
            except Exception:
                base = None
    if not base:
        base = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    shadow = tempfile.mkdtemp(prefix='batch3-pre-', dir='/tmp/opencode')
    ar = subprocess.run(f'git archive {base} openspec | tar -x -C {shadow}',
                        shell=True, capture_output=True, text=True)
    if ar.returncode != 0:
        raise SystemExit(f'  ❌ 无法构造基线影子根: {ar.stderr[:200]}')
    pre_red, pre_kinds = scan(shadow)
    post_red, post_kinds = scan(ROOT)

    fixed = sorted(set(pre_red) - set(post_red))
    remaining = []
    for d in sorted(post_red):
        r = subprocess.run(['openspec', 'validate', d, '--type', 'spec', '--strict', '--json'],
                           capture_output=True, text=True)
        reasons = []
        try:
            j = json.loads(r.stdout)
            for i in (j.get('items') or [{}])[0].get('issues', []):
                reasons.append(issue_code(i['message']))
        except Exception:
            pass
        remaining.append({'spec': d, 'reasons': reasons})

    # ── C-9 机判 ──────────────────────────────────────────────────────
    union_ok = (set(post_red) | set(fixed)) == set(pre_red)
    disjoint = not (set(post_red) & set(fixed))
    # ⚠️ critic SHOULD_FIX-2:① `fixed_pure` 原先算了却**未纳入 mc** ⇒ 无机器强制
    #    ② 原实现查的是 fixed 规格的 **post** issues,而 D-3/AC-02c 要求查 **pre**
    #    ⇒ 改为在**影子根**里查 pre issues
    fixed_pure = True
    for d in fixed:
        r = subprocess.run(['openspec', 'validate', d, '--type', 'spec', '--strict', '--json'],
                           capture_output=True, text=True, cwd=shadow)
        try:
            j = json.loads(r.stdout)
            codes = [issue_code(i['message']) for i in (j.get('items') or [{}])[0].get('issues', [])]
            if [c for c in codes if c != 'purpose.placeholder']:
                fixed_pure = False
        except Exception:
            fixed_pure = False
    mc = {
        'fixed_equals_pre_minus_post': fixed == sorted(set(pre_red) - set(post_red)),
        'union_covers_pre': union_ok,
        'disjoint': disjoint,
        # ⚠️ critic:原为硬编码 True(恒真、无判别力)⇒ 改为真判据
        'k_equals_len_fixed': len(fixed) == len(pre_red) - len(post_red),
        # ⚠️ critic:原未纳入 mc ⇒ 现纳入（fixed 的 **pre** issues ⊆ 仅 Purpose 类）
        'fixed_pre_issues_pure_purpose': fixed_pure,
        'remaining_reasons_are_real_codes': all(
            all(reason in post_kinds for reason in e['reasons']) for e in remaining),
    }
    doc_out = {
        'schema': 'spec-purpose-reconcile/v1',
        'batch': 'batch3',
        'base_commit': base,
        'pre': {'red': sorted(pre_red), 'issue_counts': dict(pre_kinds)},
        'post': {'red': sorted(post_red), 'issue_counts': dict(post_kinds)},
        'fixed': fixed,
        'remaining': remaining,
        'k': len(fixed),
        'batch_specs': sorted({it['spec'] for it in doc['items']}),
        'machine_checks': mc,
        'note': 'pre/post 均由 openspec 校验器实跑（pre = HEAD 基线影子根）⇒ 两侧口径一致。',
    }
    shutil.rmtree(shadow, ignore_errors=True)
    with open(RECONCILE, 'w', encoding='utf-8') as fh:
        json.dump(doc_out, fh, ensure_ascii=False, indent=2)
        fh.write('\n')
    print(f'  pre 红 = {len(pre_red)} · post 红 = {len(post_red)} · k = {len(fixed)}')
    print(f'  pre issue 分布: {dict(pre_kinds)}')
    print(f'  post issue 分布: {dict(post_kinds)}')
    print(f'  机判: {mc}')
    ok = all(mc.values())
    print(f'  ⇒ {"✅ 闭合成立" if ok else "❌ 闭合不成立"}')
    return 0 if ok else 1


def issue_code(msg):
    if 'placeholder' in msg:
        return 'purpose.placeholder'
    if 'very long' in msg:
        return 'requirement.too_long'
    if 'at least one scenario' in msg:
        return 'requirement.no_scenario'
    return msg[:60]


def main():
    doc = json.load(open(ATTR, encoding='utf-8'))
    if '--apply' in sys.argv:
        return do_apply(doc)
    if '--reconcile' in sys.argv:
        return do_reconcile(doc)

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
