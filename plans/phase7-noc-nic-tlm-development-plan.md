# Phase 7 NoC/NIC TLM 模块开发计划（对齐 V2.1 架构）

**版本**: 2.1（审查修正版）
**状态**: 已修正
**日期**: 2026-04-23
**作者**: Sisyphus

---

## 一、架构对齐：现有 TLM 模块模式

### 1.1 现有 TLM 模块继承体系

```
SimObject
└── ChStreamModuleBase (include/core/chstream_module.hh)
    ├── CacheTLM     (单端口: req_in_/resp_out_)
    ├── MemoryTLM    (单端口: req_in_/resp_out_)
    ├── CrossbarTLM (多端口: req_in[4]/resp_out[4])
    └── ...

// Phase 7 新增
├── RouterTLM      (多端口: 5 方向 N/E/S/W/Local)
└── NICTLM         (双端口非对称: PE侧 + Network侧)
```

### 1.2 StreamAdapter 架构对应

| 模块类型 | Adapter 类型 | 模板参数 | 端口模式 |
|----------|-------------|----------|----------|
| CacheTLM/MemoryTLM | `StandaloneStreamAdapter` | `StreamAdapter<Mod, ReqT, RespT>` | 单端口 |
| CrossbarTLM | `MultiPortStreamAdapter` | `MultiPortStreamAdapter<Mod, ReqT, RespT, N>` | N 端口 |
| **RouterTLM** | `**RouterPortAdapter**` | `RouterPortAdapter<Mod, FlitBundleT, N>` | N 端口统一类型 |
| NICTLM | `DualPortStreamAdapter` | `DualPortStreamAdapter<Mod, PE_ReqT, PE_RespT, Net_ReqT, Net_RespT>` | 双端口组 |

### 1.3 NoC Bundle 设计决策

> **关键决策 (v2.1)**: RouterTLM 所有端口使用统一的 `NoCFlitBundle` 类型，而非分离的 `NoCReqBundle`/`NoCRespBundle`。
> 原因：Router 只做转发，不感知请求/响应语义，统一 Bundle 类型简化端口设计和流水线处理。

### 1.4 模块注册模式

```cpp
// Phase 7 RouterTLM: 使用 RouterPortAdapter (专用适配器)
ChStreamAdapterFactory::get().registerRouterAdapter<RouterTLM, bundles::NoCFlitBundle, 5>("RouterTLM");

// Phase 7 NICTLM: 使用 DualPortStreamAdapter
ChStreamAdapterFactory::get().registerDualPortAdapter<NICTLM,
    CacheReqBundle, CacheRespBundle, NoCFlitBundle, NoCFlitBundle>("NICTLM");
// 注意: NICTLM 网络侧使用统一的 NoCFlitBundle
```

---

## 二、架构设计：NoC 模块

### 2.0 NoC Flit 生成与恢复机制（关键设计）

> **重要更新 (2026-04-23)**: 根据审查报告 v2.1 修正以下问题：
> 1. **路由器端口双 Bundle 类型问题**：统一为单一 NoCFlitBundle 类型
> 2. **文档清理**：删除重复的 RouterFlit 定义和 Phase 7.3 旧版本
> 3. **细化 DualPortStreamAdapter tick() 改法**：明确 net_resp_in 处理方式

#### 2.0.1 问题背景

```
CPU                NICTLM                Router                Memory              NICTLM                CPU
 │                    │                     │                     │                    │                    │
 │ CacheReq(32B)     │                     │                     │                    │                    │
 │──────────────────►│                     │                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │ NoCReq(8B) idx=0/4  │                     │                    │                    │
 │                    │────────────────────►│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │ NoCReq(8B) idx=1/4  │                     │                    │                    │
 │                    │────────────────────►│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │ NoCReq(8B) idx=2/4  │                     │                    │                    │
 │                    │────────────────────►│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │ NoCReq(8B) idx=3/4  │                     │                    │                    │
 │                    │────────────────────►│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │                     │    CacheReqBundle   │                    │                    │
 │                    │                     │────────────────────►│                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │                     │    CacheRespBundle  │                    │                    │
 │                    │                     │◄────────────────────│                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │    NoCResp idx=0/4   │                     │                    │                    │
 │                    │◄─────────────────────│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │    NoCResp idx=1/4   │                     │                    │                    │
 │                    │◄─────────────────────│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │    NoCResp idx=2/4   │                     │                    │                    │
 │                    │◄─────────────────────│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │    NoCResp idx=3/4   │                     │                    │                    │
 │                    │◄─────────────────────│                     │                    │                    │
 │                    │                     │                     │                    │                    │
 │                    │ [Reassembly: 4×8B → 32B]                   │                    │                    │
 │                    │                     │                     │                    │                    │
 │ CacheRespBundle   │                     │                     │                    │                    │
 │◄──────────────────│                     │                     │                    │                    │
```

#### 2.0.2 NoCReqBundle 设计（修正版）

```cpp
struct NoCReqBundle : public bundle_base {
    ch_uint<64> transaction_id;   // 关联原始 CacheReq.transaction_id
    ch_uint<32> src_node;        // 源节点 ID (NICTLM 节点号)
    ch_uint<32> dst_node;        // 目标节点 ID
    ch_uint<64> address;         // 访问地址
    ch_uint<64> data;            // 数据载荷 (8 字节/拍)
    ch_uint<8>  size;            // 原始请求总大小 (字节)
    ch_uint<8>  vc_id;           // 虚拟通道 ID (0-3)
    ch_uint<8>  flit_type;       // 0=HEAD, 1=BODY, 2=TAIL, 3=HEAD_TAIL
    ch_uint<8>  flit_index;      // 当前 flit 索引 (0, 1, 2, ...)
    ch_uint<8>  flit_count;      // 总 flit 数量 (1, 2, 3, ...)
    ch_uint<8>  hops;            // 已跳数计数
    ch_bool     is_write;         // 读/写标记

    NoCReqBundle() = default;

    static constexpr uint8_t FLIT_HEAD     = 0;
    static constexpr uint8_t FLIT_BODY     = 1;
    static constexpr uint8_t FLIT_TAIL     = 2;
    static constexpr uint8_t FLIT_HEAD_TAIL = 3;
};
```

**关键字段说明**:

