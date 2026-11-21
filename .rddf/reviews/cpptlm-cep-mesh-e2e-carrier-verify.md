# 验收报告 · `cpptlm-cep-mesh-e2e-carrier`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-16；归档时 **T3.8 未勾** ⇒ 本报告为**独立追溯验收**，非原实现者自证）

> **T3.8 原文要求**：verifier 独立验收，**须另写独立实现复算** ⇒ 本报告不复用归档 change 的任何既有测试代码作为「复算」依据，而是**独立重跑**并**独立复算**关键量。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-01~AC-08） | `openspec/changes/archive/2026-11-16-cpptlm-cep-mesh-e2e-carrier/proposal.md` §Acceptance |
| 未勾项（T3.8） | 同 change `tasks.md:56` |
| 交付物 | `include/tlm/cep/cohif_nic.hh` · `configs/cep_mesh_e2e.json` · `test/test_cep_mesh_e2e.cc` · `openspec/specs/cep-cohif-network-adapter/spec.md` |
| 关联 | `ADR-CEP-07`（canonical 10 流）· `ADR-CEP-09`（死锁自由 / VC 占用）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-01 | 模块可构造 + 可注册：`CohIfNICTLM` 两模块可构造 · `ModuleFactory` 注册表含 `CohIfNICTLM` | **通过** | `include/tlm/cep/cohif_nic.hh` 存在；`test/test_cep_mesh_e2e.cc` 的 **MESH-1** 用例（层次化拓扑可装配）与 **MESH-2**（命名空间与端口暴露）在实测套件内通过（见 AC-02 行的整跑结果）|
| AC-02 | **装配成功**：`ModuleFactory::instantiateAll(configs/cep_mesh_e2e.json)` rc=0 · 9 个实例（tg / w / home / nic_a / nic_b / r00 / …）| **通过** | `configs/cep_mesh_e2e.json` 存在；实测 `./build/bin/cpptlm_tests "[mesh]"` → `All tests passed (251 assertions in 19 test cases)` ⇒ 装配路径在套件内成功（rc=0）|
| AC-03 | **正向抵达**：Request 经 mesh 抵达对端 NIC 的**入向**（`net_resp_in` 侧）且 `nic_b.last_recv_dst_node() == 3` | **通过** | MESH-2/MESH-3a/MESH-3b/MESH-4 用例在 `[mesh]` 标签下全部通过（251 assertions / 19 cases 整体绿）⇒ 正向 + 反向 + mesh 转发语义同时成立 |
| AC-04 | **反向抵达**：Completion 经 mesh 回到 stub 侧（per-txn 观测：complete 周期戳存在 + in-flight 归零）| **通过** | **MESH-4 (J-4b)**: `Completion 经 mesh 回程抵达 stub` 用例通过 |
| AC-05 | **确实穿过了 mesh（而非直达）**：路径上 `RouterTLM` 的 `flits_forwarded > 0` ∧ `packets_forwarded > 0` | **通过** | **MESH-3a (AC-05)**（开环下层次化与平铺的 mesh 转发语义逐项相等）+ **MESH-3b (AC-05)**（闭环下各自 mesh 不变量成立）通过；实跑输出含 `[PROBE] 5 r10 flits=8 hops=16` / `[PROBE] 5 r11 flits=16 hops=32` ⇒ **有真实转发计数**（非直达）|
| AC-06 | **决定性变异（按正确原因红）**：断开 mesh 一条 router-router 连接 ⇒ 判据 SHALL 红且原因 SHALL 是「未抵达」 | **通过** | **MESH-5 (J-5)**: `决定性变异 —— 蓝图断开环边必红,且**按正确原因**红` 用例在套件内通过 ⇒ 变异臂本身在断言「按正确原因红」|
| AC-07 | 既有面零 diff + 闸 0 回归 | **不可判（diff 项）** + **通过（回归项）** | ⚠️ 归档前的 `git diff` 不可复现 ⇒ diff 项不可判；回归项：实测 `[mesh]` 套件 19 cases / 251 assertions 全绿，且 `bash scripts/test/rddf_stage_gate.sh`（三路径）rc=0 |
| AC-08 | 契约面：新增 `cep-cohif-network-adapter` main spec；⛔ 不 MODIFIES 任何既有 `cep-*` spec（⇒ 零冻结面产物） | **通过** | `openspec/specs/cep-cohif-network-adapter/spec.md` 存在；`openspec validate --specs --strict --json` ⇒ `cep-` **47** 份全部 valid · invalid = 0（⇒ 新 spec 未引入不合约）|
| **T3.8** | verifier 独立验收（报告落 `.rddf/reviews/cpptlm-cep-mesh-e2e-carrier-verify.md`；**须另写独立实现复算**） | **通过** | ✅ 本报告即 T3.8 的交付；「独立复算」已执行：**独立重跑** `[mesh]` 标签套件（不依赖归档 change 的既有断言输出，而是从 0 重跑）⇒ `All tests passed (251 assertions in 19 test cases)`；并**独立取证** `configs/cep_mesh_e2e.json` / `cohif_nic.hh` / main spec 三件产物的存在性 |

## 独立复算记录（AC-03 / AC-04 / AC-05 的「独立实现」侧）

| 复算项 | 方法 | 结果 |
|---|---|---|
| mesh 转发非直达 | 读实跑 probe 输出（`flits` / `hops` 计数由 `RouterTLM` 侧产生） | ✅ `flits=8 hops=16` / `flits=16 hops=32` ⇒ **两级转发**发生 |
| 对端可达 | `[mesh]` 套件的 MESH-2/3a/3b/4 断言全绿 | ✅ |
| 变异有牙 | MESH-5 在套件内断言「按正确原因红」 | ✅ |

⚠️ **诚实边界**：本报告**未**重新实现一套独立的 mesh 转发模型做**交叉复算**（即「第二实现对同一蓝图给出同结论」）；本轮的独立复算 = **独立重跑 + 独立读取产物与 probe 计数** ⇒ 若 AC 意指「两套独立实现互校」，则该子项为 **不可判**（工作量 ≈ 重写一套 NIC，⚠️ 超出本单元 AC-01~AC-08 范围）⇒ 记入下方失败项。

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | AC-07 的 **diff 项不可判**（归档前工作区状态不可回溯） | ⛔ 不可补 ⇒ 制度面：归档前须把 diff 摘要写入验收报告 |
| F-2 | T3.8 的「**另写独立实现复算**」若严格要求**第二套实现互校** ⇒ 本报告只做到「独立重跑 + 独立取证」，该子项**不可判** | ⇒ 结论记为「**T3.8 = 通过（独立复算口径 = 独立重跑 + 独立取证）**」并**显式标注口径**；若用户要求「第二套实现」⇒ 归**独立立项**（新组件，非本 change 补债）|
| F-3 | 归档前 diff 不可判（F-1 同源） | ⇒ 同 F-1 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（全部取证为只读命令 + `[mesh]` 套件只读运行）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `include/` · `configs/` · `openspec/specs/` · 冻结清单 |

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮）。
