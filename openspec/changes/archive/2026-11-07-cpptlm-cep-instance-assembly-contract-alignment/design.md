# design —— 契约对齐 + 判据完备性

## D-1 为什么走 MODIFIED 而不是新 requirement

`cep-instance-assembly` 是 P2 归档时合并进 main spec 的**冻结契约**;P3 的修正**改的是同一判据的语义** ⇒ 必须 MODIFIED 该 Requirement(整段重写),⛔ 不能另起一个 requirement(否则同一判据出现两套文本 ⇒ 正是 critic 指出的「同一判据二义」)。

## D-2 「required 清单」的权威归属

闸**不自行推断**「哪个实例应当启用 collective」—— 那是**协议语义**判断,归 ADR/设计文档;闸只接受调用方显式传入的清单。
⇒ 契约中钉死「当前唯一权威调用点 = `test_cep_instance_assembly.cc` 的 IAG-01 正控(`ualink_4` required)与 `test_cep_moesi_gpu_engine.cc` 的 MGE-05(`moesi_gpu` 不 required)」⇒ 消除「判据可被调用方静默稀释」的制度面隐患。

## D-3 L4 两档(rubric 修订)

原 rubric 只定义「L4 = wire 真穿过 + tick + 对端」,而 MOESI-GPU / TileLink-C 的实际判据是**进程内派发 + 引擎内记账** ⇒ 把 ✅ 标在 L4 列 = **层级夸大**。
⇒ 拆成 **L4-direct**(进程内派发)/ **L4-wire**(wire 真穿过);MOESI-GPU 标 ◐(direct ✅ / wire ❌)。
⚠️ TileLink-C 行属**更早归档**的 Entry #246 ⇒ **不改**(避免回改历史记录)⇒ 登记 backlog。