| 字段 | 用途 |
|------|------|
| `flit_index` | 当前 flit 在分组中的索引位置，接收端据此判断是第几个分片 |
| `flit_count` | 预知的总分片数量，接收端据此判断收齐是否完成 |
| `transaction_id` | 关联原始 CacheReq，响应返回时用于匹配请求 |
| `size` | 原始请求数据总大小（字节），用于计算 flit_count |

#### 2.0.3 NoCRespBundle 设计（修正版）

```cpp
struct NoCRespBundle : public bundle_base {
    ch_uint<64> transaction_id;   // 关联原始请求的 transaction_id
    ch_uint<64> data;            // 响应数据载荷 (8 字节/拍)
    ch_uint<32> src_node;        // 响应源节点 (Memory 节点号)
    ch_uint<32> dst_node;        // 响应目标节点 (NICTLM 节点号)
    ch_uint<8>  flit_type;        // 0=HEAD, 1=BODY, 2=TAIL, 3=HEAD_TAIL
    ch_uint<8>  flit_index;       // 当前 flit 索引
    ch_uint<8>  flit_count;       // 总 flit 数量
    ch_uint<8>  hops;            // 跳数统计
    ch_bool     is_ok;            // 成功/失败
    ch_uint<8>  error_code;       // 错误码 (0=OK)

    NoCRespBundle() = default;

    static constexpr uint8_t FLIT_HEAD     = 0;
    static constexpr uint8_t FLIT_BODY     = 1;
    static constexpr uint8_t FLIT_TAIL     = 2;
    static constexpr uint8_t FLIT_HEAD_TAIL = 3;
};
```

#### 2.0.4 NICTLM 包化逻辑 (Packetization)

```cpp
// NICTLM::packetize() - 将 CacheReqBundle 切分为 NoCFlitBundle Flits
void NICTLM::packetize(const bundles::CacheReqBundle& req) {
    uint64_t total_size = req.size.read();
    uint8_t flit_count = static_cast<uint8_t>((total_size + 7) / 8);  // 向上取整
    if (flit_count == 0) flit_count = 1;  // 最小 1 个 flit

    uint64_t addr = req.address.read();
    uint64_t data = req.data.read();

    for (uint8_t i = 0; i < flit_count; ++i) {
        bundles::NoCFlitBundle flit;  // v2.1: 统一使用 NoCFlitBundle

        // HEAD flit: 包含路由信息和事务 ID
        flit.transaction_id.write(req.transaction_id.read());
        flit.src_node.write(node_id_);
        flit.dst_node.write(computeDstNode(addr));  // 根据地址计算目标节点
        flit.address.write(addr);
        flit.size.write(total_size);
        flit.vc_id.write(selectVC(addr));            // 选择 VC
        flit.flit_index.write(i);
        flit.flit_count.write(flit_count);
        flit.hops.write(0);
        flit.is_write.write(req.is_write.read());
        flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);  // v2.1: REQUEST

        // 数据载荷: 每次取 8 字节
        uint64_t word = (data >> (i * 64)) & 0xFFFFFFFFFFFFFFFFULL;
        flit.data.write(word);

        // Flit 类型
        if (flit_count == 1) {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
        } else if (i == 0) {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD);
        } else if (i == flit_count - 1) {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_TAIL);
        } else {
            flit.flit_type.write(bundles::NoCFlitBundle::FLIT_BODY);
        }

        // 发送 flit
        net_req_out_.write(flit);
        ++stats_flits_sent_;
    }
}
```

#### 2.0.5 NICTLM 反包化逻辑 (Reassembly)

```cpp
// NICTLM 内部状态: 正在组装的响应上下文
struct RespAssemblyContext {
    uint64_t transaction_id;
    uint8_t flit_count;       // 预知的总分片数
    uint8_t flits_received;   // 已收到的分片数
    std::vector<uint64_t> data_words;  // 累积的数据字
    bool complete;

    // 响应元数据 (从 HEAD flit 提取)
    ch_bool is_ok;
    ch_uint<8> error_code;
};

// NICTLM::reassemble() - 将 NoCFlitBundle (RESPONSE) Flits 重组为 CacheRespBundle
void NICTLM::reassemble(const bundles::NoCFlitBundle& flit) {
    uint64_t tid = flit.transaction_id.read();

    // 获取或创建组装上下文
    auto& ctx = resp_contexts_[tid];
    if (ctx.flits_received == 0) {
        // 第一个 flit (HEAD 或 HEAD_TAIL)，初始化上下文
        ctx.flit_count = flit.flit_count.read();
        ctx.transaction_id = tid;
        ctx.data_words.clear();
        ctx.is_ok = flit.is_ok;
        ctx.error_code = flit.error_code;
    }

    // 累积数据字
    ctx.data_words.push_back(flit.data.read());
    ctx.flits_received++;

    DPRINTF(NIC, "[%s] Reassemble tid=%lu: flit %u/%u\n",
            name().c_str(), tid, ctx.flits_received, ctx.flit_count);

    // 检查是否收齐所有 flits
    if (ctx.flits_received == ctx.flit_count) {
        ctx.complete = true;

        // 组装 CacheRespBundle
        bundles::CacheRespBundle resp;
        resp.transaction_id.write(tid);

        // 合并多个数据字为完整数据
        uint64_t full_data = 0;
        for (uint8_t i = 0; i < ctx.data_words.size(); ++i) {
            full_data |= (ctx.data_words[i] << (i * 64));
        }
        resp.data.write(full_data);
        resp.is_hit.write(ctx.is_ok ? 1 : 0);
        resp.error_code.write(ctx.error_code.value_);

        // 发送给 PE 侧
        pe_resp_out_.write(resp);
        ++stats_pkts_received_;

        DPRINTF(NIC, "[%s] Reassembled complete tid=%lu, sent to PE\n",
                name().c_str(), tid);

        // 清理上下文
        resp_contexts_.erase(tid);
    }
}
```

#### 2.0.6 RouterTLM 六阶段流水线

### 2.1 RouterTLM 模块（5 端口多跳路由器）

**文件**: `include/tlm/router_tlm.hh`

> **端口模型修正**: RouterTLM 每个端口需要双向通信能力（既接收请求也发送响应），因此不使用 `MultiPortStreamAdapter`，而是为每个端口定义 `req_in` + `resp_out` 端口对。

