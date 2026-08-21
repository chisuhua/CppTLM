// ptxemu_link_smoke.cc — ptxemu_core 非空 + stub 符号可链接 (T-s1-2 门禁 4)
//
// 目的: WU-2 阶段 cpptlm_core 尚未引用任何 ptxemu 符号, 静态库可能
//       静默通过构建。本可执行文件显式引用 stub 符号, 把"空库成功"
//       转化为链接错误, 是唯一的链接期栅栏。
//
// 注意: 此文件不是测试用例, 不参与 Catch2 套件。纯构建期 sanity check。

#include <cstddef>
#include <cstdio>

extern "C" size_t get_gpu_clock_from_context();

int main() {
    // 显式引用 stub 符号: 若 ptxemu_core 为空库/符号缺失, 此处链接失败
    if (get_gpu_clock_from_context() != 0) {
        std::fprintf(stderr, "smoke: unexpected clock\n");
        return 1;
    }
    std::printf("ptxemu_link_smoke: OK\n");
    return 0;
}