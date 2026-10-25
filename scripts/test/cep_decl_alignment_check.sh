#!/usr/bin/env bash
# scripts/test/cep_decl_alignment_check.sh
#   —— CEP 协议实例声明 ↔ 本仓 schema / fabric 的**对齐检查**(放行判据 C-4 的机器化)
#
# 背景(per docs/implementation/2026-10-21-cep-archforge-interface-alignment.md §6 C-4):
#   ArchForge 侧交付了协议实例声明(`32-cep-based-coherence-architecture.md` §6.1 / §6.2),
#   声明的结构由**本仓** `protocol_instance.schema.json` 约束。声明与 schema 任一侧漂移,
#   都必须**报红**而不是静默 —— 否则 C-4 就是一条「声称可自动化却无自动化」的判据。
#
# 本检查做 6 件事(逐项独立报红/报绿):
#   ① 声明 ↔ 本仓 schema(**全 13 个 required 顶层属性**,用 jsonschema 校验)
#   ② vc_id < fabric 的 vc_count + 逐网络键对称 + ordering 内层 key 的 int() 解析
#   ③ ordering_assumption[n] 的每个 vc_id ∈ vc_binding[n].values()
#   ④ vc_binding[n] 的每个消息类 ∈ 6 类规范词表
#   ⑤ network ⊆ fabric = ArchForge 已锁定网络集 + vc_count 与 ArchForge 清单一致(**双向**)
#   ⑥ 本仓 `include/tlm/cep/protocol_instances.json` 是 ArchForge 声明的**逐字段派生副本**
#      + 声明边表的端点必须落在该网络的 `vc_binding` 里(镜像漂移 ⇒ 报红)
#
# ⚠️ **依赖 `jsonschema`** —— 刻意**不降级运行**:
#   ① 的「13 个 required 属性全覆盖」用通用校验器实现才可靠;手写子集会让 11 个字段零校验。
#   ②~⑤ 是 JSON Schema **表达不了**的语义规则(per-network 槽位上界 / 包含关系 / 双向漂移),
#   故仍手写。
#   缺依赖时本脚本**报错退出(exit 2)**,不静默跳过 —— 一个会降级的闸门等于没有闸门。
#   CI 安装:.github/workflows/ci.yml 的 pip 行已含 `jsonschema`。
#
# ⚠️ **ArchForge 仓不在本仓环境时** → 检查**优雅跳过**(打印 `[SKIP]`,退出 0):
#    跨仓路径是外部条件,缺失不是本仓的缺陷。
#    但「跳过」与「跑过」必须可区分 —— 故首行显式标 `[SKIP]` / `[RUN]`。
#
# 用法:
#   bash scripts/test/cep_decl_alignment_check.sh
#   ARCHFORGE_DIR=/path/to/ArchForge bash scripts/test/cep_decl_alignment_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

ARCHFORGE="${ARCHFORGE_DIR:-/workspace/main/ArchForge}"

if [ ! -d "$ARCHFORGE" ]; then
  echo "  · [SKIP] ArchForge 仓不在本环境($ARCHFORGE)—— 跨仓声明对齐无法在本仓校验"
  echo "    (设置 ARCHFORGE_DIR=<路径> 可启用)"
  exit 0
fi

python3 - "$ROOT" "$ARCHFORGE" << 'PYEOF'
import json, os, re, sys

root, archforge = sys.argv[1], sys.argv[2]

DECL_REL = "docs/architecture/32-cep-based-coherence-architecture.md"
SCHEMA_REL = "include/tlm/cep/protocol_instance.schema.json"
FABRIC_SCHEMA_REL = "include/tlm/cep/fabric.schema.json"
FABRIC_REL = "include/tlm/cep/fabric.json"
NS_REL = "docs/architecture/22-nsa-fabric-address-spec.md"
REPO_DECL_REL = "include/tlm/cep/protocol_instances.json"

# 规范消息类词表(per ArchForge 32 号 §6.0.1,6 类;排除 CATEGORY_CREDIT / forward_data /
# snoop_completion_ack —— 后两者是已有槽位的承载实例,不单列)
MSG_CLASSES = {"request", "response", "snoop", "snoop_resp", "atomic", "query"}

errors = []  # (检查编号, 明细)


def err(chk, detail):
    errors.append((chk, detail))


def read_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ── 依赖:jsonschema(缺则报错,不降级) ──
try:
    import jsonschema
except ImportError:
    print("  ✗ [RUN] 缺少 python 依赖 'jsonschema' —— 判据 C-4 的机器化**无法降级运行**")
    print("    · 安装: python3 -m pip install jsonschema")
    print("    · CI: .github/workflows/ci.yml 的 pip 安装行应含 jsonschema")
    sys.exit(2)

