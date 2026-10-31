# scripts/cep/parity/gem5_cache_probe.py
#   —— 对拍基准的 **gem5 侧**测量脚本(由 `gem5.opt` 直接执行)
#
# 功能:用 gem5 的 `MemTest`(确定性访存测试器)+ `Cache` + `MemCtrl` + `SimpleMemory`
#       构成**无 CPU** 的最小测量台,测出 gem5 侧的
#         · 命中路径延迟(hit latency)
#         · 未命中路径延迟(miss latency)
#       并把结果写成 JSON(供 `parity_compare.py` 比较)。
#
# ── 为什么用 `MemTest`(而不是 CPU)────────────────────────────���─────────
# gem5 v24 的标准库 traffic generator **需要 CPU 基础设施**(不能 CPU-less),
# 而 CPU 路径又需要 workload 二进制 ⇒ 与「两侧消费同一条 trace」的要求相悖。
# `MemTest` 是 gem5 自带的 **ClockedObject 级**访存测试器(有 `RequestPort`)
# ⇒ 可直接挂在 `Cache` 的 CPU 侧 ⇒ **无需 CPU、无需二进制**。
# ⚠️ `MemTest` 的地址序列由 `Random::genRandom()` 播种 ⇒ **每次运行不同**
#    ⇒ 本脚本只用于**常数档**(hit / miss 延迟常数,与具体访存序列无关);
#    **主对拍(容量对齐档)需要同源 trace** ⇒ 该缺口已登记(见台账 Entry #113)。
#
# ── gem5 v24 API 事实(已逐条核对源码,勿凭印象改)─────────────────────
# ① `Cache.tag_latency` / `data_latency` / `response_latency` 是 **`Param.Cycles`**
#    ⇒ 收**整数**(cycle 数),**不是** "2ns" 字符串。
# ② `MemTest` 的统计**只有** `numReads` / `numWrites` / `numAtomics`
#    ⇒ **没有** `completed_loads` / `completed_funcs`(那是我最初写错的)。
# ③ `System` **没有** `clk_domain` 参数;v24 的正确挂法是
#    `system.clk_domain = SrcClockDomain(clock=..., voltage_domain=VoltageDomain())`。
# ④ tick → ns 换算:gem5 全局 tick 频率由 `m5.ticks.setGlobalFrequency()` 决定,
#    **默认 1THz**(1 tick = 1 ps)⇒ ns = ticks / 1000。
#    本档按「gem5 侧 1 clock = 1 ns」对齐 ⇒ `setGlobalFrequency("1GHz")` **不可用**
#    (那会把 1 个 clock 变成 1000 tick)⇒ 保持 1THz,用 `ticks / 1000` 换算 ns。
# ⑤ `Cache` 的 `mshrs` / `write_buffers` 是**无默认值**的必填 param ⇒ 必须显式给。
#
# 用法:external/gem5/build/NULL/gem5.opt scripts/cep/parity/gem5_cache_probe.py
#   参数经环境变量:`PARITY_OUT`(输出 JSON 路径,默认 gem5_cache_probe.json)
import json
import os
import pathlib

import m5
from m5.objects import (
    AddrRange,
    NoncoherentCache,
    MemTest,
    Root,
    SimpleMemory,
    SrcClockDomain,
    System,
    VoltageDomain,
)

# ── 参数(默认值与 `parity_config.json` 对齐;实际取值由调用方注入)──────────
CFG = json.loads(
    open(
        os.environ.get(
            "PARITY_CONFIG",
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "parity_config.json"),
        ),
        encoding="utf-8",
    ).read()
)
OUT = os.environ.get("PARITY_OUT", "gem5_cache_probe.json")

cache_cfg = CFG["gem5"]["cache"]
mem_cfg = CFG["gem5"]["memory"]
tester_cfg = CFG["gem5"]["tester"]

# ── 时基:gem5 全局 tick 频率固定 1THz(1 tick = 1 ps)⇒ ns = ticks / 1000 ──
# ⚠️ 不要用 `m5.ticks.setGlobalFrequency("1GHz")`:那会让 1 clock = 1000 tick,
#    反而把「cycle ↔ ns」的口径搞乱。本档的换算口径 = `ticks / 1000`。
TICKS_PER_NS = 1000.0

# 本档的 clock 周期(1GHz ⇒ 1 cycle = 1 ns);与本仓侧口径一致。
clk_period_ns = float(CFG["cycle_ns"])

# ⚠️ v24 的 `System` 有个**默认子对象** `dvfs_handler`,其
#    `sys_clk_domain = Param.SrcClockDomain(Parent.clk_domain)`
#    ⇒ `system.clk_domain` 必须在 **`System` 构造时**就存在,
#    否则 unproxy 阶段报 `Can't resolve proxy 'clk_domain' ... from 'system.dvfs_handler'`。
#    (官方 `configs/example/dramsys.py` 也是先建 `vd` 再 `system.clk_domain = ...`,
#     但那里 `System` 是随后单独构造的;此处为避免 default 子对象早于赋值,
#     直接用**构造参数**传入。)
vd = VoltageDomain()
clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=vd)
system = System(clk_domain=clk_domain)
system.mem_mode = "timing"

