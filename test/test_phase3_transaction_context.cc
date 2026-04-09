// test/test_phase3_transaction_context.cc
// Phase 3: TransactionContext 测试（测试先行）

#include <catch2/catch_all.hpp>
#include "ext/transaction_context_ext.hh"
#include "tlm.h"

using tlm::tlm_generic_payload;

TEST_CASE("TransactionContextExt 基础构造", "[transaction][ext]") {
    TransactionContextExt ext;
    
    SECTION("默认值") {
        REQUIRE(ext.transaction_id == 0);
        REQUIRE(ext.parent_id == 0);
        REQUIRE(ext.fragment_id == 0);
        REQUIRE(ext.fragment_total == 1);
        REQUIRE(ext.is_root() == true);
        REQUIRE(ext.is_fragmented() == false);
    }
    
    SECTION("字段赋值") {
        ext.transaction_id = 100;
        ext.parent_id = 0;
        ext.source_module = "cpu_0";
        ext.type = "READ";
        
        REQUIRE(ext.transaction_id == 100);
        REQUIRE(ext.source_module == "cpu_0");
        REQUIRE(ext.type == "READ");
    }
}

TEST_CASE("TransactionContextExt clone 方法", "[transaction][ext]") {
    TransactionContextExt original;
    original.transaction_id = 100;
    original.parent_id = 0;
    original.fragment_id = 0;
    original.fragment_total = 1;
    original.source_module = "test_module";
    original.type = "WRITE";
    original.priority = 5;
    original.add_trace("module1", 10, 2, "hopped");
    
    tlm::tlm_extension* cloned_ptr = original.clone();
    
    REQUIRE(cloned_ptr != nullptr);
    
    auto* cloned = dynamic_cast<TransactionContextExt*>(cloned_ptr);
    REQUIRE(cloned != nullptr);
    REQUIRE(cloned->transaction_id == 100);
    REQUIRE(cloned->source_module == "test_module");
    REQUIRE(cloned->type == "WRITE");
    REQUIRE(cloned->trace_log.size() == 1);
    
    // 验证深拷贝
    cloned->trace_log[0].module = "modified";
    REQUIRE(original.trace_log[0].module == "module1");
    
    delete cloned_ptr;
}

TEST_CASE("TransactionContextExt copy_from 方法", "[transaction][ext]") {
    TransactionContextExt source;
    source.transaction_id = 200;
    source.parent_id = 100;
    source.fragment_id = 1;
    source.fragment_total = 4;
    source.source_module = "cache";
    
    TransactionContextExt dest;
    dest.copy_from(source);
    
    REQUIRE(dest.transaction_id == 200);
    REQUIRE(dest.parent_id == 100);
    REQUIRE(dest.fragment_id == 1);
    REQUIRE(dest.fragment_total == 4);
    REQUIRE(dest.source_module == "cache");
}

TEST_CASE("TransactionContextExt 分片判断", "[transaction][ext]") {
    SECTION("根交易") {
        TransactionContextExt ext;
        ext.parent_id = 0;
        ext.fragment_total = 1;
        REQUIRE(ext.is_root() == true);
        REQUIRE(ext.is_fragmented() == false);
        REQUIRE(ext.get_group_key() == ext.transaction_id);
    }
    
    SECTION("分片交易 - 第一个") {
        TransactionContextExt ext;
        ext.parent_id = 100;
        ext.fragment_id = 0;
        ext.fragment_total = 4;
        REQUIRE(ext.is_fragmented() == true);
        REQUIRE(ext.is_first_fragment() == true);
        REQUIRE(ext.is_last_fragment() == false);
        REQUIRE(ext.get_group_key() == 100);
    }
    
    SECTION("分片交易 - 最后一个") {
        TransactionContextExt ext;
        ext.parent_id = 100;
        ext.fragment_id = 3;
        ext.fragment_total = 4;
        REQUIRE(ext.is_first_fragment() == false);
        REQUIRE(ext.is_last_fragment() == true);
    }
}

TEST_CASE("便捷函数测试", "[transaction][ext]") {
    tlm_generic_payload payload;
    
    SECTION("get_transaction_context - 未设置") {
        auto* ext = get_transaction_context(&payload);
        REQUIRE(ext == nullptr);
    }
    
    SECTION("set_transaction_context") {
        TransactionContextExt src;
        src.transaction_id = 300;
        src.source_module = "memory";
        
        set_transaction_context(&payload, src);
        
        auto* retrieved = get_transaction_context(&payload);
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->transaction_id == 300);
        
        delete retrieved;
    }
    
    SECTION("create_transaction_context") {
        auto* ext = create_transaction_context(&payload, 400, 0, 0, 1);
        
        REQUIRE(ext != nullptr);
        REQUIRE(ext->transaction_id == 400);
        REQUIRE(ext->parent_id == 0);
        REQUIRE(ext->is_root() == true);
        
        delete ext;
    }
    
    SECTION("create_transaction_context - 分片") {
        auto* ext = create_transaction_context(&payload, 501, 500, 1, 4);
        
        REQUIRE(ext->transaction_id == 501);
        REQUIRE(ext->parent_id == 500);
        REQUIRE(ext->fragment_id == 1);
        REQUIRE(ext->fragment_total == 4);
        REQUIRE(ext->is_fragmented() == true);
        
        delete ext;
    }
}

