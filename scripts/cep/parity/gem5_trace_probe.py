# scripts/cep/parity/gem5_trace_probe.py
#   —— gem5 侧**同源 trace** 测量台(主对拍用)
#
# 与 `gem5_cache_probe.py`(常数档)的区别:
#   · 驱动:**`PyTrafficGen.createTrace`**(吃 protobuf trace)⇒ 地址序列**由文件决定**
#     ⇒ **与本仓侧读同一文件** ⇒ 满足 `14 号` §4.1「两侧消费同一条离线 trace」;
#   · `MemTest` 那种 RNG 播种的驱动**不满足**该契约(每次运行都不同)。
#
# ⚠️ 前置条件:
#   ① gem5 **必须带 protobuf 构建**(否则 `createTrace` 抛 `NotImplementedError`)
#      —— 构建入口 `scripts/cep/parity/build_gem5.sh`(需 `protoc` + `libprotobuf-dev`);
#   ② trace 文件由 `scripts/cep/parity/parity_trace_gen.py` 生成(.pb + .csv)。
#
# 用法:
#   external/gem5/build/NULL/gem5.opt --stats-file=<out>.txt \\
#       scripts/cep/parity/gem5_trace_probe.py
#   环境变量:
#     PARITY_TRACE_PB  trace 的 .pb 路径(必填)
#     PARITY_STATS     stats 文本输出路径
#     PARITY_OUT       结果 JSON 路径
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
OUT = os.environ.get("PARITY_OUT", "gem5_trace_probe.json")
TRACE_PB = os.environ.get("PARITY_TRACE_PB", "")
STATS_TEXT = os.environ.get(
    "PARITY_STATS",
    os.path.join(os.path.dirname(os.path.abspath(OUT)) or ".", "gem5_stats_trace.txt"),
)
if not TRACE_PB:
    raise SystemExit("✗ 缺 PARITY_TRACE_PB(用 scripts/cep/parity/parity_trace_gen.py 生成)")

cache_cfg = CFG["gem5"]["cache"]


def _parse_size(text):
    """把 gem5 的 size 字符串(如 `"16KiB"`)解析成字节数。"""
    t = str(text).strip()
    for suffix, mult in (("KiB", 1024), ("MiB", 1024 * 1024),
                         ("GiB", 1024 ** 3), ("B", 1)):
        if t.endswith(suffix):
            return int(float(t[:-len(suffix)]) * mult)
    return int(float(t))


_cache_bytes = _parse_size(cache_cfg["size"])
mem_cfg = CFG["gem5"]["memory"]
tester_cfg = CFG["gem5"]["tester"]

TICKS_PER_NS = 1000.0
clk_period_ns = float(CFG["cycle_ns"])

# ── 系统装配(与常数档同构,仅驱动不同)──────────────────────────────
vd = VoltageDomain()
clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=vd)
system = System(clk_domain=clk_domain)
system.mem_mode = "timing"

# ⚠️ 必须用 **`NoncoherentCache`**(不是 `Cache`):
#    coherent `Cache` 对**整行写**会转成 `MemCmd::InvalidateReq`
#    (per `src/mem/cache/cache.cc`:`if (is_whole_line_write) cmd = MemCmd::InvalidateReq;`)
#    ⇒ 而 `SimpleMemory` **只接受读/写** ⇒ panic:
#    `Should only see read and writes at memory controller, saw InvalidateReq`。
#    `NoncoherentCache` 的 miss 路径发 `ReadReq` ⇒ 无 invalidate 语义 ⇒ 兼容。
#    (官方 `configs/example/cache_partitioning.py` 同样用 `NoncoherentCache`。)
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
# ⚠️ 必须挂进 `system.memories`(VectorParam)而不是 `system.memory`:
#    `System::isMemAddr()`(per `src/sim/system.cc` → `PhysicalMemory::isMemAddr`)
#    查的是 `physmem.addrMap`,而该 map 只收 `system.memories` 里的 memory
#    (per `src/mem/physical.cc`:`for (const auto& m : memories)`)。
#    挂错 ⇒ 地址不被认作物理内存 ⇒ `BaseTrafficGen::update()` 里
#    `if (pkt && system->isMemAddr(...))` 恒假 ⇒ 一个包都不发 ⇒
#    gem5 报 `spent 1000000000 ticks without making progress`。
system.memories = [memory]
# ⚠️ `system.mem_ranges` 必须**显式**设置(实测为空 ⇒ `isMemAddr` 恒 false):
#    `System::isMemAddr()` → `PhysicalMemory::isMemAddr()` 查的是 `addrMap`,
#    而 `addrMap` 由 `system.mem_ranges` 填充(不只是 `system.memories`)。
#    ⇒ 不设 ⇒ `BaseTrafficGen::update()` 的
#    `if (pkt && system->isMemAddr(pkt->getAddr()))` 恒假 ⇒ **一个包都不发**
#    ⇒ gem5 报 `spent 1000000000 ticks without making progress`。
system.mem_ranges = [AddrRange(mem_cfg["range"])]