# ── 访存测试器(唯一 master)────────────────────────────────────────────
tester = MemTest()
tester.size = tester_cfg["working_set_bytes"]
tester.interval = tester_cfg["interval_cycles"]
tester.max_loads = tester_cfg["num_accesses"]
tester.percent_reads = 100  # 全读 ⇒ 纯延迟测量,排除写路径差异
# ⚠️ `percent_functional` 必须为 0:functional 访问走 `sendFunctional`
#    (零延迟、不经时序模型)⇒ 一旦开启,测到的就不是时序延迟了。
tester.percent_functional = 0
tester.percent_uncacheable = 0
tester.percent_atomic = 0
system.mem_tester = tester

# ── 缓存(命中路径 = tag + data + response;未命中 = 其上 + 存储延迟)──────
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
# ⚠️ 这三项是 `Param.Cycles` ⇒ 收**整数**;配置里的 "2ns"/"1ns" 需换算成 cycle
#    (本档 1 clock = 1 ns ⇒ 2ns ⇒ 2 cycles)。
cache.tag_latency = cache_cfg["tag_latency_cycles"]
cache.data_latency = cache_cfg["data_latency_cycles"]
cache.response_latency = cache_cfg["response_latency_cycles"]
# ⚠️ `mshrs` / `write_buffers` 在 `Cache.py` 里**无默认值** ⇒ 必须显式给。
cache.mshrs = cache_cfg["mshrs"]
cache.write_buffers = cache_cfg["write_buffers"]
cache.tgts_per_mshr = 8
system.cache = cache

# ── 存储(直接接 Cache 的 mem_side)─────────────────────────────────────
# ⚠️ v24 的 SimObject 构造是 **keyword-only** ⇒ 不能位置传参
#    (实测报 `takes 1 positional argument but 2 were given`)。
# ⚠️ `MemCtrl.dram` 的类型是 `Param.MemInterface` ⇒ **`SimpleMemory` 不可直接塞进去**
#    (实测报 `Error setting param MemCtrl.dram to <orphan SimpleMemory>`);
#    而 `MemInterface` 本身**没有** `latency` 参数(延迟藏在 DRAM 时序里)⇒
#    要精确控制「存储延迟常数」反而**不该**走 MemCtrl。
# ⇒ 官方 `configs/example/cache_partitioning.py` 的做法正是一致:
#    **`SimpleMemory` 直接挂 cache 的 `mem_side`**(无中间控制器)⇒ 延迟 = `latency` 精确可控。
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

# ── 接线:MemTest → Cache → Memory ────────────────────────────────────
tester.port = cache.cpu_side
cache.mem_side = memory.port

# ⚠️ v24:`m5.instantiate()` 前必须先建 `Root`(实测报
#    `Need to instantiate Root() before calling instantiate()`)。
root = Root(full_system=False, system=system)
m5.instantiate()

# ── 跑 ────────────────────────────────────────────────────────────
# ⚠️ `m5.stats.dump()` 在 v24 是注册到 `atexit` 的(见 `src/python/m5/simulate.py`),
#    **进程退出时**才输出 ⇒ 探针里**必须**在取完统计后**主动 dump**
#    (否则 stdout 永远看不到 stats 段,极易误判成「统计全 0」)。
# ⚠️ v24 的 `m5.stats` **没有 `get(name)` API**(实测:`src/python/m5/stats/__init__.py`
#    只导出 `dump` / `reset` / `enable` / `prepare` / `initSimStats`)
#    ⇒ 用 `m5.stats.get(...)` 取值会**静默走 except 分支返回默认值**
#    ⇒ 表现为「统计全 0」,极易误判成「模拟没跑」。
# ⇒ 正解:**用 gem5 官方 CLI 选项 `--stats-file=<path>`** 落盘,再解析该文本文件。
#    (实测 `m5.stats.addStatVisitor("text://...")` + `m5.stats.dump()` **不写文件**
#     —— 守卫 `new_dump` + 输出注册时机共同导致,不可依赖。)
m5.stats.reset()
print(f"[parity] gem5 探针开始:cache={cache_cfg} mem={mem_cfg}")
# ⚠️ **tick 上限必须按「全局 tick 频率」算,不是按 cycle 数**:
#    gem5 全局频率 = 1THz(1 tick = 1 ps)⇒ 1 个 1GHz cycle = **1000 ticks**。
#    `MemTest.interval` 是 **cycle 数** ⇒ 每次访问间隔 = interval * 1000 ticks。
#    (早期版本直接用 `num_accesses * (interval+1)` 当 tick 数 ⇒ 少算 1000 倍
#     ⇒ 模拟在第 4 次访问前就结束 ⇒ 统计全 0 —— 已被本行修正。)
TICKS_PER_CYCLE = 1000.0  # 1GHz clock @ 1THz 全局频率
sim_ticks = int(
    tester_cfg["num_accesses"] * (tester_cfg["interval_cycles"] + 1) * TICKS_PER_CYCLE
)
m5.simulate(sim_ticks)

