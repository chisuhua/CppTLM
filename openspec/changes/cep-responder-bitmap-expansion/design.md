# design: cep-responder-bitmap-expansion（ADR-CEP-21 阶段 1）

> **范围**:仅阶段 1（机制化）· 6 项 · 3 文件 · **运行时行为不变**
> **依据**:`docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md`（🟢 Proposed）· improvement v2（metis 12 条全采纳）· 台账 Entry #423

## D-1 · 限定名：用 `tlm::cep::` 全限定，不用 `using`

**决策**：W1/W2 一律写 `tlm::cep::kResponderMappedBits`。

**理由**（metis P0-1 的实测）：
- `test/cep_directory_experiment.hh:28` 是 `namespace cep_exp`；其 `using` 列表（`:30-36`）**没有** `namespace cep = tlm::cep;`
- ⇒ 写 `cep::kResponderMappedBits` **编译失败**
- ⇒ 两个选择：① 在 `using` 列表加 `namespace cep = tlm::cep;`（缩窄为一条别名）② 题内全限定
- **选 ②**：该文件已用全限定风格（`:173` `tlm::cep::snoop_handle_t`）⇒ 保持一致，且不引入新别名

**代价**：`mapped_prefix_mask` 的 `if` 行变长（`static_cast<std::uint64_t>(tlm::cep::kResponderMappedBits)`）⇒ 可读性略降，但 clang-format 可控。

## D-2 · 不用 `std::min`（偏离 ADR S1-1② 的字面）

**决策**：W2 用 `if (n > K) n = static_cast<std::uint64_t>(K);`，**不用** `std::min<std::uint64_t>(n, K)`。

**理由**：
- `std::min` 需 `#include <algorithm>`（**新增依赖**）+ 显式模板实参 `std::min<std::uint64_t>` 与 `std::size_t` 实参并存 ⇒ 需要 `static_cast` 才能编译（否则模板推导失败）
- 两行 `if` 零新依赖、零模板实参、语义完全等价
- ⇒ **属对 ADR 字面的偏离，已在 improvement 的 D-3 登记**

**代价**：与 ADR 文本不完全一致 ⇒ 读者须知道「ADR 写了 `std::min`，实现用了 `if`」⇒ design 与 improvement 双重登记。

## D-3 · `broadcast_fanout` 的**值**保留 4

**决策**：W3 只改注释，**值仍是 4**。

**理由**（metis P0-2 的实测）：
- `broadcast_fanout` 是 `ServiceConfig` 的**实验参数**（`:47`）⇒ 语义 = 「默认探前 N 个节点」⇒ 绑到 `kResponderMappedBits` 会**失去独立可调性**
- ⚠️ **关键**：唯一调用方（`:107`/`:139`）传的就是 `cfg_.broadcast_fanout`，且 `ServiceConfig` **从未被 override** ⇒ 若把值绑到常量，阶段 2 扩容会把 broadcast latency 从 `4+4*2+6=18` 改到 `8+8*2+6=30` ⇒ **破坏与 `experimental/` 已冻结 run 的可比性**
- ⇒ **代价（已如实登记）**：M1/M2 的「今日后果」保持零影响 ⇒ W1/W2 的收益**只在扩容后**兑现（⛔ 不得声称「消除今日假绿」）

**替代方案（已否决）**：值 = `kResponderMappedBits` ⇒ 见上，破坏基线可比性。

## D-4 · W5 的判别力边界（**诚实登记**）

**决策**：新增满射判据，但**显式标注「今日无增量判别力」**。

**理由**：
- `kResponderMappedBits = 4` 时，满射判据遍历 bit 0..3 ⇒ 与既有 `:211-219` 的 4 个具体 node 断言**等价** ⇒ **今日零增量**
- 它的价值**只在 `kResponderMappedBits > 4` 时**兑现（届时 `:214-219` 的 initializer list 仍只测 4 个）
- ⇒ 按 ADR 自己反对「死判据」的标准，**必须**写明这一点，⛔ 不得计入今日价值

**⚠️ W5 不替代既有断言**：满射只证**双射自洽**（`b → node → b`），**不钉 bit→具体 node** ⇒ 置换两个表项仍绿 ⇒ `:211-212` 与 `:214-219` **必须保留**。

## D-5 · `static_assert(kResponderMappedBits <= 64)` 替代运行期守卫

**决策**：在 `sharer_tracker.hh` 加编译期断言。

**理由**（metis 的 R-2 订正）：
- improvement 原稿写「`n == 0` 短路是 correctness 必需」⇒ **错误**（`(1<<0)-1 = 0`，短路仅为可读性）
- 真正的 UB 风险是 **`n >= 64`** 时的 `1 << n`
- `mapped_prefix_mask` **没有** `kMappedResponderMask`（`:125-127`）那样的 `>= 64` 三元守卫
- ⇒ 加 `static_assert(kResponderMappedBits <= 64)` ⇒ **把守卫从运行期上移到编译期**（扩容到 65+ 直接编译失败，而非运行期 UB）

## D-6 · AC-7 的变异结果（**比预期更丰富，实证 ADR 否决理由**）

**实测**：把 `kResponderMappedBits` 4→5 而 **不填** `kResponderBitToNode[4]` ⇒ **3 个 test case 失败**：

| 断言 | 结果 | 含义 |
|---|---|---|
| `responder_node_of(4) != kInvalidNode`（**满射判据**） | ❌ 红 | **W5 有牙** ✅ |
| `responder_bit_of(kInvalidNode) == kNoBit` | ❌ `4 == 64` | **sentinel 被破坏** |
| `mask(kInvalidNode) == 0u` | ❌ `16 == 0` | 同上（掩码非 0） |
| `dir.view_of(...)` no-op（`:243`/`:247`） | ❌ 红 | 目录被污染 |
| `test_cep_sparse_snoop_filter.cc:191` · `test_cep_tier2_observation.cc:291` | ❌ 红 | 连带 |

⇒ ⇒ **这组实测正好实证了 ADR-CEP-21 选项 B 的否决理由**（「`kResponderMappedBits = 8` 后 `responder_bit_of(kInvalidNode)` 在 bit 4 命中 ⇒ 返回 `4` 而非 `kNoBit` ⇒ sentinel 语义被破坏」）✅

⇒ ⚠️ **修正 AC-7 的措辞**：⛔ 不写「**只有** W5 红」——实际是「W5 **必红** + sentinel 断言同时红」⇒ 这**更好**（多重防线），但措辞必须准确。

## 零行为变化的论证

| 改动 | 当前常量值 | 等价性 |
|---|:--:|---|
| W1 `bit < 4` → `bit < K` | K = 4 | ✅ 循环次数相同 |
| W2 `if (n > 4) n = 4` → `if (n > K) n = K` | K = 4 | ✅ clamp 结果相同 |
| W3 仅注释 | — | ✅ 无代码变化 |
| W4 注释 + `static_assert` | K = 4 ≤ 64 | ✅ 断言恒真 ⇒ 无代码变化 |
| W5 新增判据 | K = 4 | ✅ 只加断言，不改实现 |

⇒ ⚠️ **数值证据**：`experimental/run-*/metrics.json` 零 diff（AC-8）⇒ 比「重跑实验逐键 diff」便宜且等效。
