# cep-canonical-flow-parser-scope Specification

## Purpose
定义 CP-02「canonical 10 流零 diff」判据的**解析范围契约**:
`parse_canonical_flow_names` SHALL **只解析** `ADR-CEP-02` 的 `## 2.` 段
⇒ 权威 ADR 的**合法追加**(`## Status Update` / 附录 / 其它章节的表格)
SHALL NOT 影响 canonical 流清单的解析结果。

> **口径已裁决**(台账 `obj-cep-platform.md` **Entry #149** + **#142** + **#141**)⇒ 本能力**不重推**语义,
> 只把**已裁决**的修法**机器化**。
>
> **边界**:不改 `ADR-CEP-02` / `ADR-CEP-01` 正文 · 不改 canonical 10 流**清单** ·
> 不改 `collective_plane_contract.hh` 的**其余判据** · **0 新增** ABI · **0 新增** invariant 编号数 ·
> **不产出任何性能结论**。
>
> **性质**:本能力修的是**误报**(判据惩罚了 ADR 的合法追加)⇒ 判据形态与既有
> 「毒化 ⇒ 必红」的 reverse_verify **相反** = 「**毒化 ⇒ 仍绿**」(见 `design.md` §5)。

---

## Requirements

### Requirement: cep-canonical-flow-parser-scope

CEP SHALL 将 CP-02 的 canonical 流清单解析范围**限定**于 `ADR-CEP-02` 的 `## 2.` 段,
并以**合成 ADR 文本**用例证明:权威源的合法追加**不**改变解析结果。

#### Scenario: CFP-01 —— 解析范围 SHALL 限定于 `## 2.` 段

- **WHEN** 对一段含 `## 2.` 段的合成 ADR 文本调用 `parse_canonical_flow_names`
- **THEN** 解析结果 SHALL **只**来自 `## 2.` 段内的表格行
- **AND** `## 2.` 段**之外**的任何表格行 SHALL **不**进入解析结果
- **AND** 该性质 SHALL 由**合成文本用例**覆盖(**不修改**真实 ADR 文件)

> **锚点**:`test/test_cep_collective_plane_contract.cc` 的解析范围用例

#### Scenario: CFP-02 —— 权威源的合法追加 SHALL NOT 影响解析结果

- **WHEN** 在合成 ADR 文本的 `## Status Update` 段内追加表格行
  `| 11 | `fake_flow` | … |`(首格 ASCII 数字 + 第二格**新**标识符)
- **THEN** 解析结果 SHALL 仍为 **10** 条
- **AND** 解析结果 SHALL **不含** `fake_flow`
- **AND** 同理,`## 3.` 之后(§3 ~ §7 区段内)追加 `| 12 | `fake_flow2` | … |` 时 SHALL 同样得 10 条且不含 `fake_flow2`

> **锚点**:`test/test_cep_collective_plane_contract.cc` 的「SU / §3+ 注入」用例
> (⚠️ 这两条是**回归防护**(毒化 ⇒ 仍绿),**不是**鉴别力用例)

#### Scenario: CFP-03 —— 权威源结构异常 SHALL **安全失败**

- **WHEN** 合成 ADR 文本**不含** `## 2.` 段
- **THEN** 解析结果 SHALL 为 **0** 条
- **AND** `evaluate_canonical_flows` SHALL 返回 **FAIL**
- **AND** detail SHALL 含有「**权威源格式已变**」的提示
- **AND** 本行为 SHALL **不得**返回 PASS(「无可检查对象」≠「通过」)

> **锚点**:`test/test_cep_collective_plane_contract.cc` 的「结构异常」用例

#### Scenario: CFP-04 —— 真实 `ADR-CEP-02` SHALL **零回归**

- **WHEN** 读取真实的 `docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md`
- **THEN** 解析结果 SHALL 为 **恰好 10** 条
- **AND** 10 个 canonical 流名(`request` / `completion` / `snoop` / `snoop_resp` / `writeback` /
  `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`)
  SHALL **各出现恰好 1 次**
- **AND** `evaluate_canonical_flows`(对真实 collective 头)SHALL 返回 **PASS**

> **锚点**:`test/test_cep_collective_plane_contract.cc` 的既有「canonical 10 流零 diff」用例
> (⚠️ 本 Scenario 的断言**零改动** —— 既有断言即已覆盖;本 change 只**新增** CFP-01 ~ CFP-03 的用例)
