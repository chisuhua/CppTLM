# ADR-X.17: 模块参数的「读取即声明」机制（内联加载 + 消费即登记，不使用宏）

## Status

Proposed — 2026-11-17（**双审完成**：oracle 2 轮 + critic 2 轮。critic 第 2 轮的唯一 BLOCKER（M4 的 NICTLM CFG-08 空窗）已按其可套用措辞闭合并自查;⚠️ **critic 复审预算（每 change ≤ 2 轮）已用尽 ⇒ 未做第 3 轮确认** ⇒ 状态保持 Proposed，待实施 change 立项时由 planner / verifier 复核。审稿报告见 `.rddf/reviews/ADR-X.17.md`）

## Metadata

- **Author**: architect（build agent · architect mode）
- **Parent**: **部分取代** `ADR-X.10-parameter-framework` 决策 1 的**「规则存放位置」**维度（其「序列化载体」维度不变）
- **闭合范围（逐条 · 不得泛化）** —— 本 ADR **部分闭合** `ADR-X.10` 决策 4.5 / 决策 7。⚠️ **「闭合」= 该子项的**决策**已作出（≠ 能力已存在）**;能力落地范围 = **已迁移类型**，进度见 M0~M8：

  | `ADR-X.10` 条款 | 闭合状态 |
  |---|---|
  | 决策 7 Stage 1（必填存在 / 类型正确 / 默认值填充） | ✅ **决策已作出 · 能力仅覆盖已迁移模块** |
  | 决策 7 Stage 2 的 **min/max 子项** | ✅ **决策已作出 · 能力仅覆盖已迁移模块** |
  | 决策 4.6 的 ①**校验位置** | ✅ **位置已就位**（`set_config` 调用链内 ⇒ 抛异常 → `module_factory.cc:416-423` catch ⇒ `return false`;**全体类型**）· ②**校验依据** = 模块内声明 = **仅已迁移模块** |
  | 决策 4.5 的「范围」子项 | ⚠️ **决策已作出 · 能力仅覆盖已迁移模块**;「参数组合不合法」（跨参数约束，如 `node_x < mesh_x`）**未闭合** |
  | 决策 7 Stage 2 的 `derive_expr` 评估与参数依赖关系 · 决策 5 · 决策 6 · 决策 4.6 的 ②异常类型与 ③回滚 | ❌ **未闭合** |

  ⇒ 未闭合项 SHALL 逐条登记为**具名 owner change**（不得以「另案」泛化）;其中 2 项**已有**具名 carrier（`ADR-X.10` §G：`.rddf/improvements/cpptlm-validate-params-dead-code.md`（决策 4.5 死代码）· `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`（`vc_count`））⇒ 直接引用，避免重复开单。
  ⇒ **M0（先于 M1）**：在 `ADR-X.10` 的 `## Status Update` 段**末尾**追加一个指向本 ADR 的子段（append-only，**不改 §A~§G 既有内容**）+ ❌ 行的**未闭合项各自**有 owner 链接（共 4 个新单）;⚠️ 上述 2 个既有 carrier 对应的是 **⚠️ 行**（决策 4.5 的范围子项 ↔ `cpptlm-validate-params-dead-code`;`vc_count` ↔ `cpptlm-router-vc-count-param-inert`），**不在** ❌ 行内，M0 不得因此少开单。
- **同族但形态不同**: `ADR-X.9-port-type-system`（端口侧 · C++ struct + JSON 宏 · 已实施）—— 本 ADR 在其**不适用域**显式偏离
- **无关**: `ADR-X.12-python-config-generator`（📝 提案态 · 未实施）
- **不在范围**: `configs/param_rules/*.json` 的存废（`ADR-X.10` §F 已登记为需独立 ADR 裁决）;非标量参数（数组 / 嵌套对象 / 字符串枚举）;配置文件的顶层结构
- **关联 change**: `backlog: cpptlm-module-param-read-declares`（待 planner 立项;立项后回填 AC 编号）
- **AC 映射**: Verification 章 V-1 ~ V-7（**执行载体为 spec + 闸**，per `ADR-X.10` §G），立项后回填 change AC 编号
- **审稿报告**: 待双审后落 `.rddf/reviews/ADR-X.17.md`

---

## Context（背景与问题）

### 事实基础（全部读码实测，非推断）

