# gem5 SLICC 协议描述语言 + AMBA CHI 协议实现调研

> 仓库: `/workspace/main/gem5/`（只读调研，未修改任何文件）
> 调研范围: `src/mem/slicc/`、`src/mem/ruby/protocol/`、`configs/ruby/`

---

## 1. SLICC 工作机理

### 1.1 编译器入口与流水线

SLICC = *Specification Language for Implementing Cache Coherence*，是一个基于 Python + PLY 的
编译器，把 `.slicc` / `.sm` 源文件编译为 C++ 控制器代码 + Python SimObject 代码。

| 阶段 | 位置 | 说明 |
|------|------|------|
| CLI 入口 | `src/mem/slicc/main.py:59` `main()` | `-C/--code-path` 默认 `generated`；要求 `*.slicc` |
| 构造解析器 | `src/mem/slicc/main.py:114` | `SLICC(slicc_file, [RubySlicc_interfaces.slicc], protocol_base, ...)` |
| 词法/语法 (PLY) | `src/mem/slicc/parser.py:55` `class SLICC(Grammar)` | LALR；`yacc_kwargs["debug"]` 由 `--debug` 控制 (`parser.py:74-75`) |
| 保留字 | `src/mem/slicc/parser.py:146-177` | `protocol/include/global/machine/in_port/out_port/action/transition/structure/external_type/enumeration/state_declaration/peek/stall_and_wait/wakeup_port/enqueue/check_allocate/check_next_cycle/check_stop_slots/check_on_cache_probe/static_cast/if/is_valid/is_invalid/else/return/void/new/OOD/defer_enqueueing` |
| 语法规则 | `parser.py:320-869` | 见下表 |
| AST 生成 | `parser.py:113` `process()` → `decl_list.generate()` | AST 节点 `self.decl_list` 来自 `parse_file` |
| 代码输出 | `parser.py:116` `writeCodeFiles()` → `SymbolTable.writeCodeFiles()` | 见 §1.4 |

**解析顺序**（`parser.py:89-97`）：先解析共享 includes（`RubySlicc_interfaces.slicc`），
把已解析类型标记 `setShared()`，再解析协议自身的 `.slicc` 文件（`protocol` 声明只能在协议文件中出现一次，`parser.py:320-339`）。

### 1.2 关键语法元素 → 语法规则 → AST 节点

| 语法元素 | 规则 (`parser.py`) | AST 节点 |
|----------|-------------------|----------|
| `protocol "X" opt1, opt2;` | `p_decl__protocol` :320 | 写入 `self.protocol` + `self.options`（`partial_func_reads` 等，:78-83） |
| `include "f.sm";` | `p_decl__include` :341 | 递归 `parse_file`（相对目录或 `base_dir`） |
| `machine(MachineType:Cache, "...") : params { decls }` | `p_decl__machine0/1` :350-356 | `ast.MachineAST` |
| `action(Name, desc="") { ... }` | `p_decl__action` :358 | `ast.ActionDeclAST` |
| `in_port(p, Type, buf, rank=N) { ... }` | `p_decl__in_port` :362 | `ast.InPortDeclAST` |
| `out_port(p, Type, buf);` | `p_decl__out_port` :366 | `ast.OutPortDeclAST` |
| `transition(State, Event) { Actions; }` | `p_decl__trans0..3` :370-384 | `ast.TransitionDeclAST` |
| `structure(Name, interface="Message") { fields }` | `p_decl__struct` :396 | `ast.TypeDeclAST` |
| `enumeration(Name) { A; B; }` | `p_decl__enum` :400 | `ast.EnumDeclAST` |
| `state_declaration(State, ...) { S, AccessPermission:X; }` | `p_decl__state_decl` :405 | `ast.StateDeclAST` |
| `peek(buf, Type) { ... }` | `p_statement__peek` :704 | `ast.PeekStatementAST` |
| `enqueue(port, Type, latency) { ... }` | `p_statement__enqueue*` :680-690 | `ast.EnqueueStatementAST` |
| `stall_and_wait(a,b);` / `wakeup_port(a,b);` | :696-702 | `StallAndWaitStatementAST` / `WakeupPortStatementAST` |
| `check_allocate` / `check_next_cycle` / `check_stop_slots` / `check_on_cache_probe` | :708-722 | 资源/流水线校验语句 |
| `static_cast<>`, `is_valid`, `is_invalid`, `OOD`, `new`, `defer_enqueueing` | :746-840 | 对应表达式/语句 AST |

