# GPGPU-Sim 4.2.0 的 Operand Collector 建模 — 源码剖析

> **代码根目录**: `/workspace/low/source/github/gpgpu-sim_distribution`
> **版本**: GPGPU-Sim Simulator **4.2.0**（Accel-Sim 2.0 Hopper 分支）
> **git**: `v4.2.1-67-gc3eeb6d1`（分支 `dev`，HEAD = `c3eeb6d1`，2026-10-03）
> **分析日期**: 2026-10-27
> **分析方式**: 只读源码通读 + 逐行核对（所有行号基于上述 commit）

---

## 0. 先纠正一个常见误解

**本仓库不存在 `src/cuda-sim/operand_collector.{cc,hh}` 文件，也不存在 `warp_inst.cc` / `sm_thread_manager.cc`。**

本次检索结果（全部 0 命中）：

```
find . -iname "*operand*" -o -iname "*collector*"
    → 仅 cutlass-example/cutlass/gemm/gemm_operand.h
grep -rn "register_file_cache|memory_operand_collector|operand_collector\.(cc|h)"
    → 0 命中
grep -rn "register_read_port|writeback_port|dispatch_thread|process_operands|operand_ready"
    → 0 命中
git log --all --name-only | grep -i operand_collector
    → 0 命中（历史上从未存在该文件）
```

**OC 实现全部内联在 `src/gpgpu-sim/shader.h`（类声明）+ `src/gpgpu-sim/shader.cc`（实现）**。

同样不存在的概念（关键词已逐一检索）：

| 关键词 | 命中 |
|--------|------|
| `reuse cache` / `operand reuse` / `operand cache` / `source-selector` | **0** |
| `issue_active_cycles` / `issue_stall_cycles` | **0**（对应概念是 `-gpgpu_max_insn_issue_per_warp` + `-gpgpu_dual_issue_diff_exec_units`） |
| `outstanding_entries` / `operand_collector_num_outstanding` | **0** |

> **结论**：GPGPU-Sim **完全没有**建模操作数复用缓存（reuse cache）。这是它相对 [US8639882B2](US8639882B2_源操作数收集器缓存_解析.md) 与 Volta 实测（[arXiv:1804.06826](arXiv1804.06826_Volta微基准_寄存器复用与bank冲突_解析.md)）的一处明确**建模缺口**。

---

## 1. 类结构

### 1.1 继承关系

`src/gpgpu-sim/shader.h:1011`：

```cpp
class opndcoll_base_t {
 public:
  typedef std::vector<register_set *> port_vector_t;
  typedef std::vector<unsigned int> uint_vector_t;
  virtual void add_cu_set(unsigned cu_set, unsigned num_cu, unsigned num_dispatch) = 0;
  virtual void init(unsigned num_banks, shader_core_ctx *shader) = 0;
  virtual bool writeback(warp_inst_t &warp) = 0;
  virtual void step() = 0;
  virtual void dump(FILE *fp) const = 0;
  virtual void add_port(port_vector_t &input, port_vector_t &output, uint_vector_t cu_sets) = 0;
 protected:
  class input_port_t {              // shader.h:1026
   public:
    port_vector_t m_in, m_out;      // 每个元素是一个 register_set*（一个流水级）
    uint_vector_t m_cu_sets;        // 该端口可服务的 collector set id 列表
  };
  std::vector<input_port_t> m_in_ports;
  shader_core_ctx *m_shader;
};
```

| 类 | 位置 | 语义 |
|----|------|------|
| `opndcoll_base_t` | `shader.h:1011` | 抽象基类；4 个纯虚函数 `add_cu_set` / `init` / `writeback` / `step` |
| `opndcoll_simple_t` | `shader.h:1043` | **简化模型**：单个 swap buffer，无 bank 仲裁 |
| `opndcoll_rfu_t` | `shader.h:1057` | **完整 banked-RF 模型**（register-file-unit based）——本专题重点 |

`opndcoll_simple_t` 的全部实现（`shader.cc:5932-5972`）：

```cpp
void opndcoll_simple_t::step() {                  // shader.cc:5944
  bool sub_core_model = m_shader->get_config()->sub_core_model;
  assert(m_in_ports.size() == 1);  // simple model only has one input port
  input_port_t &ports = m_in_ports[0];
  for (unsigned i = 0; i < ports.m_in.size(); i++) {
    register_set *in = ports.m_in[i];
    register_set *out = ports.m_out[i];
    ...
      if (out->has_free(sub_core_model, reg_id)) {
        in->move_out_to(sub_core_model, reg_id, m_swap_buffer);
        out->move_in(sub_core_model, reg_id, m_swap_buffer);
      }
  }
}
bool opndcoll_simple_t::writeback(warp_inst_t &warp) { return true; }  // shader.cc:5942
```

**SIMPLE 模型 = OC 透明**：无 bank 概念、无仲裁、无 stall，`writeback` 恒返回 true。

### 1.2 `opndcoll_rfu_t` 顶层成员

`shader.h:1429-1457`：

```cpp
  bool m_initialized;                     // shader.h:1430
  unsigned m_num_collector_sets;           // shader.h:1432
  unsigned m_num_banks;                    // shader.h:1434   ← RF bank 数
  unsigned m_warp_size;                    // shader.h:1435
  std::vector<collector_unit_t *> m_cu;    // shader.h:1436   ← 所有 CU（跨 set）
  arbiter_t m_arbiter;                     // shader.h:1437   ← bank 仲裁器
  unsigned m_num_banks_per_sched;          // shader.h:1439
  unsigned m_num_warp_scheds;              // shader.h:1440
  bool sub_core_model;                     // shader.h:1441
  typedef std::map<unsigned /* collector set */, std::vector<collector_unit_t>> cu_sets_t;
  cu_sets_t m_cus;                         // shader.h:1451
  std::vector<dispatch_unit_t> m_dispatch_units;  // shader.h:1452
  shader_core_ctx *m_shader;               // shader.h:1457
```

