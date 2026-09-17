// test/test_cpptlm_emulator_handle_helpers.hh
// RAII 守卫: 封装 cpptlm_emulator_create / cpptlm_emulator_destroy,
// 保证 REQUIRE 断言失败 (Catch2 抛异常) 时 handle 仍被释放, 不产生 ASan 泄漏.
// Author: CppTLM Team
// Date: 2026-09-08 (修订版 2027-09-17: 改用 create 替代 create_by_id,
//                   per ADR-SOC-20 §2.1 cpptlm-abi-secondary-slimming)
#ifndef CPPTLM_TEST_CPPTLM_EMULATOR_HANDLE_HELPERS_HH
#define CPPTLM_TEST_CPPTLM_EMULATOR_HANDLE_HELPERS_HH

#include "abi/cpptlm_emulator.h"

// create + destroy 的 RAII 包装: 析构自动 destroy, 异常安全, 不可拷贝.
// 修订版: 改用 cpptlm_emulator_create(nullptr) 替代 cpptlm_emulator_create_by_id(dev_id).
// dev_id 参数保留仅为接口兼容旧测试 (2027-09-17 修订前), 实际不使用.
struct EmulatorHandleGuard {
    cpptlm_emulator_t* emu;

    explicit EmulatorHandleGuard(uint32_t /*dev_id_unused*/ = 0)
        : emu(cpptlm_emulator_create(nullptr)) {}

    ~EmulatorHandleGuard() {
        if (emu) {
            cpptlm_emulator_destroy(emu);
        }
    }

    EmulatorHandleGuard(const EmulatorHandleGuard&) = delete;
    EmulatorHandleGuard& operator=(const EmulatorHandleGuard&) = delete;

    // create 失败时返回 false (create 可能因配置/SOC 初始化失败返回 nullptr)
    bool valid() const { return emu != nullptr; }
};

#endif // CPPTLM_TEST_CPPTLM_EMULATOR_HANDLE_HELPERS_HH
