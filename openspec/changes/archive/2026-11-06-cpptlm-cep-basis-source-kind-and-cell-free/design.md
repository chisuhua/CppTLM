# design — 来源类型可机器判 + `CellKind::Free`

## D-1 双载体真源：JSON(人审) + 头文件(编译期)

`sources.json` 是**人类可审计真源**(带 `sha256` / `retrieved` / `license`);`source_registry.hh` 的 `kSources[]` 是**编译期查找表**。二者由测试 `test_cep_source_registry.cc` **逐条比对**(id / kind / title),漂移 ⇒ 红。

- 理由:纯 JSON 无法编译期判定;纯头文件不可审计(无许可/哈希)。
- 风险:双写漂移 ⇒ 由测试兜底(AC-1)。

## D-2 `resolve_ref` 逐载体特化，⛔ 不按 token 名统一

4 套 basis 枚举语义**不同**,统一映射会错:

| 载体 | `resolve_ref` 规则 | `basis_ref_kind_consistent` |
|---|---|---|
| tilelink 主矩阵 | `TLC-*` ⇒ `tilelink-c`(SpecClause);空 ⇒ `repo` | `Direct` ⇒ SpecClause;`Analogical`/`Inferred` ⇒ 非 SpecClause |
| tilelink T3 | 非空(D1 §…) ⇒ `tilelink-c` | `Direct` ⇒ SpecClause;`SelfDerived`/`Pending` ⇒ RepoDerived |
| cxl | 前缀 `D1` ⇒ `cxl-d1`(PublicNonnormative);空 ⇒ `repo` | `PublicDirect` ⇒ PublicNonnormative;`ArchforgeAnchor`/`SelfDerived`/`PublicAnalogical` ⇒ RepoDerived |
| mesif | 非空(`32 号:…`) ⇒ `archforge`(RepoDerived) | `Primary` ⇒ RepoDerived **且 ref 非空**;其余 ⇒ RepoDerived |
| ualink | 含 `N-` ⇒ `ualink-pending`;含 `C-` ⇒ `ualink-c10` | `Pending` ⇒ PublicNonnormative;`SelfDerived` ⇒ RepoDerived |

⚠️ **`cxl::PublicDirect` 映射到 `PublicNonnormative` 而非 `SpecClause`** —— 其枚举注释即「公开来源明确这样写」,不是规范正文。这正是 P1 的价值:**不能按 token 名 `Direct` 跨载体套用**。

## D-3 `CellKind::Free` 语义

- `Known`:规范明确转移(目标确定/集合);
- **`Free`**:规范**明确**「实现定义 / 自由选择」⇒ 消费者 SHALL 走**合法**降级,⛔ 不得当 `Illegal`,⛔ 不得当 `Known` 的确定转移;
- `Unknown`:规范未覆盖 ⇒ 降级;
- `Illegal`(仅 cxl):规范明确不合法。

当前真实 `Free` 格 = **0**(规范可读化后填充)。AC-3 判据 = 枚举 + 四态互斥断言 + `transition_is_free` 显式分支 + 零计数自洽,⛔ 不造数据。

## D-4 决定性变异形态(影子树,照 `cep_tilelink_engine_failclosed.sh` 先例)

| 态 | 变异 | 期望 |
|---|---|---|
| clean | 影子 = 真源 | 测试/探针 PASS |
| dropped | 影子 `sources.json` 删 `cxl-d1` | 红,签名 = 未登记 source id |
| downgraded | 影子头把 `tilelink-c.kind` 改 `PublicNonnormative` | 编译/测试红,签名 = basis ↔ ref_kind 不一致 |

收尾断言真源 sha256 未变。

## D-5 零冻结面 / 零引擎逻辑

- `fabric.json` / `protocol_instance.schema.json` / `protocol_instances.json` 零 diff;
- 引擎 `*_protocol_engine.hh` 零改动;`Free` 只在 adapter 查询面新增分支。