### 1.3 嵌套类 `op_t` — 单个操作数的 bank 分配请求（`shader.h:1105`）

```cpp
  class op_t {
   public:
    op_t() { m_valid = false; }
    op_t(collector_unit_t *cu, unsigned op, unsigned reg, unsigned num_banks,
         bool sub_core_model, unsigned banks_per_sched, unsigned sched_id) {
      m_valid = true; m_warp = NULL; m_cu = cu; m_operand = op; m_register = reg;
      m_shced_id = sched_id;
      m_bank = register_bank(reg, cu->get_warp_id(), num_banks, sub_core_model,
                             banks_per_sched, sched_id);
    }
   private:
    bool m_valid;                          // shader.h:1190
    collector_unit_t *m_cu;                // shader.h:1191
    const warp_inst_t *m_warp;             // shader.h:1192
    unsigned m_operand;                    // shader.h:1193  操作数在指令中的序号
    unsigned m_register;                   // shader.h:1195  寄存器号
    unsigned m_bank;                       // shader.h:1196  bank 号
    unsigned m_shced_id;                   // shader.h:1197  发出该指令的 scheduler id
  };
```

**`op_t` 不携带数据，只携带"谁要读哪个 bank 的哪个寄存器号"**。

### 1.4 嵌套类 `allocation_t` — 每个 bank 的占用态（`shader.h:1200-1239`）

```cpp
  enum alloc_t { NO_ALLOC, READ_ALLOC, WRITE_ALLOC };   // shader.h:1200-1204
  class allocation_t {
   public:
    bool is_read() const  { return m_allocation == READ_ALLOC; }
    bool is_write() const { return m_allocation == WRITE_ALLOC; }
    bool is_free() const  { return m_allocation == NO_ALLOC; }
    void alloc_read (const op_t &op) { assert(is_free()); m_allocation = READ_ALLOC;  m_op = op; }
    void alloc_write(const op_t &op) { assert(is_free()); m_allocation = WRITE_ALLOC; m_op = op; }
   private:
    enum alloc_t m_allocation;  op_t m_op;
  };
```

**一个 bank 在一拍内只能处于 3 态之一**——这是 GPGPU-Sim 的端口冲突模型的核心。

### 1.5 嵌套类 `arbiter_t` — bank × collector 二维匹配（`shader.h:1241-1334`）

```cpp
    void init(unsigned num_cu, unsigned num_banks) {   // shader.h:1253
      m_num_collectors = num_cu;  m_num_banks = num_banks;
      _inmatch = new int[m_num_banks];        // bank → 匹配到的 OC
      _outmatch = new int[m_num_collectors];  // OC  → 匹配到的 bank
      _request = new int *[m_num_banks];      // [bank][oc] 请求矩阵
      for (unsigned i = 0; i < m_num_banks; i++) _request[i] = new int[m_num_collectors];
      m_queue = new std::list<op_t>[num_banks];      // 每 bank 一个请求 FIFO
      m_allocated_bank = new allocation_t[num_banks]; // bank → 占用态
      m_allocator_rr_head = new unsigned[num_cu];
      for (unsigned n = 0; n < num_cu; n++) m_allocator_rr_head[n] = n % num_banks;
      reset_alloction();
    }

    void add_read_requests(collector_unit_t *cu) {      // shader.h:1295
      const op_t *src = cu->get_operands();
      for (unsigned i = 0; i < MAX_REG_OPERANDS * 2; i++) {
        const op_t &op = src[i];
        if (op.valid()) { unsigned bank = op.get_bank(); m_queue[bank].push_back(op); }
      }
    }
    bool bank_idle(unsigned bank) const { return m_allocated_bank[bank].is_free(); }
   private:
    unsigned m_num_banks;  unsigned m_num_collectors;
    allocation_t *m_allocated_bank;  std::list<op_t> *m_queue;
    int *_inmatch;  int *_outmatch;  int **_request;
```

**仲裁矩阵的三要素**：

| 元素 | 含义 |
|------|------|
| `m_queue[bank]` | 每个 bank 一条**请求 FIFO**（先到先服务） |
| `_request[bank][oc]` | 请求存在性矩阵（只看每 bank 队首） |
| `_inmatch[bank]` / `_outmatch[oc]` | 匹配结果 |

### 1.6 嵌套类 `collector_unit_t` — 一个 OC 槽位（`shader.h:1336-1389`）

```cpp
    collector_unit_t() {                 // shader.h:1339
      m_free = true; m_warp = NULL; m_output_register = NULL;
      m_src_op = new op_t[MAX_REG_OPERANDS * 2];      // 64 项
      m_not_ready.reset(); m_warp_id = -1; m_num_banks = 0;
    }
    bool ready() const;                               // shader.h:1349
    const op_t *get_operands() const { return m_src_op; }
    unsigned get_warp_id() const { return m_warp_id; }
    unsigned get_id() const { return m_cuid; }        // returns CU hw id
    void collect_operand(unsigned op) { m_not_ready.reset(op); }   // shader.h:1368
    bool allocate(register_set *pipeline_reg, register_set *output_reg);  // shader.h:1366
    void dispatch();                                  // shader.h:1371
    bool is_free() { return m_free; }
   private:
    bool m_free;                      // shader.h:1375
    unsigned m_cuid;                  // shader.h:1376
    unsigned m_warp_id;               // shader.h:1377
    warp_inst_t *m_warp;              // shader.h:1378
    register_set *m_output_register;  // shader.h:1379  目标流水级
    op_t *m_src_op;                   // shader.h:1381
    std::bitset<MAX_REG_OPERANDS * 2> m_not_ready;  // shader.h:1382 (64 bit)
    unsigned m_num_banks;             // shader.h:1383
    opndcoll_base_t *m_rfu;           // shader.h:1384
    unsigned m_num_banks_per_sched;   // shader.h:1386
    bool m_sub_core_model;            // shader.h:1387
    unsigned m_reg_id;                // shader.h:1388
```

