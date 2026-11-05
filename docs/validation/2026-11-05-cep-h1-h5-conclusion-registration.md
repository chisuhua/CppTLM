# CEP 假设结论登记：H1（seam 隔离）/ H5（SHARP 0 误分类）

**日期**:2026-11-05 · **角色**:architect（裁决）· builder（证据补齐）· planner（表落）
**性质**:假设**结论登记** —— `objective §9.1` ① 表的 verdict 从 🟡「有契约级证据，未以假设结论形式登记」转为 ✅「成立」。
**性质边界**:⛔ **不改任何已归档 change 的产物**；⛔ **不重跑**已归档实验；⛔ **不改** `objective §9` 的判据**原文**（原文永远停在本文件 §1）。
**关联**:台账 **Entry #221**（授权记录）· **#219** ⑥ 剩余任务 #7 · `ADR-CEP-06` **SU#1** / **SU#2** ·
`ADR-CEP-05`（stub 定位：决策 1 = (α) RN 侧投影）· `ADR-CEP-15`（CacheTLM 对齐边界：共存不替换） ·
`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`（H2/H3/H6 的登记先例）· `objective §9.1` ① 表

---

## 0. 一句话结论

**H1 与 H5 均判 ✅「成立」**，但两者的**证据边界不同**，且 **H1 的判据原文经 architect 裁决订正**（订正授权与理由见 §2）。

⚠️ **H1 的结论强于原判据的机械强度**：原判据只有「共用同一 stub」一条（且**无守门**、**字面为假**）；
订正后的判据是 **adapter 侧零 stub 耦合（8 个逐一断言）+ stub 前向冻结守门 + 缓存核心双零** ⇒ **机械强度不低于原判据**。

---

## 1. 判据来源（**逐字**）

> ⛔ 本节是 `objective §9` 的**逐字来源** —— 原文永远停在这里。`objective §9.1` ① 表里的订正格是 **§2 的派生面**。