# ── 载入本仓 schema / fabric ──
schema = read_json(os.path.join(root, SCHEMA_REL))
fabric_schema = read_json(os.path.join(root, FABRIC_SCHEMA_REL))
fabric = read_json(os.path.join(root, FABRIC_REL))
networks = fabric.get("networks", {})

# schema 必须是 **per-network 嵌套形态** —— 否则本检查器的语义层假设不成立
props = schema.get("properties", {})
for fld in ("ordering_assumption", "vc_binding"):
    ap = props.get(fld, {}).get("additionalProperties", {})
    if ap.get("type") != "object":
        print(f"  ✗ [RUN] 本仓 schema 的 `{fld}` 不是 per-network 嵌套形态"
              f"(additionalProperties.type = {ap.get('type')!r})")
        print("    · 本检查器按 per-network 语义实现;schema 若退回扁平形态,检查器**不适用**")
        sys.exit(2)

# fabric 实例必须符合它自己的 schema
fab_errs = list(jsonschema.Draft202012Validator(fabric_schema).iter_errors(fabric))
if fab_errs:
    for e in fab_errs:
        err(5, f"[fabric 实例 ↔ fabric schema] {'/'.join(str(p) for p in e.path)}: {e.message}")

# ── 提取 ArchForge 的协议实例声明 ──
decl_path = os.path.join(archforge, DECL_REL)
if not os.path.exists(decl_path):
    print(f"  ✗ [RUN] 找不到 ArchForge 声明文档: {DECL_REL}")
    sys.exit(1)

text = open(decl_path, encoding="utf-8").read()
blocks = re.findall(r"```json\s*(\{.*?\})\s*```", text, re.S)

declarations = []
parse_failures = 0
for i, b in enumerate(blocks):
    try:
        obj = json.loads(b)
    except Exception as exc:
        # ⚠️ **不静默丢弃** —— 一份解析失败的声明 = 一份未被校验的声明 = 闸门失效
        parse_failures += 1
        err(1, f"第 {i + 1} 个 ```json fence **JSON 解析失败**:{exc}")
        continue
    if isinstance(obj, dict) and "ordering_assumption" in obj and "vc_binding" in obj:
        declarations.append(obj)

if parse_failures:
    print(f"  ✗ [RUN] ArchForge 声明块解析失败 {parse_failures} 份 —— "
          f"**未被校验的声明等于没有闸门**")
    for chk, detail in errors:
        print(f"    · {detail}")
    sys.exit(1)

if not declarations:
    print(f"  ✗ [RUN] 未从 ArchForge 声明文档提取到任何协议实例声明"
          f"(json fence 总数 {len(blocks)})")
    print(f"    · 来源: {decl_path}")
    sys.exit(1)

ids = [d.get("id", "<无 id>") for d in declarations]
dup = sorted({i for i in ids if ids.count(i) > 1})
if dup:
    err(1, f"协议实例 id 重复:{dup}")

# ══════════════════════════════════════════════════════════════════════════
# ① 声明 ↔ 本仓 schema(**全 13 个 required 属性**)
# ══════════════════════════════════════════════════════════════════════════
validator = jsonschema.Draft202012Validator(schema)
for decl in declarations:
    decl_id = decl.get("id", "<无 id>")
    for e in validator.iter_errors(decl):
        loc = "/".join(str(p) for p in e.path)
        err(1, f"{decl_id}: {loc or '<root>'}: {e.message}")

