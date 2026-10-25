# cep-vc-mapping-contract Specification

## Purpose
TBD - created by archiving change cpptlm-cep-stub-cache-core. Update Purpose after archive.

## Requirements

### Requirement: cep-vc-mapping-contract

CEP SHALL 将**传输层 flit 类别 → 物理 VC 桶**的映射关系、每个桶的**当前流量实况**、以及**冲突输入的归属规则**固化为**可回归的契约** —— 使其不只存在于注释,也不被后续 change 的新断言**静默固化**为错误语义。

> **定位**:传输层映射(dGPU SoC 内部 NoC 物理桶),**非**一致性协议的 per-network VC 分配(后者归 `vc_binding`,per ArchForge `32 号 §6.0.2`)。
> **分层**:`CATEGORY_*` 是**传输层**类别(3 类);6 类词表落 Coherence Interface 层(per `ADR-CEP-12 §8` IC2 ③)—— 二者**正交**。

#### Scenario: 传输层 `CATEGORY_*` → VC 桶的映射 SHALL 保持值域连续(索引对齐)

- **WHEN** 读取 `include/bundles/noc_bundles_tlm.hh` 的 `CATEGORY_*` 声明与 `include/tlm/cluster/soc_internal_noc.hh` 的 `VC_*` 声明
- **THEN** `CATEGORY_*` SHALL **恰为 3 个**声明,取值 SHALL 为连续 `0,1,2`
- **AND** `CATEGORY_REQUEST` SHALL 落 `VC_REQ`、`CATEGORY_RESPONSE` SHALL 落 `VC_RSP`、`CATEGORY_CREDIT` SHALL 落 `VC_DAT`
- **AND** 该连续性 SHALL 使**任何** `CATEGORY_*` 都**不会**被 `route_flit()` 的越界守卫 `(cat < VC_COUNT) ? cat : VC_SNP` **静默折叠**
- **AND** 本仓 SHALL **不**引入 `CATEGORY_*` → VC 的映射表(3 类恒等映射下映射表与索引对齐**等价**;显式映射表的归属见「显式映射表不属于本 capability」Scenario)

> **锚点**:`cep_docs_sync_check.sh` 第 10 组断言 ①(值域连续 0..2)+ `test_cep_vc_mapping_contract.cc` 的值域连续性断言

#### Scenario: 每个 VC 桶的**流量实况** SHALL 可断言

- **WHEN** 对 `SocInternalNoc` 施加 `CATEGORY_REQUEST` / `CATEGORY_RESPONSE` / `CATEGORY_CREDIT` 三类 flit
- **THEN** `vc_req_count()` SHALL 随 `CATEGORY_REQUEST` 增加
- **AND** `vc_rsp_count()` SHALL 随 `CATEGORY_RESPONSE` 增加
- **AND** `vc_dat_count()` SHALL 随 `CATEGORY_CREDIT` 增加
- **AND** `vc_snp_count()` SHALL 保持 **0**(当前无 `CATEGORY_*` 落入该桶 —— 它是**预留**桶)
- **AND** 断言 SHALL 针对**计数变化**而非「常量存在」

> **锚点**:`test_cep_vc_mapping_contract.cc` 的三桶计数断言(REQ / RSP / DAT)

#### Scenario: 零流量桶的「恒 0」SHALL 有**正控**

- **WHEN** 未向 `SocInternalNoc` 注入任何 flit
- **THEN** `vc_dat_count()` 与 `vc_snp_count()` SHALL 均为 **0**
- **AND** **若**仅有上述负向断言而无正控 → 本 Scenario 失败(「恒 0」在**判据退化**时仍会绿 → 无牙)
- **AND** 正控形态 SHALL 为:注入一个 `CATEGORY_CREDIT` flit → `vc_dat_count()` SHALL 从 **0 变为 1**
- **AND** 该正控 SHALL 存在且 SHALL **不**被删除

> **锚点**:`test_cep_vc_mapping_contract.cc` 的正控断言(注入 `CATEGORY_CREDIT` → `vc_dat_count()` 0→1)

#### Scenario: `vc_id` 与 `flit_category` 冲突时的归属规则 SHALL 显式

