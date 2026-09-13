// Per board-soc-split design §2 + §2.5 thread model (10 约束)
// Owner: CppTLM Team · Date: 2026-08-31
#include "tlm/gpu/dgpu_board_shell.hh"
#include "tlm/gpu/pcie_endpoint_tlm.h"
#include "tlm/gpu/sdma_engine_tlm.hh"  // 1.3d M6: SdmaEngineTLM::kSdmaFenceVector 单点常量引用
// #include "tlm/gpu/pcie_tlp_bundle.hh"  // for PcieTlpBundle construction (deferred T-bs-3b)
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>

namespace tlm::gpu {

    using cpptlm::tlm::DGpuSoc;

    // ── 5 职责实现 ──

    DGpuBoard::DGpuBoard(const std::string& name, EventQueue* eq)
        : name_(name), eq_(std::make_unique<EventQueue>()) // #1 每卡独立,即使外部传 eq 也不复用
    {
        // 若外部传 eq,记录但不直接使用(框架已允许每卡独立 EQ)
        // device_id_ 需从 board_cfg 加载,此处先默认 0
    }

    DGpuBoard::~DGpuBoard() {
        if (!stop_.load()) {
            destroy();
        }
    }

    bool DGpuBoard::load_soc_config(const nlohmann::json& board_cfg) {
        // #3 SOC 装配:实例化 DGpuSoc SimModule 容器
        try {
            if (!soc_) {
                soc_ = std::make_unique<DGpuSoc>(name_ + ".soc", eq_.get());
            }
            // 设备 ID 提取
            if (board_cfg.contains("params") && board_cfg["params"].contains("device_id")) {
                std::string dev_id_str = board_cfg["params"]["device_id"].get<std::string>();
                device_id_ = std::stoul(dev_id_str, nullptr, 0); // 支持 0x 前缀
            }
            // quantum 提取
            if (board_cfg.contains("params") && board_cfg["params"].contains("quantum_cycles")) {
                quantum_cycles_ = board_cfg["params"]["quantum_cycles"].get<uint64_t>();
            }
            // SOC instantiate deferred (T-bs-4 follow-up): GpuCluster 嵌套 instantiateAll 链
            // 中 unique_ptr SIGSEGV, test_cpptlm_emulator_abi.cc 已 deferred 标记.
            if (!board_cfg.contains("modules") || !board_cfg["modules"].is_array() ||
                board_cfg["modules"].empty()) {
                std::lock_guard<std::mutex> lock(inject_mu_);
                last_exception_ = std::make_exception_ptr(
                    std::runtime_error("board_cfg missing 'modules' array"));
                return false;
            }
            const auto& soc_cfg = board_cfg["modules"][0];
            soc_->simulate_instantiate(soc_cfg);
            // Pre-fill device_info_ from pcie_ep params
            for (const auto& mod : board_cfg["modules"][0].value("modules", json::array())) {
                if (mod.value("name", "") == "pcie_ep") {
                    const auto& pcie_params = mod.value("params", json::object());
                    if (pcie_params.contains("bar_sizes") && pcie_params["bar_sizes"].is_array()) {
                        const auto& bars = pcie_params["bar_sizes"];
                        for (size_t i = 0; i < bars.size() && i < 6; ++i) {
                            device_info_.bar_sizes[i] = bars[i].get<uint64_t>();
                        }
                    }
                    device_info_.visible_vram_size =
                        pcie_params.value("visible_vram_size", 256ULL * 1024 * 1024);
                    device_info_.invisible_vram_size =
                        pcie_params.value("invisible_vram_size", 15ULL * 1024 * 1024 * 1024);
                    device_info_.va_region_size = pcie_params.value("va_region_size", 1ULL << 48);
                    device_info_.gpu_id = pcie_params.value("gpu_id", 0U);
                    device_info_.gfx_version =
                        pcie_params.value("gfx_version", static_cast<uint16_t>(1100));
                    device_info_.bdf = pcie_params.value("bdf", static_cast<uint16_t>(0x0008));
                    break;
                }
            }

            // 多卡 StatsManager 前缀:为 SOC 内部组件注册(占位,deferred T-bs-4)
            // 注: StatsManager::register_group 需要 StatGroup* 指针,这里只验证 get_stats_path 接口
            // 实际注册 deferred T-bs-4(JSON 装配)

            return true;
        } catch (...) {
            last_exception_ = std::current_exception(); // #8 异常捕获
            return false;
        }
    }

