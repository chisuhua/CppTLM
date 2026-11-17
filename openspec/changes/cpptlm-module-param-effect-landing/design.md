# design: cpptlm-module-param-effect-landing

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（D-2 / D-3 / **D-6** / F-14）。
> 本文件只记录**实施层面的可逆选择**;不可逆决策以 ADR 为准。
> 逐键清单（D-1）是本 change 的**唯一真源** —— 实施期新增键必须回填此表 + `docs/implementation/module-param-baseline.md` 表 B。

## D-1 — 键盘点清单（2026-11-18 实测）

### D-1.1 盘点方法（可重跑）

1. **配置侧**：遍历 `configs/**/*.json`（排除 `configs/param_rules/**`）⇒ 收集每个「类型 × `params` 键」的存在性与实例数;
2. **读取面侧**：静态检索每个类型所在头文件是否有 `on_config_loaded() override` 或 `void set_config(` 覆写 ⇒ 无 ⇒ 该类型**零配置读取面**;
3. **效果面侧**：对有读取面的类型，逐键核对「读到的值是否进入运行时行为」（成员被读 / 被丢弃 / 被下游使用）;
4. **板级路径**：`ADR-X.17` F-15 的板级直读键（`dgpu_board_shell.cc`）**单列为「板级消费」**，不算零效果。

### D-1.2 类别 ① —— 零配置读取面（14 类 / 32 键×类型对）

⚠️ **本次新发现** —— `ADR-X.17` F-14 只登记了「有读取面但键未消费」与「声明了但成员不存在」，**未覆盖**「类型根本没有配置读取面」这一最大的一类。

| 类型 | 配置实例数 | 键数 | 键（并集） | 证据（读取面） |
|---|---:|---:|---|---|
| `CPUTLM` | 95 | 3 | `pattern`, `request_interval`, `start_addr` | `include/tlm/cpu_tlm.hh` 无 `on_config_loaded` / `set_config`;成员 `start_addr_`（`:19`）/ `request_interval_`（`:21`）只被构造期与运行期使用，从不由配置赋值 |
| `CacheTLM` | 37 | 6 | `assoc`, `associativity`, `level`, `line_size`, `protocol`, `size` | `include/tlm/cache_tlm.hh` 无配置读取面;内部只有 `cache_lines_`（`:42`），命中延迟是写死的 `hit ? 5 : 50`（`:89`）;⚠️ `level` **配置零命中**（仅被 `ADR-X.17` F-14 登记 ⇒ 仍须裁决） |
| `CrossbarTLM` | 11 | 1 | `port_count` | `include/tlm/crossbar_tlm.hh` 无配置读取面 |
| `ComputeUnitTLM` | 4 | 2 | `kernel_id`, `workgroup_size` | 无配置读取面 |
| `PcieControllerIP` | 3 | 3 | `bar_bases`, `bar_sizes`, `link_speed_gts` | 无配置读取面;⚠️ `bar_sizes` 在**板级**另有消费方（`dgpu_board_shell.cc:78-79` 读 `pcie_ep` 的 `bar_sizes`）⇒ 该键的「板级消费」需与本类型自身的键面分开处置（`ADR-X.17` D-3 三态条款） |
| `GPUTLM` | 3 | 4 | `end_addr`, `kernel_id`, `start_addr`, `workgroup_size` | 无配置读取面 |
| `PcieMemoryDevice` | 2 | 1 | `capacity_gb` | 无配置读取面（`PcieMemoryDevice` 退化为 PCIe 外观层，backing 由 `DGpuBoard` 注入 —— per `ADR-DGPU-05`） |
| `StreamingMultiprocessorTLM` | 2 | 2 | `execution_latency`, `warp_size` | 无配置读取面 |
| `ArbiterTLM` | 1 | 1 | `n_ports` | 无配置读取面 |
| `LwcBusHub` | 1 | 1 | `max_ip_ports` | 无配置读取面（`ADR-X.17` F-14 已登记） |
| `MemoryClusterTLM` | 1 | 2 | `capacity_gb`, `channels` | 无配置读取面（与 `MemoryCluster`（`cluster/memory_cluster.hh`，**有**读取面）是两个类） |
| `SharedMemoryTLM` | 1 | 2 | `banks`, `size_kb` | 无配置读取面 |
| `SocInternalNoc` | 1 | 2 | `data_flow`, `vc_counts_enabled` | 无配置读取面（该类已 archived —— per `openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-e1/`）⇒ **态 ② 的首选候选** |
| `TCCTLM` | 1 | 2 | `coalescing`, `size` | 无配置读取面 |
| **小计** | — | **32** | — | **14 类** |