# ══════════════════════════════════════════════════════════════════════════
# ② vc_id < vc_count + 逐网络键对称 + ordering 内层 key 的 int() 解析
# ③ ordering_assumption[n] 的每个 vc_id ∈ vc_binding[n].values()
# ④ vc_binding[n] 的每个消息类 ∈ 6 类规范词表
# ══════════════════════════════════════════════════════════════════════════
chk2, chk3, chk4 = [], [], []
for decl in declarations:
    decl_id = decl.get("id", "<无 id>")
    oa = decl.get("ordering_assumption", {})
    vb = decl.get("vc_binding", {})
    if not isinstance(oa, dict) or not isinstance(vb, dict):
        continue  # 形态不符已由 ① 报出,此处不重复

    # ② 逐网络键对称(per ArchForge 32 号 §6.2 纪律:每个网络必须同时出现在两个字段)
    oa_nets, vb_nets = set(oa.keys()), set(vb.keys())
    for n in sorted(oa_nets - vb_nets):
        chk2.append(f"{decl_id}.ordering_assumption.{n}: 该网络未出现在 vc_binding(键不对称)")
    for n in sorted(vb_nets - oa_nets):
        chk2.append(f"{decl_id}.vc_binding.{n}: 该网络未出现在 ordering_assumption(键不对称)")

    for n in sorted(oa_nets & vb_nets):
        net_spec = networks.get(n)
        if net_spec is None:
            continue  # ⑤ 报「网络不在 fabric」
        vc_count = net_spec.get("vc_count")
        binding = vb[n] if isinstance(vb[n], dict) else {}
        ordering = oa[n] if isinstance(oa[n], dict) else {}

        # ② vc_id < vc_count(槽位上界,来源 = fabric 实例)
        if isinstance(vc_count, int):
            for cls, vid in sorted(binding.items()):
                if isinstance(vid, int) and not isinstance(vid, bool) and vid >= vc_count:
                    chk2.append(
                        f"{decl_id}.vc_binding.{n}.{cls}: vc_id={vid} >= vc_count={vc_count}")

        # ② ordering 内层 key 一律 int() 解析;解析失败 = 该项红(不崩栈)
        parsed = {}
        for k in sorted(ordering.keys()):
            try:
                parsed[k] = int(k)
            except (TypeError, ValueError):
                chk2.append(
                    f"{decl_id}.ordering_assumption.{n}.{k}: key 非 vc_id(不可解析为整数)")
                parsed[k] = None

        # ③ ordering 的每个 vc_id ∈ vc_binding[n].values()
        bound = set(v for v in binding.values() if isinstance(v, int) and not isinstance(v, bool))
        for k, vid in sorted(parsed.items()):
            if vid is None:
                continue
            if vid not in bound:
                chk3.append(
                    f"{decl_id}.ordering_assumption.{n}.{k}: vc_id={vid} 不在 vc_binding 的值域 {sorted(bound)}")

        # ④ 消息类 ∈ 6 类规范词表
        #    注:同一 vc_id 可绑定多个消息类(per 32 号 §6.0.2 ualink VC3),**不做唯一性约束**
        for cls in sorted(binding.keys()):
            if cls not in MSG_CLASSES:
                chk4.append(
                    f"{decl_id}.vc_binding.{n}.{cls}: 消息类不在 6 类规范词表 {sorted(MSG_CLASSES)}")

for d in chk2:
    err(2, d)
for d in chk3:
    err(3, d)
for d in chk4:
    err(4, d)

# ══════════════════════════════════════════════════════════════════════════
# ⑤ network ⊆ fabric = ArchForge 已锁定网络集 + vc_count 与 ArchForge 清单一致(双向)
# ══════════════════════════════════════════════════════════════════════════
chk5 = []
used_nets = set()
for decl in declarations:
    for fld in ("ordering_assumption", "vc_binding"):
        v = decl.get(fld, {})
        if isinstance(v, dict):
            used_nets |= set(v.keys())

for n in sorted(used_nets):
    if n not in networks:
        chk5.append(f"声明用到网络 '{n}',但本仓 fabric 实例无此网络(NS-R1)")


def archforge_locked_networks():
    """从 ArchForge 22-nsa §0.5.4.1 解析 network_id → vc_count(仅已锁定者)。"""
    path = os.path.join(archforge, NS_REL)
    if not os.path.exists(path):
        return None, f"文件缺失: {NS_REL}"
    body = open(path, encoding="utf-8").read()
    m = re.search(r"###\s*§0\.5\.4\.1\b(.*?)(?=\n#{2,4}\s|\Z)", body, re.S)
    if not m:
        return None, f"未找到 §0.5.4.1 节(章节号或标题已变)"
    out = {}
    for line in m.group(1).splitlines():
        if not line.strip().startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 4:
            continue
        nid = cells[0].strip("`").strip()
        if not re.fullmatch(r"[a-z0-9_]+", nid):
            continue
        dm = re.search(r"(\d+)", cells[3])
        if dm:
            out[nid] = int(dm.group(1))
    if not out:
        return None, "§0.5.4.1 表未解析出任何 network_id / vc_count"
    return out, None


af_locked, af_err = archforge_locked_networks()
if af_err is not None:
    # ⚠️ **权威源解析失败 = 守卫失效** —— 不得降级为 warn
    #    (ArchForge 正在做 D-9 返工,§0.5.4.1 的节号/结构随时可能变;
    #     若此处静默跳过,`vc_count` 漂移将永不被发现 = M-2 病根的第三次复发)
    chk5.append(f"**无法解析 ArchForge 权威网络清单**({af_err})—— 反向漂移守卫失效,判红")