| # | 事实 | 位置 |
|:--:|---|---|
| **F-1** | 全仓参数读取面 = **20 处 / 2 类挂载点 + 1 条板级直读路径（F-15）**：① `on_config_loaded` 覆写 **11 处**（`cohif_nic.hh:174`、`cohif_responder.hh:110`、`gmmu_tlm.hh:305`、`memory_tlm.hh:162`、`nic_tlm.hh:111`、`pcie_endpoint_ip.hh:116`、`router_tlm.hh:205`、`traffic_gen_tlm.hh:106`、`vram_controller_tlm.hh:148`、`sdma_engine_tlm.hh:177`、`pcie_endpoint_tlm.h:99`）② **SimModule 家族在 `set_config` 覆写内读参数**（**9 个类型 / 26 处 `params.contains`**：`apu_soc` / `cache_cluster` / `compute_cluster` / `cpu_cluster` / `gpc_cluster` / `gpu_cluster` / `gpu_noc_cluster` / `memory_cluster` / `tpc_cluster`） | 同左 |
| **F-2** | 参数形态**含非标量**：对象数组 `address_regions`、嵌套块 `axi_adapter` / `link_layer`、字符串枚举 `target_type` | `nic_tlm.cc:38-44`、`pcie_endpoint_ip.cc:189-207` |
| **F-3** | `credit_timeout_` 为 `uint64_t`、默认 0、**无范围**，以 `get<uint64_t>()` 读取（拒绝负数） | `router_tlm.hh:332`、`router_tlm.cc:80-82` |
| **F-4** | 参数声明散在**四处**：① `configs/param_rules/*.json`（C++ 侧零加载，仅被已 Deprecated 的 `cpptlm_config/validator.py` 消费）② C++ `get_param_rules()`（`router_tlm.cc:584-611`;`type`/`min`/`max` 面**死**）③ 模块内**第三遍**手写范围（`router_tlm.cc:98-99` 的 1..64）④ **CFG-08 的 per-type 硬编码键表**（F-16）—— **强制力分布**：② 对 RouterTLM 的 `required`（+ `default_int`）**有**强制力（`router_tlm.cc:586-597` 四条 required → `module_factory.cc:276-283` 缺失即 `return false`，**仅当 `params` 存在**;legacy 扁平形式绕过）· ④ 对 **NICTLM 的 `node_id`** 与两类型的**整数类型**有强制力 ⇒ **必填强制点有两个（② 与 ④），退役须一并处置** | 同左 |
| **F-5** | 其校验器 `validate_module_params` 是 `static` **零调用方死代码** ⇒ type/min/max 在装配期**无保护** | `module_factory_validate.cc:474-499` |
| **F-6** | 工厂侧默认值注入**仅 `default_int`**、且**仅在 `mod.contains("params")` 时**发生;`cfg_src == nullptr` 时**根本不调** `set_config` ⇒ 无 `params` 键的模块拿不到任何默认值 | `module_factory.cc:265/284-286`、`:404-409` |
| **F-7** | `registerParamRules` **全仓唯一调用方** = RouterTLM ⇒ **15 个模块无任何参数声明** | `router_tlm.cc:614` |
| **F-8** | 未知键**静默忽略**：`params` 是裸 dict;`emitter.py:26 ALLOWED_TOP_LEVEL_KEYS` 定义了从不使用 | `cpptlm/topo/layer.py:29`、`cpptlm/topo/emitter.py:26` |
| **F-9** | 类型保护**存在但不在规则层**：`json::type_error` 继承 `std::exception` ⇒ 模块 `get<T>()` 的类型错误会被 `module_factory.cc:416-423` 的 catch 兜住 ⇒ 但**仅覆盖模块确实读取了的参数** | `external/json/nlohmann/json.hpp:4300/4469` |
| **F-10** | legacy 顶层参数回退把整个 module entry 当 `cfg`（RouterTLM 顶层 `node_x`、NICTLM 顶层 `node_id`） | `module_factory.cc:405-408` |
| **F-11** | `PcieEndpointIP::on_config_loaded` 被**二次调用**（`:168` 与 `:172`）⇒ 加载逻辑必须**幂等** | `pcie_endpoint_ip.cc:168/172` |
| **F-12** | `NUM_VCS = 4` 是编译期 `constexpr` ⇒ `vc_count`（声明默认 2 vs 实际 4）**当前不生效** | `router_tlm.hh:138`、`router_tlm.cc:601-603` |
| **F-15** | **第三条参数消费路径（板级直读 JSON `params`）**：`src/tlm/gpu/dgpu_board_shell.cc` **绕过 `set_config` / `on_config_loaded`** 直接读 JSON `params` 并灌进模块 setter ⇒ ① `:1039-1059`（timing-mode：`memory` / `vram_ctrl` / `gmmu` 的 `read_latency_hit_cycles` / `read_latency_miss_cycles` / `write_latency_cycles` / **`use_zero_delay_for_test`** / `row_hit_cycles` / `row_miss_cycles` / `bandwidth_gbps` / `tlb_miss_latency_cycles` / `tlb_size`）② `:53-59`（板级 `device_id` / `quantum_cycles`;另 `configs/dgpu_board_v1.json:4-8` 的 `ptx_emu_root`）③ `:75-90` / `:87-96`（`pcie_ep` 的 `bar_sizes` / `visible_vram_size` / `invisible_vram_size` / `va_region_size` / `gpu_id` / `gfx_version` / `bdf`）⇒ **F-1 的「2 类挂载点」不覆盖此路径** | 同左 |
| **F-16** | **第四个声明面 · CFG-08（`ADR-X.11` 决策 7 · 已签发 ✅ 已实施）**：工厂侧 `validateConfig`（调用点 `src/core/module_factory.cc:169`，实现在 `src/core/module_factory_validate.cc:301-330`）对 RouterTLM 的 `node_x`/`node_y`/`mesh_x`/`mesh_y` 与 NICTLM 的 `node_id` 做**必填 + 整数类型**检查（逐键检查在 **`:311-326`**;无 `params` 源直接失败在 **`:327-330`**）⇒ 其 per-type 硬编码键表本身即是「声明面与读取面分离」的形态。⚠️ 锚点**不取** `:478-481`（那落在 `validate_module_params` 死代码内，见 F-5） | 同左 |
| **F-13** | **双面漂移的现存实证**：`configs/param_rules/router_tlm.json` **无** `flit_width`，而 C++ `get_param_rules()` **有**（`router_tlm.cc:598-600`）**且该参数全仓零消费** | 同左 |
| **F-14** | **「配置里有键、实现零效果」这一族**（**全部实测**，逐条见位置列）：`CacheTLM.{size,level,associativity,protocol}`（`CacheTLM` **零配置读取面** —— `include/tlm/cache_tlm.hh` 无 `get_config` / 无 `on_config_loaded`;`size`/`level` 由 `src/tlm/cluster/cache_cluster.cc:25/28-29` 生成）、`TrafficGenTLM.{pattern,num_requests,start_addr,end_addr}`（`include/tlm/traffic_gen_tlm.hh:102-110` 注释自述「其余 params 在本 change 之前即未被解析」;`set_num_requests` / `set_addr_range` 全仓无生产调用方）、⚠️ `GmmuTLM.page_size_bytes`（**读而丢弃** —— `include/tlm/gpu/gmmu_tlm.hh:307-309` 的 `contains` 分支体为空;实际翻译用 `constexpr page_size = 4096`（`:230` / `:353`））、`LwcBusHub.max_ip_ports`（全仓仅 `configs/dgpu_soc_minimal_v1.json:69` 命中;`include/tlm/lwc/lwc_bus_hub.hh` 零配置读取面）、`MemoryTLM.{size,type,read_latency,write_latency}`（`include/tlm/memory_tlm.hh:162-168` 只读 `capacity_gb`;`size`/`type` 由 `src/tlm/cluster/memory_cluster.cc:28` 生成） ⇒ **共 5 类 / 14 键** | 各条位置如上 |

### 问题的本质

不是「规则写在哪个文件」，而是**声明与使用被分离**：规则在 ①②、赋值在 ③ ⇒ 任一处改动都可能让另一处失效，且**没有任何机制能发现**。

### 缺陷为什么能存活至今（最强的一族证据）

**幸存机制 = 配置值恰好等于硬编码默认值 ⇒ 零可观测差异 ⇒ 无任何测试可判。**

实证（F-14）：`MemoryTLM.write_latency`（配置 `120` ↔ 硬编码 `write_latency_cycles_ = 120`，`include/tlm/memory_tlm.hh:70`）与 `CacheTLM.size`（配置 `"32KB"` ↔ 实现里根本没有 size 成员）⇒ 改成 `60` / `"1MB"` 不产生任何差异。

⚠️ **幸存机制的适用边界（必须同时记录，否则会被误用）**：幸存机制**只对「实现根本不读该键」的参数成立**。凡是被**任一消费路径**读到的键，即使当前值恰好等于默认值，改值**仍会产生可观测差异** ⇒ **不能**拿「值与默认值相同」推出「该键无效」。