```cpp
class RouterTLM : public ChStreamModuleBase {
private:
    // ========== 拓扑配置 ==========
    static constexpr unsigned NUM_PORTS = 5;  // N, E, S, W, Local
    static constexpr unsigned NUM_VCS = 4;    // 每端口虚拟通道数
    static constexpr unsigned BUFFER_DEPTH = 8;  // 每 VC 缓冲深度

    // ========== 节点坐标 (Mesh 拓扑) ==========
    uint32_t node_x_ = 0;   // 本节点 X 坐标
    uint32_t node_y_ = 0;   // 本节点 Y 坐标
    uint32_t mesh_size_x_ = 2;  // Mesh X 维度
    uint32_t mesh_size_y_ = 2;  // Mesh Y 维度

    // ========== 路由算法 (可扩展) ==========
    class RoutingAlgorithm {
    public:
        virtual ~RoutingAlgorithm() = default;
        // 输入: 源端口、目标节点坐标、Mesh 大小
        // 输出: 输出端口索引
        virtual unsigned computeRoute(unsigned src_port,
                                      uint32_t dst_x, uint32_t dst_y,
                                      uint32_t mesh_x, uint32_t mesh_y) = 0;
    };
    std::unique_ptr<RoutingAlgorithm> routing_algo_;

    // ========== BidirectionalPortAdapter (每个端口双向) ==========
    cpptlm::StreamAdapterBase* adapters_[NUM_PORTS] = {nullptr};

    // ========== 端口声明: 每个端口双向 (req_in + resp_out) ==========
    // 注意: MultiPortStreamAdapter 不适用于 RouterTLM，因为路由器需要双向通信
    // v2.1 修正: 统一使用 NoCFlitBundle 类型（不再分离 req/resp）
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>   req_in_[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>  resp_out_[NUM_PORTS];

    // ========== 内部 Flit 结构 ==========
    struct RouterFlit {
        bundles::NoCFlitBundle bundle;   // 原始 Bundle 数据（统一类型）
        uint8_t vc_id;                  // 当前占用 VC
        uint8_t out_port;               // 分配的输出端口
        uint8_t out_vc;                 // 分配的输出 VC
        bool routed;                    // 是否已路由 (HEAD flit 已处理)
        bool allocated;                  // 是否已分配 VC
        uint64_t inject_time;            // 注入时间戳 (用于延迟统计)

        RouterFlit(const bundles::NoCFlitBundle& b, uint64_t t)
            : bundle(b), vc_id(b.vc_id.read()), out_port(0), out_vc(0),
              routed(b.flit_type.read() != bundles::NoCFlitBundle::FLIT_HEAD),
              allocated(routed), inject_time(t) {}
    };

    // ========== 输入缓冲区: input_buffer[port][vc] ==========
    std::queue<RouterFlit> input_buffer_[NUM_PORTS][NUM_VCS];

    // ========== 路由状态表: packet_id → {out_port, out_vc} ==========
    struct RoutingState {
        uint8_t out_port;
        uint8_t out_vc;
        bool valid;
    };
    std::unordered_map<uint64_t, RoutingState> routing_table_;

    // ========== Credit-based Flow Control ==========
    struct VCCredit {
        uint8_t count;        // 可用 credit 数量
        uint8_t max_credits;  // 最大 credit (buffer depth)
    };
    VCCredit downstream_credits_[NUM_PORTS][NUM_VCS];  // 下游返回的 credit
    VCCredit local_credits_[NUM_PORTS][NUM_VCS];       // 本地 VC credit

    // ========== 性能统计 ==========
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_flits_received_;
    tlm_stats::Scalar& stats_flits_forwarded_;
    tlm_stats::Scalar& stats_credits_returned_;
    tlm_stats::Distribution& stats_hop_latency_;
    tlm_stats::Distribution& stats_congestion_cycles_;

public:
    RouterTLM(const std::string& name, EventQueue* eq);
    ~RouterTLM() override = default;

    std::string get_module_type() const override { return "RouterTLM"; }

    // ========== ChStreamModuleBase 接口 ==========
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    unsigned num_ports() const override { return NUM_PORTS; }

    // ========== 业务逻辑 (六阶段流水线) ==========
    void tick() override;
    void do_reset(const ResetConfig&) override;

    // ========== 路由算法 ==========
    void setRoutingAlgorithm(std::unique_ptr<RoutingAlgorithm> algo) {
        routing_algo_ = std::move(algo);
    }

    // ========== VC 管理 (Credit-based) ==========
    int allocateVC(unsigned port, uint8_t requested_vc);
    void releaseVC(unsigned port, uint8_t vc);
    bool canSend(unsigned port) const;  // 检查是否有可用 credit

    // ========== Credit 信号处理 ==========
    void receiveCredit(uint8_t port, uint8_t vc);  // 从下游接收 credit
    void sendCredit(uint8_t port, uint8_t vc);     // 向上游发送 credit

    // ========== 统计 ==========
    tlm_stats::StatGroup& stats() { return stats_; }
    void dumpStats(std::ostream& os) const;

private:
    // 六阶段流水线
    void processBufferWrite();      // BW: 接收 flit 到 input_buffer
    void processRouteComputation(); // RC: HEAD flit 路由计算
    void processVCAllocation();     // VA: VC 分配
    void processSwitchAllocation();  // SA: 交叉开关仲裁
    void processSwitchTraversal();   // ST: 数据转发
    void processLinkTraversal();    // LT: 链路传输 (建模链路延迟)

    // 辅助
    unsigned xyRoute(uint32_t dst_x, uint32_t dst_y);
    void forwardFlit(RouterFlit& flit, unsigned out_port);
};

// ========== XY 路由实现 (默认) ==========
class XYRouting : public RouterTLM::RoutingAlgorithm {
public:
    unsigned computeRoute(unsigned src_port,
                          uint32_t dst_x, uint32_t dst_y,
                          uint32_t mesh_x, uint32_t mesh_y) override {
        // XY Dimension-Order Routing: 先 X 方向，再 Y 方向
        // 端口映射: 0=N(-Y), 1=E(+X), 2=S(+Y), 3=W(-X), 4=Local
        uint32_t cur_x = /* router's x */ 0;  // 需要从 RouterTLM 获取
        uint32_t cur_y = /* router's y */ 0;

        if (dst_x > cur_x) return 1;  // East
        if (dst_x < cur_x) return 3;  // West
        if (dst_y > cur_y) return 2;  // South
        if (dst_y < cur_y) return 0;  // North
        return 4;  // Local (本节点)
    }
};
```

