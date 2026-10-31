# scripts/cep/parity/gem5_linear_probe.py
#   —— gem5 侧**参数同源**测量台(线性访问序列)
#
# 为什么不用 `createTrace`(protobuf)走文件同源:
#   实测 gem5 v24 的 `ProtoInputStream` 用 `google::protobuf::io::IstreamInputStream`
#   (零拷贝路径)逐条读消息,**每条消息新建一个 `io::CodedInputStream`**
#   ⇒ 在**文件较大时提前 EOF**(实测:1000 条读到 966 / 20000 条读到 19735 /
#   100000 条读到 98874;而同一文件用 Python 的 protobuf 库可完整读出全部条数)
#   ⇒ **文件同源路径在 gem5 v24 侧不可靠**(登记为已知限制,见台账 Entry #115)。
#
# 本脚本走**参数同源**路线:用 `PyTrafficGen.createLinear` 生成**完全确定**的序列:
#   · 地址序列 = `start_addr + i*blocksize`,到 `end_addr` 后 wrap ⇒ **无 RNG**;
#   · `read_percent == 100` ⇒ 全读 ⇒ `LinearGen::getNextPacket()` 里
#     `readPercent == 100` 短路 ⇒ **不调 RNG** ⇒ 读写也确定;
#   · `min_period == max_period` ⇒ `Random::random(min,max)` 恒定 ⇒ 间隔确定
#     (per `src/base/random.hh`:`gen() % (max-min+1) + min`,min==max ⇒ 恒定);
#   ⇒ 两侧**各自独立生成**同一条序列 ⇒ 用**地址序列 sha256** 交叉验证同源。
#
# ⚠️ 用法:
#   external/gem5/build/NULL/gem5.opt --stats-file=<out>.txt \
#       scripts/cep/parity/gem5_linear_probe.py
#   环境变量:
#     PARITY_STATS  stats 文本输出路径
#     PARITY_OUT    结果 JSON 路径
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

# ⚠️ **多段设计**(per 台账 Entry #117 的主对拍修订):
#    目的 = 让**自然**产生不同的命中率 `h_m`(而不是「瞄准」某个值)⇒
#    扫 `h_0` 网格找**统一**解才有鉴别力。
#    实测:单一线性区间在 LRU 下只有**两个极端** —— 工作集 ≤ cache ⇒ ~99.7%;
#    工作集 > cache ⇒ **0%**(线性扫描是 LRU 的**最坏情况**)⇒ 无法连续调 `h`。
#    ⇒ 用「**hot 段扫 R 遍**(第 1 遍 miss、第 2~R 遍 hit)+ **cold 段扫 1 遍**(全 miss)」
#    ⇒ h_m = (R−1)×H / (R×H + C) ⇒ **连续可调**且序列**完全确定**(可参数同源)。
#
# `segments` 形态(每项):`{start, end, repeats, data_limit}`
#   · `repeats` = 该区间扫几遍(每遍一个 `createLinear` generator);
#   · `data_limit` = **每遍**的字节上限(⇒ 每遍条数 = data_limit / blocksize)。
SEGMENTS = lin_cfg.get("segments") or []
if not SEGMENTS:
    # 兼容旧形态:单区间
    SEGMENTS = [{
        "start": int(lin_cfg["start_addr"]),
        "end": int(lin_cfg["end_addr"]),
        "repeats": 1,
        "data_limit": int(lin_cfg["data_limit"]),
    }]
BLOCK_SIZE = int(lin_cfg["blocksize"])
READ_PERCENT = int(lin_cfg["read_percent"])
MIN_PERIOD = int(lin_cfg["min_period"])
MAX_PERIOD = int(lin_cfg["max_period"])

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
# ⚠️ `elastic_req = True` ⇒ 背压时 generator **自适应减速**而不是把请求堆在
#    cache 的 MSHR 上 ⇒ 条数可预测(实测:关掉它时背压把实际发送拖到名义 tick 之后,
#    导致 max tick 截断序列)。
tgen.elastic_req = bool(lin_cfg.get("elastic_req", True))
tgen.port = cache.cpu_side
cache.mem_side = memory.port
system.tgen = tgen

