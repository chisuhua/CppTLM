# proposal: cpptlm-module-param-effect-landing

## Why

「**配置里有键、实现零效果**」—— 用户写进配置的参数被静默丢弃：装配成功、仿真照跑，只是行为与配置说的完全不同。

`ADR-X.17`（模块参数「读取即声明」）已把**检出机制**落地（`RouterTLM` 迁移后，`SimObject::set_config` 统一检出未知键 ⇒ 装配失败）。但机制的适用面被 `ADR-X.17` D-6 卡住：

> 每个被声明的参数 SHALL 附带一条**行为判别**测试 …… **无此测试的参数不得被声明。**

⇒ **零效果的键不得进入声明** ⇒ 这批键所在的类型**无法迁移**（缺陷原地冻结）;且在类型迁移那一刻，它们会从「静默丢弃」突变为「未知键 ⇒ 存量配置装不上」⇒ **迁移前必须先给它们确定效果**。

### 实测规模（2026-11-18 · 全仓 `configs/**` 扫描 + 静态读取面检索）

| 类别 | 类型 | 键×类型对 | 出处 |
|---|---:|---:|---|
| ① **零配置读取面**（类型完全没有 `on_config_loaded` / `set_config` 覆写 ⇒ 其 `params` 全部键零效果） | **14** | **32** | ⚠️ **本次新发现** —— `ADR-X.17` F-14 未覆盖 |
| ② 有读取面但该键未消费（读到即丢 / 读了不消费） | 3 | 9 | `ADR-X.17` F-14 |
| ③ 生成方产出、模块从不消费 | 1 | 1 | `RouterTLM.routing`（`GpuNoC` 为每个内部 router 生成） |

最刺眼的两个：`CPUTLM`（**95 个配置实例**、3 键全零效果 —— `cpu_tlm.hh` 的 `start_addr_` / `request_interval_` 只被构造期与运行期用，从不由配置赋值）与 `CacheTLM`（**37 个实例**、6 键全零效果 —— 该类无任何配置读取面）。逐类型清单见 `design.md` D-1。

## What Changes

1. **全量键盘点**（类型 × 键 × 读取面 × 效果面）⇒ 逐键**三态裁决**（design D-2）：
   - **态 ① 接真实消费** —— 实现消费路径 + 行为判别测试（`ADR-X.17` D-6 义务）;
   - **态 ② 生成方停止产出** —— 删掉产出该键的代码路径;配置里写该键 ⇒ **装配失败**并报出该键名（不得静默忽略）;
   - **态 ③ 显式拒绝** —— 键是伪参数（合法值唯一）⇒ 读入并校验，非合法值装配失败。
2. **落地顺序**（design D-3）：先 `ADR-X.17` F-14 的 5 类（既有登记）⇒ 再零读取面家族（14 类）⇒ 最后 `RouterTLM.routing`。
3. 每键一条判别测试：态 ①/③ 用「两个不同合法值 ⇒ 可观测指标不同」;态 ② 用「配置含该键 ⇒ 装配失败」。
4. `docs/implementation/module-param-baseline.md` 表 B 逐条更新（`待修` ⇒ `已消费` / `已停止产出` / `已显式拒绝`）。

## Out of Scope

| 项 | 归属 |
|---|---|
| 机制迁移（`load_param` 准入扩容、CFG-08 per-type 分支退役、ParamRules 机制退役） | change `cpptlm-module-param-migrate-batch2` |
| `configs/param_rules/*.json` 的存废定位 | ⚠️ **需独立 ADR 级裁决**（`ADR-X.10` §F）⇒ architect 域，**本 change 不裁决**（且不得擅自删除该文件 —— 它是 Python 生成期校验的活数据源） |
| `derive_expr` 派生求值的实现 | `ADR-X.10` 决策 5 / 决策 7 Stage 2（未实施）⇒ 独立 change |
| `vc_count` 的容器级改造（`NUM_VCS` 编译期常量 ⇒ 需换 3 个核心容器的类型） | 沿用 `cpptlm-router-vc-count-param-inert` 的「**只登记不修**」结论 |
| `RouterTLM` 已迁类型的行为 | `cpptlm-module-param-read-declares`（2026-11-17 已归档） |

## 已知残留（本 change 不闭合 · 须具名 owner）

