// examples/test_cpptlm_emulator_dlopen/test_dlopen_minimal_soc.cc
// T4: dlopen-based unit test 验证 minimal SoC ABI 闭环
//   - Phase 9 P0.5-cpptlm-minimal-dgpu-soc-v1-landing (T4)
//   - 配套 test_dlopen.cc (使用 dgpu_board_v1.json): 此 sibling 显式传 minimal SoC 路径,
//     验证 cpptlm_emulator_create("configs/dgpu_soc_minimal_v1.json") dlopen 路径
//     + BAR1 framebuffer 自动分配 + BAR1 mmio_write/read roundtrip
//   - 不依赖 test_dlopen.cc (它是 UsrLinuxEmu usage template, 不动)
//   - 选 Oracle 推荐方案 (b): 新建 sibling 文件而非修改既有模板
//
// AE-G5 sibling: stdout 输出 "v1.0-dgpu-v0" + BAR1 round-trip magic + 退出码 0.

#include <dlfcn.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "abi/cpptlm_emulator.h"

int main(void) {
    void* handle = dlopen("libcpptlm_emulator.so", RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr) {
        std::fprintf(stderr, "test_dlopen_minimal_soc: dlopen failed: %s\n", dlerror());
        return 1;
    }

    using FnCreate = void* (*)(const char*);
    using FnMmioWrite = int (*)(void*, uint8_t, uint64_t, const void*, size_t);
    using FnMmioRead = int (*)(void*, uint8_t, uint64_t, void*, size_t);
    using FnDestroy = void (*)(void*);

    auto create = reinterpret_cast<FnCreate>(dlsym(handle, "cpptlm_emulator_create"));
    auto mmio_write = reinterpret_cast<FnMmioWrite>(dlsym(handle, "cpptlm_emulator_mmio_write"));
    auto mmio_read = reinterpret_cast<FnMmioRead>(dlsym(handle, "cpptlm_emulator_mmio_read"));
    auto destroy = reinterpret_cast<FnDestroy>(dlsym(handle, "cpptlm_emulator_destroy"));

    if (!create || !mmio_write || !mmio_read || !destroy) {
        std::fprintf(stderr, "test_dlopen_minimal_soc: dlsym missing: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    std::printf("%s\n", CPPTLM_EMULATOR_VERSION_STRING);

    // 显式传 minimal SoC JSON 路径 (vs test_dlopen.cc 用 nullptr 默认 dgpu_board_v1.json).
    // ctest 从 build/examples 目录运行, 需回溯到 repo root 才找得到 configs/.
    const char* candidates[] = {
        "configs/dgpu_soc_minimal_v1.json",
        "../configs/dgpu_soc_minimal_v1.json",
        "../../configs/dgpu_soc_minimal_v1.json",
    };
    void* emu = nullptr;
    for (const char* path : candidates) {
        if (FILE* f = std::fopen(path, "r")) {
            std::fclose(f);
            emu = create(path);
            break;
        }
    }
    if (emu == nullptr) {
        std::fprintf(stderr, "test_dlopen_minimal_soc: create returned NULL "
                             "(configs/dgpu_soc_minimal_v1.json not found in cwd=%s)\n",
                     std::strerror(errno));
        dlclose(handle);
        return 1;
    }

    // BAR1 framebuffer round-trip: 验证 load_soc_config 自动派生 framebuffer_size_
    // (无需 attach_framebuffer_for_testing 兜底 — Phase 9 P0.5 landing)
    constexpr uint64_t kBar1Offset = 0x1000;
    constexpr uint32_t kMagic = 0xCAFEBABEU;
    int wr_rc = mmio_write(emu, 1, kBar1Offset, &kMagic, sizeof(kMagic));
    if (wr_rc != 0) {
        std::fprintf(stderr, "test_dlopen_minimal_soc: BAR1 mmio_write rc=%d\n", wr_rc);
        destroy(emu);
        dlclose(handle);
        return 1;
    }
    uint32_t readback = 0;
    int rd_rc = mmio_read(emu, 1, kBar1Offset, &readback, sizeof(readback));
    if (rd_rc != 0) {
        std::fprintf(stderr, "test_dlopen_minimal_soc: BAR1 mmio_read rc=%d\n", rd_rc);
        destroy(emu);
        dlclose(handle);
        return 1;
    }
    if (readback != kMagic) {
        std::fprintf(stderr, "test_dlopen_minimal_soc: BAR1 mismatch wrote=0x%08X read=0x%08X\n",
                     kMagic, readback);
        destroy(emu);
        dlclose(handle);
        return 1;
    }

    std::printf("minimal_soc BAR1 round-trip OK (0x%08X)\n", readback);

    destroy(emu);
    dlclose(handle);
    return 0;
}