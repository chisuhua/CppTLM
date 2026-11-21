# design: cpptlm-spec-stale-refs-b-class

## D-1 · R-14 / R-15 的形态取舍（(a) 跨仓指针 vs (b) 本仓不承诺）

| # | 形态 | 收益 | 代价 / 风险 |
|:--:|---|---|---|
| **(a)** | **跨仓指针**：`ArchForge:docs/architecture/19-…md`（写跨仓相对路径） | 保留原始「交付意图」的指向性；读者知道去哪找 | ⚠️ **本仓没有「跨仓路径」的解析约定**（`docs_sync_check.sh` 只校验**仓内**反引号路径）⇒ 写 `ArchForge:xxx` 在闸眼里是**字符串**，既不校验也不可达 ⇒ **换一种形态的悬空引用** |
| **(b)** | **本仓不承诺**：「该文档由 **ArchForge 仓**维护；本仓**不重复交付**」（⛔ 不写路径，只写仓名 + 文档名） | 与仓内**既定分工**一致（`AGENTS.md`：「`docs/soc_arch/` 已迁至独立 ArchForge 仓…设计变更 → ArchForge 仓 PR；实现变更 → CppTLM 仓 PR」）;`docs_sync_check.sh` 的 `VIRTUAL_PATHS` 已把 `docs/soc_arch/<sub>/` 当**虚拟路径**处理 ⇒ 本仓口径本就是「不在此处」 | 失去「一行看清路径」的便利（但仓名 + 文档名仍可定位） |

**裁决（builder 定稿）**：**选 (b)** —— 理由三条：

1. **与既有约定同源**（`AGENTS.md` 的 ArchForge 分工 + `docs_sync_check.sh` 的 `VIRTUAL_PATHS` 都已按「本仓不持有 `docs/soc_arch/`」配置）⇒ 选 (a) 会**新造**一套本仓从不解析的跨仓路径语法。
2. **闸可达性**：(a) 的 `ArchForge:xxx` 形态**不被任何闸校验**（既无仓内文件、也无跨仓解析器）⇒ 只是把「找不到的路径」换成「找不到的跨仓字符串」。
3. **语义诚实**：spec 是**要被实现的契约** ⇒ 说清「谁负责」比「给一个本仓解析不了的路径」更可执行。

⚠️ **R-15 的 `SHALL` 必须同时改写**（AC-5）：原句 `The system SHALL 交付 <path>` 若只删路径不改 `SHALL` ⇒ 「SHALL 交付一个不存在且不承诺交付的文件」⇒ **自相矛盾契约**。⇒ 改为：
`The system SHALL NOT 在本仓重复交付 …（由 ArchForge 仓维护）;本仓 SHALL 保证实现与该蓝图一致。`
（⇒ 「不交付」与「保证实现一致」两条 `SHALL/SHALL NOT` 并存，**无矛盾**）

## D-2 · R-12 的等价物交代（AC-2 要求）

**事实（2026-11-21 实测）**：

| 断言 | 证据 |
|---|---|
| `include/cudart/abi_guards.h` **从未在本仓存在** | `ls include/cudart/` → 无输出（目录不存在）|
| 该行声称的「G-D4 17 条静态断言」**在本仓无法取证** | 全仓 `grep -rn "G-D4"` 仅命中该 spec 行本身 |
| 它引用的 `AGENTS.md CROSS-PROJECT` 段**亦不存在** | `grep -n "CROSS-PROJECT" AGENTS.md` → 无命中 |
| **本仓当前的 bundle 布局编译期断言** | `include/bundles/*.hh` 内 `static_assert` / `BUNDLE_FIELD_WIDTH` 共 **58** 处（主要载体：`cpphdl_types.hh` 22 · `cep_coherence_bundles_tlm.hh` 15 · `cr_bundles_tlm.hh` 9 · `doorbell_bundles_tlm.hh` 6 · `msix_bundles_tlm.hh` 4 · `dgpu_fabric_bundles_tlm.hh` 2）|
| ⚠️ **`PcieTlpBundle` 本身**（`include/bundles/pcie_bundles_tlm.hh`）**无** `static_assert` / 宽度锁 | `grep -n "static_assert\|BUNDLE_FIELD_WIDTH" include/bundles/pcie_bundles_tlm.hh` → 无命中 |

⇒ **结论：该守卫面本仓不承担**（跨仓 ABI 布局由消费方负责）;**并且**本仓对 `PcieTlpBundle` 的布局守卫**当前是缺口**（Requirement 的 `MUST enforce … via compile-time static_assert` 目前**未被满足**）。

⚠️ **本 change 不改代码补该缺口**（AC-7 禁 `include/bundles/**`，且那是**实现面**变更、须独立立项）⇒ 本 change 只做**契约去悬空** + 在 design.md **登记该缺口**（见 F-1 路由）。

## D-3 · R-20 的 ABI 选择依据（AC-6 的「不得改指其它已删 ABI」）

| 候选 | 现状 | 采用 |
|---|---|---|
| `cpptlm_emulator_get_adapter_info` | **已删除**（`include/abi/cpptlm_emulator.h:108` 注释：「已删除 (per ADR-SOC-20 §2.1)：与 `cpptlm_emulator_get_device_info` 重叠, 改为按 dev_id 查询无句柄依赖」）| ❌ |
| `cpptlm_emulator_get_device_info(uint32_t dev_id, cpptlm_device_info_t*)` | **现行**（`include/abi/cpptlm_emulator.h:71`）| ✅ |

⇒ 原 Scenario 的 `(handle, &info)` 形态在现行 ABI 下**不存在**（`get_device_info` 按 `dev_id`、无句柄）⇒ 改写为「按 `dev_id` 调用」并把「无句柄依赖」的原因写进 Scenario（⛔ 不留「句柄入参」的假形态）。

## D-4 · R-13 的处理（本 change **不含**）

AC-0 spike 证明 **delta 无法修改文档 trailer**（见 `.rddf/reviews/cep-spec-stale-refs-r14-r15-r20-ac0-spike.md`）⇒ R-13 的 2 处状态行**不能**并入本 change。
⇒ 本 change 的 delta **不触碰** `pcie-ip-microarch`（其失效内容只在 trailer ⇒ 无 delta 可做）。⚠️ 注意 `pcie-ip-integration` 的 **trailer**（`:53` 的 `**状态**`）也**不会**被本 change 的 MODIFIED 合并修正（spike 实测：合并器**原样保留** trailer）⇒ `pcie-ip-integration` 归档后**仍有** stale trailer 行 ⇒ R-13 的该项**依然待裁**。

## D-5 · 为什么不用 `--skip-specs`（AC-9）

5 项都是「**实现层已成立、只是契约措辞错**」⇒ delta **必须合并**（否则债原地不动）。本 change 只做其中 4 项，同样必须合并。
