# Design — cpptlm-gmmu-pte-bus-correction v1.0 (Phase 1 Change C)

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/gmmu-pte-wf/spec.md`](specs/gmmu-pte-wf/spec.md)
> **目标 tag**: `cpptlm-gmmu-pte-bus-correction-completion`
> **估时**: **~6d** (Phase 1 Change C, TDD 5-step + SocInternalNoc 集成)
> **Frozen surface**: 23 ABI 签名零 diff + **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面)
> **依赖**: e1 v1.1 ✅ archived (SocInternalNoc 设计已锁) + Change B ✅ Phase 0

---

## 总体设计 (引用 e1 v1.1 + 增量修改)

### 核心思路

**目标**: 修复 §0.1 Gap 矩阵 3 项:
1. **Gap I**: GMMU page-walk wire-format 错配 (PcieTlpBundle vs AxiMemBundle 自相矛盾) → 切回 AxiMemBundle (chip-internal AXI)
2. **Gap G2**: SDMA→VRAM 路径接 SocInternalNoc mesh
3. **Gap G1**: SDMA↔GMMU lambda 注入删除 (改 NOC 触发)

**SocInternalNoc 设计直接复用** `cpptlm-dgpu-soc-internal-noc-e1` v1.1 (✅ archived):
- 4 NODE_ID (`PCIE` / `SDMA` / `GMMU` / `VRAM`)
- `NoCFlitBundle` 16 字段 (无新 dual bundle)
- `route_flit()` 实际推送 + `send_request/response` API
- `vc_counts_[4]` 按 `flit_category` (REQ=VC0/RSP=VC1/DAT=VC2/SNP=VC3)
- 2 adapter (`SDMANocAdapter` + `VramNocAdapter`)，**删** `PcieNocAdapter` / `GMMUNocAdapter` (per e1 v1.1 决策沿袭)

### 数据路径 (实施后)

```
SDMA H2D/D2H (功能性):
    sdma.mem_out (AxiMemBundle MEM_WRITE)
        │
        ▼
    soc_internal_noc.send_request(NoCFlitBundle)
        │
        │ internal routing + flit_category=REQ → vc_counts_[0]++
        ▼
    noc_target = VRAM
        │
        ▼
    vram_noc_adapter.accept(flit) → NoC→Fabric2VramCtrlBundle 字段映射
        │
        ▼
    vram_ctrl->handle_functional_request(addr, size, is_write, data)
        │
        ▼
    memcpy(vram_storage_)  [VramController functional-mode, per Change A]


GMMU PTE fetch (per v0.5 三锁叠加裁决 + NVIDIA H100 GMMU dedicated memory client 模式, Change C 修订):
    gmmu.translate(iova, ...) → trigger PTE MEM_READ (GMMU 异步状态机 internal)
        │
        │ Gap I 修复: 经 GMMU dedicated memory client bus (NEW per v0.5)
        ▼
    gmmu.req_out → VramControllerTLM::handle_functional_request(pte_addr, 8, is_write=false, nullptr)  [VramController functional port, per Change A integration]
        │
        ▼ VramController functional-mode (zero-delay)
    vram_ctrl->handle_functional_request(pte_addr, 8, false, nullptr)
        │
        ▼ 直 memcpy
    memcpy(vram_storage_.get() + pte_addr, pte_out_buf, 8)


SDMA translate 请求 (Gap G1 修复, per Change C 路径):
    sdma.translate() (NEW NOC-driven, replaces lambda callback)
        │
        ▼ 删除 set_translate_cb(lambda) per Gap G1
    soc_internal_noc.send_request(NoCFlitBundle dst=GMMU, flit_category=REQ)
        │
        ▼ NOC mesh dispatch
    gmmu_noc_adapter (per e1 v1.1 决策沿袭; 但实际 GMMU 无入向, 仅作 passive endpoint register)
        │
        ▼ GMMU 异步翻译 (translate() 仍同步返回, per §1.4.6 H-B6-Phase4: 内部同步)
    gmmu.translate() 直接触发 functional 翻译
        │
        ▼ response 经 soc_internal_noc.send_response() 回到 SDMA
```

### GMMU wire-format 修订 (Gap I 核心)

**修订前** (`gmmu_tlm.hh:12,86`):
```cpp
//   - wire-format: PcieTlpBundle (v1.8 H1)  ← 错配
```

**修订后** (chip-internal AXI):
```cpp
namespace tlm::gpu {

// Gap I 修复: GMMU PTE fetch 走 chip-internal AxiMemBundle (per NVIDIA H100 / ARM CMN-700 物理 IP 边界)
// GMMU dedicated memory client, bypass mesh SocInternalNoc (per §0.1 + Gap I 修订)
class GmmuTLM : public ChStreamModuleBase {
public:
    // MasterPort (out): PTE fetch 发 AxiMemBundle to PcieMemory.handle_slave_port(port=1)
    cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& req_out() { return req_out_; }
    
    // SlavePort (in): translate 响应 from NOC 路径 (Gap G1 修复)
    cpptlm::InputStreamAdapter<bundles::AxiMemBundle>& resp_in() { return resp_in_; }
    