**端口方向映射**:

| 端口索引 | 方向 | 说明 |
|----------|------|------|
| 0 | North | −Y 方向 |
| 1 | East | +X 方向 |
| 2 | South | +Y 方向 |
| 3 | West | −X 方向 |
| 4 | Local | 连接 NICTLM |

**关键设计决策**:

1. **BidirectionalPort 模式**: 每个端口有 `req_in` (输入) + `resp_out` (输出)，而非 MultiPortStreamAdapter 的单向模式
2. **Credit-based Flow Control**: 每个 VC 有独立的 credit 计数器，发送 flit 时消耗 credit，收到 credit 返回时恢复
3. **RoutingAlgorithm 接口**: 可插拔的路由算法，XY 路由是默认实现，未来可扩展为 Odd-Even、自适应路由等

---

## 三、开发阶段详细计划

> **审查修正 (v2.1)**: 根据审查报告，新增 Phase 7.0 框架准备阶段，修正 RouterTLM 为 BidirectionalPort 模式，添加 LT 阶段和 Credit-based Flow Control。

### Phase 7.0: 框架准备 (Week 0-1)

| 步骤 | 任务 | 说明 |
|------|------|------|
| 7.0-1 | 验证 DualPortStreamAdapter 端口访问器约定 | 确认 4 个访问器命名: pe_req_in, pe_resp_out, net_req_out, net_resp_in |
| 7.0-2 | 创建 BidirectionalPortAdapter | RouterTLM 每个端口需要 req_in + resp_out 双向能力 |
| 7.0-3 | 添加 RoutingAlgorithm 接口 | 为路由算法扩展预留接口 |
| 7.0-4 | 添加 LinkTLM 模块 (可选) | 独立建模链路延迟 |

### Phase 7.1: NoC Bundle 定义 (Week 1) ✅ 已完成

| 步骤 | 任务 | 文件 | 验收标准 | 状态 |
|------|------|------|----------|------|
| 7.1-1 | 创建 `NoCFlitBundle` (统一类型) | `include/bundles/noc_bundles_tlm.hh` | Bundle 定义完成，编译通过 | ✅ |
| 7.1-2 | 单元测试: Bundle 字段读写 | `test/test_noc_bundles.cc` | 字段 `.read()`/`.write()` 正确 | ✅ |
| 7.1-3 | 单元测试: Flit 类型常量 | `test/test_noc_bundles.cc` | `FLIT_HEAD/TAIL/BODY/HEAD_TAIL` 值正确 | ✅ |

**输出**: `include/bundles/noc_bundles_tlm.hh` + `test/test_noc_bundles.cc`
**测试结果**: 12 测试用例, 77 断言, 全部通过 (2026-04-23)

**实现状态 (2026-04-23)**:
- ✅ `NoCFlitBundle` 结构完整 (137 行)
  - 14 个字段 (transaction_id, src/dst_node, address, data, size, vc_id, flit_type, flit_index, flit_count, hops, flit_category, is_write, is_ok, error_code)
  - 6 个常量 (FLIT_HEAD, FLIT_BODY, FLIT_TAIL, FLIT_HEAD_TAIL, CATEGORY_REQUEST, CATEGORY_RESPONSE)
  - 4 个辅助方法 (is_head, is_tail, is_request, is_response)
  - 2 个工厂方法 (make_head, make_resp_head)
- ✅ `test_noc_bundles.cc` 完整测试覆盖 (215 行)
- ✅ 全量测试通过: 398 测试用例, 13976 断言, 零回归

### Phase 7.2: RouterTLM 核心 (Week 2-3)

| 步骤 | 任务 | 文件 | 验收标准 |
|------|------|------|----------|
| 7.2-1 | 创建 RouterTLM 头文件骨架 | `include/tlm/router_tlm.hh` | 类定义 + 5 端口声明 + 继承正确 |
| 7.2-2 | 实现 XY 路由算法 | `include/tlm/router_tlm.hh` (inline) | `computeXYRoute()` 测试 XY 路由正确 |
| 7.2-3 | 实现输入缓冲区管理 | `include/tlm/router_tlm.hh` | VC 分配/释放正确 |
| 7.2-4 | 实现 tick() 六阶段流水线 | `src/tlm/router_tlm.cc` | BW→RC→VA→SA→ST→LT 完整 |
| 7.2-5 | 实现 Credit-based Flow Control | `include/tlm/router_tlm.hh` | credit 消耗/返回正确 |
| 7.2-6 | 实现 RoutingAlgorithm 接口 | `include/tlm/router_tlm.hh` | XY/OddEven 可切换 |
| 7.2-7 | 性能统计集成 | `include/tlm/router_tlm.hh` | `stats_` 记录 flits/hop latency/congestion |
| 7.2-8 | 注册到 `REGISTER_CHSTREAM` | `include/chstream_register.hh` | `RouterTLM` 可被 ModuleFactory 创建 |

**RouterTLM tick() 六阶段流水线**:

```
每周期执行:
┌─────────────────────────────────────────────────────────────────────┐
│ 1. Buffer Write (BW)                                               │
│    - 检查 req_in[port].valid() && req_in[port].ready()             │
│    - 读取 flit，存入 input_buffer[port][vc]                        │
│    - HEAD flit: 记录 packet_id → out_port/vc 映射到 routing_table_ │
├─────────────────────────────────────────────────────────────────────┤
│ 2. Route Computation (RC)                                          │
│    - HEAD flit: 调用 routing_algo_->computeRoute() 计算输出方向     │
│    - BODY/TAIL flit: 查找 routing_table_[packet_id] 获取已路由     │
├─────────────────────────────────────────────────────────────────────┤
│ 3. VC Allocation (VA)                                             │
│    - HEAD flit: 调用 allocateVC(out_port, requested_vc) 分配输出 VC │
│    - 检查下游 credit: downstream_credits_[out_port][out_vc] > 0     │
│    - 若无 credit，分配失败，flit 留在 input_buffer                  │
├─────────────────────────────────────────────────────────────────────┤
│ 4. Switch Allocation (SA)                                          │
│    - 仲裁: 从多个请求同一输出端口的 VC 中选择一个                  │
│    - Round-Robin 仲裁策略                                          │
│    - 选中: 消耗 downstream credit                                  │
├─────────────────────────────────────────────────────────────────────┤
│ 5. Switch Traversal (ST)                                           │
│    - 将选中的 flit 从 input_buffer 经过 crossbar 发送到 out_port    │
│    - resp_out[out_port].write(flit.bundle)                        │
│    - 若是 TAIL/HEAD_TAIL flit: releaseVC(out_port, vc_id)         │
├─────────────────────────────────────────────────────────────────────┤
│ 6. Link Traversal (LT)                                             │
│    - 建模链路传输延迟 (通过 link_delay_ 参数配置)                   │
│    - 或在 Router 之间插入 LinkTLM 模块                              │
└─────────────────────────────────────────────────────────────────────┘
```