最硬的反例 = F-14 的**近邻** `GmmuTLM.{tlb_size, tlb_miss_latency_cycles}`：二者由 `src/tlm/gpu/dgpu_board_shell.cc:1056-1057` 从 JSON `params` 注入（timing-mode 路径），`tlb_miss_latency_cycles_` 直接进入 `translate_timing` 的 miss 延迟（`include/tlm/gpu/gmmu_tlm.hh:259`）、`tlb_entries_.size()` 直接作为 TLB 索引模数（`:240` / `:265`）⇒ 改成 `10` / `64` **产生**可观测差异（`tlb_miss_latency()` / `tlb_size()` 访问器 + `translate_stats_.total_walk_cycles` + hit/miss 统计）⇒ **它们不是 F-14 成员**，但它们是**第三条参数消费路径**（F-15）的实例。

⇒ 这一族**比 `buffer_size`（已由 change `cpptlm-router-buffer-size-param` 闭合）大一个量级**，且是 D-2（未知键检出）与 V-4 的真正目标面。

### 外部参照（DRAMSys 5.6.0，纯 C++ standalone）

其机制是「JSON → 手写 `Config::Xxx`（全 `std::optional`）→ 手写 domain struct（`DEFAULT_*`）→ ctor 注入」;**手写 field-by-field 搬运已出真实 bug**（`Simulator.cpp:198` 把 `config.transitions` 写成 `desc.transitions` ⇒ 用户配置的状态机迁移被静默丢弃）;**未知 key 静默忽略**;无负向测试。⇒ 本 ADR 的两条设计（**读取行即声明行**、**未知键必须暴露**）针对的正是这两点。

⚠️ 该参照的教训只证明「**搬运会错**」，**不证明「同源就不会错」** —— 同源只消除「两处不一致」，不消除「搬错一处」。

---

## Decision

### D-1 — 参数的声明与读取**同源**：每次读取即一次声明

模块在 `on_config_loaded` **或 `set_config` 覆写**内以**一次调用**完成「声明 + 校验 + 赋值」（F-1 的两类模块挂载点在覆盖内）。
⚠️ **覆盖域**：本决策覆盖**模块自身读取**的参数;板级直读 JSON `params` 的路径（F-15）**不由模块的 `load_param` 覆盖** ⇒ 其参数的归属见 D-3 的板级条款。

> **伪代码 · 不表达签名 · 槽位顺序由实施 change 的 design.md 定稿**
> ```cpp
> load_param(/*cfg*/ cfg, /*key*/ "buffer_size",
>            /*target*/ buffer_depth_,
>            /*domain*/ ValueDomain::UInt64, /*range*/ {1, 64}, /*default*/ 8);
> load_param(/*cfg*/ cfg, /*key*/ "node_x",
>            /*target*/ node_x_,
>            /*domain*/ ValueDomain::UInt64, /*range*/ {1, 16}, /*default*/ NONE /* 必填 */);
> load_param(/*cfg*/ cfg, /*key*/ "credit_timeout",
>            /*target*/ credit_timeout_,
>            /*domain*/ ValueDomain::UInt64, /*range*/ NONE,     /*default*/ 0);
> ```

一次调用携带**全部**信息：键名 · 目标成员 · **值域类型** · 范围（可省）· 默认值（可省）。**必填由「默认值缺省（`NONE`）」表达**（⛔ 不另设标志位 —— 标志位与默认值两个来源会产生「有默认但不必填」的第三态）。

⇒ **不存在与读取分离的声明面** ⇒ 「**声明面与读取面分离**」这一形态的缺陷（同一参数需在两处各写一遍）被消除。

⚠️ **本决策不主张「声明了却无效」已被结构性消除**：参数被声明、其值却**不影响运行时行为**这一族缺陷仍然可能（例：`vc_count` 被声明而 `NUM_VCS` 仍是编译期常量;或 F-14 的「配置里有键、实现零效果（不读，或读了但丢弃）」）⇒ 该族由 D-6 承担（**纪律，非机制**）。

### D-2 — 消费即登记 ⇒ 未知键可被检出

每次 `load_param` 把键名登记进**本实例**的「已消费键」集合;模块在加载末尾可查询「`params` 中存在但未被消费的键」⇒ 即**未知键**。拼错的参数名不再伪装成「用了默认值」，而是**显式失败**。

**准入判据（不是「是否含非标量」，而是「存量配置键是否均已被消费」）**：

- **逐类型评估 · 逐类型开启**;开启前必须先登记该类型的**存量未消费键基线**（F-14 的键即基线）⇒ 存量配置不会因开启而立刻装配失败。
- **凡本机制不消费的键 SHALL 显式登记为「已消费但不经 `load_param`」**（非标量键、字符串枚举、以及由其他消费方消费的键 —— 含 F-15 板级直读键）⇒ 对象数组 / 嵌套块 / 字符串枚举不被误判为未知键（登记口径见下方「基线二表」）。
- 适用域 = `params` 子对象;**不含** F-10 的 legacy 顶层回退路径。
- **消费方不止模块自身**：某键若由板级直读路径（F-15）消费，则该键的「已消费」登记 **SHALL** 覆盖该消费方 ⇒ 否则误判会**反向**发生（「实现读了，但检不出」⇒ 反而被当作未知键基线豁免 ⇒ 真实拼写错误被永久掩盖）。
- **基线二表（D-2 / R-6 / V-4 / M5 统一引用此唯一定义）**：
  - **表 A · 已消费但不经 `load_param`** ⇒ 凡本机制**不消费**的键 SHALL 登记于此，并**逐键标注消费方**（模块 / 板级直读）;⚠️ **板级直读键仅适用于 D-3 三态 ② / ③ 的类型**（同一类型内不得同时存在 `load_param` 声明的键与板级直读的键）;
  - **表 B · 真未消费豁免** ⇒ F-14 那类**实现零效果**的键 SHALL 登记于此（5 类 / 14 键），逐条登记为待修缺陷。
  ⇒ **表 A 不得用于表 B**;表 A 的键**不算**未知键，表 B 的键**不算**已消费。

### D-3 — 加载逻辑的执行点与工厂侧注入的**按类型退役**

- 参数声明 / 校验 / 赋值的执行点 = `SimObject::set_config()` 与模块的 `set_config` 覆写。
  ⚠️ 措辞**不写「唯一执行点」**：装配期另有 Step 4.5 `SimModule::simulate_instantiate`（`module_factory.cc:497`）这条**非参数**路径，对 `PcieEndpointIP` 会二次触发同一段加载逻辑（F-11）⇒ 本 ADR 不消除该二次调用，只要求经 `set_config` 触发的加载逻辑**幂等**。
