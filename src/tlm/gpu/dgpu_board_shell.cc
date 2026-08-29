// Per board-soc-split design §2 + §2.5 thread model (10 约束)
// Owner: CppTLM Team · Date: 2026-08-31
#include "tlm/gpu/dgpu_board_shell.hh"
// #include "tlm/gpu/pcie_tlp_bundle.hh"  // for PcieTlpBundle construction (deferred T-bs-3b)
#include <chrono>
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
            // SOC 内部组件实例化
            soc_->simulate_instantiate(board_cfg);

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

    // ── ABI 翻译(占位实现,完整 deferred T-bs-3b) ──

    int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        PendingReq req;
        req.bar = bar;
        req.offset = offset;
        req.data.resize(len); // pre-allocate for response
        req.trans_id = next_trans_id_++;
        auto fut = req.resp.get_future();
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            pending_resp_[req.trans_id] = std::move(fut);
            inject_q_.push_back(std::move(req));
        }
        // #3 关键: 1ms 超时(防 sim 线程死锁)
        auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(1));
        if (status != std::future_status::ready) {
            std::lock_guard<std::mutex> lock(inject_mu_);
            pending_resp_.erase(req.trans_id);
            return -110; // ETIMEDOUT
        }
        int32_t rc = pending_resp_[req.trans_id].get();
        // TODO T-bs-3c: copy resp data to buf (per design §2.5 同步等待)
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_.erase(req.trans_id);
        return rc;
    }

    int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        PendingReq req;
        req.bar = bar;
        req.offset = offset;
        req.data.assign(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
        req.trans_id = next_trans_id_++;
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            inject_q_.push_back(std::move(req));
        }
        return 0; // async, no wait
    }

    int DGpuBoard::pcie_config_read(uint16_t offset, uint8_t width, uint32_t* val) {
        // TODO T-bs-3b
        return -ENOSYS;
    }

    int DGpuBoard::pcie_config_write(uint16_t offset, uint8_t width, uint32_t val) {
        return -ENOSYS;
    }

    // ── backdoor ABI(per design §2.5 #5 + ADR-SOC-07 Q3) ──

    int DGpuBoard::backdoor_read(uint64_t vram_offset, void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        PendingReq req;
        req.bar = 1; // BAR1 标记(BAR1 MEM 块, per ADR-SOC-07 Q3)
        req.offset = vram_offset;
        req.data.resize(len);   // backdoor 输出填充
        req.is_backdoor = true; // ⭐标识 backdoor 路径
        req.trans_id = next_trans_id_++;
        auto fut = req.resp.get_future();
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            pending_resp_[req.trans_id] = std::move(fut);
            inject_q_.push_back(std::move(req));
        }
        // #3 1ms 超时(同 mmio_read)
        auto status = pending_resp_[req.trans_id].wait_for(std::chrono::milliseconds(1));
        if (status != std::future_status::ready) {
            std::lock_guard<std::mutex> lock(inject_mu_);
            pending_resp_.erase(req.trans_id);
            return -110; // ETIMEDOUT
        }
        int32_t rc = pending_resp_[req.trans_id].get();
        // 复制 resp 数据到 buf(若成功)
        if (rc >= 0 && static_cast<size_t>(rc) <= len) {
            std::lock_guard<std::mutex> lock(inject_mu_);
            // 从 inject_q_ 或临时存储取回数据(本任务占位)
            // 占位:不复制(返回 rc 表示已读字节数)
        }
        std::lock_guard<std::mutex> lock(inject_mu_);
        pending_resp_.erase(req.trans_id);
        return rc;
    }

    int DGpuBoard::backdoor_write(uint64_t vram_offset, const void* buf, size_t len) {
        if (last_exception_) {
            std::rethrow_exception(last_exception_); // #8 异常传递
        }
        PendingReq req;
        req.bar = 1;
        req.offset = vram_offset;
        req.data.assign(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + len);
        req.is_backdoor = true;
        req.trans_id = next_trans_id_++;
        {
            std::lock_guard<std::mutex> lock(inject_mu_);
            inject_q_.push_back(std::move(req));
        }
        return 0; // async
    }

    void DGpuBoard::tick() {
        if (soc_)
            soc_->tick(); // 转发到 SimModule 递归 tick
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
                // backdoor 路径:直接访问 VRAM(本任务占位)
                // TODO T-bs-4: 真实访问 vram (经 soc_->getInternalInputPort("vram.0"))
                try {
                    req.resp.set_value(static_cast<int32_t>(req.data.size()));
                } catch (const std::future_error&) {
                }
            } else {
                // mmio 路径(W6b)
                // TODO T-bs-3c: 构造 PcieTlpBundle 注入
                // soc_->getInternalInputPort("pcie_ep.slave_in") 占位: 立即 set_value 0(success) -
                // 让 mmio_read 至少能响应
                try {
                    req.resp.set_value(0);
                } catch (const std::future_error&) {
                }
            }
            // 清理 pending_resp_
            // 注: mmio_read 的 future 由调用方持锁清理,这里不需要重复 erase
        }
    }

    // ── 内部触发接口(供 SOC 组件调用,deferred T-bs-4 装配) ──

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