// examples/cep_adapter_contracts.cc
// CEP adapter 契约判据的**离线 CLI**(per delta capability `cep-adapter-schema-contracts`)
//
// 功能:跑 `include/tlm/cep/adapter_schema_contracts.hh` 的 5 条判据并打印逐条 verdict。
//       判据实现与 Catch2 测试**共用同一份**(`adapter_schema_contracts.hh` +
//       `adapter_contracts_loader.hh`)⇒ 两端结论必然一致。
// 作者: CppTLM Team / 日期: 2026-10-27
//
// 用法:
//   cep_adapter_contracts [仓根目录]     # 默认 "."(ctest 以仓根为工作目录)
//   cep_adapter_contracts --help
//
// 退出码:
//   0 = 全部判据 PASS(或 SKIP —— 无可检查对象)
//   1 = 任一判据 FAIL(契约红)
//   2 = 用法错误 / 声明源不可读(配置错误)

#include <cstdio>
#include <string>

#include "tlm/cep/adapter_contracts_loader.hh"

int main(int argc, char** argv) {
    std::string repo_root = ".";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::printf(
                "用法: cep_adapter_contracts [仓根目录]\n"
                "  跑 adapter 契约的 5 条判据(AC-01 ~ AC-05)并打印逐条 verdict。\n"
                "  退出码: 0 = PASS/SKIP · 1 = 判据 FAIL · 2 = 用法错误/声明源不可读\n");
            return 0;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "[usage] 未知参数: %s(--help 查看用法)\n", arg.c_str());
            return 2;
        }
        repo_root = arg;
    }

    const tlm::cep::LoadedAdapterContracts loaded = tlm::cep::load_adapter_contract_inputs(repo_root);
    if (!loaded.ok) {
        std::fprintf(stderr, "[FAIL] adapter 契约判据:声明源装载失败 —— %s\n",
                     loaded.error.c_str());
        return 2;
    }

    const tlm::cep::AdapterContractReport report = tlm::cep::AdapterSchemaContracts::check_all(loaded.inputs);
    std::printf("%s", report.render().c_str());
    if (report.failed()) {
        std::printf("[FAIL] adapter 契约判据:至少一条判据为红\n");
        return 1;
    }
    std::printf("[PASS] adapter 契约判据:全部条目结论如上\n");
    return 0;
}
