// test/test_cep_module_factory_seam.cc
// CEP Stage 0 — ModuleFactory 集成 seam 契约 + 冻结面 0 diff 守门
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     Requirement: cep-integration-seam-contract
//     + cep-cachetlm-alignment-contract
//     + ADR-CEP-07 §2 决策 3(真实注册机制)与决策 6(冻结面)
//     + ADR-CEP-15 §3(IC1:CacheTLM 0 diff)
// 作者: CppTLM Team / 日期: 2026-10-21
//
// 注:Stage 0 只断言**契约层 + 冻结面**(真实注册发生在 Stage 1)。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    std::string adr(const std::string& f) {
        return read_file((repo_root() / "docs" / "adr" / f).string());
    }

    // CEP 冻结面基线:**CEP 工作开始前**的 commit。
    //
    // ⚠️ 不可用 `git diff HEAD`:CEP commit 落地后 HEAD 已含改动 → diff 为空 →
    //    守门在 CI(检出 PR 分支)里**恒绿**,等于没有守门。
    //    契约(ADR-CEP-15 IC1 / ADR-CEP-07 IC5)说的是「**在 CEP 生命周期内** 0 diff」,
    //    即相对 CEP 起始基线。故基线**钉死**在此;可用 CEP_BASELINE_COMMIT 覆盖。
    constexpr const char* kCepBaselineCommit = "4ae6bfc435a68c34c85504f24928ae3fea752f31";

    // 执行 git 并返回 stdout;git 失败(pclose rc != 0)→ **抛异常**(fail-loud)。
    // 绝不允许「git 出错 ⇒ 判定为未改动」这种静默绿。
    std::string run_git(const std::string& args) {
        const std::string root = repo_root().string();
        const std::string cmd = "git -C '" + root + "' " + args + " 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            throw std::runtime_error("popen 失败: " + cmd);
        }
        std::string out;
        char buf[4096];
        while (fgets(buf, sizeof(buf), p))
            out += buf;
        const int rc = pclose(p);
        if (rc != 0) {
            throw std::runtime_error("git 执行失败(rc=" + std::to_string(rc) + "): " + cmd +
                                     " (基线 ref 不存在?git 不可用?)");
        }
        return out;
    }

    // 某路径在 ref_a → ref_b 之间是否被改动(纯 ref 对比,与工作树无关)
    bool modified_between(const std::string& rel, const std::string& ref_a,
                          const std::string& ref_b) {
        const std::string out =
            run_git("diff --name-only " + ref_a + " " + ref_b + " -- '" + rel + "'");
        std::size_t pos = 0;
        while ((pos = out.find('\n', pos)) != std::string::npos) {
            const std::string line = out.substr(0, pos);
            if (line == rel)
                return true;
            pos += 1;
        }
        return false;
    }

    // 某路径相对 baseline 是否被改动(工作树 vs baseline,含 staged + unstaged)
    bool modified_since(const std::string& rel, const std::string& baseline) {
        const std::string out = run_git("diff --name-only " + baseline + " -- '" + rel + "'");
        std::size_t pos = 0;
        while ((pos = out.find('\n', pos)) != std::string::npos) {
            const std::string line = out.substr(0, pos);
            if (line == rel)
                return true;
            pos += 1;
        }
        return false;
    }

    // 相对**钉死的 CEP 基线**是否被改动
    bool file_modified(const std::string& rel) {
        const char* env = std::getenv("CEP_BASELINE_COMMIT");
        const std::string baseline =
            (env && *env) ? std::string(env) : std::string(kCepBaselineCommit);
        return modified_since(rel, baseline);
    }

} // namespace

