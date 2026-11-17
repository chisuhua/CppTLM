# router-buffer-size-param Specification

## Purpose
让 `RouterTLM` 的 `buffer_size` 配置项从「已声明但静默无效」变为真正生效：此前该参数被 `get_param_rules()` 与 `configs/param_rules/router_tlm.json` 双重声明，实现却用编译期常量 `BUFFER_DEPTH` 决定深度 —— 配置项写了也不起作用，是最难察觉的一类配置缺陷。本 capability 要求缓冲深度与 credit 预算**同源**（同一个参数决定二者，否则会出现「收得多但只敢发少」的隐性不一致）、非法取值在**装配期**硬失败（不静默截断或回退默认值）、且默认值必须让**声明反映实现**（取 8 而非旧声明的 16，保证接入该参数机制零行为变更）。

## Requirements

### Requirement: router-buffer-size-param-effective

`RouterTLM` SHALL 使其 `buffer_size` 配置项**实际生效** —— 该参数此前被 `get_param_rules()` 声明却无人消费，实际深度为编译期常量 ⇒ 配置项静默无效。生效后，缓冲深度与 credit 预算 SHALL **同源**（二者由同一个参数决定），且非法取值 SHALL 在**装配期**被拒绝。

#### Scenario: buffer_size SHALL 决定输入缓冲的实际容量

- **WHEN** 某 `RouterTLM` 实例的配置含 `buffer_size: N`（`N` ∈ 1..64）
- **THEN** 该实例的**输入缓冲实际容量** SHALL 为 `N`
- **AND** 注入的第 `N` 个 flit SHALL 仍被接收
- **AND** 第 `N+1` 个 flit SHALL 因缓冲已满而被丢弃
- **AND** 该实例的 **credit 预算** SHALL 同为 `N`（⛔ 缓冲深度与 credit 预算 SHALL NOT 来自两个不同的来源）

> **锚点**：`include/tlm/router_tlm.hh` 的运行时深度成员 · `src/tlm/router_tlm.cc` 的 `on_config_loaded` 与 4 个使用点 · `test/test_router_local_port_credit.cc` 同款注入装置
> **决策源**：本 change 的 `design.md` D-1（默认值裁决）+ D-2（实现形态 · 「同源」要求）
>
> ⚠️ **验证锚点（观测面边界）**：`input_buffer_` 是 private，且「容量判定」在**稳态**下与深度无关（egress 恒 1 flit/cycle 会持续消化积压 ⇒ 持续注入的转发总数由**时长**决定，与容量无关）。⇒ 「第 `N+1` 个被丢弃」**不可直接观测**，验证须用两条**互补**判据：① `RouterTLM::buffer_depth()` 只读访问器 == `N`（直接证据）② 每 in_port 注入 `K > N` 个 ⇒ **必丢**（转发数 < `4K`；4 个 in_port 汇聚同一出口，注入 4 flit/cycle 远快于消费 1 flit/cycle）。详见 `design.md` D-3。

#### Scenario: 非法 buffer_size SHALL 在装配期 fail

- **WHEN** 某 `RouterTLM` 实例的配置含**非法** `buffer_size`（`0` / `65` / 非整数类型）
- **THEN** 装配 SHALL **失败**（硬错误）
- **AND** SHALL **NOT** 静默截断到合法区间边界
- **AND** SHALL **NOT** 静默回退到默认值而让装配通过

> **锚点**：`src/tlm/router_tlm.cc` 的 `on_config_loaded` 校验分支 · 装配期 fail 的用例

#### Scenario: 未声明 buffer_size 时的默认深度 SHALL 与实现一致

- **WHEN** 某 `RouterTLM` 实例的配置**未含** `buffer_size`
- **THEN** 其生效深度 SHALL 等于**参数声明中的默认值**
- **AND** 该默认值 SHALL 与实现的默认深度**一致**（⛔ 声明值与实现值 SHALL NOT 背离）
- **AND** 该默认路径下的行为 SHALL 与「参数机制接入前」**逐项一致**

> **锚点**：`src/tlm/router_tlm.cc` 的 `get_param_rules` 默认值 · `include/tlm/router_tlm.hh` 的默认深度常量
> **决策源**：`design.md` D-1（裁决：默认值取「对齐实现」而非「对齐旧声明」⇒ 零行为变更）