TEST_CASE("Packet transaction_id 方法", "[transaction][packet]") {
    // 创建 payload
    tlm::tlm_generic_payload payload;
    Packet* pkt = new Packet(&payload, 0, PKT_REQ);
    
    SECTION("默认 transaction_id") {
        REQUIRE(pkt->get_transaction_id() == 0);
    }
    
    SECTION("设置 transaction_id") {
        pkt->set_transaction_id(100);
        REQUIRE(pkt->get_transaction_id() == 100);
        REQUIRE(pkt->stream_id == 100);  // 验证同步
    }
    
    SECTION("parent_id") {
        auto* ext = create_transaction_context(&payload, 200, 100, 0, 1);
        auto* pkt2 = new Packet(&payload, 0, PKT_REQ);
        REQUIRE(pkt2->get_parent_id() == 100);
        delete pkt2;
    }
    
    SECTION("分片信息") {
        pkt->set_fragment_info(1, 4);
        REQUIRE(pkt->is_fragmented() == true);
        
        TransactionContextExt* ext = nullptr;
        payload.get_extension(ext);
        REQUIRE(ext != nullptr);
        REQUIRE(ext->fragment_id == 1);
        REQUIRE(ext->fragment_total == 4);
    }
    
    SECTION("group_key") {
        pkt->set_transaction_id(300);
        REQUIRE(pkt->get_group_key() == 300);
        
        auto* ext = get_transaction_context(&payload);
        ext->parent_id = 200;
        REQUIRE(pkt->get_group_key() == 200);
    }
    
    delete pkt;
}

TEST_CASE("TransactionTracker 单例", "[transaction][tracker]") {
    auto& tracker = TransactionTracker::instance();
    
    SECTION("单例唯一性") {
        auto& tracker2 = TransactionTracker::instance();
        REQUIRE(&tracker == &tracker2);
    }
    
    SECTION("初始化") {
        REQUIRE(tracker.is_initialized() == false);
        tracker.initialize();
        REQUIRE(tracker.is_initialized() == true);
    }
}

TEST_CASE("TransactionTracker 创建交易", "[transaction][tracker]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    tlm::tlm_generic_payload payload;
    
    SECTION("创建根交易") {
        uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");
        
        REQUIRE(tid > 0);
        
        const auto* record = tracker.get_transaction(tid);
        REQUIRE(record != nullptr);
        REQUIRE(record->transaction_id == tid);
        REQUIRE(record->source_module == "cpu_0");
        REQUIRE(record->type == "READ");
        REQUIRE(record->parent_id == 0);
        REQUIRE(record->is_complete == false);
    }
    
    SECTION("记录 hop") {
        uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");
        tracker.record_hop(tid, "crossbar", 1, "hopped");
        
        const auto* record = tracker.get_transaction(tid);
        REQUIRE(record->hop_log.size() == 1);
        REQUIRE(record->hop_log[0].first == "crossbar");
        REQUIRE(record->hop_log[0].second == 1);
    }
    
    SECTION("完成交易") {
        uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");
        tracker.complete_transaction(tid);
        
        const auto* record = tracker.get_transaction(tid);
        REQUIRE(record->is_complete == true);
    }
    
    tracker.complete_transaction(999);  // 清理
}

TEST_CASE("TransactionTracker 父子交易", "[transaction][tracker]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    tlm::tlm_generic_payload payload;
    
    uint64_t parent_tid = tracker.create_transaction(&payload, "cpu_0", "READ");
    uint64_t child_tid = tracker.create_transaction(&payload, "cache", "READ");
    
    tracker.link_transactions(parent_tid, child_tid);
    
    SECTION("父子关联") {
        const auto* parent = tracker.get_transaction(parent_tid);
        const auto* child = tracker.get_transaction(child_tid);
        
        REQUIRE(child->parent_id == parent_tid);
        REQUIRE(parent->child_transactions.size() == 1);
        REQUIRE(parent->child_transactions[0] == child_tid);
    }
    
    SECTION("获取子交易列表") {
        auto children = tracker.get_children(parent_tid);
        REQUIRE(children.size() == 1);
        REQUIRE(children[0] == child_tid);
    }
    
    tracker.complete_transaction(child_tid);
    tracker.complete_transaction(parent_tid);
}

TEST_CASE("TransactionTracker 时间推进", "[transaction][tracker]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    REQUIRE(tracker.get_current_time() == 0);
    
    tracker.advance_time(100);
    REQUIRE(tracker.get_current_time() == 100);
    
    tracker.advance_time(50);
    REQUIRE(tracker.get_current_time() == 150);
}

TEST_CASE("TransactionTracker 活跃交易", "[transaction][tracker]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    tlm::tlm_generic_payload payload;
    
    uint64_t tid1 = tracker.create_transaction(&payload, "cpu_0", "READ");
    uint64_t tid2 = tracker.create_transaction(&payload, "cpu_1", "WRITE");
    
    SECTION("活跃交易列表") {
        auto active = tracker.get_active_transactions();
        REQUIRE(active.size() == 2);
    }
    
    tracker.complete_transaction(tid1);
    
    SECTION("完成后移除") {
        auto active = tracker.get_active_transactions();
        REQUIRE(active.size() == 1);
        REQUIRE(active[0] == tid2);
    }
    
    tracker.complete_transaction(tid2);
}
