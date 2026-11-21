# design —— `cpptlm-cep-iag-gate-execution-arm`

> **定位**:本文件是 D-2 的**实施契约**。架构决策(4 条)落 `architect 方案 .rddf/reviews/cep-cli-iag-gate-alignment.md`;本文件记录**实施期实测订正**与**判据细节**。

## D-1 · 两臂的职责边界

| 臂 | 位置 | 输入 | 失败语义 |
|---|---|---|---|
| **装配路径臂** | `src/core/module_factory.cc` **Step 0.26** | 真 config + 加载器输出 | `return false` ⇒ 装配失败,**零副作用**(模块创建在 Step 2) |
| **离线 CLI 臂** | `examples/cep_instance_assembly_gate.cc` | config 路径参数 | rc=1 + `[IAG-0x]` 签名 ⇒ 挂点红 |

⛔ **不扩** `cep_channel_dependency_gate`:后者 = **声明层**闸(读 `protocol_instances.json`,无 config 概念)⇒ 语义不同,混入会让退出码语义失真。

## D-2 · 禁双实现的三层保证

| 层 | 机制 | 可机器验 |
|---|---|---|
| **判据实现** | 两臂均调 `tlm::cep::run_instance_assembly_gate` | 对账闸 C1/C2(grep) |
| **加载规则** | 两臂均调 `tlm::cep::load_instance_assembly_inputs`(新头文件) | 对账闸 C3(grep) |
| **判据口径** | `instance_assembly_gate.hh` **零 diff** | 对账闸 C4(`git diff --quiet`) |

## D-3 · 加载器的语义(与既有口径逐条对齐)

| 输入形态 | 处置 | 依据 |
|---|---|---|
| `cep.protocol_instances` = **字符串** | **显式**源(相对 CWD)⇒ 不可读 / 畸形 / 0 实例 ⇒ **source_error**(判红) | 对齐 `parse_channel_declaration_from_config` |
| = **对象 / 数组** | 内联 ⇒ 直接用 | 同上 |
| **缺失** | 落到 `kDefaultDeclarationPath`(相对 CWD)⇒ 不可读 ⇒ **SKIP**(外部条件) | 同上 |
| 平面层 `collective_planes.json` | 不可读 ⇒ **空文档**(⛔ 不阻断,只影响 04/05) | 粒度决定:声明层缺失 = SKIP,平面层缺失 = 降级 |

## D-4 · 决定性变异(2 条 · 按签名红)

| # | 变异 | 期望 | 载体 |
|:--:|---|---|---|
| **M1** | bridge `protocol_instance` ⇒ 未声明 id | CLI rc=1 且输出含 **`[IAG-02]`** | 临时 config(/tmp),⛔ 不动真实仓 |
| **M2** | 从 `scripts/CMakeLists.txt` 摘掉 `${CPPTLM_CEP_VALIDATION_COMMANDS}` | 对账判据 **C6 红**(`NO_IAG_SEGMENT`) | **影子树**(`cep_shadow_reset`) |

## D-5 · 行为级决定性变异(真装配路径)

`test/test_cep_iag_execution_arm.cc` 用 **dup2 捕获 stdout** ⇒ 按**签名**断言:

| 形态 | 期望签名 | 有鉴别力的理由 |
|---|---|---|
| 未声明实例 | `[IAG-02]` | 无臂时**不会红**(该 config 可装配成功) |
| 桥引模块不在拓扑 | `[IAG-06]` | 无臂时会红于**模块解析**(错误签名不同)⇒ 证明臂**先于**模块解析 |

## D-6 · ⚠️ 实施期实测订正(architect 决策 D-A1 的**位置**)

architect 原判:「IAG-06 需读 *topology 模块表* ⇒ 必须等 hierarchy 解析 ⇒ 挂 **Step 0.75**」。

**实测不成立**:`instance_assembly_gate.hh` 的 `module_declared()` 读的是 **config 的 `modules` 段**(静态声明),⛔ **不需要**已创建的对象 ⇒ IAG 族**纯 config 依赖**。

⇒ 落点改为 **Step 0.26**(紧跟 Step 0.25)⇒ 比 Step 0.75 **更早**且同样「零副作用」。
⇒ 决策**目的**(早失败 + 零副作用)**不变**,仅位置提前 ⇒ 对账闸 C7 以「臂行号 < `// 2. 创建所有模块实例` 行号」机械保证。

## D-7 · ⚠️ 实施期实测订正(ctest 用例数)

improvement 的 AC-6 原写「ctest **173**」⇒ 实测 **174**(+2):① 新对账闸 ② **离线 CLI 自身**也注册了 ctest(`cep_instance_assembly_gate`)⇒ 判据**未放宽**,只订正记录值。

## 铁律

1. **禁双实现**:判据实现 + 加载规则两处共用 ⇒ 任何一处另起炉灶即违规。
2. **判据本体零 diff**:`instance_assembly_gate.hh` 只读引用。
3. **按签名红**:⛔ 不断言 `rc≠0`;每条变异都要「还原 ⇒ 绿」。
4. **装配臂零副作用**:失败必须早于 **Step 2**(模块创建)。
5. **CLI 臂不与通用 validator 混跑**:通用段跑 `${TOPOLOGY_VALIDATOR}`,CEP 段只跑 IAG CLI。
6. **变异只碰影子树 / 临时文件**;真实仓用**字节指纹前后比对**。
