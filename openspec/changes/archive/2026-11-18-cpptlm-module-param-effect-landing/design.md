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

### D-1.2 类别 ① —— 零配置读取面（**11 类 / 24 键×类型对**）

⚠️ **本次新发现** —— `ADR-X.17` F-14 只登记了「有读取面但键未消费」与「声明了但成员不存在」，**未覆盖**「类型根本没有配置读取面」这一最大的一类。

| 类型 | 配置实例数 | 键数 | 键（并集） | 证据（读取面） |
|---|---:|---:|---|---|
| `CPUTLM` | 95 | 3 | `pattern`, `request_interval`, `start_addr` | `include/tlm/cpu_tlm.hh` 无 `on_config_loaded` / `set_config`;成员 `start_addr_`（`:19`）/ `request_interval_`（`:21`）只被构造期与运行期使用，从不由配置赋值 |
| `CacheTLM` | 37 | 6 | `assoc`, `associativity`, `level`, `line_size`, `protocol`, `size` | `include/tlm/cache_tlm.hh` 无配置读取面;内部只有 `cache_lines_`（`:42`），命中延迟是写死的 `hit ? 5 : 50`（`:89`）;⚠️ `level` **配置零命中**（仅被 `ADR-X.17` F-14 登记 ⇒ 仍须裁决）;✅ **已裁决并落地**（2026-11-18,`level` ⇒ 命中延迟,见 D-1.6.4） |
| `CrossbarTLM` | 11 | 1 | `port_count` | `include/tlm/crossbar_tlm.hh` 无配置读取面 |
| `ComputeUnitTLM` | 4 | 2 | `kernel_id`, `workgroup_size` | 无配置读取面 |
| `PcieControllerIP` | 3 | 3 | `bar_bases`, `bar_sizes`, `link_speed_gts` | 无配置读取面;⚠️ `bar_sizes` 在**板级**另有消费方（`dgpu_board_shell.cc:78-79` 读 `pcie_ep` 的 `bar_sizes`）⇒ 该键的「板级消费」需与本类型自身的键面分开处置（`ADR-X.17` D-3 三态条款） |
| `PcieMemoryDevice` | 2 | 1 | `capacity_gb` | 无配置读取面（`PcieMemoryDevice` 退化为 PCIe 外观层，backing 由 `DGpuBoard` 注入 —— per `ADR-DGPU-05`） |
| `StreamingMultiprocessorTLM` | 2 | 2 | `execution_latency`, `warp_size` | 无配置读取面 |
| `ArbiterTLM` | 1 | 1 | `n_ports` | 无配置读取面 |
| `LwcBusHub` | 1 | 1 | `max_ip_ports` | 无配置读取面（`ADR-X.17` F-14 已登记） |
| `SocInternalNoc` | 1 | 2 | `data_flow`, `vc_counts_enabled` | 无配置读取面（该类已 archived —— per `openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-e1/`）⇒ **态 ② 的首选候选** |
| `TCCTLM` | 1 | 2 | `coalescing`, `size` | 无配置读取面 |
| **小计** | — | **24** | — | **11 类** |

> ⚠️ **2026-11-18 删三行**（类型归档，per change `cpptlm-config-type-registration-align` T7.1~T7.3 · 台账 Entry #481）：
> `GPUTLM`(4 键) · `MemoryClusterTLM`(2 键) · `SharedMemoryTLM`(2 键) —— 三者的 `.hh` 已**物理删除**、对应 3 份配置（`apu_soc_phase7a.json` / `gpu_standalone.json` / `templates/gpu_soc/gpu_soc_gb203_v1.json`）亦已删 ⇒ **不再出现在盘点脚本输出中** ⇒ D-1 表须同步删行（**14→11 类 / 32→24 键对**）。
> ⚠️ **该豁免仅限这三行**（逐行注明），⛔ **不得**泛化为「已归档类型一律不计」—— per `proposal.md` AC-01 的增补条款。

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
| 类型数 | **14**（类别① 11 + 类别② 3） |
| 键×类型对 | **33**（24 + 9）+ `routing` 1 = **34** |
| 其中配置零命中但被登记的键 | 1（`CacheTLM.level`） |

