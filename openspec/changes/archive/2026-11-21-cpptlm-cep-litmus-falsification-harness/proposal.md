# cpptlm-cep-litmus-falsification-harness

> **状态**: 📋 Proposed（2026-11-21）
> **类型**: 测试工装 + 契约闸（`include/tlm/cep/litmus_falsifier.hh` · `test/` · `scripts/test/`）· **零生产代码改动**
> **登记册对应**: §6.4 队列 2 的 **I-6a**（`INV-CEP-13-IC1` 的证据来源）· 实施路径 §6.9 阶段 ②
> **KPI**: 占 **1** slot（active 0/3 → 1/3 ⇒ 归档后回 0/3）
> **delta spec**: `cep-litmus-falsification`（新增 1 个 capability）

## Why

`ADR-CEP-13` 决策 5 ① 承诺「**判据可复用** —— 把 5 条组合判据（唯一序列化点 / E 态静默升级 / **复合一致性公理** / 全局可见写判据 / proxy-cache 模板）……」，
决策 5 ② 承诺「**逐组合实验证伪** —— 正确性由**随机 / litmus 实验在声明的 `verification_scope` 内尝试证伪**」。

⚠️ **实测缺口（2026-11-21）**：

| # | 缺口 | 实测证据 |
|:--:|---|---|
| **W-1** | 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` **无机器面** | `include/tlm/cep/channel_dependency_graph.hh` 的装配期闸**逐实例建图**且边**不跨网络** ⇒ 复合图 ≡ 各网络子图的并 ⇒ 「复合」与「逐网络」**当前等价** ⇒ 公理形式**未被真正检查** |
| **W-2** | 边类 `rfe` / `fr` / `ws` **两仓均无定义** | CppTLM 与 ArchForge 全仓 grep 仅命中公理公式本身（`ADR-SOC-25` D6 引 `ADR-CEP-13` 决策 2）⇒ **无法**在不发明语义的前提下构造这些边 |
| **W-3** | `verification_scope` **不是已声明字段** | 全仓 grep 仅命中 `openspec/specs/cep-core-contracts/spec.md` 的散文（`verification_mode`）⇒ 决策 5 ② 的「在声明的 `verification_scope` 内」当前**无声明载体** |
| **W-4** | 仓内**零 litmus 载体** | `find test scripts examples -iname "*litmus*"` → **0 命中** ⇒ 决策 5 ② 的「litmus 实验」**从未落地** |

⇒ `INV-CEP-13-IC1` 的登记缺口（「『采用该公理为判据』**无机器可检声明项**……**运行期待 litmus**」）由此可追。

## What Changes

| # | 动作 | 落点 |
|:--:|---|---|
| **G1** | 新增**证伪工装**：把已声明的逐网络通道依赖边 + **语料里的跨域边**合成**复合图**，搜索环 ⇒ 尝试证伪 | `include/tlm/cep/litmus_falsifier.hh`（header-only · 判定在 `test/` 侧）|
| **G2** | 新增**语料**（当前**空**）+ 其 `_status` 写明「边类无定义 ⇒ 复合公理当前**不可证伪**」 | `test/cep_litmus_corpus.json` |
| **G3** | 新增判据 4 例（诚实性 / 仓内正控 / 跨域环反 fixture / 端点未绑定不自证） | `test/test_cep_litmus_falsification.cc` |
| **G4** | 新增**闸**（两条臂：正控诚实性 + 反向 fixture 有鉴别力） | `scripts/test/cep_litmus_falsification_check.sh` · ctest 注册 |
| **G5** | 新增 capability 契约（delta spec） | `specs/cep-litmus-falsification/spec.md` |

## 明确不做（⛔ 范围纪律）

- ⛔ **不发明边类语义** —— `rfe`/`fr`/`ws` 当前无定义 ⇒ 语料**保持为空**，工装只**记录**边类不解释
- ⛔ **不判定公理成立** —— 无环 ⇒ 「本次语料范围内**未证伪**」，⛔ 不得表述为「已证明」（per 决策 5 ②「平台不提供无违例保证」）
- ⛔ **不改** `protocol_instance.schema.json`（冻结面 · 属 I-6 的 schema 13→14）
- ⛔ **不改** `protocol_instances.json` 的既有声明 · **不改**任何已签批 ADR 正文
- ⛔ **不新增 `verification_scope` 字段**（属 I-6 的结构化声明项）
- ⛔ **不改** `channel_dependency_graph.hh`（既有装配期闸语义**零变化**）