**`m_not_ready` 是一个 64-bit 位图**：每个操作数一位，置 1 = 未就绪。`collect_operand(op)` 清对应位；`ready()` 判断全清。

### 1.7 嵌套类 `dispatch_unit_t` — 派发仲裁（`shader.h:1391-1427`）

```cpp
    collector_unit_t *find_ready() {                            // shader.h:1404
      unsigned cusPerSched = m_num_collectors / m_num_warp_scheds;
      unsigned rr_increment = m_sub_core_model ? cusPerSched - (m_last_cu % cusPerSched) : 1;
      for (unsigned n = 0; n < m_num_collectors; n++) {
        unsigned c = (m_last_cu + n + rr_increment) % m_num_collectors;
        if ((*m_collector_units)[c].ready()) { m_last_cu = c; return &((*m_collector_units)[c]); }
      }
      return NULL;
    }
```

**round-robin 找就绪 CU**——对应 [US7834881B2](US7834881B2_操作数收集器架构_解析.md) FIG. 5 的 **Dispatcher Unit 540**。

### 1.8 全局函数 `register_bank`（`shader.cc:5208-5219`）

```cpp
unsigned register_bank(int regnum, int wid, unsigned num_banks,
                       bool sub_core_model, unsigned banks_per_sched,
                       unsigned sched_id) {
  int bank = regnum;
  bank += wid;                                     // ← warp id 参与 bank 映射（恒定生效）
  if (sub_core_model) {
    unsigned bank_num = (bank % banks_per_sched) + (sched_id * banks_per_sched);
    assert(bank_num < num_banks);
    return bank_num;
  } else
    return bank % num_banks;
}
```

**这是 GPGPU-Sim 规避 bank 冲突的核心手法**：`bank = (reg + warp_id) % num_banks`。不同 warp 的同一寄存器号落在不同 bank——**这与 [US7834881B2](US7834881B2_操作数收集器架构_解析.md) §4.4 的 phase 错位思想同源**（相位错位 / warp id 错位）。

> ⚠ **死配置**：`-gpgpu_reg_bank_use_warp_id`（`gpu-sim.cc:525`，默认 0）**虽被注册但全仓无任何读取点**（全仓 grep 仅命中注册处 + `shader.h:2308` 字段声明）——warp id 恒定参与 bank 映射，该开关无效。

---

## 2. 数据通路（issue → 执行 → writeback）

### 2.1 流水级定义（`shader.h:2098-2119`）

```cpp
enum pipeline_stage_name_t {
  ID_OC_SP = 0,        // ← issue 侧入口 (SP)
  ID_OC_DP, ID_OC_INT, ID_OC_SFU, ID_OC_MEM,   // issue 侧入口（其余 pipe）
  OC_EX_SP, OC_EX_DP, OC_EX_INT, OC_EX_SFU, OC_EX_MEM,   // ← OC 输出侧（执行单元入口）
  EX_WB,               // ← 执行单元输出 → writeback
  ID_OC_TENSOR_CORE, OC_EX_TENSOR_CORE,
  N_PIPELINE_STAGES
};
```

**OC 的 input port = `ID_OC_*`，output port = `OC_EX_*`**。这正是 [US7834881B2](US7834881B2_操作数收集器架构_解析.md) 里 "issue → collector → execution" 的三级结构。

### 2.2 接线（`shader.cc:272-410`）

```cpp
void shader_core_ctx::create_exec_pipeline() {              // shader.cc:272
  enum { SP_CUS, DP_CUS, SFU_CUS, TENSOR_CORE_CUS, INT_CUS, MEM_CUS, GEN_CUS };
  unsigned num_sets = 0;
  if (m_config->opndcoll_model == OPNDCOLL_TYPE::DETAILED) {
    m_operand_collector = static_cast<opndcoll_base_t *>(new opndcoll_rfu_t());
    num_sets = m_config->gpgpu_operand_collector_num_in_ports_gen;
  } else {
    m_operand_collector = static_cast<opndcoll_base_t *>(new opndcoll_simple_t());
    num_sets = 1;
  }
  ...
  m_operand_collector->add_cu_set(GEN_CUS, m_config->gpgpu_operand_collector_num_units_gen,
                                  m_config->gpgpu_operand_collector_num_out_ports_gen);
  for (unsigned i = 0; i < num_sets; i++) {
    in_ports.push_back(&m_pipeline_reg[ID_OC_SP]);
    in_ports.push_back(&m_pipeline_reg[ID_OC_SFU]);
    in_ports.push_back(&m_pipeline_reg[ID_OC_MEM]);
    out_ports.push_back(&m_pipeline_reg[OC_EX_SP]);
    out_ports.push_back(&m_pipeline_reg[OC_EX_SFU]);
    out_ports.push_back(&m_pipeline_reg[OC_EX_MEM]);
    ...
    (m_operand_collector)->add_port(in_ports, out_ports, cu_sets);   // shader.cc:324
  }
  ...
  m_operand_collector->init(m_config->gpgpu_num_reg_banks, this);    // shader.cc:410
```

> **注意**：`m_dispatch_port` 数组只被 push_back、**从未被读取**（全仓 grep `m_dispatch_port[` 无命中）——是死代码。

### 2.3 完整路径