### D-1.3 类别 ② —— 有读取面但该键未消费（3 类 / 9 键×类型对）

| 类型 | 键 | 现状（实测） | 板级消费？ |
|---|---|---|---|
| `TrafficGenTLM`（21 实例） | `pattern`, `num_requests`, `start_addr`, `end_addr`, `hotspot_addrs`, `hotspot_weights`, `stride` | `on_config_loaded` **只读 `seed`**（`include/tlm/traffic_gen_tlm.hh:106-110` 注释自述「其余 params 在本 change 之前即未被解析」）;`set_hotspot_config` / `set_stride` / `set_num_requests` / `set_addr_range` **无生产调用方**（仅 `include/tlm/stress_patterns.hh` 内部定义） | ❌ 无 |
| `MemoryTLM`（43 实例） | `size`, `type`, `mem_type`, `read_latency`, `write_latency` | `on_config_loaded` **只读 `capacity_gb`**（`include/tlm/memory_tlm.hh:162-168`）;⚠️ `mem_type` 在 `src/` + `include/` **零命中**（只出现在 configs）⇒ 也是零效果键（`ADR-X.17` F-14 未列它）;`read_latency` / `write_latency` 与写死的 `write_latency_cycles_ = 120` 数值巧合 ⇒ 零可观测差异 | ⚠️ `read_latency_hit_cycles` 等**板级直读键**属另一组键（见 R-1） |
| `GmmuTLM`（2 实例） | `page_size_bytes` | `on_config_loaded` 的 `contains` 分支**体为空**（`include/tlm/gpu/gmmu_tlm.hh:305-309`）⇒ 读而丢弃;实际翻译用 `constexpr page_size = 4096` | ⚠️ `tlb_size` / `tlb_miss_latency_cycles` 由**板级直读**消费（`dgpu_board_shell.cc:1056-1057`）⇒ 已有效果，不是零效果键 |

### D-1.4 类别 ③ —— 生成方产出、模块从不消费（1 键）

| 键 | 生成方 | 消费方 | 现状 |
|---|---|---|---|
| `routing` | `GpuNoC::simulate_instantiate`（`src/tlm/cluster/gpu_noc_cluster.cc:26-31`）为每个内部 router 生成 | ❌ 无 | `RouterTLM` 只有 `XYRouting` 一种算法 ⇒ 该键从未被消费（取值恒 `"XY"`）;`cpptlm-module-param-read-declares` 已用 `ignore_param("routing")` **显式登记**（基线台账 表 B-6）⇒ 登记 ≠ 修复 |

### D-1.5 清单统计（AC-01 的判据基准）

| 指标 | 值 |
|---|---:|
| 类型数 | **17**（类别① 14 + 类别② 3） |
| 键×类型对 | **41**（32 + 9）+ `routing` 1 = **42** |
| 其中配置零命中但被登记的键 | 1（`CacheTLM.level`） |

> ⚠️ **数字会随 `configs/` 演化** ⇒ 实施期若发现新增键，**回填本表**并在 tasks 记一条;
> AC-01 的判据 = 「重跑盘点脚本 ⇒ 类型数 / 键对数与本表一致（± 本 change 主动新增的键）」。
>
> ⚠️ **本表的 17 / 42 是「零效果族」口径，不是全部配置键** —— 全量键盘点与实测差异见
> **D-1.6.2 / D-1.6.3**（脚本 `scripts/test/param_effect_inventory.py`）；两者不可混读。