AST 目录 `src/mem/slicc/ast/`（~50 节点）。核心是 `ast/MachineAST.py:35`：
- `MachineAST.generate()`（:63）→ `pushFrame()` → 创建 `StateMachine` 符号 → `newCurrentMachine()` →
  `decls.generate()` → `machine.buildTable()` → `popFrame()`。
- `MachineAST.files()`（:47-61）声明该 machine 的 5 个输出文件名。

### 1.3 符号表与后端

- `src/mem/slicc/symbols/SymbolTable.py:46` `SymbolTable` — 作用域栈（`sym_map_vec`），
  `writeCodeFiles()`（:136-163）先写聚合头 `Types.hh` + `<Protocol>ProtocolInfo.hh`（:158-160、:165-212），
  再逐个 symbol 调用其 `writeCodeFiles()`。
- symbol 类型: `StateMachine.py`、`Type.py`、`State.py`、`Event.py`、`Action.py`、
  `Transition.py`、`Func.py`、`RequestType.py`、`Var.py`、`Symbol.py`（`symbols/__init__.py:27-37`）。
- **C++ 代码生成逻辑不在 `generate/` 里**——`generate/` 只有 `html.py`、`dot.py`、`tex.py`
  （文档/可视化输出）。真正的 C++ 后端内嵌在 `StateMachine.py` 与 `Type.py` 中。

### 1.4 C++ / Python 产物清单与生成函数

`StateMachine.writeCodeFiles()`（`StateMachine.py:257-262`）每个 machine 产出 5 个文件：

| 生成函数 | 产物 | 内容 |
|----------|------|------|
| `printControllerPython` (`StateMachine.py:264`) | `<Proto>_<Machine>_Controller.py` | RubyController SimObject（`cxx_class = 'gem5::ruby::<Proto>::<Machine>_Controller'`，:289-290） |
| `printControllerHH` (`:340`) | `<Machine>_Controller.hh` | 类声明（`doTransition/doTransitionWorker`、action 原型、TBETable/EntryType setter、统计数组） |
| `printControllerCC` (`:595`) | `<Machine>_Controller.cc` | 构造/`init()`/`initNetQueues()`/`regStats()`/`collateStats()`/所有 action 函数体（`action["c_code"]`）/`functionalRead/WriteBuffers` |
| `printCSwitch` (`:1642`) | `<Machine>_Transitions.cc` | `doTransition()` 大 switch（`HASH_FUN(state,event)`，:1665） |
| `printCWakeup` (`:1482`) | `<Machine>_Wakeup.cc` | `wakeup()` 循环遍历所有 in_port（`port["c_code_in_port"]`，:1590） |

`Type.writeCodeFiles()`（`Type.py:225-235`）：
- 枚举 → `<Type>.hh` / `<Type>.cc`（`printEnumHH/CC`）
- 结构/消息 → `<Type>.hh` / `<Type>.cc`（`printTypeHH/CC`；message 继承 `interface`，如 `Message`，:293-305）
- `external` 类型 → 不生成（`:226-228`）

`SymbolTable.writeProtocolInfo()`（`SymbolTable.py:165`）生成 `<Proto>ProtocolInfo.hh`，
携带 `slicc.options`（如 CHI 的 `partial_func_reads`）。

### 1.5 构建系统集成