> ⚠️ **数字会随 `configs/` 演化** ⇒ 实施期若发现新增键，**回填本表**并在 tasks 记一条;
> AC-01 的判据 = 「重跑盘点脚本 ⇒ 类型数 / 键对数与本表一致（± 本 change 主动新增的键）」。
>
> ⚠️ **本表的 14 / 34 是「零效果族」口径，不是全部配置键** —— 全量键盘点与实测差异见
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
| `CacheTLM.{size,level,associativity,protocol,assoc,line_size}` | ✅ **已裁决**（2026-11-18）—— 容量模型已在 `cache_tlm.hh` 内补齐（解冻经用户裁定 + `ADR-CEP-15` SU）⇒ `size` / `line_size` / `associativity` / `assoc` / `level` 取 **态 ①**（观测量 = `stats` 的 hits / misses / latency 分布）;`protocol` 取 **态 ②**（协议语义不入本类） |
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
| `ADR-X.17` F-14（5 类 / 14 键） | **本 change 的子集** | 扩展为实测全量（**14 类 / 34 键**） |
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
| 类型未定义（配置引用**未注册**类型） | 未区分 | **4**（`TmuDispatchProcessorTLM` · `ArbiterTLM` · `ComputeUnitTLM` · `TCCTLM`）/ 3 份配置 —— ⚠️ 见 **R-5** |

> 立项期的 17 / 42 是**零效果族**的口径，不是全部配置键 —— 本表把它与「全量键盘点」分开，
> 避免两个数被读成同一件事。
>
> ⚠️ **2026-11-18 类型归档后的同步**：D-1.2 / D-1.5 的零效果族口径已由 **17 类 / 42 对** 收窄为 **14 类 / 34 对**
> （删 `GPUTLM` 4 + `MemoryClusterTLM` 2 + `SharedMemoryTLM` 2）;本表「B1 落盘后实测」列是**时点快照** ⇒
> 保留原值不改，⛔ 不得与 D-1.5 的当前值混读。

### D-1.6.3 与立项期 D-1.2 的差异（逐条，无静默重算）

| # | 差异 | 原因 | 影响 |
|:--:|---|---|---|
| 1 | `ComputeUnitTLM`(2 键) · `TCCTLM`(2 键) 从类别 ① **剔除** | 仓内无类定义（`grep -rn "class ComputeUnitTLM\|class TCCTLM" include/ src/` = 0 命中）;`configs/apu_soc_full.json:3` 自述「not yet registered (planned Phase 7.B/C/D)」 | 不是零效果键,而是**配置引用未注册类型** ⇒ 不计入本 change 的键盘点;⚠️ **实测推翻「整份配置不可装配」的旧登记** —— 真实形态是「**装配成功但该模块被静默丢弃**」（`module_factory.cc` 约 385 行只打 `DPRINTF` 后继续;`DPRINTF` 仅 Debug 生效 ⇒ Release 下**零输出**）⇒ 拓扑缺模块 ⇒ **仿真结果静默失真**（比「装不上」更隐蔽）;完整清单（**4 类型 / 3 配置**，含 `TmuDispatchProcessorTLM` 与 `ArbiterTLM`）见 **R-5** |
| 2 | `LwcBusHub.max_ip_ports` 移出类别 ① | B1 已落地（T1.4）| 已消费 |
| 3 | **新增** `CoherentXBarTLM`(3 键) · `GpuMeshNoC`(2 键) · `VramControllerTLM.operating_frequency_hz`(1 键) · `MemoryCluster.memory_type`(1 键) | 立项期靠人工读配置遗漏 | 归 B2 批次（`MemoryCluster` 属 SimModule 家族;`CoherentXBarTLM.ports` 仅被 `module_factory_validate.cc:271` 读去做**越界告警**,值从不生效 ⇒ 伪参数候选） |
| 4 | `CacheTLM.level` 不在脚本输出中 | `configs/` 里无该键,它由 `src/tlm/cluster/cache_cluster.cc` 在**运行时**生成 | 仍按 `ADR-X.17` F-14 登记（脚本只能看见配置文件里的键）;**已落地**（`level` ⇒ 命中延迟,见 D-1.6.4） |

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
| `CacheTLM` | `size` · `line_size` · `associativity` · `assoc` · `level` | ① | ✅ **已解冻并落地** —— 该类原为 CEP 冻结面（`ADR-CEP-15` IC1），经**用户裁定解冻**（`ADR-CEP-15` Status Update 2026-11-18 决策 2 改判 + 台账 Entry #478）⇒ 容量模型：`size` ⇒ 槽数 = `size / line_size`，组数 = `max(1, 槽数 / associativity)`，组满 ⇒ 组内 LRU 逐出;`level` ⇒ 命中延迟（1 → 5 / 2 → 10 / ≥3 → 20 cycle）;`associativity` 与 `assoc` 是**同义别名**（同现且值不同 ⇒ 失败） | AC-03 cache（size / associativity / line_size / level） |
| `CacheTLM` | `protocol` | ② | `reject_unsupported_key("protocol")` ⇒ 装配期失败含键名 —— **协议语义不入本类**（`ADR-CEP-15` 决策 5 + IC3）⇒ 判据载体 = `test/test_cep_module_factory_seam.cc` 的 IC1 **行为**判据 | AC-04 cache |

