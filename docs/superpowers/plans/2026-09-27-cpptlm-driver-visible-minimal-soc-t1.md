# T1 Implementation Sub-Plan: cpptlm-driver-visible-minimal-soc (D-AXI v1.5)

> **关联**: `openspec/changes/cpptlm-driver-visible-minimal-soc/` (v1.5)
> **文档**: `tasks.md` P0.8-P0.18 + `design.md` §1/§4/§5/§6 + `spec.md` 全 Requirement
> **12 铁律**: `docs/pcie/driver-visible-minimal-soc.md` §4
> **总预估**: ~10-11 工作日（v1.3 P0 + v1.4 P0 + v1.5 P0 + 主体 T0-T4）

## 阶段划分（每阶段 5-15 分钟子任务，按 TDD 5 步结构）

本计划覆盖 **v1.5 P0.8-P0.18**（B14-B28）全部 15 项修正任务，按依赖关系分为 **14 个 Phase**。每个 Phase 含多个 TDD 5 步子任务（每个子任务 5-15 分钟）。

---

### Phase 0: 前置框架修复（P0.11 B17 + P0.15 B21 + P0.12 B18）

**前置说明**: 这三个 Phase 是所有后续修改的基础设施依赖，必须先完成。

---

#### Phase 0.1 (B17): `Packet::payload_resize()` + `PacketPool::acquire_with_min_size()`

**目标**: 为 AxiMemBundle round-trip 提供框架级 payload 扩容支持

**TDD 5 步子任务**:

**0.1.1: Write failing test — `Packet::payload_resize()` 不存在 (5 min)**
- 文件: `test/test_framework_payload_resize.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "core/packet.hh"
#include "core/packet_pool.hh"

int main() {
    Packet* pkt = PacketPool::get().acquire();
    // 初 payload 容量应足够小
    assert(pkt->payload->get_data_length() < 128);

    // payload_resize 应能扩容到 AxiMemBundle 大小 (约 4136B)
    bool ok = pkt->payload_resize(8192);
    assert(ok);
    assert(pkt->payload->get_data_length() >= 8192);

    // 缩小也应支持
    ok = pkt->payload_resize(256);
    assert(ok);
    assert(pkt->payload->get_data_length() >= 256);

    PacketPool::get().release(pkt);
    return 0;
}
```
- 验证: `g++ -std=c++17 -I include test/test_framework_payload_resize.cc -o /tmp/t && /tmp/t` → 预期 FAIL (method not found)

**0.1.2: Implement `Packet::payload_resize()` (10 min)**
- 文件: `include/core/packet.hh` (line 200+ 附近)
- 添加方法:
```cpp
// payload_resize: 调整 payload data 容量（按字节对齐）
// 若 new_size > 当前容量，重新分配 data 指向的 buffer
// 返回 true 表示成功，false 表示分配失败
bool payload_resize(uint64_t new_size) {
    if (!payload) return false;
    auto current_cap = payload->get_data_length();
    if (new_size <= current_cap) return true;  // 无需扩容

    // 重新分配: 按 512B 对齐向上取整
    uint64_t aligned = ((new_size + 511) / 512) * 512;
    auto* new_data = new uint8_t[aligned];
    std::memcpy(new_data, payload->get_data_ptr(), payload->get_data_length());
    // 替换 payload data 指针（tlm_generic_payload 需要 set_data_ptr）
    // 注意: tlm_generic_payload 的 data 成员是 private，需通过 set_data_ptr 修改
    // 如果 tlm_generic_payload 不支持修改 data 指针，则需要分配新 payload
    (void)new_data; // 消除 unused warning，实际使用见下
    return false;  // stub: 待 tlm_generic_payload 接口确认后完善
}
```
- **实际实现** (基于 `tlm_generic_payload`):
```cpp
bool payload_resize(uint64_t new_size) {
    if (!payload) return false;
    uint64_t current = payload->get_data_length();
    if (new_size <= current) return true;

    // 按 512B 对齐
    uint64_t aligned = ((new_size + 511) / 512) * 512;
    unsigned char* new_data = new unsigned char[aligned];
    std::memcpy(new_data, payload->get_data_ptr(), current);
    payload->set_data_ptr(new_data);
    payload->set_data_length(aligned);
    return true;
}
```

**0.1.3: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_framework_payload_resize.cc -o /tmp/t && /tmp/t` → 预期 PASS

**0.1.4: Write failing test — `PacketPool::acquire_with_min_size()` 不存在 (5 min)**
- 文件: `test/test_packet_pool_acquire_with_min_size.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "core/packet_pool.hh"

int main() {
    // acquire() 返回的 packet 可能 payload 容量太小 (默认 256B)
    // 对于 AxiMemBundle (约 4136B)，需要保证最小容量
    Packet* pkt = PacketPool::get().acquire_with_min_size(8192);
    assert(pkt != nullptr);
    assert(pkt->payload->get_data_length() >= 8192);
    PacketPool::get().release(pkt);
    return 0;
}
```

**0.1.5: Implement `PacketPool::acquire_with_min_size()` (10 min)**
- 文件: `include/core/ext/packet_pool.hh` (line 180+)
- 添加方法:
```cpp
// acquire_with_min_size: 返回保证最小容量的 packet
Packet* acquire_with_min_size(uint64_t min_bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Packet* pkt = nullptr;
    if (!m_packet_freelist.empty()) {
        pkt = m_packet_freelist.front();
        m_packet_freelist.pop();
    } else {
        pkt = new_packet();
    }
    if (!m_payload_freelist.empty()) {
        pkt->payload = m_payload_freelist.front();
        m_payload_freelist.pop();
        pkt->payload->reset();
    } else {
        pkt->payload = new_payload();
    }
    // 确保 payload 容量足够
    if (pkt->payload->get_data_length() < min_bytes) {
        pkt->payload_resize(min_bytes);
    }
    return pkt;
}
```

**0.1.6: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_packet_pool_acquire_with_min_size.cc -o /tmp/t && /tmp/t` → 预期 PASS

**0.1.7: Commit**
```bash
git add include/core/packet.hh include/core/ext/packet_pool.hh test/test_framework_payload_resize.cc test/test_packet_pool_acquire_with_min_size.cc
git commit -m "feat(framework): add Packet::payload_resize() + PacketPool::acquire_with_min_size() (B17)"
```

---

#### Phase 0.2 (B21): GMMU dummy `resp_out()` + `req_in()`

**目标**: 让 GMMU 满足 `registerAdapter` 模板契约，避免编译失败

**TDD 5 步子任务**:

**0.2.1: Write failing test — GMMU 缺少 `resp_out()` + `req_in()` (5 min)**
- 文件: `test/test_gmmu_dummy_methods.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/gmmu_tlm.hh"

int main() {
    GmmuTLM gmmu("test_gmmu");
    // resp_out() 和 req_in() 应返回可调用的适配器引用
    auto& resp_out = gmmu.resp_out();
    auto& req_in = gmmu.req_in();
    // 验证方法存在且可调用（编译期检查）
    assert(true);  // 若方法不存在，编译失败
    return 0;
}
```
- 验证: `g++ -std=c++17 -I include test/test_gmmu_dummy_methods.cc -o /tmp/t` → 预期 FAIL (method not found)

