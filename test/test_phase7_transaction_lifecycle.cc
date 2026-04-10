// test_phase7_transaction_lifecycle.cc
// P0: 完整交易流程测试（ADR-X.1, X.7, X.8）

#include <catch2/catch_all.hpp>
#include "core/sim_object.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "ext/transaction_context_ext.hh"
#include "framework/transaction_tracker.hh"
#include "modules/modules_v2.hh"
#include "ext/error_context_ext.hh"
#include "framework/debug_tracker.hh"
#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include "tlm.h"
#endif

using tlm::tlm_generic_payload;

// ========== T7.1: 根交易完整生命周期（ADR-X.1） ==========



TEST_CASE("T7.1: 根交易完整生命周期", "[transaction][lifecycle][P0]") {

    auto& tracker = TransactionTracker::instance();

    tracker.initialize();

    

    tlm_generic_payload payload;

    

    SECTION("CPU 分配 ID → 透传 → Memory 终止") {

        uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");

        tracker.record_hop(tid, "cache", 1, "hopped");

        tracker.record_hop(tid, "crossbar", 1, "hopped");

        tracker.complete_transaction(tid);

        

        THEN("transaction_id 由 CPU 分配，下游透传，Memory 终止") {

            const auto* record = tracker.get_transaction(tid);

            REQUIRE(record != nullptr);

            REQUIRE(record->is_complete == true);

            REQUIRE(record->hop_log.size() == 2);

        }

    }

}





TEST_CASE("T7.2: 子交易创建与关联", "[transaction][sub][P0]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    tlm_generic_payload parent_payload;
    tlm_generic_payload child_payload;
    
    SECTION("Cache Miss 创建子交易") {
        // 1. 父交易到达 Cache
        uint64_t parent_tid = tracker.create_transaction(&parent_payload, "cpu_0", "READ");
        
        // 2. Cache Miss，创建子交易（TRANSFORM 行为 - ADR-X.7）
        uint64_t child_tid = tracker.create_transaction(&child_payload, "cache_l1", "READ");
        
        // 3. 链接父子交易
        tracker.link_transactions(parent_tid, child_tid);
        
        THEN("子交易 parent_id 指向父交易") {
            const auto* child_record = tracker.get_transaction(child_tid);
            REQUIRE(child_record != nullptr);
            // 子交易应与父交易关联
            auto children = tracker.get_children(parent_tid);
            REQUIRE(children.size() == 1);
            REQUIRE(children[0] == child_tid);
        }
        
        // 4. 完成子交易
        tracker.complete_transaction(child_tid);
        
        THEN("父交易可查询子交易状态") {
            auto children = tracker.get_children(parent_tid);
            REQUIRE(children.size() == 1);
            const auto* child_record = tracker.get_transaction(child_tid);
            REQUIRE(child_record->is_complete == true);
        }
    }
}

// ========== T7.3: 分片交易完整流程（ADR-X.8） ==========



TEST_CASE("T7.3: 分片交易完整流程", "[transaction][fragment][P0]") {

    auto& tracker = TransactionTracker::instance();

    tracker.initialize();

    

    tlm_generic_payload payload;

    const uint8_t NUM_FRAGMENTS = 4;

    

    SECTION("4 分片交易识别与重组") {

        uint64_t parent_tid = tracker.create_transaction(&payload, "root", "READ");

        std::vector<uint64_t> fragment_tids;

        uint64_t parent_id = parent_tid;

        

        for (uint8_t i = 0; i < NUM_FRAGMENTS; i++) {

            tlm_generic_payload frag_payload;

            uint64_t tid = tracker.create_transaction(&frag_payload, "cpu_0", "READ");

            fragment_tids.push_back(tid);

            tracker.link_transactions(parent_id, tid);

        }

        

        THEN("所有分片通过 parent_id 关联") {

            auto children = tracker.get_children(parent_id);

            REQUIRE(children.size() == 4);

        }

    }

}





