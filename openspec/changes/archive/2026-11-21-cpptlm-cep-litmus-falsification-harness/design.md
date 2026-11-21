# design — cpptlm-cep-litmus-falsification-harness

## 设计边界（**最重要的一节**）

本工装**不发明任何语义**。它只做两件事：

1. 把**已声明**的通道依赖边（`protocol_instances.json` 的 `channel_dependency_edges.per_network`）
   与**语料**里的跨域边合成一张**复合图**；
2. 在该图上**搜索环** ⇒ 若找到环 ⇒ 该组合**被证伪**；找不到 ⇒ 「本次语料范围内**未证伪**」。

⚠️ **为什么当前语料是空的**：`rfe` / `fr` / `ws` 三类边在 CppTLM 与 ArchForge **两仓均无定义**
（实测：两仓 grep 仅命中公理公式本身）⇒ **任何**为它们赋予含义的做法都是**发明语义**
⇒ 语料保持为空，工装给出 `reachable == false` 的**真结果**。

⇒ 这不是缺陷，而是**缺口的可机检表达**：当 I-6 的结构化声明项（schema 13→14）落地、
边类有了定义，语料非空 ⇒ 判定自动从「不可证伪」翻转为「已尝试」⇒ **工装有牙**（闸的臂② 用跨域环证伪这一点）。

## 判定形态（⛔ 无「已证明」一档）

| 情形 | `reachable` | `falsified` | 语义 |
|:--:|:--:|:--:|---|
| 语料为空 | `false` | `false` | 复合公理**当前不可证伪**（边类无声明面）⇒ ⛔ 不是「已成立」 |
| 语料非空、无环 | `true` | `false` | 在**本次语料范围内未证伪** ⇒ ⛔ 不是「已证明」 |
| 语料非空、有环 | `true` | `true` | 该组合**被证伪**（环文本逐顶点列出） |

`coverage_note` 恒非空 ⇒ 必须随判定一起读。

## 图构造

- **顶点** = `(instance, network, vc)` —— 与既有装配期闸 `channel_dependency_graph.hh` **同形**（复用其 `ChannelDependencyGraph` / `find_cycle`）
- **边** = ① 既有逐网络边（端点须已绑定）② 语料跨域边（端点须已绑定 ⇒ 未绑定者**不参与**，不得造出假环）
- **边类**（`class`）只**记录**、⛔ 不解释（类定义当前不存在）

## 与既有闸的关系

| 闸 | 判什么 | 与本工装的关系 |
|---|---|---|
| `cep_channel_dependency_gate` | **逐实例**、边**不跨网络**的装配期判据（per `ADR-CEP-09` 死锁自由性） | **语义零变化**；本工装在其之上补**跨域**视角 |
| 本工装 | 复合图上的**证伪尝试** | 新增能力面 |

## 落地形态

| 落点 | 形态 |
|---|---|
| `include/tlm/cep/litmus_falsifier.hh` | header-only · 判定**在 `test/` 侧**（沿用「实现侧只出事实」先例）|
| `test/cep_litmus_corpus.json` | 语料（当前空）· `_status` 写明不可证伪原因 |
| `test/test_cep_litmus_falsification.cc` | 4 例（`[generic]` / `[repo-corpus]` / `[fixture]`×2）|
| `scripts/test/cep_litmus_falsification_check.sh` | 闸（两臂）· ctest 注册 |

## 风险与边界

| # | 风险 | 处置 |
|:--:|---|---|
| 1 | 语料被后人**手写**边类语义 ⇒ 绕过 I-6 的声明项 | 语料 `_fill_when` 写明「由结构化声明项**派生**填充」；⛔ 手写语义属越权 |
| 2 | 有人把「未证伪」写成「已证明」 | 闸的臂① 机读核对产物**不得**出现「已证明 / proved」 |
| 3 | 工装退化成恒绿占位 | 闸的臂② 用**跨域环**证伪 ⇒ 机械必须翻转 ⇒ 有鉴别力 |
