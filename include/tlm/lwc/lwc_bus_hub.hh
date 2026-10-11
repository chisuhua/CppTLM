// include/tlm/lwc/lwc_bus_hub.hh
// LwcBusHub: LightweightInterconnect (LWC) Bus Hub — 树形/星形轻量级总线中心
// 修复 Gap H (CR-Bus 寄存器访问) + Gap J (Doorbell 直调) + Gap K (MMIO Read AR/R)
//
// 设计 (per cpptlm-lwc-bus-hub-mvp design.md §1 + §4, v1.0.1 B-B1/B-H2 修订):
//   - ChStreamModuleBase 派生 (per B-B1: 必须走 registerObject/REGISTER_CHSTREAM, 非 REGISTER_MODULE)
//   - 1 Hub + 4-32 IP port, 树形/星形 (与 SocInternalNoc mesh 正交分层, §0.1.1 Hard Invariant)
//   - 异构 Bundle 路由: cr_targets_ (CrBundle) + db_targets_ (DoorbellBundle) 双 map
//     (per B-H2: 删除 variadic MultiPortHeterogeneousBundleAdapter, 用双指针 map)
//   - 同步 handler 路由 (host 线程直调): cr_write/doorbell_write 立即 push 到 target adapter
//     (测试期望 mmio_write 后 GMMU/SDMA 同步生效; 跨 host↔sim 线程安全由 DGpuBoard mmio_*
//     mutex 保证, per B-B2 修订为同步路径)
//   - cr_read(): 返回 std::future<CrBundle>, sim 侧经 complete_cr_read() fulfill
//     (per B-H3: class sketch 增 cr_read 方法签名)
//   - 32-port 上限 (fail soft: register_*_target 返回 false, 不抛)
// 作者: CppTLM Team · 日期: 2026-10-09
#ifndef TLM_LWC_LWC_BUS_HUB_HH
#define TLM_LWC_LWC_BUS_HUB_HH

#include "bundles/cr_bundles_tlm.hh"
#include "bundles/doorbell_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"

#include <chrono>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace tlm::lwc {

/**
 * @brief LWC Bus Hub — 配置/中断/提交等非数据流量的轻量级总线中心
 *
 * 真实物理 IP 对应: APB-lite (CR-Bus) / Posted Write Bus (DoorbellBus).
 * 与 SocInternalNoc (mesh, 数据流量) 正交分层.
 */
class LwcBusHub : public ChStreamModuleBase {
public:
    static constexpr unsigned kMaxIpPorts = 32;

    explicit LwcBusHub(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq) {}
    ~LwcBusHub() override = default;

    std::string get_module_type() const override { return "LwcBusHub"; }

    // ChStreamModuleBase 空实现 (hub 无 StreamAdapter 端口; 路由走同步 handler)
    void set_stream_adapter(cpptlm::StreamAdapterBase* /*a*/) override {}
    void tick() override {}

    // ── IP port 注册 (per design §1) ──
    /** @brief 注册 CR target (CrBundle), 返回 false 当 32-port 上限 */
    bool register_cr_target(const std::string& ip_name,
                            cpptlm::InputStreamAdapter<bundles::CrBundle>* cr_in) {
        if (cr_in == nullptr) return false;
        if (cr_targets_.size() + db_targets_.size() >= kMaxIpPorts) return false;
        cr_targets_[ip_name] = cr_in;
        if (first_cr_target_ == nullptr) first_cr_target_ = cr_in;
        return true;
    }

    /** @brief 注册 Doorbell target (DoorbellBundle), 返回 false 当 32-port 上限 */
    bool register_doorbell_target(const std::string& ip_name,
                                  cpptlm::InputStreamAdapter<bundles::DoorbellBundle>* db_in) {
        if (db_in == nullptr) return false;
        if (cr_targets_.size() + db_targets_.size() >= kMaxIpPorts) return false;
        db_targets_[ip_name] = db_in;
        if (first_db_target_ == nullptr) first_db_target_ = db_in;
        return true;
    }

