// test/test_phase3_transaction_context.cc
// Phase 3: TransactionContext 测试（测试先行）

#include <catch2/catch_all.hpp>
#include "ext/transaction_context_ext.hh"
#ifdef USE_SYSTEMC_STUB
#include "tlm/tlm_stub.hh"
#else
#include "tlm.h"
#endif
#include "core/packet.hh"
#include "framework/transaction_tracker.hh"

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
    
    auto* cloned_ptr = original.clone();
    
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
        
    }
    
    SECTION("create_transaction_context") {
        auto* ext = create_transaction_context(&payload, 400, 0, 0, 1);
        
        REQUIRE(ext != nullptr);
        REQUIRE(ext->transaction_id == 400);
        REQUIRE(ext->parent_id == 0);
        REQUIRE(ext->is_root() == true);
        
        // 注意: payload 会管理 extension 的生命周期
    }
    
    SECTION("create_transaction_context - 分片") {
        auto* ext = create_transaction_context(&payload, 501, 500, 1, 4);
        
        REQUIRE(ext->transaction_id == 501);
        REQUIRE(ext->parent_id == 500);
        REQUIRE(ext->fragment_id == 1);
        REQUIRE(ext->fragment_total == 4);
        REQUIRE(ext->is_fragmented() == true);
        
        // 注意: payload 会管理 extension 的生命周期
    }
}

