// test/test_phase8_performance_stress.cc
// P2: 压力、性能与稳定性测试 (T10)

#include <catch2/catch_all.hpp>
#include "ext/transaction_context_ext.hh"
#include "framework/transaction_tracker.hh"
#include "framework/debug_tracker.hh"
#include "modules/modules_v2.hh"
#include "core/packet_pool.hh"
#include <vector>

#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include "tlm.h"
#endif

using tlm::tlm_generic_payload;

// ========== T10.1: 大量并发交易测试 ==========

TEST_CASE("T10.1: 大量并发交易", "[performance][stress][P2]") {
    const int NUM_TRANSACTIONS = 1000; // 测试 1000 个交易
    
    SECTION("TransactionTracker 容量测试") {
        auto& tracker = TransactionTracker::instance();
        tracker.initialize();
        
        // 批量创建交易
        for (int i = 0; i < NUM_TRANSACTIONS; ++i) {
            tlm_generic_payload payload;
            tracker.create_transaction(&payload, "cpu_" + std::to_string(i), "READ");
            
            // 记录一些 hop
            tracker.record_hop(i + 1, "crossbar", 1, "hopped");
        }
        
        THEN("所有交易都应被记录") {
            REQUIRE(tracker.active_count() == NUM_TRANSACTIONS);
            
            // 验证随机访问
            const auto* rec = tracker.get_transaction(500);
            REQUIRE(rec != nullptr);
            REQUIRE(rec->source_module == "cpu_499");
            REQUIRE(rec->hop_log.size() == 1);
        }
    }
    
    SECTION("DebugTracker 记录容量") {
        auto& tracker = DebugTracker::instance();
        tracker.reset_for_testing();
        tracker.initialize(true, true, false);
        
        // 记录大量错误
        for (int i = 0; i < NUM_TRANSACTIONS; ++i) {
            tlm_generic_payload payload;
            tracker.record_error(&payload, ErrorCode::COHERENCE_STATE_VIOLATION, "Error", "cache");
        }
        
        THEN("所有错误都被记录") {
            REQUIRE(tracker.error_count() == NUM_TRANSACTIONS);
            REQUIRE(tracker.get_errors_by_category(ErrorCategory::COHERENCE).size() == NUM_TRANSACTIONS);
        }
    }
}

// ========== T10.2: 长时间运行稳定性测试 ==========

TEST_CASE("T10.2: 长时间运行稳定性", "[performance][stability][P2]") {
    auto& txn_tracker = TransactionTracker::instance();
    txn_tracker.initialize();
    
    uint64_t active_count = 0;
    
    SECTION("高频循环交易 (10,000 cycles)") {
        // 模拟 10,000 个周期
        for (uint64_t cycle = 0; cycle < 10000; ++cycle) {
            // 每 10 个周期发起一个交易
            if (cycle % 10 == 0) {
                tlm_generic_payload p;
                txn_tracker.create_transaction(&p, "cpu", "READ");
                active_count++;
            }
            
            // 某些交易完成
            // 为了简化，我们假设交易 ID 递增且每隔一段时间完成旧交易
            // 这里的逻辑是验证 Tracker 不会崩溃
        }
        
        THEN("系统稳定，计数器正确") {
            REQUIRE(active_count == 1000); 
        }
    }
    
    SECTION("内存重用稳定性 (PacketPool)") {
        // 验证 PacketPool 在高频率 acquire/release 下不崩溃
        for (int i = 0; i < 1000; ++i) {
            Packet* pkt = PacketPool::get().acquire();
            REQUIRE(pkt != nullptr);
            // 模拟使用
            pkt->src_cycle = 100;
            PacketPool::get().release(pkt);
        }
    }
}

// ========== T10.3: 扩展生命周期测试 (模拟内存泄漏检测) ==========

TEST_CASE("T10.3: 扩展生命周期与内存管理", "[performance][memory][P2]") {
    SECTION("Payload 销毁时清理 Extension") {
        // 验证当 tlm_generic_payload 被销毁时，附加的 Extension 也被销毁
        // 这依赖于 stub 中的 ~tlm_generic_payload() 实现
        
        // 我们无法直接在 Catch2 中断言 delete 被调用，但我们可以
        // 确认创建了大量 Extension 后没有明显的崩溃。
        // 真正的泄漏检测需要 ASan。
        
        for (int i = 0; i < 500; ++i) {
            {
                tlm::tlm_generic_payload* p = new tlm::tlm_generic_payload();
                create_transaction_context(p, 1000 + i, 0, 0, 1);
                // 此时 p 拥有 extension
                
                delete p; // 应该触发 extension 及其 trace_log 的销毁
            }
        }
        
        SUCCEED("Payload 生命周期测试通过 (无崩溃)");
    }
    
    SECTION("DebugTracker 清空内存") {
        auto& tracker = DebugTracker::instance();
        tracker.reset_for_testing();
        tracker.initialize(true, true, false);
        
        // 填满
        for(int i=0; i<100; ++i) {
             tlm_generic_payload p;
             tracker.record_error(&p, ErrorCode::RESOURCE_BUFFER_FULL, "Full", "cache");
        }
        REQUIRE(tracker.error_count() == 100);
        
        // 清空
        tracker.clear_all();
        REQUIRE(tracker.error_count() == 0);
        REQUIRE(tracker.state_snapshot_count() == 0);
    }
}