# ⚠️ **SimObject 参数必须在 `m5.instantiate()` 之前设** —— instantiate 会 unproxy
#    并**冻结**参数;之后再设 ⇒ gem5 **segfault**(实测踩过)。
# `progress_check` 必须远大于序列总时长(默认 `1ms` = 1e9 ticks):
#    序列播完后 generator 空转 ⇒ 若 progress_check 先于 max tick 到期 ⇒ gem5 fatal。
#    ⇒ 按**实际条数**(data_limit / blocksize)算 4 倍。
# ⚠️ `progress_check` 必须在 instantiate **之前**设(instantiate 会冻结参数 ⇒ segfault)
#    且必须远大于**总时长**(各段 duration 之和)。
#    ⇒ 先算总条数(各段每遍条数之和)。
_total_accesses = sum(
    (int(seg["data_limit"]) // BLOCK_SIZE) * int(seg.get("repeats", 1))
    for seg in SEGMENTS
)
_progress_ticks = max(_total_accesses * (MIN_PERIOD + 100000) * 4, 10_000_000)
tgen.progress_check = f"{_progress_ticks / 1000:.0f}ms"

root = Root(full_system=False, system=system)
m5.instantiate()

# ── 生成线性序列 ───────────────────────────────────────────────────
# `createLinear(duration, start_addr, end_addr, blocksize, min_period, max_period, read_percent, data_limit)`
# ⚠️ `createLinear` 是 pybind 导出的方法,**可以**在 instantiate 之后调
#    (官方 `configs/example/dramsys.py` 就是这么用的)。
DURATION = int(lin_cfg["duration_ticks"])
# ⚠️ 每段一个 generator(`createLinear` 的 `data_limit` 是**每段**的字节上限);
#    `tgen.start([...])` 按**顺序**执行各段(每段 duration 到期 ⇒ transition 到下一段)。
generators = []
for seg in SEGMENTS:
    _seg_start = int(seg["start"])
    _seg_end = int(seg["end"])
    _seg_limit = int(seg["data_limit"])
    _seg_lines = (_seg_end - _seg_start) // BLOCK_SIZE
    for _rep in range(int(seg.get("repeats", 1))):
        # ⚠️ 每段的 `duration` 必须够跑完该段 ⇒ 按该段条数 × (间隔 + 响应上界) 估
        _seg_accesses = _seg_limit // BLOCK_SIZE
        _seg_duration = int(_seg_accesses * (MIN_PERIOD + 100000) * 2)
        generators.append(tgen.createLinear(
            _seg_duration, _seg_start, _seg_end, BLOCK_SIZE,
            MIN_PERIOD, MAX_PERIOD, READ_PERCENT, _seg_limit,
        ))
tgen.start(generators)

# ⚠️ **max tick 必须严格小于 generator 的 `duration`** ——
#    gem5 v24 的 `BaseTrafficGen::update()` 在 `duration` 到期 ⇒ `transition()` ⇒
#    `nextGenerator()` 返回 null ⇒ `activeGenerator` 变 null ⇒ 紧接着的
#    `activeGenerator->nextPacketTick(...)` **null 解引用 ⇒ segfault**
#    (per `src/cpu/testers/traffic_gen/base.cc::update` —— 该路径**没有** null 检查;
#     Release 构建(`gem5.opt`)下 assert 被禁用 ⇒ 直接崩)。
#    ⇒ 故:duration 要**够跑完全部条数**,max tick 取「末条 tick + 响应余量」且 **< duration**。
# ⚠️ **max tick 必须够大**:背压(cache 未命中延迟 ~49ns + MSHR 深度)会让**实际**
#    发送时刻远晚于**名义** tick ⇒ 若 max tick 只按「名义末 tick」设,会在序列发完前
#    截断(实测:65536 条只发出 54631 条,`retryTicks` 高达 6.55e8)。
#    ⇒ max tick = 条数 × (名义间隔 + 单次响应时间上界) + 余量。
# ⚠️ 且必须 **< duration**(否则 gem5 v24 在 generator 耗尽处 segfault)。
_RESP_TICKS = 100000  # 单次未命中响应时间上界(~49ns @1THz)
LAST_TICK = _total_accesses * (MIN_PERIOD + _RESP_TICKS)
MAX_TICK = LAST_TICK + 100 * MIN_PERIOD
assert MAX_TICK < DURATION, (
    f"max tick({MAX_TICK}) 必须 < duration({DURATION})——否则 gem5 v24 会在 "
    f"generator 耗尽处 segfault")
m5.setMaxTick(MAX_TICK)
m5.stats.reset()
print(f"[parity] gem5 线性探针开始:段数={len(SEGMENTS)} 总条数={_total_accesses} "
      f"blk={BLOCK_SIZE} reads={READ_PERCENT}% period={MIN_PERIOD}..{MAX_PERIOD}")
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

# ⚠️ **平均访问延迟**:v24 classic cache **无「命中延迟总和」统计**
#    (实测 stats 文件里 `overallHitLatency` 0 命中)⇒ 命中延迟只能用配置解析式
#    ⇒ 平均延迟 = (命中数 × 派生命中延迟 + 未命中数 × 实测未命中延迟) / 总访问数。
#    ⇒ 这是**混合口径**(派生 + 实测)⇒ 仅作 P-2 的**观察值**(辅助档 P-2 为 N/A)。
_avg_latency_ns = None
if hits or misses:
    _hit_ns = (
        cache_cfg["tag_latency_cycles"]
        + cache_cfg["data_latency_cycles"]
        + cache_cfg["response_latency_cycles"]
    ) * clk_period_ns
    _avg_latency_ns = (
        float(hits) * _hit_ns + float(misses) * (miss_avg_ns or 0.0)
    ) / float(hits + misses)

hit_avg_ns = (
    cache_cfg["tag_latency_cycles"]
    + cache_cfg["data_latency_cycles"]
    + cache_cfg["response_latency_cycles"]
) * clk_period_ns

# ── **地址序列指纹**(同源的**交叉验证**证据)────────────────────────
# 两侧**各自独立**按同一参数生成序列 ⇒ 指纹相同 ⇒ 序列逐条相同。
# ⚠️ 多段:按「段的顺序 × 每段的遍数」依次展开(与 gem5 的 `tgen.start([...])`
#    **顺序**执行一致)⇒ 每段独立从头扫(per `LinearGen::enter()`:`nextAddr = startAddr`)。
seq = []
for _seg in SEGMENTS:
    _ss = int(_seg["start"])
    _se = int(_seg["end"])
    _per = int(_seg["data_limit"]) // BLOCK_SIZE
    for _rep in range(int(_seg.get("repeats", 1))):
        _a = _ss
        for _ in range(_per):
            seq.append(_a)
            _a += BLOCK_SIZE
            if _a >= _se:
                _a = _ss
# ⚠️ 哈希算法用 **FNV-1a 64bit**(顺序敏感;实现极简 ⇒ C++/Python 两侧口径易对齐)。
#    不用 SHA-256:本仓(C++)侧无现成实现 ⇒ 引入密码学哈希只为「算个校验和」不划算。
#    ⚠️ 口径(位移/乘法/取模)必须与 C++ 侧**逐字一致**。
def _fnv1a64(values):
    h = 0xCBF29CE484222325
    for v in values:
        h ^= (v & 0xFFFFFFFFFFFFFFFF)
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{h:016x}"


seq_hash = _fnv1a64(seq)

# ⚠️ **P-5**(per `14 号` §4.4 / `ADR-CEP-06 §3`):每次对拍必须声明
#    `verification_mode` + `verification_scope` ⇒ 缺失 ⇒ 判据 FAIL。
_ver = CFG.get("verification", {})

result = {
    "tool": "gem5",
    "mode": "parameter_same_source",
    "version": CFG["gem5"]["version"],
    "verification_mode": _ver.get("verification_mode"),
    "verification_scope": _ver.get("verification_scope"),
    "sequence": {
        "segments": SEGMENTS,
        "blocksize": BLOCK_SIZE,
        "read_percent": READ_PERCENT,
        "min_period": MIN_PERIOD,
        "max_period": MAX_PERIOD,
        "num_accesses": _total_accesses,
        "addr_seq_fnv1a64": seq_hash,
    },
    "hits": hits,
    "misses": misses,
    "accesses": accesses,
    "miss_rate": miss_rate,
    "num_reads": num_reads,
    "num_writes": num_writes,
    "hit_latency_ns": hit_avg_ns,
    "miss_latency_ns": miss_avg_ns,
    "avg_latency_ns": _avg_latency_ns,
    "avg_latency_ns_source": "mixed(命中=派生 tag+data+response;未命中=实测)⇒ 观察值",
    "hit_latency_ns_source": "derived(tag+data+response;v24 classic cache 无命中延迟实测统计)",
    "miss_latency_ns_source": "measured(overallAvgMissLatency::total)",
    "ticks_per_ns": TICKS_PER_NS,
}

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)
print(f"[parity] gem5 线性探针结果已写出:{OUT}")
print(json.dumps(result, ensure_ascii=False, indent=2))
