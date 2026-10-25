
## ADDED Requirements

### Requirement: cep-stub-cache-core

CEP SHALL 将 **`ADR-CEP-05` 的 Stub Cache Core 定义**(6 接口 / 5 态状态机 / 双端口形态 / 折叠映射表 / 6 条 `INV-SC*`)实现为**可执行的契约**:每条 SHALL 有**行为级**断言(不得只断言「常量存在」),且 SHALL **只收本仓可验证**的判据。

> **定位**:Stub Cache Core 是**客户端侧协议状态投影**(RN projection),**不是缓存性能模型**(per `ADR-CEP-05` §2 v0.3 定位澄清),只维护**稳定状态**;接口名**逐字对齐** `ADR-CEP-02 §2` canonical 10 流(per 决策 1)。

#### Scenario: 6 接口 SHALL 存在且命名逐字对齐 canonical 10 流

- **WHEN** 读取 stub 的 net-side 接口面
- **THEN** SHALL 存在且仅存在这 **5** 条 CohIf RN 投影:`request(addr, perm)` / `snoop(addr, type, txn_id)` / `snoop_resp(addr, hit, state, dirty, data, txn_id)` / `writeback(addr, data, dirty)` / `completion(txn_id, data, perm, error)`
- **AND** SHALL 存在 **1** 条 core-side 内部原语:`evict_line(addr)`(net-side 表现为流 5 `writeback` + 流 2 `completion`)
- **AND** 命名 SHALL **不得**出现初版旧名(`request_line` / `snoop_query` / `complete`)
- **AND** 断言 SHALL 针对**签名与命名**,且 SHALL 与 `ADR-CEP-05` 决策 1 的 6 行表**逐项对齐**

> **锚点**:`test_cep_stub_cache_core.cc` 的接口签名断言

#### Scenario: 5 态状态机 SHALL 可达

- **WHEN** 从 `NONE` 出发施加合法事件序列
- **THEN** `I` / `S` / `E` / `M` 四态 SHALL **均可达**
- **AND** 状态集 SHALL 恰为 `{NONE, I, S, E, M}`(per `ADR-CEP-05` §4.1 的 `NONE → I → S → E → M`)
- **AND** 断言 SHALL 针对**可达性**(存在一条路径抵达),**不**得只断言「状态枚举存在」

> **锚点**:`test_cep_stub_cache_core.cc` 的可达性断言

#### Scenario: 接口面 SHALL **NOT** 暴露 CHI 七态

- **WHEN** 扫描 stub 的接口面与状态面
- **THEN** SHALL **不**出现 CHI RN-F 细节态 `UC` / `SC` / `UD` / `SD`
- **AND** 理由 SHALL 记录:CHI 七态是 ProtocolEngine 适配器层的职责(per `ADR-CEP-05` §4.1「禁止」条 + `INV-CEP-05-IC1`「只暴露稳定状态」)

> **锚点**:`test_cep_stub_cache_core.cc` 的静态扫描断言

#### Scenario: `INV-SC1` ~ `INV-SC6` SHALL 各至少有 1 条**行为**断言

- **WHEN** 读取 stub 的契约测试
- **THEN** 下列 6 条**各** SHALL 至少有 **1** 条断言:

| 不变量 | 断言对象 |
|---|---|
| `INV-SC1` | 同一 addr 最多 **1** 个 outstanding request |
| `INV-SC2` | evict **不**允许选取 outstanding line |
| `INV-SC3` | snoop 响应在固定延迟内返回(参数化) |
| `INV-SC4` | completion 与 request 严格配对(无 unsolicited completion) |
| `INV-SC5` | dirty writeback 带 data;clean writeback 不带 |
| `INV-SC6` | hit 路径**不**产生网络流量 |

- **AND** 每条断言 SHALL 针对**行为**(状态迁移 / 计数 / 载荷 / 时延),SHALL **不**得只断言「某常量存在」或「某枚举值存在」
- **AND** `net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)

> **锚点**:`test_cep_stub_cache_core.cc` 的 6 组行为断言(覆盖统计断言 ≥ 6)

#### Scenario: 折叠映射表 SHALL 呈**单向形态**

- **WHEN** 扫描 stub 的接口面
- **THEN** SHALL **不**出现 `forward_data` / `back_invalidate` / `barrier` 三个词作为 stub 的对外接口
- **AND** 理由 SHALL 记录(per `ADR-CEP-05` 决策 3):流 6 `forward_data` 不可显式为接口(数据可用即视为 forward 或 memory 完成)· 流 7 `back_invalidate` 表现为普通 invalidate snoop · 流 9 `barrier` 对 stub 不可见 · 流 8 `address_resolve` 在 ProtocolEngine 内终结 · 流 10 `snoop_completion_ack` 由 ProtocolEngine 内部消化

> **锚点**:`test_cep_stub_cache_core.cc` 的静态扫描断言

#### Scenario: hit 路径 SHALL 零网络流量

- **WHEN** `cpu_side` 命中一个已缓存的 line
- **THEN** `net_side` SHALL **不**产生任何 flit
- **AND** 该断言 SHALL 是 `INV-SC6` 的**可执行**形态(计数断言),**不**得只写「设计如此」

> **锚点**:`test_cep_stub_cache_core.cc` 的 `INV-SC6` 计数断言

#### Scenario: 不可验证条款 SHALL 被降级登记且**不进**本 Requirement

- **WHEN** 核对 `ADR-CEP-05 §3` 的 10 条契约测试与本 Requirement 的覆盖
- **THEN** 契约测试 #1(`forward_data` 不可分辨性)与 #4(barrier 不可见)SHALL **不**出现在本 Requirement
- **AND** 二者 SHALL 在 design.md 的 Open Questions 中登记「触发条件 = 队列 #6」
- **AND** 理由 SHALL 记录:本仓已发生过「判据长期标『✅ 可自动化』,而该检查器从未实现」的失败模式 → 不可验证的条款写进 spec = **永假的验收条款**

> **锚点**:`test_cep_stub_cache_core.cc` 的「spec 不含 #1/#4」断言(读本 spec 文件核对)

#### Scenario: 本 change SHALL **NOT** 引入协议实例声明

- **WHEN** 扫描本 change 的产物
- **THEN** SHALL **不**存在任何 `status != "planned"` 的协议实例声明
- **AND** 理由 SHALL 记录:协议实例声明的 `status == "implemented"` 是 S1-2 / S1-3 的触发条件(per `ADR-CEP-12` `INV-CEP-12-IC3`);stub cache **不是**协议实例

> **锚点**:`test_cep_stub_cache_core.cc` 的声明文件扫描断言

#### Scenario: stub SHALL **NOT** 注册 23 ABI 且 SHALL **NOT** 进 `ModuleFactory`

- **WHEN** 检查 stub 的集成面
- **THEN** SHALL **不**新增任何 `include/abi/` 下的 ABI 函数(per `ADR-DGPU-08` 的 23 ABI 冻结)
- **AND** SHALL **不**经 `ModuleFactory::registerObject` / `registerModule` 注册(stub 是 **testbench 定位**的组件,由测试夹具装配)
- **AND** SHALL **不**改动任何 frozen header

> **锚点**:`test_cep_stub_cache_core.cc` 的 ABI 与注册面扫描断言
