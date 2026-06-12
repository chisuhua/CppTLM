# gem5 参考 SoC 设计调研报告

**调研目标**：为 CppTLM（C++ TLM 2.0 混合仿真框架）的 SoC 拓扑设计提供参考。CPU 建模不在范围内。
**调研版本**：gem5 `develop` 分支（截至 2026-06-10），`stable` 分支部分脚本已弃用。
**调研基础**：gem5 官方 GitHub 镜像 `gem5/gem5`（即 googlesource 上的 `public/gem5`），所有引用均使用 develop 分支的 raw.githubusercontent.com 路径。

---

## 0. 路径与版本勘误（重要）

gem5 v23+ 引入"标准库（gem5 standard library）"后，老的脚本路径已重命名/废弃：

| 用户原题中的路径 | 实际路径（v25/develop） | 状态 |
|---|---|---|
| `configs/example/simple.py` | **不存在**（被 `configs/example/gem5_library/` 取代） | 旧 `se.py`/`fs.py` 已 `fatal()` 拒绝运行 |
| `configs/learning_gem5/part1_simple_system/` | `configs/learning_gem5/part1/`（`simple.py`/`two_level.py`） | 正确 |
| `configs/learning_gem5/part2_simple_system/` | `configs/learning_gem5/part2/`（`simple_cache.py`/`simple_memobj.py`/`run_simple.py`/`hello_goodbye.py`） | 正确 |
| `configs/learning_gem5/part3_replacement_policy/` | `configs/learning_gem5/part3/`（`msi_caches.py`/`ruby_caches_MI_example.py`/`test_caches.py`/`simple_ruby.py`/`ruby_test.py`） | 正确 |
| `configs/learning_gem5/part4_visualization/` | **不存在**（代码无此目录；可视化已并入 part3 章节） | N/A |
| `configs/example/dma_in_loopback.py` | **不存在**（DMA 测试已并入 `memtest.py` 的 `--num-dmas` 选项） | 替代：`memtest.py` |
| `configs/example/multi_level_cache.py` | **不存在**（已并入 `memtest.py` 的 `-c caches[:level]` 树形参数） | 替代：`memtest.py` |
| `configs/example/garnet_synth_ctrl.py` | **不存在**（正确名是 `garnet_synth_traffic.py`） | 替代：`garnet_synth_traffic.py` |
| `configs/example/ruby_network_test.py` | **不存在**（正确名是 `ruby_direct_test.py`） | 替代：`ruby_direct_test.py` |
| `configs/example/disk-image*` | **无独立脚本**；`--disk-image` 是 `arm/starter_fs.py`/`gpufs/runfs.py`/`gem5_library/*-ubuntu-run.py` 的命令行参数 | 通过参数注入 |
| `configs/example/hwaccel_*` | **不存在**；gem5 风格的硬件加速器已重命名为 `ext/`/`chi_tlm/`/HSA-accelerator | 替代：`chi_tlm/tlm_generator_test.py` |
| `configs/example/riscv/` | **不存在**；RISC-V 配置在 `gem5_library/riscv-*` + `example/lupv/` | 替代：见 §6.4 |

**附加发现**：
- 旧 `se.py`/`fs.py`（`develop` 分支）只调用 `fatal()`，提示迁移到 `gem5_library/`。
- 完整目录见：<https://github.com/gem5/gem5/tree/develop/configs>

---

## 1. 教学示例：Learning gem5 系列 [SE]

> **来源**：`configs/learning_gem5/`（[README](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/README)）
> 全部为 Syscall-Emulation 模式（`Root(full_system=False, …)`），工作负载为预编译的 `tests/test-progs/` 二进制。

### 1.1 `configs/learning_gem5/part1/simple.py` — [SE] 极简单核

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part1/simple.py>

**架构概要**：
gem5 教学系列的"Hello World"配置。一个 X86TimingSimpleCPU 直接连到 `SystemXBar`，没有 cache，通过 DDR3_1600_8x8 内存控制器访问 512 MiB 物理内存。中断控制器（X86 PIO/int_requestor/int_responder）也直接连到 membus。运行 `tests/test-progs/hello` 二进制。

