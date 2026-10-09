// src/tlm/cluster/soc_internal_noc.cc
// SocInternalNoc: minimal implementation (per cpptlm-dgpu-soc-internal-noc-minimal)
//
// 本期: 仅持有 4 target 指针 + route_flit() 路由骨架 + 统计 (全部 inline 在 .hh)。
// 未来 E.1: route_flit() 实际推送 flit 到 target->accept(flit) (本期 placeholder)。
//
// 编译/链接需要 .cc 文件存在 (CMake glob include/tlm/cluster/*.hh; 但
// REGISTER_MODULE 模板实例化可能不需 .cc; 保留 .cc 占位以备 E.1 扩展)
#include "tlm/cluster/soc_internal_noc.hh"
