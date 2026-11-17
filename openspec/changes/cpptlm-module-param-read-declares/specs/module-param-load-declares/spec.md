# module-param-load-declares Specification (delta)

## ADDED Requirements

### Requirement: module-param-read-declares

模块的可配置参数 SHALL 以「一次读取调用即一次声明」的方式加载 —— 该调用同时完成**类型校验**、**范围校验**、**默认值填充**与**赋值**，并登记该键为「已消费」。⇒ 不存在与读取分离的独立声明面（⛔ 同一参数 SHALL NOT 在两处各写一遍）。

#### Scenario: 参数越界 SHALL 在装配期失败

- **WHEN** 某模块实例的配置含越界参数值（如 `buffer_size: 0` 或 `65`，声明范围 1..64）
- **THEN** 装配 SHALL **失败**（`instantiateAll` 返回 false）
- **AND** 错误信息 SHALL 含**模块名 + 键名 + 期望范围**
- **AND** SHALL **NOT** 静默截断到边界
- **AND** SHALL **NOT** 回退默认值而让装配通过

#### Scenario: 参数类型错误 SHALL 在装配期失败

- **WHEN** 某模块实例的配置含类型不符的参数值（如 `buffer_size: "abc"`）
- **THEN** 装配 SHALL **失败**
- **AND** SHALL **NOT** 依赖模块内部 `get<T>()` 的顺带异常作为唯一防线

#### Scenario: 数值边界 SHALL NOT 被静默放宽或收窄

- **WHEN** 某参数的声明范围与成员类型可表示范围不一致（如声明 `1..64` 但成员为 `uint8_t`）
- **THEN** 装配 SHALL 以**声明范围**为准判定（⛔ 不得因成员类型更宽而静默放宽）
- **AND** 整型参数 SHALL 拒绝非整数 JSON 值（`8.0` / `1e20` / `true` 一律失败，⛔ 不得依赖 `get<int64_t>()` 的隐式转换）
- **AND** 无符号参数 SHALL 拒绝负值

#### Scenario: 无 params 键的实例 SHALL 仍获得默认值

- **WHEN** 某模块实例的配置**不含** `params` 键（或 `params` 中不含该参数）
- **THEN** 声明的默认值 SHALL 落到对应成员
- **AND** 该实例 SHALL 被加载一次（⛔ 不得因 `params` 缺失而跳过加载）

#### Scenario: 加载 SHALL 幂等

- **WHEN** 同一实例被二次加载
- **THEN** 成员值 SHALL 不变
- **AND** 加载 SHALL NOT 清除或重置**非本机制写入**的状态
- **AND** 该「不清除」性质 SHALL 由**既有副作用套件零变化**承载判定（本 change 不迁移带此类副作用的类型 ⇒ 其行为差异只可能来自被改动的公共路径）

#### Scenario: 默认值 SHALL NOT 写回配置 JSON

- **WHEN** 某参数声明了默认值，而实例配置中**不提供**该键
- **THEN** 默认值 SHALL 落到成员
- **AND** 装配后的配置对象中 SHALL **NOT** 出现该键（⛔ 不得为「让默认值生效」而把默认值注入回 `params`）

### Requirement: module-param-unknown-key-detection

对**已准入**的模块类型（`T` 的全部参数均已声明或已登记为「已消费但不经 `load_param`」，且 `T` 无遗留 `ParamRules` 条目、无遗留 per-type 硬编码必填分支），`params` 中**未被消费**的键 SHALL 被判为**未知键**并使装配失败。判定 SHALL 由 `SimObject::set_config` 在模块回调返回后**统一执行**（⛔ 不依赖各模块自行调用）;准入表 SHALL 是**唯一 C++ 真源**（`is_param_admitted`），任何闸/测试 SHALL 从该表派生（⛔ 不得硬编码类型名）。

#### Scenario: 已准入类型上的拼错参数名 SHALL 装配失败

- **WHEN** 已准入类型的配置含拼错的参数名（如 `buffer_sizes`）
- **THEN** 装配 SHALL **失败**
- **AND** 错误信息 SHALL 含该键名
- **AND** SHALL **NOT** 静默忽略（不得伪装成「用了默认值」）

#### Scenario: 未准入类型 SHALL 维持宽松（零回归）

- **WHEN** 某类型尚未准入（未迁移 / 仍有遗留声明面 / 含非标量参数）
- **THEN** 其参数行为 SHALL 与改动前**逐项一致**
- **AND** 未知键检查 SHALL NOT 作用于该类型

#### Scenario: legacy 扁平路径 SHALL 豁免未知键检查

- **WHEN** 模块配置采用 legacy 扁平形式（参数直接写在 module 条目上，而非 `params` 子对象）
- **THEN** 未知键检查 SHALL 被豁免（⛔ 否则 `name` / `type` / `layout` 等条目字段会被误判为未知键）
- **AND** 该豁免 SHALL NOT 导致必填检查被跳过

#### Scenario: 板级直读消费的键 SHALL 登记为已消费

- **WHEN** 某键由板级直读路径（而非模块自身）消费
- **THEN** 该键 SHALL 登记为「已消费但不经 `load_param`」并标注消费方
- **AND** 同一类型内 SHALL NOT 同时存在「`load_param` 声明的键」与「板级直读的键」

### Requirement: module-param-behavioral-evidence

每个被声明的参数 SHALL 附带一条**行为判别**测试：显式设置两个不同合法值（默认值 / 非默认值）⇒ 至少一个可观测指标（统计量 / 只读访问器 / 输出计数）不同。⛔ 无可观测行为影响的参数 SHALL NOT 进入声明。

#### Scenario: 声明了却无效果的参数 SHALL 被拒绝

- **WHEN** 某参数被声明，但其值不影响任何可观测指标（如声明了 `vc_count` 而维度仍是编译期常量）
- **THEN** 该参数 SHALL NOT 进入声明
- **AND** 该缺陷 SHALL 由行为判别测试的缺失暴露（⛔ 机制本身不保证「写入即被读取」）