| # | 假设（逐字） | 成功判据（逐字） |
|---|---|---|
| **H1** | 协议抽象层(seam 在 L5↔L4)确实让缓存核心免于协议改动 | 全部 adapter 只改 adapter,stub 0 diff(**adapter 实例计数口径见 `07 号 §5.1.1`;本表不复述具体数**,per Entry #22) |
| **H5** | SHARP 5 条防误分类规则(R1-R5)能可靠区分 SHARP atomic vs cache-coherent atomic | 0 误分类 |

---

## 2. H1 的判据订正（architect 裁决，2026-11-04 复核）

### 2.1 订正理由（三条实测依据）

| # | 依据 | 实测 |
|---|---|---|
| **①** | **原判据第②半「全部 adapter 共用同一 stub」字面为假** | `grep -rn "StubCacheCore\|stub_cache" include/tlm/cep/{chi_rnf,mesif,moesi_gpu}_adapter.hh \| grep -v "^.*://"` ⇒ **零命中** ⇒ 这 3 个 adapter **只在注释里**提 stub，**不构造也不使用**。其余 5 个（`cxl_cache` / `cxl_dcoh` / `nvlink` / `tilelink_c` / `ualink`）**连注释都没提**，其文件头写的是「状态机 = **协议自身的** N 态语义(与 Stub Cache 的 5 态投影是**两个不同的东西**)」 |
| **②** | **这正是 `ADR-CEP-05` 的签发形态，不是缺陷** | `ADR-CEP-05` 决策 1 = 「**(α) RN 侧投影(α')+ 命名复用 CohIf canonical 10 流清单**」;`:38` 定位澄清：「Stub Cache Core **不是缓存性能模型**，而是**客户端侧协议状态投影**(RN projection)」⇒ 各 adapter 持有**协议自身**的状态语义、**不持有** stub，**正是 (α) 的实现形态** |
| **③** | **原判据第③半「stub 0 diff」无法用既有基线落地** | `stub_cache_core.hh` 在 CEP 基线 `4ae6bfc4` 处**不存在**（创建于 `7a2efe23` 队列 #3）⇒ 用既有 `kCepBaselineCommit` 查 stub **必然恒红** ⇒ 必须用 **per-file 基线**（自创建起前向冻结） |

⇒ **原判据把「seam 的架构语义」误写成了「实现机制」**：它按落选方案 (β)「独立接口 + 适配」的措辞写，
而签发的是 (α)。⇒ 订正是**纠错**，不是**弱化**（见 §2.3 的机械强度对比）。

### 2.2 订正后的判据（**可机器判定**）

> **缓存核心零协议改动**：`cache_tlm.hh/.cc` 相对 CEP 基线 **0 diff**（守门在案，per `ADR-CEP-15` IC1）+ CacheTLM 源文本**零协议标识符**；
> **且** **adapter ↔ 缓存核心解耦**：全部 adapter 源文本**不含** `StubCacheCore` / `stub_cache_core` 的构造 / 持有 / 调用（注释级提及不计），各 adapter 持有**协议自身**状态语义；
> **且** `stub_cache_core.hh` **自创建 commit `7a2efe23` 起前向 0 diff**（per `ADR-CEP-05` 契约冻结）。

**依赖观测**（订正后）：CacheTLM 源文本零协议标识符 · `cache_tlm.hh/.cc` 基线 0 diff · **8 个 adapter 逐一零 stub 耦合** · `stub_cache_core.hh` 前向 0 diff。

### 2.3 机械强度对比（订正**未弱化**原判据）

| 维度 | 原判据 | 订正后 |
|---|---|---|
| 可执行的断言数 | **1**（「共用 stub」，且**无守门**） | **4 类**（缓存核心双零 + 8 adapter 逐一 + stub 前向冻结） |
| 覆盖的 adapter 数 | 隐含「全部」，但**字面为假** ⇒ 0 个可判 | **8 个逐一断言**（防「默认绿」有自证正控） |
| 反向 fixture | **无** | 2 条（改 adapter ⇒ 红 · 改 stub ⇒ 红），实测见 §3.3 |

---

## 3. 证据清单（全部 file:line 实测）

### 3.1 H1 的证据

| 判据分支 | 载体 | 断言 |
|---|---|---|
| CacheTLM 零协议标识符 | `test/test_cep_module_factory_seam.cc:131-146` | 5 个协议标识符（`SNOOP` / `COHEREN` / `MESI` / `MOESI` / `DIRECTORY`）大写归一后**均不出现** |
| `cache_tlm.hh/.cc` 基线 0 diff | `test/test_cep_module_factory_seam.cc:127-128` | `file_modified()` 相对 `kCepBaselineCommit` 为 false |
| **8 adapter 零 stub 耦合** | `test/test_cep_module_factory_seam.cc`（**2026-11-05 新增用例**） | 逐个 adapter 去注释后断言 `StubCacheCore` / `stub_cache_core` **零命中**;**含 1 条自证正控**（`protocol_engine_base.hh` 应命中 ⇒ 证明扫描有牙） |
| **stub 前向冻结** | `test/test_cep_module_factory_seam.cc`（**2026-11-05 新增用例**） | 相对 **per-file 基线 `7a2efe23`**（stub 的创建 commit）0 diff;并断言该 commit 存在且可达 |

**H1 依赖的 adapter 清单**（8 个，per `07 号 §5.1.1` 的 8 实例）：
`chi_rnf` · `cxl_2_0` · `cxl_3_0_symmetric` · `tilelink_c` · `nvlink_4` · `ualink_4` · `moesi_gpu` · `mesif`。

### 3.2 H5 的证据

| 规则 | 载体 | 覆盖 |
|---|---|---|
| **R1 / R2** 分类判据 | `test/test_cep_nvlink_sharp_state_spec.cc`（NSS-02） | `classify_at_fabric_ingress` 2 输入 **4/4 穷尽**（`(T,T)→DiverT` / `(T,F)→Pass` / `(F,T)→Pass` / `(F,F)→Pass`） |
| **R4** epoch 语义双向 | 同上（NSS-02 的 R4 回归锁） | `epoch_routes_to_collective(Uncommitted)==false` / `(Committed)==true` / `kEpochStateCount==2` ⇒ **2 态穷尽** |
| **R3** 注册三步原子 | 同上（NSS-03） | drain 失败 ⇒ `DrainFailed` + **不建 epoch** |
| **R4** 注销 | 同上（NSS-04） | 注销 ⇒ 失效化并**回归 coherence 域**;注销失败 ⇒ 仍 divert（**泄露可观测**） |
| **R5** fail-stop | 同上（NSS-05） | `is_fail_stop` **3 bool 输入 = 8 组合**;**2026-11-05 补齐后 8/8 显式断言**（此前仅 4/8 ⇒ 「穷尽」是 overstated） |
| **R1/R2 ↔ R5 交叉锁** | `include/tlm/cep/nvlink_sharp_state_spec.hh`（**2026-11-05 新增**） | `static_assert` × 5 钉住 `is_fail_stop(r,e,c) == (classify_at_fabric_ingress(r,e)==DiverT && c)` ⇒ 两者输入语义分叉即**编译红** |
| ordering A/B/C/D | 同上（NSS-06） | 四类各一组**正反例**;C 的 tie 未定义已显式登记 |

### 3.3 反向 fixture（判据「因正确原因红」的实测）

| # | 注入 | 实测红因 |
|---|---|---|
| **1** | `classify_at_fabric_ingress` 去掉 `epoch_committed` 条件（与 `is_fail_stop` 分叉） | `error: static assertion failed: R5 fail-stop 的输入语义必须与 R1/R2 分流一致(交叉锁)`（**编译红**） |
| **2** | `chi_rnf_adapter.hh` 注入 `StubCacheCore` 引用 | `FAILED: adapter 构造/持有 StubCacheCore: include/tlm/cep/chi_rnf_adapter.hh` |
| **3** | `stub_cache_core.hh` 追加一行 | `FAILED: stub 自创建起被改动` |

⇒ **3 条全部因正确原因红**;恢复后 `test_cep_module_factory_seam` **238 assertions / 11 cases** 全绿。

---

## 4. 口径限定（⚠️ 必须随任何引用重复）

1. **H1 的结论是「缓存核心免于协议改动」**，**不是**「所有 adapter 接到同一个 stub 实例」——
   后者是原判据按落选方案 (β) 的措辞（`ADR-CEP-05` 签发的是 (α)）。
2. **H1 的「0 diff」是 per-file 基线语义**：`cache_tlm.*` 相对 CEP 基线 `4ae6bfc4`;`stub_cache_core.hh` 相对其创建 commit `7a2efe23`
   ⇒ 二者**基线不同**，**不可混用**。
3. **H5 的「0 误分类」是**函数级**证明** —— 覆盖 `is_fail_stop` 与 `classify_at_fabric_ingress` 的**输入域**。
   ⛔ **不是**协议级 / 仿真流量级证明（本仓**无** NP atomic 注入实验）⇒ 引用时**不得**表述为「已证明协议层无误分类」。
4. **H5 的 R4 出现在两处断言**（NSS-02 的 epoch 回归锁 + NSS-04 的注销）⇒ 是**两条不同的 R4 语义**（epoch 提交态 / 注销失效化），非重复。
5. **H1 / H5 都是契约级假设** ⇒ 它们**不产生** `metrics.json` run ⇒ 在 `objective §9.1` ② 预登记表中
   登记为「**全部 13 项 out-of-scope**」（含义：**不产生 run**，⛔ **不是**「允许以 0 冒充未测」）。

---

## 5. Verdict

| 假设 | verdict | 计数影响 |
|---|---|---|
| **H1** | ✅ **成立**（判据经 §2 订正；原文留痕于 §1） | §6.2 **B7**「8 假设全部有实验结论」计数 **+1** |
| **H5** | ✅ **成立**（证据边界 per §4.3 —— **函数级**） | §6.2 **B7** 计数 **+1** |

⇒ **B7 计数由 4/8 → 6/8**（H2 / H3 / H6 / H7 已成立 + 本次 H1 / H5）。
⇒ **H4 / H8 仍未测量**（H4 = 运行期装置未建;H8 = A/B 数据未产）⇒ B7 **仍未达成**。

**复现**:
```bash
cmake --build build -j4 --target cpptlm_tests
./build/bin/cpptlm_tests "[cep][module-factory-seam]"   # H1 的 2 条新守门(238 assertions / 11 cases)
./build/bin/cpptlm_tests "[cep][nvlink-sharp]"          # H5 的 NSS-02 ~ NSS-06(含 8/8 组合 + 交叉锁)
```
