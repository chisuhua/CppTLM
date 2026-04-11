// src/cpu_main.cpp
// CPU 模式主程序（历史遗留 demo，等待 v2.1 架构升级后重写）
// 功能描述：预留入口，当前为空实现以避免编译阻塞
// 作者 CppTLM Team
// 日期 2026-04-12
#include <iostream>

extern "C" int sc_main(int argc, char* argv[]) {
    return 0;
}

int main() {
    std::cout << "[INFO] cpu_main is a placeholder. Use configs/ with cpptlm_sim.\n";
    return 0;
}
