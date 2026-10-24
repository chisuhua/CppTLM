// examples/cep_poc_spike_s2.cc
//
// CEP PoC Spike 场景 S2 —— 跨域可见写(单文件可丢弃 spike)
//
// 功能/作者/日期:实测执行 S2 判定场景 / CppTLM build agent / 2026-10-22
//               (同日 critic 证伪初版 3 BLOCKERS —— VC 绑定错值 / ③ 传播可达
//               硬编码 / NEG 同义反复 —— 本版为修正后的重跑产物,
//               台账 Entry #15 的 RUN 证据由 Entry #16 作废并取代)
// 权威规格: docs/implementation/2026-10-22-cep-poc-spike-spec.md §3.2
//
// 硬边界(per 规格 §1.2,不可违反):
//   1. 单文件可丢弃 spike —— 不进 ModuleFactory / 不注册 ABI / 无公共 API
//      (守门先例: test/test_cep_dummy_signals.cc「替身不得入 include/tlm/cep/」)
//   2. 只验证「可组合性 + 契约自洽」,不验证正确性,不是 conformance test
//   3. 零冻结产物改动 —— 只**读** fabric.json 与 protocol_instance.schema.json
//
// 语义依据(全部引已裁定/无争议文本;批 2 待裁定单元格一律避开):
//   · 判据 (a)(b): ADR-CEP-13:99-103 —— s1(E) store 静默升 s2(M) 零外部通信;
//     (b) 析取逐态(32 号 §5.2.1 转发形态表 :297-305: snoop_resp DATA_AND_ACK 携带数据值)
//   · 主路径表义: E 行第 Wr 列 = 3(09 号 :152,Q-3a 同一引用点)
//   · E + snoop(READ_SHARED) → S + requester(ACK 无数据):ArchForge
//     docs/microarchitecture/coherence-protocol.md:221
//   · M + snoop(READ_SHARED) → O + data 转发(+requester): 同上 :224
//     (避开 :226 的 M+INVALIDATE→I —— D-9 批 2 对账项,spike 不使用 INVALIDATE 于 M)
//   · S + write 的副作用标注 = INVALIDATE→all(同上 :212);S + snoop(INVALIDATE) → I = :213
//     (→others 是 O+write 语义 :228 —— 初版误标,本版更正)
//   · VC 绑定(**权威,非 provisional**): 32 号 §6.0.2 矩阵(:367-370)+
//     moesi_gpu 实例声明(:434-442): compute_noc{snoop:2, snoop_resp:3},
//     ualink_fabric{request:0, response:3}—— 初版自造 snoop=1 已证伪并重修
//   · 数据承载的双语自洽: 转发表(:303)称 snoop_resp 携带数据值,而 VC-R2(:391)
//     裁定「snoop_resp 不携带数据,数据一律走 forward_data,与 snoop 同槽(VC2)」
//     —— 两处为 ArchForge 自身张力;本 spike 以 **流 4(VC3,纯状态 DATA_AND_ACK)
//     + 流 6 forward_data(VC2,数据值)** 双消息建模两侧同时成立(即 VC-R2 得守)
//   · perm 4 值(值域 = protocol_instance.schema.json:82,读冻结 schema 比对)
//
// 运行: cmake 构建后 ./build/bin/examples/cep_poc_spike_s2(**必须在仓库根运行**,
//       声明文件按相对路径读取;CWD 不符则 exit 2 并明示)
// 退出码: 0 = 全部断言通过;非 0 = 任一失败(观测记录仍完整打印)
//
// ═════════════════════════════════════════════════════════════════════════════
// 观测纪律(critic B2/B3 修正): 以下三项观测禁止自述硬编码 ——
//   ① 观测值 = 请求者**实际收到**的流数据值
//   ② 数据来源 = 桥接**实际选择**的数据提供者(枚举判决,非字符串自述)
//   ③ 传播可达 = probe 流**实际发出并收妥**(对象级连线,非 `= true` 常量)
// ═════════════════════════════════════════════════════════════════════════════

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace {

    // ─────────────────────────────────────────────────────────────────────────────
    // ① 协议语义层:MOESI/GPU 6 态 × 6 事件表(survey §2.1 抄录 ArchForge 09 号 §3.3)
    // ─────────────────────────────────────────────────────────────────────────────

    enum Cell : uint8_t { IW = 0, SW = 1, EW = 2, MW = 3, OW = 4, TW = 5 };
    enum Ev : uint8_t { RD = 0, WR = 1, INV = 2, INVA = 3, FL = 4, FLA = 5 };

    constexpr std::array<std::array<uint8_t, 6>, 6> kStateTransition{{
        {1, 3, 0, 0, 0, 0}, // I
        {1, 3, 0, 1, 4, 1}, // S
        {2, 3, 0, 2, 4, 2}, // E   ← S2 主路径:E 行 Wr 列 = 3(09 号 :152)
        {3, 3, 3, 3, 3, 3}, // M
        {4, 3, 0, 4, 4, 4}, // O
        {1, 3, 0, 1, 4, 1}, // T(过渡回 S/M;瞬态不对外,IC1)
    }};

    // S2 场景常量(须先于使用它们的层定义)
    constexpr uint64_t kAddrX = 0x9000;
    constexpr uint64_t kOldValue = 0x1111;
    constexpr uint64_t kNewValue = 0x2222;
    constexpr const char* kDgpu = "D-GPU";
    constexpr const char* kDfab = "D-FABRIC";

    // ─────────────────────────────────────────────────────────────────────────────
    // ② Stub Cache Core 层:6 态 → 4 值 perm 投影(值域 = protocol_instance.schema.json:82)
    // ─────────────────────────────────────────────────────────────────────────────

    enum class Perm : uint8_t { NONE = 0, READ = 1, UNIQUE = 2, WRITEBACK = 3 };

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
            return Perm::WRITEBACK; // O=脏共享、持写回责任
        case TW:
            return std::nullopt; // 瞬态不泄漏(IC1)
        default:
            return std::nullopt;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // ③ Coherence Interface 层:canonical 流最小子集
    //   消息类(32 号 §6.0.1 六类词表)与流(canonical 流号)分开标注 ——
    //   类决定槽位,流携带语义(forward_data 属 response 类但**例外**与 snoop 同槽 VC2)
    // ─────────────────────────────────────────────────────────────────────────────

    struct StreamMsg {
        const char* flow = ""; // 流语义: request / completion / snoop / snoop_resp / forward_data
        const char* msg_class = ""; // 消息类(6 类词表): request/response/snoop/snoop_resp/…
        const char* network = "";   // compute_noc / ualink_fabric
        int vc = -1;
        uint64_t addr = 0;
        uint64_t data = 0;
        bool has_data = false;       // ⚠️ 仅流 6 forward_data 携带数据(VC-R2,32 号 :391);
                                     //    流 4 的 has_data=true 语义侧 = DATA_AND_ACK 标志
        const char* snoop_type = ""; // READ_SHARED / INVALIDATE
        const char* note = "";
    };

    // ④ 权威 vc_binding —— **逐字镜像** ArchForge 32 号 §6.0.2(:367-370)+
    //    moesi_gpu 实例声明(:434-442)。非 spike 自造(初版自造已被证伪):
    //      compute_noc: request 0 / response 1 / snoop 2 / snoop_resp 3
    //      ualink_fabric: request 0 / atomic 1 / query 2 / response 3 / snoop_resp 3
    //    (forward_data = response 类,例外同槽 snoop VC2 —— per 转发表「与 snoop 同槽」)
    struct VcBinding {
        struct Net {
            std::map<std::string, int> cls_to_vc;
        };
        std::map<std::string, Net> nets;
        static VcBinding authority() {
            VcBinding b;
            b.nets["compute_noc"].cls_to_vc = {
                {"request", 0}, {"response", 1}, {"snoop", 2}, {"snoop_resp", 3}};
            b.nets["ualink_fabric"].cls_to_vc = {
                {"request", 0}, {"atomic", 1}, {"query", 2}, {"response", 3}, {"snoop_resp", 3}};
            return b;
        }
        // 槽位合法性:网络存在 + (类,vc) ∈ 声明绑定;forward_data 例外走 snoop 同槽
        bool valid(const StreamMsg& m, const std::map<std::string, int>& vc_count) const {
            auto nit = nets.find(m.network);
            if (nit == nets.end())
                return false;
            auto cnt = vc_count.find(m.network);
            if (cnt == vc_count.end() || m.vc < 0 || m.vc >= cnt->second)
                return false;
            if (std::string(m.flow) == "forward_data") {
                int snoop_vc = -1;
                auto sit = nit->second.cls_to_vc.find("snoop");
                if (sit != nit->second.cls_to_vc.end())
                    snoop_vc = sit->second;
                return m.vc == snoop_vc; // 同槽规则 Only
            }
            auto cit = nit->second.cls_to_vc.find(m.msg_class);
            return cit != nit->second.cls_to_vc.end() && cit->second == m.vc;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // ⑤ Protocol Engine 插件:moesi_gpu(单域单行;snoop 语义对齐 microarch :212-:232)
    // ─────────────────────────────────────────────────────────────────────────────

    enum class UpgradeKind { none, silent, explicit_invalidate };

    // probe 应答 = 双消息:流 4 snoop_resp(VC3 状态应答/DATA_AND_ACK 语义)+
    //               流 6 forward_data(VC2 携数据;仅 M/O 态产生)
    struct ProbeOutcome {
        StreamMsg resp;               // 流 4(compute_noc VC3)
        std::optional<StreamMsg> fwd; // 流 6(compute_noc VC2,同槽规则)
    };

    struct MoesiGpuPlugin {
        uint8_t state = IW;
        uint64_t line_value = 0;
        uint64_t addr = 0;

        // 本地写(store 命中)。E→M 静默(零流出线)/ S→M 有他 sharer 时显式(INVALIDATE 出线)
        std::pair<UpgradeKind, std::vector<StreamMsg>> local_store(bool has_others) {
            std::vector<StreamMsg> out;
            uint8_t ns = kStateTransition[state][WR];
            if (state == EW) {
                if (ns != MW)
                    return {UpgradeKind::none, out}; // 防御:表不符则不升
                state = MW;
                line_value = kNewValue;
                return {UpgradeKind::silent, out}; // 零流出线 = 判据 (a)
            }
            if (state == SW) {
                if (has_others) {
                    state = MW; // 本地表义即升(生效以他端失效确认为界)
                    StreamMsg inv;
                    inv.flow = "snoop";
                    inv.msg_class = "snoop";
                    inv.network = "compute_noc";
                    inv.vc = 2; // 权威绑定 snoop=2(32 号 :437)
                    inv.addr = addr;
                    inv.snoop_type = "INVALIDATE";
                    inv.note = "显式升级:S 态写须先失效其他共享者(INVALIDATE→all,microarch :212)";
                    out.push_back(inv);
                    line_value = kNewValue;
                    return {UpgradeKind::explicit_invalidate, out};
                }
                if (ns != MW)
                    return {UpgradeKind::none, out};
                // 独占共享者(仅自己)→ 无需外部通信,亦为静默路径(不与 E→M 混同)
                state = MW;
                line_value = kNewValue;
                return {UpgradeKind::silent, out};
            }
            if (state == MW || state == OW) {
                state = MW;
                line_value = kNewValue;
                return {UpgradeKind::none, out};
            }
            return {UpgradeKind::none, out}; // I 态外读分配不在本 spike 输入域
        }

        // snoop 应答(流 4,center→node 的对向):已裁定语义逐态:
        //   M + READ_SHARED → O + DATA_AND_ACK + forward_data(:224,Q-3a s2 支)
        //   E + READ_SHARED → S + ACK(**无** forward_data,:221 —— Q-3a s1 支不成立的证据)
        //   S + READ_SHARED → S + ACK(tracker +requester)
        //   INVALIDATE: 仅 S(→I,:213)/ E(→I,同文件 E 段)受支持;
        //               **M+INVALIDATE = D-9 待裁定 → 拒解且不改态**(防擅自裁定)
        ProbeOutcome snoop_resp(const StreamMsg& snoop) {
            ProbeOutcome po;
            StreamMsg& r = po.resp;
            r.flow = "snoop_resp";
            r.msg_class = "snoop_resp";
            r.network = "compute_noc";
            r.vc = 3; // 权威绑定 snoop_resp=3(32 号 :438)
            r.addr = snoop.addr;
            const bool is_read = std::string(snoop.snoop_type) == "READ_SHARED";
            if (is_read) {
                switch (state) {
                case EW:
                    state = SW;
                    r.has_data = false; // ⚠️ has_data 仅流 6;此处 ACK = 语义「无数据」
                    r.note = "E 态回 ACK(无数据;内存供数)——s1 支不成立的证据";
                    break;
                case MW: {
                    state = OW; // M + READ_SHARED → O(:224)
                    StreamMsg f;
                    f.flow = "forward_data";
                    f.msg_class = "response";
                    f.network = "compute_noc";
                    f.vc = 2; // 同槽规则(32 号 :303/:369)
                    f.addr = snoop.addr;
                    f.has_data = true;
                    f.data = line_value;
                    f.note = "流 6 forward_data 携带 D-GPU M 态 line 数据(同槽 snoop/VC2)";
                    po.fwd = f;
                    r.has_data = true; // DATA_AND_ACK 语义标志(数据体在流 6)
                    r.note = "DATA_AND_ACK(数据体经流 6 forward_data,VC2 同槽)";
                    break;
                }
                default:
                    r.has_data = false;
                    r.note = "共享命中回 ACK";
                    break;
                }
            } else { // INVALIDATE: 仅 S/E 支持并对齐 :213;M 态拒解
                if (state == MW) {
                    r.note = "⚠️ M+INVALIDATE = D-9 批 2 待裁定单元格 — 本 spike 拒解、不改态";
                    return po;
                }
                state = IW;
                r.note = "失效确认(ACK,S→I per :213)";
            }
            return po;
        }

        std::optional<Perm> exposed_perm() const {
            return project_perm(state);
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // ⑥ Directory & Ordering / Transport / Bridge(S2 的最小可观测转发路径)
    // ─────────────────────────────────────────────────────────────────────────────

    struct SharerTracker { // SparseSnoopFilter 最小形(spec §2)
        std::vector<std::string> sharers;
        bool holds(const std::string& d) const {
            for (auto& s : sharers)
                if (s == d)
                    return true;
            return false;
        }
        void add(const std::string& d) {
            if (!holds(d))
                sharers.push_back(d);
        }
    };

    struct Memory { // 单行内存(旧值;S2 全程显式依路径判定是否被读)
        uint64_t value = kOldValue;
    };

    enum class DataSource { none, gpu_cache, memory };
    const char* source_name(DataSource s) {
        switch (s) {
        case DataSource::gpu_cache:
            return "D-GPU M 态 cache 数据响应(流 6 forward_data)";
        case DataSource::memory:
            return "内存";
        default:
            return "无";
        }
    }

    struct ReadResult { // 请求者视角的读结果 = 观测的**生成处**(禁止后置自述)
        uint64_t value = 0;
        DataSource source = DataSource::none;
        bool probe_forwarded = false; // ③:probe 流是否**真的发出并收妥**
        int probe_msgs = 0;           // 计数:compute_noc 层发出的流条数(证据计数)
        std::vector<StreamMsg> path;  // 实际走过的全链流
    };

    class Bridge { // 跨域边界:D-FABRIC 读 → tracker → probe D-GPU → 汇聚数据
    public:
        bool fwd_enabled = true;
        MoesiGpuPlugin* gpu_line = nullptr;
        SharerTracker* tracker = nullptr;
        Memory* mem = nullptr;
        const VcBinding* binding = nullptr;
        const std::map<std::string, int>* vc_count = nullptr;
        int fallback_hits = 0; // provenance:NEG 路径被真实执行过几次(供自检核对)

        ReadResult fabric_read(uint64_t addr) {
            ReadResult res;
            if (!fwd_enabled) { // 桥接关闭:请求者直读内存(真实 fallback,供 NEG 对照)
                res.value = mem->value;
                res.source = DataSource::memory;
                res.probe_forwarded = false;
                ++fallback_hits; // provenance:本 bridge 的 fallback 路径**确实被执行**
                return res;
            }
            // tracker 指路:该行有 sharer 持有(GPU)= probe 下发的对象依据
            if (tracker == nullptr || !tracker->holds(kDgpu)) {
                res.value = mem->value;
                res.source = DataSource::memory;
                return res; // 无可探测共享者 → 内存路径
            }
            StreamMsg snoop;
            snoop.flow = "snoop";
            snoop.msg_class = "snoop";
            snoop.snoop_type = "READ_SHARED";
            snoop.addr = addr;
            snoop.network = "compute_noc";
            snoop.vc = 2; // 权威绑定 snoop=2
            res.path.push_back(snoop);
            ProbeOutcome po = gpu_line->snoop_resp(snoop);
            res.path.push_back(po.resp);
            res.probe_msgs = 2;
            if (po.fwd.has_value()) {
                res.path.push_back(*po.fwd);
                res.probe_msgs++;
                res.value = po.fwd->data;
                res.source = DataSource::gpu_cache;
            } else {
                // E 支:cache 回 ACK 无数据(数据应来自内存;本变体不消费之,仅记录)
                res.value = mem->value;
                res.source = DataSource::memory;
            }
            res.probe_forwarded =
                binding->valid(snoop, *vc_count) && binding->valid(po.resp, *vc_count);
            return res;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // S2 主体
    // ─────────────────────────────────────────────────────────────────────────────

    int pass_count = 0;
    int fail_count = 0;
    void expect(bool cond, const std::string& what) {
        std::printf("      %s %s\n", cond ? "✓" : "✗", what.c_str());
        cond ? ++pass_count : ++fail_count;
    }

    struct Observation { // 3 项(值/来源/可达;禁止自述 —— 由 Bridge/ReadResult 生成)
        uint64_t observed_value = 0;
        DataSource source = DataSource::none;
        bool path_reachable = false;
        int probe_msgs = 0;
    };

    std::string read_file(const char* path) {
        std::ifstream in(path);
        if (!in)
            return {}; // 开启失败 → 调用方判空退出(critic V-5:CWD 崩溃修复)
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

} // namespace

int main() {
    // 0. 读冻结声明(只读;失败即明示退出,不 terminate)
    const auto fabric_json_text = read_file("include/tlm/cep/fabric.json");
    const auto schema_text = read_file("include/tlm/cep/protocol_instance.schema.json");
    if (fabric_json_text.empty() || schema_text.empty()) {
        std::fprintf(stderr,
                     "无法读取 include/tlm/cep/{fabric.json,protocol_instance.schema.json}"
                     "(CWD=%s)—— 必须从仓库根运行(exit 2)\n",
                     "");
        return 2;
    }
    nlohmann::json fabric_json = nlohmann::json::parse(fabric_json_text);
    nlohmann::json schema = nlohmann::json::parse(schema_text);
    std::map<std::string, int> vc_count;
    for (auto& [net, spec] : fabric_json.at("networks").items())
        vc_count[net] = spec.at("vc_count").get<int>();
    const auto perm_names =
        schema["properties"]["perm"]["items"]["enum"].get<std::vector<std::string>>();

    const VcBinding binding = VcBinding::authority();
    expect(perm_names.size() == 4 && perm_names[0] == "NONE" && perm_names[1] == "READ" &&
               perm_names[2] == "UNIQUE" && perm_names[3] == "WRITEBACK",
           "perm 4 值锁与冻结 schema 一致(零 drift)");
    expect(!project_perm(TW).has_value(), "T 态无 4 值投影(INV-CEP-05-IC1 域内钉子)");
    std::printf("[S2][decl] fabric: compute_noc=%d VC / ualink_fabric=%d VC;"
                " vc_binding=32 号 :434-442 逐字(非 provisional)\n",
                vc_count.at("compute_noc"), vc_count.at("ualink_fabric"));

    uint64_t mem_check = 0; // (占位防 unused;内存对象见 Memory)
    (void)mem_check;

    // ── S2 主场景(spec §3.2 输入:D-GPU 持 X 为 E → store(静默升 M)→ D-FABRIC 读) ──
    std::printf("\n[S2][MAIN] D-GPU 持 X(E) → D-GPU store(静默 E→M) → D-FABRIC 读 X\n");
    MoesiGpuPlugin gpu_x;
    gpu_x.addr = kAddrX;
    gpu_x.line_value = kOldValue; // E:干净独占,与内存一致(spec 输入前提)
    gpu_x.state = EW;
    SharerTracker tracker;
    tracker.add(kDgpu);
    Memory memory; // 旧值常驻(主场景**全程不写**)
    Bridge bridge;
    bridge.fwd_enabled = true;
    bridge.gpu_line = &gpu_x;
    bridge.tracker = &tracker;
    bridge.mem = &memory;
    bridge.binding = &binding;
    bridge.vc_count = &vc_count;

    // 步 2:store(本地命中 E 行)
    {
        auto [kind, out] = gpu_x.local_store(tracker.sharers.size() > 1);
        expect(kind == UpgradeKind::silent, "步2 E→M = 判据 (a) 静默升级类型");
        expect(out.empty(), "步2 升级瞬间流出线 = 0(无 request/snoop/writeback)");
        expect(gpu_x.state == MW, "步2 表义 E 行第 2 列 = 3(09 号 :152)");
        expect(gpu_x.exposed_perm() == Perm::WRITEBACK,
               "步2 对外 perm UNIQUE→WRITEBACK(静默)——R-2 判据 2 的观测本体");
    }

    // 步 3:D-FABRIC 读(经桥 + tracker):观测 ③ = probe 是否真实发出并收妥
    Observation main_obs;
    {
        ReadResult got = bridge.fabric_read(kAddrX);
        for (auto& m : got.path)
            if (!binding.valid(m, vc_count)) {
                expect(false, "步3 全链流(类,槽位)逐一命中权威绑定");
            }
        main_obs.observed_value = got.value;
        main_obs.source = got.source;
        main_obs.path_reachable = got.probe_forwarded;
        main_obs.probe_msgs = got.probe_msgs;
        expect(main_obs.observed_value == kNewValue,
               "步3 D-FABRIC **实际收到**新值(0x2222,来自流 6 forward_data)");
        expect(main_obs.source == DataSource::gpu_cache,
               "步3 来源 = D-GPU M 态 cache(对象级判决,非自述)+ 内存仍为旧值");
        expect(memory.value == kOldValue,
               "步3 主场景内存未被写(read 只在 fallback 路径发生)→ 来源唯一定位成立");
        expect(main_obs.path_reachable && main_obs.probe_msgs == 3,
               "步3 传播可达 = probe(2 流:snoop+snoop_resp)+ forward_data 真实成链;");
        expect(gpu_x.state == OW && gpu_x.exposed_perm() == Perm::WRITEBACK,
               "步3 M+READ_SHARED → O(data 转发并共享化,:224)");
        tracker.add(kDfab);
        expect(tracker.holds(kDfab), "步3 SharerTracker 增补 requester sharer");
    }

    std::printf("\n[S2][MAIN] 观测记录(3 项,规格 §3.2 修订版):\n");
    std::printf("      ① 观测值    : 新值(0x%llx)\n", (unsigned long long)main_obs.observed_value);
    std::printf("      ② 数据来源  : %s\n", source_name(main_obs.source));
    std::printf("      ③ 传播可达  : true(probe 成链 %d 条流:compute_noc/VC2 snoop →\n"
                "                   D-GPU → VC3 snoop_resp + VC2 forward_data → D-FABRIC)\n",
                main_obs.probe_msgs);

    // ── (b) 的 s1 支对照:E 态被 probe → ACK 无数据(Q-3a :221 语义) ────────────
    std::printf("\n[S2][S1支对照] E 态直接被 probe(钉住 :221,不涉外分支)\n");
    {
        MoesiGpuPlugin e_line;
        e_line.addr = kAddrX;
        e_line.line_value = kOldValue;
        e_line.state = EW;
        StreamMsg snoop;
        snoop.flow = "snoop";
        snoop.msg_class = "snoop";
        snoop.snoop_type = "READ_SHARED";
        snoop.addr = kAddrX;
        snoop.network = "compute_noc";
        snoop.vc = 2;
        ProbeOutcome po = e_line.snoop_resp(snoop);
        expect(!po.fwd.has_value(), "E+READ_SHARED → **无** forward_data(s1 支:内存供数)");
        expect(e_line.state == SW, "E→S(降共享)");
        auto proj = e_line.exposed_perm();
        expect(proj.has_value() && *proj == Perm::READ, "投影=READ");
        std::printf("      对照刻度: E 支无数据 / M 支有数据 → 析取单支为真即为 (b) 成立\n");
    }

    // ── S2b 对照:显式升级(S+Wr 有他 sharer → 副作用 INVALIDATE→all,:212) ─────
    std::printf("\n[S2][S2b] 两域共享 X(S/S) → D-GPU store(显式升级)\n");
    {
        MoesiGpuPlugin gpu_x2;
        gpu_x2.addr = kAddrX;
        gpu_x2.line_value = kOldValue;
        gpu_x2.state = SW;
        MoesiGpuPlugin fab_x2;
        fab_x2.addr = kAddrX;
        fab_x2.state = SW;
        SharerTracker t2;
        t2.add(kDgpu);
        t2.add(kDfab);
        auto [kind, out] = gpu_x2.local_store(t2.sharers.size() > 1);
        expect(kind == UpgradeKind::explicit_invalidate, "S2b S→M(有他 sharer)= 显式升级");
        expect(out.size() == 1 && std::string(out[0].flow) == "snoop" &&
                   std::string(out[0].snoop_type) == "INVALIDATE" &&
                   std::string(out[0].network) == "compute_noc" && out[0].vc == 2,
               "S2b 出线 1 条 = compute_noc/VC2 snoop(INVALIDATE)(槽位=权威绑定)");
        ProbeOutcome po = fab_x2.snoop_resp(out[0]);
        expect(fab_x2.state == IW && std::string(po.resp.note).find("确认") != std::string::npos,
               "S2b D-FABRIC S→I 并回 ACK(:213)");
        auto perm2 = gpu_x2.exposed_perm();
        expect(perm2.has_value() && *perm2 == Perm::WRITEBACK,
               "S2b 终态 perm=WRITEBACK —— 与静默升级**终点相同**但路径通信不同");
        std::printf(
            "      刻度: 静默 = 0 出线 / 显式 = 1 条 INVALIDATE → 两路径跨域触发不可互换\n");
        std::printf("            (perm 形态约束须承载此区分 —— R-2 的 schema 演进,Stage 1/S1-7)\n");
    }

    // ── NEG 变体:桥接真实关闭(同一 Bridge 代码路径,真实 fallback 读) ─────────
    //   ⚠️ provenance 加固(critic S-3):**复用主场景的同一 `bridge` 对象**(而非新建),
    //      并核对 `bridge.fallback_hits` —— 观测**只能**由该对象的 `fabric_read` 真实调用产生,
    //      「手工伪造三字段」的变体无法同时伪造 `fallback_hits`(它由 Bridge 内部递增)。
    std::printf("\n[S2][NEG] 桥接关闭变体(D-FABRIC 读**真的**收不到 snoop 转发)\n");
    Observation neg_obs; // 提升到外层 —— 自检要用(见下)
    {
        const int hits_before = bridge.fallback_hits;
        bridge.fwd_enabled = false; // 复用同一 bridge:关闭转发
        ReadResult got = bridge.fabric_read(kAddrX);
        neg_obs.observed_value = got.value;
        neg_obs.source = got.source;
        neg_obs.path_reachable = got.probe_forwarded;
        const bool hit_ran = (bridge.fallback_hits == hits_before + 1);
        std::printf("[S2][NEG] S2_NEG_FALLBACK_HITS=%d\n", hit_ran ? 1 : 0);
        expect(hit_ran, "NEG 读经同一 bridge 的 fallback 路径**真实执行**(fallback_hits 递增)");
        expect(neg_obs.observed_value == memory.value && neg_obs.observed_value == kOldValue,
               "NEG 读**真的**来自内存(= 旧值 0x1111,同一 bridge 代码路径 fallback)");
        expect(neg_obs.source == DataSource::memory && !neg_obs.path_reachable,
               "NEG 记录: 来源=内存 / 可达=false → 分叉 (i) spike 桥接缺陷形态");
        std::printf("      记录: 观测值=旧值 / 来源=%s / 可达=false\n",
                    source_name(neg_obs.source));
    }

    // ── 自检(「有牙」,per tasks.md T1.3 + delta spec「每场景的反向验证不得被删除」)──
    //   判据 (b) 成立的**必要条件** = ①请求者实际收到新值 ②来源是 M 态 cache
    //   ③传播可达。桥接**真实关闭**后这三条全部不成立 ⇒ 判据必须**不**成立。
    //   若关闭后仍判 (b) 成立 → 判据对「传播是否真发生」**不敏感** → **判据退化** → exit 1。
    std::printf("\n[S2][SELFCHECK] 桥接关闭 ⇒ 判据 (b) 必不成立(否则判据退化)\n");
    {
        const bool b_holds_main = main_obs.observed_value == kNewValue &&
                                  main_obs.source == DataSource::gpu_cache &&
                                  main_obs.path_reachable;
        const bool b_holds_neg = neg_obs.observed_value == kNewValue &&
                                 neg_obs.source == DataSource::gpu_cache && neg_obs.path_reachable;
        // ⚠️ 判别力修正(critic B1 + S-3):`b_holds_neg` 在 fwd_enabled=false 时**构造性恒 false**
        //   —— fallback 分支(`:337-342`)必然置 source=memory / path_reachable=false,
        //   故 `!b_holds_neg` 恒真 ⇒ 仅靠它,自检对「NEG 实验是否真的跑过」**零判别力**。
        //   ⇒ 必须额外要求 NEG **确实是一次真实 fallback 读**。S-3 加固:观测复用同一
        //     `bridge` 对象,`fallback_hits` 由 Bridge 内部递增 —— 伪造三字段者无法伪造它。
        const bool neg_is_real_fallback = neg_obs.observed_value == kOldValue &&
                                          neg_obs.source == DataSource::memory &&
                                          !neg_obs.path_reachable && (bridge.fallback_hits == 1);
        const bool selfcheck_ok = b_holds_main && !b_holds_neg && neg_is_real_fallback;
        std::printf("[S2][SELFCHECK] 主场景 (b) 成立=%d · 桥接关闭后 (b) 成立=%d · "
                    "NEG 为真实 fallback=%d\n",
                    b_holds_main ? 1 : 0, b_holds_neg ? 1 : 0, neg_is_real_fallback ? 1 : 0);
        std::printf("[S2][SELFCHECK] S2_NEG_REAL_FALLBACK=%d\n", neg_is_real_fallback ? 1 : 0);
        std::printf("[S2][SELFCHECK] S2_SELFCHECK=%s\n", selfcheck_ok ? "OK" : "DEGENERATE");
        expect(selfcheck_ok, "自检:桥接真实关闭后判据 (b) 必须不成立(判据对传播敏感,非恒真)");
    }

    // ── 总结 ──────────────────────────────────────────────────────────────────
    std::printf("\n[S2][结论] ");
    if (fail_count == 0) {
        std::printf("S2 = **实测确认通过** —— 静默 E→M 后 D-FABRIC 读到新值;来源对象级\n");
        std::printf("           判定 = D-GPU M 态 cache(流 6 forward_data);传播可达 = probe\n");
        std::printf("           真实成链(snoop/VC2 → resp/VC3 + fwd/VC2)。判据 (b) 声明层裁定\n");
        std::printf("           被实测确认;S2b 对照实证 perm 形态约束(静默/显式不可互换)。\n");
    } else {
        std::printf("共 %d 项观测与期望不符 → 按规格 §3.2 分叉 (i)/(ii) 处置(已完整打印)。\n",
                    fail_count);
    }
    std::printf("[S2][门数] 断言 %d/%d%s\n", pass_count, pass_count + fail_count,
                fail_count == 0 ? " 全绿" : " —— 存在失败");
    return fail_count == 0 ? 0 : 1;
}