`src/mem/ruby/protocol/SConscript`：
- `:47` `from slicc.parser import SLICC`
- `:49-53` 收集所有 `src/mem/slicc` 源作为依赖
- `:67` `slicc_includes = ["mem/ruby/slicc_interface/RubySlicc_includes.hh", ...]`
- `:72` `slicc_emitter` / `:104` `slicc_action`（调 `slicc.process()` + `slicc.writeCodeFiles(output_dir, slicc_includes)`）
- `:123-124` 注册 `Builder(action=MakeAction(..., Transform("SLICC")))`
- `:142-163` 从 Kconfig 变量 `RUBY_PROTOCOL_*` 收集协议，`find_protocol_sources()` 在 `PROTOCOL_DIRS` 下定位 `<Proto>.slicc`
- `:165-167` `env.SLICC([], sources)`，输出到 `build/.../mem/ruby/protocol/`

---

## 2. CHI 协议族 (`src/mem/ruby/protocol/chi/`)

### 2.1 目录布局（全部 `.sm` 文件）

| 文件 | 一句话说明 |
|------|-----------|
| `CHI.slicc` | 协议入口：`protocol "CHI" partial_func_reads;` include 下面 4 个 |
| `CHI-msg.sm` | 消息定义：`CHIRequestMsg`/`CHIResponseMsg`/`CHIDataMsg` 三个 `structure(..., interface="Message")` + 三个枚举 `CHIRequestType`/`CHIResponseType`/`CHIDataType` |
| `CHI-cache.sm` | `machine(MachineType:Cache, ...)`（:39）声明：config params、network MessageBuffers、`state_declaration`（:231）、`enumeration(Event)`（:292） |
| `CHI-cache-transitions.sm` | 全部 transition（按 Stable/Transient 分类） |
| `CHI-cache-actions.sm` | 全部 action（TBE 分配、发送/接收、cache 更新等） |
| `CHI-cache-funcs.sm` | helper 函数（`respToEvent`/`snpToEvent`/`dataToEvent` 等映射、`getCacheEntry`/TBE 管理） |
| `CHI-cache-ports.sm` | `in_port`/`out_port` 定义（:40-48 out，:66+ in） |
| `CHI-mem.sm` | `machine(MachineType:Memory, ...)`（:39）内存控制器（SNF）；自带 state_declaration（:81）、ports、transitions、actions |
| `CHI-dvm-misc-node.sm` | `machine(MachineType:MiscNode, ...)`（:39）DVM 分发；仅 config + include |
| `CHI-dvm-misc-node-transitions.sm` | MiscNode transitions |
| `CHI-dvm-misc-node-ports.sm` | MiscNode ports |
| `CHI-dvm-misc-node-funcs.sm` | MiscNode helper funcs |
| `CHI-dvm-misc-node-actions.sm` | MiscNode actions |
| `generic/CHIGenericController.{hh,cc}` + `generic/CHIGeneric.py` + `generic/SConscript` | **手写** C++ facade 控制器（`AbstractController` 派生） |
| `tlm/*` | ARM CHI-over-TLM 实现（手写 `controller.hh/cc`、`generator`、`port`、`utils`；`TlmController.py`/`TlmGenerator.py` 生成胶水） |
| `Kconfig`, `SConsopts` | 构建配置 |

### 2.2 过渡消息类目（REQ / RSP / SNP / DAT）

`CHI-msg.sm` 用**三个 `structure` + 三个枚举**覆盖四条 CHI 通道：