```
① Scheduler.issue()
   shader_core_ctx::issue_warp()                    shader.cc:1071
   → **pipe_reg = pipe_reg_set.get_free(...)**
     (*pipe_reg) = *next_inst;
     (*pipe_reg)->issue(active_mask, warp_id, ...);
     func_exec_inst(**pipe_reg);                    shader.cc:1087  ← 功能侧此时就算完
     m_scoreboard->reserveRegisters(*pipe_reg);     shader.cc:1282
   → warp_inst_t* 进入 m_pipeline_reg[ID_OC_*]

② OC.step()                                        shader.h:1075-1080
   void step() override {
     dispatch_ready_cu();                                    // ① 已就绪 CU → OC_EX_*
     allocate_reads();                                       // ② 仲裁器分配 bank
     for (unsigned p = 0; p < m_in_ports.size(); p++) allocate_cu(p);  // ③ 抢空闲 CU
     process_banks();                                        // ④ 清空本拍 bank 分配表
   }
   调用点：shader_core_ctx::read_operands()        shader.cc:1955-1958
     for (unsigned int i = 0; i < m_config->reg_file_port_throughput; ++i)
       m_operand_collector->step();

③ 仲裁：arbiter_t::allocate_reads()                shader.cc:4597-4680
   （BookSim wavefront 匹配，见 §3.2）

④ 回写：opndcoll_rfu_t::allocate_reads()           shader.cc:5334-5372
   → m_cu[cu]->collect_operand(operand);           清 m_not_ready 对应 bit

⑤ 派发：dispatch_ready_cu()                       shader.cc:5264-5292
   → collector_unit_t::dispatch()                  shader.cc:5445-5451
     assert(m_not_ready.none());
     m_output_register->move_in(m_sub_core_model, m_reg_id, m_warp);   // → OC_EX_*
     m_free = true;

⑥ 执行：shader_core_ctx::execute()                 shader.cc:2048-2081
   → m_fu[n]->issue(issue_inst)                    → 结果进 m_pipeline_reg[EX_WB]

⑦ 写回：shader_core_ctx::writeback()               shader.cc:2173-2224
   → m_operand_collector->writeback(*pipe_reg);    shader.cc:2211
   → m_scoreboard->releaseRegisters(pipe_reg);     shader.cc:2213
```

### 2.4 一个关键建模事实：**OC 不搬运寄存器值**

GPGPU-Sim 的 OC **只模拟端口争用与 bank 冲突带来的延迟**，不搬运数据：

- 寄存器值真源在 functional simulator 侧：`ptx_thread_info::m_regs`（`src/cuda-sim/ptx_sim.h:524-525`，`reg_map_t`），访问入口 `ptx_thread_info::set_reg`（`src/cuda-sim/instructions.cc:179`）/ `get_reg`（`:232`）；
- 指令在 **issue 当拍**就通过 `func_exec_inst` → `execute_warp_inst_t` → `ptx_exec_inst` 完成功能执行；
- `collector_unit_t::m_warp` 存的是 `warp_inst_t*` 指针，不是数据。

> **建模启示**：GPGPU-Sim 把"操作数供给"建模为**流水级占用与排队**，而非数据搬运。CppTLM 走 ChStream bundle 路线（真实搬运 bundle），数据面是真实的，**缺的是"占用/排队/冲突"这一层时序**。

### 2.5 OC 与 Scoreboard **互不引用**

- Scoreboard（`src/gpgpu-sim/scoreboard.cc`）独立做 RAW/WAW 冲突检查（`checkCollision`，`scoreboard.cc:127`）；
- `opndcoll_rfu_t` 不引用 Scoreboard；Scoreboard 也不引用 OC。

**两层完全解耦**——这与 [US7834881B2](US7834881B2_操作数收集器架构_解析.md) §4.1 专利原文（"uses a scoreboard unit ... to avoid writeback conflicts and **cycle based conflicts**"）一致。

### 2.6 `collector_unit_t::allocate()`（`shader.cc:5410-5443`）

```cpp
bool opndcoll_rfu_t::collector_unit_t::allocate(register_set *pipeline_reg_set,
                                                register_set *output_reg_set) {
  m_free = false;
  m_output_register = output_reg_set;
  warp_inst_t **pipeline_reg = pipeline_reg_set->get_ready();
  if ((pipeline_reg) and !((*pipeline_reg)->empty())) {
    m_warp_id = (*pipeline_reg)->warp_id();
    std::vector<int> prev_regs;  // remove duplicate regs within same instr
    for (unsigned op = 0; op < MAX_REG_OPERANDS; op++) {
      int reg_num = (*pipeline_reg)->arch_reg.src[op];
      bool new_reg = true;
      for (auto r : prev_regs) { if (r == reg_num) new_reg = false; }
      if (reg_num >= 0 && new_reg) {            // valid register
        prev_regs.push_back(reg_num);
        m_src_op[op] = op_t(this, op, reg_num, m_num_banks, m_sub_core_model,
                            m_num_banks_per_sched, (*pipeline_reg)->get_schd_id());
        m_not_ready.set(op);                    // ← 标记"未就绪"
      } else
        m_src_op[op] = op_t();
    }
    pipeline_reg_set->move_out_to(m_warp);       // ← warp 移入 CU
    return true;
  }
  return false;
}
```

**细节**：同一条指令内的**重复源寄存器只登记一次**（`prev_regs` 去重）——这是对 [US8639882B2](US8639882B2_源操作数收集器缓存_解析.md) 的 cache table 思想的简化体现（虽然没有缓存复用，但避免了重复读请求）。

### 2.7 `collector_unit_t::ready()`（`shader.cc:5374-5377`）

```cpp
bool opndcoll_rfu_t::collector_unit_t::ready() const {
  return (!m_free) && m_not_ready.none() &&
         (*m_output_register).has_free(m_sub_core_model, m_reg_id);
}
```

**三个条件全满足才算就绪**：① 已占用；② 所有操作数已取回；③ 目标流水级（`OC_EX_*`）有空位。

---

## 3. 端口 / 带宽 / 冲突建模

### 3.1 读端口

