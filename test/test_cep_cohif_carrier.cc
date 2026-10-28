// test/test_cep_cohif_carrier.cc
// 队列 #7 `cpptlm-cep-cohif-carrier` Phase 6 —— CohIf 载体判据收口
//   (CC-01 / CC-02 / CC-04)
//
// per openspec/changes/cpptlm-cep-cohif-carrier/tasks.md task 6.1
// per openspec/changes/cpptlm-cep-cohif-carrier/specs/cep-cohif-carrier/spec.md
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ⚠️ **判据纪律(本文件的组织原则)**─────────────────────────────
// ① **每条子条件都有正向断言**;每条判据都有**反向 fixture**。
// ② **反向 fixture 必须「因正确原因」红** —— 对编译期锁:不仅断言「编译失败」,
//    还要断言**失败原因命中预期的那条 `static_assert`**(否则「因无关原因红」)。
// ③ **编译期锁不得只以「编译红」充当 ctest 判据** —— 本文件在 ctest **内**调用
//    编译器并断言「编译**确实**失败」⇒ 若有人删掉锁,本用例会**假绿**(⇒ 必须
//    同时保留**正控**:干净头必须编译成功)。
// ④ **纯负向断言必须有正控** —— 扫描器自检(`cohif_scan_self_check()`) +
//    干净头的扫描/编译正控。
#include "catch_amalgamated.hpp"

#include "cep_cohif_assembly_helper.hh"
#include "cep_cohif_contract.hh"

#include "bundles/cep_coherence_bundles_tlm.hh"

#include "framework/chstream_adapter_factory.hh"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/wait.h>

using json = nlohmann::json;
using cpptlm::cep::testing::ReachOutcome;

namespace {

/// 检测某枚举类型**是否有** `Barrier` 枚举项
///
/// ⚠️ 必须写成**依赖**表达式的 concept —— 非依赖的 `requires { T::Barrier; }`
///    在非模板上下文里是**硬错误**(而非 SFINAE 失败)⇒ 依赖化后,`T::Barrier`
///    的查找失败变成 substitution failure ⇒ concept 求值为 `false`。
template <typename T>
concept HasBarrierEnumerator = requires { T::Barrier; };

// ── CC-02 ①:`sizeof` 锁(布局契约)───────────────────────────────
// ⚠️ 与 `scripts/test/cep_cohif_compile_probe.cc` 的同款断言**镜像**
//    (spec 要求「test TU 内」有该断言)⇒ 两处漂移会被本文件与反向脚本同时抓到。
static_assert(sizeof(bundles::CohIfBundle) == 96,
              "CC-02 ①: CohIfBundle 布局尺寸漂移(期望 96 字节)");

// ── CC-02 ②:`is_trivially_copyable` 锁 ───────────────────────────
static_assert(std::is_trivially_copyable_v<bundles::CohIfBundle>,
              "CC-02 ②: CohIfBundle 必须 trivially copyable");

// ── CC-01 ③:`flow_id` 是**强类型枚举**(域外流**类型不可表达**)────
static_assert(std::is_enum_v<bundles::CohIfFlowId>,
              "CC-01 ③: flow_id 必须是强类型枚举(裸整数会让域外流仍可表达)");
static_assert(std::is_same_v<std::underlying_type_t<bundles::CohIfFlowId>, std::uint8_t>,
              "CC-01 ③: flow_id 的底层类型必须是 uint8_t");

std::filesystem::path repo_root() { return cpptlm::cep::repo_root(); }

std::filesystem::path bundle_header() {
    return repo_root() / "include" / "bundles" / "cep_coherence_bundles_tlm.hh";
}

std::string read_all(const std::filesystem::path& p) {
    std::ifstream in(p);
    if (!in.good())
        throw std::runtime_error("无法打开文件: " + p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// 临时目录(RAII)—— 编译期反向 fixture 需要把**毒化副本**写成影子头文件。
class TempDir {
public:
    TempDir() {
        static int counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cep_cohif_carrier_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path_ / "bundles");
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

    /// 把内容写成影子头文件 `<dir>/bundles/cep_coherence_bundles_tlm.hh`
    void write_shadow_header(const std::string& content) const {
        std::ofstream out(path_ / "bundles" / "cep_coherence_bundles_tlm.hh");
        if (!out.good())
            throw std::runtime_error("无法写影子头文件");
        out << content;
    }

private:
    std::filesystem::path path_;
};

struct CompileResult {
    int rc;             ///< 进程退出码(0 = 成功)
    std::string output; ///< 编译器输出(诊断失败原因)
};

/// 执行一条 shell 命令,返回退出码 + 输出
CompileResult run_shell(const std::string& cmd) {
    const std::string full = cmd + " 2>&1";
    FILE* p = popen(full.c_str(), "r");
    if (!p)
        throw std::runtime_error("popen 失败: " + cmd);
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), p))
        out += buf;
    const int status = pclose(p);
    return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, out};
}