| # | 残留 | 处置 |
|---|---|---|
| R-1 | **板级直读路径**（`ADR-X.17` F-15 · `dgpu_board_shell.cc` 直读 JSON `params`）覆盖的类型（`MemoryTLM` / `VramControllerTLM` / `GmmuTLM` / `PcieEndpointIP`）其「已消费」登记归板级而非模块 | 由 `cpptlm-module-param-migrate-batch2` 的表 A 处置（`ADR-X.17` D-3 三态条款） |
| R-2 | `derive_expr`（`configs/param_rules/router_tlm.json` 的 `vc_count` 条目）两侧皆无人执行 | `ADR-X.10` 决策 5 未实施 ⇒ 独立 change |
| R-3 | `cpptlm_config`（Legacy / Deprecated）是 `configs/param_rules/*.json` 的唯一消费者;其继任者 `cpptlm/` 零 `param_rules` 引用 | `ADR-X.10` §F 裁决时一并回答「生成期校验能力迁往何处」 |
| **R-4** | ~~**`CacheTLM` 的 6 键本 change 无法落地**~~ —— ✅ **已解除**（2026-11-18）:该类原为 CEP 冻结面（`ADR-CEP-15` IC1）⇒ 经**用户裁定解冻**（`ADR-CEP-15` Status Update 2026-11-18 决策 2 改判 + 台账 Entry #478）⇒ 容量模型已在 `cache_tlm.hh` 内补齐，`size` / `line_size` / `associativity` / `assoc` / `level` 走**态 ①**、`protocol` 走**态 ②**（协议语义不入本类） | **已闭合**（不再依赖 architect 裁决;连带面见 `ADR-CEP-15` SU §⑤） |
| **R-5** | 配置引用了**运行时注册表里没有的类型** —— 实测 **4 个类型 / 3 份配置**：`TmuDispatchProcessorTLM`（类存在但**从未注册** + 配置名与类名不一致;`configs/dgpu_board_v1.json`，**嵌套**）· `ArbiterTLM`（模板基类未注册，注册名只有 `ArbiterTLM2/4`;`configs/library_memory_demo.json`）· `ComputeUnitTLM`（类不存在，4 实例;`configs/apu_soc_full.json`）· `TCCTLM`（类不存在，2 实例;同上）。**失败形态 = 装配成功但模块被静默丢弃**（`module_factory.cc` 约 385 行只打 `DPRINTF` 后继续;且 `DPRINTF` 仅 Debug 构建生效 ⇒ Release 下**零输出**）⇒ **拓扑缺模块 ⇒ 仿真结果静默失真** | 机制 fail-fast（顶层）+ 4 处对齐 + 「配置 type ⊆ 运行时注册表」判据闸 ⇒ 独立 change **`cpptlm-config-type-registration-align`** 承接;台账 §2bis 已按实测订正（原登记「整份配置不可装配」与实测不符，已改） |

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | **键盘点完备（口径对齐 v2 · 2026-11-18 二次改判 —— 验收第 1 轮发现 v1 亦不可达）** —— D-1 表登记的**每个类型**都在 `docs/implementation/module-param-baseline.md` 的**表 A / 表 B / §2bis** 中**至少有一行状态登记**（即：登记过的类型都有落点）;D-1 表的**每个键**在 `design.md` **D-1.6.2~D-1.6.6** 的逐批裁决表中有对应行（态①/②/③ 之一）。<br>⚠️ **为什么不再判「类型 ∈ 盘点脚本输出」（v1 的失败）** —— 盘点的口径是「配置里的『类型 × 键』」;本 change 的处置**把键从配置里清掉**（态② 停止产出）⇒ 配置里无键 ⇒ 脚本不再输出该类型 ⇒ **「处置完成」与「类型 ∈ 脚本输出」互斥**（实测：`PcieControllerIP` / `PcieMemoryDevice` / `ArbiterTLM` / `SocInternalNoc` / `CrossbarTLM` / `StreamingMultiprocessorTLM` / `CoherentXBarTLM` / `GpuMeshNoC` **8 个类型完全消失**）。<br>⚠️ **为什么不再判「数字直比」（v0 的失败）** —— D-1 是「零效果族登记口径」、脚本是「当前有无读面口径」，且本 change 的处置本身给多数类型**新增了读面**（`on_config_loaded` 的拒绝路径）⇒ 数字直比恒不成立。<br>判据载体 = 台账 `grep`（每个 D-1 类型名至少出现一次）+ `design.md` D-1.6 的逐批裁决表。⚠️ 已归档类型（`GPUTLM` / `MemoryClusterTLM` / `SharedMemoryTLM`）**不在 D-1 表内**（原表已删三行:17→14 类 / 42→34 对）。 |
| **AC-02** | **逐键三态** —— D-1 清单中每个键都有唯一态（①/②/③）且有对应实现或显式拒绝路径;⛔ 不得有「待定」态留在表里 |
| **AC-03** | **态 ① 的行为判别测试** —— 每个态 ① 键有一条测试：两个不同合法值 ⇒ 至少一个可观测指标不同（`ADR-X.17` D-6）;判据 = 该测试存在且绿 |
| **AC-04** | **态 ② 的显式失败** —— 配置中写有「已停止产出」的键 ⇒ `instantiateAll` 返回 **false** 且错误信息含该键名;⛔ 不得静默忽略 |
| **AC-05** | **态 ③ 的显式拒绝** —— 伪参数写入非唯一合法值 ⇒ 装配失败且错误信息含该键名与合法值集合 |
| **AC-06** | **零回归** —— `cpptlm_tests` 全量绿（assertions / cases **≥** 基线 883191 / 2371）;`git diff <base> -- configs/` 的每处变更都对应一个「态 ② 停止产出」任务（⛔ 不得顺手删配置） |
| **AC-07** | **台账一致** —— `docs/implementation/module-param-baseline.md` 表 B 的每个键状态与实现一致;`grep -rn 'ignore_param(' src/ include/` 的非注释调用点与表 B 条目一一对应（承接 `cpptlm-module-param-read-declares` 的 AC-14） |
| **AC-08** | **`routing` 处置落地** —— `RouterTLM` 对 `routing` 的处置为态 ② 或 ③（⛔ 不得停留在 `ignore_param` 登记）;`GpuNoC` 的内部配置生成与该处置一致;`[simmodule][infra]` 的 GpuNoC 用例绿 |
| **AC-09** | **未准入类型零变化** —— 本 change 不动任何「已准入类型」以外的行为面 ⇒ `[param]` / `[router]` / `[config][schema]` / `[cep]` 全绿 |

