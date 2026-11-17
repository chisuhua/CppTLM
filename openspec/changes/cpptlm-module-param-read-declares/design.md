# design: cpptlm-module-param-read-declares

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（Proposed · v1.5）。本文件只记录**实施层面的可逆选择**;不可逆决策以 ADR 为准。
> ⚠️ 本文件已按立项期 gap 分析（metis 1 轮）修订：**削掉 `ParamSpec` / `ParamRange` / `ValueDomain` 三件套**（它们是 ADR 选项表里被标为「高成本」的形态 A 的数据结构，本 change 不需要把异构参数塞进同一容器），改为**单个自由函数模板**。

## D-1 — `load_param` 的唯一形态（**本文件定稿**）

```cpp
// include/core/param_loader.hh
namespace cpptlm {

// 一次调用 = 声明 + 类型校验 + 范围校验 + 默认值 + 赋值 + 已消费键登记
//   def 缺省（nullopt）⇒ 必填
//   值域由 T 决定（本 change 的六个参数都不需要「域 ≠ 存储类型」）
template <class T>
void load_param(const json& cfg, std::string_view key, T& target,
                std::optional<T> lo = std::nullopt,
                std::optional<T> hi = std::nullopt,
                std::optional<T> def = std::nullopt);

// 已消费键登记（per-instance，由基类持有）
class ParamLoadContext {
public:
    ParamLoadContext(const json& cfg, std::string_view module_name);
    const json& cfg() const;
    std::string_view module_name() const;
    void mark_consumed(std::string_view key);
    std::vector<std::string> unknown_keys() const;   // cfg 键 − 已消费键
    bool cfg_has_params_key() const;                 // legacy 扁平路径判定（D-3a）
private:
    const json& cfg_;                                // ⚠️ 生命周期由调用方保证（见 D-6）
    std::string_view module_name_;
    std::set<std::string> consumed_;
};

} // namespace cpptlm
```

**失败通道**：所有校验失败 **SHALL** 抛 `cpptlm::ParamValidationError(module_name, key, detail)`（`include/core/param_errors.hh:11-24`，**已有测试** `test/test_param_errors.cc`）;`detail` **SHALL** 含期望范围（形如 `expected range [1,64], got 0`）。⛔ **不新造异常类型**。

**数值边界**（逐条钉，避免可接受值集合被静默放宽）：
- 整型参数：`json::is_number_integer()` 为真才接受（`8.0` / `1e20` / `true` **一律拒绝**）。
- 无符号参数（`credit_timeout`）：另加 `>= 0` 检查（`-1` ⇒ 失败，错误文本见上）。
- 浮点参数（本 change 无调用点）：`is_number()` 即可。

## D-2 — 执行点与加载时机

- **装配入口** = `SimObject::set_config()` 调用链（`module_factory.cc:404-425`）。
- **无 `params` 键的实例也调用一次**（传 `json::object()`）—— 否则默认值永远不落成员（现状 `cfg_src == nullptr` 时根本不调）。
  ⚠️ **与「SHALL NOT 清除外部状态」的冲突已裁决**：该条对**全部未迁移类型**生效，会首次触发它们的 `on_config_loaded` 副作用（`CohIfNICTLM::regions_.clear()` / `PcieEndpointTLM` 的 `cfg_space_` 重初始化 / `PcieEndpointIP` 的主动 detach）。
  ⇒ **收窄**：`SimObject::set_config` 只在 **`is_param_admitted(type()) == true`** 或 **该实例的 `params` 键存在** 时调用 `on_config_loaded()`;对「未准入且无 `params`」的实例**维持现状跳过**（⛔ 不引入新的副作用触发面）。AC-03 钉死该零变化。
- **幂等**：同一实例二次加载 ⇒ 成员被同值覆盖（幂等成立）;`consumed_` 每次 `ParamLoadContext` 构造时清空。

## D-3 — 准入判定（**唯一 C++ 真源**）

```cpp
// include/core/param_admission.hh
namespace cpptlm {
// 唯一真源：哪些模块类型已迁移到 load_param（⇒ 启用未知键检查）
bool is_param_admitted(std::string_view type);
} // namespace cpptlm
```

某类型 `T` 进入准入表的条件（`ADR-X.17` D-3 的「T 迁移完成」）：
- (a) `T` 的全部参数均由 `load_param` 声明，**或**已登记进**表 A**（已消费但不经 `load_param`）;
- (b) `getParamRulesRegistry()` 中无 `T` 的条目;
- (c) CFG-08 的 `T` 分支已删除。

⇒ 本 change 只有 **RouterTLM** 满足 (a)(b)(c);`NICTLM` **不满足 (c)**（分支保留）⇒ **不准入**。
⚠️ 仓库级测试（AC-04）与任何辅助脚本 **SHALL** 从 `is_param_admitted()` 派生，⛔ 不得硬编码类型名。

## D-3a — 未知键的失败位置（**ADR-X.17 D-3a 的落地**）

- `ParamLoadContext` 由 `SimObject` **按实例持有**（成员，非局部变量）;
- `SimObject::set_config` 在 `on_config_loaded()` 返回后执行：
  ```cpp
  if (is_param_admitted(type()) && !ctx_->unknown_keys().empty()) {
      throw ParamValidationError(module_name(), ctx_->unknown_keys().front(),
                                 "unknown parameter");
  }
  ```
