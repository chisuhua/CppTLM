// 提供一个空的sc_main实现，以解决链接时的符号问题
// 添加 main 以避免 "undefined reference to `main'"
#include <iostream>
extern "C" int sc_main(int argc, char* argv[]) {
    return 0;
}
int main() {
    std::cout << "[INFO] sc_main is a placeholder. Use cpptlm_sim with configs/.\n";
    return 0;
}