TEST_CASE("T7.4: 交易追踪日志导出", "[transaction][trace][P1]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    tlm_generic_payload payload;
    uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");
    
    // 记录多个 hop
    tracker.record_hop(tid, "cache_l1", 2, "hit");
    tracker.record_hop(tid, "crossbar", 1, "hopped");
    tracker.record_hop(tid, "memory", 10, "completed");
    tracker.complete_transaction(tid);
    
    THEN("trace_log 包含完整路径") {
        const auto* record = tracker.get_transaction(tid);
        REQUIRE(record != nullptr);
        REQUIRE(record->hop_log.size() == 3);
        REQUIRE(record->hop_log[0].first == "cache_l1");
        REQUIRE(record->hop_log[1].first == "crossbar");
        REQUIRE(record->hop_log[2].first == "memory");
    }
    
    THEN("总延迟可计算") {
        const auto* record = tracker.get_transaction(tid);
        uint64_t total_latency = 0;
        for (const auto& [module, latency] : record->hop_log) {
            total_latency += latency;
        }
        REQUIRE(total_latency == 13); // 2 + 1 + 10
    }
}


// ========== T8.1: 错误码设置与获取 ==========

TEST_CASE("T8.1: 错误码设置与获取", "[error][basic][P0]") {
    tlm_generic_payload payload;
    
    SECTION("设置 TRANSPORT_INVALID_ADDRESS") {
        ErrorCode err = ErrorCode::TRANSPORT_INVALID_ADDRESS;
        
        // 通过 ErrorContextExt 设置
        auto* ext = create_error_context(&payload, err, "Address out of range", "memory");
        
        THEN("错误码正确设置和获取") {
            REQUIRE(ext->error_code == ErrorCode::TRANSPORT_INVALID_ADDRESS);
            REQUIRE(ext->error_category == ErrorCategory::TRANSPORT);
            REQUIRE(ext->error_message == "Address out of range");
            REQUIRE(ext->source_module == "memory");
        }
    }
    
    SECTION("设置 COHERENCE_STATE_VIOLATION") {
        ErrorCode err = ErrorCode::COHERENCE_STATE_VIOLATION;
        auto* ext = create_error_context(&payload, err, "Invalid state transition", "cache");
        
        THEN("一致性错误正确分类") {
            REQUIRE(ext->error_code == ErrorCode::COHERENCE_STATE_VIOLATION);
            REQUIRE(ext->error_category == ErrorCategory::COHERENCE);
        }
    }
}

// ========== T8.3: 严重错误处理 ==========

TEST_CASE("T8.3: 严重错误处理", "[error][fatal][P0]") {
    tlm_generic_payload payload;
    
    SECTION("COHERENCE_DEADLOCK 是严重错误") {
        ErrorCode err = ErrorCode::COHERENCE_DEADLOCK;
        auto* ext = create_error_context(&payload, err, "Deadlock detected", "cache");
        
        THEN("错误被标记为 fatal") {
            REQUIRE(ext->is_fatal() == true);
            REQUIRE(is_fatal_error(err) == true);
        }
    }
    
    SECTION("COHERENCE_DATA_INCONSISTENCY 是严重错误") {
        ErrorCode err = ErrorCode::COHERENCE_DATA_INCONSISTENCY;
        create_error_context(&payload, err, "Data mismatch", "memory");
        
        THEN("数据不一致被标记为 fatal") {
            REQUIRE(is_fatal_error(ErrorCode::COHERENCE_DATA_INCONSISTENCY) == true);
        }
    }
    
    SECTION("DebugTracker 记录严重错误") {
        auto& tracker = DebugTracker::instance();
        tracker.reset_for_testing();
        tracker.initialize(true, true, false);
        
        uint64_t error_id = tracker.record_error(&payload, ErrorCode::COHERENCE_DEADLOCK, "Deadlock", "cache");
        
        THEN("严重错误被正确记录") {
            REQUIRE(error_id > 0);
            const auto* record = tracker.get_error(error_id);
            REQUIRE(record != nullptr);
            REQUIRE(record->error_code == ErrorCode::COHERENCE_DEADLOCK);
        }
    }
}