- **过渡期按类型逐个退役工厂侧注入**（`module_factory.cc:263-291`）：某类型迁移完成 ⇒ **撤销其 `ParamRules` 注册** ⇒ 该类型的工厂注入自动失效。
  ⛔ **禁止**出现「同一类型既有 `load_param` 声明又有 `ParamRule` 注入」的重叠期（理由：同键双真值时覆盖顺序决定行为）。
  ⛔ **必填保护不得出现空窗**：某类型从工厂注入切换到 `load_param(..., 必填)` 必须在**同一 change 内原子完成**;**禁止**先删注入、后迁模块。
  ⚠️ 注入的现存保护本身有洞：`required` 检查只在 `mod.contains("params")` 时生效（F-6）,legacy 顶层回退（F-10）**绕过**它 ⇒ 保留注入的保护力是**不一致的**，不构成「立即删除是净损失」的理由。
  - **「重叠」是类型级**：T 已存在 `load_param` 声明 ⇒ `getParamRulesRegistry()` 中 SHALL 不存在 T 的条目（**含任何键**）;迁移 T 时其规则表全部键随注册一并删除（未迁移键的声明归属另案）。
  - **CFG-08 的处置（`ADR-X.11` 决策 7 · 已签发）**：工厂侧 `validateConfig` 的 per-type 硬编码键表（F-16）是**第四个声明面**，且其必填检查当前是 RouterTLM/NICTLM 的**唯一强制点**。⇒ 本 ADR 裁决：**退役** —— 把该键表并入同名类型的 `load_param` 必填声明，删除 CFG-08 的 RouterTLM/NICTLM 分支（**与该类型的 `registerParamRules` 注册、`get_param_rules()` 本体同 change 删除**）。退役前**必填真值唯一 = `load_param`**（不得与 CFG-08 并存）。
  - **必填保护的实例层级覆盖（实施期订正）**：`SimModule::simulate_instantiate` 经 `internal_factory->instantiateAll(config)` 对子模块**重跑同一路径**（`include/core/sim_module.hh:95`）⇒ **嵌套实例已受同一必填路径覆盖**（⚠️ 本段 v1.3 曾写「嵌套实例当前零必填强制」，**该结论为假**，已于 v1.5 订正）。⇒ 本决策**不改变**实例层级覆盖范围;⛔ 不得假设「迁移后会新增对嵌套实例的强制」——若实施期发现确有路径未覆盖，须在 change 内逐条登记为**新发现**，而非本决策的既有推论。
  - **未知键的失败位置（D-3a · 本决策的唯一执行点）**：`ParamLoadContext` 由 `SimObject` **基类按实例持有**（不依赖模块自己调用 `unknown_keys()`）;`SimObject::set_config` 在 `on_config_loaded()` 返回后执行一次「准入判定 + 未知键失败」：判定式 = `is_param_admitted(type()) && !ctx_.unknown_keys().empty()` ⇒ 抛 `cpptlm::ParamValidationError(module_name(), key, "unknown parameter")`（复用 `include/core/param_errors.hh` 的既有类型，⛔ 不新造异常）。⇒ **准入表 SHALL 是唯一 C++ 真源**（`is_param_admitted(type)`），仓库级闸/测试 **SHALL 从该表派生**，⛔ 不得在脚本里硬编码类型名（否则运行时与闸各持一份准入事实，复现本 ADR 要消灭的形态）。
- ⛔ **板级直读路径（F-15）不得成为第三条真值**：某类型纳入本机制时，板级直读键 SHALL 满足**三态之一**：① 该键所属类型**整体**改走已声明入口（板级读成员 / 访问器，不再读 JSON）;或 ② 该键在**该类型的 `load_param` 声明中不存在**，且登记为「已消费但不经 `load_param`」（消费方 = 板级直读）;或 ③ 该类型**整体排除**在未知键检查之外。⇒ **同一类型内不得同时存在「`load_param` 声明的键」与「板级直读的键」**（覆盖顺序决定行为）。
- **「T 迁移完成」的判据**：⇔ (a) T 的全部参数均由 `load_param` 声明（或按 D-2 登记为非 `load_param` 消费）**且** (b) `getParamRulesRegistry()` 中无 T 的条目 **且** (c) F-16 的 CFG-08 分支已删除（若 T 曾被 CFG-08 覆盖）;(a)(b)(c) 同时成立，才允许开启 T 的未知键检查。
- **配套硬约束**：加载逻辑 **SHALL NOT 清除或重置非本机制写入的状态**（幂等性不足以覆盖「清空外部状态」）。实证：`CohIfNICTLM::on_config_loaded` 无条件 `regions_.clear()`（`cohif_nic.hh:185`）;`PcieEndpointTLM::on_config_loaded` 无条件重初始化 `cfg_space_` / `bar_router_` / `msix_`（`pcie_endpoint_tlm.cc:140-143`）;`PcieEndpointIP` 在 `axi_adapter` / `link_layer` 缺席时**主动 detach** AXI 适配器 / 链路层 / PHY / Bypass Mux（`pcie_endpoint_ip.cc:202-215`）。
- 无 `params` 键的实例 **SHALL** 仍被加载一次（传空对象）—— 否则默认值永不落成员（F-6）;该动作对**全部类型**生效，故受上一条硬约束约束。

### D-4 — 值域类型与成员存储类型**解耦**

加载声明 SHALL 显式携带**值域类型**（用于 JSON 校验与范围比较），**不由成员存储类型推导**;绑定侧由存储类型单独转换，**收窄 / 符号不符一律装配失败**。

⇒ 成员存储类型变更（`unsigned` → `uint8_t`）SHALL NOT 静默改变参数可接受域。
⇒ **入声明不得收窄既有可接受值集合**（除显式收紧，且须在 change 里逐条给出「旧域 → 新域 → 行为面评估」）。

### D-5 — 默认值只落成员，**不写回 JSON**

现状把 `default_int` 注入回 `mod["params"]`（F-6）⇒ 装配后 JSON 从「用户键」变成「用户键 ∪ 默认键」⇒ 污染配置比对与 dump。⇒ 默认值**只落成员**;JSON 保持为**用户显式值的真源**。

### D-6 — 准入义务：每个进入声明的参数必须带一条**行为判别**测试

每个被声明的参数 SHALL 附带一个测试：显式设置两个不同合法值（默认值 / 非默认值）⇒ 至少一个**可观测指标**（统计量、只读访问器、输出计数）不同。**无此测试的参数不得被声明。**

