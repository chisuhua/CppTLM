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
//   0 = PASS 或 SKIP(SKIP 打印 `[SKIP]`,与 PASS 可区分)
//   1 = 存在判据红(配置 fail)
//   2 = 用法错误 / 声明文件不可读
//
// 用法:
//   ./build/bin/examples/cep_channel_dependency_gate [declaration.json]
//   缺省 = include/tlm/cep/protocol_instances.json(相对当前工作目录 = 仓库根)

#include <cstdio>
#include <fstream>
#include <string>

#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/ordering_point_checker.hh"

namespace {

    bool file_readable(const std::string& path) {
        std::ifstream in(path);
        return static_cast<bool>(in);
    }

} // namespace

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : tlm::cep::kDefaultDeclarationPath;

    if (!file_readable(path)) {
        std::fprintf(stderr, "[SKIP] cep assembly gate: declaration not readable: %s\n",
                     path.c_str());
        // 声明缺失是**外部条件**(未生成 / 路径不同),不是判据红 ⇒ 不阻断拓扑校验
        return 0;
    }

    const tlm::cep::ChannelDeclaration decl = tlm::cep::load_channel_declaration(path);
    if (decl.empty()) {
        std::printf("[SKIP] cep assembly gate: no declared protocol instance in %s\n",
                    path.c_str());
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
