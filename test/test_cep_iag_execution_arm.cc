// test/test_cep_iag_execution_arm.cc
// CEP **实例级装配闸(IAG 族)**的**执行臂**判据(装配路径侧)
//
// 功能:证明 IAG 族(`[IAG-01]`~`[IAG-08]`)不再「只测不拦」——
//       `ModuleFactory::instantiateAll()` 在**装配路径上**就会因 IAG 违规而失败。
// 作者: CppTLM Team / 日期: 2026-11-12
//
// ── 为什么需要本文件(per 台账 Entry #379)──────────────────────────────
//   `include/tlm/cep/instance_assembly_gate.hh` 的 IAG 族此前**只被测试侧调用**
//   ⇒ 装配路径**不拦**(与 `test_cep_bridge_reattach_guard.cc` 头注对 `[BSS-01]`
//   的记述同型)⇒ 本文件补上**执行臂**的行为级证据。
//
// ── 判据纪律 ───────────────────────────────────────────────────────────
//   ① **按签名红**:断言输出含 `[IAG-02]` / `[IAG-06]`,⛔ **不断言 rc≠0**;
//   ② **同一 config 形态**:正控 = 原拓扑 ⇒ 装配成功;破坏 = 只改 config ⇒ 装配失败;
//   ③ **还原 ⇒ 绿**:同一 TEST_CASE 内再跑一次正控。
//
// ⚠️ **「禁双实现」的落地证明**:本文件同时直接调用
//    `tlm::cep::run_instance_assembly_gate`(装配闸本体)与真 `instantiateAll`,
//    断言两者对同一 config **结论一致** ⇒ 杜绝「一边红一边不红」。
//    ⚠️ 两臂的**加载规则**亦共用 `tlm::cep::load_instance_assembly_inputs`
//    (include/tlm/cep/instance_assembly_loader.hh)⇒ ⛔ 不得各自实现。

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/instance_assembly_gate.hh"
#include "tlm/cep/instance_assembly_loader.hh"

#include <catch2/catch_all.hpp>

namespace {

/// 读一份拓扑 config(把 `cep.protocol_instances` 改成**绝对路径** —— 该字段是相对路径
/// ⇒ 装配期按 CWD 解析 ⇒ 须绝对化,per `test_cep_bridge_reattach_guard.cc` 的既有做法)
nlohmann::json load_topology(const char* file) {
    const auto path = std::filesystem::path(CPPTLM_SOURCE_DIR) / "configs" / file;
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json cfg = nlohmann::json::parse(in);
    cfg["cep"]["protocol_instances"] =
        (std::filesystem::path(CPPTLM_SOURCE_DIR) / "include" / "tlm" / "cep" /
         "protocol_instances.json")
            .string();
    return cfg;
}

/// 负向变换 ①:bridge 指向**未声明**的实例 ⇒ `[IAG-02]`
nlohmann::json with_undeclared_instance(nlohmann::json cfg) {
    cfg["cep"]["bridges"][0]["protocol_instance"] = "not_declared_xyz";
    return cfg;
}

/// 负向变换 ②:bridge 引用的模块**不在拓扑里** ⇒ `[IAG-06]`
/// (⚠️ 该形态若无 IAG 臂,会一路走到**模块解析**阶段才失败 ⇒ 因此本条对「臂是否在位」
///    **有鉴别力**:错误签名是 `[IAG-06]` 而非「模块不存在」类解析错误)
nlohmann::json with_module_not_in_topology(nlohmann::json cfg) {
    cfg["cep"]["bridges"][0]["modules"] = nlohmann::json::array({"__no_such_module__"});
    return cfg;
}

/// 捕获 stdout(C++ `printf`/`fputs` 通道)⇒ 供**按签名**断言。
/// ⚠️ 装配臂把 IAG 的 log 与 errors 都写 stdout(与 Step 0.25 的 gate log 同通道)。
std::string capture_stdout(const std::function<void()>& fn) {
    std::fflush(stdout);
    const int saved = ::dup(fileno(stdout));
    FILE* tmp = std::tmpfile();
    REQUIRE(tmp != nullptr);
    ::dup2(fileno(tmp), fileno(stdout));
    fn();
    std::fflush(stdout);
    ::dup2(saved, fileno(stdout));
    ::close(saved);
    std::rewind(tmp);
    std::string out;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), tmp)) > 0) {
        out.append(buf, n);
    }
    std::fclose(tmp);
    return out;
}

