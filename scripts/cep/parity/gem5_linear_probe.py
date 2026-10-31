# scripts/cep/parity/gem5_linear_probe.py
#   —— gem5 侧测量台(**单 generator** 统一结构;linear / random 两个 driver)
#
# ⚠️ **为什么统一成「单 generator」**(2026-10-31 实测结论):
#   gem5 v24 的 `BaseTrafficGen` 在**多 generator** 下有两个坑:
#   ① `LinearGen` / `RandomGen` 的 `data_limit` 在 **Release 构建下不生效**
#      (`nextPacketTick()` 返回 `MaxTick` 后,`update()` 里的 assert 被禁用 ⇒ 继续发包)
#      ⇒ 实际终止由 **`duration`** 决定;
#   ② 多 generator 的 `duration` 之和与 `MAX_TICK` 互相牵制 ⇒ **序列被截断**
#      (实测 R4:名义 512 条、实测 340 条)且**加倍预算无效**(比例不变);
#   ③ 全部 generator 播完后 transition 到空 ⇒ gem5 v24 **null 崩**
#      (`BaseTrafficGen::update()` 无 null 检查;Release 构建 assert 被禁用)。
#   ⇒ **单 generator** ⇒ 三个坑全规避;条数由 `duration` 控制,`MAX_TICK < duration`。
#
# ── 两个 driver ────────────────────────────────────────────────────────
# · **linear**(`createLinear`):地址 = 线性扫描 + wrap ⇒ **完全确定** ⇒ 序列可复现
#   ⇒ 支撑「参数同源档」的**地址序列指纹**(FNV-1a 64bit)交叉验证;
#   ⚠️ 命中率在 LRU 下只有**两个极端**(工作集 ≤ cache ⇒ ~99.7%;> cache ⇒ **0%**,
#      线性扫描是 LRU 的**最坏情况**)⇒ **不能**用它扫中间命中率。
# · **random**(`createRandom`):地址 = **均匀随机** ⇒ 命中率随「工作集/容量」
#   **连续可调** ⇒ 支撑「主对拍档」扫不同 `h_m`;
#   ⚠️ gem5 用标准 `std::mt19937_64` + 固定 `globalSeed=5489` ⇒ 序列**跨进程可复现**,
#      但本仓侧**未**逐字复刻该 RNG ⇒ **本档没有跨侧序列指纹证据**(登记为局限)。
#
# 用法:external/gem5/build/NULL/gem5.opt --stats-file=<out>.txt <this>
#   环境变量:`PARITY_CONFIG`(必填,指向 parity_config.json)/`PARITY_OUT`/`PARITY_STATS`
import hashlib
import json
import os
import pathlib
import sys

import m5
import m5.stats
from m5.objects import (
    AddrRange,
    NoncoherentCache,
    PyTrafficGen,
    Root,
    SimpleMemory,
    SrcClockDomain,
    System,
    VoltageDomain,
)

CFG = json.loads(

    open(
        os.environ.get(
            "PARITY_CONFIG",
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "parity_config.json"),
        ),
        encoding="utf-8",
    ).read()
)

# ⚠️ **gem5 版本的唯一真源** = `gem5.versions.default.version`;
#    顶层 `gem5.version` 仅作**兼容镜像** ⇒ 两者不一致 ⇒ 报错退出(D-4 自检)。
_GEM5_VERSION = CFG["gem5"]["versions"]["default"]["version"]
_mirror = CFG["gem5"].get("version")
if _mirror and _mirror != _GEM5_VERSION:
    raise SystemExit(
        f"✗ gem5 版本口径不一致:`gem5.version`={_mirror} vs "
        f"`gem5.versions.default.version`={_GEM5_VERSION}"
        f" ⇒ 产物会错标版本 ⇒ 拒绝出结果")