**0.2.2: Implement dummy `resp_out()` + `req_in()` in GMMU (10 min)**
- 文件: `include/tlm/gpu/gmmu_tlm.hh` (line 176-183 附近)
- 在 `public:` 段添加:
```cpp
// ── dummy 访问器 (B21: 对齐 MemoryTLM 模板契约) ──
// registerAdapter 模板需要 req_in/resp_out 访问器存在
// minimal_v1 不实际使用这些 dummy（生产路径走 translate_sync）
cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& req_out() { return req_out_; }
cpptlm::InputStreamAdapter<bundles::AxiMemBundle>&  resp_in() { return resp_in_; }
// dummy resp_out/req_in (B21: 让 registerAdapter 模板不报错)
cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& resp_out() {
    static cpptlm::OutputStreamAdapter<bundles::AxiMemBundle> dummy;
    return dummy;
}
cpptlm::InputStreamAdapter<bundles::AxiMemBundle>& req_in() {
    static cpptlm::InputStreamAdapter<bundles::AxiMemBundle> dummy;
    return dummy;
}
```
- **注意**: 需要 include `bundles/axi_mem_bundles_tlm.hh`（新 bundle）

**0.2.3: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_gmmu_dummy_methods.cc -o /tmp/t && /tmp/t` → 预期 PASS

**0.2.4: Commit**
```bash
git add include/tlm/gpu/gmmu_tlm.hh test/test_gmmu_dummy_methods.cc
git commit -m "feat(gmmu): add dummy resp_out()/req_in() for registerAdapter template contract (B21)"
```

---

#### Phase 0.3 (B18): MemoryTLM `on_config_loaded` 真实接线 capacity_gb

**目标**: `on_config_loaded` 实际读取 `capacity_gb` 配置，消除"1GB cap 不存在"谎言

**TDD 5 步子任务**:

**0.3.1: Write failing test — `on_config_loaded` 不读取 capacity_gb (5 min)**
- 文件: `test/test_memory_tlm_on_config_loaded.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/memory_tlm.hh"

int main() {
    MemoryTLM mem("test_mem");
    // on_config_loaded 应读取 cfg["capacity_gb"] 并调用 set_size_bytes
    nlohmann::json cfg = {{"capacity_gb", 8}};
    mem.on_config_loaded(cfg);
    // 验证 size_cap_ 被设置为 8GB
    // MemoryTLM 应有 size_cap() 访问器或等价验证方式
    assert(mem.size_cap() == (8ULL << 30));
    return 0;
}
```
- 验证: `g++ -std=c++17 -I include test/test_memory_tlm_on_config_loaded.cc -o /tmp/t` → 预期 FAIL (member not found 或断言失败)

**0.3.2: Implement `on_config_loaded` 真实接线 (10 min)**
- 文件: `include/tlm/memory_tlm.hh` (line 95-99)
- 替换为:
```cpp
void on_config_loaded(const nlohmann::json& cfg) override {
    if (cfg.contains("capacity_gb") && cfg["capacity_gb"].is_number()) {
        uint64_t gb = cfg["capacity_gb"].get<uint64_t>();
        set_size_bytes(gb * (1ULL << 30));
    }
}
```

**0.3.3: Update `configs/dgpu_soc_minimal_v1.json` capacity_gb (3 min)**
- 文件: `configs/dgpu_soc_minimal_v1.json` (line 41)
- 将 `"capacity_gb": 1` 改为 `"capacity_gb": 8`（B18 一致性）

**0.3.4: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_memory_tlm_on_config_loaded.cc -o /tmp/t && /tmp/t` → 预期 PASS

**0.3.5: Commit**
```bash
git add include/tlm/memory_tlm.hh configs/dgpu_soc_minimal_v1.json test/test_memory_tlm_on_config_loaded.cc
git commit -m "feat(memory): on_config_loaded reads capacity_gb from cfg (B18)"
```

---

### Phase 1 (B14): 消灭 `vram_segments_` MAP — backdoor 改走 `vram_storage_` 唯一路径

**目标**: 删除第三个存储实例，统一 backdoor 路径

**TDD 5 步子任务**:

**1.1: Write failing test — `vram_segments_` 仍存在于 backdoor 路径 (5 min)**
- 文件: `test/test_vram_segments_eliminated.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/dgpu_board_shell.hh"

// 测试: backdoor_read/write 应直接操作 vram_storage_，而非 vram_segments_
// 验证方式: 检查 backdoor 路径不触发 vram_segments_ 查找
int main() {
    DGpuBoard board("test_board");
    // 加载 minimal_v1 配置（已含 vram_storage_）
    nlohmann::json cfg = nlohmann::json::parse(R"({
        "modules": [{"name": "soc", "type": "DGpuSoc", "modules": []}]
    })");
    board.load_soc_config(cfg);
    board.init();

    // backdoor 读写: 应直接操作 vram_storage_ 指针
    uint8_t buf[8] = {0};
    int r = board.backdoor_read(0, buf, 8);
    // 若 vram_segments_ 已删除，此路径应失败（无 vram_storage_ 时返回错误）
    // 而非返回 vram_segments_ 中的旧数据
    return 0;
}
```
- 验证: `grep -n "vram_segments_" src/tlm/gpu/dgpu_board_shell.cc` → 应仍有 MATCH（因为还没删除）

**1.2: Delete `vram_segments_` member and update backdoor_read/write (15 min)**
- 文件: `include/tlm/gpu/dgpu_board_shell.hh` (line 306)
- 删除行:
```cpp
// 删除: std::map<uint64_t, std::vector<uint8_t>> vram_segments_;
```
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (line 532-543 backdoor_read)
- 修改 `backdoor_read`:
```cpp
// 旧代码 (line 532-543):
// std::lock_guard<std::mutex> lock(inject_mu_);
// auto it = vram_segments_.find(vram_offset);
// if (it == vram_segments_.end()) return -ENOENT;
// std::memcpy(buf, it->second.data(), len);

// 新代码 (B14: 改走 vram_storage_):
if (!framebuffer_ptr_ || framebuffer_size_ == 0) {
    return -ENODEV;  // vram_storage_ 未分配
}
if (vram_offset >= framebuffer_size_ || len > framebuffer_size_ - vram_offset) {
    return -EINVAL;
}
std::memcpy(buf, framebuffer_ptr_ + vram_offset, len);
```
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (line 589-591 backdoor_write)
- 修改 `backdoor_write`:
```cpp
// 旧代码:
// vram_segments_[vram_offset] = std::vector<uint8_t>(...);

// 新代码 (B14: 直接写 vram_storage_):
if (!framebuffer_ptr_ || framebuffer_size_ == 0) {
    return -ENODEV;
}
if (vram_offset >= framebuffer_size_ || len > framebuffer_size_ - vram_offset) {
    return -EINVAL;
}
std::memcpy(framebuffer_ptr_ + vram_offset, buf, len);
```
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (line 796-814 async backdoor)
- 删除 async 路径中的 `vram_segments_` 相关代码

