# cpptlm-ptxemu-public-device-api

PTX-EMU 端公共设备 API (IPtxEmuDevice) + 双层 facade 迁移：CppTLM 仅 include ptxemu/device_api.h, PTX-EMU 实现细节封装在 PTX-EMU 端 .a 静态库内. 取代 S1 的编译防火墙 + 直接 include PTX-EMU 实现头模式.