OUT = os.environ.get("PARITY_OUT", "gem5_linear_probe.json")
STATS_TEXT = os.environ.get(
    "PARITY_STATS",
    os.path.join(os.path.dirname(os.path.abspath(OUT)) or ".", "gem5_linear_stats.txt"),
)

cache_cfg = CFG["gem5"]["cache"]
mem_cfg = CFG["gem5"]["memory"]
lin_cfg = CFG["gem5"]["linear"]

TICKS_PER_NS = 1000.0
clk_period_ns = float(CFG["cycle_ns"])

DRIVER = lin_cfg.get("driver", "linear")
BLOCK_SIZE = int(lin_cfg["blocksize"])
READ_PERCENT = int(lin_cfg["read_percent"])
MIN_PERIOD = int(lin_cfg["min_period"])
MAX_PERIOD = int(lin_cfg["max_period"])
TICK_BUDGET = int(lin_cfg.get("tick_budget_per_access", 200000))
NOMINAL_ACCESSES = int(lin_cfg["nominal_accesses"])
# ⚠️ **`duration` 决定实际访问条数** —— 因为 gem5 v24 的 `data_limit`
#    **在 Release 构建下不生效**(`nextPacketTick()` 返回 `MaxTick` 后,
#    `update()` 里的 assert 被禁用 ⇒ 继续发包)⇒ 真正的终止条件是 `duration`。
# ⇒ 条数 = `duration / 每条实际耗时` ⇒ **每条实际耗时受背压影响** ⇒
#    **不能靠猜常数** ⇒ 驱动脚本**迭代校准** `duration` 使实际条数 == 名义条数。
#    (`duration_ticks` 由驱动脚本写入;为 0 时用「名义条数 × tick 预算」作初值。)
DURATION = int(lin_cfg.get("duration_ticks") or 0)
if DURATION <= 0:
    DURATION = NOMINAL_ACCESSES * TICK_BUDGET
# ⚠️ **`MAX_TICK` 必须 < `duration`** —— 否则 generator 播完后 transition 到空 ⇒ **null 崩**。
#    ⇒ 取「duration − 一个间隔」⇒ max tick 落在 generator 内部 ⇒ 既不崩也不丢条。
MAX_TICK = DURATION - MIN_PERIOD

# ── 系统装配 ───────────────────────────────────────────────────────
vd = VoltageDomain()
clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=vd)
system = System(clk_domain=clk_domain)
system.mem_mode = "timing"

cache = NoncoherentCache()
cache.size = cache_cfg["size"]
cache.assoc = cache_cfg["assoc"]
cache.tag_latency = cache_cfg["tag_latency_cycles"]
cache.data_latency = cache_cfg["data_latency_cycles"]
cache.response_latency = cache_cfg["response_latency_cycles"]
cache.mshrs = cache_cfg["mshrs"]
cache.write_buffers = cache_cfg["write_buffers"]
cache.tgts_per_mshr = 8
system.cache = cache

memory = SimpleMemory(latency=mem_cfg["latency"])
memory.range = AddrRange(mem_cfg["range"])
system.memories = [memory]
system.mem_ranges = [AddrRange(mem_cfg["range"])]

tgen = PyTrafficGen()
tgen.elastic_req = bool(lin_cfg.get("elastic_req", True))
tgen.port = cache.cpu_side
cache.mem_side = memory.port
system.tgen = tgen

# ⚠️ **`progress_check` 必须在 `m5.instantiate()` 之前设**(instantiate 会冻结参数 ⇒ segfault)
#    且必须远大于总时长(否则 generator 空转到 progress_check ⇒ gem5 fatal)。
tgen.progress_check = f"{max(DURATION * 4, 10_000_000) / 1000:.0f}ms"

root = Root(full_system=False, system=system)
m5.instantiate()