⚠️ **本条是纪律，不是机制** —— 「某成员是否被运行时读取并影响行为」**不是可判定性质**，任何机制都无法自动判定 ⇒ 由 review + 测试存在性承担。ADR 明示此点，以免后续读者误以为病根已被结构性消除。

**可机械枚举的部分**：已声明参数 SHALL 可被机械枚举（`load_param` 调用点即为唯一枚举源）⇒「已声明但成员在模块内**零引用**」的参数可在 change 内以静态检索核对;**枚举结果为空的参数不得进入声明**。

⇒ **F-12 的 `vc_count` 因此暂不进入声明**（其值不影响运行时行为，`NUM_VCS` 仍是编译期常量）。

### D-7 — 不使用宏（对 `ADR-X.9` 端口侧形态的显式偏离）

本 ADR 的形态受一条**外部约束**：**不使用宏**。

`ADR-X.9` 决策 1 选项 A 采用「C++ struct + JSON 宏」并注明「DRAMSys 验证模式」。本 ADR 在其**不适用域**显式偏离（异构标量 + 每参元数据 + 值域/存储类型解耦 ⇒ 宏需承载多维元组且编译期报错信息劣化）。**与 `ADR-X.9` 的决策内容不冲突**（端口侧保持现状不动）。

### D-8 — 不改变 Python 侧定位

`ADR-X.12` 仍为 📝 提案（未实施）⇒ 本 ADR 不引入 Python 依赖、不改变 `cpptlm.topo` / `cpptlm.library` 的职责边界。

### 选项对比（决策 D-1 的替代形态）

| 形态 | 做法 | 对「声明/读取分离」的判别力 | 对「声明了却无效」的判别力 | 对「未知键检出」的判别力 | 成本 |
|---|---|---|---|---|---|
| **A′ 内联加载 + 消费即登记** ✅ | `load_param(cfg, key, member, domain, range, default)`;调用即登记已消费键;末尾差集检出未知键 | ✅ **结构性消除**（读取行**就是**声明行） | ❌ **仍可能**（与形态 A 同）⇒ 靠 D-6 兜底 | ✅ **天然具备**（已消费键集即检出机制） | 低（无类型擦除、无独立声明表） |
| **A 独立声明表 + 成员指针** | 模块返回 `std::vector<ParamSpec>`;`apply` 遍历表驱动校验与写入 | ✅ 同样消除（声明与绑定同表） | ❌ **仍可能**（表项指向无人读取的成员，照样失效） | ⚠️ 需额外机制（表键集 ≠ 已消费集） | 高（类型擦除 + `ParamSpec` 类型体系 + 声明表与绑定两套形态） |
| **A″ 单个 helper（只做类型+范围+默认）** | `load_param(...)`，**不做**未知键检查 | ✅ 同 A′ | ❌ 同上 | ❌ 无 | 最低 |
| **C 手写 `from_json` + 独立规则表**（现状 + DRAMSys 式） | 每模块一个 params struct + 独立 `ParamRule` 表 | ❌ 声明与绑定分离 ⇒ 缺陷仍可能 | ❌ 同上 | ❌ 无 | 中;且手写 field-by-field 搬运**已出真实 bug**（DRAMSys `Simulator.cpp:198`） |

⇒ **本 ADR 选择 A′**：目标（类型对 / 范围对 / 默认值对 / 不重复写解析代码 / **未知键可检出（D-2）**）直接达成且成本最低;A 的额外收益（派生统一规则面）**已被 `ADR-X.10` §F 划出本 ADR 范围** ⇒ 不构成本 ADR 的选型依据。
⇒ ⚠️ A′ 与 A 在「声明了却无效」这一族上**等价**（都不解决）⇒ 选 A′ 的理由**不是**它更彻底，而是**更便宜**。

---

## Consequences（后果 / 代价）

### 正面

1. **读取行即声明行** ⇒ 「声明面与读取面分离」这一形态的缺陷消除（D-1）;范围 / 默认值**只写一次**（消灭 F-4 的第三遍）。
2. **type / min / max 在装配期生效** —— 但**保护范围仅限已迁移模块**。⇒ **保护位置必须与保护范围一起声明**：保护**不在规则层**，而在**每个已迁移模块的 `load_param` 调用点**;**未迁移模块零保护**。读者不得据本 ADR 推断「模块参数已有统一校验层」。
3. **未知键暴露**（D-2）⇒ 拼错的参数名不再伪装成默认值（F-8）;并为 F-14 那一族提供**唯一**的检出机制。
4. **可机械判定的准入门**（V-4）⇒ 把「声明-实际一致」从纪律变成测试。
5. **覆盖面按需增长** ⇒ 模块用一次调用即获得约束，不要求全量迁移。

### 负面 / 代价

1. **改造量**：20 处读取点（F-1）需改为 `load_param` 调用;其中 **SimModule 家族 9 个类型 / 26 处**的挂载点与 ChStream 家族不同 ⇒ 需分别设计。
2. **本 ADR 不统一 ①② 声明面** ⇒ `configs/param_rules/*.json` ↔ C++ `get_param_rules()` ↔ 模块内 `load_param` 调用点的**多份维护问题原样留存**（按 D-3 的「按类型退役」执行后，稳定态为**两份**：json ↔ `load_param`;退役前为**三份**）。**现存漂移实证**：F-13（`flit_width` 两面不一致，且该参数零消费）。
3. **「已消费键」需要 per-instance 状态** ⇒ 每个使用该机制的模块需持有该集合（可由基类提供）。
4. **值域类型与存储类型双轨** ⇒ API 复杂度上升（D-4 的必然代价）。
5. **行为判别测试是义务不是机制**（D-6）⇒ 覆盖率依赖人力，无法自动保证。
6. **F-14 那一族（存量配置里有键、实现不读）在本 ADR 内不被修复** —— 本 ADR 只提供**检出**机制（D-2）+ 基线登记;**修复它们是独立 change**（含 `CacheTLM` / `TrafficGenTLM` / `GmmuTLM` / `LwcBusHub` / `MemoryTLM` 各自的参数落地）。

### 回滚路径

本 ADR 的实施 SHALL 满足「**零配置面变更 + 单点回滚**」：删除 `load_param` 调用、恢复模块内的逐键读取、恢复该类型的 `ParamRules` 注册即可完整回滚;**不涉及** JSON schema、**不涉及** `configs/*.json`、**不涉及** ABI。

### 风险