else:
    our_set, their_set = set(networks.keys()), set(af_locked.keys())
    for n in sorted(our_set - their_set):
        chk5.append(
            f"本仓 fabric 含网络 '{n}',ArchForge 已锁定清单无此网络(NS-R2:单点权威在 ArchForge)")
    for n in sorted(their_set - our_set):
        chk5.append(
            f"ArchForge 已锁定网络 '{n}',本仓 fabric 实例缺此网络(需同步 22-nsa §0.5.4.1)")
    for n in sorted(our_set & their_set):
        ours = networks[n].get("vc_count")
        theirs = af_locked[n]
        if ours != theirs:
            chk5.append(
                f"网络 '{n}' 的 vc_count 漂移:本仓 {ours} != ArchForge {theirs}"
                f"(22-nsa §0.5.4.1)")

for d in chk5:
    err(5, d)

# ══════════════════════════════════════════════════════════════════════════
# 输出
# ══════════════════════════════════════════════════════════════════════════
# ══════════════════════════════════════════════════════════════
# ⑥ 本仓派生副本 ↔ ArchForge 声明(**逐字段**;镜像漂移 ⇒ 红)
# ══════════════════════════════════════════════════════════════
repo_decl_path = os.path.join(root, REPO_DECL_REL)
repo_insts = {}
repo_edges = {}
if not os.path.exists(repo_decl_path):
    err(6, f"本仓派生副本缺失:{REPO_DECL_REL}")
else:
    try:
        repo_doc = read_json(repo_decl_path)
    except Exception as exc:
        err(6, f"本仓派生副本 JSON 解析失败:{exc}")
        repo_doc = {}
    repo_insts = {d.get("id"): d for d in repo_doc.get("instances", []) if isinstance(d, dict)}
    repo_edges = (repo_doc.get("channel_dependency_edges") or {}).get("per_network") or {}

    # ① 逐实例逐字段相等(副本不是「差不多」,是**逐字段**)
    for d in declarations:
        rid = d.get("id")
        if rid not in repo_insts:
            err(6, f"派生副本缺少 ArchForge 已声明的实例:{rid}")
            continue
        local = repo_insts[rid]
        for key in sorted(set(local) | set(d)):
            if local.get(key) != d.get(key):
                err(6, f"{rid}.{key} 与 ArchForge 不符(派生副本已漂移)")
    extra_ids = set(repo_insts) - set(ids)
    if extra_ids:
        err(6, f"派生副本含 ArchForge 未声明的实例:{sorted(extra_ids)}")

    # ② 边表端点必须被该网络的 vc_binding 绑定(否则装配闸会以「端点未绑定」报红)
    for rid, inst in repo_insts.items():
        binding = inst.get("vc_binding", {}) or {}
        for net, elist in repo_edges.items():
            if net not in binding:
                continue
            bound = set(binding[net].values())
            for pair in elist:
                if not isinstance(pair, list) or len(pair) != 2:
                    err(6, f"{rid}:网络 {net} 的边 {pair!r} 不是二元组")
                    continue
                for vc in pair:
                    if vc not in bound:
                        err(6, f"{rid}:边端点 {net}.vc{vc} 未被 vc_binding 绑定")

LABELS = {
    1: "① 声明 ↔ schema 全 13 属性(含 required/enum/嵌套结构)",
    2: "② vc_id < vc_count + 键对称 + key 解析",
    3: "③ ordering ⊆ vc_binding(逐网络)",
    4: "④ 消息类 ∈ 6 类规范词表",
    5: "⑤ network ⊆ fabric = ArchForge 已锁定集 + vc_count 双向一致",
    6: f"⑥ 派生副本逐字段 = ArchForge 声明 + 边端点已绑定({REPO_DECL_REL})",
}

print("  · [RUN] ArchForge 协议实例声明 ↔ 本仓 schema/fabric 对齐检查")
print(f"    · 声明源: {os.path.join(archforge, DECL_REL)} §6.1 / §6.2")
print(f"    · 声明份数: {len(declarations)} 份 —— " + ", ".join(ids))
print(f"    · 本仓 schema: {SCHEMA_REL}")
print(f"    · 本仓 fabric: {FABRIC_REL}" +
      (f"(ArchForge 清单 {len(af_locked)} 网络)" if af_locked else "(ArchForge 清单**未解析**)"))

for chk in (1, 2, 3, 4, 5, 6):
    sub = [d for c, d in errors if c == chk]
    if sub:
        print(f"    · {LABELS[chk]}: ✗ ({len(sub)} 处)")
        for d in sub:
            print(f"        - {d}")
    else:
        print(f"    · {LABELS[chk]}: ✓")

if errors:
    n_chk = len(set(c for c, _ in errors))
    print(f"  ✗ [RUN] 声明对齐检查失败:{n_chk}/6 项红,{len(errors)} 处失配")
    sys.exit(1)

print("  ✓ [RUN] 声明对齐检查通过:6 项子校验全绿"
      f"({len(declarations)} 份声明,{len(used_nets)} 个网络)")
sys.exit(0)
PYEOF
exit $?