# ── 驱动(单 generator)────────────────────────────────────────────────
if DRIVER == "random":
    # ⚠️ 容错:`random_*` 未设时回退到 `start_addr/end_addr`
    #    (两档用**不同** driver ⇒ driver 由各自的配置工具设置,共享 config 只给默认)
    _rs = int(lin_cfg.get("random_start_addr", lin_cfg.get("start_addr", 0)))
    _re = int(lin_cfg.get("random_end_addr", lin_cfg.get("end_addr", 0)))
    gen = tgen.createRandom(
        DURATION, _rs, _re, BLOCK_SIZE, MIN_PERIOD, MAX_PERIOD, READ_PERCENT, 0)
    seq_desc = {"driver": "random", "start_addr": _rs, "end_addr": _re}
else:
    _ss = int(lin_cfg["start_addr"])
    _se = int(lin_cfg["end_addr"])
    gen = tgen.createLinear(
        DURATION, _ss, _se, BLOCK_SIZE, MIN_PERIOD, MAX_PERIOD, READ_PERCENT, 0)
    seq_desc = {"driver": "linear", "start_addr": _ss, "end_addr": _se}

tgen.start([gen])


def _fnv1a64(values):
    """FNV-1a 64bit(与本仓 C++ 侧 `seq_fnv1a64` **逐字一致**)。"""
    h = 0xCBF29CE484222325
    for v in values:
        h ^= (v & 0xFFFFFFFFFFFFFFFF)
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{h:016x}"


def _linear_seq_fingerprint(n_accesses):
    """`linear` driver 的地址序列指纹(线性 + wrap)。

    ⚠️ **按「实际发出的条数」算**(不是名义条数)—— gem5 v24 的 `data_limit` 不生效
    ⇒ 实际条数由 `duration` 决定 ⇒ 与名义值可能差几十条
    ⇒ 本仓侧也按**同一个实际条数**生成 ⇒ 指纹**逐条一致** ⇒ P-1c 才是真正的同源证据。
    """
    if DRIVER != "linear":
        return None
    _ss = int(lin_cfg["start_addr"])
    _se = int(lin_cfg["end_addr"])
    seq = []
    a = _ss
    for _ in range(n_accesses):
        seq.append(a)
        a += BLOCK_SIZE
        if a >= _se:
            a = _ss
    return _fnv1a64(seq)


m5.stats.reset()
print(f"[parity] gem5 探针开始:driver={DRIVER} 名义条数={NOMINAL_ACCESSES} "
      f"duration={DURATION} max_tick={MAX_TICK}")
m5.simulate(MAX_TICK)
m5.stats.dump()


# ── 取统计量 ───────────────────────────────────────────────────────
def parse_stats_text(path):
    out = {}
    p = pathlib.Path(path)
    if not p.exists():
        return out
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("-") or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        try:
            out[parts[0]] = float(parts[1])
        except ValueError:
            continue
    return out


STATS = parse_stats_text(STATS_TEXT)


def stat(name, default=None):
    v = STATS.get(name)
    return default if v is None else v


hits = stat("system.cache.overallHits::total", 0)
misses = stat("system.cache.overallMisses::total", 0)
accesses = stat("system.cache.overallAccesses::total", 0)
miss_rate = stat("system.cache.overallMissRate::total", None)
avg_miss_latency_ticks = stat("system.cache.overallAvgMissLatency::total", None)
num_reads = stat("system.tgen.totalReads", None)
num_writes = stat("system.tgen.totalWrites", None)

miss_avg_ns = None
if avg_miss_latency_ticks is not None:
    miss_avg_ns = float(avg_miss_latency_ticks) / TICKS_PER_NS

# ⚠️ v24 classic cache **无「命中延迟总和」统计**(实测 0 命中)⇒ 命中延迟只能用
#    配置解析式 ⇒ 平均访问延迟是**混合口径**(派生命中 + 实测未命中)⇒ 仅作观察值。
_hit_ns = (
    cache_cfg["tag_latency_cycles"]
    + cache_cfg["data_latency_cycles"]
    + cache_cfg["response_latency_cycles"]
) * clk_period_ns
_avg_latency_ns = None
if hits or misses:
    _avg_latency_ns = (
        float(hits) * _hit_ns + float(misses) * (miss_avg_ns or 0.0)
    ) / float(hits + misses)