## D-2 — 逐键三态裁决

每个键**必须**落进且仅落进一个态;⛔ 不允许「待定」留在表里。

| 态 | 判据 | 必须交付 | 禁止 |
|:--:|---|---|---|
| **① 接真实消费** | 该键的语义**可被实现**且实现后行为可观测 | ① 实现消费路径（写入成员并被运行时读取）;② 一条**行为判别测试**（两个不同合法值 ⇒ 可观测指标不同 —— `ADR-X.17` D-6）;③ 台账表 B 该行状态改「已消费」 | 只写成员不被读取（= 换个地方存零效果） |
| **② 生成方停止产出** | 该键**不应存在**（历史遗留 / 语义已废弃 / 键名与实现重复） | ① 删掉**产出**该键的代码路径（生成方）;② 配置里写该键 ⇒ **装配失败**并报出该键名（不得静默忽略）;③ `configs/` 中该键的存量写法**逐处登记**并在 task 中列出 | ⛔ 静默忽略（等价于把缺陷永久化）;⛔ 擅自删 `configs/param_rules/**`（那是 Python 生成期校验的活数据源，`ADR-X.10` §F 未裁决） |
| **③ 显式拒绝（伪参数）** | 该键的**合法值唯一**（如 `routing` 只有 `"XY"`）⇒ 它的价值是「挡住拼错」而非「选行为」 | ① 读入并校验;② 非法值 ⇒ 装配失败且错误含**键名 + 合法值集合**;③ 台账表 B 该行状态改「已显式拒绝」 | ⛔ 用 `ignore_param` 长期豁免（`ADR-X.17` D-3b 明禁） |

### D-2.1 态 ① 的「可观测指标」选型（避免造观测面）

判别测试必须用**已存在的**运行时观测量（统计量 / 只读访问器 / 输出计数）;
⛔ 不得为通过判别而**新增**一个只为测试存在的观测面（那等于把零效果换个地方存）。

| 键 | 可用观测量（候选，实施期核实） |
|---|---|
| `TrafficGenTLM.{pattern,num_requests,start_addr,end_addr,hotspot_addrs,hotspot_weights,stride}` | 生成器发出的 flit 序列 / `num_requests` / 地址分布 ⇒ 「两个不同合法值 ⇒ 发出序列不同」 |
| `MemoryTLM.{read_latency,write_latency}` | 读写响应延迟 / 延迟统计 |
| `MemoryTLM.{size,type,mem_type}` | ⚠️ 与 `capacity_gb` **语义重叠**（`size` 与 `capacity_gb` 同时存在）⇒ 倾向态 ②（停产出 + 明确报错）而非态 ① |
| `GmmuTLM.page_size_bytes` | GMMU 翻译的页号 / 页内偏移（改页大小 ⇒ 同一地址的翻译结果不同） |
| `CPUTLM.{pattern,request_interval,start_addr}` | CPU 发出的请求序列 / 间隔 |
| `CacheTLM.{size,level,associativity,protocol,assoc,line_size}` | ⚠️ 该类**无容量/组相联模型**（只有 `cache_lines_` 与写死延迟）⇒ 态 ① 需要**先建模**（工作量大）⇒ 倾向态 ② 或**推迟到独立 change**（决策点见 tasks T1.2 的裁决记录） |
| 其余零读取面类型的键 | 实施期逐键核实（部分类型的键在当前实现里**没有对应概念**，如 `SocInternalNoc.vc_counts_enabled`（该类已 archived）⇒ 态 ②） |

### D-2.2 态 ② 的「存量配置登记」要求

态 ② 会让**写了该键的存量拓扑从装得上变为装不上**。⇒ 每个态 ② 键必须：

