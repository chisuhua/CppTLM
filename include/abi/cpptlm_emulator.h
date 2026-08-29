// include/abi/cpptlm_emulator.h
// CppTLM Emulator C ABI (pure declarations; impl moves to
// cpptlm-dgpu-abi-export change W3.2)
// Author: CppTLM Team
// Date: 2026-08-29
// Reference: ADR-088 §D5 (23 ABI contract), ADR-088 §D6.2 (semver),
//            ADR-SOC-07 D5 (Board/SOC split + ABI translation),
//            HSK-6 ack commit 369cf71 (CPPTLMBRIDGE_VERSION=2 freeze)
//
// ABI frozen per cpptlm-dgpu-board-soc-split T-bs-6. Implementation in a
// later change (cpptlm-dgpu-abi-export) provides SHARED library
// `libcpptlm_emulator.so` + cpptlm_core PIC + device-registry mutex.
//
// ABI ownership (per ADR-088 §D5 + UsrLinuxEmu ADR-088 cross-crate hand-off):
//   - 19 forward functions (16 device + 3 register)
//   -  4 callback typedefs (interrupt/error/reset/power)
//
// Header is C-clean: #ifdef __cplusplus extern "C" linkage-safe.
// Consumers: UsrLinuxEmu linux_compat (dlopen) + CppTLM test harnesses.
#ifndef CPPTLM_EMULATOR_H
#define CPPTLM_EMULATOR_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define CPPTLM_EMULATOR_VERSION_STRING "v1.0-dgpu-v0"

typedef struct cpptlm_emulator_s cpptlm_emulator_t;

typedef struct cpptlm_device_info_s {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  revision;
    uint32_t subsys_vendor_id;
    uint32_t subsys_device_id;
    char     profile_path[256];
} cpptlm_device_info_t;

typedef struct cpptlm_register_info_s {
    uint32_t offset;
    char     name[64];
    uint8_t  access;
    uint8_t  side_effect;
    uint32_t stream_id;
} cpptlm_register_info_t;

typedef void (*cpptlm_intr_deliver_cb_t)(void* user_ctx,
                                         uint32_t vector,
                                         uint32_t trans_id);

typedef void (*cpptlm_error_cb_t)(void* user_ctx,
                                  int error_code,
                                  const char* msg);

typedef void (*cpptlm_reset_complete_cb_t)(void* user_ctx, int reset_phase);

typedef void (*cpptlm_power_cb_t)(void* user_ctx, int power_state);

const char* cpptlm_emulator_get_version(void);

uint32_t cpptlm_emulator_get_device_count(void);

int cpptlm_emulator_get_device_info(uint32_t dev_id,
                                   cpptlm_device_info_t* out_info);

cpptlm_emulator_t* cpptlm_emulator_create(const char* profile_path);

cpptlm_emulator_t* cpptlm_emulator_create_by_id(uint32_t dev_id);

void cpptlm_emulator_destroy(cpptlm_emulator_t* emu);

int cpptlm_emulator_mmio_write(cpptlm_emulator_t* emu,
                              uint8_t bar,
                              uint64_t offset,
                              const void* buf,
                              size_t len);

int cpptlm_emulator_mmio_read(cpptlm_emulator_t* emu,
                             uint8_t bar,
                             uint64_t offset,
                             void* buf,
                             size_t len);

int cpptlm_emulator_pcie_config_write(cpptlm_emulator_t* emu,
                                     uint16_t offset,
                                     uint8_t width,
                                     uint32_t val);

int cpptlm_emulator_pcie_config_read(cpptlm_emulator_t* emu,
                                    uint16_t offset,
                                    uint8_t width,
                                    uint32_t* val);

int cpptlm_emulator_backdoor_read(cpptlm_emulator_t* emu,
                                  uint8_t bar,
                                  uint64_t offset,
                                  void* buf,
                                  size_t len);

int cpptlm_emulator_backdoor_write(cpptlm_emulator_t* emu,
                                   uint8_t bar,
                                   uint64_t offset,
                                   const void* buf,
                                   size_t len);

int cpptlm_emulator_msix_init(cpptlm_emulator_t* emu,
                              uint32_t table_size,
                              uint32_t mask);

int cpptlm_emulator_msix_update_pending(cpptlm_emulator_t* emu,
                                        uint32_t vector);

int cpptlm_emulator_msix_clear_pending(cpptlm_emulator_t* emu,
                                       uint32_t vector);

int cpptlm_emulator_lookup_register(cpptlm_emulator_t* emu,
                                    uint32_t offset,
                                    cpptlm_register_info_t* out_info);

int cpptlm_emulator_register_callbacks(cpptlm_emulator_t* emu,
                                      cpptlm_intr_deliver_cb_t intr_cb,
                                      cpptlm_error_cb_t err_cb,
                                      cpptlm_reset_complete_cb_t reset_cb,
                                      cpptlm_power_cb_t power_cb,
                                      void* user_ctx);

int cpptlm_emulator_register_backdoor_cb(cpptlm_emulator_t* emu,
                                        void* cb);

int cpptlm_emulator_register_dma_translate_cb(cpptlm_emulator_t* emu,
                                             void* cb);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // CPPTLM_EMULATOR_H