    // (master 端口 resp_out + req_in 返回 dummy, 同 CPUTLM 模式)
    
    // v1.0 NEW: bind backing (per existing D-AXI §X ADR-DGPU-10 沿袭)
    void set_mem_view(uint8_t* ptr, uint64_t sz) noexcept {
        mem_view_ = ptr;
        mem_view_size_ = sz;
    }
    
    // GAP G1 修复: 删 set_translate_cb(lambda), 改由 NOC-driven translate
    // 用 master/slave port 经 NOC receive translate request, response 回 write
    
private:
    // SlavePort + backing + 状态机 (per e1 v1.1 B22 异步 + iova 匹配 + max_retry=16)
    enum class State : uint8_t { IDLE = 0, WAIT = 1, COMPLETE = 2 };
    State state_ = State::IDLE;
    
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    cpptlm::OutputStreamAdapter<bundles::AxiMemBundle> req_out_;
    cpptlm::InputStreamAdapter<bundles::AxiMemBundle>  resp_in_;
    
    uint8_t* mem_view_ = nullptr;
    uint64_t mem_view_size_ = 0;
    uint64_t pending_iova_ = 0;
    uint32_t pending_size_ = 0;
    std::array<uint8_t, 8> pte_buf_{};
};

}  // namespace tlm::gpu
```

**关键决策**:
- **Gap I**: 主用 chip-internal AXI (`bundles::AxiMemBundle` 而非 PcieTlpBundle)
- **Gap G1**: translate 触发改 NOC path, 移除 `set_translate_cb(lambda)` 公共 API (breaking change with deprecation)
- **Phase 4 T2 兼容**: timing-mode `translate_timing()` API 保留 (functional mode 走 async 状态机, timing mode 走 sync TLB)

### JSON 修订 (Gap G2)

**修订前** (`dgpu_soc_minimal_v1.json:58-61`):
```json
"connections": [
  { "src": "sdma.2",  "dst": "pcie_memory.0", "latency": 1 },
  { "src": "gmmu.0",  "dst": "pcie_memory.1", "latency": 1 }
]
```

**修订后** (Gap G1+G2: GMMU 独立 bypass + SDMA 经 NOC):
```json
{
  "modules": [
    {
      "name": "soc", 
      "type": "DGpuSoc",
      "modules": [
        ... (既有 pcie_ep / pcie_ctrl / pcie_memory / sdma / gmmu / completion) ...
        {
          "name": "soc_internal_noc",     // [NEW per e1 v1.1]
          "type": "SocInternalNoc",
          "params": {
            "vc_counts_enabled": true,
            "data_flow": true,
            "max_outstanding": 8
          }
        }
      ],
      "connections": [
        // SDMA bulk DMA 经 NOC (Gap G2)
        { "src": "sdma.2",           "dst": "soc_internal_noc.0", "latency": 1 },
        { "src": "soc_internal_noc.2", "dst": "pcie_memory.0",   "latency": 1 }  // NC dst VRAM (per Change A integration, v0.5.1 终态)

        // [v0.5.1 修订 per Oracle B1 + B4] GMMU PTE fetch 不走 JSON ChStream connection
        // 而走 programmatic bridge (per set_vram_target 先例, 仿 VramControllerTLM 装配模式)
        // - GMMU 增加 `set_vram_target(VramControllerTLM*)` API (per tasks.md 新增 T2.5)
        // - 装配在 DGpuBoard::bind_memory_backings() (per Change A 装配段, 1 行 setter)
        // - GMMU.translate() 走 handle_functional_request() (per H-B6 v0.5.1 修订)
        // JSON 中无 GMMU-VramController connection, 走 programmatic binding
        // PcieMemoryDevice port=1 保留 (不改, per Change A H-B6 不变), 但 GMMU 不再使用
      ]
    }
  ]
}
```

**关键决策**:
- GMMU **不进 mesh NOC** (per Gap I: dedicated memory client)
- SDMA→VRAM 经 `soc_internal_noc` mesh
- GMMU 翻译触发改 NOC path (Gap G1) — SDMA 调 soc_internal_noc.send_request(dst=GMMU), GMMU 主端口拉 translate, resp 经 send_response 回 SDMA

### bind_memory_backings 修订 (Gap G1 关键)

**修订前** (`dgpu_board_shell.cc:711-728`, ~20 行):
```cpp
if (auto* sdma = dynamic_cast<SdmaEngineTLM*>(...)) {
    sdma->set_vram_backdoor(...);
    sdma->set_translate_cb([this](iova, size, &phys) {  // ← 删除!
        return gmmu_ ? gmmu_->translate(...) : -EIO;
    });
    set_sdma_engine(sdma);
}
```

**修订后**:
```cpp
if (auto* sdma = dynamic_cast<SdmaEngineTLM*>(...)) {
    sdma->set_vram_backdoor(...);
    // GAP G1 修复: 删 set_translate_cb lambda, 改成 NOC-driven translate
    // SDMA 调 soc_internal_noc.send_request(dst=GMMU, ...) 触发翻译
    // 由 soc_internal_noc.send_response 回 SDMA
    if (auto* noc = dynamic_cast<SocInternalNoc*>(soc_->getInternalInstance("soc_internal_noc"))) {
        sdma->set_translate_target(noc);  // 注入 NOC, SDMA translate 经 NOC
    }
    set_sdma_engine(sdma);
}
```

### 关键不变式 (Hard Invariants)

- **H-C1**: Gap I 修复后 GMMU 仅 chip-internal AXI (AxiMemBundle 单 bundle)
- **H-C2**: Gap G2 SDMA 经 SocInternalNoc mesh; GMMU bypass (一主一辅两条路, **不同路径**)
- **H-C3**: Gap G1 删 `set_translate_cb(lambda)`, 改 NOC 触发
- **H-C4**: 23 ABI + **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面)
- **H-C5**: SocInternalNoc 沿用 e1 v1.1 设计 (`4 NODE_ID + 16 字段 + vc_counts_[4]`), 不引入 dual bundle
- **H-C6**: Phase 4 timing-mode `translate_timing()` 兼容 (mem_view_ 注入下走 sync TLB path, 不阻塞 async wire-format 路径)
- **H-C7**: VramController 数据路径 (per Change A) 与本 change 是**并行** (本 change SDMA→NOC→VRAM, Change A 是 functional-mode 数据路径) — 0 依赖

---

## Backward Compatibility 策略

| 场景 | 行为 |
|------|------|
| 既有 `[sdma]` 测试需要 SDMA translate 路径 | SDMA 调 `soc_internal_noc->send_request(dst=GMMU)`, NOC dispatch, GMMU translate; 测试需要 sim tick 触发 NOC dispatch (与现有 inject_q_ 模式一致) |
| 既有 `[pcie]` 测试 (430 cases) | GMMU wire-format 修订可能影响少数 GMMU 异步路径测试 (verify Phase 1.8); PCIe EP 行为不变 |
| 既有 `[minimal_dgpu_soc]` (12 cases / 66 assn) | driver 视角不变; SDMA translate 经 NOC 语义等效 (但跨多 sim tick) |
| `set_translate_cb` 旧 API | 直接删 (breaking change); 0.5d 兼容窗口不适用 (API 已删除) |

---

## 测试策略

1. **NOC 单元测试 `[noc][unit]`** (5 cases): SocInternalNoc 基础 (route_flit + vc_counters + send_request)
2. **GMMU bypass `[noc][gmmu-bypass]`** (5 cases): GMMU PTE fetch 走 chip-internal AXI, **不进** mesh NOC (intercept NOC traffic)
3. **SDMA bulk `[noc][sdma-bulk-dma]`** (5 cases): SDMA→VRAM 经 NOC 路径 byte-level match
4. **Deadlock fuzz `[noc][deadlock-fuzz]`** (3 cases): random flit injection, 检查 NOC 无 deadlock (≤ 1000 cycles resolve)
5. **0 回归锁** (per Phase 1.8 tasks.md): `[sdma]` / `[pcie]` / `[minimal_dgpu_soc]` / `[dgpu_soc_timing]`

总 18 cases / 估 30+ assertions

---

## 风险与缓解

| Risk | Phase | Mitigation |
|------|-------|-----------|
| 删 `set_translate_cb` 是 breaking change, 既有 SDMA 测试可能 break | Phase 1.5 | 测试全跑 + 重联 SDMA translate path |
| SocInternalNoc 落地复杂 (e1 v1.1 已设计但未实施) | Phase 1.4 | 直接复用 e1 v1.1 设计 (NODE_ID + 16 字段 + vc_counts_[4] 已锁); Adapter 实现同步 |
| GMMU wire-format 切回 AxiMemBundle 影响既有 async 状态机测试 | Phase 1.2 | `gmmu_tlm.hh:43` TLB + hits/misses 统计保留; 仅 wire-format 换 |

---

## 文件清单

| 文件 | 类型 | 估时 |
|------|------|------|
| `include/tlm/gpu/gmmu_tlm.hh` | 修改 (~50 行) | 0.5d |
| `include/tlm/cluster/soc_internal_noc.hh` | 升级 (~80 行, per e1 v1.1 设计) | 1d |
| `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings` | 修改 (~20 行) | 0.5d |
| `configs/dgpu_soc_minimal_v1.json` | 修改 (+~20 行 soc_internal_noc 节点) | 0.1d |
| `test/test_soc_internal_noc_data_flow.cc` | 新增 (~250 行, 18 cases) | 2d |
| `docs/implementation/2026-10-09-gmmu-pte-bus-correction.md` | 新增 (~50 行) | 0.25d |
| `docs/designs/dgpu-soc/architecture.md` §0.2 Gap I/G2/G1 ✅ | 文档同步 | 0.25d |
| **总计** | | **~5d + 1d buffer = 6d** |

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-09 (v1.0 起草, Phase 1 Change C 启动)
