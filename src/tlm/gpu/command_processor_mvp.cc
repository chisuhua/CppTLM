// command_processor_mvp.cc
// CommandProcessorTLM - ChStream 组件(CP 5-state FSM + 4 端口) 实现
// Per board-soc-split design §3.5 ports table + ADR-SOC-07 D1
// Author: CppTLM Team · Date: 2026-08-31 (updated 2026-08-31: promote to ChStreamModuleBase)
//
// s3 T-s3-2: 填充 GPU VA fetch + parse_method + DECODE 实际逻辑 + 退避策略
// 5-state FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE → IDLE
// 4 端口化: cmd_in[0] / fetch_out[1] ⭐新增 / dma_req[2] / dispatch[3]
//
// 关键约束 (per Oracle P1-a 修复):
//   - CP 内部自动从 dispatch_target 返回值触发退避(无需外部调 on_backpressure)
//   - on_backpressure / on_submit_queue_rejected 默认空实现(可选外部通知)
//
// 同样适用 GCC -O2 寄存器复用陷阱(per pm4_decoder_mvp.cc 已发现):
//   5 transition FSM 的 if/switch + 状态寄存器操作需小心 ABI 返回。
//   此处 CP 类成员较多,return 时编译器会拆分成多个寄存器赋值;
//   我们用单一 dispatch 入口 + 单 return 保证所有状态路径一致 ABI 行为。
#include "tlm/gpu/command_processor_mvp.hh"

#include <cstring>

namespace tlm::gpu {

    CommandProcessorTLM::CommandProcessorTLM(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq) {
    }

    void CommandProcessorTLM::set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder) {
        decoder_ = std::move(decoder);
    }

    void CommandProcessorTLM::set_vram_reader(VramReadFn reader) {
        vram_read_cb_ = std::move(reader);
    }

    void CommandProcessorTLM::set_dispatch_target(DispatchFn fn) {
        dispatch_target_ = std::move(fn);
    }

    void CommandProcessorTLM::on_backpressure(uint64_t cycles) {
        // 可选外部通知接口(per Oracle P1-a);默认空实现。
        // CP 已自动从 dispatch_target 返回值退避(见 enter_backoff)。
        (void)cycles;
    }

    void CommandProcessorTLM::on_submit_queue_rejected(uint64_t cycles) {
        // 同 on_backpressure:可选外部通知,默认空实现
        (void)cycles;
    }

    void CommandProcessorTLM::wake() {
        ++wake_count_;
        if (state_ == State::IDLE) {
            transition_to(State::FETCH);
        }
    }

    void CommandProcessorTLM::tick() {
        ++tick_count_;

        switch (state_) {
        case State::IDLE:
            // 等待 wake()
            break;

        case State::FETCH: {
            // 调 vram_read_cb_ 读取 32-bit Pm4MethodHeader (per design §3.1)
            // s3 MVP: 仅读一个 dword;后续 T-s3-3 / Phase F-H.3 扩展读 gpfifo_entry 完整 payload
            uint32_t packed_header = 0;
            int32_t rv = vram_read_cb_ ? vram_read_cb_(0 /* gpu_va, MVP 不维护*/, &packed_header,
                                                       sizeof(packed_header))
                                       : -22;
            if (rv != 0) {
                // VRAM read 失败 → 进 COMPLETE (跳过 DISPATCH)
                transition_to(State::COMPLETE);
                break;
            }
            transition_to(State::DECODE);
            break;
        }

        case State::DECODE: {
            // 调 decoder_->parse_method 解析 Pm4MethodDispatch
            // s3 MVP: payload 暂未消费,仅传 header
            if (!decoder_) {
                transition_to(State::COMPLETE);
                break;
            }
            // 简化:把上一步读取的 header 重新解析。
            // 生产实现会在 FETCH 阶段缓存 packed_header。
            uint32_t packed_header = 0;
            int32_t rv =
                vram_read_cb_ ? vram_read_cb_(0, &packed_header, sizeof(packed_header)) : -22;
            if (rv != 0) {
                transition_to(State::COMPLETE);
                break;
            }
            Pm4MethodDispatch dispatch = decoder_->parse_method(packed_header, nullptr, 0);
            if (dispatch.type == Pm4MethodType::UNKNOWN) {
                // 错误方法 → 跳过 DISPATCH,直接 COMPLETE
                transition_to(State::COMPLETE);
                break;
            }
            transition_to(State::DISPATCH);
            break;
        }

        case State::DISPATCH: {
            // 调 dispatch_target_ 把 Pm4MethodDispatch 适配为 TmuDispatchRecord
            // s3 MVP: 简化适配 - 仅 task_id + method_addr 代理(后续 T-s3-3 填充 CTA fields)
            if (!dispatch_target_) {
                transition_to(State::COMPLETE);
                break;
            }
            TmuDispatchRecord rec{};
            rec.task_id = next_task_id_++;
            TmuSubmitResult result = dispatch_target_(rec);
            if (result == TmuSubmitResult::BACKPRESSURED ||
                result == TmuSubmitResult::SUBMIT_QUEUE_REJECTED) {
                enter_backoff(result);
                // 不进 COMPLETE,回到 FETCH (per §4.4 退避策略)
                transition_to(State::FETCH);
                break;
            }
            transition_to(State::COMPLETE);
            break;
        }

        case State::COMPLETE:
            transition_to(State::IDLE);
            break;
        }
    }

    void CommandProcessorTLM::enter_backoff(TmuSubmitResult dispatch_result) {
        ++cp_backoff_count_;
        backoff_cycles_remaining_ = MIN_BACKOFF_CYCLES;
        (void)dispatch_result;
        if (cp_backoff_count_ >= CP_BACKOFF_DEGRADED_THRESHOLD) {
            degraded_ = true; // latch,不自动恢复,s4 实现
        }
    }

    void CommandProcessorTLM::transition_to(State new_state) {
        state_ = new_state;
        ++state_transitions_;
    }

} // namespace tlm::gpu