`-gpgpu_reg_file_port_throughput`（默认 **1**，`gpu-sim.cc:682-684`），语义 = **每拍可发起的 RF 读请求轮数**，实现为 `read_operands()` 里循环调 `step()` 的次数：

```cpp
void shader_core_ctx::read_operands() {                     // shader.cc:1955
  for (unsigned int i = 0; i < m_config->reg_file_port_throughput; ++i)
    m_operand_collector->step();
}
```

> **注意**：这不是"每拍 N 个 bank 可用"，而是"**每拍 OC 整体做 N 轮调度**"。

### 3.2 仲裁算法：BookSim wavefront 匹配

`arbiter_t::allocate_reads()`（`shader.cc:4597-4680`）核心片段：

```cpp
  ///// wavefront allocator from booksim... --->

  // Loop through diagonals of request matrix
  for (int p = 0; p < _square; ++p) {
    output = (_pri + p) % _outputs;

    // Step through the current diagonal
    for (input = 0; input < _inputs; ++input) {
      if ((output < _outputs) && (_inmatch[input] == -1) &&
          //( _outmatch[output] == -1 ) &   //allow OC to read multiple reg
          // banks at the same cycle
          (_request[input][output] /*.label != -1*/)) {
        // Grant!
        _inmatch[input] = output;
        _outmatch[output] = input;
      }
      output = (output + 1) % _outputs;
    }
  }

  // Round-robin the priority diagonal
  _pri = (_pri + 1) % _outputs;

  /// <--- end code from booksim
```

**三条关键规则**：

| 规则 | 代码位置 | 语义 |
|------|---------|------|
| **写优先** | `shader.cc:4629`：`_inmatch[i] = 0;  // write gets priority` | 已分配写的 bank，**读请求一律不服务** |
| **一个 OC 可同时读多个 bank** | `shader.cc:4646-4647`：`_outmatch[output] == -1` 检查被**注释掉** | OC 与 bank 是多对多；只有 bank 是独占的 |
| **每 bank 每拍只服务一个请求** | `_inmatch[input] == -1` | 一 bank 一拍一请求 |

**这正是 NVIDIA 真实硬件的建模**：一个 warp 指令的 3 个源操作数可以同时从 3 个不同 bank 读出（如果它们落在不同 bank），**1 拍搞定**；如果 3 个源落在同一 bank，则需 **3 拍**——与 [US7834881B2](US7834881B2_操作数收集器架构_解析.md) 的 "at least 3 clock cycles" 完全一致。

### 3.3 写端口

**没有独立的写端口计数参数**。写端口隐含在两处：

1. `arbiter_t::m_allocated_bank`（每 bank 一个 `allocation_t`），一 bank 一拍只允许 1 读或 1 写；
2. `opndcoll_rfu_t::writeback()` 的 `bank_idle(bank)` 检查 → 失败返回 false = stall。

```cpp
bool opndcoll_rfu_t::writeback(warp_inst_t &inst) {          // shader.cc:5221
  assert(!inst.empty());
  std::list<unsigned> regs = m_shader->get_regs_written(inst);
  for (unsigned op = 0; op < MAX_REG_OPERANDS; op++) {
    int reg_num = inst.arch_reg.dst[op];
    if (reg_num >= 0) {                   // valid register
      unsigned bank = register_bank(reg_num, inst.warp_id(), m_num_banks, sub_core_model,
                                    m_num_banks_per_sched, inst.get_schd_id());
      if (m_arbiter.bank_idle(bank)) {
        m_arbiter.allocate_bank_for_write(bank, op_t(&inst, reg_num, m_num_banks, ...));
        inst.arch_reg.dst[op] = -1;      // ← 标记已写，防止重复计数
      } else {
        return false;                    // ← 写端口被占 → stall
      }
    }
  }
  ...
  return true;
}
```

**写回 stall 的处理路径不同**：

| 路径 | 是否真的 stall | 出处 |
|------|---------------|------|
| 计算类 FU（SP/DP/INT/SFU/TC） | ✗ **忽略返回值**（不 stall） | `shader.cc:2211`（注释解释见 `shader.cc:2199-2209`） |
| LD/ST 单元 | ✓ **真的 stall** | `shader.cc:3043` / `shader.cc:3097` |

> `shader.cc:2199-2209` 的注释解释了为何计算类可忽略：
> > "The operand collector writeback can generally generate a stall. However, here, the pipelines should be un-stallable. This is guaranteed because this is the first time the writeback function is called after the operand collector's step function, which resets the allocations. There is one case which could result in the writeback function returning false (stall), which is when an instruction tries to modify two registers (GPR and predicate). To handle this case, we ignore the return value (thus allowing no stalling)."

### 3.4 结果总线

`EX_WB` 流水级宽度决定 **result bus 数量**（`shader.cc:473-477`）：

```cpp
  // there are as many result buses as the width of the EX_WB stage
  num_result_bus = m_config->pipe_widths[EX_WB];
```

---

## 4. 配置参数清单

### 4.1 OC 核心参数