- **legacy 扁平路径豁免**（proposal 已知残留 R-3）：当 `cfg_src == &mod`（`module_factory.cc:405-408`）时，`consumed_` 与**整个 module entry** 求差会把 `name` / `type` / `layout` 等判为未知键 ⇒ **该路径显式豁免未知键检查**（判据 = `ParamLoadContext::cfg_has_params_key() == false`）。⛔ 不得改为「特判 `params` 就跳过必填」（那会让扁平配置永久绕过必填）。

## D-4 — 基线（**降级为辅助台账，不是机制**）

| 表 | 内容 | 落点 |
|---|---|---|
| **表 A** · 已消费但不经 `load_param` | 逐键 + 消费方（模块 / 板级直读） | `docs/implementation/module-param-baseline.md` |
| **表 B** · 真未消费豁免 | F-14 的 5 类 / 14 键，逐条登记为待修缺陷 | 同上第二节 |

⚠️ **机制判定由运行时测试承担**（AC-04）;该文档只是**人类可读的台账**，⛔ 不作任何闸的输入（避免第二真源）。

## D-5 — 迁移顺序（**必填保护无空窗**）

| 步 | 内容 | 风险控制 |
|:--:|---|---|
| 1 | `param_loader.hh` + `param_admission.hh` + 夹具模块（**不动** `module_factory`） | 纯新增 |
| 2 | `module_factory` 接入（D-2 的收窄版）+ 基类持有 `ctx_` + 未知键失败（D-3a） | 未准入类型零变化（AC-03） |
| 3 | **单 commit**：RouterTLM 逐参数 `load_param` + 删 `registerParamRules` / `get_param_rules()` / **CFG-08 的全部 RouterTLM 引用（含 `:327` 析取）** + `is_param_admitted("RouterTLM") = true` + 处置 `test_d3_param_resolver_registry.cc` | 必填由 `load_param(def=nullopt)` 承接 ⇒ 无空窗;该 commit 内编译不得中断 |
| 4 | 基线台账（表 A / 表 B） | 辅助 |
| 5 | 回归（AC-03 / AC-08 / G-1 / G-2） | — |

## D-6 — `ParamLoadContext` 的生命周期

`ctx_` 是 `SimObject` 的**成员**（`std::unique_ptr<ParamLoadContext>`），在 `set_config` 内重建（持有 `cfg_` 的**引用**）⇒ 引用目标是 `config_`（也是成员），生命周期与实例一致，**不会悬垂**。⚠️ 若 `config_` 被二次赋值（`set_config` 的常见形态），`ctx_` 必须**同步重建**（在赋值之后）。

## D-7 — RouterTLM 试点细节

| 参数 | 声明 | 说明 |
|---|---|---|
| `node_x` / `node_y` / `mesh_x` / `mesh_y` | `load_param<uint32_t>(cfg, "node_x", node_x_)`（无默认 ⇒ 必填） | ⚠️ **不设范围**（`param-validation` spec 的「mesh 1-16」与事实冲突：`configs/mesh_4x4.json` 有 `node_x: 0`）⇒ 该条由 proposal 已知残留 R-4 另案处置 |
| `buffer_size` | `load_param<uint32_t>(cfg, "buffer_size", buffer_depth_, 1u, 64u, 8u)` | 赋值后 **SHALL** 保留 credit 同源（下条） |
| `credit_timeout` | `load_param<uint64_t>(cfg, "credit_timeout", credit_timeout_, std::nullopt, std::nullopt, 0ull)` | 无范围（既有可接受值集合不收窄） |
| `vc_count` | ⛔ **不声明** | 其值不影响运行时行为（`NUM_VCS` 是编译期常量）⇒ 会退化成「声明了却无效」 |
| `flit_width` | ⛔ **不声明** | 随 `get_param_rules()` 删除而**自然消失**（`configs/` 零命中、实现零消费）⇒ 不进任何基线表 |

**credit 同源副作用（AC-09 的落点）**：`buffer_size` 赋值后重算 `downstream_credits_`，且**仅在深度实际变化时**执行（`if (new_depth != buffer_depth_)`）—— 保证「流量已发生后二次加载」不刷新已消耗额度（`router_tlm.cc:102-108` 的注释明写「此刻尚无任何 flit 流动 ⇒ 重置安全」，该前提只在首次加载成立）。

## D-8 — 观测面与验证载体

- AC-01/02/05/06/10 以**夹具模块**为载体;AC-07 以 **RouterTLM** 为载体（证明基类的未知键检查真的执行）。
- AC-04 是 **Catch2 用例**（`[param][alignment]`）：遍历 `configs/**/*.json`（排除 `configs/param_rules/**` 与非拓扑片段），对含已准入类型实例的配置 `instantiateAll` + 断言 `unknown_keys()` 为空。
- ⛔ **不使用 grep 提取键名**（键名写成常量 / 换行 / 加注释都会让静态闸静默漏键 ⇒ 闸变松而永不红）。