# ⚠️ **v24 的 `m5.stats` 没有 `get(name)` API**(实测:`src/python/m5/stats/__init__.py`
#    只导出 `dump` / `reset` / `enable` / `prepare` / `initSimStats`,**无 `get`**)
#    ⇒ 早期版本用 `m5.stats.get(...)` 取值会**静默走 except 分支返回默认值**
#    ⇒ 表现为「统计全 0」,极易误判成「模拟没跑」。
# ⇒ 改为**解析 `m5.stats.dump()` 的文本输出**(gem5 官方 text visitor 的稳定格式):
#       `--------- Begin Simulation Statistics ---------`
#       `group.name    value    # description`
m5.stats.dump()


# ── 取统计量(解析 dump 文本)────────────────────────────────────────
def parse_stats_text(path):
    """把 gem5 的 stats text dump 解析成 `{统计名: 数值}`。

    格式(v24 text visitor):
        group.sub    12345    # description
    """
    out = {}
    if not os.path.exists(path):
        return out
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("-"):
            continue
        if line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        name, value = parts[0], parts[1]
        try:
            out[name] = float(value)
        except ValueError:
            continue
    return out


# 统计文件路径由 CLI `--stats-file=` 注入(见 gem5_cache_probe.sh 包装器);
# 环境变量 `PARITY_STATS` 优先,便于直接手工跑。
STATS_TEXT = os.environ.get(
    "PARITY_STATS",
    os.path.join(os.path.dirname(os.path.abspath(OUT)) or ".", "gem5_stats_probe.txt"),
)
STATS = parse_stats_text(STATS_TEXT)


def stat(name, default=None):
    v = STATS.get(name)
    return default if v is None else v


# ⚠️ **v24 classic Cache 的统计名带 `::total` / `::<requestor>` 后缀**
#    (实测:`system.cache.overallHits::total`)⇒ 必须用带后缀的全名。
hits = stat("system.cache.overallHits::total", 0)
misses = stat("system.cache.overallMisses::total", 0)
accesses = stat("system.cache.overallAccesses::total", 0)
miss_rate = stat("system.cache.overallMissRate::total", None)

# ⚠️ **v24 classic Cache 没有 `overallHitLatency` 统计**(实测 stats 文件 0 命中):
#    命中延迟统计在 `CacheStats` 的**按 requestor 向量**里(`hitLatency`),
#    而 classic cache 的**命中路径**在 v24 里不产生独立的延迟统计 ⇒
#    **命中延迟只能用「命中路径延迟 = tag + data + response」的解析式**。
#    ⚠️ 这意味着命中延迟是**配置派生值**,不是实测值 ⇒ 比较器必须把它当
#       「口径对齐项」而非「实测项」处理(见 `parity_compare.py` 的标注)。
avg_miss_latency_ticks = stat("system.cache.overallAvgMissLatency::total", None)

# ⚠️ `MemTest` 只有这三个统计量(v24 事实 ②),且**统计名无后缀**
num_reads = stat("system.mem_tester.numReads", 0)
num_writes = stat("system.mem_tester.numWrites", 0)
num_atomics = stat("system.mem_tester.numAtomics", 0)


def ticks_to_ns(t):
    if t is None:
        return None
    return float(t) / TICKS_PER_NS


# 命中延迟 = tag + data + response(**解析式**,v24 无实测统计 ⇒ 标为 derived)
hit_avg_ns = (
    cache_cfg["tag_latency_cycles"]
    + cache_cfg["data_latency_cycles"]
    + cache_cfg["response_latency_cycles"]
) * clk_period_ns
miss_avg_ns = None
if avg_miss_latency_ticks is not None:
    miss_avg_ns = ticks_to_ns(avg_miss_latency_ticks)

# ⚠️ **P-5**(per `14 号` §4.4 / `ADR-CEP-06 §3`):每次对拍必须声明
#    `verification_mode` + `verification_scope` ⇒ 缺失 ⇒ 判据 FAIL。
_ver = CFG.get("verification", {})

result = {
    "tool": "gem5",
    "version": CFG["gem5"]["version"],
    "verification_mode": _ver.get("verification_mode"),
    "verification_scope": _ver.get("verification_scope"),
    "hits": hits,
    "misses": misses,
    "accesses": accesses,
    "miss_rate": miss_rate,
    "hit_latency_ns_source": "derived(tag+data+response;v24 classic cache 无命中延迟实测统计)",
    "miss_latency_ns_source": "measured(overallAvgMissLatency::total)",
    "num_reads": num_reads,
    "num_writes": num_writes,
    "num_atomics": num_atomics,
    "hit_latency_ns": hit_avg_ns,
    "miss_latency_ns": miss_avg_ns,
    "ticks_per_ns": TICKS_PER_NS,
    "num_accesses": tester_cfg["num_accesses"],
    "scope_note": (
        "本档测的是 **gem5 侧路径延迟常数**;访存序列由 MemTest 的 "
        "Random::genRandom() 播种 ⇒ 每次运行不同 ⇒ **不构成同源 trace 对拍**"
    ),
}

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)
print(f"[parity] gem5 探针结果已写出:{OUT}")
print(json.dumps(result, ensure_ascii=False, indent=2))