**Credit-based Flow Control 机制**:

```
Upstream Router                          Downstream Router
      │                                        │
      │  send flit (消耗 downstream credit)     │
      │───────────────────────────────────────►│
      │                                        │
      │  credit return (恢复 downstream credit)  │
      │◄───────────────────────────────────────│
      │                                        │

每个 VC 有独立 credit 计数器:
- 发送 flit: downstream_credits[port][vc]--
- 收到 credit return: downstream_credits[port][vc]++
- credit 为 0 时不能再发送，直到下游返回 credit
```

**输出**: `include/tlm/router_tlm.hh`

### Phase 7.3: NICTLM 核心 (Week 3-4)

| 步骤 | 任务 | 文件 | 验收标准 |
|------|------|------|----------|
| 7.3-1 | 创建 NICTLM 头文件骨架 | `include/tlm/nic_tlm.hh` | 四端口声明 (PE侧×2 + Net侧×2) |
| 7.3-2 | 实现 AddressMap 地址映射 | `include/tlm/nic_tlm.hh` | 地址 → 节点 ID 映射正确 |
| 7.3-3 | 实现 `packetize()` | `src/tlm/nic_tlm.cc` | CacheReq → NoC Flits 切分正确 |
| 7.3-4 | 实现 `reassemble()` | `src/tlm/nic_tlm.cc` | NoC Flits → CacheResp 重组正确 |
| 7.3-5 | 实现 tick() 双向转发 | `src/tlm/nic_tlm.cc` | PE→Net 和 Net→PE 同时工作 |
| 7.3-6 | 注册到 `REGISTER_CHSTREAM_DUAL` | `include/chstream_register.hh` | NICTLM 使用 DualPortAdapter |
| 7.3-7 | 性能统计集成 | `include/tlm/nic_tlm.hh` | `stats_` 记录 pkts/flits/latency |

**NICTLM 端口约定** (统一 4 个访问器，遵循 DualPortStreamAdapter):

| 访问器 | 方向 | Bundle 类型 | 用途 |
|--------|------|------------|------|
| `pe_req_in()` | 输入 | CacheReqBundle | 接收 PE 侧请求（来自 Cache/CPU） |
| `pe_resp_out()` | 输出 | CacheRespBundle | 向 PE 侧返回响应 |
| `net_req_out()` | 输出 | NoCFlitBundle | 向 NoC 发送 Flit |
| `net_resp_in()` | 输入 | NoCFlitBundle | 从 NoC 接收 Flit |

```cpp
// PE 侧访问器
cpptlm::InputStreamAdapter<bundles::CacheReqBundle>&   pe_req_in()   { return pe_req_in_; }
cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>&  pe_resp_out() { return pe_resp_out_; }

// Network 侧访问器 (v2.1: 统一使用 NoCFlitBundle)
cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>&   net_req_out() { return net_req_out_; }
cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>&   net_resp_in()  { return net_resp_in_; }
```

**AddressMap 地址映射**:

```cpp
// 地址空间划分: 每个区域映射到特定 Mesh 节点
struct AddressRegion {
    uint64_t base_addr;      // 区域起始地址
    uint64_t size;           // 区域大小
    uint32_t target_node;   // 目标节点 ID
    std::string target_type; // "MEMORY_CTRL", "CACHE", etc.
};

class AddressMap {
private:
    std::vector<AddressRegion> regions_;
public:
    // 根据地址查找目标节点
    uint32_t lookupNode(uint64_t addr) const;
    // 节点坐标转换
    std::pair<uint32_t, uint32_t> nodeToCoord(uint32_t node_id) const;
    uint32_t coordToNode(uint32_t x, uint32_t y) const;
};
```

### Phase 7.4: JSON 配置扩展 (Week 4)

| 步骤 | 任务 | 文件 | 验收标准 |
|------|------|------|----------|
| 7.4-1 | RouterTLM JSON 参数 | `include/utils/config_utils.hh` | 支持 `num_vcs`, `buffer_depth`, `routing_algo` |
| 7.4-2 | NICTLM JSON 参数 | `include/utils/config_utils.hh` | 支持 `node_id`, `traffic_pattern` |
| 7.4-3 | 测试拓扑: NIC→Router→NIC | `configs/test/nic_router_nic.json` | 最小拓扑可仿真 |
| 7.4-4 | 测试拓扑: 2×2 Mesh | `configs/test/mesh_2x2.json` | 4 NIC + 4 Router 拓扑 |

### Phase 7.5: 端到端集成测试 (Week 5)

| 步骤 | 任务 | 文件 | 验收标准 |
|------|------|----------|----------|
| 7.5-1 | Bundle 单元测试 | `test/test_noc_bundles.cc` | `[noc]` tag |
| 7.5-2 | Router XY 路由测试 | `test/test_router_xy_routing.cc` | `[router]` tag |
| 7.5-3 | Router VC/仲裁测试 | `test/test_router_vc_arbiter.cc` | `[router]` tag |
| 7.5-4 | NIC 包化/反包化测试 | `test/test_nic_packetization.cc` | `[nic]` tag |
| 7.5-5 | NIC→Router→NIC 端到端 | `test/test_phase7_integration.cc` | `[phase7]` tag |

### Phase 7.6: 统计与流量模式 (Week 6)