    // ── Hub 主入口: CR-Bus write (host 线程, 同步路由) ──
    void cr_write(const bundles::CrBundle& tx) {
        std::lock_guard<std::mutex> lock(route_mu_);
        if (first_cr_target_ == nullptr) {
            ++dropped_count_;
            return;
        }
        // posted write 覆盖语义: 无条件覆盖 data + set_valid (不检旧 valid)
        first_cr_target_->data() = tx;
        first_cr_target_->set_valid(true);
        ++routed_count_;
    }

    // ── Hub 主入口: DoorbellBus write (host 线程, 同步路由) ──
    void doorbell_write(const bundles::DoorbellBundle& tx) {
        std::lock_guard<std::mutex> lock(route_mu_);
        if (first_db_target_ == nullptr) {
            ++dropped_count_;
            return;
        }
        first_db_target_->data() = tx;
        first_db_target_->set_valid(true);
        ++routed_count_;
    }

    // ── CR-Bus 同步读 (mmio_read 路径, per B-H3 + design §3) ──
    /** @brief 发起同步 CR read, 返回 future (sim 侧 complete_cr_read fulfill) */
    std::future<bundles::CrBundle> cr_read(const bundles::CrBundle& tx) {
        std::lock_guard<std::mutex> lock(route_mu_);
        if (first_cr_target_ == nullptr) {
            // 无 target: 立即返 error (DECERR)
            std::promise<bundles::CrBundle> failed;
            bundles::CrBundle resp = tx;
            resp.error.write(bundles::CrBundle::ERR_DECERR);
            failed.set_value(resp);
            ++dropped_count_;
            return failed.get_future();
        }
        if (first_cr_target_->valid()) {
            // target 忙: 立即返 error (SLVERR)
            std::promise<bundles::CrBundle> busy;
            bundles::CrBundle resp = tx;
            resp.error.write(bundles::CrBundle::ERR_SLVERR);
            busy.set_value(resp);
            ++dropped_count_;
            return busy.get_future();
        }
        first_cr_target_->data() = tx;
        first_cr_target_->set_valid(true);
        ++routed_count_;
        pending_cr_reads_[tx.transaction_id.read()] = std::promise<bundles::CrBundle>();
        return pending_cr_reads_[tx.transaction_id.read()].get_future();
    }

    /** @brief sim 侧完成 CR read (GMMU tick 处理后回填 rdata/ready/error) */
    void complete_cr_read(const bundles::CrBundle& resp) {
        std::lock_guard<std::mutex> lock(route_mu_);
        auto it = pending_cr_reads_.find(resp.transaction_id.read());
        if (it != pending_cr_reads_.end()) {
            try {
                it->second.set_value(resp);
            } catch (const std::future_error&) {
                // future 已被调用方销毁 (超时放弃)
            }
            pending_cr_reads_.erase(it);
        }
    }

    // ── 测试钩子 ──
    uint64_t routed_count() const noexcept { return routed_count_; }
    uint64_t dropped_count() const noexcept { return dropped_count_; }
    size_t cr_target_count() const noexcept { return cr_targets_.size(); }
    size_t db_target_count() const noexcept { return db_targets_.size(); }

private:
    mutable std::mutex route_mu_;
    std::unordered_map<std::string, cpptlm::InputStreamAdapter<bundles::CrBundle>*> cr_targets_;
    std::unordered_map<std::string, cpptlm::InputStreamAdapter<bundles::DoorbellBundle>*>
        db_targets_;
    cpptlm::InputStreamAdapter<bundles::CrBundle>* first_cr_target_ = nullptr;
    cpptlm::InputStreamAdapter<bundles::DoorbellBundle>* first_db_target_ = nullptr;
    std::unordered_map<uint8_t, std::promise<bundles::CrBundle>> pending_cr_reads_;
    uint64_t routed_count_ = 0;
    uint64_t dropped_count_ = 0;
};

}  // namespace tlm::lwc

#endif  // TLM_LWC_LWC_BUS_HUB_HH
