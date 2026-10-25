// include/tlm/cep/stub_cache_core.hh
// Stub Cache Core —— RN 侧协议状态投影(Stub Cache),CEP Stage 1 首个可实现实体
//
// 功能:把 `ADR-CEP-05` 的定义(6 接口 / 5 态状态机 / 双端口形态 / 折叠映射表 / 6 条
//       `INV-SC1` ~ `INV-SC6`)实现为**可执行**的契约。
// 作者: CppTLM Team / 日期: 2026-10-25
//
// ── 定位(per `ADR-CEP-05` §2 v0.3 定位澄清)────────────────────────────
// **不是缓存性能模型**,而是**客户端侧协议状态投影**(RN projection)。只维护协议
// 关心的**稳定状态**;不含 SRAM 阵列 / 多路组相联细节 / 预取器 / 功耗模型 / 流水线时序
// (per `ADR-CEP-05` §4.2「不实现」清单)。
//
// ── 双端口形态(per 决策 2「三明治挂载」)──────────────────────────────
//   Traffic Generator ──(cpu_side)──> Stub Cache ──(net_side)──> ProtocolEngine ──> CohIf
//   · **cpu_side** = 功能操作(读 / 写 / 驱逐);`evict_line()` 是唯一 core-side 原语
//   · **net_side**  = CohIf RN 投影 5 条,命名**逐字对齐** `ADR-CEP-02 §2` canonical 10 流
//   · TG 不知道 CohIf 存在;ProtocolEngine 不知道 TG 存在 ⇒ hit 路径零网络流量**天然正确**
//
// ── 6 接口(per 决策 1,逐字对齐 canonical 10 流)──────────────────────
//   | 方法 | 归属 | 语义 | canonical 流 |
//   |------|------|------|-------------|
//   | `request(addr, perm)` | net-side | 本 stub **发起**缓存请求(返回 txn_id) | 流 1 `request` |
//   | `completion(addr, txn_id, data, perm, error)` | net-side | 本 stub **消费**请求完成 | 流 2 `completion` |
//   | `snoop(addr, type, txn_id)` | net-side | 本 stub **消费** snoop 查询 | 流 3 `snoop` |
//   | `snoop_resp(out)` | net-side | 本 stub **发起** snoop 响应(延迟后可得) | 流 4 `snoop_resp` |
//   | `writeback(addr, data, dirty)` | net-side | 本 stub **发起**写回 | 流 5 `writeback` |
//   | `evict_line(addr)` | **core-side 原语** | 驱逐策略触发(非 CohIf 流投影) | — |
//
// ── 折叠映射表(per 决策 3)—— 5 条隐藏流的**单向**折叠 ─────────────────
//   · canonical 流 6        → **不可分辨**:数据由 `completion` 携带,本 stub 无从区分
//                             「peer 转发」与「内存完成」(真实判定需 ProtocolEngine)
//   · canonical 流 7(反向量)→ 表现为普通 invalidate(走 `snoop(addr, INVALIDATE, …)`)
//   · canonical 流 8        → 在 ProtocolEngine 内终结,不泄漏到本接口
//   · canonical 流 9        → 对 stub **不可见**
//   · canonical 流 10       → 由 ProtocolEngine 内部消化,与本类 `snoop_resp` 配对
//   ⇒ **审计红线**:本文件是折叠表的**单向形态**证据 —— 上述 5 个流名**不得**作为本类的
//      接口标识符出现(由 `test/test_cep_stub_cache_core.cc` 去注释后静态核对)。
//
// ── 集成面(per delta capability `cep-stub-cache-core`)──────────────────
//   · **不**注册 23 ABI(per `ADR-DGPU-08`);**不**经 `ModuleFactory` 注册(本类是
//     **testbench 定位**的组件,由测试夹具装配);**不**改动任何 frozen header。
//   · `net_side` 由 **mock ProtocolEngine** 驱动(本 change 不引入真实协议引擎)。
//
// ── 与 `ADR-CEP-05 §3` 契约测试 #1 / #4 的关系(显式降级)───────────────
//   #1(canonical 流 6 不可分辨性)与 #4(canonical 流 9 不可见)需**真实 ProtocolEngine**
//   (仓内首个协议实例在队列 #6)⇒ **不进**本 change 的 spec,以「触发条件 = 队列 #6」登记
//   (per `openspec/changes/cpptlm-cep-stub-cache-core/design.md` D10 + Open Questions Q2)。
//   本文件只静态保证这两个流名不出现在接口面 —— 那是**必要条件**,不是充分条件。
#ifndef CPPTLM_CEP_STUB_CACHE_CORE_HH
#define CPPTLM_CEP_STUB_CACHE_CORE_HH

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>