    bool DGpuBoard::init() {
        if (soc_) {
            soc_->init(); // SimModule 递归 init
        }
        // 启动 sim 线程(每卡独立,per #1)
        if (!sim_thread_.joinable()) {
            stop_ = false;
            sim_thread_ = std::thread(&DGpuBoard::sim_loop, this);
        }
        return true;
    }

    void DGpuBoard::shutdown() {
        destroy();
    }

    // ── C3 (基础任务 1.2.3): MSI-X 中断合并访问器 ──

    void DGpuBoard::set_msix_coalesce_threshold(uint32_t threshold) {
        std::lock_guard<std::mutex> lock(coalesce_mu_);
        msix_coalesce_threshold_ = threshold;
        coalesce_cv_.notify_all();
    }

    void DGpuBoard::set_msix_coalesce_timeout(std::chrono::microseconds timeout) {
        std::lock_guard<std::mutex> lock(coalesce_mu_);
        msix_coalesce_timeout_ = timeout;
        // 若已有 armed 窗口: 重算 deadline 并唤醒 timer (窗口延长/缩短生效)
        if (coalesce_armed_) {
            coalesce_deadline_ = std::chrono::steady_clock::now() + timeout;
        }
        coalesce_cv_.notify_all();
    }

    // ── C3: 合并器核心 (threshold + timeout) ──
    // 设计选择 (spec §2.5.2 授权 "cancel the timer deadline (extend it)" 变体):
    //   每次新投递都延长 deadline (窗口从最近一次投递起算) → 密集 burst 持续推后窗口,
    //   阈值路径 (无 sleep 的紧连调用) 保持确定性: timer 不可能在 burst 中途触发,
    //   唯一 drain 源是同步的阈值 drain。源静默 timeout 后由 timer flush。
    //   阈值 drain 不取消 deadline — timer 醒来见 armed==false 即 no-op (回到等待)。

    void DGpuBoard::coalesce_arm_or_drain(uint32_t vector) {
        uint32_t rep = 0;
        bool fire = false;
        {
            std::lock_guard<std::mutex> lock(coalesce_mu_);
            ++coalesce_count_;
            if (coalesce_count_ == 1) {
                coalesce_rep_vector_ = vector; // 代表 vector = 窗口内首个投递
                coalesce_armed_ = true;
            }
            coalesce_deadline_ = std::chrono::steady_clock::now() + msix_coalesce_timeout_;
            if (!coalesce_timer_.joinable()) {
                coalesce_timer_ = std::thread(&DGpuBoard::coalesce_timer_loop, this);
            }
            if (coalesce_count_ >= msix_coalesce_threshold_) {
                rep = coalesce_rep_vector_;
                coalesce_count_ = 0;
                coalesce_armed_ = false;
                fire = true;
            }
        }
        coalesce_cv_.notify_all();
        if (fire) {
            trigger_irq_async(rep);
        }
    }

    void DGpuBoard::coalesce_timer_loop() {
        std::unique_lock<std::mutex> lock(coalesce_mu_);
        while (!coalesce_stop_.load()) {
            if (!coalesce_armed_) {
                coalesce_cv_.wait(lock,
                                  [this] { return coalesce_stop_.load() || coalesce_armed_; });
                continue;
            }
            auto deadline = coalesce_deadline_;
            if (coalesce_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                if (coalesce_armed_ && coalesce_count_ > 0) {
                    uint32_t rep = coalesce_rep_vector_;
                    coalesce_count_ = 0;
                    coalesce_armed_ = false;
                    lock.unlock(); // trigger 在锁外执行, 不阻塞其他 update_pending
                    trigger_irq_async(rep);
                    lock.lock();
                } else {
                    coalesce_armed_ = false; // 已被阈值 drain 清空 → 仅清 armed
                }
            }
        }
    }

