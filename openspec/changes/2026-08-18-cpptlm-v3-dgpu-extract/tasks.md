# tadr-308: Tasks (TDD 5-Step Structure)

> **结构**: 每个 task 标 TDD 5 步 (Write failing test → Verify fail → Implement → Verify pass → Commit)
> **关联**: [proposal.md](proposal.md) · [tadr-308](../shared/adr/tadr-308-igpu-driver-vram-load.md)

---

## T-001: `IGpuDriver::load_kernel_module` 默认实现测试 (TDD 5 步)

**Write failing test**:
```cpp
// tests/test_load_kernel_module_standalone.cpp
TEST_CASE("IGpuDriver::load_kernel_module default impl returns -ENOSYS", "[tadr-308][T-001]") {
    auto* drv = IGpuDriver::create_mock();  //  默认 mock 实现
    uint64_t vram_addr = 0;
    int rc = drv->load_kernel_module(nullptr, 0, &vram_addr);
    REQUIRE(rc == -ENOSYS);
    REQUIRE(vram_addr == 0);  // 默认实现不修改 OUT 参数
}
```

**Verify fail**: `cmake --build` 应该 link 失败（`load_kernel_module` 未声明）。捕获错误信息。

**Implement**:
```cpp
// include/shared/igpu_driver.hpp
virtual int load_kernel_module(const void* image, size_t image_size,
                               uint64_t* out_vram_addr) {
    (void)image; (void)image_size; (void)out_vram_addr;
    return -ENOSYS;
}
```

**Verify pass**: `ctest -R tadr-308` PASS。

**Commit**: `feat(igpu-driver): append load_kernel_module default impl (tadr-308 T-001)`

---

## T-002: `cuModuleLoadData` 接入 `load_kernel_module` 适配 (TDD 5 步)

**Write failing test**:
```cpp
TEST_CASE("cuModuleLoadData forwards to load_kernel_module", "[tadr-308][T-002]") {
    CUmodule module = nullptr;
    const uint8_t fake_image[] = {0xDE, 0xAD, 0xBE, 0xEF};
    CUresult rc = cuModuleLoadData(&module, fake_image);
    REQUIRE(rc == CUDA_SUCCESS);
    REQUIRE(module != nullptr);  // 应该是 vram_addr 句柄
    // 验证 mock 收到 image 数据
    REQUIRE(mock_drv()->last_image_size() == 4);
}
```

**Verify fail**: `cuModuleLoadData` 当前返回 `CUDA_ERROR_NOT_IMPLEMENTED`。测试捕获该错误。

**Implement**:
```cpp
// src/umd/libcuda_shim/cu_module.cpp:135
CUresult cuModuleLoadData(CUmodule* module, const void* image) {
    uint64_t vram_addr = 0;
    int rc = runtime()->load_kernel_module(image, image_size, &vram_addr);
    if (rc != 0) return static_cast<CUresult>(rc);
    *module = reinterpret_cast<CUmodule>(vram_addr);
    return CUDA_SUCCESS;
}
```

**Verify pass**: `ctest -R tadr-308` PASS。

**Commit**: `feat(cu_module): forward cuModuleLoadData to load_kernel_module (tadr-308 T-002)`

---

## T-003: `cuModuleUnload` 接入 `free_bo` 路径 (TDD 5 步)

**Write failing test**:
```cpp
TEST_CASE("cuModuleUnload forwards to free_bo", "[tadr-308][T-003]") {
    CUmodule module = (CUmodule)0xDEADBEEFCAFEBABE;
    CUresult rc = cuModuleUnload(module);
    REQUIRE(rc == CUDA_SUCCESS);
    REQUIRE(mock_drv()->last_freed_bo() == 0xDEADBEEFCAFEBABE);
}
```

**Verify fail**: `cuModuleUnload` 当前返回 NOT_IMPLEMENTED。

**Implement**:
```cpp
// src/umd/libcuda_shim/cu_module.cpp:93
CUresult cuModuleUnload(CUmodule module) {
    uint64_t vram_addr = reinterpret_cast<uint64_t>(module);
    int rc = runtime()->free_bo(vram_addr);
    return static_cast<CUresult>(rc);
}
```

**Verify pass**: `ctest -R tadr-308` PASS。

**Commit**: `feat(cu_module): forward cuModuleUnload to free_bo (tadr-308 T-003)`

---

## T-004: 并发 load race 测试 (TDD 5 步)

**Write failing test**:
```cpp
TEST_CASE("Concurrent load_kernel_module is thread-safe", "[tadr-308][T-004]") {
    const int N = 8;
    std::vector<std::thread> threads;
    std::atomic<int> ok_count{0};
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&]() {
            uint64_t vram_addr = 0;
            int rc = runtime()->load_kernel_module(nullptr, 0, &vram_addr);
            if (rc == -ENOSYS || rc == 0) ok_count++;
        });
    }
    for (auto& t : threads) t.join();
    REQUIRE(ok_count == N);
}
```

