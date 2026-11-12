// examples/cep_instance_assembly_gate.cc
// CEP 实例级装配闸(IAG 族)的**离线挂点** CLI
//
// 功能:`cmake --build build --target validate_topology` 追加入口 —— 对一份**拓扑 config**
//       跑 `tlm::cep::run_instance_assembly_gate`(声明层 + 拓扑层 + 平面层)
//       ⇒ 打印逐条 `[IAG-0x]` 归因 + verdict。
// 作者: CppTLM Team / 日期: 2026-11-12
//
// ── 为什么要有这个 CLI(per 台账 Entry #379 的 D-A2)────────────────────
//   · `src/core/module_factory.cc::instantiateAll()` —— 装配路径上的**硬闸**
//     (不通过 = 装配失败,且此时**模块尚未创建** ⇒ 零副作用)
//   · 本 CLI —— `validate_topology` target 上的**离线闸**(人工 / CI 一键跑)
//   两处**必须**调用**同一组**函数与**同一份**加载规则:
//     判据实现 = `tlm::cep::run_instance_assembly_gate`(include/tlm/cep/instance_assembly_gate.hh)
//     加载规则 =
//     `tlm::cep::load_instance_assembly_inputs`(include/tlm/cep/instance_assembly_loader.hh)
//   ⇒ **禁双实现**(per `src/core/module_factory.cc:173` 的同族纪律)。
//
// ── 退出码(红绿分离)────────────────────────────────────────────────────
//   0 = PASS 或 SKIP(外部条件)
//   1 = 判据红(IAG 族),或配置错(config 不可读 / JSON 畸形 / 显式声明源写错)
//   2 = 用法错误(参数个数不对)
//
// 用法:
//   ./build/bin/examples/cep_instance_assembly_gate                      # 用缺省 config
//   ./build/bin/examples/cep_instance_assembly_gate <config.json>        # 显式 config
//   ./build/bin/examples/cep_instance_assembly_gate --help
//
// ⚠️ **相对路径按当前工作目录解析**(与 `cep_channel_dependency_gate` 同一约定)
//    ⇒ 从仓库根以外运行时须改用绝对路径。

#include <cstdio>
#include <string>

#include <nlohmann/json.hpp>

#include "tlm/cep/instance_assembly_gate.hh"
#include "tlm/cep/instance_assembly_loader.hh"

namespace {

    /// 缺省 config(相对仓库根)
    constexpr const char* kDefaultConfigPath = "configs/cep_chi_rnf_pe_minimal.json";

    void print_usage(const char* argv0) {
        std::printf("用法: %s [<config.json>]\n\n"
                    "  (无参数)       用缺省 config %s\n"
                    "  <config.json>  显式 config —— 不可读 / JSON 畸形 ⇒ 退出码 1\n\n"
                    "退出码: 0 = PASS 或 SKIP · 1 = 判据红 / 配置错 · 2 = 用法错误\n",
                    argv0, kDefaultConfigPath);
    }

} // namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }
    if (argc > 2) {
        std::fprintf(stderr, "[usage] 最多接受 1 个位置参数(收到 %d 个);--help 查看用法\n",
                     argc - 1);
        return 2;
    }

    const std::string config_path = (argc == 2) ? argv[1] : kDefaultConfigPath;

    // ── 读 config ─────────────────────────────────────────────────────────
    nlohmann::json config;
    {
        std::string err;
        if (!tlm::cep::detail::read_json_file(config_path, config, err)) {
            std::printf("[FAIL] cep instance assembly gate: config 读取失败 —— %s\n", err.c_str());
            return 1;
        }
    }

    // ── 加载三层输入(**与装配路径臂共用同一加载器**)──────────────────────
    const tlm::cep::InstanceAssemblyInputs inputs = tlm::cep::load_instance_assembly_inputs(config);
    if (inputs.source_error) {
        std::printf("[FAIL] cep instance assembly gate: 声明源配置错误 —— %s\n",
                    inputs.source_error_detail.c_str());
        return 1;
    }
    if (inputs.skip) {
        std::printf("[SKIP] cep instance assembly gate: %s\n", inputs.skip_reason.c_str());
        return 0;
    }
    if (inputs.planes_unavailable) {
        std::printf("[NOTE] cep instance assembly gate: %s\n",
                    inputs.planes_unavailable_detail.c_str());
    }

    // ── 跑判据(与装配路径臂**同一份**实现)───────────────────────────────
    const tlm::cep::InstanceAssemblyReport rep =
        tlm::cep::run_instance_assembly_gate(config, inputs.instances_doc, inputs.planes_doc);

    std::fputs(rep.log.c_str(), stdout);
    for (const auto& e : rep.errors) {
        std::printf("[FAIL] %s\n", e.c_str());
    }
    if (rep.verdict == tlm::cep::InstanceAssemblyVerdict::SKIP) {
        std::printf("[SKIP] cep instance assembly gate: %s\n", rep.skip_reason.c_str());
        return 0;
    }
    const bool failed = rep.verdict == tlm::cep::InstanceAssemblyVerdict::FAIL;
    std::printf("[%s] cep instance assembly gate: %zu instance(s) mounted, %zu error(s)\n",
                failed ? "FAIL" : "PASS", rep.mounted.size(), rep.errors.size());
    return failed ? 1 : 0;
}
