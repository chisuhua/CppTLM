// include/core/param_admission.hh
// 参数加载机制的「准入表」：哪些模块类型已迁移到 load_param（⇒ 启用未知键检查）
// 作者 CppTLM Team / 日期 2026-11-17
//
// 决策源：docs/adr/ADR-X.17-module-param-read-declares.md（D-3 / D-3a）
//
// ⚠️ 本表是**唯一 C++ 真源**：运行时（SimObject::set_config）与任何仓库级测试/闸
//    都必须经由本头文件的接口判定，⛔ 不得在脚本或测试里硬编码类型名。
//
// ⚠️ **header-only 是必须的**：`SimObject::set_config` 的 inline 成员（enforce_param_contract）
//    调用本表 ⇒ 若本表在 .cc 里定义，任何「直接编译 include/ 的独立探针」都会链接失败
//    （实证：`scripts/test/cep_*_probe.cc` 族）。⇒ 表用 function-local static，跨 TU 唯一。
//
// 准入条件（ADR-X.17 D-3 的「T 迁移完成」）：
//   (a) T 的全部参数均由 load_param 声明，或已登记为「已消费但不经 load_param」
//   (b) getParamRulesRegistry() 中无 T 的条目
//   (c) CFG-08 的 T 分支已删除
#ifndef CORE_PARAM_ADMISSION_HH
#define CORE_PARAM_ADMISSION_HH

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cpptlm {

namespace detail {

/// 准入类型集合（function-local static ⇒ inline 函数内跨 TU 唯一）
inline std::set<std::string, std::less<>>& param_admission_table() {
    static std::set<std::string, std::less<>> table = {
        // 已迁移：模块内 load_param 声明 + 无 ParamRules 条目 + 无 CFG-08 分支
        "RouterTLM",
        // ── change `cpptlm-module-param-migrate-batch2` T1.1 ──
        // `node_id` 必填（无默认值）+ `mesh_x`/`mesh_y` 可选 + `address_regions`（非标量）态②。
        // ⚠️ CFG-08 的 NICTLM per-type 分支已同批退役 ⇒ 必填真值唯一。
        "NICTLM",
        // ── T1.2 ──
        // `capacity_gb`/`size`/`read_latency`/`write_latency` 态① + `type`/`mem_type` 态②拒绝
        // + 7 个板级直读键 `ignore_param` 登记（D-6 / 台账表 A）。
        "MemoryTLM",
        // ── T1.3 ──
        // `vram_size_bytes` 态① + `operating_frequency_hz` 态②拒绝
        // + 继承基类的板级直读键登记。
        "VramControllerTLM",
        // ── T1.4 ──
        // `page_size_bytes` 态①（2 的幂 ≥ 4096）+ `tlb_size`/`tlb_miss_latency_cycles`
        // 板级直读键登记。
        "GmmuTLM",
        // ── T1.5 ──
        // `pattern`/`num_requests`/`seed`/`stride` 态① + `start_addr`/`end_addr`
        // 地址读法（`parse_addr_json`，非 load_param）+ `hotspot_addrs`/`hotspot_weights`
        // 非标量登记。
        "TrafficGenTLM",
        // ── T1.6 ──
        // `max_inflight`/`translate_latency`/`vram_size_bytes` 全部态①（可选）。
        "SdmaEngineTLM",
        // ── T1.8 ──
        // `node_id`/`mesh_x`/`mesh_y` 态①（可选）+ `address_regions` 非标量登记。
        "CohIfNICTLM",
        // ── T1.9 ──
        // `emit_completions`/`completion_latency_cycles`/`emit_snoops`/`snoop_interval_cycles`
        // 全部态①（可选，类型不符 ⇒ 装配期失败）。
        "CohIfResponderTLM",
        // ── T2 SimModule 家族（P1b）────────────────────────────────────────
        // 全部在 `set_config` 覆写里经 `set_config_with` 自读（enforce 晚于自读）。
        // CpuCluster: `num_cpus` / `cluster_id` 态①（可选）。
        "CpuCluster",
        // CacheCluster: `l1_count` 态① + `l1_size`/`l2_size`（带单位字符串，解析归模块内）。
        "CacheCluster",
        // MemoryCluster: `channel_count` 态① + `channel_size`（带单位字符串）+ `memory_type` 态②拒绝。
        "MemoryCluster",
        // ComputeCluster: `cu_count` 态①（钳位归模块内）+ `cu_template`（路径字符串）。
        "ComputeCluster",
        // TpcCluster / GpcCluster / GpuCluster: 几何计数 + `cu_template`（路径字符串）。
        "TpcCluster",
        "GpcCluster",
        "GpuCluster",
        // ApuSoC: `cpu_topology`/`gpu_topology`（路径字符串）+ `coherent_xbar_name`。
        "ApuSoC",
        // CepMeshInterconnect: `template`（路径字符串）。
        "CepMeshInterconnect",
        // ⚠️ `GpuNoC` 的准入在 **T3.2** 落地（`routing` 的裁决由 C1 的 B3 完成 ——
        //    `RouterTLM` 落态③「合法值唯一 `XY`」，本类只是把它透传给内部 router）。
        //    T2 批次**未**准入（当时的窗口期约束），T3 承载该动作。
        "GpuNoC",
    };
    return table;
}

} // namespace detail

/// 该模块类型是否已准入（⇒ 启用未知键检查 + 无 params 也加载）
inline bool is_param_admitted(std::string_view type) {
    const auto& t = detail::param_admission_table();
    return t.find(type) != t.end();
}

/// 已准入类型的完整清单（升序）—— 供仓库级对拍从本表派生，避免第二份真源
inline std::vector<std::string> admitted_param_types() {
    const auto& t = detail::param_admission_table();
    return std::vector<std::string>(t.begin(), t.end());
}

/// 注册一个准入类型。**生产代码不应调用**；仅测试夹具与迁移期新类型使用。幂等。
inline void register_param_admitted(std::string_view type) {
    detail::param_admission_table().emplace(type);
}

} // namespace cpptlm

#endif // CORE_PARAM_ADMISSION_HH