1. 列出**全仓写了该键的配置清单**（`grep -rn '"<key>"' configs/`）;
2. 逐处判定：改键名 / 删该键 / 保留并接受失败（**保留并接受失败须在台账显式登记**）;
3. 在 tasks 中列出该清单。

## D-3 — 落地顺序（批次）

| 批 | 内容 | 理由 |
|:--:|---|---|
| **B1** | `ADR-X.17` F-14 的 5 类（`CacheTLM` / `TrafficGenTLM` / `GmmuTLM` / `LwcBusHub` / `MemoryTLM`）+ 扩展键（`mem_type` / `hotspot_*` / `stride` / `level` / `assoc` / `line_size`） | 既有登记，缺陷面已知 |
| **B2** | 类别 ① 家族（14 类 / 32 键） | 规模最大 ⇒ 逐类一批;⚠️ 其中 `CacheTLM` / `TCCTLM` 等**无对应概念**的类型优先走态 ② |
| **B3** | `RouterTLM.routing`（态 ② 或 ③） | 与 `GpuNoC` 的内部配置生成耦合 ⇒ 单独一批，避免与 B2 交织 |

> ⚠️ **B3 与 `cpptlm-module-param-migrate-batch2` 的先后**：`routing` 若走态 ③（显式拒绝），`RouterTLM` 已是已准入类型 ⇒ 该键的拒绝路径必须在**迁移批次 2 扩大准入**之前落地，否则会出现「已准入类型 + 未裁决键」的窗口期。⇒ **B3 优先于迁移批次 2 的 T1**。

## D-4 — 与既有 improvement / ADR 的关系

| 既有物 | 关系 | 本 change 的处置 |
|---|---|---|
| `ADR-X.17` F-14（5 类 / 14 键） | **本 change 的子集** | 扩展为实测全量（17 类 / 42 键） |
| `cpptlm-router-vc-count-param-inert`（backlog · 「只登记不修」） | **结论沿用** | `vc_count` 需要换 3 个核心容器的类型（`std::array` → 动态容器）⇒ 不在本 change;其 json 侧条目归 `ADR-X.10` §F 裁决 |
| `cpptlm-validate-params-dead-code`（backlog） | **部分重叠** | 其 C++ 侧诉求（`validate_module_params` 死代码）由 `cpptlm-module-param-migrate-batch2` 承接;其 Python 生成期诉求归 `ADR-X.10` §F |
| `ADR-X.10` 决策 5 / 决策 7 Stage 2（`derive_expr` / 跨参数约束） | **未实施** | 不在本 change |
| `ADR-X.17` D-6（行为判别义务） | **本 change 的判据来源** | 每个态 ① 键一条判别测试 |

## D-5 — 与未知键检查的交互（为什么先做本 change）

`SimObject::enforce_param_contract` 只对**已准入类型**做未知键检查 ⇒ 类别 ①②③ 的键在类型迁移那一刻才变成「未知键」。

⇒ 若先迁移后修语义，存量拓扑（`CPUTLM` 95 实例 / `CacheTLM` 37 实例 / …）会**突然装不上**（用户没改配置却报错）⇒ **必须先给键确定效果，再迁移类型**。

⇒ **依赖顺序**：`cpptlm-module-param-effect-landing`（本 change）⇒ `cpptlm-module-param-migrate-batch2`。

> ⚠️ 例外：迁移批次 2 的 **T1a**（迁移**已有读取面**的 21 个类型）不依赖本 change ⇒ 两条线可并行。

## D-1.6 — 实施期回填（T0.1 盘点脚本 + T0.2 逐键裁决 · B1 批次完成态）

### D-1.6.1 盘点脚本（可重跑，AC-01 的取证工具）

```bash
python3 scripts/test/param_effect_inventory.py            # 人类可读逐键表
python3 scripts/test/param_effect_inventory.py --json     # 机读（供对拍）
python3 scripts/test/param_effect_inventory.py --check    # 与本表统计对拍,不一致 exit 1
```