**回归护栏**（改动前后均绿）

- **G-1** 已准入类型（`RouterTLM`）的既有判据零变化 —— `[param]` 28 cases 全绿
- **G-2** 全量套件零回归 —— `cpptlm_tests` 全绿 + `pytest test/python/` 232 passed + `ctest -R cep` 100/100

## 测试覆盖审查（Scenario → AC 矩阵）

| Scenario | 覆盖 AC | 说明 |
|:--:|---|---|
| S1 配置里的每个参数键 SHALL 有确定效果 | AC-01, AC-02 | 盘点 + 逐键三态 |
| S2 态 ① 的键 SHALL 产生可观测差异 | AC-03 | 行为判别测试（`ADR-X.17` D-6） |
| S3 态 ② 的键 SHALL 使配置显式失败 | AC-04 | 生成方停止产出 ⇒ 装配失败 |
| S4 态 ③ 的伪参数 SHALL 只接受唯一合法值 | AC-05 | 显式拒绝 |
| S5 零效果键 SHALL NOT 长期以 ignore_param 豁免 | AC-07, AC-08 | 台账一致 + `routing` 处置落地 |
| S6 本 change SHALL NOT 改变未准入类型的行为 | AC-06, AC-09 | 零回归 |

## 已知测试薄弱点（本 change 接受）

| # | 薄弱点 | 理由 / 缓解 |
|:--:|---|---|
| W-1 | AC-01 的「完备性」由**盘点脚本 + 人工复核**承担，不是自动闸 | 键的「效果面」不可机械判定（`ADR-X.17` D-6 明示）⇒ 只能靠盘点表 + 测试存在性;**不**为此造闸（会造出第二份真源） |
| W-2 | 态 ②「停止产出」会改变**存量配置的可装配性**（写了该键的拓扑从「装得上」变「装不上」） | 这是**有意的**行为收紧（静默丢弃比失败更坏）;AC-04 要求错误信息含键名，且 §Out of Scope 明确「不得擅自删除 `configs/param_rules/**`」;受影响配置逐个在 tasks 登记 |

## 关联

- 决策源：`docs/adr/ADR-X.17-module-param-read-declares.md`（D-2 消费即登记 / D-3 准入三态 / **D-6 行为判别义务** / F-14 零效果族清单）
- 基线台账：`docs/implementation/module-param-baseline.md`（表 B）
- 前序 change（已归档 2026-11-17）：`cpptlm-module-param-read-declares`（机制 + `RouterTLM` 试点）· 台账 Entry #472 / #473 / #474
- 关联 improvement（backlog · 本 change 不闭合其 C++ 侧诉求）：`cpptlm-router-vc-count-param-inert`（只登记不修）· `cpptlm-validate-params-dead-code`（C++ 侧死代码 ⇒ 归 `cpptlm-module-param-migrate-batch2`）
- 后继 change：`cpptlm-module-param-migrate-batch2`（机制迁移批次 2）
