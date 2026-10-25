// examples/cep_channel_dependency_gate.cc
// CEP 装配期判据的**离线挂点** CLI(与 module_factory 挂点共享同一组判据函数)
//
// 功能:`cmake --build build --target validate_topology` 追加入口 —— 读本仓声明,
//       打印逐 (network, vc) 的通道依赖 verdict + 逐判据的 ordering 结论。
// 作者: CppTLM Team / 日期: 2026-10-25
//
// ── 为什么要在两个地方各挂一次(per ADR-CEP-09 抓取点 1)──────────────
//   · `module_factory.cc::instantiateAll()` —— 装配路径上的**硬闸**(不通过 = 配置 fail)
//   · 本 CLI —— 拓扑校验 target 上的**离线闸**(人工/CI 一键跑)
//   两处**必须**调用同一组函数(`run_channel_gate` / `run_ordering_gate` 的底层判据),
//   否则「守卫」与「被守卫的命令」会漂移。
//
// ── 退出码(红绿分离)──────────────────────────────────────────────────
//   0 = PASS,或 SKIP(**缺省**声明源不可读/为空 —— 外部条件)
//   1 = 判据红:存在环,或**声明源配置错误**(显式给的路径不可读 / JSON 畸形 / 类型非法)
//   2 = 用法错误(多余参数)
//
// 用法:
//   ./build/bin/examples/cep_channel_dependency_gate               # 用缺省声明源
//   ./build/bin/examples/cep_channel_dependency_gate <decl.json>   # 显式声明源(不可读 ⇒ rc=1)
//   ./build/bin/examples/cep_channel_dependency_gate --help

#include <cstdio>
#include <filesystem>
#include <string>

#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/ordering_point_checker.hh"

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            std::printf(
                "用法: %s [<declaration.json>]\n\n"
                "  (无参数)        用缺省声明源 %s\n"
                "  <declaration.json>  显式声明源 —— 不可读/JSON 畸形 ⇒ 退出码 1\n\n"
                "退出码: 0 = PASS 或 SKIP(缺省源不可用) · 1 = 判据红/配置错误 · 2 = 用法错误\n",
                argv[0], tlm::cep::kDefaultDeclarationPath);
            return 0;
        }
    }
    if (argc > 2) {
        std::fprintf(stderr, "[usage] 最多接受 1 个位置参数(收到 %d 个);--help 查看用法\n",
                     argc - 1);
        return 2;
    }

    const bool explicit_path = argc > 1;
    const std::string path = explicit_path ? argv[1] : tlm::cep::kDefaultDeclarationPath;

    // 显式路径不可读/畸形 ⇒ source_error(⇒ 判据 FAIL);缺省路径不可读 ⇒ 空声明(⇒ SKIP)
    const tlm::cep::ChannelDeclaration decl =
        tlm::cep::load_channel_declaration(path, explicit_path);

    if (decl.source_error) {
        std::printf("[FAIL] cep assembly gate: 声明源配置错误 —— %s\n",
                    decl.source_error_detail.c_str());
        return 1;
    }
    if (decl.empty()) {
        std::printf("[SKIP] cep assembly gate: no declared protocol instance in %s\n",
                    path.c_str());
        if (!explicit_path) {
            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            std::printf("  (缺省声明源按**当前工作目录**解析;cwd=%s —— 从仓库根以外运行时"
                        "需改用显式路径)\n",
                        ec ? "<unavailable>" : cwd.string().c_str());
        }
        return 0;
    }

    const tlm::cep::GateReport channel = tlm::cep::evaluate_channel_dependency_gate(decl);
    const tlm::cep::OrderingReport ordering = tlm::cep::evaluate_ordering_points(decl);

    std::fputs(channel.to_log().c_str(), stdout);
    std::fputs(ordering.to_log().c_str(), stdout);

    const bool failed = channel.verdict == tlm::cep::GateVerdict::FAIL ||
                        ordering.verdict == tlm::cep::GateVerdict::FAIL;
    std::printf("[%s] cep assembly gate: %zu instance(s) from %s\n", failed ? "FAIL" : "PASS",
                decl.instances.size(), path.c_str());
    return failed ? 1 : 0;
}
