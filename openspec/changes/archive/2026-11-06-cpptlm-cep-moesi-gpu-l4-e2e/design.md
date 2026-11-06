# design —— MOESI-GPU L4 E2E

## D-1 「10 流」在 CohIf 边界上只有 5 条

`bundles::CohIfFlowId` 只有 **5** 个取值(`kCohIfFlowCount = 5`)⇒ canonical 10 流中**只有 5 条**跨 Stub↔PE 边界 ⇒ 其余 5 条(`ForwardData` / `BackInvalidate` / `AddressResolve` / `Barrier` / `SnoopCompletionAck`)**不能**经 `dispatch_cohif` 派发 ⇒ 必须**直调** `on_*`(显式落点,per `protocol_engine_base.hh` 的 10 流语义契约)⇒ 判据覆盖「5 + 5」两路,断言 `flow_events[0..9]` 各为 1。

## D-2 「抵达」在本引擎的语义 = **记账 + 状态机**

引擎的 10 个 `on_*` 按决策 **M-6「只记账」**(零发射)⇒ 「抵达」的可判形态 = `flow_events[]` 计数 + `state_of(addr)` 状态机 + `adapter_transitions` 迁移计数;⛔ **不**假装有跨节点传输语义(那需要专用引擎)⇒ 契约文本显式写明该边界。

## D-3 补偿被拒 ⇒ 迁移**未发生** ⇒ 地址**未被登记**

`submit_local_request` 在补偿被拒时 `return from;` 且**不写** `lines_` ⇒ `state_of(addr)` 返回默认 `I`(而非 `O`)⇒ 判据断言 `line_count() == 0` + `state_of == I` ⇒ 锁住「拒绝 ⇒ 无状态记录」这条语义(⛔ 不断言成「停在 O」)。