# ⚠️ **指纹按「实际发出的条数」算**(gem5 v24 的 `data_limit` 不生效 ⇒ 实际条数
#    由 `duration` 决定 ⇒ 可能与名义值差几十条)⇒ 本仓侧用**同一个实际条数**生成
#    ⇒ 指纹**逐条一致** ⇒ P-1c 才是真正的同源证据。
_seq_fnv1a64 = _linear_seq_fingerprint(int(accesses or 0))

# ⚠️ **P-5**(per `14 号` §4.4 / `ADR-CEP-06 §3`):每次对拍必须声明
#    `verification_mode` + `verification_scope` ⇒ 缺失 ⇒ 判据 FAIL。
_ver = CFG.get("verification", {})

result = {
    "tool": "gem5",
    "mode": "parameter_same_source",
    # ⚠️ **D-4 修复**:version 取**唯一真源** `gem5.versions.default.version`
    #    (旧的顶层 `gem5.version` 曾停在 v24 ⇒ v25 的产物**全部错标 v24**)。
    "version": _GEM5_VERSION,
    "verification_mode": _ver.get("verification_mode"),
    "verification_scope": _ver.get("verification_scope"),
    "sequence": {
        **seq_desc,
        "blocksize": BLOCK_SIZE,
        "read_percent": READ_PERCENT,
        "min_period": MIN_PERIOD,
        "max_period": MAX_PERIOD,
        "nominal_accesses": NOMINAL_ACCESSES,
        # ⚠️ **声明的 profile**(per `14 号` §4.4 封闭枚举表)——
        #    ⚠️ `random` driver = gem5 `createRandom` = **真均匀** ⇒ `conforming` 类;
        #    `linear` driver = 线性扫描 ⇒ 命中/未命中两极 ⇒ `observe` 类
        #    (不参与判定;其 R-B 误差随 W/C 连续变化 ⇒ 不能当 `breaking` 正控)。
        "profile": "uniform" if DRIVER == "random" else "fixed_ratio",
        # ⚠️ **workload 特征**(供分析器用 R-B 规则估 h_0)
        "working_set_bytes": lin_cfg.get("working_set_bytes"),
        "cache_bytes": lin_cfg.get("cache_bytes"),
        "duration_ticks": DURATION,
        "max_tick": MAX_TICK,
        # ⚠️ **只有 `linear` driver 能算跨侧序列指纹** —— 地址序列完全确定
        #    (线性 + wrap)⇒ 本仓侧可逐字复现 ⇒ 指纹交叉验证。
        #    ⚠️ **`random` driver 无指纹**(gem5 用标准 `std::mt19937_64` +
        #      固定 `globalSeed=5489` ⇒ 序列**跨进程可复现**,但本仓侧**未**逐字复刻
        #      该 RNG ⇒ **无跨侧指纹证据**(登记为局限)。
        "addr_seq_fnv1a64": _seq_fnv1a64,
    },
    "hits": hits,
    "misses": misses,
    "accesses": accesses,
    "miss_rate": miss_rate,
    "num_reads": num_reads,
    "num_writes": num_writes,
    "hit_latency_ns": _hit_ns,
    "miss_latency_ns": miss_avg_ns,
    "avg_latency_ns": _avg_latency_ns,
    "hit_latency_ns_source": "derived(tag+data+response;v24 classic cache 无命中延迟实测统计)",
    "miss_latency_ns_source": "measured(overallAvgMissLatency::total)",
    "avg_latency_ns_source": "mixed(命中=派生;未命中=实测)⇒ 观察值",
    "ticks_per_ns": TICKS_PER_NS,
}

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)
print(f"[parity] gem5 探针结果已写出:{OUT}")
print(json.dumps(result, ensure_ascii=False, indent=2))