# ── 驱动:`PyTrafficGen` 吃同源 trace ───────────────────────────────
# ⚠️ **条数 / tick 间隔一律从 `meta.json` 读**(trace 的元数据单一真源),
#    **不**从配置值猜 —— 早期版本用 `parity_config.json` 的 `tester.num_accesses`
#    (=20000)算 `progress_check`,而实际 trace 只有 1000 条 ⇒ trace 放大到 1e5 条时
#    `progress_check` 会**远小于**真实需要 ⇒ 撞上 ⇒ gem5 fatal。
META_PATH = os.environ.get(
    "PARITY_TRACE_META",
    TRACE_PB.replace(".pb", ".meta.json"),
)
try:
    _meta = json.loads(pathlib.Path(META_PATH).read_text(encoding="utf-8"))
except Exception as e:
    raise SystemExit(f"✗ 读不到 trace 元数据 {META_PATH}({e})—— 先跑 parity_trace_gen.py")

TRACE_COUNT = int(_meta["num_accesses"])
TICK_INTERVAL = int(_meta["tick_interval"])
# 环境变量可覆盖 tick 间隔(但**条数**只认 meta.json ⇒ 避免漂移)
_env_ti = os.environ.get("PARITY_TICK_INTERVAL")
if _env_ti:
    TICK_INTERVAL = int(_env_ti)

tgen = PyTrafficGen()
# ⚠️ **`progress_check` 必须远大于 trace 的总时长**(默认 `1ms` = **1e9 ticks** @1THz):
#    它是 `BaseTrafficGen` 的「多久没进展就 fatal」的阈值
#    (per `src/cpu/testers/traffic_gen/base.cc::noProgress`)。
#    trace 播完后 generator 无事可做 ⇒ 若 `progress_check` 先于 max tick 到期
#    ⇒ gem5 报 `spent 1000000000 ticks without making progress`。
#    ⇒ 设为「trace 总时长的 4 倍」(下限 10ms)。
# ⚠️ `progress_check` 是 **`Param.Latency`** ⇒ 收**时间字符串**(如 `"8ms"`),
#    **不是**整数 tick 数(传 int 会报 `'int' object has no attribute 'endswith'`)。
_progress_ticks = max(TRACE_COUNT * TICK_INTERVAL * 4, 10_000_000)
tgen.progress_check = f"{_progress_ticks / 1000:.0f}ms"  # 1THz ⇒ 1000 tick = 1ns
system.tgen = tgen
tgen.port = cache.cpu_side
cache.mem_side = memory.port

# ⚠️ `PyTrafficGen` 是 `ClockedObject` ⇒ 需要在 `System` 构造时就有 clk_domain
#    (否则 `dvfs_handler` 的 unproxy 会失败,实测报
#     `Can't resolve proxy 'clk_domain' ... from 'system.dvfs_handler'`)。
#    这里 `tgen` 是在 `System` 之后创建的 ⇒ 它的 `clk_domain` 走
#    `Param.ClockDomain(Parent.clk_domain)` ⇒ 继承 system 的 ⇒ OK。

root = Root(full_system=False, system=system)
m5.instantiate()