| 通道 | 消息类型 | 枚举 | 代表成员 |
|------|----------|------|----------|
| REQ | `CHIRequestMsg`（:103-133） | `CHIRequestType`（:44-101） | `ReadShared/ReadNotSharedDirty/ReadUnique/ReadOnce/CleanUnique/MakeReadUnique/Evict/WriteBackFull/WriteCleanFull/WriteUnique*/Atomic*/ReadNoSnp/WriteNoSnp/StashOnce*` + DVM 请求 |
| SNP | 复用 `CHIRequestMsg` | 同上 | `SnpShared/SnpUnique/SnpCleanInvalid/SnpOnce/Snp*Fwd/SnpDvmOp*` |
| RSP | `CHIResponseMsg`（:165-182） | `CHIResponseType`（:135-163） | `Comp_I/Comp_UC/Comp_SC/CompAck/CompDBIDResp/DBIDResp/ReadReceipt/RespSepData/SnpResp_*/RetryAck/PCrdGrant` |
| DAT | `CHIDataMsg`（:215-266） | `CHIDataType`（:184-213） | `CompData_*/DataSepResp_UC/CBWrData_*/NCBWrData/SnpRespData_*` |

注意：**SNP 与 REQ 共用 `CHIRequestMsg`**（不是独立 struct）；枚举内用 `Snp*` 前缀区分。
`CHIRequestMsg` 额外带 DCT/DMT 字段：`fwdRequestor`、`dataToFwdRequestor`、`retToSrc`、`allowRetry`（:109-112），
以及 `txnId`/`ns`（:123-125）。`CHIDataMsg` 带 `dataBlk` + `bitMask`（:220-221）与 dirty 判定逻辑（:237-250）。

### 2.3 网络通道划分（多 vnet）

CHI 用 4 条虚拟网络，SLICC 声明在 machine config 里：

`CHI-cache.sm:189-197`（`CHI-mem.sm:55-63` 一致）：
```
reqOut / reqIn → virtual_network="0", vnet_type="none"
snpOut / snpIn → virtual_network="1", vnet_type="none"
rspOut / rspIn → virtual_network="2", vnet_type="none"
datOut / datIn → virtual_network="3", vnet_type="response"
```
注释（`CHI-cache.sm:187`）：`virtual networks: 0=request, 1=snoop, 2=response, 3=data`。
`network="To"` 为出站队列，`network="From"` 为入站队列（:186）。

配置侧：`configs/ruby/CHI.py:236-237` 设 `ruby_system.network.number_of_virtual_networks = 4`；
`data_channel_size` 从 `NoC_Params.data_width` 取（`CHI.py:232-233`，`CHI_config.py:113-114`）。

### 2.4 CHI cache 状态机（`CHI-cache.sm:231-285`）

| 类别 | 状态 |
|------|------|
| Stable | `I, SC, UC, SD, UD, UD_T`（UD 带 use-timeout，:241） |
| 只被上游持有的 "R" 状态 | `RU, RSC, RSD, RUSC, RUSD`（:244-248） |
| 本地+上游组合状态 | `SC_RSC, SD_RSC, SD_RSD, UC_RSC, UC_RU, UD_RU, UD_RSD, UD_RSC`（:251-258） |
| DVM 状态 | `DvmTlbi_Unconfirmed/DvmSync_Unsent/DvmSync_Unconfirmed/DvmTlbi_Waiting/DvmSync_Waiting/DvmOp_Finished`（:261-266） |
| 外部 DVM 状态 | `DvmExtTlbi_Partial/DvmExtTlbi_Executing/DvmExtSync_Partial/DvmExtSync_Executing/DvmExtOp_Finished`（:268-272） |
| **通用 transient** | `BUSY_INTR`（snoop 可回应）/ `BUSY_BLKD`（阻塞 snoop）（:280-281） |
| null | `null`（:284） |

关键设计（:274-281 注释）：**CHI 只有一个通用 transient "BUSY" 状态族**，具体行为与最终 stable 状态
由 **TBE（Transaction Buffer Entry）中的信息动态决定**。

### 2.5 与 MESI_Two_Level 的核心差异

