// src/traffic_main.cpp
// 流量仿真主程序（等待 v2.1 架构升级后重写）
// 功能描述：预留入口，当前为空实现以避免编译阻塞
// 作者 CppTLM Team
// 日期 2026-04-12
#include <iostream>
#include "event_queue.hh"
#include "modules/legacy/traffic_gen.hh"
#include "modules/legacy/memory_sim.hh"

int main() {
    EventQueue eq;

    MemorySim mem("mem", &eq);
    TrafficGenerator tg("tg", &eq);

    tg.initiate_tick();
    mem.initiate_tick();

    eq.run(1000);
    return 0;
}