| 步骤 | 任务 | 文件 | 验收标准 |
|------|------|------|----------|
| 7.6-1 | NoCStatistics 类 | `include/tlm/noc_statistics.hh` | 延迟/吞吐/跳数统计 |
| 7.6-2 | Uniform Random 流量 | NICTLM 内置 | uniform 模式可配置 |
| 7.6-3 | Hotspot 流量 | NICTLM 内置 | hotspot 模式可配置 |
| 7.6-4 | 4×4 Mesh 拓扑 | `configs/mesh_4x4.json` | 16 节点可仿真 |

---

## 四、架构约束清单

### 4.1 与 V2.1 TLM 架构完全一致

| 维度 | 现有模式 | Phase 7 遵循方式 |
|------|----------|-----------------|
| **基类** | `ChStreamModuleBase` | RouterTLM, NICTLM 全部继承 |
| **Bundle 命名空间** | `bundles::` | NoCFlitBundle (统一类型) 在 bundles:: |
| **Bundle 基类** | `bundle_base` | 所有 Bundle 继承 bundle_base |
| **类型包装** | `ch_uint<W>`, `ch_bool` | NoC Bundle 全部使用 cpphdl_types.hh |
| **StreamAdapter** | `Standalone/Bidirectional/DualPort` | RouterTLM=Bidirectional(5×2端口), NICTLM=DualPort |
| **注册宏** | `REGISTER_CHSTREAM` | RouterTLM 用 BidirectionalPortAdapter |
| **注册宏** | `REGISTER_CHSTREAM_DUAL` | NICTLM 用 registerDualPortAdapter |
| **端口访问器** | 4 个固定访问器 | pe_req_in, pe_resp_out, net_req_out, net_resp_in |
| **tick() 模式** | 每周期遍历端口 | RouterTLM 六阶段流水线, NICTLM 双向转发 |
| **统计系统** | `tlm_stats::StatGroup` | 全部使用 metrics/stats.hh |

### 4.2 DualPortStreamAdapter 端口约定 (NICTLM 必须遵循)

> **v2.1 修正**: Network 侧端口统一使用 `NoCFlitBundle` 类型

| 访问器 | 方向 | Bundle 类型 | 用途 |
|--------|------|------------|------|
| `pe_req_in()` | 输入 | CacheReqBundle | 接收 PE 侧请求（来自 Cache/CPU） |
| `pe_resp_out()` | 输出 | CacheRespBundle | 向 PE 侧返回响应 |
| `net_req_out()` | 输出 | NoCFlitBundle | 向 NoC 发送 Flit |
| `net_resp_in()` | 输入 | NoCFlitBundle | 从 NoC 接收 Flit |

```cpp
// ModuleT 必须提供以下 4 个访问器:
cpptlm::InputStreamAdapter<PE_ReqBundleT>&   pe_req_in();      // PE 侧请求输入
cpptlm::OutputStreamAdapter<PE_RespBundleT>&  pe_resp_out();    // PE 侧响应输出
cpptlm::OutputStreamAdapter<Net_ReqBundleT>&  net_req_out();   // Network 侧请求输出
cpptlm::InputStreamAdapter<Net_RespBundleT>&   net_resp_in();   // Network 侧响应输入
```

### 4.3 BidirectionalPortAdapter 端口约定 (RouterTLM 使用)

> **重要修正**: RouterTLM 每个端口需要双向通信能力（既接收请求也发送响应），因此不使用 `MultiPortStreamAdapter`，而是为每个端口定义 `req_in + resp_out` 端口对。

```cpp
// RouterTLM 端口结构: 每个端口双向
// 端口 i: req_in_[i] (输入) + resp_out_[i] (输出)
// 端口数量: 5 (N, E, S, W, Local)
// v2.1: 统一使用 NoCFlitBundle

cpptlm::InputStreamAdapter<NoCFlitBundle>   req_in_[NUM_PORTS];    // N 个输入端口
cpptlm::OutputStreamAdapter<NoCFlitBundle> resp_out_[NUM_PORTS];  // N 个输出端口
unsigned num_ports() const override { return NUM_PORTS; }
```

### 4.4 Credit-based Flow Control 约定

```cpp
// 每个 VC 有独立的 credit 计数器
struct VCCredit {
    uint8_t count;        // 当前可用 credit
    uint8_t max_credits;  // 最大 credit (= buffer depth)
};

// 发送 flit 时消耗 credit
if (downstream_credits_[port][vc].count > 0) {
    send_flit(flit);
    downstream_credits_[port][vc].count--;
} else {
    // 阻塞，等待 credit 返回
}

// 收到 credit return 时恢复
void receiveCredit(uint8_t port, uint8_t vc) {
    if (downstream_credits_[port][vc].count < max_credits) {
        downstream_credits_[port][vc].count++;
    }
}
```

### 4.5 JSON 配置端口索引语法

```json
{
  "connection": { "src": "router_0_0.4", "dst": "router_0_1.2" }
}
// router_0_0 的 Local 端口 (索引 4) → router_0_1 的 South 端口 (索引 2)
```

---

## 五、风险评估

### 5.1 高优先级风险（必须解决）

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| BidirectionalPortAdapter 与 RouterTLM 端口模型不匹配 | 中 | 高 | Phase 7.0 创建专用适配器 |
| DualPortStreamAdapter tick() 未处理 net_resp_in | 高 | 高 | Phase 7.0 修正 DualPortStreamAdapter::tick() |
| 缺少 LT 阶段导致 NoC 延迟低估 15-30% | 高 | 高 | Phase 7.2 实现六阶段流水线 |
| Credit-based Flow Control 实现不完整 | 中 | 高 | Phase 7.2 从 Day 1 实现 credit 机制 |

### 5.2 中优先级风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| XY 路由死锁 (仅理论风险) | 低 | 高 | XY 路由本身死锁自由，后续扩展 Odd-Even |
| 缓冲区溢出导致仿真崩溃 | 中 | 高 | 从 Day 1 实现反压机制 |
| 路由算法硬编码，无扩展性 | 中 | 中 | Phase 7.0 添加 RoutingAlgorithm 接口 |
| AddressMap 未定义导致 dst_node 错误 | 中 | 中 | Phase 7.3 实现 AddressMap |

### 5.3 低优先级风险（后续迭代）

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 缺少死锁检测 | 低 | 中 | Phase 7.x 添加超时告警 |
| 缺少多播/广播支持 | 低 | 中 | Phase 7.x 扩展 Bundle 添加 dest_mask |
| 大规模 Mesh (8×8+) 仿真性能 | 低 | 中 | Phase 7.x 实现 LT 快速模式 |