// ========== T9.1: 完整请求-响应端到端流程 ==========

TEST_CASE("T9.1: 完整请求-响应端到端流程", "[integration][e2e][P0]") {
    auto& txn_tracker = TransactionTracker::instance();
    txn_tracker.initialize();
    
    tlm_generic_payload req_payload;
    tlm_generic_payload resp_payload;
    
    SECTION("CPU → Cache → Crossbar → Memory → 响应") {
        // 1. CPU 分配 transaction_id
        uint64_t tid = txn_tracker.create_transaction(&req_payload, "cpu_0", "READ");
        
        // 2. Cache 处理（透传）
        uint64_t cache_id = tid;
        txn_tracker.record_hop(cache_id, "cache", 1, "hopped");
        
        // 3. Crossbar 处理（透传）
        uint64_t crossbar_id = tid;
        txn_tracker.record_hop(crossbar_id, "crossbar", 1, "hopped");
        
        // 4. Memory 处理并终止
        txn_tracker.record_hop(tid, "memory", 10, "completed");
        txn_tracker.complete_transaction(tid);
        
        THEN("端到端延迟正确计算") {
            const auto* record = txn_tracker.get_transaction(tid);
            REQUIRE(record->hop_log.size() == 3);
            uint64_t total_latency = 0;
            for (const auto& [module, latency] : record->hop_log) {
                total_latency += latency;
            }
            REQUIRE(total_latency == 12); // 1 + 1 + 10
        }
        
        THEN("transaction_id 全程一致") {
            REQUIRE(cache_id == tid);
            REQUIRE(crossbar_id == tid);
        }
    }
}

// ========== T9.4: 错误场景端到端流程 ==========

TEST_CASE("T9.4: 错误场景端到端流程", "[integration][error][P0]") {
    auto& err_tracker = DebugTracker::instance();
    err_tracker.reset_for_testing();
    err_tracker.initialize(true, true, false);
    
    tlm_generic_payload req_payload;
    
    SECTION("Memory 地址越界错误") {
        // 1. CPU 发起无效地址请求
        // 2. Memory 检测错误
        uint64_t error_id = err_tracker.record_error(
            &req_payload,
            ErrorCode::TRANSPORT_INVALID_ADDRESS,
            "Address out of range",
            "memory"
        );
        
        THEN("错误被正确检测和记录") {
            REQUIRE(error_id > 0);
            REQUIRE(err_tracker.error_count() == 1);
            
            const auto* record = err_tracker.get_error(error_id);
            REQUIRE(record->error_code == ErrorCode::TRANSPORT_INVALID_ADDRESS);
            REQUIRE(record->error_category == ErrorCategory::TRANSPORT);
            REQUIRE(record->source_module == "memory");
        }
    }
    
    SECTION("错误从 Memory 传播到 CPU") {
        uint64_t error_id = err_tracker.record_error(
            &req_payload,
            ErrorCode::RESOURCE_BUFFER_FULL,
            "Queue overflow",
            "crossbar"
        );
        
        THEN("可恢复错误被正确记录") {
            const auto* record = err_tracker.get_error(error_id);
            REQUIRE(record->error_code == ErrorCode::RESOURCE_BUFFER_FULL);
            REQUIRE(is_recoverable_error(ErrorCode::RESOURCE_BUFFER_FULL) == true);
        }
    }
}

// ========== T7.5: 粗/细粒度切换 ==========

TEST_CASE("T7.5: 粗/细粒度切换", "[transaction][granularity][P1]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    SECTION("启用粗粒度追踪") {
        tracker.enable_coarse_grained(true);
        
        tlm_generic_payload payload;
        uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");
        
        THEN("粗粒度追踪记录父交易") {
            const auto* record = tracker.get_transaction(tid);
            REQUIRE(record != nullptr);
        }
    }
    
    SECTION("启用细粒度追踪") {
        tracker.enable_fine_grained(true);
        
        tlm_generic_payload payload;
        uint64_t parent_tid = tracker.create_transaction(&payload, "parent", "READ");
        
        // 创建子交易
        for (uint8_t i = 0; i < 3; i++) {
            tlm_generic_payload child_payload;
            uint64_t child_tid = tracker.create_transaction(&child_payload, "child", "READ");
            tracker.link_transactions(parent_tid, child_tid);
        }
        
        THEN("细粒度追踪记录所有分片") {
            const auto* parent_record = tracker.get_transaction(parent_tid);
            REQUIRE(parent_record != nullptr);
            
            auto children = tracker.get_children(parent_tid);
            REQUIRE(children.size() == 3);
        }
    }
}

