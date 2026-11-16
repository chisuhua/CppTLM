// include/tlm/cep/cep_mesh_interconnect.hh
// CepMeshInterconnect: 互联拓扑的**容器型 SimModule**（header-only）
//
// 功能描述：把互联件（router / link）的**模块与内部连接**交由一份**独立蓝图
//           JSON** 承载（`params.template` 指向），使
//             ① 互联层与业务层在**文件**维度分离（蓝图 vs 主拓扑）
//             ② 互联层在**命名空间**维度分离（`<容器>.<内部名>` vs 业务模块名）
//             ③ 同一份蓝图可被多个主拓扑**复用**，换形态只改 `params.template`
//
// 作者 CppTLM Team / 日期 2026-11-16
//
// ── 与既有机制的关系（⛔ 不发明任何新机制）────────────────────────────
//
// · **蓝图加载**：照 `ComputeCluster::cu_template` 的既有模式
//   （`set_config` 读 params → `JsonIncluder::loadAndInclude` →
//    `internal_factory->instantiateAll`），见 `src/tlm/cluster/compute_cluster.cc`。
// · **端口暴露**：走 `SimModule` 的**通用** `outputs` / `inputs` 机制
//   ⇒ 本类**零端口代码**（⛔ 不写专用暴露 API）。
// · **时钟**：`SimModule(n, eq)` 的 `internal_factory` 共用同一 `EventQueue`
//   ⇒ 内部模块与外部**天然同步**（`include/core/sim_module.hh:66-67`）。
//
// ── 蓝图契约（由 design D-2.3 定义）────────────────────────────────────
//
// · **必须**含非空 `modules` 数组（缺 ⇒ **抛错**，⛔ 不静默空装配）
// · 内部 router 命名 `r00`..`rNN` 是**接口契约**（主拓扑的 `outputs.internal`
//   按名指过去）⇒ ⛔ 改名会破坏契约
// · ⛔ 蓝图**不得**含 `outputs` / `inputs`（端口暴露归主拓扑）
// · ⛔ 蓝图**不得**含业务模块（NIC / coherence / 地址映射）
//
// ── 边界（如实登记）──────────────────────────────────────────────────
//
// ⚠️ 本类**不承载业务语义** —— 它只做「加载蓝图 + 交由内部工厂装配 + 转发周期
//    推进」。NIC / coherence 协议 / 地址映射任一职责均**不在**本类。
#ifndef TLM_CEP_CEP_MESH_INTERCONNECT_HH
#define TLM_CEP_CEP_MESH_INTERCONNECT_HH

#include "core/sim_module.hh"
#include "utils/json_includer.hh"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace tlm::cep {

/// 互联拓扑容器：蓝图由 `params.template` 指向的外部 JSON 承载
class CepMeshInterconnect : public SimModule {
public:
    explicit CepMeshInterconnect(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    std::string get_module_type() const override { return "CepMeshInterconnect"; }

    /// 读 `params.template`（其余 params 交基类）
    void set_config(const nlohmann::json& params) override {
        SimModule::set_config(params);
        if (params.contains("template") && params["template"].is_string()) {
            const auto path = params["template"].get<std::string>();
            if (!path.empty()) {
                template_path_ = path;
            }
        }
    }

    /// ⚠️ 顺序照 `ComputeCluster::simulate_instantiate`（⛔ 不可颠倒）：
    ///    先让基类用**主 config** 解析 `outputs`/`inputs`，再装配蓝图。
    void simulate_instantiate(const nlohmann::json& cfg) override {
        // 幂等守卫（防双激活二次装配）
        if (internal_factory && !internal_factory->getAllInstances().empty()) {
            return;
        }
        // ① 基类：幂等守卫 + instantiateAll(cfg) + **parsePortConfigs(cfg)**
        //    ⇒ 端口暴露表用**主拓扑**的 outputs/inputs 填充（蓝图里没有它们）
        SimModule::simulate_instantiate(cfg);

        if (template_path_.empty()) {
            return;
        }
        // ② 加载蓝图（相对路径以 CWD 为准 ⇒ 配置须给仓库根相对路径）
        nlohmann::json blueprint = JsonIncluder::loadAndInclude(template_path_);
        if (!blueprint.contains("modules") || !blueprint["modules"].is_array() ||
            blueprint["modules"].empty()) {
            throw std::runtime_error(
                "CepMeshInterconnect: 蓝图必须含非空 'modules' 数组: " + template_path_);
        }
        // ③ 装配蓝图内部件
        if (!internal_factory->instantiateAll(blueprint)) {
            throw std::runtime_error("CepMeshInterconnect: 蓝图装配失败: " + template_path_);
        }
    }

    const std::string& template_path() const { return template_path_; }

private:
    std::string template_path_;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_CEP_MESH_INTERCONNECT_HH