### D-1.6.5 B2 批次（第一批：6 个类型 / 9 个键）—— 全部 **态②**（显式拒绝）

| 类型 | 键 | 态 | 为什么是态②（不可实现） | 落地位置 |
|---|:--:|:--:|---|---|
| `CrossbarTLM` | `port_count` · `ports` | ② | 端口数是**编译期常量** `NUM_PORTS`（`crossbar_tlm.hh` 的 `static constexpr`） | `CrossbarTLM::on_config_loaded` |
| `CoherentXBarTLM` | `ports` | ② | 继承 `CrossbarTLM`（同上游端口数常量） | 同上（继承） |
| `ArbiterTLM` | `n_ports` · `ports` | ② | 端口数由**模板参数** `N_PORTS` 决定（注册名 `ArbiterTLM2/4` 即其规格） | `ArbiterTLM::on_config_loaded` |
| `SocInternalNoc` | `data_flow` · `vc_counts_enabled` | ② | 该类已随 `cpptlm-dgpu-soc-internal-noc-e1` **archived**;NoC 语义由 `GpuInternalFabric` 承担 | `SocInternalNoc::on_config_loaded` |
| `VramControllerTLM` | `operating_frequency_hz` | ② | 频率是**编译期锚点** `kOperatingFrequencyHz`（与 `dgpu-realip-pin-contract` D.4 + `timing-mode.md §2.4` 双向绑定）;运行时可配需「GB/s 带宽输入」这个**尚不存在**的配置面来闭合语义链 ⇒ 实现它 = 新功能（D3 seam 正式落地） | `VramControllerTLM::on_config_loaded` |
| `MemoryCluster` | `memory_type` | ② | 本仓**无内存类型模型**（无 DRAM/HBM 行为差异） | `MemoryCluster::set_config` + 删除 `memory_type_` 成员 |
| `PcieMemoryDevice` | `capacity_gb` | ② | 容量由**注入的 backing** 决定（`backing_view_size_`;ADR-DGPU-05 单一 VRAM 真源 + D-AXI v1.4 B8）⇒ 配置值不可能有效果（该类**零配置读取面**） | `PcieMemoryDevice::on_config_loaded`（态②拒绝）;2 份配置已清理 |

**连带处置**（⛔ 不只是加拒绝逻辑）：

| # | 处置 | 理由 |
|:--:|---|---|
| 1 | **LINT007 告警段退役**（`src/core/module_factory_validate.cc` 的 `kMaxPortsByType` + 越界告警） | 它只对 `{CrossbarTLM, CoherentXBarTLM, ArbiterTLM, ArbiterTLM2, ArbiterTLM4}` 的 `params.ports` 做**告警**;所有这 5 个类型现在都**拒绝**该键 ⇒ 告警路径永不触发 ⇒ 留着就是死代码 |
| 2 | **`test/test_lint007_ports_validation.cc` 判据改判**（5 个 case：`ports` 越界 / 不足 / 恰好相等 ⇒ 全部断言**装配失败**;无 `ports` ⇒ 成功） | ⛔ 不得删测试换绿灯;`params.ports == max_ports` 也**必须失败**（态② 是「键存在即无效」，与值无关） |
| 3 | **`src/tlm/cluster/memory_cluster.cc` 不再生成 `ports`** | 它给 `ArbiterTLM2/4` 子模块生成 `{"ports": channel_count_}` ⇒ 现在会让**内部装配失败**（实测：内部实例数 0） |
| 4 | **存量配置清理 5 处**：`dgpu_soc_minimal_v1.json`（`data_flow` + `vc_counts_enabled` + `operating_frequency_hz`）· `apu_soc_v1.json`（`ports`）· `example_emitter_soc.json`（`port_count`）· `library_memory_demo.json`（`n_ports` + `memory_type`） | 态② 会让「写了该键」的配置**装不上** ⇒ 必须同步清理 |
| 5 | **2 个测试数据里的 `memory_type` 清理**（`test_p2_residual.cc` / `test_simmodule_infra_clusters.cc`） | 它们传的是「从未透传成功」的键 ⇒ 现在会抛 |

### D-1.6.6 B2 批次（A2a/A2b/A2c）—— 7 键态② + 2 键态① + 3 键（态③+态①×2）

