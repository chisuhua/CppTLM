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

#include <memory>
#include <string>

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

    // === IComputeDevice 15 方法 stub (Task 18 完整实现) ===
    // 11 preserved
    bool initialize(const cpptlm::gpu::DeviceConfig& cfg) override { (void)cfg; return false; }
    void shutdown() override {}
    int  exe_once() override { return 0; }
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
        (void)buf; (void)count;
    }
    // 2 Round 4 new
    bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                             uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) override {
        (void)sm_id; (void)warp_id; (void)reg_id; (void)out_value; (void)lane_id; return false;
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

    cpptlm::StreamAdapterBase* adapter_ = nullptr;
};

}  // namespace tlm

#endif