| 参数名 | 默认 | 作用 | 出处 |
|--------|------|------|------|
| `-gpgpu_opndcoll_model` | `0` | 0=DETAILED(`opndcoll_rfu_t`) / 1=SIMPLE(`opndcoll_simple_t`) | `gpu-sim.cc:696-698`；枚举 `abstract_hardware_model.h:689`；使用 `shader.cc:278` |
| `-gpgpu_num_reg_banks` | `8` | RF bank 数（冲突粒度） | `gpu-sim.cc:521` |
| `-gpgpu_reg_file_port_throughput` | `1` | 每拍 RF 读调度轮数 | `gpu-sim.cc:682` |
| `-gpgpu_enable_specialized_operand_collector` | `1` | 专用 OC（按 pipe 分 set）vs 通用 OC | `gpu-sim.cc:530`；使用 `shader.cc:328` |
| `-gpgpu_operand_collector_num_units_sp` | `4` | SP collector unit 数 | `gpu-sim.cc:533` |
| `-gpgpu_operand_collector_num_units_dp` | `0` | DP | `gpu-sim.cc:536` |
| `-gpgpu_operand_collector_num_units_sfu` | `4` | SFU | `gpu-sim.cc:539` |
| `-gpgpu_operand_collector_num_units_int` | `0` | INT | `gpu-sim.cc:542` |
| `-gpgpu_operand_collector_num_units_tensor_core` | `4` | TC | `gpu-sim.cc:545` |
| `-gpgpu_operand_collector_num_units_mem` | `2` | MEM | `gpu-sim.cc:549` |
| `-gpgpu_operand_collector_num_units_gen` | `0` | 通用（GEN） | `gpu-sim.cc:552` |
| `-gpgpu_operand_collector_num_in_ports_*` | sp/sfu/tc/mem=1，其余 0 | OC 输入端口数 | `gpu-sim.cc:555-582` |
| `-gpgpu_operand_collector_num_out_ports_*` | sp/sfu/tc/mem=1，其余 0 | OC 输出（dispatch unit）数 | `gpu-sim.cc:583-610` |
| `-gpgpu_sub_core_model` | `0` | Volta+ sub-core：每 scheduler 独占 RF bank 段 | `gpu-sim.cc:527` |
| `-gpgpu_num_sched_per_core` | `1` | 每 SM 的 warp scheduler 数 | `gpu-sim.cc:614` |
| `-gpgpu_reg_bank_use_warp_id` | `0` | ⚠ **死配置**，全仓无读取点 | `gpu-sim.cc:525` |
| `-gpgpu_pipeline_widths` | 13 项，全 1 | 各流水级宽度 | `gpu-sim.cc:632` |

### 4.2 关联的 RF / 调度 / 功耗参数

| 参数名 | 默认 | 作用 | 出处 |
|--------|------|------|------|
| `-gpgpu_shader_registers` | — | 每 SM 寄存器总量（occupancy 限制） | `gpu-sim.cc:434` |
| `-gpgpu_shader_core_pipeline` | — | `threads:warpsize`，推导 `max_warps_per_shader` | `gpu-sim.cc:359` |
| `-gpgpu_clock_gated_reg_file` | `0` | clock-gated RF 功耗（按 active lane 计数） | `gpu-sim.cc:428`；使用 `shader.cc:5243/5271/5354` |
| `-n_regfile_gating_group` | `4` | clock gating 分组粒度 | `gpu-sim.cc:425` |
| `-gpgpu_max_insn_issue_per_warp` | `2` | 每 warp 每拍最多发几条 | `gpu-sim.cc:617`；使用 `shader.cc:1516` |
| `-gpgpu_dual_issue_diff_exec_units` | `1` | 双发射是否必须用不同执行单元 | `gpu-sim.cc:622`；使用 `shader.cc:1517-1522` |
| `-gpgpu_scheduler` | — | `lrr`/`rrr`/`gto`/`oldest`/`two_level_active`/`warp_limiting` | `shader.cc:193-205` |

### 4.3 各架构 config 实际取值

| config | num_reg_banks | port_throughput | num_units_gen | in/out_ports_gen | specialized_oc | num_sched_per_core | sub_core | opndcoll_model | max_insn_issue_per_warp |
|--------|---------------|-----------------|---------------|------------------|-----------------|--------------------|-----------|----------------|------------------------|
| SM2_GTX480 | 16 | — | — | — | — | 2 | — | — | 1 |
| SM3_KEPLER_TITAN | 16 | 2 | — | — | — | 4 | — | — | 2 |
| SM6_TITANX | 16 | 2 | 8 | 8/8 | 0 | 4 | 1 | — | 2 |
| SM7_QV100 | 16 | 2 | 8 | 8/8 | 0 | 4 | 1 | — | 1 |
| SM75_RTX2060 | 8 | 2 | 8 | 8/8 | 0 | 4 | 1 | — | 1 |
| SM80_A100 | **32** | 2 | 8 | 8/8 | 0 | 4 | 1 | —（DETAILED） | 1 |
| SM86_RTX3070 | 8 | 2 | 8 | 8/8 | 0 | 4 | 1 | — | 1 |
| **SM90_H100 / H200** | **64** | 2 | 8 | 8/8 | 0 | 4 | 1 | **1 = SIMPLE** | 1 |

### 4.4 ★ 行业事实：Hopper 配置刻意关闭 bank 冲突建模

`configs/tested-cfgs/SM90_H100/gpgpusim.config`：

```
71  -gpgpu_sub_core_model 1
72  # disable specialized operand collectors and use generic operand collectors instead
73  -gpgpu_enable_specialized_operand_collector 0
74  -gpgpu_operand_collector_num_units_gen 8
75  -gpgpu_operand_collector_num_in_ports_gen 8
76  -gpgpu_operand_collector_num_out_ports_gen 8
78  # register banks
79  -gpgpu_num_reg_banks 64
80  -gpgpu_reg_file_port_throughput 2
83  -gpgpu_num_sched_per_core 4
84  -gpgpu_scheduler lrr
85  -gpgpu_opndcoll_model 1          ← ★ SIMPLE 模型：不建模 RF bank 冲突
87  -gpgpu_max_insn_issue_per_warp 1
```

引入该开关的 commit：`e10018b6`（2026-08-25，"Accel-Sim 2.0: Hopper (H100/H200) performance model (#143)"）。

> **这是本专题最值得单独强调的行业事实**：Accel-Sim 2.0 的 Hopper 性能模型**主动选择不建模寄存器堆 bank 冲突**。方向上与 [arXiv:2503.20481](arXiv2503.20481_现代NVIDIA核心无操作数收集器_解析.md) 的实测结论一致（现代 NVIDIA 的冲突表现为 issue 侧气泡，而非 OC 内的可变延迟）。