- **WHEN** 构造一个 `vc_id = VC_DAT` 且 `flit_category = CATEGORY_REQUEST` 的 flit
- **THEN** **计数归属** SHALL 为 `flit_category` —— 该 flit SHALL 计入 `vc_req_count()`,**不**计入 `vc_dat_count()`
- **AND** **编码归属** SHALL 为 `vc_id` —— `chip_select = (vc << 4) | src` SHALL 用 `vc_id` 的值参与计算
- **AND** 该规则 SHALL 在 `include/tlm/cluster/soc_internal_noc.hh` 的注释中**显式陈述**(供下一个读者直接引用)

> **锚点**:`test_cep_vc_mapping_contract.cc` 的冲突输入断言 + `cep_docs_sync_check.sh` 第 10 组断言 ⑤

#### Scenario: 传输层 SHALL **NOT** 仲裁 `vc_id` 与 `flit_category` 的冲突

- **WHEN** 上述冲突输入被送入 `route_flit()`
- **THEN** 传输层 SHALL **不**报错、**不**纠正、**不**丢弃该 flit
- **AND** 理由 SHALL 记录:仲裁需要**一致性词表**知识(6 类消息类 ↔ `CATEGORY_*`),那是 **adapter 层**的职责(per `ADR-CEP-12 §8` IC2 ③ 分层纪律;显式映射表归队列 #6)
- **AND** 传输层**新增**该校验 SHALL 被视为**越权**(把协议层语义泄漏进物理层)

> **锚点**:`test_cep_vc_mapping_contract.cc` 的「不报错 / 不纠正」断言

#### Scenario: `VC_DAT` 的语义描述 SHALL **NOT** 声称「统计 credit 流量」

- **WHEN** 读取 `include/tlm/cluster/soc_internal_noc.hh` 中 `VC_DAT` 与 `vc_dat_count()` 的说明
- **THEN** 该说明 SHALL **不得**出现「`vc_dat_count()` 实际统计的是 credit 流量」这类**描述从未发生之流量**的措辞
- **AND** 该说明 SHALL 陈述:该桶是**预留 DAT 桶**,当前**无** `CATEGORY_*` 数据类,`CATEGORY_CREDIT` 落此桶是**索引对齐的副产物**
- **AND** 同理,`test/test_soc_internal_noc_data_flow.cc` SHALL **不得**保留「但 `vc_counts_[2]` (DAT VC) 仍增」这类**陈旧注释** —— 该用例实际写 `CATEGORY_REQUEST` 且**只**断言 `flits_dispatched()` / `unknown_dst_count()`

> **锚点**:`cep_docs_sync_check.sh` 第 10 组断言 ④(禁止「统计 credit 流量」措辞)与 ⑥(禁止陈旧注释)

#### Scenario: `CATEGORY_CREDIT` SHALL **NOT** 被表述为独占 VC

- **WHEN** 读取本仓任何描述 `CATEGORY_CREDIT` 与 VC 关系的文本
- **THEN** 该文本 SHALL 与 ArchForge `32 号 §6.0.1`「明确排除」表一致:`CATEGORY_CREDIT` 是**流控类别**,非一致性消息类,**credit 随所在 VC 的流一起走,不单独占 VC**
- **AND** 本仓 SHALL **不**重新裁决该问题(它**已**被跨仓权威关闭)

> **锚点**:`cep_docs_sync_check.sh` 第 10 组断言 ②(幽灵类别防护)+ `test_cep_vc_mapping_contract.cc` 的权威引用断言

#### Scenario: 显式映射表(6 类消息类 → `CATEGORY_*`)**不属于**本 capability

- **WHEN** 检查本仓是否存在「6 类一致性消息类 → `CATEGORY_*`」的映射表
- **THEN** 在本 change 交付前 SHALL **不存在**该表
- **AND** 该表的归属 SHALL 为**队列 #6**(`cpptlm-cep-chi-rnf-subset`,S1-5 表体 + contract test)
- **AND** 本仓 SHALL **不**在传输层另建等价的重复映射

> **锚点**:`cep_docs_sync_check.sh` 第 10 组断言 ①(恰 3 类 —— 映射表若落地必然扩类)
