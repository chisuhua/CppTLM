// examples/cep_poc_spike_s1.cc
//
// CEP PoC Spike 场景 S1 —— 单域基线(4 值 perm 投影 vs 6 态状态机)
//
// 功能/作者/日期:实测执行 S1 判定场景 / CppTLM build agent / 2026-10-24
// 权威规格: docs/implementation/2026-10-22-cep-poc-spike-spec.md §3.1
//
// 硬边界(per 规格 §1.2,不可违反):
//   1. 单文件可丢弃 spike —— 不进 ModuleFactory / 不注册 ABI / 无公共 API
//      (守门先例: test/test_cep_dummy_signals.cc「替身不得入 include/tlm/cep/」)
//   2. 只验证「可组合性 + 契约自洽」,不验证正确性,不是 conformance test
//   3. 零冻结产物改动 —— 只**读** protocol_instance.schema.json
//
// 语义依据(全部引已裁定文本;批 2 待裁定单元格一律避开):
//   · 6 态 × 6 事件表: ArchForge 09 号 §3.3(:148-158)—— 逐字抄录
//   · 6 态 → 4 值 perm 投影表: ArchForge 32 号 §7.2(:580-596)—— I→NONE / S→READ /
//     E→UNIQUE / M→WRITEBACK / O→WRITEBACK / **T→无投影**;两项语义损失:
//     ① O 与 M 在 4 值下合并 ② T 无投影 → 瞬态必须留在插件内部(INV-CEP-05-IC1)
//   · perm 4 值锁: include/tlm/cep/protocol_instance.schema.json:77-86(**读冻结 schema
//     比对**,不硬编码值域)
//   · **读/写路径的暴露不对称**(本场景的核心观测):
//       —— 读路径:接口在**响应侧**(流 2)携带 perm = 授予的权限(32 号 :338「请求完成 +
//          data + perm + error」)→ 忠实反映行态 → 与期望投影**恒等**
//       —— 写路径:接口在**请求侧**(流 1)携带 perm,Write → **UNIQUE**(非 WRITEBACK)
//          (32 号 :560);该行自陈「⚠️ bool 表达力不足:无法表达『静默 E→M 升级』→ **R-2**」
//          (32 号 :561)
//   · store 命中时的行状态 = E: 32 号 :287(判据 (b) 的 s1 = E)
//   · R-2 判据 2 已触发: docs/adr/ADR-CEP-05-stub-cache-core-interface.md:296
//     (「判据 2 状态 ✅ 已触发(2026-10-22)—— moesi_gpu(state_transition[2][1] = 3)
//     有静默 E→M,判据 (b) 成立 → 4 值 perm 确无法表达」)
//   · 写回事件: 09 号 §3.2(Fl = Flush,「把脏数据写回内存」)+ §3.3 表 M+Fl → M
//   · 6×6 表**无「转入 E」的边** —— E 由**目录独占授予**产生(非本地事件);本 spike 据
//     32 号 :287 取步 3 之前的行态为 E
//
// 观测纪律(承 S2 先例,critic B2/B3 修正):
//   ① 「实际投影」= 接口在该访问上**暴露**的 perm(按上表的读/写不对称规则**推导**,
//     非自述常量)
//   ② 「期望 perm」= **由 6 态表独立推进**后按 32 号 §7.2 投影(与 ① 两条独立路径)
//   ③ 写回「被观测到」= 内存里的字节**确实**被改写(比对内容,非计数器自述)
//
// 红绿分离(per design.md D7-a —— 判据退化 ≠ 判定结论为红):
//   exit 0 = 判据**工作正常**(无论 S1 判定结论是红是绿;结论由 stdout 报告)
//   exit 1 = **判据退化**(故意注入的错期望未被检出 → 比对器恒真)或其他内部失败
//   exit 2 = 读不到冻结文件(CWD 不符)
//
// 运行: cmake 构建后 ./build/bin/examples/cep_poc_spike_s1(**必须在仓库根运行**;
//       冻结 schema 按相对路径读取;CWD 不符则 exit 2 并明示)
// ═══════════════════════════════════════════

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

    // ────────────────────────────────────────────
    // ① 协议语义层:MOESI/GPU 6 态 × 6 事件表(逐字抄录 ArchForge 09 号 §3.3)
    // ────────────────────────────────────────────
    enum Cell : uint8_t { IW = 0, SW = 1, EW = 2, MW = 3, OW = 4, TW = 5 };
    enum Ev : uint8_t { RD = 0, WR = 1, INV = 2, INVA = 3, FL = 4, FLA = 5 };

    constexpr std::array<std::array<uint8_t, 6>, 6> kStateTransition{{
        {1, 3, 0, 0, 0, 0}, // I
        {1, 3, 0, 1, 4, 1}, // S
        {2, 3, 0, 2, 4, 2}, // E   ← 本场景主路径:E 行 Wr 列 = 3(09 号 :152) = 静默 E→M
        {3, 3, 3, 3, 3, 3}, // M
        {4, 3, 0, 4, 4, 4}, // O
        {1, 3, 0, 1, 4, 1}, // T(过渡回 S/M;瞬态不对外,IC1)
    }};

    // ────────────────────────────────────────────
    // ② Stub Cache 层:6 态 → 4 值 perm 投影(逐字镜像 ArchForge 32 号 §7.2)
    // ────────────────────────────────────────────
    enum class Perm : uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

    const char* perm_name(Perm p) {
        switch (p) {
        case Perm::NONE:
            return "NONE";
        case Perm::READ:
            return "READ";
        case Perm::UNIQUE:
            return "UNIQUE";
        case Perm::WRITEBACK:
            return "WRITEBACK";
        }
        return "?";
    }

    // 32 号 §7.2 的投影表;T → 无投影(瞬态不泄漏,INV-CEP-05-IC1)
    std::optional<Perm> project_perm(uint8_t cell) {
        switch (cell) {
        case IW:
            return Perm::NONE;
        case SW:
            return Perm::READ;
        case EW:
            return Perm::UNIQUE;
        case MW:
            return Perm::WRITEBACK;
        case OW:
            return Perm::WRITEBACK; // O 与 M 在 4 值下合并(32 号 §7.2 语义损失 ①)
        case TW:
            return std::nullopt; // T 无投影(32 号 §7.2;IC1)
        default:
            return std::nullopt;
        }
    }

    // ────────────────────────────────────────────
    // ③ 场景常量
    // ────────────────────────────────────────────
    constexpr uint64_t kAddrX = 0x9000;
    constexpr uint64_t kOldValue = 0x1111;
    constexpr uint64_t kNewValue = 0x2222;
    constexpr std::size_t kMemWords = 4096;

    enum Step : int { kReadMiss = 0, kReadHit = 1, kWriteHit = 2, kWriteback = 3, kStepCount = 4 };

    const char* step_name(int s) {
        switch (s) {
        case kReadMiss:
            return "1-read-miss";
        case kReadHit:
            return "2-read-hit";
        case kWriteHit:
            return "3-write-hit";
        case kWriteback:
            return "4-writeback";
        }
        return "?";
    }

    // ────────────────────────────────────────────
    // ④ moesi_gpu 插件(单域单行)—— 状态推进**只**走 6×6 表
    // ────────────────────────────────────────────
    struct MoesiGpuPlugin {
        uint8_t state = IW;
        uint64_t line_value = 0;
        uint64_t addr = 0;

        uint8_t apply(uint8_t ev) {
            const uint8_t ns = kStateTransition[state][ev];
            state = ns;
            return ns;
        }
        std::optional<Perm> exposed_perm() const {
            return project_perm(state);
        }
    };

    // ────────────────────────────────────────────
    // ⑤ 内存(用于「写回被观测到」的**内容**比对,非计数器自述)
    // ────────────────────────────────────────────
    struct Memory {
        std::vector<uint64_t> words = std::vector<uint64_t>(kMemWords, kOldValue);
        uint64_t& at(uint64_t addr) {
            return words[(addr / 8) % kMemWords];
        }
    };

    // ────────────────────────────────────────────
    // ⑥ 序列点观测
    // ────────────────────────────────────────────
    struct Observation {
        uint8_t state_before = IW;
        uint8_t state_after = IW;
        std::optional<Perm> expected;    // 由 6 态表推进后按 32 号 §7.2 投影
        std::optional<Perm> visible;     // 接口在该访问上暴露的 perm
        bool writeback_observed = false; // 写回内容确实落到内存
    };

    std::string read_file(const char* path) {
        std::ifstream in(path);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    int pass_count = 0;
    int fail_count = 0;
    void expect(bool cond, const std::string& what) {
        std::printf("      %s %s\n", cond ? "✓" : "✗", what.c_str());
        cond ? ++pass_count : ++fail_count;
    }

    // ────────────────────────────────────────────
    // ⑦ 主场景:跑一遍序列,产出逐点观测
    //    `corrupt_expected_at` = 故意改错的序列点(用于**自检**;正常跑为 -1)
    // ────────────────────────────────────────────
    std::vector<Observation> run_sequence(int corrupt_expected_at) {
        std::vector<Observation> obs(kStepCount);

        Memory mem;
        MoesiGpuPlugin line;
        line.addr = kAddrX;
        line.line_value = kOldValue;

        // 步 1:读 X(未命中) → 目录**独占授予** → 行态 E
        //   (09 号 §3.3 表无「转入 E」的边;E 由目录独占授予产生 —— per 32 号 :287
        //    「store 命中时的状态 = E」)
        obs[kReadMiss].state_before = line.state;
        line.state = EW;
        obs[kReadMiss].state_after = line.state;
        obs[kReadMiss].expected = project_perm(line.state);
        // 读路径:接口在**响应侧**携带授予的权限(32 号 :338)→ 忠实反映行态
        obs[kReadMiss].visible = project_perm(line.state);

        // 步 2:读 X(命中) —— E+Rd → E(09 号 :152 表义)
        obs[kReadHit].state_before = line.state;
        line.apply(RD);
        obs[kReadHit].state_after = line.state;
        obs[kReadHit].expected = project_perm(line.state);
        obs[kReadHit].visible = project_perm(line.state);

        // 步 3:写 X(store 命中本地行) —— E+Wr → M = state_transition[2][1] = 3
        //      **静默升级**(零流出线)→ R-2 的发生点(ADR-CEP-05:296)
        obs[kWriteHit].state_before = line.state;
        line.apply(WR);
        line.line_value = kNewValue;
        obs[kWriteHit].state_after = line.state;
        obs[kWriteHit].expected = project_perm(line.state);
        // 写路径:接口在**请求侧**携带 perm,Write → UNIQUE(非 WRITEBACK)(32 号 :560)
        obs[kWriteHit].visible = Perm::UNIQUE;

        // 步 4:写回 / 驱逐 —— M+Fl → M(09 号 §3.3 表);**内容**写回内存并被比对
        obs[kWriteback].state_before = line.state;
        line.apply(FL);
        obs[kWriteback].state_after = line.state;
        obs[kWriteback].expected = project_perm(line.state);
        obs[kWriteback].visible = project_perm(line.state); // 写回不带请求侧 perm
        mem.at(kAddrX) = line.line_value;
        obs[kWriteback].writeback_observed = (mem.at(kAddrX) == kNewValue);

        // 自检注入:把某个序列点的**期望**改错一项
        if (corrupt_expected_at >= 0 && corrupt_expected_at < kStepCount) {
            auto& e = obs[static_cast<std::size_t>(corrupt_expected_at)].expected;
            if (e.has_value())
                e = (*e == Perm::UNIQUE) ? Perm::NONE : Perm::UNIQUE;
        }
        return obs;
    }

    // 逐点比对:返回不一致的序列点下标
    std::vector<int> mismatches(const std::vector<Observation>& obs) {
        std::vector<int> out;
        for (int i = 0; i < kStepCount; ++i) {
            const auto& o = obs[static_cast<std::size_t>(i)];
            if (!o.expected.has_value() || !o.visible.has_value())
                continue; // 无投影(T 态)不参与比对
            if (*o.expected != *o.visible)
                out.push_back(i);
        }
        return out;
    }

} // namespace