namespace tlm::cep {

// ─────────────────────────────────────────────────────────────────────
// 稳定状态(协议无关,per `ADR-CEP-05` §4.1)
// ─────────────────────────────────────────────────────────────────────
//   NONE → I → S → E → M(per MESI)
// **禁止**暴露 CHI RN-F 的 4 个细节态(那 4 个名字归 ProtocolEngine 适配器层,
// per §4.1「禁止」条 + `INV-CEP-05-IC1`「只暴露稳定状态」)。
enum class LineState : uint8_t {
    NONE = 0,  // 不在缓存中
    I = 1,     // Invalid
    S = 2,     // Shared
    E = 3,     // Exclusive
    M = 4,     // Modified
};

// perm 枚举 —— **4 值锁**(per `ADR-CEP-02` CEN-Ib11:不新增 ATOMIC perm)
// atomic 意图作为 `request` 流的**独立属性**承载,不扩 perm。
enum class Perm : uint8_t {
    NONE = 0,       // 无权限(仅占位)
    READ = 1,       // 可读(S)
    UNIQUE = 2,     // 独占干净(E)
    WRITEBACK = 3,  // 独占脏(M)
};

// snoop 类型(per 决策 3:canonical 流 7 的**反向量**表现为普通 invalidate ⇒ 无需独立类型)
enum class SnoopType : uint8_t {
    INVALIDATE = 0,  // 失效
    CLEAN = 1,       // 降至 S(clean)
    FORWARD = 2,     // 取数据但不改状态(cache-to-cache)
};

// snoop 响应 payload(per canonical 流 4 的字段集:`hit + state + dirty + data + txn_id`)
struct SnoopResp {
    uint64_t addr = 0;
    bool hit = false;
    LineState state = LineState::NONE;
    bool dirty = false;
    uint64_t data = 0;
    uint64_t txn_id = 0;
};

// 写回 payload(per canonical 流 5:`addr + data + dirty`)
struct WritebackOut {
    uint64_t addr = 0;
    uint64_t data = 0;
    bool dirty = false;
    /// `INV-SC5` 的**结构**编码:dirty 写回**带** data;clean 写回**不带**。
    /// ⇒ 恒等式 `carries_data == dirty` 是 `INV-SC5` 的可断言形态。
    bool carries_data = false;
    bool valid = false;  // false = 无实际写回(未命中 / 被拒)
};

// 发起请求的结果(per canonical 流 1)
struct RequestOut {
    uint64_t txn_id = 0;  // 0 = 被拒(`INV-SC1`:同 addr 已有 outstanding / MSHR 满)
    uint64_t addr = 0;
    Perm perm = Perm::NONE;
    bool accepted = false;
};

// cpu_side 功能访问结果
struct AccessResult {
    bool hit = false;      // true = 命中(`INV-SC6`:hit 路径零网络流量)
    uint64_t data = 0;
    uint64_t txn_id = 0;   // 仅 miss 时非 0(已发起 net-side request)
    bool pending = false;  // miss 且已发起请求(等待 `completion`)
};

// ─────────────────────────────────────────────────────────────────────
// 可观测计数
// ─────────────────────────────────────────────────────────────────────
// **口径**:本 struct 只收「**保护性拒绝**」与「**实际动作**」两类计数 —— 不收
// 「不可能发生的违例」的死计数器(那种计数器恒 0,是假覆盖;判据:断言须有判别力)。
//   · `INV-SC3`(snoop 固定延迟)/ `INV-SC5`(写回载荷)/ `INV-SC6`(hit 零流量)是
//     **纯行为断言**(轮询序列 / payload 恒等式 / 计数差),不设计数器。
struct StubCounters {
    // cpu_side 动作
    uint64_t cpu_reads = 0;
    uint64_t cpu_writes = 0;
    uint64_t cpu_evicts = 0;
    // net-side **发起**动作
    uint64_t net_requests = 0;       // `INV-SC6` 的观测点(hit 路径不增)
    uint64_t net_snoop_resps = 0;
    uint64_t net_writebacks = 0;
    // net-side **消费**动作
    uint64_t net_completions = 0;
    uint64_t net_snoops = 0;
    // 保护性拒绝(各对应一条 `INV-SC`)
    uint64_t mshr_rejects = 0;             // `INV-SC1`:同 addr 重复 / MSHR 满 → 背压
    uint64_t evict_skipped = 0;            // `INV-SC2`:outstanding line 拒绝驱逐
    uint64_t unsolicited_completions = 0;  // `INV-SC4`:无配对的 completion(唯一真违例)
};

// ─────────────────────────────────────────────────────────────────────
// StubCacheCore —— 客户端侧协议状态投影
// ─────────────────────────────────────────────────────────────────────
class StubCacheCore {
public:
    struct Config {
        size_t max_mshr = 16;               // MSHR 容量(`INV-SC1` 的第二个约束)
        unsigned snoop_latency_cycles = 2;  // `INV-SC3` 的**参数化**固定延迟
    };

