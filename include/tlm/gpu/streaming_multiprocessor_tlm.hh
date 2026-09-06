// include/tlm/gpu/streaming_multiprocessor_tlm.hh
// StreamingMultiprocessorTLM: SM 顶层容器 (gpgpu-sim 风格 SM 微架构)
//
// 功能:
//   - 持有 12 个 ChStream 子模块 (per architecture/15 §15.2 + §15.3)
//   - 实现 IComputeDevice 15 方法 (per architecture/15 §15.5 + HSK-9 §3 + ADR-SOC-16 §2.3)
//   - 11 preserved 方法签名与 IPtxEmuDevice 逐字同构 (含 get_thread_state 返回 ThreadState)
//   - SM-owns-state 模式: RegFileUnit 持寄存器唯一真值源 (per architecture/15 §15.5.6)
//
// 实施状态:
//   - Task 4: SM 顶层 + IComputeDevice 15 方法 stub (本文件 + .cc)
//   - Task 5: 12 个子模块独立 .hh + 空 .cc (per architecture/15 §15.3.3)
//   - Task 7: 12 子模块完整实现 (连接 8 Bundle)
//   - Task 9: GpuComputeUnitTLM rename 合并入此
//   - Task 18: 完整实现 15 方法真实逻辑 (Tick + SM-owns-state 协议)
//
// 作者 CppTLM Team / 日期 2027-02-09 (SM 重构 Task 4 + Task 5 stub)
// 参考: docs/soc_arch/architecture/15-sm-microarchitecture-design.md §15.2 + §15.5
//       docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md
//       docs/soc_arch/adr/hsk9-announcement-draft.md §3
#ifndef TLM_GPU_STREAMING_MULTIPROCESSOR_TLM_HH
#define TLM_GPU_STREAMING_MULTIPROCESSOR_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/i_compute_device.hh"
#include "tlm/gpu/sm/scalar_alu.hh"  // Task 1.3 P1-3 ScalarALU 真值 (独立 cpptlm::gpu::ScalarALU)
#include "bundles/compute_bundles_tlm.hh"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm::sm {

// === 12 个 SM 子模块 stub 定义 (per architecture/15 §15.3) ===
// Task 5: 这些 stub 将拆分到独立 .hh 文件; Task 7: 完整实现