| 维度 | MESI_Two_Level | CHI |
|------|----------------|-----|
| 状态枚举 | `NP,I,S,E,M` + 显式 transient `IS,IM,SM,IS_I,M_I,SINK_WB_ACK` + prefetch `PF_IS/PF_IM/PF_SM/PF_IS_I` + `LLSC_E/LLSC_M`（`MESI_Two_Level-L1cache.sm:86-112`） | 稳定态含大量组合态（本地/上游/组合三元组）+ 通用 `BUSY_INTR/BUSY_BLKD`（`CHI-cache.sm:231-285`） |
| transient 风格 | **每 transition 一对显式 transient 状态**（编译期枚举固定） | **单一 BUSY + TBE 携带意图**（运行期动态决定迁移） |
| 消息建模 | 每种消息一个 `structure`（`MESI_Two_Level-msg.sm`） | 三通道三个 `structure` + 类型枚举（`CHI-msg.sm`） |
| 通道/vnet | 3 个 vnet（`MESI_Two_Level.py:252` `number_of_virtual_networks = 3`）；vnet_type 有 `request/response/unblock`（`MESI_Two_Level-L1cache.sm:59-77`） | 4 个 vnet：REQ/SNP/RSP/DAT（`CHI.py:237`；`CHI-cache.sm:189-197`） |
| 一致性模型 | 目录式 MESI（L1↔L2/Dir） | MOESI-like（含 SD/UD dirty-shared）+ DCT/DMT 转发 + DVM/TLB 一致性 + retry/credit 机制（`RetryAck`/`PCrdGrant`） |
| 协议选项 | `use_secondary_load_linked` 等（`MESI_Two_Level.slicc:1`） | `partial_func_reads`（`CHI.slicc:1`） |
| Machine 类型 | `L1Cache/L2Cache/L2Cache/Directory/DMA` 多种（`MESI_Two_Level-*.sm`） | 仅 3 种：`Cache` / `Memory` / `MiscNode`（`machine(MachineType:...)`；见 §2.1） |

---

## 3. 通用 `src/mem/ruby/protocol/` 协议族清单

`.slicc` 入口 + 核心 `.sm` machine 文件：

| 协议 | `.slicc` 选项 | 核心 machine 文件 |
|------|---------------|-------------------|
| `MI_example` | — | `-msg` `-cache` `-dir` `-dma` |
| `MESI_Two_Level` | `use_secondary_load_linked` | `-msg` `-L1cache` `-L2cache` `-dir` `-dma` |
| `MESI_Three_Level` | `use_secondary_load_linked, use_secondary_store_conditional, supports_flushes` | 复用 `MESI_Two_Level-msg/-L2cache/-dir/-dma` + `MESI_Three_Level-msg/-L0cache/-L1cache` |
| `MESI_Three_Level_HTM` | `use_secondary_store_conditional` | 复用 `MESI_Three_Level-*` + `MESI_Three_Level_HTM-L0cache` |
| `MOESI_CMP_directory` | — | `-msg` `-L1cache` `-L2cache` `-dma` `-dir` |
| `MOESI_CMP_token` | — | `-msg` `-L1cache` `-L2cache` `-dir` `-dma` |
| `MOESI_hammer` | — | `-msg` `-cache` `-dir` `-dma` |
| `MOESI_AMD_Base` | — | `-msg` `-CorePair` `-L3cache` `-dir`（另有 `-probeFilter`、`-Region*`，未列入 `.slicc`） |
| `GPU_VIPER` | — | 复用 `MOESI_AMD_Base-msg/-dir/-dma/-CorePair/-L3cache` + `GPU_VIPER-msg/-TCP/-SQC/-TCC` |
| `Garnet_standalone` | — | `-msg` `-cache` `-dir` |
| `CHI` | `partial_func_reads` | 见 §2.1 |
| `MULTIPLE` | （由 `MULTIPLE.py` 处理多协议） | — |

共享前缀（所有协议自动 include，`RubySlicc_interfaces.slicc:3-8`）：
`RubySlicc_Exports.sm`（含 `enumeration(MachineType)`，:268）、`RubySlicc_Types.sm`、
`RubySlicc_Util.sm`、`RubySlicc_ComponentMapping.sm`、`RubySlicc_Defines.sm`、`RubySlicc_MemControl.sm`。