**关键代码引用**（[simple.py#L60-L78](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part1/simple.py#L60-L78)）：
```python
system.cpu = X86TimingSimpleCPU()
system.membus = SystemXBar()
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports
system.cpu.createInterruptController()
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
```

**拓扑图**：
```
+----------+      +-----------+      +----------------+
| X86Timing|      |           |      |   MemCtrl      |
| SimpleCPU|----->| SystemXBar|----->| (DDR3_1600_8x8)|
+----------+      | (membus)  |      +----------------+
   |               |           |
   v               v           v
[IRQ ctrl]   [system_port]  [512 MiB DRAM]
```

**规模**：1 核 / 0 cache 级 / 1 DDR3 通道 / 总线：SystemXBar（coherent） / 协议：none（直接 uncached 访问）

---

### 1.2 `configs/learning_gem5/part1/two_level.py` — [SE] 2 级 cache

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part1/two_level.py>（配合 [`caches.py`](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part1/caches.py)）

**架构概要**：
经典的"L1 I/D 分离 + L2 共享"配置。L1ICache(16KiB/2-way)、L1DCache(64KiB/2-way) 通过 `l2bus = L2XBar()` 汇接到 256KiB/8-way L2Cache，L2 再经 `membus = SystemXBar()` 挂到 MemCtrl+DDR3。所有 cache 都使用 Classic（非 Ruby）的 in-cache MSHR 协议（`BaseCache.py`）。

**关键代码引用**（[two_level.py#L70-L94](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part1/two_level.py#L70-L94)）：
```python
system.cpu.icache = L1ICache(args); system.cpu.dcache = L1DCache(args)
system.l2bus = L2XBar()
system.cpu.icache.connectBus(system.l2bus)
system.cpu.dcache.connectBus(system.l2bus)
system.l2cache = L2Cache(args)
system.l2cache.connectCPUSideBus(system.l2bus)
system.membus = SystemXBar()
system.l2cache.connectMemSideBus(system.membus)
```

**拓扑图**：
```
              +-----------+
   +----+    /|           |\         +-----+      +--------+
   |CPUi|---/ |  L2XBar    | \------>| L2  |----->|membus|----->[MemCtrl]
   +----+    \(l2bus)      |/        |Cache|      |XBar   |      [DDR3]
              |\           /         +-----+      +--------+
   +----+    /|           |\
   |CPUd|---/ |           |
   +----+    \|           |/
             +-----------+
   (L1 私有)
```

**规模**：1 核 / 2 cache 级（L1I+L1D 私有 → L2 共享） / 1 DDR3 通道 / 互联：SystemXBar + L2XBar / 协议：Classic（默认 MESI 衍生，in-cache snooping）

---

### 1.3 `configs/learning_gem5/part2/simple_cache.py` — [SE] SimpleCache memobj

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part2/simple_cache.py>

**架构概要**：
教学示例 2，演示"自定义内存对象（SimObject）"插在 CPU 和 membus 之间。`SimpleCache` 是一个用户编写的 `SimObject`（在 `src/learning_gem5/part2/SimpleCache.py` 中实现），统一处理 I/D 请求。CPU 的 `icache_port` 和 `dcache_port` 都接到 `system.cache.cpu_side`，`system.cache.mem_side` 接到 `SystemXBar`。

**关键代码引用**（[simple_cache.py#L51-L57](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part2/simple_cache.py#L51-L57)）：
```python
system.cache = SimpleCache(size="1KiB")
system.cpu.icache_port = system.cache.cpu_side
system.cpu.dcache_port = system.cache.cpu_side
system.cache.mem_side = system.membus.cpu_side_ports
```

**拓扑图**：
```
+----------+      +--------------+      +-----------+      +--------+
| X86Timing|      | SimpleCache  |      |           |      | MemCtrl|
| SimpleCPU|----->| (1KiB, unified| ---->| SystemXBar|----->| DDR3   |
+----------+      |  I+D, custom)|      | (membus)  |      |        |
                  +--------------+      +-----------+      +--------+
```

**规模**：1 核 / 1 cache 级（1 KiB unified, 自定义 memobj） / 1 DDR3 / Classic 协议

---

### 1.4 `configs/learning_gem5/part2/simple_memobj.py` — [SE] SimpleMemobj（passthrough）

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part2/simple_memobj.py>

**架构概要**：
演示如何实现一个**透传**的 memobj（不进 cache 决策，直接转发）。`SimpleMemobj` 暴露独立的 `inst_port`/`data_port`/`mem_side` 三个端口。

**关键代码引用**（[simple_memobj.py#L51-L58](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part2/simple_memobj.py#L51-L58)）：
```python
system.memobj = SimpleMemobj()
system.cpu.icache_port = system.memobj.inst_port
system.cpu.dcache_port = system.memobj.data_port
system.memobj.mem_side = system.membus.cpu_side_ports
```

**拓扑**：1 核 / 0 cache / 1 透传 memobj / 1 DDR3 / 协议：透传（无 cache，模拟 LLC bypass）

---

### 1.5 `configs/learning_gem5/part2/hello_goodbye.py` + `run_simple.py` — [SE] 最小 HelloObject

**源文件**：
- <https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part2/hello_goodbye.py>
- <https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part2/run_simple.py>

**架构概要**：
最简 SimObject 示例，连 `System` 都没有——只创建一个 `Root(full_system=False)` 并实例化一个 `SimpleObject()`，模拟立刻结束。`hello_goodbye.py` 则扩展了 `HelloObject`，添加 `say_hello()`/`say_goodbye()` 事件。教学作用大于实用。

**规模**：0 核 / 0 cache / 0 内存 / 0 协议（仅验证 SimObject/Event 机制）

---

### 1.6 `configs/learning_gem5/part3/msi_caches.py` — [SE+FS] Ruby MSI

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/msi_caches.py>

**架构概要**：
**第一个进入 Ruby 协议**的教学配置。`MyCacheSystem` 子类化 `RubySystem`，创建 N 个 L1 controller + 1 个 DirController + SimpleNetwork（点对点，2D mesh 完全图）。`MSI` 协议下虚拟网络 = 3（request / response / forward）。

**关键代码引用**（[msi_caches.py#L48-L78](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/msi_caches.py#L48-L78)）：
```python
self.network = MyNetwork(self)
self.number_of_virtual_networks = 3
self.network.number_of_virtual_networks = 3
self.controllers = [L1Cache(system, self, cpu) for cpu in cpus] + [
    DirController(self, system.mem_ranges, mem_ctrls)
]
self.sequencers = [RubySequencer(version=i, dcache=..., clk_domain=...) for i in range(len(cpus))]
self.network.connectControllers(self.controllers)   # 点对点
```

**`MyNetwork` 实现**（[msi_caches.py#L228-L266](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/msi_caches.py#L228-L266)）：SimpleNetwork + Switch + SimpleExtLink/SimpleIntLink，双向完全图（`for ri in routers: for rj in routers: if ri != rj`）。**没有 Garnet**，是简单路由+有序链路。

**L1Cache 类**（[msi_caches.py#L120-L170](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/msi_caches.py#L120-L170)）：继承 `MSI_L1Cache_Controller`，关联 16KiB/8-way `RubyCache`。

**DirController**（[msi_caches.py#L174-L218](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/msi_caches.py#L174-L218)）：继承 `MSI_Directory_Controller`，挂 `RubyDirectoryMemory`。

**拓扑图**：
```
N x [L1Cache] -- SimpleExtLink -- N x [Switch] -- 完全图 SimpleIntLink
                                       |
                                  [DirController] -- 1x [SimpleMemory]
```

**规模**：N 核（默认 2，见 simple_ruby.py）/ 1 cache 级 + directory / SimpleMemory / 网络：完全图 SimpleNetwork（无 Garnet） / 协议：MSI

---

### 1.7 `configs/learning_gem5/part3/ruby_caches_MI_example.py` — [SE+FS] Ruby MI_example

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/ruby_caches_MI_examples.py>（注意：实际文件名是 `ruby_caches_MI_example.py`，单数 example）

**架构概要**：
与 `msi_caches.py` 结构几乎相同，但使用 `MI_example` 协议（更简单，只有 Modified/Invalid 两态），**5 个虚拟网络**。教学作用：演示如何换协议（注释说 "You can change simple_ruby to import from this file instead of from msi_caches to use the MI_example protocol instead of MSI"）。

**关键代码引用**（[ruby_caches_MI_example.py#L43-L48](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/ruby_caches_MI_example.py#L43-L48)）：
```python
self.number_of_virtual_networks = 5
self.network.number_of_virtual_networks = 5
```

**规模**：N 核 / 1 cache 级 + directory / 协议：MI_example（5 vnets）

---

### 1.8 `configs/learning_gem5/part3/simple_ruby.py` + `ruby_test.py` + `test_caches.py` — [SE] Ruby 测试驱动

**源文件**：
- <https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/simple_ruby.py>
- <https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/ruby_test.py>
- <https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/test_caches.py>

**架构概要**：
- `simple_ruby.py`：2 个 `X86TimingSimpleCPU` + MSI cache 系统 + `tests/test-progs/threads`（多线程假共享基准）。
- `test_caches.py`：基于 `msi_caches.py` 的 `TestCacheSystem`（同 N 端口 + SimpleNetwork）。
- `ruby_test.py`：[**SE**] 用 `RubyTester`（`checks_to_complete=100, num_cpus=2`）替代真实 CPU，注入随机访问测试 coherence 正确性。这是 gem5 中"非 CPU"的 cache 系统测试器。

**关键代码引用**（[ruby_test.py#L43-L51](https://github.com/gem5/gem5/blob/develop/configs/learning_gem5/part3/ruby_test.py#L43-L51)）：
```python
system.tester = RubyTester(checks_to_complete=100, wakeup_frequency=10, num_cpus=2)
system.mem_ctrl = SimpleMemory(latency="50ns", bandwidth="0GiB/s")
system.caches = TestCacheSystem()
system.caches.setup(system, system.tester, [system.mem_ctrl])
```

**规模**：2 核 / 1 级 cache / Ruby MSI / 测试驱动（RubyTester 替代 CPU）

---

## 2. 经典 Example 目录（除学习系列外） [SE]

> gem5 `develop` 分支中位于 `configs/example/` 顶层。SE 模式（`Root(full_system=False, …)`），除非标注。

### 2.1 `configs/example/memtest.py` — [SE] 树形 cache + DMA 测试

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/memtest.py>

**架构概要**：
gem5 自带的"内存系统压测工具"，**递归创建任意深度的 cache 树**，每层 L2XBar 汇接。命令行 `-c "2:2:1"` `-t "1:1:0:2"` 指定每层 cache 数和 tester 数。每个 tester 是 `MemTest`（注入 false sharing 的负载生成器）。支持 `--noncoherent-cache` 在末层加 `NoncoherentCache(size="16MiB")` 作为 LLC。树根可挂 `NoncoherentCache` 或直接接 `SimpleMemory`。

**关键代码引用**（[memtest.py#L268-L300](https://github.com/gem5/gem5/blob/develop/configs/example/memtest.py#L268-L300)）：
```python
proto_l1 = Cache(size="32KiB", assoc=4, clusivity="mostly_incl", writeback_clean=True)
# 按 cachespec 缩放生成 cache_proto[0]=LN, cache_proto[-1]=L1
make_cache_level(cachespec, cache_proto, len(cachespec), None)
last_subsys.xbar.point_of_coherency = True
if args.noncoherent_cache:
    system.llc = NoncoherentCache(size="16MiB", assoc=16)
    last_subsys.xbar.mem_side_ports = system.llc.cpu_side
    system.llc.mem_side = system.physmem.port
```

**`clusivity` 在 L2/L3 之间交替**（[memtest.py#L202-L213](https://github.com/gem5/gem5/blob/develop/configs/example/memtest.py#L202-L213)）：
```python
next.writeback_clean = not prev.writeback_clean
if prev.clusivity.value == "mostly_incl":
    next.clusivity = "mostly_excl"
else:
    next.clusivity = "mostly_incl"
```

**拓扑图**（`-c 2:2:1 -t 1:1:0:2`）：
```
[Tester] [Tester]    [Tester] [Tester]    [Tester] [Tester]
   \       /            \       /            \       /
  [L1#1] [L1#2]      [L1#1] [L1#2]       (no L1 here, only tester)
     \   /                \   /
   [L2XBar]            [L2XBar]
        \   |   |  /         \   |   |  /
         [L2#1] [L2#2]        [L2#1] [L2#2]
            \       /            \       /
          [L2XBar L3-subtree] [L2XBar L3-subtree]
                  \                   /
                   [Top L2XBar] --[NoncoherentCache(LLC)]-- [SimpleMemory]
```

**规模**：可配置 / N cache 级（每级 L2XBar + Cache） / 协议：Classic（in-cache）+ 末级 NoncoherentCache（不参与 coherence）

---

### 2.2 `configs/example/memcheck.py` — [SE] 树形 + MemChecker

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/memcheck.py>

**架构概要**：
memtest 的**带验证**变体——在每条 tester 路径上加 `MemCheckerMonitor` + 集中式 `MemChecker`，用 `TrafficGen`（配置脚本写入的 3-state IDLE/RANDOM/LINEAR 状态机）替代 `MemTest`。Cache 仍为经典 in-cache。

**关键代码引用**（[memcheck.py#L160-L180](https://github.com/gem5/gem5/blob/develop/configs/example/memcheck.py#L160-L180)）：
```python
proto_tester = TrafficGen(config_file=cfg_file_path)
# ...
testers = [proto_tester() for i in range(ntesters)]
checkers = [MemCheckerMonitor(memchecker=system.memchecker) for i in range(ntesters)]
tester.port = checker.cpu_side_port
checker.mem_side_port = cache.cpu_side
```

**规模**：可配置树 / N cache 级 / 协议：Classic / 验证：MemChecker（cache-coherent invariant check）

---

### 2.3 `configs/example/cache_partitioning.py` — [SE] Cache Partitioning

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/cache_partitioning.py>

**架构概要**：
展示 v25 新增的 **Cache Partitioning Policy**。`PyTrafficGen` → 64KiB/8-way `NoncoherentCache(partitioning_manager=PartitionManager(...))` → IOXBar → `SimpleMemory(64KiB)`。两种 policy：`WayPolicyAllocation`（指定 ways 数量）和 `MaxCapacityPartitioningPolicy`（按比例分配容量）。

**关键代码引用**（[cache_partitioning.py#L94-L107](https://github.com/gem5/gem5/blob/develop/configs/example/cache_partitioning.py#L94-L107)）：
```python
part_manager = PartitionManager(partitioning_policies=[generatePartPolicy(args)])
system.cache = NoncoherentCache(
    size="64KiB", assoc=8, partitioning_manager=part_manager,
    tag_latency=0, data_latency=0, response_latency=0, mshrs=1,
    replacement_policy=MRURP(),
)
```

**规模**：0 CPU / 1 cache / 64 KiB memory / 协议：None（NoncoherentCache）

---

### 2.4 `configs/example/garnet_synth_traffic.py` — [SE] Garnet NoC 合成流量

> 注：用户原题中的 `garnet_synth_ctrl.py` 不存在，正确名是 `garnet_synth_traffic.py`。

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/garnet_synth_traffic.py>

**架构概要**：
**Garnet 网络 NoC 仿真**。完全无 CPU——`GarnetSyntheticTraffic` 对象直接通过 `system.ruby._cpu_ports.in_ports` 注入合成流量到 Garnet 路由器。注入模式：`uniform_random / tornado / bit_complement / bit_reverse / bit_rotation / neighbor / shuffle / transpose`。**仅支持 Garnet_standalone 协议**（不带 cache coherence，只仿 NoC）。

**关键代码引用**（[garnet_synth_traffic.py#L130-L137](https://github.com/gem5/gem5/blob/develop/configs/example/garnet_synth_traffic.py#L130-L137)）：
```python
cpus = [GarnetSyntheticTraffic(
    num_packets_max=args.num_packets_max,
    single_sender=args.single_sender_id, single_dest=args.single_dest_id,
    sim_cycles=args.sim_cycles, traffic_type=args.synthetic,
    inj_rate=args.injectionrate, inj_vnet=args.inj_vnet,
    precision=args.precision, num_dest=args.num_dirs,
) for i in range(args.num_cpus)]
```

调用：`./build/NULL/gem5.debug configs/example/garnet_synth_traffic.py --num-cpus=16 --num-dirs=16 --network=garnet2.0 --topology=Mesh_XY --mesh-rows=4`

**规模**：0 真实 CPU / 0 cache（仅 Garnet_standalone 路由器）/ NoC：Garnet 2.0 + Mesh_XY / 流量：合成注入器

**关联文件**（[src/cpu/testers/garnet_synthetic_traffic/](https://github.com/gem5/gem5/tree/develop/src/cpu/testers/garnet_synthetic_traffic)）：流量发生器实现。

---

### 2.5 `configs/example/ruby_random_test.py` — [SE] Ruby MOESI Hammer 随机测试

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/ruby_random_test.py>

**架构概要**：
Ruby 协议回归测试器。`RubyTester` 对象注入随机请求，支持 `MOESI_hammer`、`MESI_Three_Level` 等协议（依据 build 选项 `--protocol` 切换）。cache 尺寸**故意设得很小**（L1D/L1I=256B、L2=512B、L3=1KiB，2-way），制造 race 条件。

**关键代码引用**（[ruby_random_test.py#L97-L107](https://github.com/gem5/gem5/blob/develop/configs/example/ruby_random_test.py#L97-L107)）：
```python
args.l1d_size = "256B"; args.l1i_size = "256B"
args.l2_size = "512B"; args.l3_size = "1KiB"
args.l1d_assoc = 2; args.l1i_assoc = 2; args.l2_assoc = 2; args.l3_assoc = 2
tester = RubyTester(check_flush=check_flush, checks_to_complete=args.maxloads,
                    wakeup_frequency=args.wakeup_freq)
```

**规模**：0 真实 CPU / 3 cache 级（L1+L2+L3） / Ruby MOESI_hammer / 测试器

---

### 2.6 `configs/example/ruby_mem_test.py` — [SE] Ruby + MemTest + DMA

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/ruby_mem_test.py>

**架构概要**：
`MemTest` 注入到 Ruby，加 `--num-dmas` 启用 DMA 测试。`system.dma_devices` 经 `dma_ports=dma_ports` 注入 `Ruby.create_system()`，挂到 `dma_cntrl` 控制器。`deadlock_threshold=5000000` 防止 bursty memtester 误报。

**关键代码引用**（[ruby_mem_test.py#L83-L99](https://github.com/gem5/gem5/blob/develop/configs/example/ruby_mem_test.py#L83-L99)）：
```python
if args.num_dmas > 0:
    dmas = [MemTest(...) for i in range(args.num_dmas)]
    system.dma_devices = dmas
dma_ports = []
for i, dma in enumerate(dmas):
    dma_ports.append(dma.test)
Ruby.create_system(args, False, system, dma_ports=dma_ports)
```

**规模**：N CPU + M DMA / Ruby / 协议：MESI/MOESI 等

---

### 2.7 `configs/example/ruby_direct_test.py` — [SE] Ruby Directed Tester

> 注：用户原题中 `ruby_network_test.py` 不存在。

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/ruby_direct_test.py>

**架构概要**：
定向 coherence 测试器（区别于随机 tester）。4 种 pattern：`SeriesGetx`（连续 GetX/写）、`SeriesGets`（连续 GetS/读）、`SeriesGetMixed`（混合读写 %）、`Invalidate`（生成 invalidate 流）。

**关键代码引用**（[ruby_direct_test.py#L80-L96](https://github.com/gem5/gem5/blob/develop/configs/example/ruby_direct_test.py#L80-L96)）：
```python
if args.test_type == "SeriesGetx":
    generator = SeriesRequestGenerator(num_cpus=args.num_cpus, percent_writes=100)
elif args.test_type == "SeriesGets":
    generator = SeriesRequestGenerator(num_cpus=args.num_cpus, percent_writes=0)
elif args.test_type == "Invalidate":
    generator = InvalidateGenerator(num_cpus=args.num_cpus)
```

**规模**：0 真实 CPU / 1 RubyTester（directed）/ Ruby / 协议：MESI/MOESI/…

---

### 2.8 `configs/example/hmc_hello.py` — [SE] HMC 内存 + CPU

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/hmc_hello.py>

**架构概要**：
**HMC（Hybrid Memory Cube）** 作为主存的单核配置。`HMC.add_options()` 注入 HMC 控制器配置；通过 `MemConfig.config_mem()` 把 HMC 挂到 membus。CPU 直连 membus（无 cache）。

**关键代码引用**（[hmc_hello.py#L43-L62](https://github.com/gem5/gem5/blob/develop/configs/example/hmc_hello.py#L43-L62)）：
```python
HMC.add_options(parser)
# ...
system.cpu = ObjectList.cpu_list.get(options.cpu_type)()
MemConfig.config_mem(options, system)   # 这里会创建 HMC 控制器
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports
```

**规模**：1 核 / 0 cache / HMC 内存 / membus：SystemXBar / 协议：none

---

### 2.9 `configs/example/dramsys.py` — [SE] DRAMSys 集成

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/dramsys.py>

**架构概要**：
将 **DRAMSys**（外部 SystemC DRAM 仿真器）作为 gem5 的内存后端。`DRAMSys` SimObject 在 gem5 侧提供 `port`，运行时与外部 DRAMSys 进程通信。教学/混合仿真价值。

**规模**：1 核 / 0 cache / DRAMSys 内存（SystemC co-sim）/ 协议：none

---

### 2.10 `configs/example/sc_main.py` — [SE] SystemC 协同仿真入口

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/sc_main.py>

**架构概要**：
gem5 充当 SystemC 仿真主控的入口：`m5.systemc.sc_main(*sys.argv)`。创建一个 `SystemC_Kernel()` 放到 `Root(full_system=True, systemc_kernel=kernel)`。用于 gem5↔SystemC 联合仿真（例如 TLM 加速器模型）。

**规模**：0 CPU / 0 cache / SystemC kernel 作为 system 根

---

### 2.11 `configs/example/etrace_replay.py` — [SE] 指令跟踪重放

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/etrace_replay.py>

**架构概要**：
读取预录制的 instruction trace（`.etrace` 文件，由 `gem5 m5util trace` 生成）并通过 `EtraceCPU` 注入。无 cache 仿真，纯访存 trace 重放。常用于快速研究 cache 行为。

**规模**：1 EtraceCPU（伪 CPU） / 0 cache（可选） / 协议：可选

---

### 2.12 `configs/example/splash2/run.py` — [SE] Splash-2 基准 + 共享 L2

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/splash2/run.py>

**架构概要**：
跑 Splash-2 benchmark 套件（Cholesky/FFT/LU/Radix/Barnes/FMM/Ocean/Raytrace/Water-NSquared/Water-Spatial）。**所有 CPU 共享同一个 L2**：`system.toL2bus = L2XBar()` 汇接所有 L1。每个 CPU 通过 `cpu.addPrivateSplitL1Caches()` 加 L1 I+D。

**关键代码引用**（[splash2/run.py#L228-L246](https://github.com/gem5/gem5/blob/develop/configs/splash2/run.py#L228-L246)）：
```python
system.toL2bus = L2XBar(clock=busFrequency)
system.l2 = L2(size=args.l2size, assoc=8)
system.physmem.port = system.membus.mem_side_ports
system.l2.cpu_side = system.toL2bus.mem_side_ports
system.l2.mem_side = system.membus.cpu_side_ports
for cpu in cpus:
    cpu.addPrivateSplitL1Caches(L1(size=args.l1size, assoc=1), L1(size=args.l1size, assoc=4))
    cpu.connectAllPorts(system.toL2bus.cpu_side_ports, system.membus.cpu_side_ports, ...)
```

**规模**：N 核（默认 1, `-n` 指定）/ 2 cache 级（L1 私有 + L2 共享）/ 1 SimpleMemory / 协议：Classic

---

### 2.13 `configs/splash2/cluster.py` — [SE] Splash-2 + Cluster 拓扑

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/splash2/cluster.py>

**架构概要**：
Splash-2 的**层次化变体**：CPU 分成 `numclusters` 个 cluster，每个 cluster 一个 L2XBar (`clusterbus`) + 一个 L1 Cache 共享给 cluster 内所有 CPU。`toL2bus` 再汇接所有 cluster 的 L1 到全局 L2。

**关键代码引用**（[splash2/cluster.py#L152-L170](https://github.com/gem5/gem5/blob/develop/configs/splash2/cluster.py#L152-L170)）：
```python
cluster.clusterbus = L2XBar(clock=busFrequency)
cluster.cpus = [TimingSimpleCPU(...) for i in range(cpusPerCluster)]
cluster.l1 = L1(size=args.l1size, assoc=4)
# ...
cluster.l1.cpu_side = cluster.clusterbus.mem_side_ports
cluster.l1.mem_side = system.toL2bus.cpu_side_ports
```

**拓扑图**（4 核 / 2 cluster / 2 核每 cluster）：
```
Cluster 0:                    Cluster 1:
  CPU0--\                       CPU2--\
  CPU1--->[clusterbus]---L1#0---|  CPU3--->[clusterbus]---L1#1
              |                         |
              +--->[toL2bus]<-----------+
                        |
                       L2 (shared)
                        |
                     [membus]
                        |
                    [SimpleMemory]
```

**规模**：N 核分 M cluster / 2 cache 级（cluster-shared L1 + 全局 L2） / 协议：Classic

---

## 3. ARM Full-System 参考 [FS]

> ARM FS 模板使用 `Root(full_system=True, …)`、RealView 平台、ARM 通用中断控制器 (GIC)、VirtIO 块设备加载 disk image。Linux 内核通过 DTB 自动生成或外部提供。

### 3.1 `configs/example/arm/starter_fs.py` — [FS] ARM 单核 Linux 启动

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/starter_fs.py>

**架构概要**：
ARM Research Starter Kit 的标准 FS 模板。`devices.SimpleSystem` 创建 RealView 平台、CPU cluster（默认 1 核 `MinorCPU`/4GHz）+ L1+L2 cluster private caches。PCI 子系统挂 `PciVirtIO(vio=VirtIOBlock(image=CowDiskImage(...)))` 作为根文件系统磁盘。Linux 通过 `setupBootLoader` + DTB（自动生成）启动。

**关键代码引用**（[arm/starter_fs.py#L65-L92](https://github.com/gem5/gem5/blob/develop/configs/example/arm/starter_fs.py#L65-L92)）：
```python
system = devices.SimpleSystem(
    want_caches, args.mem_size, mem_mode=mem_mode,
    workload=ArmFsLinux(object_file=SysPaths.binary(args.kernel)),
    readfile=args.script,
)
# 添加 VirtIO 块设备作为 boot disk
system.pci_devices = [
    PciVirtIO(vio=VirtIOBlock(image=create_cow_image(args.disk_image)))
]
# Wire up memory system + CPU cluster
system.connect()
system.cpu_cluster = [devices.ArmCpuCluster(
    system, args.num_cores, args.cpu_freq, "1.0V", *cpu_types[args.cpu],
    tarmac_gen=args.tarmac_gen, tarmac_dest=args.tarmac_dest,
)]
system.addCaches(want_caches, last_cache_level=2)
system.realview.setupBootLoader(system, SysPaths.binary)
```

**关键特性**（[arm/starter_fs.py#L121-L142](https://github.com/gem5/gem5/blob/develop/configs/example/arm/starter_fs.py#L121-L142)）：4 种 CPU：`atomic`(AtomicSimpleCPU)、`minor`(MinorCPU)、`hpi`(HPI/HPI)、`o3`(O3_ARM_v7a_3)；`--with-pmu` 启 PMU；`--tarmac-gen` 写 Tarmac trace。

**拓扑图**：
```
1×[ARM CPU] -- L1I + L1D -- [Cluster L2XBar] -- L2 (cluster shared)
                                   |
                              [Membus SystemXBar] -- [DDR3/DDR4]
                                   |
                              [IOBus] -- [GIC] -- [PCI] -- [VirtIO Block] -- [CowDiskImage]
                                   |
                              [system.realview platform (VExpress_GEM5)]
```

**规模**：1-N 核（`--num-cores`）/ 2 cache 级（L1 私有 + L2 cluster 共享） / DDR3-1600/8x8 / 1 VirtIO 块设备 / 协议：Classic

---

### 3.2 `configs/example/arm/starter_se.py` — [SE] ARM 多核 SE 启动

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/starter_se.py>

**架构概要**：
ARM SE 多核版本。`--num-cores` 启动多个 CPU（默认 atomic MinorCPU），每个跑一个 `Process`（多进程 SE 模式）。`SimpleSeSystem` + `ArmCpuCluster` + `addCaches(want_caches, last_cache_level=2)`。`MemConfig.config_mem` 配置 DDR3 或 DDR4，可调通道数。

**关键代码引用**（[arm/starter_se.py#L72-L90](https://github.com/gem5/gem5/blob/develop/configs/example/arm/starter_se.py#L72-L90)）：
```python
system = devices.SimpleSeSystem(mem_mode=mem_mode)
system.cpu_cluster = devices.ArmCpuCluster(
    system, args.num_cores, args.cpu_freq, "1.2V", *cpu_types[args.cpu],
    tarmac_gen=args.tarmac_gen, tarmac_dest=args.tarmac_dest,
)
system.addCaches(want_caches, last_cache_level=2)
system.mem_ranges = [AddrRange(start=0, size=args.mem_size)]
MemConfig.config_mem(args, system)
```

**规模**：N 核 / 2 cache 级 / DDR3 or DDR4 / 0 IO 设备 / SE 模式

---

### 3.3 `configs/example/arm/fs_bigLITTLE.py` — [FS] big.LITTLE 异构多核

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/fs_bigLITTLE.py>

**架构概要**：
**经典 ARM big.LITTLE 异构**：`BigCluster` 用 `O3_ARM_v7a_3`（乱序，高频 2GHz），`LittleCluster` 用 `MinorCPU`（顺序，低频 1GHz）。每个 cluster 内部独立 cache。**两个 cluster 共享 membus**，挂多个 `PciVirtIO` 块设备 + 简单 SimpleMemory。Platform 默认 `VExpress_GEM5`。

**关键代码引用**（[arm/fs_bigLITTLE.py#L46-L84](https://github.com/gem5/gem5/blob/develop/configs/example/arm/fs_bigLITTLE.py#L46-L84)）：
```python
class BigCluster(devices.ArmCpuCluster):
    def __init__(self, system, num_cpus, cpu_clock, cpu_voltage="1.0V"):
        cpu_config = [ObjectList.cpu_list.get("O3_ARM_v7a_3"), devices.L1I, devices.L1D, devices.L2]
        super().__init__(system, num_cpus, cpu_clock, cpu_voltage, *cpu_config)

class LittleCluster(devices.ArmCpuCluster):
    def __init__(self, system, num_cpus, cpu_clock, cpu_voltage="1.0V"):
        cpu_config = [ObjectList.cpu_list.get("MinorCPU"), devices.L1I, devices.L1D, devices.L2]
        super().__init__(system, num_cpus, cpu_clock, cpu_voltage, *cpu_config)

cpu_types = {
    "timing": (BigCluster, LittleCluster),
    "exynos": (Ex5BigCluster, Ex5LittleCluster),
}
# Only add KVM/FastModel if compiled in
if devices.have_kvm: cpu_types["kvm"] = (KvmCluster, KvmCluster)
if devices.have_fastmodel: cpu_types["fastmodel"] = (FastmodelCluster, FastmodelCluster)
```

**挂载磁盘**（[arm/fs_bigLITTLE.py#L116-L131](https://github.com/gem5/gem5/blob/develop/configs/example/arm/fs_bigLITTLE.py#L116-L131)）：
```python
sys.disk_images = [cow_disk(f) for f in disks]
sys.pci_vio_block = [PciVirtIO(vio=VirtIOBlock(image=img)) for img in sys.disk_images]
for dev in sys.pci_vio_block:
    sys.attach_pci(dev)
```

**拓扑图**：
```
[Big CPU 0..N] -- L1I+D -- [Cluster L2] --+
                                          |
[Little CPU 0..M] -- L1I+D -- [Cluster L2] -+--> [membus] -- [SimpleMemory × N]
                                                       |
                                                  [IOBus] -- [GIC] -- [PCI × N] -- [VirtIO Block × N]
                                                       |
                                                  [RealView platform]
```

**规模**：N 大核（O3/2GHz） + M 小核（Minor/1GHz） / 2 cache 级（每 cluster 私有 L2） / KVM/FastModel 可选 / 1-N PCI 块设备 / 协议：Classic

---

### 3.4 `configs/example/arm/ruby_fs.py` — [FS] ARM + Ruby MOESI

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/ruby_fs.py>

**架构概要**：
ARM FS + Ruby 协议。CPU 通过 `Ruby.create_system(args, True, system, system.iobus, system._dma_ports, system.realview.bootmem, cpus)` 注入。L1D=64KiB、L1I=32KiB、L2=2MiB、L3=16MiB（通过 `--l1d_size` 等参数可调）。IObus 走 GIC 中断 + VirtIO 块设备。

**关键代码引用**（[arm/ruby_fs.py#L52-L78](https://github.com/gem5/gem5/blob/develop/configs/example/arm/ruby_fs.py#L52-L78)）：
```python
def config_ruby(system, args):
    cpus = [cpu for cluster in system.cpu_cluster for cpu in cluster.cpus]
    Ruby.create_system(
        args, True, system, system.iobus, system._dma_ports,
        system.realview.bootmem, cpus,
    )
    system.ruby.clk_domain = SrcClockDomain(clock=args.ruby_clock,
                                            voltage_domain=system.voltage_domain)
    for cluster in system.cpu_cluster:
        for i, cpu in enumerate(cluster.cpus):
            system.ruby._cpu_ports[i].connectCpuPorts(cpu)
```

**规模**：1-N 核 / 3 cache 级（L1+L2+L3）/ Ruby 协议（CHI/MESI_Three_Level 等）

---

### 3.5 `configs/example/arm/fdp_neoverse_v2.py` — [FS] ARM Neoverse V2 (Fixed Debugger Protocol)

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/fdp_neoverse_v2.py>

**架构概要**：
基于 Arm **Fast Models** 协同仿真（SystemC + FDP 协议）的 Neoverse V2（Armv9）平台。SystemC 端跑真实 CPU 模型，gem5 端模拟外设和 memory。

---

### 3.6 `configs/example/arm/baremetal.py` — [FS] ARM Baremetal 通用平台

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/baremetal.py>

**架构概要**：
通用的 ARM baremetal 平台（无 Linux，可加载 `ArmBaremetal` 等专用 workload），常用于驱动固件仿真。

---

### 3.7 `configs/example/arm/etrace_se.py` — [SE] ARM ETrace 重放

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/etrace_se.py>

**架构概要**：
ARM 指令跟踪重放（与 §2.11 类似，但指定 ARM ISA）。

---

### 3.8 `configs/example/arm/dist_bigLITTLE.py` — [FS] 分布式 big.LITTLE（PDES）

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/arm/dist_bigLITTLE.py>

**架构概要**：
PDES（Parallel Discrete Event Simulation）模式的 big.LITTLE，使用 KVM CPU + 用户态 GIC + `sim-quantum=1ms` 时间片，多个 CPU 跑在不同事件队列上并行仿真。

---

## 4. Ruby + Garnet 网络拓扑库（独立可复用）

> 路径：`configs/topologies/`

| 文件 | 拓扑 | 关键特点 |
|---|---|---|
| [`Crossbar.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/Crossbar.py) | 中心化 Crossbar | N+1 个 router，最后一个 router 作为 crossbar；ext_link 单向内部 link 两条。Garnet 用 [`CrossbarGarnet.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/CrossbarGarnet.py) |
| [`Pt2Pt.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/Pt2Pt.py) | 全连接点对点 | N router 完全图，每对之间两条 IntLink（双向）。高基数 router |
| [`Mesh_XY.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/Mesh_XY.py) | 2D Mesh + XY 路由 | `num_rows × num_cols` 网格；东-西 weight=1，南-北 weight=2（XY 死锁自由）；支持 per-vnet links（`--per-vnet-links`） |
| [`Mesh_westfirst.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/Mesh_westfirst.py) | 2D Mesh + West-First 路由 | 自适应路由变体 |
| [`MeshDirCorners_XY.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/MeshDirCorners_XY.py) | Mesh + 角落 Directory | Directory 节点固定在 mesh 4 角 |
| [`CustomMesh.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/CustomMesh.py) | 用户自定义 Mesh | `router_list` 指定每类节点（RNF/HNF/MN/SNF/RNI）的 router 位置；CHI 协议用 |
| [`Cluster.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/Cluster.py) | 层次 Cluster | Cluster 内 1 跳，Cluster 间可设带宽/延迟 |
| [`BaseTopology.py`](https://github.com/gem5/gem5/blob/develop/configs/topologies/BaseTopology.py) | 抽象基类 | `SimpleTopology` 与 `BaseTopology` 接口 |

**调用方式**（`--topology=Mesh_XY --mesh-rows=4`），由 `Ruby.create_system()` 通过 `--topology` 选项加载。

---

## 5. RISC-V FS 参考 [FS]

> gem5 中 RISC-V FS 主要通过 `gem5_library` + `LupioBoard` 实现。

### 5.1 `configs/example/lupv/run_lupv.py` — [FS] RISC-V LupIO 平台

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/lupv/run_lupv.py>（[README](https://github.com/gem5/gem5/blob/develop/configs/example/lupv/README.md)）

**架构概要**：
RISC-V 上跑 Linux + LupIO 设备族（UART、TIMER、PLIC、BLOCK、SPI、I2C、GPU、VIDEO）的小型 SoC。`LupvBoard`（`gem5.components.boards.experimental.lupv_board`）使用 `PrivateL1PrivateL2WalkCacheHierarchy(L1d=32KiB, L1i=32KiB, L2=512KiB)` + `SingleChannelDDR3_1600(128MiB)`。CPU 是 `RiscvTimingSimpleCPU` 或 `AtomicSimpleCPU`。内核 + busybox disk image 通过 `obtain_resource("riscv-lupio-linux-kernel", "riscv-lupio-busybox-img")` 拉取。

**关键代码引用**（[lupv/run_lupv.py#L41-L77](https://github.com/gem5/gem5/blob/develop/configs/example/lupv/run_lupv.py#L41-L77)）：
```python
cache_hierarchy = PrivateL1PrivateL2WalkCacheHierarchy(
    l1d_size="32KiB", l1i_size="32KiB", l2_size="512KiB"
)
memory = SingleChannelDDR3_1600(size="128MiB")
processor = SimpleProcessor(cpu_type=CPUTypes.TIMING, num_cores=args.num_cpus, isa=ISA.RISCV)
board = LupvBoard(clk_freq="1GHz", processor=processor, memory=memory, cache_hierarchy=cache_hierarchy)
board.set_kernel_disk_workload(
    kernel=obtain_resource("riscv-lupio-linux-kernel", resource_version="1.0.0"),
    disk_image=obtain_resource("riscv-lupio-busybox-img", resource_version="1.0.0"),
)
```

**规模**：1-N 核 / 2 cache 级（per-core L1 + shared L2 + walk cache） / DDR3 1 channel / LupIO 设备族 / 协议：Classic

---

### 5.2 `configs/example/gem5_library/riscvmatched-hello.py` — [SE] RISC-V Matched 板

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/riscvmatched-hello.py>

**架构概要**：
**预制 RISC-V 板**（`RISCVMatchedBoard`）的 SE 模式 hello world。预制板默认带 L1 + L2 cache hierarchy + DDR4 memory + 标准 Linux boot。

**关键代码**：
```python
from gem5.prebuilt.riscvmatched.riscvmatched_board import RISCVMatchedBoard
board = RISCVMatchedBoard()
board.set_se_binary_workload(obtain_resource("riscv-hello", resource_version="1.0.0"))
```

---

### 5.3 `configs/example/gem5_library/riscvmatched-fs.py` — [FS] RISC-V Matched 板 + Ubuntu

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/riscvmatched-fs.py>

**架构概要**：
同一 `RISCVMatchedBoard`（`clk_freq="1.2GHz", l2_size="2MiB", is_fs=True`）跑 Ubuntu 24.04。`KernelBootedExitHandler` 自定义处理：`-i` 参数让仿真在 kernel 启动后立即退出。

**规模**：1-N 核（默认 1）/ 2 cache 级 / DDR4 / Ubuntu FS / 协议：Classic

---

## 6. gem5 标准库（gem5_library）[SE+FS]

> 这是 v23+ 推荐的现代配置范式。位于 [`configs/example/gem5_library/`](https://github.com/gem5/gem5/tree/develop/configs/example/gem5_library)，使用 `Board` + `Simulator` API。

### 6.1 `x86-ubuntu-run.py` — [FS] x86 Ubuntu 启动

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/x86-ubuntu-run.py>

**架构概要**：
**最简化的现代 x86 FS 模板**。直接用预制的 `X86DemoBoard`（默认 L1+L2 + DDR4 + 标准外设 + KVM 可选），调用 `board.set_workload(obtain_resource("x86-ubuntu-24.04-boot-with-systemd"))` 启动 Ubuntu 24.04 镜像，`Simulator(board=board).run()` 一行启动。

**关键代码**（[`x86-ubuntu-run.py#L43-L57`](https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/x86-ubuntu-run.py#L43-L57)）：
```python
from gem5.prebuilt.demo.x86_demo_board import X86DemoBoard
board = X86DemoBoard()
workload = obtain_resource("x86-ubuntu-24.04-boot-with-systemd", resource_version="5.0.0")
board.set_workload(workload)
simulator = Simulator(board=board)
simulator.run()
```

**规模**：默认 1 核（`X86DemoBoard` 默认参数）/ 2 cache 级 / DDR4 / 标准外设 / Ubuntu FS / 协议：Classic

---

### 6.2 `arm-ubuntu-run.py` — [FS] ARM Ubuntu 启动

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/arm-ubuntu-run.py>

**架构概要**：
用 `PrivateL1PrivateL2CacheHierarchy(L1d=16KiB, L1i=16KiB, L2=256KiB)` + `DualChannelDDR4_2400(2GiB)` + 2×`TimingSimpleCPU`。`VExpress_GEM5_Foundation` 平台 + `ArmDefaultRelease`（Armv8 扩展集）。启动后通过 `gem5-bridge hypercall 3` 退出。

**关键代码**（[`arm-ubuntu-run.py#L40-L70`](https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/arm-ubuntu-run.py#L40-L70)）：
```python
cache_hierarchy = PrivateL1PrivateL2CacheHierarchy(l1d_size="16KiB", l1i_size="16KiB", l2_size="256KiB")
memory = DualChannelDDR4_2400(size="2GiB")
processor = SimpleProcessor(cpu_type=CPUTypes.TIMING, num_cores=2, isa=ISA.ARM)
release = ArmDefaultRelease()
platform = VExpress_GEM5_Foundation()
board = ArmBoard(clk_freq="3GHz", processor=processor, memory=memory, cache_hierarchy=cache_hierarchy,
                 release=release, platform=platform)
workload = obtain_resource("arm-ubuntu-24.04-boot-with-systemd", resource_version="3.0.0")
board.set_workload(workload)
```

**规模**：2 核 / 2 cache 级 / DDR4 双通道 / 协议：Classic / 平台：VExpress_GEM5_Foundation

---

### 6.3 `riscv-ubuntu-run.py` — [FS] RISC-V Ubuntu 启动

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/riscv-ubuntu-run.py>

**架构概要**：
类似 arm-ubuntu 但用 `RiscvBoard`（`gem5.components.boards.riscv_board`）+ `PrivateL1PrivateL2WalkCacheHierarchy`（多了 walk cache 用于页表遍历）+ `DualChannelDDR4_2400(3GiB)`。启动 RISC-V Ubuntu 24.04。

**规模**：2 核 / 2 cache 级 + walk cache / DDR4 双通道 / 协议：Classic

---

### 6.4 `x86-mi300x-gpu.py` / `x86-mi355x-gpu.py` — [FS] AMD MI300X dGPU

**源文件**：
- <https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/x86-mi300x-gpu.py>
- <https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/x86-mi355x-gpu.py>

**架构概要**：
**异构 CPU + GPU 集成 SoC** 模板。`ViperBoard` 预制板：1 核 `KvmCPU` + `MI300X` GPU（带 `HBM2Stack(size="16GiB")`）+ `ViperCPUCacheHierarchy`（CPU 侧 MOESI_AMD_Base）+ `SingleChannelDDR4_2400(8GiB)`（CPU host 内存）。Coherence protocol `GPU_VIPER`（编译时指定）。

**关键代码**（[`x86-mi300x-gpu.py#L69-L96`](https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/x86-mi300x-gpu.py#L69-L96)）：
```python
gpu0 = MI300X(gpu_memory=HBM2Stack(size="16GiB"))
board = ViperBoard(
    clk_freq="3GHz", processor=processor, memory=memory,
    cache_hierarchy=ViperCPUCacheHierarchy(), gpus=[gpu0],
)
```

**拓扑图**：
```
[KvmCPU] -- [ViperCPUCacheHierarchy] -- [DDR4_2400 8GiB]      (CPU host side)
                                     \
                                      \-- [VIPER Coherence Network] -- [GPU TCM]
                                                                         |
                                                                       [HBM2 16GiB]
                                                                         |
                                                                       [MI300X GPU]
                                                                         |
                                                              (kernel launches via HSAPP/CP)
```

**规模**：1 KVM 核 + 1 MI300X GPU / 2 cache 级（CPU 侧 MOESI_AMD_Base）/ DDR4 host + HBM2 device / 协议：GPU_VIPER

---

### 6.5 `riscvmatched-microbenchmark-suite.py` — [SE] RISC-V Vertical Microbenchmark

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/riscvmatched-microbenchmark-suite.py>

**架构概要**：
展示如何使用 `Suite` API：在 `RISCVMatchedBoard` 上批量跑 `riscv-vertical-microbenchmarks` 套件，filter by input group（如 `cca`）。适用于套件级 RISC-V 微基准评测。

---

## 7. AMD GPU + dGPU 模板 [SE+FS]

> 路径：`configs/example/gpufs/`，使用 VIPER / MOESI_AMD_Base coherence protocol。

### 7.1 `configs/example/apu_se.py` — [SE] AMD APU（CPU + GPU SoC）

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/apu_se.py>

**架构概要**：
**最完整的"CPU + GPU 集成 SoC"模板**。N 个 X86TimingSimpleCPU（`X86O3CPU` 也支持） + 1 个 `Shader`（含 `ComputeUnit` 数组，每 CU 默认 4 SIMD、10 wavefronts、64 wavefront size、64 KiB LDS、SQC/TCC/Scalar/TCP cache）+ `HSAPacketProcessor` + `GPUDispatcher` + `GPUCommandProcessor`。Coherence 通过 VIPER 协议。CPU/GPU 共享 DDR4 host memory（无独立 device memory）。

**关键组件**（[apu_se.py#L545-L600](https://github.com/gem5/gem5/blob/develop/configs/example/apu_se.py#L545-L600)）：
- `Shader.CUs = [ComputeUnit(...), ...]`：每个 CU 有 4 SIMD、`VectorRegisterFile(2048 regs)`、`ScalarRegisterFile(2048)`、`LdsState(banks=32, size=65536)`
- `gpu_hsapp = HSAPacketProcessor(pioAddr=..., numHWQueues=10)`：HSA packet processor
- `dispatcher = GPUDispatcher(kernel_exit_events=True)`：调度器
- `gpu_cmd_proc = GPUCommandProcessor(hsapp=gpu_hsapp, dispatcher=dispatcher)`：命令处理器
- `gpu_driver = GPUComputeDriver(filename="kfd", isdGPU=False, gfxVersion="gfx902", dGPUPoolID=0, m_type=5)`

**`--dgpu` 标志**（[apu_se.py#L756-L770](https://github.com/gem5/gem5/blob/develop/configs/example/apu_se.py#L756-L770)）：切换到 dGPU 模式，GPU 走 PCIe-like IO bus 而非共享内存。

**拓扑图**：
```
[X86Timing/O3 × N]  -- VIPER protocol  --+
                                          |
[HSAPP]  [Dispatcher]  [CmdProcessor]  --|--> [VIPER Ruby Network]
[CU #0..M] -- [TCP/TCC/SQC/Scalar]  ----+        |
                                                   |
                                            [GPU DRAM (host mem)]
```

**规模**：1-N CPU + 1-N CU（默认 4）/ TCP/TCC/SQC/Scalar + L2 / 1 host memory / 协议：GPU_VIPER

---

### 7.2 `configs/example/gpufs/mi200.py` — [FS] AMD MI200 dGPU

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/mi200.py>

**架构概要**：
**dGPU FS 启动**：CPU 走 KVM、`--dgpu_mem_size="16GiB"` 设备内存、`--dgpu_start="0GiB"` 地址、`--disjoint` 启用 `DisjointNetwork`（CPU/GPU 物理隔离网络）。通过 base64 编码把 GPU app 注入 `demo_runscript` 在启动时执行。

**关键代码**（[`gpufs/mi200.py#L155-L168`](https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/mi200.py#L155-L168)）：
```python
args.ruby = True
args.cpu_type = "X86KvmCPU"
args.mem_size = "8GiB"     # CPU host memory
args.dgpu = True
args.dgpu_mem_size = "16GiB"  # GPU device memory
args.disjoint = True       # CPU/GPU disjoint networks
```

**规模**：1 KVM CPU + 1 MI200 GPU / 2 套独立网络（CPU + GPU）/ DDR4 host + HBM2 device / 协议：MOESI_AMD_Base

---

### 7.3 `configs/example/gpufs/Disjoint_VIPER.py` — [FS] Disjoint Network 拓扑

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/Disjoint_VIPER.py>

**架构概要**：
定义 **Disjoint VIPER Ruby 系统**：两个独立网络 `network_cpu` + `network_gpu`，CPU/GPU 控制器物理分离；用 `connectCPU` / `connectGPU` 分别绑定；11 个虚拟网络（涵盖 CPU request/response + GPU VIPER）。

**关键代码**（[`gpufs/Disjoint_VIPER.py#L41-L62`](https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/Disjoint_VIPER.py#L41-L62)）：
```python
if "garnet" in options.network:
    self.network_cpu = DisjointGarnet(self)
    self.network_gpu = DisjointGarnet(self)
else:
    self.network_cpu = DisjointSimple(self)
    self.network_gpu = DisjointSimple(self)
self.number_of_virtual_networks = 11
self.network_cpu.number_of_virtual_networks = 11
self.network_gpu.number_of_virtual_networks = 11
self.network_cpu.connectCPU(options, cpu_cntrls)
self.network_gpu.connectGPU(options, gpu_cntrls)
```

---

### 7.4 `configs/example/gpufs/DisjointNetwork.py` — 拓扑容器

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/DisjointNetwork.py>

**架构概要**：
`DisjointSimple` / `DisjointGarnet` 网络类，参数化 `cpu_topology` / `gpu_topology`（分别独立选择 `Mesh_XY` / `Crossbar` / `Pt2Pt` 等）。

---

### 7.5 `configs/example/gpufs/mi300.py` / `gpufs/runfs.py` — dGPU FS 通用

**源文件**：
- <https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/mi300.py>
- <https://github.com/gem5/gem5/blob/develop/configs/example/gpufs/runfs.py>

**架构概要**：
mi300 类似 mi200。`runfs.py` 提供 `addRunFSOptions` 通用 FS GPU runner，处理 kernel/disk/app 注入。

---

## 8. CHI TLM 2.0 硬件加速器集成 [SE]

> 路径：`configs/example/chi_tlm/`（gem5 中"硬件加速器"最直接的对应物是 TLM 2.0 / SystemC 加速器集成）

### 8.1 `configs/example/chi_tlm/tlm_generator_test.py` — [SE] CHI TLM 2.0 加速器

**源文件**：<https://github.com/gem5/gem5/blob/develop/configs/example/chi_tlm/tlm_generator_test.py>

**架构概要**：
gem5 集成 **CHI TLM 2.0 加速器**。每个 CPU 端口是 `TlmGenerator`（发 TLM transaction，max_pending_tran=64，独立 3 GHz 时钟），通过 `TLM_RNF` 包装连接到 `SimpleNetwork` 上的 CHI 节点。CHI 节点类型 `CHI_RNF / CHI_HNF / CHI_MN / CHI_SNF_MainMem / CHI_SNF_BootMem / CHI_RNI_DMA / CHI_RNI_IO` 通过 [`configs/example/noc_config/2x4.py`](https://github.com/gem5/gem5/blob/develop/configs/example/noc_config/2x4.py) 映射到 2×4 mesh 的特定 router。L3=16MiB/16-way 共享 cache。`4GiB SimpleMemory`。

**关键代码**（[`chi_tlm/tlm_generator_test.py#L47-L66`](https://github.com/gem5/gem5/blob/develop/configs/example/chi_tlm/tlm_generator_test.py#L47-L66)）：
```python
class TLM_RNF(CHI_Node):
    def __init__(self, ruby_system, parent):
        super().__init__(ruby_system)
        self._cntrl = TlmController(version=Versions.getVersion(CHI_Cache_Controller),
                                    ruby_system=ruby_system)
        parent.out_port = self._cntrl.in_port
        parent.in_port = self._cntrl.out_port

def rnf_gen(options, ruby_system, cpus):
    return [TLM_RNF(ruby_system, cpu) for cpu in system.cpu]
```

**调用**：`./build/ALL/gem5.opt configs/example/chi_tlm/tlm_generator_test.py <suite> --num-cpus=N`

**规模**：N TLM 加速器 / 2×4 mesh NoC（CHI protocol）/ 1 L3 + 1 SimpleMemory / 协议：CHI

---

## 9. 其他 reference 配置（精简列出）

| 路径 | 模式 | 类型 | 关键内容 |
|---|---|---|---|
| `configs/example/etrace_replay.py` | SE | Trace replay | EtraceCPU + DDR4，可选 cache |
| `configs/example/sc_main.py` | FS | SystemC 协同仿真 | SystemC_Kernel 作为根 |
| `configs/example/se.py` | – | **已弃用** | 调 `fatal()` 指向 `gem5_library/` |
| `configs/example/fs.py` | – | **已弃用** | 同上 |
| `configs/example/hsaTopology.py` | – | APU 文件系统欺骗 | `createVegaTopology` / `createRavenTopology`，写入 KFD 拓扑文件模拟 ROCm 运行时 |
| `configs/example/read_config.py` | SE/FS | 配置回放 | 从 `.ini`/`.json` config 文件重建 SimObject 树 |
| `configs/example/cache_partitioning.py` | SE | Cache 划分 | WayPolicyAllocation / MaxCapacityPartitioningPolicy |
| `configs/example/hmc_hello.py` | SE | HMC 内存 | Hybrid Memory Cube 控制器 |
| `configs/example/dramsys.py` | SE | DRAMSys 集成 | 外部 SystemC DRAM 仿真器 |
| `configs/example/lupv/run_lupv.py` | FS | RISC-V LupIO | 见 §5.1 |
| `configs/example/gem5_library/x86-*-benchmarks.py` | SE/FS | x86 基准 | SPEC CPU2006/CPU2017、GAPBS、NPB 等 |
| `configs/example/gem5_library/arm-hello.py` | SE | ARM hello | `X86DemoBoard` 替换为 ARM 等价板 |
| `configs/example/gem5_library/x86-ubuntu-run-with-kvm.py` | FS | x86 + KVM 加速 | 用 KVM CPU 加速 Ubuntu 启动 |
| `configs/example/gem5_library/fdp-hello.py` | SE | Arm FDP 协同仿真 | gem5 端仅模拟 memory+IO |
| `configs/example/gem5_library/riscv-rvv-example.py` | SE | RISC-V 向量 | RISC-V "V" 扩展 |
| `configs/example/gem5_library/dramsys/` | SE | DRAMSys 多场景 | HBM2、HMC、GDDR 等 |
| `configs/example/gem5_library/multisim/` | SE | 多 workload 串联 | checkpoint + restore 流水线 |
| `configs/example/gem5_library/exit_handling/` | SE/FS | 自定义退出 | hypercall-driven exit handler |

---

## 10. 总结：gem5 涵盖的 SoC 拓扑空间（CPU 之外）

| 维度 | gem5 中的代表方案 |
|---|---|
| **CPU 数量** | 1（simple/two_level/memtest）→ 2/4/8（Ruby 测试）→ 16（Garnet 流量）→ 64+（Ruby MOESI Cluster）→ 异构 big.LITTLE / CPU+GPU APU |
| **Cache 层次** | 1 级（透传 memobj）→ 2 级（L1 私有 + L2 共享）→ 3 级（L1+L2+L3）→ 树形 N 级（memtest `2:2:1`）→ Cluster 拓扑（splash2 cluster.py） |
| **Cache 一致性** | ① 无 cache（透传）② Classic MESI（默认）③ Classic 划分（`cache_partitioning.py`）④ Ruby MSI（learning_gem5 part3）⑤ Ruby MI_example ⑥ Ruby MOESI_hammer ⑦ Ruby MOESI_AMD_Base（VIPER）⑧ CHI（CHI_tlm/CHI_config）|
| **互联** | ① SystemXBar（点对点/广播）② L2XBar（小规模 crossbar）③ IOXBar（IO）④ Ruby SimpleNetwork（点对点+Switch 完全图）⑤ Garnet 2.0 + Mesh_XY/Mesh_westfirst/CrossbarGarnet/Pt2Pt ⑥ CustomMesh ⑦ DisjointNetwork（CPU+GPU 双独立网络）⑧ CHI NoC（2x4 mesh with router_list mapping）|
| **内存控制器** | SimpleMemory（timing 50ns）/ DDR3_1600_8x8 / DDR4_2400 / HMC / HBM2Stack / DRAMSys 外部 / 多个 mem_ctrl × 多通道 |
| **存储 IO** | VirtIO 块设备（CowDiskImage）/ IDE / KFD 拓扑（ROCm 欺骗）/ LupIO 设备族（UART/TIMER/PLIC/BLOCK/SPI/I2C/GPU/VIDEO） |
| **加速器** | ① TLM 2.0 加速器（CHI 集成，tlm_generator_test.py）② GPU Shader+CU（apu_se.py）③ dGPU（mi200.py）④ SystemC 协仿真（sc_main.py）|
| **GPU 集成** | ① APU 共享内存（VIPER）② dGPU PCIe + 独立 HBM（Disjoint_VIPER）③ MI300X 预制板（ViperBoard）|
| **平台** | ① X86DemoBoard（x86 标准）② ArmBoard + VExpress_GEM5_Foundation（ARMv8）③ RiscvBoard（RISC-V）④ RISCVMatchedBoard（预制）⑤ LupvBoard（LupIO 设备）⑥ ViperBoard（AMD GPU 异构）|

---

## 11. 路径修正（汇总）

调研发现用户原题中的部分路径在 gem5 `develop` 分支已不存在或被重命名。修正对照表：

| 原题路径 | 实际路径 | 备注 |
|---|---|---|
| `configs/example/simple.py` | `configs/example/gem5_library/x86-ubuntu-run.py` 或 `learning_gem5/part1/simple.py` | 旧 se.py/fs.py 已 fatal() |
| `configs/learning_gem5/part1_simple_system/` | `configs/learning_gem5/part1/` | |
| `configs/learning_gem5/part2_simple_system/` | `configs/learning_gem5/part2/` | |
| `configs/learning_gem5/part3_replacement_policy/` | `configs/learning_gem5/part3/` | |
| `configs/learning_gem5/part4_visualization/` | 不存在 | 并入 part3 |
| `configs/example/dma_in_loopback.py` | `configs/example/memtest.py --num-dmas` | |
| `configs/example/multi_level_cache.py` | `configs/example/memtest.py -c "2:2:1"` | 树形参数 |
| `configs/example/hwaccel_*` | `configs/example/chi_tlm/tlm_generator_test.py` | TLM 2.0 加速器 |
| `configs/example/garnet_synth_ctrl.py` | `configs/example/garnet_synth_traffic.py` | |
| `configs/example/ruby_network_test.py` | `configs/example/ruby_direct_test.py` | |
| `configs/example/ruby_mem_test.py` | 存在 | |
| `configs/example/ruby_random_test.py` | 存在 | |
| `configs/example/disk-image*` | `arm/starter_fs.py --disk-image` / `gpufs/mi200.py --disk-image` | |
| `configs/example/arm/` | 存在（arm/{starter_fs,starter_se,fs_bigLITTLE,ruby_fs,fdp_neoverse_v2,baremetal,...}.py） | |
| `configs/example/riscv/` | 不存在；改用 `gem5_library/riscv-*` + `example/lupv/run_lupv.py` | |

---

## 12. 参考资源

- **gem5 官网**：<https://www.gem5.org/>
- **Learning gem5 教程**：<http://learning.gem5.org/>
- **gem5 GitHub**：<https://github.com/gem5/gem5>（`develop` 分支 = 最新）
- **gem5 GoogleSource**：<https://gem5.googlesource.com/public/gem5>（`stable` 分支 = 已弃用旧 se.py）
- **资源库 gem5-resources**：<https://github.com/gem5/gem5-resources>（disk images、kernels、benchmarks）

> **调研完成时间**：2026-06-10（Asia/Shanghai）
> **调研方式**：直接抓取 `https://raw.githubusercontent.com/gem5/gem5/develop/configs/...` raw 文件 + GitHub API 目录树 + grep_app_searchGitHub 搜索