| # | 风险 | 缓解 |
|:--:|---|---|
| **R-1** | 「声明了但成员不被运行时读取」仍可能（D-6 明示不可自动判定） | D-6 的行为判别测试义务 + 可机械枚举的零引用核对 + review 把关 |
| **R-2** | 未知键检查误伤**存量未消费键**（F-14 那一族，**多数为标量**（含若干字符串值，如 `pattern` / `start_addr` / `CacheTLM.size`）且当前配置里就有）⇒ 开启即装配失败 | D-2 的**逐类型开启** + 存量未消费键基线登记;非标量键显式登记为已消费 |
| **R-3** | 值域声明**收窄**既有可接受值集合 ⇒ 静默行为变更 | D-4 的「不得收窄」约束 + change 内逐条行为面评估 |
| **R-4** | 「无 `params` 键也加载一次」对**全部未迁移类型**生效 ⇒ 触发无守卫副作用（`regions_.clear()` / `cfg_space_` 重初始化 / 主动 detach） | D-3 的「SHALL NOT 清除非本机制写入的状态」硬约束;V-3 覆盖未迁移类型的零变化 |
| **R-5** | 迁移期若出现「同类型既有注入又有 `load_param`」⇒ 同一键两个真值 | D-3 的按类型原子退役 + 禁止重叠期 |
| **R-6** | 板级直读路径（F-15）未被纳入 D-3 的退役范围 ⇒ 同一键出现「模块 `load_param` + 板级直读」两个真值;且这些键会被 D-2 误判为未知键 ⇒ 拼写错误被基线豁免永久掩盖 | D-3 的板级路径条款（改走已声明入口 **或** 显式排除）+ D-2 的消费方扩展;V-4 的基线清单 SHALL **逐键标注消费方**（模块 / 板级直读 / 真未消费） |

---

## Verification（可勾选验收判据）

> **前瞻**判据（非实测日志）。每条须有工具证据方可判 pass（0 工具调用即判假阳性）。
> ⚠️ **执行载体 = spec + 闸**（per `ADR-X.10` §G「ADR 回答为什么，spec 回答是什么，闸回答不准偏」）⇒ 本章列出判据，**落地在 spec 与闸**，不重复其内容。

| # | 判据 | 判别力说明 |
|:--:|---|---|
| **V-1** | 新增一个**测试夹具模块**（仅测试用，声明 2 个标量参数，其一带默认值）⇒ 越界 / 类型错 / 未知键 三种配置分别使 `instantiateAll` 返回 **false**，且错误信息含**模块名 + 键名 + 期望范围** | ✅ **真红** —— 唯一能证明「机制存在」而非 RouterTLM 既存行为的判据（RouterTLM 上的越界/类型错已被既有实现满足 ⇒ 无判别力） |
| **V-2** | 夹具模块的两个参数各有一条「两个不同合法值 ⇒ 可观测指标不同」的测试（D-6 的落地形式） | ✅ 真红 |
| **V-3** | ① **未**迁移的类型（含全部 SimModule 家族）：行为与改动前**逐项一致**（全标签套件全绿 + **既有 `REQUIRE` 常量零变化**）② **已迁移类型**的**嵌套实例**必填行为变更（由「静默取默认值」变为「装配失败」）须逐类型登记于行为面评估 | ✅ 真红 —— 最大风险面（R-4）;② 是**有意的**行为变更（D-3 实例层级条款） |
| **V-4** | 仓库级对拍，**限定在「已迁移且已开启未知键检查」的类型集合内**：该集合的配置键集 ⊆ **源码声明键集 ∪ 表 A 键集**（表 A = 已消费但不经 `load_param` 的键，含 F-15 板级直读键;缺此并集则该式对含板级键的类型字面不可满足）。**未迁移类型不在判定范围**（其配置键未被消费是**既存缺陷**F-14，不是本次回归） | ⚠️ **范围限定**后的真红判据;**须先落基线二表**（表 A 已消费但不经 `load_param` · 表 B 真未消费豁免〔F-14 的 5 类 / 14 键，逐条登记为待修缺陷〕，两表逐键标注消费方），否则该闸第一天即因既存缺陷而红 |
| **V-5** | 声明的 `min` / `max` 与失败行为一致：在测试内构造越界值 ⇒ 失败;边界值 ⇒ 通过（**不改源码**即可验证） | ✅ 真红 |
| **V-6** | 断言对象 SHALL 为 **V-1 的测试夹具模块**（成员初值 ≠ 声明默认）⇒ 无 `params` 键时声明的默认值仍落到成员（只读访问器可读） | ✅ 真红 —— **对 RouterTLM 不成立**（其成员初值 `BUFFER_DEPTH = 8` 恰等于声明默认 ⇒ 今天即绿） |
| **V-7** | 加载逻辑**幂等**：同一实例二次加载 ⇒ 成员值不变（F-11） | ⚠️ **回归护栏**（RouterTLM 今天是纯赋值 ⇒ 今天即绿）⇒ 定位为「迁移后不得破坏幂等」，**不是变更探测器** |

### 一次性判别力证据（人工变异，非 CI 判据）

- 注释掉某次 `load_param` 的赋值 ⇒ 对应 V-2 的行为判别测试**必红**。
- 把某声明的 `max` 改小 ⇒ 对应 V-5 的边界用例**必红**。

---

## 与既有 ADR / main spec 的关系

