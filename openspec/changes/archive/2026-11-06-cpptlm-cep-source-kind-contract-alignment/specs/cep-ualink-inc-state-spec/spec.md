# cep-ualink-inc-state-spec Specification (delta)

## ADDED Requirements

### Requirement: cep-ualink-inc-state-spec-self-containment

`ualink_inc` 规约载体 SHALL 保持**自足**:除**标准库**外 SHALL NOT 引入任何项目内头(引号形式 include 即红)⇒ 载体不得依赖运行期平台标识 / 区域表 / 转移判定。**唯一白名单**为 `"tlm/cep/source_registry.hh"` —— 纯 `constexpr`、只依赖标准库的**来源类型注册表**(来源类型单一真源)⇒ 允许引用;⛔ 除此之外的任何项目内头 SHALL 判红。

判据 SHALL 由**载体自足扫描**(主测试 `UIS-10` + 反向验证探针 `P-1b`)承担,且扫描器 SHALL 具备**鉴别力**(同一扫描器对毒化文本必须命中)。

#### Scenario: UIS-12 —— 载体自足:仅标准库 + 唯一注册表白名单

- **WHEN** 扫描 `include/tlm/cep/ualink_inc_state_spec.hh` 的 include 列表
- **THEN** 引号形式 include SHALL 仅允许 `"tlm/cep/source_registry.hh"`;其余引号形式 SHALL **判红**
- **AND** 注册表头的引用 SHALL NOT 引入平台标识 / 区域表 / 转移判定(该头为纯 `constexpr` + 标准库)
- **AND** 扫描器 SHALL 具备**鉴别力**:对**毒化文本**(故意加入引号形式项目内头)必须命中 ⇒ 否则「零命中」可能只是扫描器失效
- **AND** **反向 fixture**:删掉注册表白名单 ⇒ 该 include 仍为白名单项(不误红);把白名单换成**其它**项目内头 ⇒ SHALL 判红