---

## 六、验收标准

### 6.1 功能验收

1. **编译通过**: `cmake --build build` 零警告
2. **测试通过**: `[phase7]` 标签测试 100% 通过 (预计 50+ 用例)
3. **拓扑可用**: 2×2 Mesh 配置能完整仿真 (NICTLM → Router → NICTLM 全链路)
4. **延迟统计**: 仿真结束输出平均延迟/吞吐量/跳数
5. **零回归**: 现有 `[phase6]`, `[chstream]`, `[crossbar]` 测试不受影响
6. **架构对齐**: RouterTLM/NICTLM 必须通过 `lsp_diagnostics` 零错误

### 6.2 NoC 建模验收

7. **六阶段流水线**: BW→RC→VA→SA→ST→LT 完整执行
8. **Credit-based Flow Control**: credit 消耗/返回机制正确工作
9. **XY 路由**: 路由计算正确 (单元测试覆盖所有方向组合)
10. **包化/反包化**: 多 flit 分组传输 + 重组正确

### 6.3 接口验收

11. **DualPortStreamAdapter**: 4 个端口访问器命名统一
12. **BidirectionalPortAdapter**: RouterTLM 5 端口双向通信正确
13. **RoutingAlgorithm 接口**: 可切换不同路由算法

---

## 七、文件清单

### 新增文件 (Phase 7)

| 文件 | 类型 | 估计行数 | 说明 |
|------|------|----------|------|
| `include/bundles/noc_bundles_tlm.hh` | 头文件 | ~100 | NoCFlitBundle（含 flit_index/flit_count/flit_category） |
| `include/tlm/router_tlm.hh` | 头文件 | ~280 | RouterTLM 类定义（含六阶段流水线 + RoutingAlgorithm） |
| `include/tlm/nic_tlm.hh` | 头文件 | ~220 | NICTLM 类定义（含 AddressMap + 包化/反包化状态机） |
| `include/tlm/link_tlm.hh` | 头文件 | ~60 | LinkTLM 链路延迟模块（可选） |
| `include/tlm/noc_statistics.hh` | 头文件 | ~100 | NoCStatistics 统计类 |
| `include/framework/bidirectional_port_adapter.hh` | 头文件 | ~120 | RouterTLM 专用双向端口适配器 |
| `src/tlm/router_tlm.cc` | 源文件 | ~400 | RouterTLM 六阶段流水线 + Credit Flow 实现 |
| `src/tlm/nic_tlm.cc` | 源文件 | ~380 | NICTLM 包化/反包化 + AddressMap 实现 |
| `src/tlm/link_tlm.cc` | 源文件 | ~80 | LinkTLM 实现 |
| `test/test_noc_bundles.cc` | 测试 | ~80 | Bundle 字段读写、flit_index/flit_count 测试 |
| `test/test_router_xy_routing.cc` | 测试 | ~150 | XY 路由算法单元测试 |
| `test/test_router_vc_arbiter.cc` | 测试 | ~180 | VC 分配 + RR 仲裁 + Credit Flow 测试 |
| `test/test_router_six_stage.cc` | 测试 | ~120 | 六阶段流水线单元测试 |
| `test/test_nic_packetization.cc` | 测试 | ~140 | 包化/反包化 + AddressMap 测试 |
| `test/test_phase7_integration.cc` | 集成测试 | ~250 | NIC→Router→NIC 端到端测试 |
| `configs/test/nic_router_nic.json` | 配置 | ~30 | 最简拓扑: 1 NIC + 1 Router |
| `configs/test/mesh_2x2.json` | 配置 | ~80 | 2×2 Mesh (4 NIC + 4 Router) |
| `configs/mesh_4x4.json` | 配置 | ~200 | 4×4 Mesh (16 NIC + 16 Router) |

### 修改文件 (Phase 7)

| 文件 | 修改内容 |
|------|----------|
| `include/chstream_register.hh` | 添加 RouterTLM (Bidirectional) + NICTLM (DualPort) 注册 |
| `include/framework/dual_port_stream_adapter.hh` | 修正 tick() 处理 net_resp_in，添加注释明确 4 个访问器 |
| `CMakeLists.txt` | 添加 `src/tlm/router_tlm.cc`, `src/tlm/nic_tlm.cc`, `src/tlm/link_tlm.cc` |
| `include/tlm/AGENTS.md` | 补充 RouterTLM/NICTLM/LinkTLM 文档 |
| `include/utils/config_utils.hh` | 添加 RouterTLM 参数 (routing_algo, link_delay, num_vcs) | |

---

## 八、Legacy 死代码参考总结

### 8.1 NoC/NIC 归档文件分析

| Legacy 文件 | 设计理念 | Phase 7 改进 |
|-------------|----------|-------------|
| `dead-code-headers-2026-04-14/noc_bundles.hh` | 字段设计合理（transaction_id, src/dst_id, data, is_write） | 改用 cpphdl_types.hh 轻量类型 + 添加 flit_index/flit_count |
| `dead-code-sources-2026-04-14/router.cc` | 5 阶段流水线 + VC 分配 + FlitTypeExtension 携带分片状态 | 简化为 ChStreamModuleBase tick() + RouterFlit 内部封装 |
| `dead-code-sources-2026-04-14/nic.cc` | 包化/反包化逻辑（payload → Flits，FlitExtension 携带 HEAD/BODY/TAIL） | 改为 NICTLM 双端口模式 + RespAssemblyContext 状态机 |
| `dead-code-headers-2026-04-14/router_sc.hh` | SC_MODULE 5 端口架构（input_buffer/output_buffer，routing/arbitration/switching 分离） | 改用 ChStreamModuleBase 非 SystemC |
| `dead-code-headers-2026-04-14/flit_extension.hh` | FlitExtension tlm_extension 扩展（FlitType, packet_id, vc_id） | 改用 NoCFlitBundle.flit_type + RouterTLM 内部状态表 |

### 8.2 Legacy 关键设计模式（参考提取）

**FlitType 分片模式**:
```
HEAD      → 触发路由计算 + VC 分配，携带 total_flit_count
BODY      → 查表获取已分配的路由 + VC，继续转发
TAIL      → 最后一个分片，携带最后数据
HEAD_TAIL → 单 flit 包，简化处理
```