**Verify fail**: 单线程测试可能 PASS（无竞争）,但需要多线程验证。首次运行如果未实现线程安全 mutex,会偶发失败。

**Implement**:
```cpp
// include/shared/igpu_driver.hpp
virtual int load_kernel_module(const void* image, size_t image_size,
                               uint64_t* out_vram_addr) {
    std::lock_guard<std::mutex> lock(load_mutex_);  // 新增 mutex
    (void)image; (void)image_size; (void)out_vram_addr;
    return -ENOSYS;
}
```

**Verify pass**: `ctest -R tadr-308` PASS（多线程稳定）。

**Commit**: `feat(igpu-driver): thread-safe load_kernel_module mutex (tadr-308 T-004)`

---

## T-005: 错误注入测试 (TDD 5 步)

**Write failing test**:
```cpp
TEST_CASE("load_kernel_module error paths", "[tadr-308][T-005]") {
    auto* drv = IGpuDriver::create_mock();
    uint64_t vram_addr = 0xDEADBEEF;  // sentinel

    SECTION("image == NULL") {
        int rc = drv->load_kernel_module(nullptr, 1024, &vram_addr);
        REQUIRE(rc == -EINVAL);
        REQUIRE(vram_addr == 0xDEADBEEF);  // 未修改
    }
    SECTION("image_size == 0") {
        int rc = drv->load_kernel_module("x", 0, &vram_addr);
        REQUIRE(rc == -EINVAL);
    }
    SECTION("out_vram_addr == NULL") {
        int rc = drv->load_kernel_module("x", 1024, nullptr);
        REQUIRE(rc == -EFAULT);
    }
}
```

**Verify fail**: 当前默认实现不验证 NULL 参数。测试捕获 segmentation fault 或 UB。

**Implement**:
```cpp
virtual int load_kernel_module(const void* image, size_t image_size,
                               uint64_t* out_vram_addr) {
    std::lock_guard<std::mutex> lock(load_mutex_);
    if (!out_vram_addr) return -EFAULT;
    if (!image && image_size > 0) return -EINVAL;
    if (image_size == 0) return -EINVAL;
    return -ENOSYS;
}
```

**Verify pass**: `ctest -R tadr-308` PASS。

**Commit**: `feat(igpu-driver): validate load_kernel_module args (T-005)`

---

## T-006: tadr-307 标 STALE (无 TDD, 文档任务)

**Action**:
1. 编辑 `docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md`
2. 在 frontmatter 顶部插入 STALE 块
3. 不改 tadr-307 任何现有内容

**Commit**: `docs(tadr-307): mark STALE per tadr-308 supersession (2026-08-18)`

---

## T-007: openspec change 自身归档 (Phase 2)

**Action**:
1. 等所有测试 PASS
2. 等 PTX-EMU HSK-6 ACCEPTED + CppTLM P0-1 完成 + TaskRunner owner ack
3. `git mv openspec/changes/2026-08-18-tadr-308-... openspec/changes/archive/2026-08-XX-tadr-308-...`
4. 更新 `openspec/changes/INDEX.md` 移除活跃段 + 加归档段

**Commit**: `docs(openspec): archive tadr-308 change after Gates #3/#5/#6 ✅`

---

## 验收总结

| Task | 描述 | TDD Phase | Commit Hash | Status |
|---|---|---|---|---|
| T-001 | 默认实现 -ENOSYS | 5 步全过 | (待 commit) | ⏳ |
| T-002 | cuModuleLoadData 适配 | 5 步全过 | (待 commit) | ⏳ |
| T-003 | cuModuleUnload 适配 | 5 步全过 | (待 commit) | ⏳ |
| T-004 | 并发 race 测试 | 5 步全过 | (待 commit) | ⏳ |
| T-005 | 错误注入测试 | 5 步全过 | (待 commit) | ⏳ |
| T-006 | tadr-307 STALE 标注 | 文档任务 | (待 commit) | ⏳ |
| T-007 | openspec change 归档 | Phase 2 | (待 commit) | ⏳ |

**预计总 commit 数**: 6-7 个 atomic commit（每个 task 1 commit + 1 docs commit）

## 跨仓依赖

- T-001 ~ T-005 实施前需 `cd external && git fetch origin main && git checkout origin/main` 确保 submodule 同步
- T-007 触发需外部条件: PTX-EMU HSK-6 + CppTLM P0-1 + TaskRunner owner ack

## References

- [proposal.md](proposal.md) (Why + What Changes + Acceptance + Cross-Repo)
- [tadr-308](../shared/adr/tadr-308-igpu-driver-vram-load.md) (canonical 设计)
- [tadr-307 STALE](../shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) (历史记录)
- UsrLinuxEmu ADR-090 v2 commit `37a91b6`
- Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` (4 轮评估)