---

## 4. `configs/ruby/` 协议切换 config 模板

协议选择由 `buildEnv["PROTOCOL"]` 驱动：

- `configs/ruby/Ruby.py:129` `import_module(f"ruby.{buildEnv['PROTOCOL']}").define_options(parser)` — 动态导入协议模块
- `Ruby.py:223` `create_system()` → `:253` 调用协议模块的 `create_system()`
- `Ruby.py:210` `create_topology(controllers, options)` — 构建互联拓扑
- `Ruby.py:309` `create_directories()` → `:318` `f"{options.protocol}_Directory_Controller"`（协议无关目录控制器查找）

| 模板 | 要点 |
|------|------|
| `MESI_Two_Level.py` | `create_system` :56；L1(`MESI_Two_Level_L1Cache_Controller`) + L2 + Dir + DMA；`number_of_virtual_networks=3`（:252） |
| `MOESI_hammer.py` | 增加 `ProbeFilter` 缓存 + 选项 `--allow-atomic-migration`、`--pf-on`（:61-70） |
| `CHI.py` + `CHI_config.py` | CHI 专用节点类；`CHI.py:66 create_system`，:94-101 读取 `NoC_Params`/`CHI_RNF/HNF/MN/SNF_*/RNI_*`；`number_of_virtual_networks=4`（:237） |
| `Garnet_standalone.py` | 独立网络测试协议（无 cache 一致性）：单 L1 + Dir，`create_system` :50，无 DMA（:61 assert） |
| `MOESI_CMP_directory.py` / `MOESI_CMP_token.py` / `MOESI_AMD_Base.py`（+ `AMD_Base_Constructor.py`） / `MESI_Three_Level.py` / `MESI_Three_Level_HTM.py` / `MI_example.py` / `GPU_VIPER.py` | 各自 `create_system`，控制器类名 = SLICC 生成的 `<Proto>_<Machine>_Controller` |
| `MULTIPLE.py` | 多协议共存（`PROTOCOL=MULTIPLE`） |
| `CntrlBase.py` | 控制器基类辅助 |
| `__init__.py` | 包初始化 |

`CHI_config.py` 关键：`NoC_Params`（:102，`cntrl_msg_size=8`/`data_width=32`，:113-114）、
`CHI_Node`（:119）、`Base_CHI_Cache_Controller(CHI_Cache_Controller)`（:220，直接继承 **SLICC 生成的** Python 类）、
`CHI_L1Controller/CHI_L2Controller/CHI_HNFController/CHI_MNController/CHI_DMAController`（:249/287/324/362/395）、
`CHI_RNF/HNF/MN/SNF_Base/SNF_MainMem/SNF_BootMem/RNI_Base/RNI_DMA/RNI_IO`（:476/620/687/730/790/779/803/834/845）。

---

## 5. "SLICC 生成" vs "手写 C++" 的精确边界

### 5.1 由 SLICC 生成（输出到 `build/.../mem/ruby/protocol/<Proto>/`）

每个 `machine(...)`：
- `<Machine>_Controller.hh` / `<Machine>_Controller.cc`
- `<Machine>_Transitions.cc`（`doTransition` switch）
- `<Machine>_Wakeup.cc`（`wakeup()` 事件循环）
- `<Proto>_<Machine>_Controller.py`（RubyController SimObject）

每个 `state_declaration` / `enumeration`：
- `<Machine>_State.{hh,cc}` / `<Machine>_Event.{hh,cc}` / `<Machine>_RequestType.{hh,cc}`

每个 `structure` / `enumeration`（协议内）：
- `<Type>.{hh,cc}`（如 `CHIRequestMsg`、`CHIResponseMsg`、`CHIDataMsg`、`CHIRequestType`、`CHIDataType`）