| 类型 | 键 | 态 | 为什么 | 落地 |
|---|---|:--:|---|---|
| `PcieControllerIP` | `bar_bases` · `bar_sizes` · `link_speed_gts` | ② | stateless facade，**有意**从 `PcieEndpointIP::bar_sizes_` 动态读（no static snapshot）⇒ 零消费面;⚠️ **同名不同语义**（板级读的是 **`pcie_ep` 节点**） | `on_config_loaded` 拒绝;3 份配置 `pcie_ctrl.params` 清空 |
| `StreamingMultiprocessorTLM` | `execution_latency` · `warp_size` | ② | 前者全仓零概念（延迟由 12 子模块决定）;后者**不是**该类成员（只在 `GpuTopologyParams`） | `on_config_loaded` 拒绝;配置侧零命中 |
| `CoherentXBarTLM` | `domain_protocol` · `snoop_filter` | ② | 无协议状态机（`snoop_broadcast()` 无条件 write-through）;无 filter（ArchForge `snoop_filter.md` 标「🟡 规划中」） | `on_config_loaded` 拒绝 + **调基类**（否则继承的 `ports` 拒绝失效） |
| `GpuMeshNoC` | `dim` · `hops_latency` | **①** | 两成员**真被** `route_latency()` / `d2d_forward_payload()` 读 ⇒ 可接真实消费 | `load_param_opt`;判别测试用既有 `last_transfer_latency_cycles()` |
| `CPUTLM` | `pattern` | **③** | 合法值**唯一**（`SEQUENTIAL`）—— 本类只有线性扫描，无多模式生成 | 新增 `on_config_loaded` + 校验 + 非法值失败;⛔ 不做模式实现 |
| `CPUTLM` | `start_addr` · `request_interval` | **①** | 两成员**真被**运行期读（`:87` 窗口 · `:90` timer 取模） | `load_param_opt` + ⚠️ **同步 `cur_addr_`**（否则首请求仍用旧默认地址） |

> **连带（生成器漂移，oracle 断裂 A 的实证）**：重跑生成器后发现它仍在 emit 已被态② 的键 —— `CacheTLM.protocol` · `MemoryTLM.read_latency`/`write_latency`/`type` · `CoherentXBarTLM.snoop_filter`/`domain_protocol` ⇒ **`apu_soc_full.json` 装配失败**（`test_param_alignment:129`）⇒ 生成器已停发这些键 + 重跑。这正是 R-5 的 **AC-05** 会爆炸的同一根因。

> **剩余 B2（未做）—— 12 键**：`CoherentXBarTLM.domain_protocol` / `snoop_filter`(2) · `PcieControllerIP`(3) · `StreamingMultiprocessorTLM`(2) · `GpuMeshNoC`(2) · `CPUTLM`(3)。
>
> ✅ **已追加处置**：`PcieMemoryDevice.capacity_gb`（**态②**：容量由注入 backing 决定 ⇒ 配置值不可能有效果）—— 2026-11-18 落地。
>
> ⚠️ **2026-11-18 范围缩减（21 → 13 → 12 键）**：`GPUTLM`(4) · `MemoryClusterTLM`(2) · `SharedMemoryTLM`(2) 共 **8 键**已随**类型归档**一并消失;`PcieMemoryDevice.capacity_gb`(1) 另于同日落态② ⇒ 剩余 **12 键** ——
> 用户裁定「ArchForge 有计划做的设计 IP ⇒ CppTLM 只保留影响框架层的示例，多余示例归档」，
> 由 change `cpptlm-config-type-registration-align` 的 **T7** 承接（其 design **D-8** 含逐类型证据与完整影响面）。
> ⇒ 这 8 键**不需要**在本 change 里逐个裁决（类型已不存在）。

> **解冻连带面**（`ADR-CEP-15` Status Update 2026-11-18 §⑤ 逐条）：守门闸（0 diff 断言 → `protocol` 拒绝的**行为**判据）· `ADR-CEP-07` 决策 6 表行指针 · `openspec/specs/cep-core-contracts/spec.md` 的 IC1 场景改判（**场景名沿用历史命名** —— openspec 的 `MODIFIED` 按场景名精确匹配且 archive 拒绝丢弃既有场景名）· `docs/architecture/cep/fabric-transport.md` 冻结面声明限定 · `docs/architecture/cep/parity-baseline.md` 的容量口径限定 · 对拍常量抽取器 `scripts/cep/parity/cpptlm_params.py` 形态同步（旧字面量 `hit ? 5 : 50` → 成员初值）。

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