    /// 默认配置(延迟 2 拍 / MSHR 16)
    StubCacheCore() = default;
    /// 自定义配置 —— 延迟 / MSHR 容量都是**参数化**的(`INV-SC3` / `INV-SC1`)
    explicit StubCacheCore(const Config& cfg) : cfg_(cfg) {}

    // ═════════════════════════════════════════════════════════════════
    // cpu_side(功能操作,阻塞式)—— TG 只驱这一侧
    // ═════════════════════════════════════════════════════════════════

    /// 读:`hit` → 立即返回(`INV-SC6`:net-side 零流量);`miss` → 分配 MSHR + 发起 request
    AccessResult cpu_read(uint64_t addr) {
        ++ctr_.cpu_reads;
        AccessResult r;
        auto it = lines_.find(addr);
        if (it != lines_.end() && it->second.state != LineState::I) {
            r.hit = true;
            r.data = it->second.data;
            return r;  // INV-SC6:命中路径**不**产生任何 net-side 动作
        }
        const RequestOut ro = request(addr, Perm::READ);
        r.txn_id = ro.txn_id;
        r.pending = ro.accepted;
        return r;
    }

    /// 写:功能写。hit & (E|M) → 就地更新为 M;否则发起带 `WRITEBACK` perm 的 request
    AccessResult cpu_write(uint64_t addr, uint64_t data) {
        ++ctr_.cpu_writes;
        AccessResult r;
        auto it = lines_.find(addr);
        if (it != lines_.end() &&
            (it->second.state == LineState::E || it->second.state == LineState::M)) {
            it->second.state = LineState::M;
            it->second.data = data;
            r.hit = true;
            r.data = data;
            return r;  // INV-SC6
        }
        const RequestOut ro = request(addr, Perm::WRITEBACK);
        r.txn_id = ro.txn_id;
        r.pending = ro.accepted;
        return r;
    }

    /// **core-side 原语**:驱逐策略触发(per 决策 1:唯一非 CohIf 流投影的接口)
    /// `INV-SC2`:MSHR 里有 outstanding 的 line **不可**被驱逐 ⇒ 返回 `valid=false`
    /// 且 **line 保留**(`.second.state` 不变),并计 `evict_skipped`。
    WritebackOut evict_line(uint64_t addr) {
        ++ctr_.cpu_evicts;
        if (is_outstanding(addr)) {
            ++ctr_.evict_skipped;
            return {};  // 跳过:不驱逐、不写回、不改状态
        }
        auto it = lines_.find(addr);
        if (it == lines_.end() || it->second.state == LineState::I)
            return {};  // 无可驱逐内容

        const bool dirty = (it->second.state == LineState::M);
        const uint64_t data = it->second.data;
        lines_.erase(it);
        return writeback(addr, data, dirty);
    }

    // ═════════════════════════════════════════════════════════════════
    // net_side(CohIf RN 投影 5 条,per 决策 1)
    // ═════════════════════════════════════════════════════════════════

    /// 流 1 `request` —— 本 stub **发起**。`INV-SC1`:同 addr 最多 1 个 outstanding。
    RequestOut request(uint64_t addr, Perm perm) {
        RequestOut out;
        out.addr = addr;
        out.perm = perm;
        if (is_outstanding(addr) || mshr_.size() >= cfg_.max_mshr) {
            ++ctr_.mshr_rejects;  // 背压:不分配、不发起
            return out;
        }
        const uint64_t txn = next_txn_id_++;
        mshr_[addr] = txn;  // map 键 = addr ⇒ 同 addr 至多 1 条(`INV-SC1` 的结构保障)
        out.txn_id = txn;
        out.accepted = true;
        ++ctr_.net_requests;
        return out;
    }

    /// 流 2 `completion` —— 本 stub **消费**。`INV-SC4`:无 unsolicited completion。
    /// 数据可用性:无论来自内存还是 peer 转发,**本 stub 无从区分**(折叠表流 6)。
    bool completion(uint64_t addr, uint64_t txn_id, uint64_t data, Perm perm, bool error) {
        auto it = mshr_.find(addr);
        if (it == mshr_.end() || it->second != txn_id) {
            ++ctr_.unsolicited_completions;  // INV-SC4:无配对 → 拒绝
            return false;
        }
        mshr_.erase(it);
        ++ctr_.net_completions;
        lines_[addr] = CachedLine{error ? LineState::I : state_for_perm(perm), data};
        return true;
    }