**Router 内部状态追踪**:
```cpp
// Legacy 方案: FlitExtension 携带 packet_id，每个 flit 都带
FlitExtension* ext = get_flit_ext(pkt->payload);
ext->packet_id  // 分组 ID
ext->vc_id      // 虚拟通道
ext->type       // HEAD/BODY/TAIL
```

**Phase 7 改进方案**:
```cpp
// RouterTLM 内部: routing_table_[packet_id] → {out_port, out_vc}
std::unordered_map<uint64_t, RoutingState> routing_table_;

// HEAD flit: 写入路由状态
// BODY/TAIL flit: 查表获取路由
```

**NIC 包化逻辑（Legacy 参考）**:
```cpp
// Legacy nic.cc
size_t flit_count = (data.size() + 7) / 8;  // 向上取整
for (size_t i = 0; i < flit_count; ++i) {
    FlitExtension flit_ext;
    if (flit_count == 1) flit_ext.type = HEAD_TAIL;
    else if (i == 0) flit_ext.type = HEAD;
    else if (i == flit_count - 1) flit_ext.type = TAIL;
    else flit_ext.type = BODY;
    flit_ext.packet_id = packet_counter;
    flit_ext.vc_id = selectVC(dst);
}
```

**Phase 7 改进方案**:
```cpp
// NICTLM::packetize() - 直接在 Bundle 层面操作
flit.flit_index.write(i);
flit.flit_count.write(flit_count);
flit.flit_type.write(i == 0 ? FLIT_HEAD : (i == flit_count-1 ? FLIT_TAIL : FLIT_BODY));
```

### 8.3 关键改进总结

| 维度 | Legacy 方案 | Phase 7 改进 |
|------|-------------|-------------|
| **分片元数据** | `FlitExtension` tlm_extension 携带 | 直接在 `NoCFlitBundle.flit_type/flit_index/flit_count` 字段携带 |
| **Router 内部状态** | 每个 flit 携带 `packet_id` 查表 | `routing_table_[packet_id]` 集中维护 HEAD flit 路由结果 |
| **VC 分配** | `allocateVC()` 返回输出 VC | 同方案，RouterTLM 内部 `allocateVC()` |
| **Bundle 封装** | `ch::core::ch_uint<W>` 复杂模板 | `bundles::ch_uint<W>` 轻量包装 |
| **模块架构** | SystemC `SC_MODULE` + 直接 `send/recv` | `ChStreamModuleBase` + `StreamAdapter` |
| **端口模式** | `sc_port<sc_signal<Flit>>` | `InputStreamAdapter<>/OutputStreamAdapter<>` |

### 8.4 保留的设计理念

1. **Wormhole + Virtual Channel 流控**: Noxim/BookSim/Garnet 标准，Phase 7 保留
2. **XY Dimension-Order 路由**: 死锁自由，Phase 7 首选实现
3. **输入缓冲区组织**: `input_buffer[port][vc]` → FIFO，Phase 7 保留
4. **Round-Robin 仲裁**: 简单公平，Phase 7 首选实现

---

**版本历史**:
- v2.1 (2026-04-23): 审查修正版 - 修正 DualPortStreamAdapter 接口、RouterTLM 改为 BidirectionalPortAdapter、添加六阶段流水线 + LT、Credit-based Flow Control、RoutingAlgorithm 接口
- v2.0 (2026-04-23): 对齐 V2.1 TLM 模块架构，添加 Flit 生成/恢复机制详细设计
- v1.0 (2026-04-14): 初始版本

---

**总计**: 新增 ~2400 行代码（头文件 ~760 + 源文件 ~860 + 测试 ~780 + 配置 ~350），修改 ~5 个文件
**Phase 7 完成标准**: 2×2 Mesh NoC 仿真可运行，六阶段流水线正确，Credit Flow 工作，统计输出正确

---

## 实施进度 (2026-04-23 更新)

| Phase | 任务 | 状态 | 实现文件 |
|-------|------|:----:|----------|
| **Phase 7.0** | 框架准备 | ✅ | `bidirectional_port_adapter.hh`, `routing_algorithm.hh` |
| **Phase 7.1** | NoC Bundle 定义 | ✅ | `noc_bundles_tlm.hh`, `test_noc_bundles.cc` |
| **Phase 7.2** | RouterTLM 核心 | ✅ | `router_tlm.hh`, `router_tlm.cc`, `test_router_tlm.cc` |
| **Phase 7.3** | NICTLM 核心 | ✅ | `nic_tlm.hh`, `nic_tlm.cc`, DualPortAdapter 注册 |
| **Phase 7.4** | JSON 配置扩展 | 📋 | - |
| **Phase 7.5** | 端到端集成测试 | 📋 | - |
| **Phase 7.6** | 统计与流量模式 | 📋 | - |

**图例**: ✅ 完成 🔄 进行中 📋 待开始

---

## 附录：审查报告问题跟踪

| 问题 | 严重程度 | 状态 |
|------|---------|------|
| DualPortStreamAdapter 端口访问器命名冲突 | 高 | ✅ 已修正 (v2.1 统一 4 端口约定) |
| MultiPortStreamAdapter 与 RouterTLM 端口模型不匹配 | 高 | ✅ 已修正 (统一 NoCFlitBundle 类型) |
| 路由器端口双 Bundle 类型问题 | 高 | ✅ 已修正 (统一 NoCFlitBundle + flit_category) |
| 缺少 Link Traversal (LT) 阶段 | 高 | ✅ 已修正 (六阶段流水线) |
| 缺少拥塞与背压建模 | 高 | ✅ 已修正 (Credit-based Flow Control) |
| 路由算法硬编码 | 中 | ✅ 已修正 (RoutingAlgorithm 接口) |
| 缺少地址映射 | 中 | ✅ 已修正 (AddressMap 类) |
| 缺少 TopologyRegistry 集成 | 中 | 📋 计划 Phase 7.x |
| 缺少死锁检测 | 低 | 📋 计划 Phase 7.x |
| 缺少多播/广播支持 | 低 | 📋 计划 Phase 7.x |
| 缺少功耗/面积估算 | 低 | 📋 计划 Phase 7.x |
