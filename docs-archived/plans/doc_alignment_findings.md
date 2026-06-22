# CppTLM 文档对齐检查 — 发现报告

> **生成日期**: 2026-04-27
> **检查范围**: docs/architecture/ + docs/implementation/ vs 实际代码
> **检查者**: AI Architect

---

## 一、文档之间的不一致

### 1.1 Phase 6 状态矛盾（严重）

| 文档 | Phase 6 状态 | 实际代码状态 |
|------|-------------|-------------|
| `docs/implementation/README.md` | ⏳ 进行中 / 0% | ✅ 已完成 / 100% |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | ✅ Phase 6 完成 | ✅ 匹配 |
| `docs/implementation/01-implementation-plan.md` | Phase 6: 示例与测试（2 天） | 计划描述与 v2.1 实现不匹配 |

**分析**: `docs/implementation/README.md` 未更新 Phase 6 完成状态。`01-implementation-plan.md` 描述的是 v2.0 早期计划，其 Phase 定义（构建系统/核心基础/交易处理/错误处理/复位系统/示例测试）与 v2.1 的 Phase 定义（Phase 0-6 基础设施→端到端集成）完全不同。

### 1.2 架构文档版本号不一致

**文件**: `docs/architecture/01-hybrid-architecture-v2.1.md`

| 位置 | 版本号 |
|------|--------|
| 文档头部（第 4 行） | v2.1.9 |
| 文档底部（第 622 行） | v2.1.2 |
| 底部日期（第 624 行） | 2026-04-12 Phase 1 实施后 |
| 顶部日期（第 5 行） | 2026-04-22 |

**分析**: 文档底部元数据未随内容更新同步修改。

### 1.3 重复章节

**文件**: `docs/architecture/01-hybrid-architecture-v2.1.md`

- 第 785-799 行: "8.5 Phase 6 端到端集成验证 (2026-04-13)"
- 第 801-815 行: "8.6 Phase 6 端到端集成验证 (2026-04-22)"

**分析**: 8.5 和 8.6 两节内容几乎完全相同（仅日期不同），疑似复制粘贴未清理。

### 1.4 v2.0 与 v2.1 架构文档脱节

| 特性 | v2.0 文档 | v2.1 文档 | 实际代码 |
|------|----------|----------|---------|
| 模块接口 | `ch_stream<T>` 直接 | `InputStreamAdapter<T>` | `InputStreamAdapter<T>` |
| 注册方式 | `ModuleRegistry` | `ModuleFactory` 扩展 | `ModuleFactory` + `ChStreamAdapterFactory` |
| Bundle 类型 | CppHDL `bundle_base` | 双轨（轻量+完整） | 仅轻量级 |
| 目录结构 | 含 `rtl/`、`mapper/` | 含 `rtl/`、`mapper/` | 无 `rtl/`、`mapper/` |

---

## 二、文档与代码的不匹配

### 2.1 目录结构不匹配

**文档声称存在但实际不存在的目录/文件**:

| 文档路径 | 实际状态 | 备注 |
|---------|---------|------|
| `include/tlm/legacy/` | ❌ 不存在 | Legacy 模块在 `include/modules/legacy/` |
| `include/rtl/` | ❌ 不存在 | 未来扩展，未实现 |
| `include/mapper/` | ❌ 不存在 | 未来扩展，未实现 |
| `include/framework/stream_adapter_registry.hh` | ❌ 不存在 | 注册功能在 `core/chstream_adapter_factory.hh` |
| `include/bundles/cache_bundles.hh` | ❌ 不存在 | 实际使用 `cache_bundles_tlm.hh`（轻量级） |
| `include/bundles/noc_bundles.hh` | ❌ 不存在 | 实际使用 `noc_bundles_tlm.hh` |
| `include/bundles/fragment_bundles.hh` | ❌ 不存在 | 实际使用 `noc_bundles_tlm.hh` 中的定义 |

### 2.2 JSON 配置格式不匹配

**文档 (v2.1 4.4 节) 描述的 JSON 格式**:
```json
{
  "modules": [
    { "name": "l1", "type": "CacheTLM", "mode": "chstream",
      "req_bundle": "CacheReqBundle", "resp_bundle": "CacheRespBundle" }
  ]
}
```

**实际代码使用的 JSON 格式** (`test/test_phase6_integration.cc`):
```json
{
  "modules": [
    {"name": "cache", "type": "CacheTLM"},
    {"name": "xbar", "type": "CrossbarTLM"},
    {"name": "mem", "type": "MemoryTLM"}
  ],
  "connections": [
    {"src": "cache", "dst": "xbar.0", "latency": 1}
  ]
}
```

**差异**:
- 文档要求 `"mode": "chstream"`，实际不需要（自动识别）
- 文档要求 `"req_bundle"` / `"resp_bundle"`，实际不需要（模板参数在注册时确定）

### 2.3 注册体系不匹配