判定口径（脚本机械部分，**不含**「值是否真的进入行为」—— 那一步见 W-1）：

| 判定 | 含义 |
|---|---|
| `read-surface` | 类体里有 `on_config_loaded` / `set_config` 覆写 ⇒ 有自有读取面（是否消费该键**不可机械判定**） |
| `zero-ref` | 无自有读取面 **且** 键名字面量在 `include/` + `src/` 中零出现 ⇒ **类别 ① 强候选** |
| `external-ref` | 无自有读取面但键名在**本类型之外**被引用（板级直读 / 父模块转发 / 校验器）⇒ 须人工核实 |
| `undefined-type` | 配置引用了仓内**没有类定义**的类型 ⇒ 整份配置不可装配（不是零效果键） |

> ⚠️ 两个实现细节（会静默误判，已处理）：① 读面判定必须**限定类体**（聚合头里相邻类的
> `on_config_loaded` 会让本类被误判为「有读面」—— 实测 `GpuMeshNoC` 曾因此漏判）；
> ② 前向声明（`class X;`）没有类体，遇到 `;` 先于 `{` 必须跳过（否则错抓下一个类的花括号）。

### D-1.6.2 实测（2026-11-18 · B1 落盘后）

| 指标 | 立项期 D-1.5 | B1 落盘后实测 |
|---|---:|---:|
| 配置里出现的类型数 | 17 | **36** |
| 键 × 类型对数 | 42 | **107** |
| 类别 ① 强候选（`zero-ref`） | 14 类 / 32 对 | **12 类 / 22 对** |
| 无自有读面但键被他处引用 | 未区分 | 17 对（板级直读 / 父模块转发 / LINT 校验） |
| 类型未定义 | 未区分 | **2**（`ComputeUnitTLM` · `TCCTLM`） |

> 立项期的 17 / 42 是**零效果族**的口径，不是全部配置键 —— 本表把它与「全量键盘点」分开，
> 避免两个数被读成同一件事。

### D-1.6.3 与立项期 D-1.2 的差异（逐条，无静默重算）

| # | 差异 | 原因 | 影响 |
|:--:|---|---|---|
| 1 | `ComputeUnitTLM`(2 键) · `TCCTLM`(2 键) 从类别 ① **剔除** | 仓内无类定义（`grep -rn "class ComputeUnitTLM\|class TCCTLM" include/ src/` = 0 命中）;`configs/apu_soc_full.json:3` 自述「not yet registered (planned Phase 7.B/C/D)」 | 不是零效果键,而是**配置引用不存在类型** ⇒ 归「未定义类型」,不计入本 change 的键盘点;受影响配置本身**不可装配**（新增登记 R-5） |
| 2 | `LwcBusHub.max_ip_ports` 移出类别 ① | B1 已落地（T1.4）| 已消费 |
| 3 | **新增** `CoherentXBarTLM`(3 键) · `GpuMeshNoC`(2 键) · `VramControllerTLM.operating_frequency_hz`(1 键) · `MemoryCluster.memory_type`(1 键) | 立项期靠人工读配置遗漏 | 归 B2 批次（`MemoryCluster` 属 SimModule 家族;`CoherentXBarTLM.ports` 仅被 `module_factory_validate.cc:271` 读去做**越界告警**,值从不生效 ⇒ 伪参数候选） |
| 4 | `CacheTLM.level` 不在脚本输出中 | `configs/` 里无该键,它由 `src/tlm/cluster/cache_cluster.cc` 在**运行时**生成 | 仍按 `ADR-X.17` F-14 登记（脚本只能看见配置文件里的键） |

### D-1.6.4 B1 批次逐键裁决结果（T1.1~T1.5）