### 4.5 AccelWattch 侧的 OC 建模

`src/accelwattch/core.h:203-226`（`RegFU` 类）：

```cpp
class RegFU : public Component {
  ...
  ArrayST *IRF;      // Integer Register File
  ArrayST *FRF;      // FP Register File
  ArrayST *RFWIN;    // RegWindow
  ArrayST *OPC;      // Operand collectors      ← core.h:215
  // OC Modelling (Syed)                          ← core.h:218
  Crossbar *xbar_rfu;                             // core.h:219
  MCPAT_Arbiter *arbiter_rfu;                     // core.h:220
```

`src/accelwattch/core.cc:1947-1992`（OPC 阵列：32-bit 宽、8 entry bank）：

```cpp
  /**
   * Operand collectors (32-bit wide, 8 entry banks )
   */
  data = 32;
  interface_ip.line_sz = 4;
  interface_ip.cache_sz = 8 * 4;
  interface_ip.assoc = 1;
  interface_ip.nbanks = 1;
  interface_ip.num_rd_ports = 1;  // 2*coredynp.peak_issueW;
  interface_ip.num_wr_ports = 1;  // coredynp.peak_issueW;
  OPC = new ArrayST(&interface_ip, "Operand collectors", Core_device, ...);
```

crossbar + arbiter 尺寸 = `rf_banks × collector_units`（`core.cc:1866-1884`）。

> **注意**：这是**功耗模型**侧的 OC 抽象，与时序侧的 `opndcoll_rfu_t` 是两套独立实现，通过 config 的 `rf_banks` / `collector_units` XML 字段（`XML_Parse.cc:1090-1107`）对齐。

---

## 5. 硬件对照与源码注释摘录

### 5.1 唯一一处把 OC 与 NVIDIA 硬件直接关联的注释

`src/accelwattch/core.cc:53-57`：

```cpp
//*********************
//  Operand collector (OC) modelling (Syed Gilani)
//*********************
//  The OCs are modelled similar to the GPGPU-Sim v3.x documentation and
//  nVIDIA patents.
```

> 原文逐字：「The OCs are modelled similar to the GPGPU-Sim v3.x documentation and **nVIDIA patents**.」
>
> 这是全仓唯一一处把 OC 与 NVIDIA 硬件直接关联的注释。**但它没有给出专利号**，也没引用任何 microbenchmark 结论。

### 5.2 OC 的历史沿革（`CHANGES`）

```
Version 3.0.0b versus 2.1.2b
- massive refactoring of code to C++ (most global variables eliminated)
- support for executing native SASS code via decuda
- added to timing model:
   - in order scoreboard allowing multiple instructions per warp in pipeline at a time
   - operand collector for modeling access to banked register files      ← CHANGES:521
   - prefetching texture cache model (Igehy et al., Graphics Hardware 1998)
```

```
- Register bank conflict modeling (see MICRO-42 tutorial)              ← CHANGES:536
```

```
- Added a missed call to operand collector writeback which fixes some
  writebacks that were not considered by the operand collector.          ← CHANGES:417-418
```

### 5.3 建模假设注释（原文）

`shader.cc:2188-2192`：
> "Right now, the writeback stage drains all waiting instructions assuming there are enough ports in the register file or the conflicts are resolved at issue."

`shader.cc:1519-1522`（双发射的架构对照）：
> "In tis mode, we only allow dual issue to diff execution units (as in Maxwell and Pascal)"

`shader.cc:1262`：
> "Leave it here as for future finer timing model we might need this"

---

## 6. 版本与支持架构

| 项 | 值 |
|----|-----|
| 声明版本 | `GPGPU-Sim Simulator Version 4.2.0`（`version:1`） |
| CMake project version | `4.2.0`（`CMakeLists.txt:4-8`） |
| git describe | `v4.2.1-67-gc3eeb6d1`（分支 `dev`） |
| HEAD commit | `c3eeb6d1`（2026-10-03） |
| 上游主页 | https://github.com/accel-sim/gpgpu-sim_distribution |
| CUDA 支持 | 4.2 / 5.0 / 5.5 / 6.0 / 7.5 / 8.0 / 9.0 / 9.1 / 10 / 11 / 12（`README.md:31-32`） |

**支持的 SM 架构**（按 `configs/` 目录）：sm_10 / sm_12 / sm_13 / sm_20 / sm_30 / sm_35 / sm_50 / sm_52 / sm_60 / sm_61 / sm_70 / sm_75 / sm_80 / sm_86 / sm_90。

**关键历史 commit**：

| commit | 日期 | 说明 |
|--------|------|------|
| `80e1b49f` | 2010 | "modified operand collector with parallel ALU/SFU pipelines" |
| `b0cf7929` | 2010 | "re-enabling operand collector writeback for all instruction types" |
| `4fce546c` | 2010 | "Removing option to have an operand collector -- now you MUST have the operand collector." |
| `8da76f1d` | 2012-05-07 | "A configurable number of result buses is added ... Modified the Fermi config file to add two ports to the operand collector" |
| `92f313b6` | — | "fix sub-core operand collector dispatch rr" |
| `e10018b6` | 2026-08-25 | "Accel-Sim 2.0: Hopper (H100/H200) performance model (#143)" — 引入 `opndcoll_model` 开关 + SM90 config |

---

## 7. 与专利/论文的逐点对照