聚合与元数据：
- `<Proto>/Types.hh`（聚合头，`SymbolTable.py:158`）
- `<Proto>ProtocolInfo.hh`（`SymbolTable.py:165-212`）

### 5.2 手写 C++（不受 SLICC 生成）

`src/mem/ruby/slicc_interface/`（SLICC 运行时接口层）：
- `AbstractController.{hh,cc}` — **所有生成的 `*_Controller` 的基类**
- `AbstractCacheEntry.{hh,cc}` — cache entry 基类（`interface="AbstractCacheEntry"`）
- `Message.hh` — **所有生成 message 结构的基类**（生成代码 `: public Message`）
- `ProtocolInfo.hh` — `<Proto>ProtocolInfo` 的基类
- `RubyRequest.{hh,cc}`、`RubySlicc_Util.hh`、`RubySlicc_includes.hh`、`RubySlicc_ComponentMapping.hh`
- `Controller.py` — `RubyController` Python 基类

框架侧（生成代码调用但不生成）：
- `mem/ruby/network/MessageBuffer.{hh,cc}` — 消息队列
- `mem/ruby/system/CacheMemory`（Python 名 `RubyCache`）
- `mem/ruby/system/Sequencer`（`RubySequencer`）、`DMASequencer`
- 网络模型 `SimpleNetwork` / `GarnetNetwork`

CHI 专属手写扩展：
- `chi/generic/CHIGenericController.{hh,cc}` — **手写**控制器（`AbstractController` 派生；
  include SLICC 生成的 `CHIDataMsg.hh`/`CHIRequestMsg.hh`/`CHIResponseMsg.hh`，见 `CHIGenericController.hh:50-53`）
- `chi/tlm/*` — ARM CHI-over-TLM 手写实现（`controller.hh:41-45` include `arm_chi.h` 与生成的枚举头）
- `chi/generic/CHIGeneric.py`、`chi/tlm/*.py` — 手写/生成器结合的 Python 包装

### 5.3 边界胶水

- SLICC config 参数引用手写类名（如 `Sequencer * sequencer; CacheMemory * cache;`），
  由 `StateMachine.py:48-69` 的 `python_class_map` 映射为 Python 类型 → 生成 Python SimObject 参数。
- 生成代码通过 `#include "mem/ruby/slicc_interface/RubySlicc_includes.hh"`（`SConscript:67`）
  和共享的 `RubySlicc_*.sm` 定义的 external 类型对接手写运行时。

---

## 6. 速查：关键 file:line

- SLICC 入口 `src/mem/slicc/main.py:59`；解析器 `src/mem/slicc/parser.py:55`
- 语法规则 machine/action/port/transition `parser.py:350/358/362/370`
- Machine AST `src/mem/slicc/ast/MachineAST.py:35,63`
- C++ 生成器 `src/mem/slicc/symbols/StateMachine.py:257-262`（分派）与 `:264/:340/:595/:1482/:1642`
- 类型生成 `src/mem/slicc/symbols/Type.py:225-235`
- 符号表输出 `src/mem/slicc/symbols/SymbolTable.py:136,165`
- 构建集成 `src/mem/ruby/protocol/SConscript:47,72,104,123,165`
- CHI 入口 `src/mem/ruby/protocol/chi/CHI.slicc:1`
- CHI 消息 `chi/CHI-msg.sm:44,103,135,165,184,215`
- CHI vnet `chi/CHI-cache.sm:189-197`
- CHI 状态 `chi/CHI-cache.sm:231-285`
- CHI machine 类型 `chi/CHI-cache.sm:39` / `chi/CHI-mem.sm:39` / `chi/CHI-dvm-misc-node.sm:39`
- MESI 状态 `MESI_Two_Level-L1cache.sm:86-112`
- config 分发 `configs/ruby/Ruby.py:129,210,223,253,309`
- CHI config `configs/ruby/CHI.py:66,236` / `CHI_config.py:102,220,476`