| 类型 | 键 | 态 | 落地位置 | 判别测试 |
|---|:--:|:--:|---|---|
| `TrafficGenTLM` | `pattern` | ① | `traffic_gen_tlm.hh` `apply_pattern()`（7 个模式 → `GenMode_TLM` + `StressPattern`） | AC-03 pattern |
| | `num_requests` | ① | `on_config_loaded` ③（负数 ⇒ 失败） | AC-03 num_requests |
| | `start_addr` / `end_addr` | ① | `read_addr_key()` + `set_addr_range()`（`addr_dist_` 构造期绑定边界 ⇒ 必须走 setter） | AC-03 start/end |
| | `hotspot_addrs` / `hotspot_weights` | ① | `parse_hotspot()` + `set_hotspot_config()` | AC-03 hotspot |
| | `stride` | ① | `on_config_loaded` ⑥ + `set_stride()` | AC-03 stride |
| | `seed` | ①（原有） | `set_seed()`（同时重播种 `rng_`） | AC-03 seed |
| `MemoryTLM` | `capacity_gb` | ①（原有） | `set_size_bytes()` | — |
| | `size` | ① | `parse_size_bytes()`（B/KB/MB/GB，1024 进制） | AC-03 size |
| | `read_latency` | ① | hit **与** miss 同值（`read_latency_hit_cycles_` + `_miss_`）;细粒度拆分归板级 | AC-03 read_latency |
| | `write_latency` | ① | `write_latency_cycles_`（functional legacy 采样从写死 120/100 改为读成员） | AC-03 write_latency |
| | `type` / `mem_type` | ② | `reject_unsupported_key()` ⇒ 装配期失败含键名 | AC-04 memory |
| `GmmuTLM` | `page_size_bytes` | ① | `set_page_size_bytes()`（成员 + 掩码 + 移位，替换 `constexpr 4096`）;2 的幂且 ≥ 4096,否则失败 | AC-03 gmmu |
| | `tlb_size` / `tlb_miss_latency_cycles` | 表 A | `ignore_param()` 登记（消费方是**板级** `dgpu_board_shell.cc:1056-1057`） | 台账表 A |
| `LwcBusHub` | `max_ip_ports` | ① | 成员 `max_ip_ports_`（默认 = 编译期 `kMaxIpPorts = 32`），作用于两个 register 上限判断 | AC-03 lwc |
| `CacheTLM` | `size` · `level` · `associativity` · `assoc` · `line_size` · `protocol` | **待裁决** | ⛔ 不在本 change 范围 —— 该类是**冻结面**（`ADR-CEP-15` IC1：`cache_tlm.hh` / `cache_tlm.cc` 相对 CEP 基线零 diff，闸 = `test/test_cep_module_factory_seam.cc:133-134`）⇒ 解冻须 architect 裁决（R-4） | — |

**机制新增**：`load_param_opt(key, target[, lo, hi])`（`include/core/sim_object.hh`）——
键存在 ⇒ 校验赋值返回 `true`;缺失 ⇒ **保持成员当前值**返回 `false`。
存在理由：`load_param` 的「无默认值 ⇒ 必填」语义不适用于「默认值 = 成员初值」的键
（否则每个可选键都要在调用点重抄一遍初值，抄漏即静默改默认行为）。

**B1 的键间一致性裁决**（⛔ 不一致即显式失败 —— 零效果键不得换个形式静默失效）：

| 规则 | 失败键 |
|---|---|
| `hotspot_addrs`/`hotspot_weights` 出现但 `pattern != HOTSPOT` | `hotspot_addrs` |
| `pattern == HOTSPOT` 但无 `hotspot_addrs`（空离散分布 ⇒ UB） | `hotspot_addrs` |
| `hotspot_weights.size() != hotspot_addrs.size()` | `hotspot_weights` |
| `stride` 出现但 `pattern != STRIDED` | `stride` |
| `end_addr <= start_addr`（含只给一端时与默认另一端比较） | `end_addr` |
| `size` 与 `capacity_gb` 同时出现（同一量的两个真值） | `size` |