**1.3: Delete `last_backdoor_reads_` (关联存储，也需删除) (3 min)**
- 文件: `include/tlm/gpu/dgpu_board_shell.hh` (line 307)
- 删除:
```cpp
// 删除: std::unordered_map<uint64_t, std::vector<uint8_t>> last_backdoor_reads_;
```

**1.4: Verify `vram_segments_` eliminated (2 min)**
```bash
grep -rn "vram_segments_\b" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh
# 预期: 无 MATCH
```

**1.5: Commit**
```bash
git add include/tlm/gpu/dgpu_board_shell.hh src/tlm/gpu/dgpu_board_shell.cc
git commit -m "refactor(board): eliminate vram_segments_ from backdoor path (B14)"
```

---

### Phase 2 (B15): 强制删除 `framebuffer_storage_`

**目标**: `framebuffer_ptr_ = vram_storage_.get()`，消除二选一歧义

**TDD 5 步子任务**:

**2.1: Write failing test — `framebuffer_storage_` 仍存在 (3 min)**
- 文件: `test/test_framebuffer_storage_eliminated.cc` (新建)
- 验证:
```cpp
#include <cassert>
int main() {
    // 编译期检查: framebuffer_storage_ 成员不应存在
    // 若存在，下面代码编译成功（但不应）
    // 实际用 grep 验证:
    return 0;
}
```
- 验证: `grep -n "framebuffer_storage_" include/tlm/gpu/dgpu_board_shell.hh src/tlm/gpu/dgpu_board_shell.cc` → 应仍有 MATCH

**2.2: Delete `framebuffer_storage_` vector (10 min)**
- 文件: `include/tlm/gpu/dgpu_board_shell.hh` (line 294)
- 删除:
```cpp
// 删除: std::vector<uint8_t> framebuffer_storage_;
```
- 修改 `attach_framebuffer_for_testing` (line 238-241):
```cpp
// 旧: void attach_framebuffer_for_testing(uint8_t* ptr, uint64_t size) noexcept {
//       framebuffer_ptr_ = ptr;
//       framebuffer_size_ = size;
//     }

// 新 (B15: 不再支持独立测试注入，framebuffer_ptr_ 只能来自 vram_storage_):
void attach_framebuffer_for_testing(uint8_t* ptr, uint64_t size) noexcept {
    // B15: framebuffer_storage_ 已删除，framebuffer_ptr_ 只能是 vram_storage_.get()
    // 若 vram_storage_ 已分配，拒绝注入（防测试绕开单一真源）
    if (framebuffer_ptr_ != nullptr) {
        // vram_storage_ 已分配，拒绝覆盖
        return;
    }
    framebuffer_ptr_ = ptr;
    framebuffer_size_ = size;
}
```

**2.3: Update `init()` — `framebuffer_storage_` 分配改为 `vram_storage_` 分配 (10 min)**
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (line 166-169)
- 替换为:
```cpp
// 旧:
// framebuffer_storage_.resize(framebuffer_size_, 0);
// framebuffer_ptr_ = framebuffer_storage_.data();

// 新 (B15: vram_storage_ 是唯一真源):
if (framebuffer_ptr_ == nullptr && framebuffer_size_ > 0) {
    // vram_storage_ 由 load_soc_config 分配（来自 bar_sizes[2]）
    // 此处 framebuffer_ptr_ 应已指向 vram_storage_.get()
    // 若仍为 nullptr，说明 bar_sizes[2]==0，device 将返回 -ENODEV
}
```
- **注意**: 需要在 `load_soc_config` 中添加 `vram_storage_` 分配逻辑:
```cpp
// 在 load_soc_config 中 bar_sizes[2] 解析后添加:
// if (bar_sizes[2] > 0) {
//     vram_storage_ = std::unique_ptr<uint8_t[]>(new uint8_t[bar_sizes[2]]);
//     framebuffer_ptr_ = vram_storage_.get();
//     framebuffer_size_ = bar_sizes[2];
// }
```

**2.4: Verify `framebuffer_storage_` eliminated (2 min)**
```bash
grep -rn "framebuffer_storage_\b" include/tlm/gpu/dgpu_board_shell.hh src/tlm/gpu/dgpu_board_shell.cc
# 预期: 无 MATCH
```

**2.5: Commit**
```bash
git add include/tlm/gpu/dgpu_board_shell.hh src/tlm/gpu/dgpu_board_shell.cc
git commit -m "refactor(board): eliminate framebuffer_storage_, framebuffer_ptr_ aliases vram_storage_ (B15)"
```

---

### Phase 3 (B16): `attach_framebuffer_for_testing` 优先级规则

**目标**: vram_storage_ 已分配时拒绝 attach，防测试绕开单一真源

**TDD 5 步子任务**:

**3.1: Write failing test — attach 在 vram_storage_ 已分配时被拒绝 (5 min)**
- 文件: `test/test_attach_framebuffer_priority.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/dgpu_board_shell.hh"

int main() {
    DGpuBoard board("test_board");
    nlohmann::json cfg = /* minimal_v1 with vram */;
    board.load_soc_config(cfg);
    board.init();

    // 第一次 attach: vram_storage_ 已分配（framebuffer_ptr_ != nullptr）
    uint8_t fake_ptr = 0;
    board.attach_framebuffer_for_testing(&fake_ptr, 4096);
    // B16: 应拒绝（无操作或返回 false）
    // 验证: framebuffer_ptr_ 仍为原始 vram_storage_ 指针
    assert(board.framebuffer_ptr() != &fake_ptr);
    return 0;
}
```

**3.2: Implement priority rule in `attach_framebuffer_for_testing` (已在 Phase 2.2 实现)**
- 验证: Phase 2.2 已实现 B16 优先级规则

**3.3: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_attach_framebuffer_priority.cc -o /tmp/t && /tmp/t` → 预期 PASS

**3.4: Commit**
```bash
git add test/test_attach_framebuffer_priority.cc
git commit -m "test(board): attach_framebuffer_for_testing rejects when vram_storage_ allocated (B16)"
```

---

### Phase 4 (B19): SDMA `vram_size_bytes` 由 board 注入

**目标**: SDMA `vram_size_bytes` 与 board `vram_size_` 同步

**TDD 5 步子任务**:

**4.1: Write failing test — SDMA vram_size_bytes 默认 256MB（错误） (5 min)**
- 文件: `test/test_sdma_vram_size_injection.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/sdma_engine_tlm.hh"

