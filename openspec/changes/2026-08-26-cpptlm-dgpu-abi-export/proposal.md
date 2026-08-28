# cpptlm-dgpu-abi-export: 23 ABI C 函数体实现 + cpptlm_emulator SHARED 库 + cpptlm_core PIC + 全局设备注册表 mutex

> **状态**: 📋 Proposed — 2026-08-26 · **日期**: 2026-08-26 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D5 + Status Update Q2/Q6 · UsrLinuxEmu [ADR-088 §D5](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) (23 ABI 契约)
> **依赖**:
> - [`2026-08-26-cpptlm-dgpu-board-soc-split`](../2026-08-26-cpptlm-dgpu-board-soc-split/) **必须已 archive**（T-bs-6 已交付 `include/abi/cpptlm_emulator.h` 纯声明头，shell 5 职责就位）
> - [`2026-08-26-cpptlm-dgpu-pcie-endpoint`](../2026-08-26-cpptlm-dgpu-pcie-endpoint/) + [`-sdma-engine`](../2026-08-26-cpptlm-dgpu-sdma-engine/) **应已 archive**（backdoor / DMA translate callback 调用点需要组件就位）

---

## Why

ADR-088 §D5 定义 23 ABI 是 CppTLM 对 UsrLinuxEmu 的**外部契约**。change C 的 T-bs-6 已在文件层冻结 C 头文件（`include/abi/cpptlm_emulator.h`，0 实现），本 change 补齐函数体，把 CppTLM 的 dGPU board 仿真能力以 SHARED 库形态暴露给 UsrLinuxEmu 真正 dlopen/链接。

**必要性（per Oracle Q2 评审）**：

1. **23 ABI 当前 0 命中**（跨仓 grep 验证）：UsrLinuxEmu linux_compat/ 已定义 ABI 调用契约，但 CppTLM 端无任何 `extern "C"` 导出，集成测试无法跑通；
2. **cpptlm_core 是 STATIC**（`src/CMakeLists.txt:84`），UsrLinuxEmu `ExternalProject_Add` 静态消费路径不变；但 ABI 函数体的 SHARED 包装层需独立 target；
3. **跨仓发布形态变更风险**：把 `cpptlm_core` 改成 PIC + SHARED 会破坏 PTX-EMU 端既有 `ExternalProject_Add` 路径（per ADR-SOC-07 D5），所以策略是 **`cpptlm_core` 增 PIC 但保持 STATIC** + 新建独立 SHARED `cpptlm_emulator` target 包装 ABI；
4. **多卡多线程**：Q6 裁决每卡独立线程 + 注入队列 → 设备注册表必须 mutex 保护（`create_by_id`/`destroy` 期间）。

**触发事件**：
- change C T-bs-6 已冻结 C 头（声明 0 实现）
- Oracle 评审 Q2 明确"独立 change，SHARED + PIC + mutex"
- UsrLinuxEmu ADR-088 §D5 已锁定 23 ABI 名 + §D3 给定大多数签名 + §D6.2 Semver `"v1.0-dgpu-v0"`

---

## What Changes

### 1. 新建文件

| 文件 | 用途 |
|------|------|
| `src/abi/cpptlm_emulator.cc` | **🆕** 23 ABI C 函数体实现（薄包装：device registry lookup → DGpuBoard shell 方法调用） |
| `test/test_cpptlm_emulator_abi.cc` | 端到端 ABI 测试（创建→mmio_write→mmio_read→pcie_config_read→msix→backdoor→destroy，6-8 用例） |
| `test/test_cpptlm_emulator_registry.cc` | 多设备注册表 mutex 并发测试（2+ 卡并发 create_by_id/destroy，验证无 race） |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `src/CMakeLists.txt`（PIC 启用：`set_target_properties(cpptlm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)`，target 定义于 line 84）+ 根 `CMakeLists.txt` | install 规则补 `cpptlm_emulator` 并 `EXPORT cpptlmTargets`（per design §5） |
| `src/CMakeLists.txt` | 新增 `cpptlm_emulator` SHARED target：`add_library(cpptlm_emulator SHARED src/abi/cpptlm_emulator.cc)` + `target_link_libraries(cpptlm_emulator PRIVATE cpptlm_core)` + `-fvisibility=hidden` 默认 + 23 个 `__attribute__((visibility("default")))` 显式导出 |
| `include/abi/cpptlm_emulator.h` | **零实现改动**（change C T-bs-6 已冻结）；仅添加 `CPPTLM_EMULATOR_EXPORT` 宏（与 visibility 一致） |
| `examples/test_cpptlm_emulator_dlopen/` | 最小 dlopen 验证示例（UsrLinuxEmu 端用法镜像） |