| 机制 | NVIDIA 专利/实测 | GPGPU-Sim 实现 | 差距 |
|------|-----------------|----------------|------|
| bank 划分 | [US7834881B2](US7834881B2_操作数收集器架构_解析.md) FIG.3：每 bank 1R1W | `allocation_t` 每 bank 一拍 1 读或 1 写 | ✅ 一致 |
| 收集延迟 | 同 bank 3 源 → ≥3 拍 | wavefront 匹配，同 bank 请求串行 → 3 拍 | ✅ 一致 |
| 一个 OC 多 bank 同拍 | 专利：Selector/Crossbar 允许 | `_outmatch` 检查被注释掉 | ✅ 一致 |
| 写优先 | 专利未明说 | `_inmatch[i] = 0; // write gets priority` | 模拟器自加 |
| 写回冲突避让 | 专利 §4.6：目标寄存器被高优先级 EU 占用 → **换一条指令** | ✗ **未实现**（只做 bank 空闲检查） | ⚠ **差距** |
| 常量 cache miss 丢弃重选 | 专利 FIG.8：**丢弃整条读请求**改选另一条 | ✗ 未实现 | ⚠ **差距** |
| 操作数复用缓存 | [US8639882B2](US8639882B2_源操作数收集器缓存_解析.md)：6 槽位 + cache table | ✗ **未实现**（`reuse cache` 0 命中） | ⚠ **差距** |
| phase / warp id 错位 | 专利：XOR 或 mod phase | `register_bank()`：`bank = (reg + warp_id) % num_banks` | ✅ 同源思想 |
| 定长延迟（Volta+） | 实测：3 周期固定，冲突→issue 气泡 | ✗ 建模为**可变延迟**（OC 内等待） | ⚠ **口径差异** |
| Hopper 实际行为 | 实测方向：无 OC | ✓ **sm_90 走 SIMPLE 模型** | ✅ 已对齐 |

---

## 8. 对 CppTLM 的可借鉴点（具体到类与字段）

CppTLM 现状对照：

| 文件 | 现状 | 缺什么 |
|------|------|-------|
| `include/tlm/gpu/sm/reg_file.hh` | `cpptlm::gpu::RegFileUnit`：flat `unordered_map<uint64_t,uint64_t>`，key = `(warp_id<<32)\|reg_id`；`write()` / `read()` **无节流直读写** | **无 bank / 无端口 / 无冲突概念** |
| `include/tlm/gpu/sm/issue_unit_tlm.hh` | `tlm::sm::IssueUnitTLM`：round-robin 调 warp，`tick()` 直接产出 `IssueToExecBundle` | **无 "issue 后 / 执行前" 中间站** |
| `include/tlm/gpu/sm/writeback.hh` | `cpptlm::gpu::WritebackUnit`：in-flight 队列 | **纯 FIFO，无节拍冲突** |
| `include/tlm/gpu/sm/hazard_tracker.hh` | vgpr 占用位图，解决 RAW | 与端口争用**混在一起的风险** |
| `include/bundles/sm_bundles_tlm.hh` | `IssueToExecBundle`：`src_values[2]` / `src_valid[2]` 纯值数组 | **无寄存器号 → bank 映射** |

### 建议 1：给 `RegFileUnit` 加 bank 维度 + 冲突仲裁器

参照 `opndcoll_rfu_t::arbiter_t`（`shader.h:1241`）与 `register_bank()`（`shader.cc:5208`）：

```cpp
// 伪代码（CppTLM 侧建议，非最终 API）
class RegFileUnit {
  unsigned num_banks_ = 8;
  std::vector<BankAlloc> m_allocated_bank_;   // 每 bank: NONE / READ / WRITE

  static unsigned register_bank(uint32_t regnum, uint32_t warp_id, unsigned num_banks) {
    return (regnum + warp_id) % num_banks;      // 照抄 gpgpu-sim
  }
  // read() 增加 bank 占用检查；write() 同样
};
```

### 建议 2：把"数据冒险"与"端口争用"分层

GPGPU-Sim 中 Scoreboard（RAW）与 OC（bank/端口）是**两套互不引用的机制**；[US7834881B2](US7834881B2_操作数收集器架构_解析.md) 专利原文也把 "writeback conflicts" 与 "cycle based conflicts" 并列。CppTLM 应保持 `HazardTracker` 不变，另建独立的 bank/端口仲裁器。

### 建议 3：引入"已 issue 但操作数未齐"的中间态

GPGPU-Sim 用 `m_pipeline_reg[ID_OC_*]` → OC → `m_pipeline_reg[OC_EX_*]` 表达该在飞态。CppTLM 可在 `sm_bundles_tlm.hh` 增加 `IssueToOperandCollectBundle`（或给 `IssueToExecBundle` 加 `collected` 标志位），并在 `StreamingMultiprocessorTLM::exe_once()` 的 tick 链中插一段 OC tick。

### 建议 4：写端口 stall 语义

`WritebackUnit::tick()` 增加"本拍可接受的写次数 = num_write_ports_"与"目标 bank 已被占 → 延后一拍"。这正是 gpgpu-sim `opndcoll_rfu_t::writeback()` 的 `bank_idle()` 语义。

### 建议 5：bundle 需携带 bank 元数据

`IssueToExecBundle::src_values[2]` 目前是纯值数组，无法建模"两条源落同 bank → 冲突"。需补 `src_regs[2]` / `src_banks[2]`，由 `IssueUnitTLM` 在调度时按 `register_bank()` 计算。

### 建议 6：配置项命名对标，便于回归迁移

建议 CppTLM config 引入与 gpgpu-sim 同名键：`num_reg_banks`（sm_90 用 64）、`reg_file_port_throughput`（2）、`num_collector_units`（8）、`opndcoll_model`（DETAILED/SIMPLE 二值）。同时**明确记录** gpgpu-sim 在 Hopper 上刻意选择 SIMPLE 模型这一取舍。

---

*本剖析基于 2026-10-27 对 `/workspace/low/source/github/gpgpu-sim_distribution`（git `c3eeb6d1`，v4.2.1-67）的只读源码通读。所有行号基于该 commit；未修改任何文件。*