std::string quote(const std::filesystem::path& p) { return "'" + p.string() + "'"; }

/// 编译 CohIf 载体的**编译期锁探针**,`shadow_dir` 为空表示用**真实头文件**
///
/// ⚠️ `-I <shadow>` 排在 `-I <repo>/include` **之前** ⇒ 影子头文件**遮蔽**真实头
///    ⇒ 探针编译的就是**毒化副本**。
CompileResult compile_width_probe(const std::filesystem::path* shadow_dir) {
    std::string cmd = "g++ -std=c++17 -fsyntax-only ";
    if (shadow_dir)
        cmd += "-I " + quote(*shadow_dir) + " ";
    cmd += "-I " + quote(repo_root() / "include") + " " +
           quote(repo_root() / "scripts" / "test" / "cep_cohif_compile_probe.cc");
    return run_shell(cmd);
}

/// 编译 `REGISTER_CHSTREAM_BIDIRECTIONAL` 的**宏探针**,`port_count` 为 N
///
/// ⚠️ 必须用 **C++23** —— 探针 include `chstream_register.hh` ⇒ 拉进
///    `include/utils/module_group.hh`(它用了 `std::println`,C++23 特性)。
CompileResult compile_macro_probe(int port_count) {
    const auto root = repo_root();
    std::string cmd = "g++ -std=c++23 -fsyntax-only -DPROBE_PORT_COUNT=" +
                      std::to_string(port_count) + " -I " + quote(root / "include") + " -I " +
                      quote(root / "include" / "bundles") + " -I " + quote(root / "include" / "core") +
                      " -I " + quote(root / "test") + " -I " + quote(root / "external" / "json") + " " +
                      quote(root / "scripts" / "test" / "cep_chstream_bidirectional_macro_probe.cc");
    return run_shell(cmd);
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════
//  CC-01 —— `CohIfBundle` 只含 5 条 RN-side 流且不含网络层字段
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("CC-01 ① flow_id 域恰好 5 值(request/completion/snoop/snoop_resp/writeback)",
          "[cep][cohif-carrier]") {
    const auto res = cpptlm::cep::scan_cohif_bundle(read_all(bundle_header()));
    INFO(res.render());
    REQUIRE(res.ok());
    REQUIRE(res.flow_members.size() == 5u);
    REQUIRE(res.flow_members == cpptlm::cep::cohif_expected_flow_members());
}

TEST_CASE("CC-01 ② 字段集不含任何网络层字段(src_node/dst_node/flit_category/vc_id)",
          "[cep][cohif-carrier]") {
    const auto res = cpptlm::cep::scan_cohif_bundle(read_all(bundle_header()));
    INFO(res.render());
    for (const auto& v : res.violations) {
        INFO("契约违反: " << v);
        REQUIRE(v.find("网络层字段") == std::string::npos);
    }
}

TEST_CASE("CC-01 ③ 域外 5 条流不可能出现在 flow_id 域内(类型保证,非运行期纪律)",
          "[cep][cohif-carrier]") {
    const auto res = cpptlm::cep::scan_cohif_bundle(read_all(bundle_header()));
    INFO(res.render());
    for (const auto& v : res.violations) {
        INFO("契约违反: " << v);
        REQUIRE(v.find("域外流名") == std::string::npos);
    }
    // 域的**上界**是 4(恰好 5 值 ⇒ 0..4)⇒ 域外流**类型不可表达**
    static_assert(static_cast<std::uint8_t>(bundles::CohIfFlowId::Writeback) == 4,
                  "CC-01 ①: flow_id 域必须恰好 5 值(上界 4)");
    // 域外流的**名字**在类型上**不存在**(编译期即可判定)
    //
    // ⚠️ 必须用**依赖**表达式的 concept(`HasBarrierEnumerator`,定义见本文件上方)
    //    —— 非依赖的 `requires { T::Barrier; }` 在非模板上下文里是**硬错误**
    //    (而非 SFINAE 失败)⇒ 改用 concept 让 `T::Barrier` 的查找失败变成
    //    substitution failure。
    static_assert(!HasBarrierEnumerator<bundles::CohIfFlowId>,
                  "CC-01 ③: 域外流不得是 CohIfFlowId 的枚举项");
}

TEST_CASE("CC-01 反向 fixture ① 新增第 6 个 flow_id ⇒ 静态扫描必红", "[cep][cohif-carrier]") {
    const std::string src = read_all(bundle_header());
    const std::string poisoned =
        cpptlm::cep::poison_cohif_bundle(src, cpptlm::cep::CohIfPoison::FlowCount6);
    REQUIRE(poisoned != src);  // 毒化必须真的改了内容

    const auto res = cpptlm::cep::scan_cohif_bundle(poisoned);
    INFO(res.render());
    REQUIRE_FALSE(res.ok());  // 扫描器**必须**报红
}

TEST_CASE("CC-01 反向 fixture ② 混入网络层字段 ⇒ 静态扫描必红", "[cep][cohif-carrier]") {
    const std::string src = read_all(bundle_header());
    const std::string poisoned =
        cpptlm::cep::poison_cohif_bundle(src, cpptlm::cep::CohIfPoison::NetworkField);
    REQUIRE(poisoned != src);

    const auto res = cpptlm::cep::scan_cohif_bundle(poisoned);
    INFO(res.render());
    REQUIRE_FALSE(res.ok());
}

TEST_CASE("CC-01 正控:扫描器自身有判别力(自检必须全绿)", "[cep][cohif-carrier]") {
    const auto bad = cpptlm::cep::cohif_scan_self_check();
    for (const auto& b : bad)
        INFO("扫描器自检失败: " << b);
    REQUIRE(bad.empty());
}

// ══════════════════════════════════════════════════════════════════════
//  CC-02 —— 编译期锁与 trivially copyable 生效
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("CC-02 ① sizeof 布局锁在 test TU 内生效", "[cep][cohif-carrier]") {
    // TU 级 `static_assert` 在编译期即已生效(编译不过 ⇒ 用例不存在)
    REQUIRE(sizeof(bundles::CohIfBundle) == 96u);
}

TEST_CASE("CC-02 ② is_trivially_copyable 锁生效", "[cep][cohif-carrier]") {
    REQUIRE(std::is_trivially_copyable_v<bundles::CohIfBundle>);
}

TEST_CASE("CC-02 ③ BUNDLE_FIELD_WIDTH 覆盖 CohIfBundle 的全部 12 字段", "[cep][cohif-carrier]") {
    const auto locks = cpptlm::cep::scan_cohif_width_locks(read_all(bundle_header()));
    const auto& expected = cpptlm::cep::cohif_expected_width_locks();
    REQUIRE(locks.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("宽度锁不匹配: 实际 " << locks[i].field << "/" << locks[i].width << " 期望 "
                                   << expected[i].field << "/" << expected[i].width);
        REQUIRE(locks[i].field == expected[i].field);
        REQUIRE(locks[i].width == expected[i].width);
    }
}

TEST_CASE("CC-02 ③ 反向 fixture:某字段漏加宽度锁 ⇒ 扫描必红", "[cep][cohif-carrier]") {
    std::string src = read_all(bundle_header());
    const std::string lock_line = "BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, atomic, 1);";
    const auto pos = src.find(lock_line);
    REQUIRE(pos != std::string::npos);  // 锚点必须存在(否则 fixture 已失效)
    src.replace(pos, lock_line.size(), "");

    const auto locks = cpptlm::cep::scan_cohif_width_locks(src);
    REQUIRE(locks.size() == cpptlm::cep::cohif_expected_width_locks().size() - 1);
}

TEST_CASE("CC-02 反向 fixture ③ 字段宽度 +1 ⇒ 编译必失败(且原因命中宽度锁)",
          "[cep][cohif-carrier]") {
    TempDir tmp;
    tmp.write_shadow_header(cpptlm::cep::poison_cohif_bundle(
        read_all(bundle_header()), cpptlm::cep::CohIfPoison::WidthPlusOne));

    const auto res = compile_width_probe(&tmp.path());
    INFO("编译输出:\n" << res.output);
    REQUIRE(res.rc != 0);  // 编译**确实**失败
    // ⚠️ **因正确原因红** —— 失败必须源于 `BUNDLE_FIELD_WIDTH` 的宽度锁,
    //    而**不是**别的(例如类型不匹配)⇒ 断言输出命中宽度锁的特征串
    REQUIRE(res.output.find("Bundle field width mismatch") != std::string::npos);
}

TEST_CASE("CC-02 反向 fixture ④ 加虚函数 ⇒ 编译必失败(且原因命中 trivially copyable)",
          "[cep][cohif-carrier]") {
    TempDir tmp;
    tmp.write_shadow_header(cpptlm::cep::poison_cohif_bundle(
        read_all(bundle_header()), cpptlm::cep::CohIfPoison::VirtualFunction));

    const auto res = compile_width_probe(&tmp.path());
    INFO("编译输出:\n" << res.output);
    REQUIRE(res.rc != 0);
    REQUIRE(res.output.find("trivially copyable") != std::string::npos);
}

TEST_CASE("CC-02 正控:干净头的编译期锁全绿(编译成功)", "[cep][cohif-carrier]") {
    const auto res = compile_width_probe(nullptr);
    INFO("编译输出:\n" << res.output);
    REQUIRE(res.rc == 0);
}

// ══════════════════════════════════════════════════════════════════════
//  CC-04 —— JSON 装配后事务穿过 adapter 抵达对端,且 N ≥ 2
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("CC-04 ① 一次 CohIf 事务穿过 adapter 抵达对端(端到端,非仅绑定成功)",
          "[cep][cohif-carrier]") {
    REQUIRE(cpptlm::cep::testing::cohif_transaction_reaches_peer(
                cpptlm::cep::testing::load_pe_topology()) == ReachOutcome::Arrived);
}

TEST_CASE("CC-04 ② 实例端口数 N ≥ 2(isMultiPort 要求 port_count > 1)", "[cep][cohif-carrier]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cpptlm::cep::testing::load_pe_topology()));

    auto* pe = dynamic_cast<tlm::cep::ProtocolEngineBase<2>*>(
        factory.getInstanceForTesting("pe0"));
    REQUIRE(pe != nullptr);
    REQUIRE(pe->num_ports() >= 2u);
    REQUIRE(ChStreamAdapterFactory::get().isMultiPort("CohIfProtocolEngine"));
}

TEST_CASE("CC-04 ③ N = 1 ⇒ 编译期被 static_assert 拒绝(且原因命中断言)",
          "[cep][cohif-carrier]") {
    // 正控:N=2 ⇒ 编译成功
    const auto ok = compile_macro_probe(2);
    INFO("N=2 编译输出:\n" << ok.output);
    REQUIRE(ok.rc == 0);

    // 反向:N=1 ⇒ 编译失败,且失败原因**必须**命中宏内的 `static_assert(n > 1)`
    const auto bad = compile_macro_probe(1);
    INFO("N=1 编译输出:\n" << bad.output);
    REQUIRE(bad.rc != 0);
    REQUIRE(bad.output.find("端口数 N 必须 > 1") != std::string::npos);
}

TEST_CASE("CC-04 反向 fixture:断开端口绑定 ⇒ 端到端断言必红", "[cep][cohif-carrier]") {
    // 断链形态 ①:无连接 ⇒ 装配成功但事务**通不到**(必须判为 `NotArrived`,
    //         而**不是** `!= Arrived` —— 后者会把「装配被拒」也算成「红」)
    {
        json broken = cpptlm::cep::testing::load_pe_topology();
        broken["connections"] = json::array();
        REQUIRE(cpptlm::cep::testing::cohif_transaction_reaches_peer(broken) ==
                ReachOutcome::NotArrived);
    }
    // 断链形态 ②:dst 指向不存在的端口索引 ⇒ 装配层拒绝该连接
    {
        json broken = cpptlm::cep::testing::load_pe_topology();
        broken["connections"][0]["dst"] = "pe1.9";
        REQUIRE(cpptlm::cep::testing::cohif_transaction_reaches_peer(broken) !=
                ReachOutcome::Arrived);
    }
}