### 3. 边界（不修改）

- **不改 `cpptlm_core` 从 STATIC 改 SHARED**（per Oracle Q2 警告：会破坏 PTX-EMU 端 `ExternalProject_Add` 静态消费路径）；
- **不实现系统 IOMMU**——`cpptlm_dma_translate_cb` 由 UsrLinuxEmu `src/system_hw/iommu/` 注入，本 change 仅注册 callback（per ADR-088 §D6.1）；
- **不实现 system_hw/cxl_memdev**（非 CppTLM 范围）。

---

## ABI 实现映射表（per ADR-088 §D5）

| # | ABI 函数 | 调用 DGpuBoard shell 方法 | 备注 |
|---|---------|--------------------------|------|
| 1 | `cpptlm_emulator_get_version()` | 返回字符串字面量 | `"v1.0-dgpu-v0"` (per §D6.2) |
| 2 | `cpptlm_emulator_create(profile_path)` | `DGpuBoard::create(profile_path)` → 返回 opaque handle | profile_path 是 board JSON 路径 |
| 3 | `cpptlm_emulator_mmio_read(emu, bar, off, buf, size)` | `board->mmio_read(bar, off, buf, size)` | 同步等待 future |
| 4 | `cpptlm_emulator_mmio_write(emu, bar, off, buf, size)` | `board->mmio_write(bar, off, buf, size)` | 同步等待 |
| 5 | `cpptlm_emulator_lookup_register(emu, off, *info)` | `board->lookup_register(off)` | 寄存器元数据查询 |
| 6 | `cpptlm_emulator_destroy(emu)` | `board->destroy()` | 触发 §2.5 destroy 顺序 |
| 7 | `cpptlm_emulator_get_device_count()` | 静态：扫描 `configs/dgpu_*.json` 或读取已注册表 | 返回 `uint32_t` |
| 8 | `cpptlm_emulator_get_device_info(dev_id, *info)` | 静态：从 device profile JSON 读取 + 填 `cpptlm_device_info_t` | per §D3.1 |
| 9 | `cpptlm_emulator_create_by_id(dev_id)` | 静态：根据 dev_id 找 profile path → `create(path)` | per §D3.2 |
| 10 | `cpptlm_emulator_pcie_config_read(emu, bus, dev, fn, off, w, *val)` | `board->pcie_config_read(off, w, val)` | per §D3.3 |
| 11 | `cpptlm_emulator_pcie_config_write(emu, bus, dev, fn, off, w, val)` | `board->pcie_config_write(off, w, val)` | per §D3.3 |
| 12 | `cpptlm_emulator_backdoor_read(emu, bar, off, buf, len)` | `board->backdoor_read(bar, off, buf, len)` | 走 inject_q per §2.5 |
| 13 | `cpptlm_emulator_backdoor_write(emu, bar, off, buf, len)` | `board->backdoor_write(bar, off, buf, len)` | 走 inject_q |
| 14 | `cpptlm_emulator_msix_init(emu, table_size, mask)` | `board->msix_init(table_size, mask)` | per §D3.5 |
| 15 | `cpptlm_emulator_msix_update_pending(emu, vector)` | `board->msix_update_pending(vector)` | 触发 sim→host 中断 callback |
| 16 | `cpptlm_emulator_msix_clear_pending(emu, vector)` | `board->msix_clear_pending(vector)` | driver EOI 路径 |
| 17-20 | 4 callback typedefs | 仅头文件声明，0 实现 | `cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t` |
| 21 | `cpptlm_emulator_register_callbacks(emu, cbs)` | `board->register_callbacks(cbs)` | 一次性绑定 4 个回调 |
| 22 | `cpptlm_emulator_register_backdoor_cb(emu, cb)` | `board->register_backdoor_cb(cb)` | per §D3.4 |
| 23 | `cpptlm_emulator_register_dma_translate_cb(emu, cb)` | `board->register_dma_translate_cb(cb)` | per §D3.8 |