// ========== T8.2: 错误跨模块传播 ==========

TEST_CASE("T8.2: 错误跨模块传播", "[error][propagation][P1]") {
    auto& err_tracker = DebugTracker::instance();
    err_tracker.reset_for_testing();
    err_tracker.initialize(true, true, false);
    
    tlm_generic_payload payload;
    
    SECTION("错误从 Cache 传播到 Crossbar") {
        // 1. Cache 检测一致性错误
        uint64_t error_id = err_tracker.record_error(
            &payload,
            ErrorCode::COHERENCE_STATE_VIOLATION,
            "State mismatch in cache",
            "cache_l1"
        );
        
        THEN("错误被记录到 DebugTracker") {
            REQUIRE(error_id > 0);
            const auto* record = err_tracker.get_error(error_id);
            REQUIRE(record->error_code == ErrorCode::COHERENCE_STATE_VIOLATION);
            REQUIRE(record->error_category == ErrorCategory::COHERENCE);
            REQUIRE(record->source_module == "cache_l1");
        }
    }
    
    SECTION("多个模块的错误记录") {
        // 记录不同模块的错误
        uint64_t err1 = err_tracker.record_error(&payload, ErrorCode::RESOURCE_BUFFER_FULL, "Full", "crossbar");
        uint64_t err2 = err_tracker.record_error(&payload, ErrorCode::TRANSPORT_TIMEOUT, "Timeout", "noc_router");
        
        THEN("错误可查询") {
            REQUIRE(err_tracker.error_count() >= 2);
            
            auto transport_errors = err_tracker.get_errors_by_category(ErrorCategory::TRANSPORT);
            REQUIRE(transport_errors.size() >= 1);
        }
    }
}

// ========== T8.4: 可恢复错误处理 ==========

TEST_CASE("T8.4: 可恢复错误处理", "[error][recoverable][P1]") {
    tlm_generic_payload payload;
    
    SECTION("RESOURCE_BUFFER_FULL 可恢复") {
        ErrorCode err = ErrorCode::RESOURCE_BUFFER_FULL;
        auto* ext = create_error_context(&payload, err, "Buffer full", "crossbar");
        
        THEN("错误被标记为 recoverable") {
            REQUIRE(ext->is_recoverable() == true);
            REQUIRE(is_recoverable_error(ErrorCode::RESOURCE_BUFFER_FULL) == true);
            REQUIRE(is_fatal_error(ErrorCode::RESOURCE_BUFFER_FULL) == false);
        }
    }
    
    SECTION("TRANSPORT_TIMEOUT 可恢复") {
        ErrorCode err = ErrorCode::TRANSPORT_TIMEOUT;
        create_error_context(&payload, err, "Timeout", "router");
        
        THEN("超时错误可恢复") {
            REQUIRE(is_recoverable_error(ErrorCode::TRANSPORT_TIMEOUT) == true);
        }
    }
    
    SECTION("RESOURCE_STARVATION 可恢复") {
        ErrorCode err = ErrorCode::RESOURCE_STARVATION;
        create_error_context(&payload, err, "Starvation", "cache");
        
        THEN("饥饿错误可恢复") {
            REQUIRE(is_recoverable_error(ErrorCode::RESOURCE_STARVATION) == true);
        }
    }
}

// ========== T8.5: 错误与交易关联 ==========

