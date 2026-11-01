# TileLink-C 规约载体 —— 锚点核对表(机器生成)

> **本文件由** `scripts/test/generate_tilelink_anchor_checklist.sh` **生成** —— 手工编辑会被覆盖。
> **配套**: `include/tlm/cep/tilelink_state_spec.hh`(规范性载体)
> · `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md` §2.3bis(basis 唯一人工审计面)
> · `openspec/changes/cpptlm-cep-tilelink-state-spec/`(change 4 件套)
> · 台账 `obj-cep-platform.md` Entry #140(change 建档)· #141(二审落修 批 2)
>
> **用途**:逐格核对载体 ↔ 锚点摘要。生成器做**逐格**交叉核对(条目号一一对应 +
> 目标掩码 + 分桶)⇒ **手改任一侧 ⇒ 必须因正确原因红**。
> ⚠️ **ID 文法**:`TLC-<n>` 前缀 + 全数字后缀,**容忍 Markdown 粗体**
> (⚠️ 照抄 CXL 版的「`prefix` + 全数字」且不处理粗体 ⇒ 合法集为**空** ⇒ 恒真空转)。

## 12 格逐格核对表

<!-- TL-TABLE:MATRIX:START -->
| 条目号 | 事件 | 源态 | 目标掩码 | 分桶 | 载体 `ref` |
|---|---|---|---|---|---|
| `TLC-01` | `Cap` | `Nothing` | `0b111` | `multi` | `TLC-01` |
| `TLC-02` | `Cap` | `Branch` | `0b111` | `multi` | `TLC-02` |
| `TLC-03` | `Cap` | `Tip` | `0b111` | `multi` | `TLC-03` |
| `TLC-04` | `Grow` | `Nothing` | `0b110` | `multi` | `TLC-04` |
| `TLC-05` | `Grow` | `Branch` | `0b100` | `single` | `TLC-05` |
| `TLC-06` | `Grow` | `Tip` | `0b000` | `empty` | `TLC-06` |
| `TLC-07` | `Prune` | `Nothing` | `0b000` | `empty` | `TLC-07` |
| `TLC-08` | `Prune` | `Branch` | `0b001` | `single` | `TLC-08` |
| `TLC-09` | `Prune` | `Tip` | `0b011` | `multi` | `TLC-09` |
| `TLC-10` | `Report` | `Nothing` | `0b001` | `single` | `TLC-10` |
| `TLC-11` | `Report` | `Branch` | `0b010` | `single` | `TLC-11` |
| `TLC-12` | `Report` | `Tip` | `0b100` | `single` | `TLC-12` |
<!-- TL-TABLE:MATRIX:END -->

## 读法

| 列 | 含义 |
|---|---|
| 条目号 | 锚点摘要 §2.3bis 的**唯一条目号**(载体 `basis.ref` 的目标) |
| 事件 / 源态 | 矩阵坐标(event-major × state-minor) |
| 目标掩码 | 3 位:bit0 = `Nothing` · bit1 = `Branch` · bit2 = `Tip` |
| 分桶 | `single`(1 位)· `multi`(≥2 位)· `empty`(0 位,**规范明确**空集)· `unknown`(规范未覆盖) |