    /// 流 3 `snoop` —— 本 stub **消费**。`INV-SC3`:响应在**参数化固定延迟**后可得。
    bool snoop(uint64_t addr, SnoopType type, uint64_t txn_id) {
        ++ctr_.net_snoops;
        SnoopResp resp;
        resp.addr = addr;
        resp.txn_id = txn_id;
        auto it = lines_.find(addr);
        if (it != lines_.end() && it->second.state != LineState::I) {
            resp.hit = true;
            resp.state = it->second.state;
            resp.dirty = (it->second.state == LineState::M);
            resp.data = it->second.data;
            switch (type) {
                case SnoopType::INVALIDATE:
                    it->second.state = LineState::I;
                    if (resp.dirty) writeback(addr, resp.data, true);  // 隐式写回(带 data)
                    break;
                case SnoopType::CLEAN:
                    it->second.state = LineState::S;
                    if (resp.dirty) writeback(addr, resp.data, true);
                    break;
                case SnoopType::FORWARD:
                    break;  // 状态不变(cache-to-cache 供数)
            }
        }
        pending_resps_.push_back(PendingResp{resp, cycle_ + cfg_.snoop_latency_cycles});
        return true;
    }

    /// 流 4 `snoop_resp` —— 本 stub **发起**。到期前**不**交付(非阻塞轮询的合法结果)。
    bool snoop_resp(SnoopResp& out) {
        if (pending_resps_.empty())
            return false;
        if (pending_resps_.front().ready_cycle > cycle_)
            return false;  // 未到期:INV-SC3 的延迟保证
        out = pending_resps_.front().resp;
        pending_resps_.pop_front();
        ++ctr_.net_snoop_resps;
        return true;
    }

    /// 流 5 `writeback` —— 本 stub **发起**。`INV-SC5`:dirty 带 data;clean 不带。
    WritebackOut writeback(uint64_t addr, uint64_t data, bool dirty) {
        WritebackOut out;
        out.addr = addr;
        out.data = dirty ? data : 0;  // clean 写回**不带** data
        out.dirty = dirty;
        out.carries_data = dirty;  // 恒等式:carries_data == dirty
        out.valid = true;
        ++ctr_.net_writebacks;
        return out;
    }

    // ═════════════════════════════════════════════════════════════════
    // 时钟与观测
    // ═════════════════════════════════════════════════════════════════

    /// 推进内部周期(仅驱动 `INV-SC3` 的参数化延迟;无时序模型 —— 见「定位」)
    void tick(unsigned n = 1) { cycle_ += n; }
    uint64_t cycle() const { return cycle_; }

    LineState state_of(uint64_t addr) const {
        auto it = lines_.find(addr);
        return (it == lines_.end()) ? LineState::NONE : it->second.state;
    }
    bool has_line(uint64_t addr) const { return lines_.count(addr) != 0; }
    size_t outstanding_count() const { return mshr_.size(); }
    bool is_outstanding(uint64_t addr) const { return mshr_.count(addr) != 0; }
    uint64_t outstanding_txn(uint64_t addr) const {
        auto it = mshr_.find(addr);
        return (it == mshr_.end()) ? 0 : it->second;
    }
    size_t pending_snoop_resp_count() const { return pending_resps_.size(); }
    const StubCounters& counters() const { return ctr_; }
    const Config& config() const { return cfg_; }

    /// 状态集规模(canonical:恰 5 态)—— 供「5 态可达」Scenario 核对
    static constexpr size_t kStateCount = 5;

private:
    struct CachedLine {
        LineState state = LineState::NONE;
        uint64_t data = 0;
    };
    struct PendingResp {
        SnoopResp resp;
        uint64_t ready_cycle = 0;
    };

    static LineState state_for_perm(Perm p) {
        switch (p) {
            case Perm::READ: return LineState::S;
            case Perm::UNIQUE: return LineState::E;
            case Perm::WRITEBACK: return LineState::M;
            case Perm::NONE: return LineState::I;
        }
        return LineState::NONE;
    }

    Config cfg_;
    std::map<uint64_t, CachedLine> lines_;
    std::map<uint64_t, uint64_t> mshr_;  // addr → txn_id(`INV-SC1` 守卫:map ⇒ 同 addr 至多 1)
    std::deque<PendingResp> pending_resps_;
    uint64_t next_txn_id_ = 1;
    uint64_t cycle_ = 0;
    StubCounters ctr_;
};

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_STUB_CACHE_CORE_HH