    void DGpuBoard::coalesce_force_flush() {
        // msix_init resize 钩子 (Oracle caveat ①): 合并状态绝不跨 resize 悬空 —
        // 不缓存 MsiXTable::pending_irq_out_ 原始指针; 仅排空任何 armed 窗口
        // (释放累计 pending 计数, 以防丢中断)。
        uint32_t rep = 0;
        bool fire = false;
        {
            std::lock_guard<std::mutex> lock(coalesce_mu_);
            if (coalesce_armed_ && coalesce_count_ > 0) {
                rep = coalesce_rep_vector_;
                coalesce_count_ = 0;
                coalesce_armed_ = false;
                fire = true;
            }
        }
        coalesce_cv_.notify_all(); // timer 若在旧 deadline 上等待 → 见 armed==false → no-op
        if (fire) {
            trigger_irq_async(rep);
        }
    }

    // ── ABI 翻译(占位实现,完整 deferred T-bs-3b) ──

    int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        // null buf 无条件拒绝(避免 memcpy nullptr)(修复 #5)
        if (buf == nullptr) {
            return -EINVAL;
        }
        PendingReq req;
        req.bar = bar;
        req.offset = offset;
        req.data.resize(len); // pre-allocate for response
        req.trans_id = next_trans_id_++;
        req.is_mmio_read = true; // drain 时按读路径回填 pending_data_ (修复 #5)
        auto fut = req.resp.get_future();
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            pending_resp_[req.trans_id] = std::move(fut);
            inject_q_.push_back(std::move(req));
        }
        // 调用线程自 drain 回退(修复 #5 flakiness): 成功路径不依赖 sim 线程被调度 —
        // 重载主机上外部 drain 线程可能 50ms 内未获调度, 由本线程确定性完成自身读请求.
        // 若另一 drain 线程已 swap 走本请求, 此处队列为空, wait 仍由该线程完成.
        if (mmio_self_drain_enabled) {
            drain_injection_queue();
        }
        // #3 关键: kMmioWaitTimeout 超时(防 sim 线程死锁; 50ms 吸收一次 sim-tick 调度延迟, 修复 #5
        // flakiness)
        auto status = pending_resp_[req.trans_id].wait_for(kMmioWaitTimeout);
        if (status != std::future_status::ready) {
            std::lock_guard<std::mutex> lock(inject_mu_);
            pending_resp_.erase(req.trans_id);
            // Oracle Gate E: 超时放弃时同步清理 pending_data_ 残留 — drain 线程可能在超时前已
            // 回填该 trans_id 的 payload (其 future 共享状态仍被 promise 持有, set_value 不抛
            // future_error), 否则 vector 条目永久泄漏. 与下方成功路径的 erase 模式一致.
            pending_data_.erase(req.trans_id);
            return -110; // ETIMEDOUT, buf 不变
        }
        int32_t rc = pending_resp_[req.trans_id].get();
        // 修复 #5: 从 drain 响应 payload 拷贝真实数据到调用方 buf (TODO T-bs-3c 占位 set_value(0)
        // 已真实化)
        std::vector<uint8_t> payload;
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            auto it = pending_data_.find(req.trans_id);
            if (it != pending_data_.end()) {
                payload = std::move(it->second);
                pending_data_.erase(it);
            }
            pending_resp_.erase(req.trans_id);
        }
        if (rc == 0 && !payload.empty()) {
            std::memcpy(buf, payload.data(), std::min(len, payload.size()));
        }
        return rc;
    }

    int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        if (buf == nullptr) {
            return -EINVAL;
        }
        // 修复 #5: 同步存入 BAR-keyed 寄存器映射, 作为 mmio_read roundtrip 的数据源
        std::vector<uint8_t> payload(static_cast<const uint8_t*>(buf),
                                     static_cast<const uint8_t*>(buf) + len);
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            mmio_regs_[std::make_pair(bar, offset)] = payload;
        }
        PendingReq req;
        req.bar = bar;
        req.offset = offset;
        req.data = std::move(payload);
        req.trans_id = next_trans_id_++;
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            inject_q_.push_back(std::move(req));
        }
        return 0; // async, no wait (修复 #7: 保持异步语义)
    }

    int DGpuBoard::pcie_config_read(uint16_t offset, uint8_t width, uint32_t* val) {
        (void)width;
        if (!val)
            return -EINVAL;
        if (!soc_)
            return -ENOSYS;
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep || !ep->has_config_space())
            return -ENOSYS;
        *val = ep->config_space().read(offset);
        return 0;
    }

    int DGpuBoard::pcie_config_write(uint16_t offset, uint8_t width, uint32_t val) {
        (void)width;
        if (!soc_)
            return -ENOSYS;
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep || !ep->has_config_space())
            return -ENOSYS;
        ep->config_space().write(offset, val);
        return 0;
    }

    // ── backdoor ABI(per design §2.5 #5 + ADR-SOC-07 Q3) ──

    int DGpuBoard::backdoor_read(uint64_t vram_offset, void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        // null buf 无条件拒绝(未初始化 board 也返 -EINVAL, 避免 memcpy 到 nullptr)(修复 #6)
        if (buf == nullptr) {
            return -EINVAL;
        }
        // Bounds check仅当 device_info_ 已初始化时生效(bar_sizes[1] > 0);
        // 未初始化的 board(直接构造未调 load_soc_config)走 sync 路径。
        if (device_info_.bar_sizes[1] > 0 &&
            (buf == nullptr || len == 0 || vram_offset >= device_info_.bar_sizes[1] ||
             len > device_info_.bar_sizes[1] - vram_offset)) {
            return -22; // EINVAL
        }
        // 同步从 vram_segments_ 读(SOC deferred,shell 本地处理,不依赖 sim_thread drain)
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            auto it = vram_segments_.find(vram_offset);
            if (it == vram_segments_.end()) {
                return -ENOENT; // miss → -ENOENT, 不再伪装成功返 len (修复 #6)
            }
            if (it->second.size() != len) {
                return -EINVAL;
            }
            std::memcpy(buf, it->second.data(), len);
        }
        return 0;
    }

    int DGpuBoard::backdoor_write(uint64_t vram_offset, const void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        // Bounds check仅当 device_info_ 已初始化时生效(bar_sizes[1] > 0)
        if (device_info_.bar_sizes[1] > 0 &&
            (buf == nullptr || len == 0 || vram_offset >= device_info_.bar_sizes[1] ||
             len > device_info_.bar_sizes[1] - vram_offset)) {
            return -22; // EINVAL
        }
        // 同步存储数据到 VRAM map(SOC deferred,shell 本地存储)
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            vram_segments_[vram_offset] = std::vector<uint8_t>(
                static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
        }
        PendingReq req;
        req.bar = 1;
        req.offset = vram_offset;
        req.data.assign(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
        req.is_backdoor = true;
        req.is_backdoor_read = false; // write
        req.trans_id = next_trans_id_++;
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            inject_q_.push_back(std::move(req));
        }
        return 0; // async
    }

    // ── T-W3-3: msix_* + lookup_register wrappers ──

    int DGpuBoard::msix_init(uint32_t table_size, uint32_t mask) {
        if (table_size > 2048)
            return -22; // EINVAL: PCI-SIG MSI-X 11-bit cap
        if (!soc_)
            return -38; // ENOSYS: SOC not instantiated
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep)
            return -38;
        // C2 (基础任务 1.2.2): resize 先于 init, 使 table_size 真正生效到 MsiXTable。
        // resize(0) 拒绝 (保留 MsiXTable >0 不变式) → -EINVAL
        // C3 (Oracle caveat ①): resize 前 force-flush 任何 armed 合并窗口 —
        // 合并状态绝不跨 resize 悬空 (不缓存 pending_irq_out_ 指针, 仅释放累计计数)。
        coalesce_force_flush();
        if (!ep->msix().resize(static_cast<uint16_t>(table_size)))
            return -22;
        ep->msix().init();
        for (uint32_t v = 0; v < table_size && v < ep->msix().num_vectors(); ++v) {
            if (mask & (1u << v)) {
                ep->msix().set_mask(static_cast<uint16_t>(v), true);
            }
        }
        // C2 修复 (Oracle MEDIUM-2): 运行时 resize 后同步 MSI-X Cap Table Size 字段,
        // 使 host CFG_READ 的 Message Control (bits[31:16]) 跟随最新 vector 数。
        ep->sync_msix_cap_table_size(static_cast<uint16_t>(table_size));
        return 0;
    }

    int DGpuBoard::msix_update_pending(uint32_t vector) {
        if (!soc_)
            return -38;
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep)
            return -38;
        if (ep->msix().update_pending(static_cast<uint16_t>(vector))) {
            // 修复 #4: 中断链接线 — 仅当 IRQ 真正投递 (unmasked, did_deliver) 才触发 host 侧
            // intr_cb; masked vector 仅置 PBA 不入队 (per PCI-SIG MSI-X), 不得触发
            if (ep->msix().did_deliver_last_update()) {
                // C3 (基础任务 1.2.3): 合并开关 — off 时直发 (保留 C1 语义);
                // on 时进 threshold+timeout 合并器 (N 次投递 → 1 次 intr_cb)
                if (msix_coalesce_enabled_.load()) {
                    coalesce_arm_or_drain(vector);
                } else {
                    trigger_irq_async(vector);
                }
            }
            return 0;
        }
        return -22;
    }

    int DGpuBoard::msix_clear_pending(uint32_t vector) {
        if (!soc_)
            return -38;
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep)
            return -38;
        return ep->msix().clear_pending(static_cast<uint16_t>(vector)) ? 0 : -22;
    }

    int DGpuBoard::lookup_register(uint32_t offset, uint32_t* value) {
        if (!value)
            return -22;
        if ((offset & 0x3) != 0)
            return -22; // 4-byte align
        if (offset >= 65536)
            return -22; // BAR0 only
        if (!soc_)
            return -38;
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep)
            return -38;
        const auto* entry = ep->bar_router().lookup(offset);
        if (!entry)
            return -38;
        *value = entry->value;
        return 0;
    }

    const PcieBarRouter::RegisterEntry* DGpuBoard::lookup_register_entry(uint32_t offset) {
        if ((offset & 0x3) != 0)
            return nullptr;
        if (offset >= 65536)
            return nullptr;
        if (!soc_)
            return nullptr;
        auto* ep = dynamic_cast<PcieEndpointTLM*>(soc_->getInternalInstance("pcie_ep"));
        if (!ep)
            return nullptr;
        return ep->bar_router().lookup(offset);
    }

    void DGpuBoard::tick() {
        if (soc_)
            soc_->tick();        // 转发到 SimModule 递归 tick
        drain_injection_queue(); // drain pending backdoor/mmio requests
    }

    // ── 线程模型 #10 destroy 顺序(严格) ──

    void DGpuBoard::destroy() {
        // Step 1: stop_=true
        stop_.store(true);

        // Step 2: 推 poison pill 唤醒 sim 线程
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            PendingReq poison;
            poison.trans_id = UINT64_MAX; // 标记为 poison
            inject_q_.push_back(std::move(poison));
        }

        // Step 3: join sim 线程
        if (sim_thread_.joinable()) {
            sim_thread_.join();
        }

        // Step 3.5 (C3): stop + notify + join 合并 timer 线程 — 必须在析构任一成员之前,
        // 保证 joinable 线程不泄漏、也不在已析构 board 上运行 (确定性析构 per §2.5 #10:
        // stop_ → poison → join → destruct; 此处 stop 顺序: stop_ → cv notify → join)
        coalesce_stop_.store(true);
        coalesce_cv_.notify_all();
        if (coalesce_timer_.joinable()) {
            coalesce_timer_.join();
        }

        // Step 4: 析构 SOC
        soc_.reset();

        // Step 5: 析构 EventQueue
        eq_.reset();
    }

    // ── sim_loop(sim 线程主循环) ──

    void DGpuBoard::sim_loop() {
        // #8 异常经 exception_ptr 跨线程
        try {
            while (!stop_.load()) {
                // #9 idle 检测用 SQ/CQ 计数器,不是 event_queue.empty()
                // TODO T-bs-3b: 真实 quantum 边界 + TickEvent 自续处理
                eq_->run(quantum_cycles_); // per design §2.5 TickEvent 自续
                drain_injection_queue();   // quantum 边界处理 host→sim 注入
            }
        } catch (...) {
            // #8 sim 线程静默吞异常 = 卡死无诊断。必须捕获并存 exception_ptr
            last_exception_ = std::current_exception();
        }
    }

    // ── drain_injection_queue(quantum 边界服务) ──

    void DGpuBoard::drain_injection_queue() {
        std::deque<PendingReq> drained;
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            drained.swap(inject_q_);
        }
        for (auto& req : drained) {
            if (req.trans_id == UINT64_MAX) {
                // poison pill,跳过
                continue;
            }
            if (req.is_backdoor) {
                if (req.is_backdoor_read) {
                    // backdoor read: 从 vram_segments_ 取数据存入 last_backdoor_reads_
                    auto it = vram_segments_.find(req.offset);
                    if (it != vram_segments_.end() && it->second.size() == req.data.size()) {
                        std::lock_guard<std::mutex> lock(inject_mu_);
                        last_backdoor_reads_[req.trans_id] = it->second;
                        try {
                            req.resp.set_value(0);
                        } catch (const std::future_error&) {
                        }
                    } else {
                        // offset 未写入或长度不匹配
                        try {
                            req.resp.set_value(-22);
                        } // EINVAL
                        catch (const std::future_error&) {
                        }
                    }
                } else {
                    // backdoor write: 数据已在 backdoor_write 同步存储到 vram_segments_
                    try {
                        req.resp.set_value(0);
                    } catch (const std::future_error&) {
                    }
                }
            } else if (req.is_mmio_read) {
                // mmio read (修复 #5): 从 mmio_regs_ 取数据存入 pending_data_, 再 set_value
                auto key = std::make_pair(req.bar, req.offset);
                std::vector<uint8_t> payload;
                int32_t rc = 0;
                {
                    std::lock_guard<std::mutex> lock(inject_mu_);
                    auto it = mmio_regs_.find(key);
                    if (it != mmio_regs_.end()) {
                        if (it->second.size() == req.data.size()) {
                            payload = it->second;
                        } else {
                            rc = -EINVAL; // 长度不匹配
                        }
                    }
                    // full miss: rc=0, 未写寄存器按复位值 0 回填(保持既有 mmio_read 语义)
                    if (rc == 0 && payload.empty()) {
                        payload.assign(req.data.size(), 0);
                    }
                    if (rc == 0) {
                        pending_data_[req.trans_id] = std::move(payload);
                    }
                }
                try {
                    req.resp.set_value(rc);
                } catch (const std::future_error&) {
                    // 调用方已超时放弃(future 已销毁): 清理 pending_data_ 防泄漏
                    std::lock_guard<std::mutex> lock(inject_mu_);
                    pending_data_.erase(req.trans_id);
                }
            } else {
                // mmio write (修复 #5): 数据已在 mmio_write 同步存入 mmio_regs_, 无 future 等待
                // (默认构造 promise 无 shared state, 不 set_value)
            }
            // 清理 pending_resp_
            // 注: mmio_read 的 future 由调用方持锁清理,这里不需要重复 erase
        }
    }

    // ── 内部触发接口(供 SOC 组件调用,deferred T-bs-4 装配) ──

    // Stage 1.3d: SDMA Fence → MSI-X vector 0 接线
    //   SdmaEngineTLM::process_fence_queue → board->sdma_fence_complete(fence_id)
    //   → DGpuBoard::msix_update_pending(kSdmaFenceVector=0) → trigger_irq_async(0) → UE intr_cb
    void DGpuBoard::sdma_fence_complete(uint64_t /*fence_id*/) {
        // 单点常量引用 (per Oracle R-D 修订); sdma_engine_tlm.hh 已 include
        msix_update_pending(::tlm::gpu::SdmaEngineTLM::kSdmaFenceVector);
    }

    void DGpuBoard::trigger_irq_async(uint32_t vector_id) {
        IrqCallback cb;
        {
            std::lock_guard<std::mutex> lock(callback_mu_);
            cb = irq_cb_;
        }
        if (cb) {
            // 异步执行:立即返回 sim 线程不被阻塞
            std::thread([cb, vector_id]() {
                try {
                    cb(vector_id);
                } catch (...) {
                    // host 端错误不应反向影响 sim 线程
                }
            }).detach();
        }
    }

    void DGpuBoard::trigger_dma_translate_async(uint64_t iova, size_t size) {
        DmaTranslateCallback cb;
        {
            std::lock_guard<std::mutex> lock(callback_mu_);
            cb = dma_translate_cb_;
        }
        if (cb) {
            std::thread([cb, iova, size]() {
                try {
                    cb(iova, size); // host 返回翻译地址(deferred)
                } catch (...) {
                }
            }).detach();
        }
    }

    void DGpuBoard::trigger_error_async(int err_code, const std::string& msg) {
        ErrorCallback cb;
        {
            std::lock_guard<std::mutex> lock(callback_mu_);
            cb = error_cb_;
        }
        if (cb) {
            std::thread([cb, err_code, msg]() {
                try {
                    cb(err_code, msg);
                } catch (...) {
                }
            }).detach();
        }
    }

} // namespace tlm::gpu