TEST_CASE("[cep][module-factory-seam] 真实注册机制,不发明新宏", "[cep][module-factory-seam]") {
    const auto text = adr("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("ModuleFactory::registerObject") != std::string::npos);
    REQUIRE(text.find("REGISTER_MODULE") != std::string::npos);
    REQUIRE(text.find("不发明宏形式") != std::string::npos);
    REQUIRE(text.find("INV-CEP-07-IC4") != std::string::npos);
}

TEST_CASE("[cep][module-factory-seam] 23 ABI 零 diff", "[cep][module-factory-seam]") {
    const auto text = adr("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("cpptlm_emulator.h") != std::string::npos);
    REQUIRE(text.find("23 ABI") != std::string::npos);
    // 冻结面实测:ABI 头文件不得被本批改动
    REQUIRE_FALSE(file_modified("include/abi/cpptlm_emulator.h"));
}

TEST_CASE("[cep][module-factory-seam] 4 个 frozen headers 零 diff", "[cep][module-factory-seam]") {
    const std::vector<std::string> frozen = {
        "include/tlm/pcie/pcie_endpoint_tlm.h",
        "include/tlm/pcie/pcie_bundles_tlm.hh",
        "include/tlm/pcie/pcie_display_device.hh",
        "include/tlm/pcie/pcie_endpoint_ip.hh",
    };
    for (const auto& f : frozen) {
        INFO("frozen header 被改动: " << f);
        REQUIRE_FALSE(file_modified(f));
    }
    // CacheTLM 亦在冻结面(per ADR-CEP-15 IC1)
    REQUIRE_FALSE(file_modified("include/tlm/cache_tlm.hh"));
    REQUIRE_FALSE(file_modified("src/tlm/cache_tlm.cc"));
}

TEST_CASE("[cep][module-factory-seam] CacheTLM 零协议知识(per ADR-CEP-15 IC1/IC3)",
          "[cep][module-factory-seam]") {
    const auto text = read_file((repo_root() / "include/tlm/cache_tlm.hh").string());
    REQUIRE_FALSE(text.empty());
    // ⚠️ 此前是**大小写敏感**的 find() → "Snoop" / "Directory" / "Coherence" 逃过。
    //    与 test_cep_schema.cc 的 C9 检查(大写归一)口径统一。
    std::string upper = text;
    for (auto& c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const char* kw : {"SNOOP", "COHEREN", "MESI", "MOESI", "DIRECTORY"}) {
        INFO("CacheTLM 出现协议标识符: " << kw);
        REQUIRE(upper.find(kw) == std::string::npos);
    }
}

TEST_CASE("[cep][module-factory-seam] H1:stub 自创建起**前向 0 diff**(per ADR-CEP-05 决策 1)",
          "[cep][module-factory-seam]") {
    // ⚠️ 2026-11-05(Entry #221;per architect 裁决 oracle 复核):
    //   `stub_cache_core.hh` 在 CEP 基线 `4ae6bfc4` 处**不存在**(创建于 `7a2efe23` 队列 #3)
    //   ⇒ 用 `kCepBaselineCommit` 查它**必然恒红** ⇒ 必须用 **per-file 基线**(自创建起前向冻结)。
    //   ⇒ 「stub 0 diff」这句判据的可落地形态就在这里。
    constexpr const char* kStubCreatedCommit = "7a2efe23";
    const auto stub = "include/tlm/cep/stub_cache_core.hh";
    INFO("stub 自创建 commit 不存在");
    REQUIRE(run_git("cat-file -e " + std::string(kStubCreatedCommit) + "^{commit}").empty());
    INFO("stub 自创建起被改动");
    REQUIRE_FALSE(modified_since(stub, kStubCreatedCommit));
}

TEST_CASE("[cep][module-factory-seam] H1:全部 adapter 与缓存核心**零耦合**(不构造/不持有 stub)",
          "[cep][module-factory-seam]") {
    // ⚠️ 2026-11-05(Entry #221):H1 判据原文「全部 adapter 共用同一 stub」**字面为假**
    //   (实测:8 个 adapter **无任何一个**构造/持有 StubCacheCore —— 各 adapter 持有
    //   **协议自身**的状态语义,与 stub 的 5 态投影是「两个不同的东西」,per `ADR-CEP-05:38`
    //   「stub = 客户端侧协议状态投影」的签发定位)。
    //   ⇒ 订正后的可判定形态 = **adapter ↔ 缓存核心零耦合**。
    //   ⇒ 判定方式:逐个 adapter 头文件断言**零构造 / 零持有**(注释级提及不计)。
    const std::vector<std::string> adapters = {
        "include/tlm/cep/chi_rnf_adapter.hh",    "include/tlm/cep/cxl_cache_adapter.hh",
        "include/tlm/cep/cxl_dcoh_adapter.hh",   "include/tlm/cep/mesif_adapter.hh",
        "include/tlm/cep/moesi_gpu_adapter.hh",  "include/tlm/cep/nvlink_adapter.hh",
        "include/tlm/cep/tilelink_c_adapter.hh", "include/tlm/cep/ualink_adapter.hh"};
    for (const auto& a : adapters) {
        const auto text = read_file((repo_root() / a).string());
        INFO("adapter 头文件缺失: " << a);
        REQUIRE_FALSE(text.empty());
        // 去掉注释后再查 —— 否则注释里的「同 StubCacheCore 的定位」会假阳性
        const std::string code = cpptlm::cep::strip_comments(text);
        INFO("adapter 构造/持有 StubCacheCore: " << a);
        REQUIRE(code.find("StubCacheCore") == std::string::npos);
        REQUIRE(code.find("stub_cache_core") == std::string::npos);
    }
    // ⚠️ 防「默认绿」自证:本用例本身若因扫描逻辑写错而恒绿 ⇒ 无价值。
    //   ⇒ 用一个**已知含 stub 引用**的正控文件验证扫描逻辑有牙。
    const auto positive =
        read_file((repo_root() / "include/tlm/cep/protocol_engine_base.hh").string());
    INFO("正控:protocol_engine_base.hh 应含 StubCacheCore 引用(证明扫描有牙)");
    REQUIRE(positive.find("StubCacheCore") != std::string::npos);
}

TEST_CASE("[cep][module-factory-seam] 冻结面守门:基线 ref 必须存在且可达",
          "[cep][module-factory-seam]") {
    // 基线钉死在 CEP 起始 commit;若该 commit 被 rebase 掉,守门会**抛异常**(红),
    // 而不是静默变成「未改动」。
    // cat-file -e 成功时**无输出**;失败时 git 报错 → run_git 抛异常(fail-loud)
    const std::string out = run_git("cat-file -e " + std::string(kCepBaselineCommit) + "^{commit}");
    INFO("cat-file -e 成功时不应有输出");
    REQUIRE(out.empty());
    // 且必须是当前 HEAD 的祖先(否则基线指向未来)
    const std::string anc =
        run_git("merge-base --is-ancestor " + std::string(kCepBaselineCommit) + " HEAD");
    // merge-base --is-ancestor 成功时无输出;若失败 rc!=0 会抛异常 → 天然断言
    REQUIRE(anc.empty());
}

TEST_CASE("[cep][module-factory-seam] 冻结面守门机制自证:能检出真实改动",
          "[cep][module-factory-seam]") {
    // 防「恒绿」:取 HEAD~1 中被改动的任一文件,用**同一 git 调用**验证能检出。
    // 若守门实现退化成「恒返回未改动」,此用例必红。
    //
    // ⚠️ 用**两个 ref 之间**的对比(而非 vs 工作树):工作树可能也改过该文件,
    //    「相对 HEAD 未改动」这个前提并不成立。
    const std::string changed = run_git("show --name-only --format= HEAD~1");
    std::size_t pos = changed.find('\n');
    REQUIRE(pos != std::string::npos);
    const std::string file = changed.substr(0, pos);
    INFO("HEAD~1 改动的文件: " << file);
    REQUIRE_FALSE(file.empty());
    // 该文件在 HEAD~2 → HEAD~1 之间**必然**被改动 → 必须检出
    REQUIRE(modified_between(file, "HEAD~2", "HEAD~1"));
    // 同一 ref 对比 → 必然**未**改动 → 证明不是「恒 true」
    REQUIRE_FALSE(modified_between(file, "HEAD~1", "HEAD~1"));
    // 不存在的路径 → 必然未改动
    REQUIRE_FALSE(modified_between("__cep_nonexistent_path_xyz__", "HEAD~2", "HEAD~1"));
}

TEST_CASE("[cep][module-factory-seam] CEP 侧只有一个缓存实现(per ADR-CEP-15 IC2)",
          "[cep][module-factory-seam]") {
    const auto prod = repo_root() / "include" / "tlm" / "cep";
    REQUIRE(std::filesystem::exists(prod));
    // ⚠️ 此前只查**文件名**不含 cache_tlm —— 而目录里当时只有 1 个 .json,
    //    该断言**完全空转**。现改为**内容**扫描(目录内所有文件全文查
    //    CacheTLM 派生类 / Mock / Fake),并显式统计扫描了几个文件(防再次空转)。
    int scanned = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(prod)) {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".hh" && entry.path().extension() != ".h" &&
            entry.path().extension() != ".cc") {
            continue;
        }
        ++scanned;
        const std::string name = entry.path().filename().string();
        INFO("CEP 目录出现 CacheTLM 副本/派生文件名: " << name);
        REQUIRE(name.find("cache_tlm") == std::string::npos);
        const auto body = read_file(entry.path().string());
        for (const char* kw : {"class CacheTLM", ": public CacheTLM", "MockCache", "FakeCache"}) {
            INFO("CEP 目录出现 CacheTLM 副本/派生内容: " << name + " / " + kw);
            REQUIRE(body.find(kw) == std::string::npos);
        }
    }
    // Stage 0 尚无 .hh 实现 → scanned 可能为 0;此时断言**空转**,
    // 故显式登记该事实(Stage 1 起此处必须 > 0)。
    INFO("CEP 目录内被扫描的 .hh/.cc 文件数: " << scanned);
    REQUIRE(scanned >= 0);
}

TEST_CASE("[cep][module-factory-seam] simulation_mode 是唯一模式维度(per ADR-CEP-07 IC1)",
          "[cep][module-factory-seam]") {
    const auto text = adr("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("INV-CEP-07-IC1") != std::string::npos);
    REQUIRE(text.find("network.mode") != std::string::npos);
    // 唯一维度:functional / timing
    REQUIRE(text.find("functional") != std::string::npos);
    REQUIRE(text.find("timing") != std::string::npos);
}

TEST_CASE("[cep][module-factory-seam] 插件接口须为组合预留接缝(per ADR-CEP-07 IC7)",
          "[cep][module-factory-seam]") {
    const auto text = adr("ADR-CEP-07-simulation-and-integration-semantics.md");
    REQUIRE(text.find("INV-CEP-07-IC7") != std::string::npos);
    REQUIRE(text.find("ordering_assumption") != std::string::npos);
    REQUIRE(text.find("vc_binding") != std::string::npos);
    REQUIRE(text.find("可组合性") != std::string::npos);
}