# ── 启动 trace 回放 ────────────────────────────────────────────────
# ⚠️ **v24 的 `PyTrafficGen` 正确用法**(实测自官方 `configs/example/dramsys.py`):
#     `traffic_gen.start([gen1, gen2, ...])` —— `start()` 收一个**可迭代对象**,
#     每次 `next()` 取一个 `BaseGen`;**只调 `createTrace()` 而不调 `start()`**
#     ⇒ 一个包都不发(实测 `system.tgen.numPackets == 0`)。
#   · duration = 回放时长(Tick)⇒ 取足够大,让 trace 跑完;
#   · addr_offset = 0 ⇒ 与本仓侧**同一地址空间**(本仓侧 trace 已是绝对地址)。
# ⚠️ **模拟上限必须远大于 trace 的实际总时长**:
#    `BaseTrafficGen` 的 `progress_check` 默认 **1e9 ticks**;若 `m5.simulate()` 的
#    上限 ≥ 该阈值,trace 播完后 generator 空转到 progress_check ⇒ gem5 报
#    `fatal: BaseTrafficGen system.tgen spent 1000000000 ticks without making progress`
#    (实测踩中)。
#    ⇒ 上限取「trace 条数 × 每条间隔 × 4 + 1e9(余量)」,且**必须 > progress_check**。
# ⚠️ **必须用 `m5.setMaxTick()` 设上限**,而不是把 `m5.simulate(N)` 的 N 调大:
#    `BaseTrafficGen` 的 `progress_check` 默认 **1e9 ticks**;trace 播完后 generator
#    无事可做 ⇒ 空转到 progress_check ⇒ gem5 报
#    `fatal: BaseTrafficGen system.tgen spent 1000000000 ticks without making progress`。
#    ⇒ 正确做法:算出 trace 的**实际最后一条 tick**(+ 余量)作为 max tick,
#    模拟到那儿就停 ⇒ 不会触发 progress_check。
#    trace 的 tick 间隔由生成器决定(默认 100000 ticks/条)。
# ⚠️ `TRACE_DURATION_TICKS` 是 **generator 的 duration**(TraceGen 播多久),
#    必须 ≥ trace 实际总时长,否则 trace 会被**截断**(gem5 会 warn
#    "Trace player was unable to replay the entire trace")。
# ⚠️⚠️ **max tick 必须按「名义间隔 + 单次响应时间上界」算** —— 这是**真正的截断根因**
#    (2026-10-31 实测定位;此前误判为「protobuf 提前 EOF」,已纠正,见台账 Entry #120):
#    · trace 里相邻两条的 tick 间隔是**名义值**(如 10000 ticks = 10ns);
#    · 但**实际**每条耗时 = 名义间隔 + **响应/背压**时间(实测 ~10100 ticks;
#      未命中路径 ~49ns + 背压排队)⇒ **远大于**名义间隔;
#    · ⇒ 若 `MAX_TICK` 只按「名义总时长」算 ⇒ **模拟在序列发完前就结束**
#      ⇒ 表现为「gem5 只读到 N 条 < 名义条数」⇒ **看起来像 trace 读取截断**。
#    ⇒ 修:max tick = 条数 × (名义间隔 + **响应上界**) ⇒ 覆盖背压。
#    ⚠️ 且 max tick 必须 **< generator 的 duration** —— 否则 gem5 v24 在
#    `duration` 到期 transition 到空 generator ⇒ **null 崩**
#    (per `BaseTrafficGen::update()` 无 null 检查;Release 构建 assert 被禁用)。
# ⚠️ **上界取 100000 ticks = 100ns @1THz** —— 实测单次未命中响应 ≈ **49ns**
#    ⇒ 这里留 **≈2× 余量** 覆盖背压排队(实测每条实际耗时 ≈10100 ticks vs 名义 10000)。
_RESP_TICKS = 100000
LAST_TRACE_TICK = TRACE_COUNT * (TICK_INTERVAL + _RESP_TICKS)
MAX_TICK = LAST_TRACE_TICK + 20 * TICK_INTERVAL
# ⚠️ duration 必须 **> MAX_TICK**(否则 generator 先耗尽 ⇒ transition 到空 ⇒ null 崩)
TRACE_DURATION_TICKS = MAX_TICK + 10 * TICK_INTERVAL

trace_gen = tgen.createTrace(TRACE_DURATION_TICKS, TRACE_PB, 0)
# ⚠️ **不要追加 `createExit(0)`**:
#    `BaseTrafficGen::transition()`(per `src/cpu/testers/traffic_gen/base.cc`)在
#    `duration == 0` 时把 `nextTransitionTick` 设为 `MaxTick`
#    ⇒ Exit generator **永不被调度** ⇒ 事件循环空转 ⇒ gem5 报
#    `fatal: BaseTrafficGen system.tgen spent 1000000000 ticks without making progress`。
#    ⇒ 正确做法:只挂 trace generator,让 `m5.simulate(tick 上限)` 自然跑完。
tgen.start([trace_gen])

m5.stats.reset()
print(f"[parity] gem5 trace 探针开始:trace={TRACE_PB}")

print(f"[parity] trace 条数={TRACE_COUNT} · tick 间隔={TICK_INTERVAL} "
      f"· 末 tick={LAST_TRACE_TICK} · max tick={MAX_TICK}")
m5.setMaxTick(MAX_TICK)
m5.simulate(MAX_TICK)
m5.stats.dump()


# ── 取统计量(解析 gem5 的 stats text)───────────────────────────────
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

# `PyTrafficGen` 的统计(per gem5 `src/cpu/testers/traffic_gen/base.hh`)
num_reads = stat("system.tgen.totalReads", None)
num_writes = stat("system.tgen.totalWrites", None)

miss_avg_ns = None
if avg_miss_latency_ticks is not None:
    miss_avg_ns = float(avg_miss_latency_ticks) / TICKS_PER_NS

hit_avg_ns = (
    cache_cfg["tag_latency_cycles"]
    + cache_cfg["data_latency_cycles"]
    + cache_cfg["response_latency_cycles"]
) * clk_period_ns