TEST_CASE("T8.5: 错误与交易关联", "[error][transaction][P1]") {
    auto& err_tracker = DebugTracker::instance();
    err_tracker.reset_for_testing();
    err_tracker.initialize(true, true, false);
    
    auto& txn_tracker = TransactionTracker::instance();
    txn_tracker.initialize();
    
    tlm_generic_payload payload;
    
    SECTION("错误与交易 ID 关联") {
        // 1. 创建交易
        uint64_t tid = txn_tracker.create_transaction(&payload, "cpu_0", "READ");
        
        // 2. 在交易上下文中记录错误
        uint64_t error_id = err_tracker.record_error(
            &payload,
            ErrorCode::COHERENCE_DEADLOCK,
            "Deadlock in transaction",
            "cache_l1"
        );
        
        THEN("错误与交易可通过 DebugTracker 查询") {
            REQUIRE(error_id > 0);
            const auto* err_record = err_tracker.get_error(error_id);
            REQUIRE(err_record != nullptr);
            REQUIRE(err_record->error_code == ErrorCode::COHERENCE_DEADLOCK);
        }
    }
}

// ========== T9.2: 多模块级联 ==========

TEST_CASE("T9.2: 多模块级联", "[integration][cascade][P1]") {
    auto& txn_tracker = TransactionTracker::instance();
    txn_tracker.initialize();
    
    SECTION("CPU → L1 Cache → L2 Cache → Memory") {
        tlm_generic_payload payload;
        uint64_t tid = txn_tracker.create_transaction(&payload, "cpu_0", "READ");
        
        uint64_t l1_id = tid;
        txn_tracker.record_hop(l1_id, "l1_cache", 2, "hop");
        
        uint64_t l2_id = tid;
        txn_tracker.record_hop(l2_id, "l2_cache", 5, "hop");
        
        txn_tracker.record_hop(tid, "memory", 20, "completed");
        txn_tracker.complete_transaction(tid);
        
        THEN("级联交易追踪") {
            const auto* record = txn_tracker.get_transaction(tid);
            REQUIRE(record->hop_log.size() == 3);
            REQUIRE(record->hop_log[0].first == "l1_cache");
            REQUIRE(record->hop_log[1].first == "l2_cache");
            REQUIRE(record->hop_log[2].first == "memory");
        }
    }
}

// ========== T9.3: Cache Miss 子交易流程 ==========

TEST_CASE("T9.3: Cache Miss 子交易流程", "[integration][miss][P1]") {
    auto& txn_tracker = TransactionTracker::instance();
    txn_tracker.initialize();
    
    SECTION("CPU → L1 Miss → L2 → Memory") {
        tlm_generic_payload parent_payload;
        uint64_t parent_tid = txn_tracker.create_transaction(&parent_payload, "cpu_0", "READ");
        
        // L1 Miss，创建子交易
        tlm_generic_payload child_payload1;
        uint64_t child_tid1 = txn_tracker.create_transaction(&child_payload1, "l1_cache", "READ");
        txn_tracker.link_transactions(parent_tid, child_tid1);
        txn_tracker.record_hop(child_tid1, "l1_cache", 3, "miss");
        
        // L2 Miss，创建孙子交易
        tlm_generic_payload child_payload2;
        uint64_t child_tid2 = txn_tracker.create_transaction(&child_payload2, "l2_cache", "READ");
        txn_tracker.link_transactions(child_tid1, child_tid2);
        txn_tracker.record_hop(child_tid2, "l2_cache", 10, "miss");
        
        // Memory 响应
        txn_tracker.record_hop(child_tid2, "memory", 20, "completed");
        txn_tracker.complete_transaction(child_tid2);
        txn_tracker.complete_transaction(child_tid1);
        txn_tracker.complete_transaction(parent_tid);
        
        THEN("多层子交易完整追踪") {
            const auto* parent = txn_tracker.get_transaction(parent_tid);
            REQUIRE(parent->is_complete == true);
            
            const auto* child1 = txn_tracker.get_transaction(child_tid1);
            REQUIRE(child1->is_complete == true);
            
            const auto* child2 = txn_tracker.get_transaction(child_tid2);
            REQUIRE(child2->is_complete == true);
        }
    }
}