class FetchUnitTLM : public ChStreamModuleBase {
public:
    explicit FetchUnitTLM(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "FetchUnitTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class DecodeUnitTLM : public ChStreamModuleBase {
public:
    explicit DecodeUnitTLM(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "DecodeUnitTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class IssueUnitTLM : public ChStreamModuleBase {
public:
    explicit IssueUnitTLM(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "IssueUnitTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class ScalarALU : public ChStreamModuleBase {
public:
    explicit ScalarALU(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "ScalarALU"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class VectorALU : public ChStreamModuleBase {
public:
    explicit VectorALU(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "VectorALU"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class MatrixCore : public ChStreamModuleBase {
public:
    explicit MatrixCore(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "MatrixCore"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class SIMTLane : public ChStreamModuleBase {
public:
    explicit SIMTLane(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "SIMTLane"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class LsuGlobal : public ChStreamModuleBase {
public:
    explicit LsuGlobal(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "LsuGlobal"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class LsuLDS : public ChStreamModuleBase {
public:
    explicit LsuLDS(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "LsuLDS"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class RegFileUnit : public ChStreamModuleBase {
public:
    explicit RegFileUnit(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "RegFileUnit"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class WritebackUnit : public ChStreamModuleBase {
public:
    explicit WritebackUnit(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "WritebackUnit"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

class HazardTracker : public ChStreamModuleBase {
public:
    explicit HazardTracker(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "HazardTracker"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { (void)a; }
    void tick() override {}
};

}  // namespace tlm::sm

namespace tlm {

class StreamingMultiprocessorTLM : public ChStreamModuleBase,
                                    public cpptlm::gpu::IComputeDevice {
public:
    explicit StreamingMultiprocessorTLM(const std::string& name, EventQueue* eq);
    ~StreamingMultiprocessorTLM() override = default;

    std::string get_module_type() const override { return "StreamingMultiprocessorTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;

    // === 4 端口访问器 (GPUTLM 范式, per include/tlm/gpu/gpu_tlm.hh:177-189) ===
    // StreamAdapter::tick() 契约要求 ModuleT 提供 req_out()/resp_in() (master 方向)
    // + req_in()/resp_out() (slave 方向), 全部 4 方向 (per plan Task 1.1 v2 P0-1 修订)
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>&  req_out()  { return req_out_;  }
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>&   resp_in()  { return resp_in_;  }
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>&    req_in()   { return req_in_;   }
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle>&  resp_out() { return resp_out_; }

    // === get/set_scalar_reg (per Oracle P1-7, Task 1.3 依赖) ===
    // interim 真值源, Task 2.11 须迁移到 RegFileUnit. 见 plan Task 1.1 Step 3.
    void set_scalar_reg(uint32_t reg_id, uint64_t value) { scalar_regs_[reg_id] = value; }
    uint64_t get_scalar_reg(uint32_t reg_id) const {
        auto it = scalar_regs_.find(reg_id);
        return it != scalar_regs_.end() ? it->second : 0;
    }

    // === IComputeDevice 15 方法 stub (Task 18 完整实现) ===
    // 11 preserved
    bool initialize(const cpptlm::gpu::DeviceConfig& cfg) override {
        (void)cfg;
        scalar_regs_.clear();
        internal_buf_.clear();
        return true;
    }
    void shutdown() override {}
    int  exe_once() override {
        if (internal_buf_.empty()) return 0;
        auto& desc = internal_buf_.front();
        if (desc.pipe == cpptlm::gpu::PipeClass::kScalarALU) {
            scalar_alu_->execute(desc);
        }
        internal_buf_.erase(internal_buf_.begin());
        return 1;
    }
    int  sm_exe_once(uint32_t sm_id) override { (void)sm_id; return 0; }
    int  warp_exe_once(uint32_t sm_id, uint32_t warp_id) override { (void)sm_id; (void)warp_id; return 0; }
    bool set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask) override {
        (void)sm_id; (void)warp_id; (void)mask; return false;
    }
    cpptlm::gpu::ThreadState get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) override {
        (void)sm_id; (void)warp_id; (void)lane_id; return cpptlm::gpu::ThreadState::kIdle;
    }
    bool set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) override {
        (void)sm_id; (void)warp_id; (void)mask; return false;
    }
    bool set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) override {
        (void)sm_id; (void)warp_id; (void)lane_id; (void)pc; return false;
    }
    cpptlm::gpu::WarpStatus get_warp_status(uint32_t sm_id, uint32_t warp_id) override {
        (void)sm_id; (void)warp_id; return {};
    }
    bool is_finished() override { return false; }
    // 1 HSK-9 new
    void set_instr_descriptor_buf(const cpptlm::gpu::InstrDescriptor* buf, uint32_t count) override {
        if (!buf || count == 0) return;
        internal_buf_.reserve(internal_buf_.size() + count);
        for (uint32_t i = 0; i < count; ++i) {
            internal_buf_.push_back(buf[i]);  // 浅拷贝 POD copy (per HSK-9 §3 buf 内存所有权)
        }
    }
    // 2 Round 4 new
    bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                             uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) override {
        (void)sm_id; (void)warp_id; (void)lane_id;
        if (!out_value) return false;
        auto it = scalar_regs_.find(reg_id);
        if (it == scalar_regs_.end()) return false;
        *out_value = it->second;
        return true;
    }
    bool is_instruction_completed(uint64_t instr_id) override {
        (void)instr_id; return false;
    }
    // 1 reset
    // Per Oracle P0-2 Task 8 review: IComputeDevice::reset() 与 SimObject::reset(const ResetConfig&)
    // 名字遮蔽. 桥接方案: IComputeDevice::reset() 委托给框架 do_reset(),
    // 并 using SimObject::reset 恢复框架签名.
    void reset() override { do_reset({}); }
    using ChStreamModuleBase::reset;  // 恢复框架 reset(const ResetConfig&) 重载可见性

    // === ChStreamModuleBase tick() ===
    void tick() override;

private:
    // 12 子模块 (per architecture/15 §15.2)
    std::unique_ptr<sm::FetchUnitTLM>    fu_;
    std::unique_ptr<sm::DecodeUnitTLM>    du_;
    std::unique_ptr<sm::IssueUnitTLM>     iu_;
    std::unique_ptr<sm::ScalarALU>        sa_;
    std::unique_ptr<sm::VectorALU>        va_;
    std::unique_ptr<sm::MatrixCore>       mc_;
    std::unique_ptr<sm::SIMTLane>         sl_;
    std::unique_ptr<sm::LsuGlobal>        lg_;
    std::unique_ptr<sm::LsuLDS>           ll_;
    std::unique_ptr<sm::RegFileUnit>      rf_;
    std::unique_ptr<sm::WritebackUnit>    wb_;
    std::unique_ptr<sm::HazardTracker>    ht_;

    // === 4 适配器成员 (GPUTLM 范式, per include/tlm/gpu/gpu_tlm.hh:28-29) ===
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>   req_out_;
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>    resp_in_;
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>     req_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle>  resp_out_;

    // === scalar_regs_ (per Oracle P1-7 Task 1.3 依赖, interim 真值源) ===
    // Task 2.11 须迁移到 RegFileUnit
    std::unordered_map<uint32_t, uint64_t> scalar_regs_;

    // === Task 1.3 P1-3 ScalarALU 真值 (per plan) ===
    // cpptlm::gpu::ScalarALU 独立真值类 (include/tlm/gpu/sm/scalar_alu.hh),
    // SM 顶层持 unique_ptr, exe_once() 调用其 execute() 处理 internal_buf_ 中 kScalarALU 指令
    std::unique_ptr<cpptlm::gpu::ScalarALU> scalar_alu_;
    // 浅拷贝 PTX-EMU 注入的 InstrDescriptor buf (per HSK-9 §3 buf 内存所有权语义)
    std::vector<cpptlm::gpu::InstrDescriptor> internal_buf_;

    cpptlm::StreamAdapterBase* adapter_ = nullptr;
};

}  // namespace tlm

#endif