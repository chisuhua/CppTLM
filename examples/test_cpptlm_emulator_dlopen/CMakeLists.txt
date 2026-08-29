# examples/test_cpptlm_emulator_dlopen/CMakeLists.txt
# T-W3-2 (T-ae-4): dlopen usage example for UsrLinuxEmu integration.

add_executable(test_cpptlm_emulator_dlopen test_dlopen.cc)
target_link_libraries(test_cpptlm_emulator_dlopen PRIVATE cpptlm_core cpptlm_emulator)