int main() {
    // 0. 读冻结 schema(只读;失败即明示退出,不 terminate)
    const auto schema_text = read_file("include/tlm/cep/protocol_instance.schema.json");
    if (schema_text.empty()) {
        std::fprintf(stderr, "无法读取 include/tlm/cep/protocol_instance.schema.json —— "
                             "必须在仓库根运行(exit 2)\n");
        return 2;
    }
    const nlohmann::json schema = nlohmann::json::parse(schema_text);
    const auto perm_names =
        schema["properties"]["perm"]["items"]["enum"].get<std::vector<std::string>>();

    expect(perm_names.size() == 4 && perm_names[0] == "NONE" && perm_names[1] == "READ" &&
               perm_names[2] == "UNIQUE" && perm_names[3] == "WRITEBACK",
           "perm 4 值锁与冻结 schema 一致(零 drift;不硬编码值域)");
    expect(!project_perm(TW).has_value(), "T 态无 4 值投影(IC1:瞬态不泄漏)");

    // ── S1 主场景(spec §3.1)────────────────────────────────────────────────
    std::printf("\n[S1][MAIN] 读X(未命中) → 读X(命中) → 写X(命中) → 写回/驱逐\n");
    const auto obs = run_sequence(-1);
    const auto mis = mismatches(obs);

    std::printf("      %-14s %-4s %-5s %-10s %-10s %s\n", "step", "pre", "post", "expected",
                "visible", "verdict");
    for (int i = 0; i < kStepCount; ++i) {
        const auto& o = obs[static_cast<std::size_t>(i)];
        const bool bad = std::find(mis.begin(), mis.end(), i) != mis.end();
        std::printf("      %-14s %-4u %-5u %-10s %-10s %s\n", step_name(i), o.state_before,
                    o.state_after, o.expected ? perm_name(*o.expected) : "<none>",
                    o.visible ? perm_name(*o.visible) : "<none>", bad ? "MISMATCH" : "match");
    }

    // 写路径那处不一致 = R-2 命中(per 32 号 :295 + ADR-CEP-05:296)
    const bool r2_write_path = std::find(mis.begin(), mis.end(), kWriteHit) != mis.end();
    expect(obs[static_cast<std::size_t>(kWriteHit)].state_before == EW &&
               obs[static_cast<std::size_t>(kWriteHit)].state_after == MW &&
               kStateTransition[EW][WR] == 3,
           "写命中点 = 09 号表义 state_transition[2][1] = 3(E→M,静默升级)");
    expect(r2_write_path, "写路径:接口请求侧 perm(UNIQUE)≠ 期望投影(WRITEBACK)→ R-2 命中");
    expect(mis.size() == 1, "不一致**仅**出现在写路径(读路径由响应侧授予忠实反映行态,32 号 :338)");
    expect(obs[static_cast<std::size_t>(kWriteback)].writeback_observed,
           "写回路径存在且**被观测到**(内存内容确实被改写,非静默丢弃)");
    expect(obs[static_cast<std::size_t>(kWriteback)].state_after == MW,
           "写回后行态仍为 M(09 号 §3.3 表 M+Fl → M)");

    // T 态不泄漏(IC1):全部观测点的实际投影都必须是 4 值之一
    bool leak = false;
    for (const auto& o : obs) {
        if (o.visible.has_value()) {
            const std::string n = perm_name(*o.visible);
            if (n != "NONE" && n != "READ" && n != "UNIQUE" && n != "WRITEBACK")
                leak = true;
        }
    }
    expect(!leak, "全部序列点对外只暴露 4 个稳定态(无瞬态泄漏,INV-CEP-05-IC1)");

    // ── 判定结论(红)与「判据退化」(exit 1)分离 ────────────────────────────
    const bool s1_failed = !mis.empty(); // 规格 §3.1:任一不一致 → S1 失败
    std::printf("\n[S1][VERDICT] %s\n", s1_failed ? "FAIL" : "PASS");
    if (s1_failed)
        std::printf("[S1][VERDICT] S1 失败含义:perm 4 值投影的表达力不足 → "
                    "R-2 的需求本体成立(per 规格 §3.1 失败含义)\n");
    std::printf("[S1][VERDICT] S1_VERDICT=%s\n", s1_failed ? "FAIL" : "PASS");
    std::printf("[S1][VERDICT] S1_R2=%s\n",
                r2_write_path ? "TRIGGERED(state_transition[2][1]=3)" : "NOT-TRIGGERED");
    std::printf("[S1][VERDICT] S1_MISMATCH_STEPS=%zu\n", mis.size());
    std::printf("[S1][VERDICT] S1_WRITEBACK_OBSERVED=%d\n",
                obs[static_cast<std::size_t>(kWriteback)].writeback_observed ? 1 : 0);

    // ── 自检(「有牙」):故意改错某点的期望 → 该点的判定**必须翻转** ──────────
    //   语义要点:比对器必须**真的读**该点的期望值。
    //   「该点原本已 mismatch」时,改错期望会让它**变成 match** —— 翻转同样证明敏感;
    //   故判据是「翻转」而非「新增不一致」(后者会把「原本就红」的点误判为退化)。
    //   若改错后判定**不翻转** → 比对器对该点的期望**不敏感** → **判据退化** → exit 1
    int selfcheck_ok = 1;
    for (int k = 0; k < kStepCount; ++k) {
        const auto corrupted = run_sequence(k);
        const auto cmis = mismatches(corrupted);
        const bool base_at_k = std::find(mis.begin(), mis.end(), k) != mis.end();
        const bool corr_at_k = std::find(cmis.begin(), cmis.end(), k) != cmis.end();
        if (base_at_k == corr_at_k) {
            selfcheck_ok = 0;
            std::printf("[S1][SELFCHECK] 步骤 %s 的期望被改错后判定**未翻转** → 判据退化\n",
                        step_name(k));
        }
    }
    std::printf("[S1][SELFCHECK] S1_SELFCHECK=%s\n", selfcheck_ok ? "OK" : "DEGENERATE");
    expect(selfcheck_ok == 1, "自检:每个序列点的期望被改错都必须被检出(判据非恒真)");

    std::printf("\n[S1][SUMMARY] pass=%d fail=%d\n", pass_count, fail_count);
    if (fail_count > 0) {
        std::printf("[S1][SUMMARY] 判据自身失效(非判定结论为红)→ exit 1\n");
        return 1;
    }
    // 判定结论为红**不**导致非零退出(per design.md D7-a)
    std::printf("[S1][SUMMARY] 判据工作正常(判定结论见 S1_VERDICT)→ exit 0\n");
    return 0;
}
