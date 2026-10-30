# 队列 #9 `cpptlm-cep-mesif-adapter` 归档快照(2026-10-30)

> **性质**:时点快照(append-only)。**不改**历史快照;旧证据失效时由台账补注。
> **依据**:`obj-cep-platform §4.7.3` 收尾清单 D3 + 台账 Entry #105 / #106。

## 1. 交付物(6 项)

| # | 载体 | 状态 |
|---|------|------|
| A | `include/tlm/cep/mesif_adapter.hh` | ✅ 新(零自有表副本,委托消费 `MesifStateSpec`) |
| B | `include/tlm/cep/sharer_tracker.hh` | ✅ 改(`SnoopMechanics` 抽取 + `HierarchicalDirectory` 新增) |
| C | `include/tlm/cep/mesif_protocol_engine.hh` + `configs/cep_mesif_pe_minimal.json` + 注册行 | ✅ 新 |
| D | `include/tlm/cep/perm_upgrades.json` + `protocol_instances.json` | ✅ 改(静默升级行 + `status=implemented`) |
| E | 4 个测试文件 + `scripts/test/cep_mesif_adapter_reverse_verify.sh` | ✅ 新 |
| F | `openspec/specs/{cep-mesif-adapter,cep-hierarchical-directory}/` | ✅ 归档时合并 |

## 2. 判据实测(归档时)

| 项 | 数值 |
|---|---|
| `[cep]` | **247 case / 4190 assertion** |
| 全仓 `cpptlm_tests` | **2029 case / 874636 assertion** |
| `ctest -R cep` | **40/40 passed** |
| 反向验证(新) | **16 条毒化 + 基线 2 项,18/18** |
| 反向验证(既有 MESIF 规约) | **11/11**(零回归) |
| 13 项 CEP 脚本 + `docs_sync --strict` + `openspec validate --strict` + `format --check` | 全 **rc=0** |

## 3. 残余风险(未消除,带标记)

- **R-1**:按档批量 14 格证据强度弱于逐格(归纳 vs 枚举)。
- **R-3**:「已写回(干净)」与「仍脏」在 4 值 `perm` 上同值不可分辨 ⇒ 补偿 = `LocalView.dirty`
  掩码 + **写回接线**(AC-7 使其可观测;断接线 ⇒ 必红)。
- **残留 A**:`HierarchicalDirectory` 行为语义**无真源** ⇒ `kBehaviorDecisions[5]` 逐条 `basis`
  + `pending` + **跨仓追认债 #1**。
- **残留 C**:`atomic` 的 `consistency_intent` **无独立字段** ⇒ 显式登记 + **跨仓债 #2**。
- **引擎决策 E-1 ~ E-6** ⇒ **跨仓债 #3**(design `D-λ`)。

## 4. 冻结面

两件(`07 号` + `openspec/specs/cep-sharer-tracker-base/spec.md`),同批**两次** `--update`
(`authorized-by` 最终为 Entry #106;第二次为承载 S1-3 重评结论的第十六则)。