/// 跑一次真装配,返回 {成功?, 输出}
struct AssembleResult {
    bool ok = false;
    std::string out;
};

AssembleResult assemble(const nlohmann::json& cfg) {
    AssembleResult r;
    r.out = capture_stdout([&] {
        REGISTER_CHSTREAM;
        EventQueue eq;
        ModuleFactory factory(&eq);
        r.ok = factory.instantiateAll(cfg);
    });
    return r;
}

}  // namespace

// ── AC-1 · 正控绿 ⇒ 破坏红(按签名)⇒ 还原绿 ────────────────────────────
TEST_CASE("CEP IAG execution arm: 未声明实例 ⇒ 装配路径按 [IAG-02] 失败(并可还原)",
          "[cep][assembly][iag]") {
    const auto base = load_topology("cep_chi_rnf_pe_minimal.json");

    // ① 正控:原拓扑 ⇒ 装配成功
    {
        const auto r = assemble(base);
        REQUIRE(r.ok);
    }

    // ② 破坏:bridge 指向未声明实例 ⇒ 装配失败 **且** 输出含 `[IAG-02]`
    {
        const auto r = assemble(with_undeclared_instance(base));
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.out.find("[IAG-02]") != std::string::npos);
    }

    // ③ 还原:同一正控 config ⇒ 再次成功(证明失败源确为那处破坏)
    {
        const auto r = assemble(base);
        REQUIRE(r.ok);
    }
}

// ── AC-1 · 「臂在模块解析之前」的有鉴别力证据 ──────────────────────────
TEST_CASE("CEP IAG execution arm: 桥引模块不在拓扑 ⇒ 按 [IAG-06] 失败(而非模块解析错误)",
          "[cep][assembly][iag]") {
    const auto base = load_topology("cep_chi_rnf_pe_minimal.json");
    const auto r = assemble(with_module_not_in_topology(base));
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.out.find("[IAG-06]") != std::string::npos);
}

// ── AC-2 · 装配路径与装配闸结论一致(禁双实现的落地证明)────────────────
TEST_CASE("CEP IAG execution arm: 装配路径与装配闸对同一 config 结论一致",
          "[cep][assembly][iag]") {
    // ⚠️ 加载规则**共用** `load_instance_assembly_inputs`(⛔ 不得各自实现)⇒
    //    闸与装配路径喂给判据的输入必然相同。
    const auto cfg = load_topology("cep_chi_rnf_pe_minimal.json");
    const auto inputs = tlm::cep::load_instance_assembly_inputs(cfg);
    REQUIRE_FALSE(inputs.skip);
    REQUIRE_FALSE(inputs.source_error);

    // 负控:未声明实例 ⇒ 闸判红 **且** 装配失败
    {
        const auto broken = with_undeclared_instance(cfg);
        const auto binputs = tlm::cep::load_instance_assembly_inputs(broken);
        const auto rep =
            tlm::cep::run_instance_assembly_gate(broken, binputs.instances_doc, binputs.planes_doc);
        REQUIRE(rep.verdict == tlm::cep::InstanceAssemblyVerdict::FAIL);
        REQUIRE(rep.has_error("[IAG-02]"));
        REQUIRE_FALSE(assemble(broken).ok);
    }

    // 正控:原拓扑 ⇒ 闸判绿 **且** 装配成功
    {
        const auto rep =
            tlm::cep::run_instance_assembly_gate(cfg, inputs.instances_doc, inputs.planes_doc);
        REQUIRE_FALSE(rep.has_error("[IAG-02]"));
        REQUIRE(assemble(cfg).ok);
    }
}