**文档 (v2.1 8.1 节) 声称**:
- `REGISTER_CHSTREAM_MODULE` 宏未定义，复用 `REGISTER_OBJECT`
- ChStream 模块通过 `dynamic_cast` 识别

**实际代码** (`include/chstream_register.hh`):
- 定义了完整的 `REGISTER_CHSTREAM` 宏
- 注册了 8 个模块（CacheTLM, MemoryTLM, CrossbarTLM, CPUTLM, TrafficGenTLM, ArbiterTLM<2>, ArbiterTLM<4>, RouterTLM, NICTLM）
- 注册了 8 个对应的 StreamAdapter

### 2.4 ModuleFactory 逻辑不匹配

**文档 (v2.1 4.4 节) 伪代码**:
```cpp
for (auto& mod : final_config["modules"]) {
    std::string mode = mod.value("mode", "legacy");
    if (mode != "chstream") continue;
    // 手动创建 adapter
    auto* adapter = create_stream_adapter(chstream_mod, ...);
    chstream_mod->set_adapter(adapter);
}
```

**实际代码** (`src/core/module_factory.cc` 第 301-371 行):
- 遍历所有已创建的模块实例
- 通过 `dynamic_cast<ChStreamModuleBase*>(obj)` 自动识别
- 使用 `ChStreamAdapterFactory::create(type, obj)` 自动创建适配器
- 支持单端口/多端口/双端口/双向端口感知

### 2.5 StreamAdapter 实现不匹配

**文档 (v2.1 4.3 节) 伪代码**:
```cpp
class StreamAdapter : public StreamAdapterBase {
    std::function<ch_stream<ReqBundle>&()> get_req_in_;
    std::function<ch_stream<RespBundle>&()> get_resp_out_;
};
```

**实际代码** (`include/framework/stream_adapter.hh`):
```cpp
template<typename ModuleT, typename ReqBundleT, typename RespBundleT>
class StreamAdapter : public StreamAdapterBase {
    ModuleT* module_;  // 直接指针，非 std::function
    // 通过 module_->req_in() / module_->resp_out() 直接访问
};
```

**差异**: 文档使用 `std::function` 间接层，实际代码使用直接模块指针。

### 2.6 额外模块未在文档中体现

**文档 (v2.1) 提到的模块**: CacheTLM, CrossbarTLM, MemoryTLM

**实际代码中存在的额外模块**:
- CPUTLM (`include/tlm/cpu_tlm.hh`)
- TrafficGenTLM (`include/tlm/traffic_gen_tlm.hh`)
- ArbiterTLM<2> / ArbiterTLM<4> (`include/tlm/arbiter_tlm.hh`)
- RouterTLM (`include/tlm/router_tlm.hh`)
- NICTLM (`include/tlm/nic_tlm.hh`)

### 2.7 AGENTS.md 自身错误

**AGENTS.md 声称**:
> "docs/architecture/01-hybrid-architecture-v2.1.md 停留在 v2.1.5，Phase 6 未同步"

**实际文档状态**:
- 文档头部明确标注 "v2.1.9"
- 8.5/8.6/8.7 节详细描述了 Phase 6 完成和 CI/CD 集成
- 文档状态为 "✅ Phase 6 完成，端到端验证通过"

**分析**: AGENTS.md 自身的文档状态信息已经过时。

---

## 三、问题严重度分级

| 严重度 | 数量 | 问题类别 |
|--------|------|---------|
| 🔴 **严重** | 3 | Phase 6 状态错误、JSON 格式不匹配、目录结构不匹配 |
| 🟡 **中等** | 5 | 版本号不一致、重复章节、注册体系不匹配、ModuleFactory 逻辑不匹配、额外模块未文档化 |
| 🟢 **轻微** | 2 | StreamAdapter 实现差异、AGENTS.md 过时 |

---

## 四、修复建议

1. **立即修复** (高优先级):
   - 更新 `docs/implementation/README.md` Phase 6 状态为 "✅ 完成 / 100%"
   - 更新 `AGENTS.md` 中关于文档状态的描述
   - 删除/合并 `01-hybrid-architecture-v2.1.md` 中重复的 8.5/8.6 节

2. **短期修复** (中优先级):
   - 更新 `01-hybrid-architecture-v2.1.md` 底部版本号与顶部一致
   - 更新 JSON 配置格式描述，删除 `"mode"` / `"req_bundle"` 要求
   - 更新目录结构图，移除未实现的 `rtl/`、`mapper/`、`tlm/legacy/`
   - 补充文档中缺失的模块（CPUTLM、TrafficGenTLM、ArbiterTLM 等）

3. **长期清理** (低优先级):
   - 归档或更新 `docs/implementation/01-implementation-plan.md` 和 `02-implementation-plan-detailed.md`（它们描述的是 v2.0 计划，与 v2.1 实现严重不符）
   - 统一文档中的伪代码与实际代码风格