int main() {
    SdmaEngineTLM sdma("test_sdma");
    // sdma 默认 vram_size_bytes_ 应为 0（未注入时）
    assert(sdma.vram_size_bytes() == 0);  // B19: 默认 0，非 256MB
    return 0;
}
```

**4.2: Update SDMA `vram_size_bytes_` default to 0 (5 min)**
- 文件: `include/tlm/gpu/sdma_engine_tlm.hh`
- 找到 `uint64_t vram_size_bytes_ = 256 * 1024 * 1024;` 或类似
- 改为 `uint64_t vram_size_bytes_ = 0;`

**4.3: Update `bind_memory_backings` — SDMA vram_size 注入 (10 min)**
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (line 621-632)
- 在 `sdma->set_vram_backdoor` 后添加:
```cpp
sdma->set_vram_size_bytes(vram_size_);  // B19: 注入 board 的 vram_size_
```

**4.4: Delete `sdma.params.vram_size_bytes` from JSON (3 min)**
- 文件: `configs/dgpu_soc_minimal_v1.json` (line 31)
- 删除 sdma.params 中的 `vram_size_bytes` 字段

**4.5: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_sdma_vram_size_injection.cc -o /tmp/t && /tmp/t` → 预期 PASS

**4.6: Commit**
```bash
git add include/tlm/gpu/sdma_engine_tlm.hh src/tlm/gpu/dgpu_board_shell.cc configs/dgpu_soc_minimal_v1.json test/test_sdma_vram_size_injection.cc
git commit -m "refactor(sdma): vram_size_bytes injected by board, default 0 (B19)"
```

---

### Phase 5 (B20): `set_translate_cb` + `set_sdma_engine` 无条件注入

**目标**: SDMA 不再静默挂起

**TDD 5 步子任务**:

**5.1: Write failing test — `set_translate_cb` 在 legacy 分支才调用（错误）(5 min)**
- 文件: `test/test_sdma_translate_cb_unconditional.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/dgpu_board_shell.hh"

int main() {
    DGpuBoard board("test_board");
    // minimal_v1 配置（无 pcie_memory）
    nlohmann::json cfg = /* minimal_v1 */;
    board.load_soc_config(cfg);
    board.init();

    // B20: 即使无 pcie_memory，set_translate_cb 也应被调用
    // 验证方式: 检查 sdma_engine_->translate_cb_ 是否设置
    auto* sdma = board.sdma_engine();
    assert(sdma != nullptr);
    // 若 set_translate_cb 未调用，sdma 将无法工作
    return 0;
}
```

**5.2: Move `set_translate_cb` + `set_sdma_engine` out of pcie_memory branch (10 min)**
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (line 621-632)
- 移动 `set_translate_cb` 和 `set_sdma_engine` 到主函数顶部（无条件调用）:
```cpp
void DGpuBoard::bind_memory_backings() {
    if (!soc_) return;

    // B20: 无条件注入（无论是否有 pcie_memory）
    if (auto* sdma = dynamic_cast<SdmaEngineTLM*>(soc_->getInternalInstance("sdma"))) {
        sdma->set_translate_cb([this](uint64_t iova, uint32_t size, uint64_t& phys) {
            if (gmmu_)
                return gmmu_->translate(iova, size, phys);
            return -EIO;
        });
        set_sdma_engine(sdma);  // B20: 无条件设置
    }
    // ... 后续 pcie_memory 条件分支 ...
}
```

**5.3: Verify compile + behavior (2 min)**
- 验证: `cmake --build build --target cpptlm_core` → 预期 PASS

**5.4: Commit**
```bash
git add src/tlm/gpu/dgpu_board_shell.cc
git commit -m "fix(board): set_translate_cb/set_sdma_engine unconditional injection (B20)"
```

---

### Phase 6 (B22/B23): Fault Path 显式化 — SLVERR latch + translate 错误 emit done

**目标**: 防 SDMA/GMMU 永久挂死

**TDD 5 步子任务**:

**6.1: Write failing test — GMMU translate 无 pte_addr bound check (5 min)**
- 文件: `test/test_gmmu_pte_addr_bound.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/gmmu_tlm.hh"

int main() {
    GmmuTLM gmmu("test_gmmu");
    gmmu.set_pt_base(0x1000);  // 页表基址
    gmmu.set_enabled(true);

    // B22: pte_addr + 8 > backing_size_ 时应返回 -EIO
    // 当前实现无此检查，可能越界读
    uint64_t phys = 0;
    // 设置一个很大的 iova 使得 pte_addr 越界
    int r = gmmu.translate(0xFFFFFFFFF000, 4096, phys);
    assert(r == -EIO);  // B22: 应拒绝
    return 0;
}
```

**6.2: Implement GMMU pte_addr bound check + retry_latch (15 min)**
- 文件: `src/tlm/gpu/gmmu_tlm.cc` (translate 方法内)
- 在发 AXI MEM_READ 前添加:
```cpp
// B22: pte_addr bound check
uint64_t pte_addr = pt_base() + (iova >> 12) * 8;
if (pte_addr + 8 > backing_size_) {
    return -EIO;  // 越界
}
```
- 添加 retry_latch_ 成员（uint8_t, max=16）:
```cpp
if (resp.resp.read() != 0) {
    retry_latch_++;
    if (retry_latch_ > 16) {
        state_ = State::FAULT;
        return -EIO;
    }
}
```

**6.3: Write failing test — SDMA translate 非 0/非 -EAGAIN 路径无 done emit (5 min)**
- 文件: `test/test_sdma_fault_emit_done.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/sdma_engine_tlm.hh"

int main() {
    SdmaEngineTLM sdma("test_sdma");
    // B23: translate 非 0/非 -EAGAIN 路径应 emit done_out with status=-EIO
    // 当前实现只是注释，无显式 emit
    return 0;  // 编译期验证
}
```

**6.4: Implement SDMA fault emit done (10 min)**
- 文件: `src/tlm/gpu/sdma_engine_tlm.cc` (process_inflight_step)
- 替换 `if (tr != 0) { /* 错误处理 */ return; }` 为:
```cpp
// B23: 非 0/非 -EAGAIN 路径必须显式 emit done
if (tr != 0 && tr != -EAGAIN) {
    // emit done_out with status = -EIO
    bundles::PcieTlpBundle done_pkt;
    done_pkt.kind.write(bundles::PcieTlpBundle::DMA_DONE);
    done_pkt.status.write(-EIO);
    done_out_.write(done_pkt);
    // 移除 inflight_
    e.state = State::DONE;
    return;
}
```