| 对象 | 关系 | 说明 |
|---|---|---|
| `ADR-X.10` 决策 1 | **部分取代**（仅「规则存放位置」维度） | 该 ADR §B 已登记「载体」与「存放位置」两维度共用 A/B 标签导致文本歧义 ⇒ 本 ADR 只裁决后者 |
| `ADR-X.10` 决策 4.5 / 7 | **部分闭合**（闭合范围见 Metadata 逐条表） | 本 ADR 决定「声明写在哪、由谁执行」，不重新决定「要不要运行时校验」;未闭合项须具名登记 |
| `ADR-X.10` §A~§G | **既有内容不触碰**;仅在其 `## Status Update` 段**末尾追加**一个指向本 ADR 的子段（append-only，见 M0） | `configs/param_rules/*.json` 存废另案（且「删除 json 不是可选项」—— 它是已 Deprecated 但仍在跑的 Python 生成期校验的活数据源） |
| `ADR-X.10` §G | **遵守** | 可测不变量的**权威落点 = spec + 闸** ⇒ 本 ADR 的 Verification 章只列判据，其执行载体在 spec + 闸 |
| `ADR-X.11` 决策 7（CFG-08 · 已签发） | **须登记 + 处置 = 退役** | 工厂侧 per-type 硬编码必填/整数类型表（F-16）是**第四个声明面**，且是 RouterTLM/NICTLM 必填的**唯一强制点** ⇒ 本 ADR 裁决退役（并入 `load_param` 必填声明，删除该分支，与注册删除同 change） |
| `ADR-X.16` | **登记其过期断言的闭合** | `:113` 的「`buffer_size` param 不生效（默认 16）」已被 change `cpptlm-router-buffer-size-param` 闭合 ⇒ **待追加**（append-only，在本 ADR 之外独立执行，不构成本 ADR 的依赖;实测该 ADR 的 Status Update 目前**只有** D4 stall 锚点澄清一条） |
| `ADR-X.9` | **偏离但不相冲** | 端口侧保持现状;形态差异见 D-7 |
| `ADR-X.12` | **无关** | Python 生成器未实施 |
| `openspec/specs/param-derivation/` | **决策取代（spec 文本待修订）** | 该 spec `:16` 强制「Each TLM module SHALL declare its parameter rules via a static `get_param_rules()` method」、`:22-24` 强制 `flit_width` 派生 ⇒ **与本 ADR 正面矛盾**。本 ADR **不直接修改** spec;在实施 change 的 delta 中修订（删 `get_param_rules()` 条款 + 删 `flit_width` 派生条款 + 补 TBD 占位的 Purpose）;**spec 修订前该条款仍是活契约** ⇒ 已知张力登记在实施 change 的 proposal 中 |
| `openspec/specs/param-validation/` | **部分对齐 + 须修订（spec 文本待修订）** | ⚠️ `:38-45`「Required parameter enforcement」是与必填语义**最贴近**的活条款，其「marked required」表述与 D-1 的「`default = NONE`」术语有差 ⇒ 须对齐;`:26-36` 的**跨参数约束**（`node_x < mesh_x`）本 ADR **无机制**;`:14-20` 的 mesh 范围 1-16 与 `:22-24` 的 `vc_count ∈ [1,8]` 在本 ADR 下**仍未实现**;`:8` 的执行者写作「The ModuleFactory SHALL validate」，本 ADR 把校验移到模块内 ⇒ **执行者变更**（ModuleFactory → 模块内 `load_param`）;**spec 修订前该条款仍是活契约** ⇒ 实施 change 须同步修订 |
| `openspec/specs/param-type-conversion/` | **部分对齐 + 须修订（spec 文本待修订）** | `:46-56` 强制「参数值按声明类型在 **ModuleFactory** 侧自动转换（『100ns』→100000 后写入）」⇒ 与本 ADR 把值域判定放到模块内冲突（构成与 D-3「不得双真值」同构的**第二个转换真值点**）⇒ 实施 change 须同步修订 |
| `docs/architecture/06-inter-module-network.md` §9.4（真源 1） | **须同步改写** | §9.4 把「运行时声明面 = 各模块 `get_param_rules()`（经 `getParamRulesRegistry()` 注册）」写为现状，并要求「两面的内容应当相等」⇒ 本 ADR 逐类型退役该面 ⇒ 实施 change 落地后须改写（运行时声明面行 + 「两面相等」的不变量表述） |
| `docs/architecture/10-topology-generation-management-system.md` §8.2/8.3/8.4（真源 1） | **须同步** | 该章以「Step 2.5 传参 + 模块内逐键 `contains` 读取」为参数流范式 ⇒ D-1 正是要替换该范式 ⇒ 实施 change 落地后须登记同步 |
| `openspec/specs/params-config-semantics/` | **兼容且收窄** | 该 spec 允许数组/嵌套对象值;本 ADR 仅覆盖标量子集，不改变该 spec |

---

## 迁移计划

| 步 | 内容 | 门 |
|---|---|---|
| **M0** | 在 `ADR-X.10` 的 `## Status Update` 段**末尾** append-only 追加一个指向本 ADR 的子段 + 6 项未闭合项各有具名 owner（2 项引用既有 carrier，见 Metadata） | diff 仅追加（§A~§G 既有内容零改动） |
| **M1** | 新增 `include/core/param_loader.hh`（加载 + 校验 + 已消费键登记 + 值域/存储类型解耦），**不动既有代码** | 编译通过 |
| **M2** | **测试夹具模块**（仅测试用）⇒ 先证明机制本身可红 | V-1 / V-2 |
| **M3** | 模块侧接入点（`on_config_loaded` **或** `set_config` 覆写）+ 加载幂等 + 「SHALL NOT 清除非本机制写入的状态」硬约束 | V-2 / V-7 |
| **M4** | **RouterTLM 迁移 + 同 change 内删除**：其 `registerParamRules` 注册调用（`router_tlm.cc:617-620`）、`get_param_rules()` 本体（`router_tlm.cc:584-610` + `router_tlm.hh:208`）、**CFG-08 的 RouterTLM 分支**（`module_factory_validate.cc:304-305` 扁平回退 + `:311-319` 逐键检查）⇒ 必填语义由 `load_param(…, default = NONE)` 承接 ⇒ **必填保护无空窗**。<br>⛔ **NICTLM 分支（`:306-307` + `:320-326` + `:327-330` else-if）不得在本 change 删除** —— NICTLM 无 `registerParamRules` 注册（F-7），CFG-08 是其 `node_id` 必填的**唯一**强制点;该分支必须在 **NICTLM 迁移 change** 内与 `load_param("node_id", …, default = NONE)` 一并删除，删除前 NICTLM 仍属「未迁移类型」，受 V-3 ① / M6 零变化约束（`test/test_config_schema_validation.cc:122/138/219` 三条断言依赖它）。<br>⚠️ `test/test_d3_param_resolver_registry.cc` 须随 RouterTLM 退役**删除或改写**（`:52-59` 的 `REQUIRE(before == after)` 幂等断言在注册退役后必失败;`:62` 的 `getParamRulesRegistry()["RouterTLM"]()` 在键消失时由 `operator[]` 插入默认项 ⇒ sentinel 断言空转假绿） | V-5 / V-6（夹具） |
| **M5** | 未知键检出的**逐类型准入**（判据见 D-3 的「T 迁移完成」）+ **基线二表**落盘（表 A 已消费但不经 `load_param`〔含 F-15 板级直读键〕· 表 B 真未消费豁免〔F-14 的 5 类 / 14 键〕，两表逐键标注消费方） | V-4 |
| **M6** | 回归：全标签套件全绿 + **既有 `REQUIRE` 常量零变化**（覆盖**全部未迁移类型**） | V-3 |
| **M7** | 仓库级对拍闸（范围限定 + 基线豁免）落地 | V-4 |
| **M8** | 其余类型**按需**渐进纳入（每个类型各自完成「迁移 + 撤销注册」两步） | 每类型各自 V-1~V-5 |

---

## 开放问题

