# cpptlm-dgpu-abi-export: Design

> **配套**: [`proposal.md`](./proposal.md) · [`tasks.md`](./tasks.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D5 + Status Update Q2/Q6 · UsrLinuxEmu ADR-088 §D5

## 1. 构建目标布局

build/
- lib/libcpptlm_core.a      (STATIC + PIC)
- lib/libcpptlm_emulator.so (SHARED, link cpptlm_core, 导出 23 ABI)

关键约束：
- cpptlm_core 保持 STATIC（per Oracle Q2：PTX-EMU 端 ExternalProject_Add 静态消费路径不能破坏），仅追加 POSITION_INDEPENDENT_CODE ON 使其可被链入 .so
- cpptlm_emulator SHARED 是唯一 ABI 暴露点——所有 23 ABI extern "C" 函数在此编译，符号默认 -fvisibility=hidden，仅 23 个 ABI 名字显式 __attribute__((visibility("default")))
- UsrLinuxEmu 端通过 find_package(cpptlm) 或直接 dlopen libcpptlm_emulator.so 调用；PTX-EMU 端继续静态链 cpptlm_core，ABI 完全不影响 PTX-EMU 路径

## 2. C 头文件补充（change C T-bs-6 已冻结）

include/abi/cpptlm_emulator.h 仅添加导出宏，无实现改动：

```c
#if defined(_WIN32) || defined(__CYGWIN__)
#  define CPPTLM_EMULATOR_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  define CPPTLM_EMULATOR_EXPORT __attribute__((visibility("default")))
#else
#  define CPPTLM_EMULATOR_EXPORT
#endif
// 全部 16 forward + 3 callback-register 函数（`cpptlm_emulator_register_callbacks` / `_register_backdoor_cb` / `_register_dma_translate_cb`）应用此宏；4 callback typedef 仅头文件声明不导出
```

## 3. 23 ABI 函数体实现模式

所有 forward 函数遵循同一壳（细节见 proposal.md 映射表 16/17/22/23 行）：

```cpp
namespace {
std::mutex registry_mu_;
std::unordered_map<uint32_t, DGpuBoard*> registry_;
DGpuBoard* lookup(uint32_t dev_id) {
    std::lock_guard<std::mutex> lk(registry_mu_);
    auto it = registry_.find(dev_id);
    return (it != registry_.end()) ? it->second : nullptr;
}
}  // namespace

extern "C" CPPTLM_EMULATOR_EXPORT
const char* cpptlm_emulator_get_version(void) { return "v1.0-dgpu-v0"; }

extern "C" CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_mmio_read(uint32_t dev_id, uint8_t bar, uint64_t offset,
                              void* buf, uint32_t size) {
    try {
        auto* board = lookup(dev_id);
        if (!board) return -1;
        return board->mmio_read(bar, offset, buf, size);
    } catch (const std::exception&) { return -EINVAL; }
      catch (...) { return -EFAULT; }
}
```

关键点：
- 每个 ABI 函数不做业务逻辑——薄壳转发到 DGpuBoard shell 方法（change C 交付）
- 同步/异步边界已在 shell 层处理（per change C design §2.5）：mmio_read 等 future；mmio_write 注入队列即返回；backdoor 走 inject_q quantum 边界服务
- 异常边界：每个 extern "C" 函数体顶层 try/catch 全包，C 边界不容许 C++ 异常逃逸（UB）

## 4. 设备注册表 mutex 语义（per Q6 多线程）

创建路径（锁内只做登记 + 启动 sim_thread，不持锁 join）：
```cpp
extern "C" CPPTLM_EMULATOR_EXPORT
uint32_t cpptlm_emulator_create_by_id(uint32_t dev_id) {
    std::unique_lock<std::mutex> lk(registry_mu_);
    if (dev_id != 0 && registry_.count(dev_id)) return dev_id;  // 幂等
    auto path = resolve_profile_path(dev_id);
    auto* board = new DGpuBoard("board_" + std::to_string(dev_id_or_next));
    board->load_soc_config(parse_json(path));
    // NOTE: board->init() 内部启动 sim_thread_ 可能耗时，但在此处持有 registry_mu_ 会
    // 阻塞其他卡的 mmio_read（也走 lookup() → registry_mu_）。这是已知权衡：
    // ① 在 init() 期间对其他卡的并发 ABI 调用会阻塞至 init 完成；
    // ② 可改进方案：锁内预留 dev_id → 锁外 init() → 锁内 insert（避免持锁 init），
    //    但需解决两个 create 并发请求相同 dev_id 的 race（需双检查）。
    // 当前实现选择简单优先（per design §4 陷阱 4 的简化路径），文档化此限制供后续优化。
    board->init();  // init 内部启动 sim_thread_
    uint32_t assigned = (dev_id != 0) ? dev_id : next_dev_id_++;
    registry_[assigned] = board;
    return assigned;
}
```

销毁路径（锁内 erase + 解锁后 delete）：
```cpp
extern "C" CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_destroy(uint32_t dev_id) {
    DGpuBoard* board = nullptr;
    { std::lock_guard<std::mutex> lk(registry_mu_);
      auto it = registry_.find(dev_id);
      if (it == registry_.end()) return -1;
      board = it->second;
      registry_.erase(it);
    }  // 解锁
    delete board;  // ~DGpuBoard: stop_=true → join sim_thread_ → destruct
    return 0;
}
```

陷阱：
1. 销毁顺序：必须在锁外 delete；~DGpuBoard 内 join sim_thread_ 可能耗时（quantum 边界），持锁 join 会让其他 API 调用阻塞
2. destroy 期间的 API 调用：正在 destroy 的设备若被另一线程调 mmio_read，lookup 返回 null → 返回 -1；不抛异常
3. 静态路径：get_device_count / get_device_info 不持 board 句柄，扫描 configs/dgpu_*.json 即可，无锁
4. 多卡创建期间：create_by_id 期间任何对其他 board 的 ABI 调用允许并发（不持单 board 锁），仅 registry_mu_ 串行

## 5. CMake 变更（细节见 tasks.md T-ae-1）

- `src/CMakeLists.txt`：`set_target_properties(cpptlm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)`（target 定义于该文件 line 84；PIC 是构建属性非源码改动，PTX-EMU 静态消费路径不变）
- 根 `CMakeLists.txt`：install(cpptlm_emulator) + install(EXPORT cpptlmTargets) → 生成 cpptlmConfig.cmake 由 UsrLinuxEmu 端 find_package 自动消费
- src/CMakeLists.txt：add_library(cpptlm_emulator SHARED abi/cpptlm_emulator.cc) + -fvisibility=hidden + target_link_libraries(cpptlm_emulator PRIVATE cpptlm_core)

## 6. 测试策略

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| test_cpptlm_emulator_abi.cc | [abi] | create→mmio_write/read→pcie_config_read/write→msix_init/update/clear→backdoor_read/write→destroy 全链路 PASS；含 ABI 与 shell 行为等价对比 |
| test_cpptlm_emulator_registry.cc | [abi][concurrent] | 2+ 卡并发 create_by_id + mmio_read + destroy，TSan 干净；destroy 期间其他 API 调用返回 -1 |

dlopen 验证：examples/test_cpptlm_emulator_dlopen/ 单独二进制，dlopen libcpptlm_emulator.so → dlsym 5+ ABI → 创建→destroy，作为 UsrLinuxEmu 端集成模板

## 7. 风险与缓解

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | cpptlm_core 加 PIC 触发全量重编，影响 PTX-EMU build_ptx_emu.sh | 全量重编仅一次（PIC 是构建属性非源码改动）；PTX-EMU submodule 静态消费路径不变 |
| R2 | 23 ABI 函数体内异常逃逸 SO 边界 = UB | 每个 extern "C" 函数顶层 try/catch 全包，返回 -EINVAL / -EFAULT |
| R3 | 多卡注册表 mutex 持锁 join 死锁 | create/destroy 锁仅覆盖 registry 登记/擦除，delete board / ~DGpuBoard::join 在锁外执行 |
| R4 | nm -D 导出检查遗漏——某些 ABI 因宏未应用 CPPTLM_EMULATOR_EXPORT 而隐藏 | AE-G2 必须 nm -D 断言 16 forward + 3 callback-register 符号可见（总共 19 个 `cpptlm_emulator_*` 函数符号；4 typedef 不导出）；CI 集成精确计数（`grep -c == 19`） |
| R5 | UsrLinuxEmu 端链接 SHARED 时找不到符号 → dlopen fail | AE-G5 dlopen 示例必须 PASS；CI 在仓库内 smoke test |
| R6 | 与 PTX-EMU 既有 cpptlm_* 命名冲突 | nm 查 cpptlm_emulator_* (本 change) 与 cpptlm_* (PTX-EMU 历史) 零冲突（命名空间隔离：cpptlm_emulator_* 前缀） |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 待评审后实施