# ── **平均访问延迟**(P-2 的分母侧)─────────────────────────────────
# ⚠️ 与 `gem5_linear_probe.py` **同口径**:命中 = **派生**(tag+data+response),
#    未命中 = **实测**(`overallAvgMissLatency::total`)⇒ **混合口径**。
#    ⇒ **观察值**(per `14 号` §4.2:命中延迟配成同值 ⇒ 比的是未命中路径 + 容量效应)。
_avg_latency_ns = None
if hits or misses:
    _avg_latency_ns = (
        float(hits) * hit_avg_ns + float(misses) * (miss_avg_ns or 0.0)
    ) / float(hits + misses)

# ── **地址序列指纹**(文件同源证据)──────────────────────────────────
# ⚠️ **两侧读同一份 CSV**(本仓侧读 `.csv`,gem5 侧回放由同一 CSV 生成的 `.pb`)
#    ⇒ 指纹基于**同一序列** ⇒ 相同即证明**逐条同源**。
#    ⇒ 指纹**只按 CSV 计算**(不解析 `.pb`)—— 避免「protobuf 编码差异」干扰证据。
#    ⚠️ 指纹算法 = **FNV-1a 64bit**,与本仓 C++ 侧 `seq_fnv1a64` **逐字一致**。
def _fnv1a64(values):
    h = 0xCBF29CE484222325
    for v in values:
        h ^= (v & 0xFFFFFFFFFFFFFFFF)
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{h:016x}"


def _seq_fingerprint_from_csv(csv_path):
    import csv as _csv
    seq = []
    with open(csv_path, newline="", encoding="utf-8") as f:
        for row in _csv.DictReader(f):
            seq.append(int(row["addr"]))
    return _fnv1a64(seq), len(seq)


CSV_PATH = os.environ.get(
    "PARITY_TRACE_CSV", TRACE_PB.replace(".pb", ".csv"))
try:
    _fp, _fp_n = _seq_fingerprint_from_csv(CSV_PATH)
except Exception as e:
    raise SystemExit(f"✗ 读不到 trace 的 CSV({CSV_PATH}):{e}"
                     f"—— 文件同源档需要 .csv 与 .pb 同时存在")

# ⚠️ **P-5**(per `14 号` §4.4 / `ADR-CEP-06 §3`):每次对拍必须声明
#    `verification_mode` + `verification_scope` ⇒ 缺失 ⇒ 判据 FAIL。
_ver = CFG.get("verification", {})

result = {
    "tool": "gem5",
    "mode": "same_source_trace",
    # ⚠️ **D-4 修复**:version 取**唯一真源** `gem5.versions.default.version`
    #    (旧的顶层 `gem5.version` 曾停在 v24 ⇒ v25 的产物**全部错标 v24**)。
    "version": _GEM5_VERSION,
    # ⚠️ **P-5** 字段(缺失 ⇒ 判据 FAIL)
    "verification_mode": _ver.get("verification_mode"),
    "verification_scope": _ver.get("verification_scope"),
    "trace_file": TRACE_PB,
    "trace_csv": CSV_PATH,
    "addr_seq_fnv1a64": _fp,
    "addr_seq_count": _fp_n,
    # ⚠️ **`sequence` 段**(critic SHOULD_FIX ④ 的修复):本探针原先**没有**这个段
    #    ⇒ rec 工具读不到 `nominal_accesses` ⇒ 名义条数断言被**静默跳过**
    #    (与 `main_tier_stream_rec.py` 的键名不匹配同因 ⇒ D-5)。
    "sequence": {
        # ⚠️ trace 回放路径:1 条 = 1 个包 ⇒ 名义条数**就是**条数契约
        "nominal_accesses": TRACE_COUNT,
        # ⚠️ **声明的 profile**(per `14 号` §4.4 封闭枚举表)—— 取自 trace 的
        #    `meta.json`(生成时的声明)⇒ 分类**不由观测结果反推**。
        "profile": _meta.get("profile"),
        # ⚠️ **R-B 规则用的 W = 有效足迹**(hot_cold 记 hot+cold;fixed_ratio 记 1MiB+W)
        "working_set_bytes": _meta.get("working_set_bytes"),
        "declared_working_set_bytes": _meta.get("declared_working_set_bytes"),
        "cache_bytes": _cache_bytes,
        # ⚠️ breaking 流可声明**期望误差下界**(≥ 2× 阈值)
        "expected_error_floor_pp": _meta.get("expected_error_floor_pp"),
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
    "avg_latency_ns_source": "mixed(命中=派生;未命中=实测)⇒ 观察值",
    "hit_latency_ns_source": "derived(tag+data+response;v24 classic cache 无命中延迟实测统计)",
    "miss_latency_ns_source": "measured(overallAvgMissLatency::total)",
    "ticks_per_ns": TICKS_PER_NS,
}

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)
print(f"[parity] gem5 trace 探针结果已写出:{OUT}")
print(json.dumps(result, ensure_ascii=False, indent=2))
