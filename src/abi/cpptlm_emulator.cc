// src/abi/cpptlm_emulator.cc
// T-ae-1 stub: 19 forward ABI 函数体占位 (返回 -ENOSYS).
// T-ae-2 将替换为真实实现 (设备注册表 mutex + DGpuBoard shell 转发).
// Author: CppTLM Team
// Date: 2026-08-29
// Reference: ADR-088 §D5, ADR-SOC-07 D5, ADR-SOC-07 Status Update Q2/Q6

#include "abi/cpptlm_emulator.h"

#include <cerrno>

namespace {

constexpr int kEnosys = 38;

} // namespace

extern "C" {

CPPTLM_EMULATOR_EXPORT
const char* cpptlm_emulator_get_version(void) {
    return "v1.0-dgpu-v0";
}

CPPTLM_EMULATOR_EXPORT
uint32_t cpptlm_emulator_get_device_count(void) {
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_get_device_info(uint32_t dev_id,
                                   cpptlm_device_info_t* out_info) {
    (void)dev_id;
    (void)out_info;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
cpptlm_emulator_t* cpptlm_emulator_create(const char* profile_path) {
    (void)profile_path;
    return nullptr;
}

CPPTLM_EMULATOR_EXPORT
cpptlm_emulator_t* cpptlm_emulator_create_by_id(uint32_t dev_id) {
    (void)dev_id;
    return nullptr;
}

CPPTLM_EMULATOR_EXPORT
void cpptlm_emulator_destroy(cpptlm_emulator_t* emu) {
    (void)emu;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_mmio_write(cpptlm_emulator_t* emu,
                              uint8_t bar,
                              uint64_t offset,
                              const void* buf,
                              size_t len) {
    (void)emu; (void)bar; (void)offset; (void)buf; (void)len;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_mmio_read(cpptlm_emulator_t* emu,
                             uint8_t bar,
                             uint64_t offset,
                             void* buf,
                             size_t len) {
    (void)emu; (void)bar; (void)offset; (void)buf; (void)len;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_pcie_config_write(cpptlm_emulator_t* emu,
                                     uint16_t offset,
                                     uint8_t width,
                                     uint32_t val) {
    (void)emu; (void)offset; (void)width; (void)val;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_pcie_config_read(cpptlm_emulator_t* emu,
                                    uint16_t offset,
                                    uint8_t width,
                                    uint32_t* val) {
    (void)emu; (void)offset; (void)width; (void)val;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_backdoor_read(cpptlm_emulator_t* emu,
                                  uint8_t bar,
                                  uint64_t offset,
                                  void* buf,
                                  size_t len) {
    (void)emu; (void)bar; (void)offset; (void)buf; (void)len;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_backdoor_write(cpptlm_emulator_t* emu,
                                   uint8_t bar,
                                   uint64_t offset,
                                   const void* buf,
                                   size_t len) {
    (void)emu; (void)bar; (void)offset; (void)buf; (void)len;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_msix_init(cpptlm_emulator_t* emu,
                              uint32_t table_size,
                              uint32_t mask) {
    (void)emu; (void)table_size; (void)mask;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_msix_update_pending(cpptlm_emulator_t* emu,
                                        uint32_t vector) {
    (void)emu; (void)vector;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_msix_clear_pending(cpptlm_emulator_t* emu,
                                       uint32_t vector) {
    (void)emu; (void)vector;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_lookup_register(cpptlm_emulator_t* emu,
                                    uint32_t offset,
                                    cpptlm_register_info_t* out_info) {
    (void)emu; (void)offset; (void)out_info;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_register_callbacks(cpptlm_emulator_t* emu,
                                      cpptlm_intr_deliver_cb_t intr_cb,
                                      cpptlm_error_cb_t err_cb,
                                      cpptlm_reset_complete_cb_t reset_cb,
                                      cpptlm_power_cb_t power_cb,
                                      void* user_ctx) {
    (void)emu; (void)intr_cb; (void)err_cb; (void)reset_cb;
    (void)power_cb; (void)user_ctx;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_register_backdoor_cb(cpptlm_emulator_t* emu,
                                        void* cb) {
    (void)emu; (void)cb;
    return -kEnosys;
}

CPPTLM_EMULATOR_EXPORT
int cpptlm_emulator_register_dma_translate_cb(cpptlm_emulator_t* emu,
                                             void* cb) {
    (void)emu; (void)cb;
    return -kEnosys;
}

} // extern "C"
