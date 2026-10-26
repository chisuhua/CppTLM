// examples/cep_collective_plane_contract.cc
// CEP collective plane 契约判据的**离线挂点** CLI
//
// 功能:读本仓声明,跑 5+1 条契约判据,打印逐条 verdict(可 grep)。
// 作者: CppTLM Team / 日期: 2026-10-26
//
// ── 退出码(红绿分离)──────────────────────────────────────────────────
//   0 = PASS,或 SKIP(无可检查对象)
//   1 = 判据红(契约被违反,含「声明源配置错误」)
//   2 = 用法错误
//
// 用法:
//   ./build/bin/examples/cep_collective_plane_contract               # 用仓根(从仓库根运行)
//   ./build/bin/examples/cep_collective_plane_contract <repo_root>    # 显式指定仓库根
//   ./build/bin/examples/cep_collective_plane_contract --help
//
// ⚠️ 与 `module_factory.cc` 的 CEP 装配闸**不共用**挂点:collective plane 的判据是
//    **契约层**(跨平面规则),不接装配路径(本仓无 collective 流量通路,per design D-γ
//    「注册推迟到 P3」)。判据**实现**与 test 侧完全同一份(`collective_plane_contract.hh`)
//    ⇒ 「守卫」与「被守卫的对象」不会漂移。

#include <cstdio>
#include <string>

#include "tlm/cep/collective_plane_contract.hh"

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            std::printf("用法: %s [<repo_root>]\n\n"
                        "  (无参数)      以当前工作目录为仓库根\n"
                        "  <repo_root>   显式指定仓库根(声明源与权威源都按它解析)\n\n"
                        "退出码: 0 = PASS 或 SKIP · 1 = 判据红/配置错误 · 2 = 用法错误\n",
                        argv[0]);
            return 0;
        }
    }
    if (argc > 2) {
        std::fprintf(stderr, "[usage] 最多接受 1 个位置参数(收到 %d 个);--help 查看用法\n",
                     argc - 1);
        return 2;
    }

    tlm::cep::ContractPaths paths;
    if (argc > 1)
        paths.root = argv[1];

    const tlm::cep::ContractReport report = tlm::cep::run_collective_plane_contract(paths);
    std::fputs(report.to_log().c_str(), stdout);

    const bool failed = report.verdict == tlm::cep::ContractVerdict::FAIL;
    std::printf("[%s] cep collective plane contract: %zu rule(s), %zu failed\n",
                failed ? "FAIL"
                       : (report.verdict == tlm::cep::ContractVerdict::SKIP ? "SKIP" : "PASS"),
                report.lines.size(), report.count(tlm::cep::ContractVerdict::FAIL));
    return failed ? 1 : 0;
}