1. **未知键检查何时可推广到 F-14 那一族类型？** —— 需先决定是「先检出再逐个修」还是「先修再开启」⇒ 与 F-14 的独立 change 协同。
2. **`vc_count` 何时做成运行时可配？** —— 需把 `NUM_VCS` 从编译期常量改为运行时维度（影响三个容器的数组第二维）⇒ 独立 change。
3. **`get_param_rules()` / `ParamRule` 的最终归宿？** —— 本 ADR 不裁决（`ADR-X.10` §F）;但按 D-3 逐类型退役后，它将只剩「未迁移类型」在用。
4. **SimModule 家族（F-1②的 9 个类型）是否适用同一套加载机制？** —— 它们的 `set_config` 覆写形态与 ChStream 家族不同 ⇒ 可能需要独立的挂载形态（待实施 change 的 design.md 判定）。

---

## 变更记录

| 版本 | 日期 | 变更 |
|---|---|---|
| 1.0 | 2026-11-17 | 初稿：内联加载 + 消费即登记（不使用宏）;声明与读取同源;未知键检出;值域/存储类型解耦;默认只落成员不写回 JSON;行为判别测试义务 |
| 1.1 | 2026-11-17 | 按 oracle 第 1 轮（REJECT · 9 项 must-fix）修订：收窄 D-1 的主张并修正「选项对比」表对 A 的偏袒描述;D-3 改为按类型退役 + 必填保护无空窗 + 「SHALL NOT 清除非本机制写入的状态」;迁移计划改序（M4 合并「迁移 + 撤销注册」）;Verification 三处订正（V-4 范围限定 + 基线豁免、V-6 换夹具、V-7 标护栏）;Metadata 逐条列闭合范围;F-1 订正为 20 处 / 2 类挂载点;新增 F-13 / F-14 与「缺陷幸存机制」段;关系表三行订正（`param-derivation` 取代 / `param-validation` 须修订 / `ADR-X.10` §G 遵守）;示例统一为伪代码六槽位形态 |
| 1.2 | 2026-11-17 | 按 oracle 第 2 轮（REJECT · 本轮新增内容引入 2 项 must-fix）修订：**MF-1** F-14 的 `GmmuTLM` 行订正（`tlb_size` / `tlb_miss_latency_cycles` 由板级直读路径消费 ⇒ **不是** F-14 成员）并新增「幸存机制的适用边界」段;**MF-2** 新增 **F-15**（第三条消费路径 = 板级直读 JSON `params`）+ D-1 覆盖域说明 + D-2 消费方扩展与基线分表 + D-3 板级真值条款 + **R-6** + V-4/M5 的基线清单逐键标注消费方;另采纳 S1（选项对比表增「未知键检出」列 + 选型理由补该目标）/ S2（Metadata 闭合范围第 3、4 行补「仅对已迁移模块」）/ S3（F-4 行号）/ S4（R-2 措辞）/ S5（M4 删除注册调用与 `get_param_rules()` 本体）/ N1（F-12 行号）/ N2（本行）/ N3（D-1 反例措辞） |
| 1.5 | 2026-11-17 | 立项期 gap 分析（metis 1 轮）实证订正 2 处：① **D-3 的「嵌套实例当前零必填强制」为假** —— `SimModule::simulate_instantiate` 经 `internal_factory->instantiateAll` 对子模块重跑同一路径（`sim_module.hh:95`）⇒ 改为「嵌套实例已受同一必填路径覆盖;本决策不改变该覆盖范围」;② **新增 D-3a（未知键的失败位置）** —— `ParamLoadContext` 由 `SimObject` 基类持有、`set_config` 在 `on_config_loaded()` 返回后执行「准入判定 + 未知键失败」、抛既有 `ParamValidationError`、准入表为唯一 C++ 真源（`is_param_admitted`）⇒ 关闭「`unknown_keys()` 无调用者 ⇒ 未知键判据可用夹具手写 throw 蒙过」的判别力空洞。 |
| 1.4 | 2026-11-17 | 按 critic 第 2 轮（复审 · REJECT · 1 BLOCKER + 6 SHOULD_FIX + 5 NITS）修订：**B-1** M4 拆分（只删 CFG-08 的 **RouterTLM** 分支;NICTLM 分支须在 NICTLM 迁移 change 内与 `load_param("node_id", default=NONE)` 一并删，否则 `node_id` 必填空窗）+ `test_d3_param_resolver_registry.cc` 处置改写（幂等断言失效 + sentinel 空转 ⇒ 删除或改写）;**S-A** F-16 锚点改 `:311-326` / `:327-330`（`:478-481` 属死代码）;**S-B** F-4 补「必填强制点有两个（② 与 ④）」;**S-C** `ADR-X.16` 行改「待追加」;**S-D** M0 进迁移计划表;**S-E** V-4 判据改「⊆ 源码声明键集 ∪ 表 A 键集」;**S-F** 见 B-1 的测试处置;**N-a** 括注改纯理由;**N-b/N-c** 表 A 与 R-2 对齐覆盖域 + 三态限定;**N-d** `param-validation` 补 `:38-45`;**N-e** M0 的 carrier 口径指名 |
| 1.3 | 2026-11-17 | 按 critic 第 1 轮（REJECT · 1 BLOCKER + 8 SHOULD_FIX + 5 NITS）修订：**BLOCKER** 登记 `ADR-X.11` 决策 7（CFG-08）为**第四个声明面**（新增 F-16）并裁决**退役**（D-3 + M4 + 关系表）;F-4 改写为「四处 + 仅 CFG-08 有强制力」;**S-1** F-15 键清单补全（`use_zero_delay_for_test` / `ptx_emu_root` / `va_region_size` / `gpu_id` / `gfx_version` / `bdf`）+ M5 基线二表 + V-4 输入=两表并集;**S-2** D-3 增「必填保护实例层级 = 全部实例（含嵌套）= 有意行为变更」+ V-3 补已迁移类型登记;**S-3** D-3 三处可判定性（重叠=类型级 /「T 迁移完成」判据 / 板级路径三态穷举）;**S-4** 关系表增真源 1 §9.4、真源 1 `10-topology-generation-management-system.md`、`param-type-conversion` spec;**S-5** spec 行改「决策取代（spec 文本待修订）」+「spec 修订前仍是活契约」;**S-6** Metadata 闭合表加「闭合=决策已作出 ≠ 能力已存在」+ 4.6 ①拆分 + M0 绑定;**S-7** §F 行改「append-only 追加子段」;**S-8** D-2 改按覆盖域表述 + 基线二表唯一定义;**N-1** 必填由 `default = NONE` 表达;**N-2** 基线二表合并定义;**N-3** 关系表加 `ADR-X.16` 行;**N-4** M4 增 `test_d3_param_resolver_registry.cc`;**N-5** 版本序修正 |