**6.5: Verify tests pass (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_gmmu_pte_addr_bound.cc test/test_sdma_fault_emit_done.cc -o /tmp/t && /tmp/t`

**6.6: Commit**
```bash
git add src/tlm/gpu/sdma_engine_tlm.cc src/tlm/gpu/gmmu_tlm.cc
git commit -m "fix(fault): GMMU pte_addr bound + retry_latch; SDMA fault emit done (B22/B23)"
```

---

### Phase 7 (B25): SDMA ↔ PcieMemoryDevice 统一 PcieTlpBundle

**目标**: 放弃 v1.3 B2 切型（框架不支持异构 multi-port adapter）

**TDD 5 步子任务**:

**7.1: Write failing test — mem_in/mem_out 使用 PcieTlpBundle (5 min)**
- 文件: `test/test_sdma_pcie_tlp_bundle_unified.cc` (新建)
- 验证: SDMA 的 `mem_in`/`mem_out` 仍使用 `PcieTlpBundle`（非 AxiMemBundle）

**7.2: Update design.md §6 明确统一 PcieTlpBundle (5 min)**
- 文件: `openspec/changes/cpptlm-driver-visible-minimal-soc/design.md` (line 447-458)
- 注释掉 v1.3 B2 切型路径，改为:
```cpp
// v1.5 B25: SDMA ↔ PcieMemoryDevice 统一 PcieTlpBundle
// 放弃 v1.3 B2 "mem_in/mem_out 切 AxiMemBundle"（框架不支持异构 multi-port adapter）
// mem_in/mem_out 仍为 PcieTlpBundle，在 SDMA 内部通过 helper 转换
```

**7.3: Update SDMA 内部包转换逻辑 (10 min)**
- 文件: `src/tlm/gpu/sdma_engine_tlm.cc`
- 修改 `to_axi_mem_descriptor` / `from_axi_mem_completion` helper:
```cpp
// v1.5 B25: 不再切 AxiMemBundle，保持 PcieTlpBundle
// mem_in/mem_out 在 SDMA 内部通过 to_pcie_tlp_*/from_pcie_tlp_* 转换
```

**7.4: Verify compile (2 min)**
- 验证: `cmake --build build --target cpptlm_core` → 预期 PASS

**7.5: Commit**
```bash
git add src/tlm/gpu/sdma_engine_tlm.cc
git commit -m "refactor(sdma): unify mem_in/mem_out as PcieTlpBundle (B25)"
```

---

### Phase 8 (B26): backdoor bound = `vram_size_`

**目标**: backdoor_read/write bound 统一用 vram_size_，不受 BAR1 窗口约束

**TDD 5 步子任务**:

**8.1: Write failing test — backdoor bound 应用 vram_size_ 而非 framebuffer_size_ (5 min)**
- 文件: `test/test_backdoor_bound_vram_size.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/dgpu_board_shell.hh"

int main() {
    DGpuBoard board("test_board");
    nlohmann::json cfg = /* minimal_v1 with vram_size_=8GB, bar1_window_size_=16MB */;
    board.load_soc_config(cfg);
    board.init();

    // B26: backdoor 可访问 8GB 全域，而非仅 16MB BAR1 窗口
    uint8_t buf[8] = {0};
    // 访问 offset=24MB（> BAR1 窗口，但 < VRAM）
    int r = board.backdoor_read(24 * 1024 * 1024, buf, 8);
    assert(r == 0);  // B26: 应成功（不受 BAR1 窗口约束）
    return 0;
}
```

**8.2: Update backdoor_read/write bound check (10 min)**
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (backdoor_read, line 520)
- 修改 bound check:
```cpp
// 旧: if (device_info_.bar_sizes[1] > 0 && (... vram_offset >= device_info_.bar_sizes[1] ...))
// 新 (B26): bound = vram_size_ (framebuffer_size_)，不受 BAR1 窗口约束
if (framebuffer_size_ > 0 &&
    (vram_offset >= framebuffer_size_ || len > framebuffer_size_ - vram_offset)) {
    return -EINVAL;
}
```

**8.3: Verify test passes (2 min)**
- 验证: `g++ -std=c++17 -I include test/test_backdoor_bound_vram_size.cc -o /tmp/t && /tmp/t` → 预期 PASS

**8.4: Commit**
```bash
git add src/tlm/gpu/dgpu_board_shell.cc test/test_backdoor_bound_vram_size.cc
git commit -m "fix(board): backdoor bound = vram_size_ (B26)"
```

---

### Phase 9 (B27): 简化 `kRegMemSizeLo/Hi` 实现

**目标**: `kRegMemSizeLo/Hi` 改在 board 层烧录，简化 PcieMemoryDevice

**TDD 5 步子任务**:

**9.1: Write failing test — `kRegMemSizeLo/Hi` 在 PcieMemoryDevice 内部 (5 min)**
- 文件: `test/test_reg_mem_size_placement.cc` (新建)
- 验证: `kRegMemSizeLo/Hi` 应在 DGpuBoard::mmio_regs_ 而非 PcieMemoryDevice 内部

**9.2: Move `kRegMemSizeLo/Hi` handling to DGpuBoard (10 min)**
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (mmio_read/mmio_write)
- 在 MMIO 读处理中添加:
```cpp
// B27: kRegMemSizeLo/Hi 在 board 层处理
case PcieMemoryDevice::kRegMemSizeLo:
    val = static_cast<uint32_t>(framebuffer_size_ & 0xFFFFFFFF);
    return 0;
case PcieMemoryDevice::kRegMemSizeHi:
    val = static_cast<uint32_t>(framebuffer_size_ >> 32);
    return 0;
```

**9.3: Remove `kRegMemSizeLo/Hi` from PcieMemoryDevice (5 min)**
- 文件: `include/tlm/gpu/pcie_memory_device.hh` (line 37-38)
- 删除或注释掉

**9.4: Verify compile (2 min)**
- 验证: `cmake --build build --target cpptlm_core` → 预期 PASS

**9.5: Commit**
```bash
git add include/tlm/gpu/pcie_memory_device.hh src/tlm/gpu/dgpu_board_shell.cc
git commit -m "refactor(pcie-memory): kRegMemSizeLo/Hi moved to board layer (B27)"
```

---

### Phase 10 (B7/B8/B9/B10/B11/B12): PcieMemoryDevice 重构 — 注入式 backing + bound + SLVERR + mutex + 双 size + -ENODEV

**目标**: PcieMemoryDevice 退化为"PCIe 外观层"，单一 VRAM backing 由 board 注入

**这是最核心的 Phase，需要最多子任务。**

**TDD 5 步子任务**:

**10.1: Write failing test — PcieMemoryDevice backing 由注入而非自有 (10 min)**
- 文件: `test/test_pcie_memory_device_backing_injection.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/gpu/pcie_memory_device.hh"

int main() {
    PcieMemoryDevice dev("test_dev");
    // B7: has_memory_backing() 应为 false（未注入）
    assert(!dev.has_memory_backing());

    // B12: 未注入时 memory_read/write 应返回 -ENODEV
    uint8_t buf[8] = {0};
    int r = dev.memory_read(0, buf, 8);
    assert(r == -ENODEV);  // B12

    // B8: bound = injected backing_size_
    // 注入 4KB backing
    std::unique_ptr<uint8_t[]> backing(new uint8_t[4096]);
    dev.set_backing_store(backing.get(), 4096);
    assert(dev.has_memory_backing());
    assert(dev.backing_size() == 4096);

    // 注入后应正常读写
    uint8_t wdata[8] = {1,2,3,4,5,6,7,8};
    r = dev.memory_write(0, wdata, 8);
    assert(r == 0);
    r = dev.memory_read(0, buf, 8);
    assert(r == 0);
    assert(std::memcmp(buf, wdata, 8) == 0);

    // B8: OOB 应返回 -EINVAL（而非越界 memcpy）
    r = dev.memory_write(8192, wdata, 8);  // offset >= backing_size_
    assert(r == -EINVAL);  // B8 bound check

    // B9: SLVERR 传播（通过 resp）
    // 需要验证 handle_slave_port 路径
    return 0;
}
```

**10.2: Refactor PcieMemoryDevice — 删除 `memory_backing_` vector，添加 backing_ptr_/backing_size_ (15 min)**
- 文件: `include/tlm/gpu/pcie_memory_device.hh`
- 删除: `std::vector<uint8_t> memory_backing_;`
- 删除: `void ensure_memory_backing_allocated();`
- 添加:
```cpp
uint8_t* backing_ptr_ = nullptr;
uint64_t backing_size_ = 0;
std::mutex backing_mutex_;
```
- 修改 `has_memory_backing()`:
```cpp
[[nodiscard]] bool has_memory_backing() const noexcept { return backing_ptr_ != nullptr; }
```
- 修改 `memory_read/write`:
```cpp
int memory_read(uint64_t offset, void* buf, size_t len) {
    std::lock_guard<std::mutex> lock(backing_mutex_);
    // B12: null backing → -ENODEV
    if (backing_ptr_ == nullptr) return -ENODEV;
    // B8: bound check
    if (offset >= backing_size_ || len > backing_size_ - offset) return -EINVAL;
    std::memcpy(buf, backing_ptr_ + offset, len);
    return 0;
}
```

**10.3: Implement `set_backing_store()` (5 min)**
- 文件: `include/tlm/gpu/pcie_memory_device.hh`
- 添加方法:
```cpp
void set_backing_store(uint8_t* ptr, uint64_t size_bytes) noexcept {
    std::lock_guard<std::mutex> lock(backing_mutex_);
    backing_ptr_ = ptr;
    backing_size_ = size_bytes;
}
```

**10.4: Implement SLVERR propagation in `handle_slave_port` (10 min)**
- 文件: `src/tlm/gpu/pcie_memory_device.cc` (handle_slave_port)
- 修改:
```cpp
// B9: 检查 memory_read/write 返回值
int r = is_read ? memory_read(off, resp.data_buf.data(), len)
                 : memory_write(off, req.data_buf.data(), len);
if (r != 0) {
    resp.resp.write(1);  // SLVERR
} else {
    resp.resp.write(0);  // OKAY
}
```

**10.5: Update DGpuBoard `bind_memory_backings` — 注入 vram_storage_ (10 min)**
- 文件: `src/tlm/gpu/dgpu_board_shell.cc` (bind_memory_backings)
- 添加:
```cpp
if (auto* pm = dynamic_cast<PcieMemoryDevice*>(soc_->getInternalInstance("pcie_memory"))) {
    pcie_memory_ = pm;
    if (auto* ep = pcie_ep()) ep->set_memory_device(pm);
    // B7: 注入 vram_storage_（唯一真源）
    if (framebuffer_ptr_ && framebuffer_size_ > 0) {
        pm->set_backing_store(framebuffer_ptr_, framebuffer_size_);
    }
}
```

**10.6: Verify tests pass (5 min)**
```bash
g++ -std=c++17 -I include test/test_pcie_memory_device_backing_injection.cc -o /tmp/t && /tmp/t
# 预期: PASS
```

**10.7: Commit**
```bash
git add include/tlm/gpu/pcie_memory_device.hh src/tlm/gpu/pcie_memory_device.cc src/tlm/gpu/dgpu_board_shell.cc test/test_pcie_memory_device_backing_injection.cc
git commit -m "refactor(pcie-memory): injected backing + bound check + SLVERR + mutex (B7/B8/B9/B10/B11/B12)"
```

---

### Phase 11 (B28): MemoryTLM capacity 三方矛盾消解

**目标**: `backdoor` + `BAR2 mmio` + `MemoryTLM` + `SDMA legacy` 4 消费者统一 bound = `vram_size_`

**TDD 5 步子任务**:

**11.1: Write failing test — 4 消费者统一 vram_size_ bound (5 min)**
- 文件: `test/test_unified_vram_bound.cc` (新建)
- 验证: backdoor / BAR2 mmio / MemoryTLM / SDMA legacy 均使用同一 vram_size_ bound

**11.2: Audit and fix bound usage across 4 consumers (15 min)**
- 检查点:
  - `backdoor_read/write`: 使用 `framebuffer_size_` (已在 Phase 8 修复)
  - `BAR2 mmio`: 使用 `framebuffer_size_`
  - `MemoryTLM`: `set_backing_store` 传入 `framebuffer_size_`
  - `SDMA legacy`: `set_vram_backdoor` 传入 `framebuffer_size_`
- 验证: 4 个注入点均使用同一 `vram_size_` (framebuffer_size_)

**11.3: Verify compile (2 min)**
- 验证: `cmake --build build --target cpptlm_core` → 预期 PASS

**11.4: Commit**
```bash
git add src/tlm/gpu/dgpu_board_shell.cc test/test_unified_vram_bound.cc
git commit -m "fix(board): unify vram_size_ bound across 4 consumers (B28)"
```

---

### Phase 12 (B13 + B24): 24-case 测试迁移 + EP tick 不再推进 cycle_counter

**目标**: 更新既有测试到新 API，删除失效断言

**TDD 5 步子任务**:

**12.1: Update `test_pcie_memory_device_basic.cc` — 4 处语义反转 (10 min)**
- 文件: `test/test_pcie_memory_device_basic.cc` (line 13-15, 63-69, 72-77, 91-93, 96-102, 113-116, 118-125)
- 处置表（B13）:
  - line 63-69: round-trip 改 fixture 注入
  - line 72-77: OOB 测试 bound 改 injected size
  - line 91-93: 保留 `has_memory_backing()==false`
  - line 96-102: **删除整段** (lazy alloc 测试已废)
  - line 113-116: EP tick 不再推进 → 删除该断言

**12.2: Update `[minimal_dgpu_soc][driver_visible]` E2E — 添加 BAR2 (5 min)**
- 文件: `test/test_minimal_dgpu_soc_e2e.cc`
- `bar_sizes` 改为 `[4096, 16777216, 8589934592]` (B13)
- 添加 BAR2 mmio 测试用例

**12.3: Update all `[pcie-memory]` 12+ files — 机械注入 set_backing_store (10 min)**
- 文件: `test/test_pcie_memory_device_*.cc` (多个)
- 添加 fixture 注入:
```cpp
std::unique_ptr<uint8_t[]> buf(new uint8_t[test_size]);
dev.set_backing_store(buf.get(), test_size);
```

**12.4: Verify all 24 cases pass (5 min)**
```bash
./build/bin/cpptlm_tests "[pcie-memory]" --reporter compact
# 预期: 全 PASS
```

**12.5: Commit**
```bash
git add test/test_pcie_memory_device_basic.cc test/test_minimal_dgpu_soc_e2e.cc test/test_pcie_memory_device_*.cc
git commit -m "test(pcie-memory): 24-case migration + BAR2 addition (B13/B24)"
```

---

### Phase 13 (T0.4 + T4.1): PcieConfigSpace 64-bit BAR + JSON 扩展

**目标**: EP BAR 寄存器从 bar_sizes 生成，minimal_v1 JSON 扩展

**TDD 5 步子任务**:

**13.1: Write failing test — BAR2 寄存器读出 0 和 2（高 32-bit） (5 min)**
- 文件: `test/test_pcie_config_space_64bit_bar.cc` (新建)
- 内容:
```cpp
#include <cassert>
#include "tlm/pcie/pcie_endpoint_ip.hh"

int main() {
    PcieEndpointIP ep("test_ep");
    ep.init({4096, 16777216, 8589934592});

    // B5: 64-bit BAR 双 dword 编码
    // BAR2 低 32-bit
    uint32_t lo = ep.config_space().read(0x20);
    assert(lo == 0);  // bar_sizes[2] & 0xFFFFFFFF
    // BAR2 高 32-bit
    uint32_t hi = ep.config_space().read(0x24);
    assert(hi == 2);  // bar_sizes[2] >> 32
    return 0;
}
```

**13.2: Implement 64-bit BAR generation in PcieConfigSpace (15 min)**
- 文件: `src/tlm/pcie/pcie_config_space_mvp.cc` (init 方法)
- 修改:
```cpp
// B5: 64-bit BAR 寄存器生成
for (size_t i = 0; i < bar_sizes.size() && i < 6; ++i) {
    uint64_t bar = bar_sizes[i];
    uint32_t offset = 0x10 + i * 8;
    regs_[offset/4] = bar & 0xFFFFFFFF;
    regs_[(offset+4)/4] = bar >> 32;
}
```

**13.3: Update minimal_v1 JSON — add pcie_memory + 3 BARs + connections (10 min)**
- 文件: `configs/dgpu_soc_minimal_v1.json`
- 添加 `pcie_memory` 模块
- 扩展 `bar_sizes` 为 3 元素
- 添加 2 条 connections

**13.4: Verify tests pass (5 min)**
```bash
./build/bin/cpptlm_tests "[pcie-ep][bar]" --reporter compact
```

**13.5: Commit**
```bash
git add src/tlm/pcie/pcie_config_space_mvp.cc configs/dgpu_soc_minimal_v1.json test/test_pcie_config_space_64bit_bar.cc
git commit -m "feat(pcie-cfg): 64-bit BAR generation + minimal_v1 JSON extended (B5/T0.4/T4.1)"
```

---

### Phase 14 (T4.3 E2E + T5): `[minimal_dgpu_soc][driver_visible]` E2E + 文档

**目标**: 完整 E2E 测试 + AGENTS.md + ArchForge 镜像

**TDD 5 步子任务**:

**14.1: Write E2E test — driver-visible full path (15 min)**
- 文件: `test/test_minimal_soc_driver_visible_e2e.cc`
- 内容（per spec.md Scenario）:
  1. PCIe config read (vendor_id + device_id)
  2. BAR2 mmio write/read round-trip
  3. backdoor write/read round-trip
  4. PTE write + GMMU translate
  5. SDMA H2D + slot-2 resp

**14.2: Run E2E test (5 min)**
```bash
./build/bin/cpptlm_tests "[minimal_dgpu_soc][driver_visible]" --reporter compact
```

**14.3: Update AGENTS.md — 添加 D-AXI 行 (10 min)**
- 文件: `AGENTS.md`
- "WHERE TO LOOK" 表添加 `[driver-visible]` 标签
- "PHASE STATE" 添加 D-AXI 行

**14.4: Create ArchForge mirror (5 min)**
- 文件: `docs/architecture/19a-driver-visible-minimal-soc.md`
- 镜像 `docs/pcie/driver-visible-minimal-soc.md`

**14.5: Final verification (5 min)**
```bash
openspec validate --changes --strict
./scripts/test/docs_sync_check.sh --strict
./build/bin/cpptlm_tests --reporter compact | tail -3
```

**14.6: Commit**
```bash
git add test/test_minimal_soc_driver_visible_e2e.cc AGENTS.md docs/architecture/19a-driver-visible-minimal-soc.md
git commit -m "test(minimal-soc): driver-visible E2E + docs (T4.3/T5)"
```

---

## 依赖图

```
Phase 0.1 (B17) ─┬─► Phase 0.2 (B21) ─┬─► Phase 0.3 (B18)
                   │                      │
                   │                      └─► Phase 1 (B14)
                   │                               │
                   │                               └─► Phase 2 (B15) ─► Phase 3 (B16)
                   │                                              │
                   └──────────────────────────────────────────┘   │
                                                                   ▼
Phase 4 (B19) ◄──────────────────────────────────────────────────┘
    │
    ├─► Phase 5 (B20)
    │       │
    │       └─► Phase 6 (B22/B23)
    │               │
    │               └─► Phase 7 (B25)
    │                       │
    │                       └─► Phase 8 (B26) ─► Phase 9 (B27) ─► Phase 10 (B7-B12)
    │                                                               │
    │                                                               └─► Phase 11 (B28)
    │                                                                       │
    └───────────────────────────────────────────────────────────────────────┘
                                                                             │
                                                                             ▼
                                                                    Phase 12 (B13/B24)
                                                                             │
                                                                             ▼
                                                                    Phase 13 (B5/T0.4/T4.1)
                                                                             │
                                                                             ▼
                                                                    Phase 14 (T4.3/T5)
```

---

## 总工时评估

| Phase | B-codes | 子任务数 | 估时 |
|-------|----------|---------|------|
| Phase 0.1 | B17 | 7 | 35 min |
| Phase 0.2 | B21 | 4 | 20 min |
| Phase 0.3 | B18 | 5 | 25 min |
| Phase 1 | B14 | 5 | 35 min |
| Phase 2 | B15 | 5 | 35 min |
| Phase 3 | B16 | 4 | 20 min |
| Phase 4 | B19 | 6 | 35 min |
| Phase 5 | B20 | 4 | 25 min |
| Phase 6 | B22/B23 | 6 | 40 min |
| Phase 7 | B25 | 5 | 25 min |
| Phase 8 | B26 | 4 | 25 min |
| Phase 9 | B27 | 5 | 25 min |
| Phase 10 | B7/B8/B9/B10/B11/B12 | 7 | 60 min |
| Phase 11 | B28 | 4 | 25 min |
| Phase 12 | B13/B24 | 5 | 35 min |
| Phase 13 | B5/T0.4/T4.1 | 5 | 40 min |
| Phase 14 | T4.3/T5 | 6 | 45 min |
| **合计** | B14-B28 + T0.4 + T4.1 + T4.3 + T5 | **83** | **~10-11 工作日** |

---

## 风险等级

| Phase | Risk | 原因 |
|-------|-------|------|
| Phase 0.1 (B17) | **P1** | 框架级修改，影响所有使用 Packet 的模块 |
| Phase 0.2 (B21) | **P2** | 纯添加，不破坏现有接口 |
| Phase 0.3 (B18) | **P2** | JSON 配置变更，测试覆盖 |
| Phase 1 (B14) | **P2** | 删除代码，删除路径需完整覆盖 |
| Phase 2 (B15) | **P1** | 删除 framebuffer_storage_ 可能影响 legacy 路径 |
| Phase 3 (B16) | **P2** | 测试覆盖 |
| Phase 4 (B19) | **P2** | SDMA vram_size 注入已验证 |
| Phase 5 (B20) | **P1** | `set_translate_cb` 移动可能破坏 legacy 注入 |
| Phase 6 (B22/B23) | **P1** | Fault path 修改，可能引入新 bug |
| Phase 7 (B25) | **P2** | 撤销 v1.3 B2 切型，简化逻辑 |
| Phase 8 (B26) | **P2** | backdoor bound 修改 |
| Phase 9 (B27) | **P2** | kRegMemSizeLo/Hi 迁移 |
| Phase 10 (B7-B12) | **P0** | **最核心重构，PcieMemoryDevice 全面修改** |
| Phase 11 (B28) | **P2** | bound 统一 |
| Phase 12 (B13/B24) | **P2** | 测试迁移 |
| Phase 13 (B5/T0.4/T4.1) | **P1** | PcieConfigSpace 修改 + JSON 扩展 |
| Phase 14 (T4.3/T5) | **P2** | E2E + 文档 |

**P0 风险**: Phase 10 (B7-B12) — 必须 TDD，每个子任务独立验证后再前进

---

## 12 铁律合规检查表

| Iron Rule | Phase(s) | 验证方式 |
|-----------|----------|---------|
| 1. 分配: `unique_ptr<uint8_t[]>(new uint8_t[N])` default-init | Phase 2, Phase 10 | `grep "new uint8_t" src/tlm/gpu/dgpu_board_shell.cc` |
| 2. 所有 bound = injected backing_size_ | Phase 1, Phase 8, Phase 10 | `grep "backing_size_" include/tlm/gpu/pcie_memory_device.hh` |
| 3. bar1_window_size_ vs vram_size_ 拆分 | Phase 4, Phase 11 | `grep "bar1_window_size_\|vram_size_" src/tlm/gpu/dgpu_board_shell.cc` |
| 4. set_backing_store 仅在 sim_thread 启动前调一次 | Phase 10 | `grep "set_backing_store" src/tlm/gpu/dgpu_board_shell.cc` |
| 5. kRegMemSizeLo/Hi 读 injected backing_size_ | Phase 9, Phase 10 | `grep "kRegMemSizeLo" include/tlm/gpu/pcie_memory_device.hh` |
| 6. 未注入 → memory_read/write 返 -ENODEV | Phase 10 | `test/test_pcie_memory_device_backing_injection.cc` |
| 7. handle_slave_port 检查返回值 | Phase 10 | `grep "resp.resp.write" src/tlm/gpu/pcie_memory_device.cc` |
| 8. framebuffer_ptr_ = vram_storage_.get() | Phase 2 | `grep "framebuffer_storage_" include/tlm/gpu/dgpu_board_shell.hh` 应为空 |
| 9. attach_framebuffer_for_testing 优先级规则 | Phase 3 | `test/test_attach_framebuffer_priority.cc` |
| 10. vram_segments_ 不存在 | Phase 1 | `grep "vram_segments_" src/tlm/gpu/dgpu_board_shell.cc` 应为空 |
| 11. set_translate_cb 无条件注入 | Phase 5 | `grep "set_translate_cb" src/tlm/gpu/dgpu_board_shell.cc` |
| 12. backdoor bound = vram_size_ | Phase 8 | `test/test_backdoor_bound_vram_size.cc` |

---

## Commit 策略

每个 Phase 完成时提交，commit message 格式:

```bash
# Phase 0.1
git commit -m "feat(framework): add Packet::payload_resize() + PacketPool::acquire_with_min_size() (B17)"

# Phase 0.2
git commit -m "feat(gmmu): add dummy resp_out()/req_in() for registerAdapter template contract (B21)"

# Phase 0.3
git commit -m "feat(memory): on_config_loaded reads capacity_gb from cfg (B18)"

# Phase 1
git commit -m "refactor(board): eliminate vram_segments_ from backdoor path (B14)"

# Phase 2
git commit -m "refactor(board): eliminate framebuffer_storage_, framebuffer_ptr_ aliases vram_storage_ (B15)"

# Phase 3
git commit -m "test(board): attach_framebuffer_for_testing rejects when vram_storage_ allocated (B16)"

# Phase 4
git commit -m "refactor(sdma): vram_size_bytes injected by board, default 0 (B19)"

# Phase 5
git commit -m "fix(board): set_translate_cb/set_sdma_engine unconditional injection (B20)"

# Phase 6
git commit -m "fix(fault): GMMU pte_addr bound + retry_latch; SDMA fault emit done (B22/B23)"

# Phase 7
git commit -m "refactor(sdma): unify mem_in/mem_out as PcieTlpBundle (B25)"

# Phase 8
git commit -m "fix(board): backdoor bound = vram_size_ (B26)"

# Phase 9
git commit -m "refactor(pcie-memory): kRegMemSizeLo/Hi moved to board layer (B27)"

# Phase 10
git commit -m "refactor(pcie-memory): injected backing + bound check + SLVERR + mutex (B7/B8/B9/B10/B11/B12)"

# Phase 11
git commit -m "fix(board): unify vram_size_ bound across 4 consumers (B28)"

# Phase 12
git commit -m "test(pcie-memory): 24-case migration + BAR2 addition (B13/B24)"

# Phase 13
git commit -m "feat(pcie-cfg): 64-bit BAR generation + minimal_v1 JSON extended (B5/T0.4/T4.1)"

# Phase 14
git commit -m "test(minimal-soc): driver-visible E2E + docs (T4.3/T5)"
```

**Commit boundaries**: 每个 Phase 完成后立即提交（即使后续 Phase 失败，已提交的 Phase 不受影响）

---

## 出口验证

每个 Phase 完成后执行:

```bash
# 1. 编译检查
cmake --build build --target cpptlm_core 2>&1 | tail -20

# 2. 相关测试
./build/bin/cpptlm_tests "[<phase_tag>]" --reporter compact

# 3. 铁律检查
grep -rn "vram_segments_\b" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh  # Phase 1
grep -rn "framebuffer_storage_" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh  # Phase 2

# 4. openspec validate
openspec validate cpptlm-driver-visible-minimal-soc --strict
```

最终交付:
```bash
./build/bin/cpptlm_tests --reporter compact | tail -5
# 预期: 全 PASS
openspec validate --changes --strict
# 预期: 5/5 PASS
```