---

## 端口协议：device registry mutex

```cpp
namespace {

struct DeviceEntry {
    void*                handle;         // opaque handle (== DGpuBoard*)
    DGpuBoard*           board;
};

std::mutex                                registry_mu_;
std::unordered_map<uint32_t, DeviceEntry>  registry_;   // dev_id → entry
std::atomic<uint32_t>                      next_dev_id_{1};

// create_by_id 路径
uint32_t create_by_id(uint32_t requested_dev_id) {
    std::lock_guard<std::mutex> lk(registry_mu_);
    if (requested_dev_id != 0 && registry_.count(requested_dev_id)) {
        return requested_dev_id;  // 幂等
    }
    auto path = resolve_profile_path(requested_dev_id);
    DGpuBoard* board = new DGpuBoard("board_" + std::to_string(...));
    board->load_soc_config(parse_json(path));
    board->init();
    // 多线程 Q6：sim_thread_ 在 board init 时启动
    uint32_t dev_id = (requested_dev_id != 0) ? requested_dev_id : next_dev_id_++;
    registry_[dev_id] = DeviceEntry{static_cast<void*>(board), board};
    return dev_id;
}

// destroy 路径
int destroy(uint32_t dev_id) {
    std::unique_lock<std::mutex> lk(registry_mu_);
    auto it = registry_.find(dev_id);
    if (it == registry_.end()) return -1;
    DGpuBoard* board = it->second.board;
    registry_.erase(it);
    lk.unlock();   // 释放锁后再 join sim_thread_（避免 self-deadlock）
    delete board;  // ~DGpuBoard: stop_=true → join → destruct SOC
    return 0;
}

} // namespace
```

---

## Acceptance Gate（本 change）

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **AE-G1** `cpptlm_core` PIC 启用，全量编译 PASS | CppTLM | ⏳ | `cmake --build build` PASS |
| **AE-G2** `cpptlm_emulator` SHARED target 构建 + 23 个 `cpptlm_emulator_*` + `cpptlm_*_cb_t` 符号导出 | CppTLM | ⏳ | `nm -D --defined-only build/lib/libcpptlm_emulator.so | grep -c 'cpptlm_emulator_' == 19`（16 forward + 3 register 函数；4 个 typedef 仅头文件声明，不导出为符号） |
| **AE-G3** 端到端 ABI 测试 PASS：create→mmio_write→mmio_read→pcie_config_read→msix→backdoor→destroy | CppTLM | ⏳ | `ctest -R "test_cpptlm_emulator_abi"` PASS |
| **AE-G4** 多卡注册表 mutex 并发测试 PASS（2+ 卡并发 create_by_id/destroy，无 race） | CppTLM | ⏳ | `cmake -B build-tsan -DUSE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build-tsan && ctest --test-dir build-tsan -R "test_cpptlm_emulator_registry" --output-on-failure` PASS（含 TSan, Debug × TSan 模式 PASS，零 race） |
| **AE-G5** dlopen 示例（`examples/test_cpptlm_emulator_dlopen/`）编译并跑通 | CppTLM | ⏳ | 示例二进制返回 `"v1.0-dgpu-v0"` 并成功 create/destroy |
| **AE-G6** 全量无回归 + documents sync | CppTLM | ⏳ | `build/bin/cpptlm_tests` 全 PASS + `docs_sync_check.sh --strict` PASS |

**最终验收**: AE-G1~G6 全部 ✅ + 本 change 可独立 archive。

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **UsrLinuxEmu** | ADR-088 §D5 23 ABI（**由本 change 实现**）+ §D3 签名冻结；可执行 `find_package(cpptlm)` 链入 `cpptlm_emulator` SHARED；dlopen 示例作为集成模板 | ⏳ 待 CppTLM archive |
| **PTX-EMU** | 无（cpptlm_core 仍 STATIC，`ExternalProject_Add` 路径不变） | ✅ N/A |

---

**起草**: Sisyphus (2026-08-26, per Oracle Q2 评审)
**Owner**: CppTLM Team
**状态**: 📋 Proposed — 依赖 change C archive（含 T-bs-6 C 头冻结）
