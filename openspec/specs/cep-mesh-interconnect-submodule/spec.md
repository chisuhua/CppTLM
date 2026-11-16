# cep-mesh-interconnect-submodule Specification

## Purpose
让互联拓扑从业务拓扑中独立出来：把 router 与链路收进一个可复用的容器（`CepMeshInterconnect`），由独立蓝图文件承载互联件、由主拓扑只保留业务模块与跨层接线，从而使同一份互联蓝图可被不同主拓扑复用、同一容器类可换蓝图装配不同形态，且改造前后业务事务的 mesh 级行为不变。

## Requirements

### Requirement: cep-mesh-interconnect-submodule

系统 SHALL 提供 **`CepMeshInterconnect`** —— 互联拓扑的容器型 SimModule，使互联件的**模块与内部连接**由一份独立蓝图文件承载，业务模块只在主拓扑里出现。

#### Scenario: 互联蓝图 SHALL 由独立文件承载

- **WHEN** 主拓扑声明 `CepMeshInterconnect` 且 `params.template` 指向一份蓝图
- **THEN** 该蓝图 SHALL 提供 `modules` 数组（互联件：router / link）
- **AND** 蓝图 SHALL **NOT** 含 `outputs` / `inputs`（端口暴露归主拓扑）
- **AND** 蓝图 SHALL **NOT** 含业务模块（NIC / coherence / 地址映射）
- **AND** 蓝图缺 `modules` 时 SHALL 显式抛错（⛔ 不得静默空装配）

#### Scenario: 互联层 SHALL 在独立命名空间内

- **WHEN** 读取 `CepMeshInterconnect` 的内部实例
- **THEN** 内部模块 SHALL 可经 `<容器名>.<内部名>` 访问
- **AND** 内部模块名 SHALL **不**与主拓扑的业务模块名冲突

#### Scenario: 端口暴露 SHALL 走通用机制而非容器专用代码

- **WHEN** 主拓扑声明 `outputs` / `inputs`
- **THEN** 其 `internal` SHALL 为 `<内部模块名>.<端口名>`（端口名可带索引，如 `resp_out[4]`）
- **AND** 容器 SHALL **NOT** 提供任何端口暴露专用代码
- **AND** 接入点数 SHALL 由主拓扑的暴露声明数量决定

#### Scenario: 同一蓝图 SHALL 可为不同主拓扑复用

- **WHEN** 两个主拓扑引用同一份蓝图
- **THEN** 各自装配出的内部模块数与连接数 SHALL 与蓝图逐项相等

#### Scenario: 换互联形态 SHALL 只改蓝图引用

- **WHEN** 将 `params.template` 换成另一份蓝图
- **THEN** 主拓扑 SHALL **NOT** 需要修改其 `modules` 或 `connections` 中的业务部分

#### Scenario: 容器 SHALL NOT 承载业务语义

- **WHEN** 检视 `CepMeshInterconnect` 的实现
- **THEN** 其 SHALL **NOT** 含 NIC / coherence 协议 / 地址映射任一职责
- **AND** 其职责 SHALL 限于「加载蓝图 + 交由内部工厂装配 + 转发周期推进」
