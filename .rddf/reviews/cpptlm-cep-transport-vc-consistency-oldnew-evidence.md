# C2 · 「旧绿 / 新红」并排取证（AC-3 / AC-12）

> **作者**: builder（C2 实施期 · 台账 **Entry #351**）
> **日期**: 2026-11-12
> **依据**: `tasks.md` **A4.1 / A4.2**（per metis **P0-2** 的「版本无关探针」约束）
> **性质**: **跨实现**的取证 —— ⛔ 不是「同一份 fixture 在新旧两套代码上跑」那种同源自证

---

## 0 · 为什么必须「版本无关」

| # | 事实 | 影响 |
|:--:|---|---|
| 1 | 旧实现（`0ceb0860`）的 `VcBindingSnapshot` **没有** `per_network_orderings` 字段 | ⇒ 把**新结构体**交给旧实现 = **物理不可实现**（编译不过）|
| 2 | 旧实现 `grep -c ordering_assumption include/tlm/cep/collective_plane_contract.hh` = **0** | ⇒ 旧实现**根本不读** `ordering_assumption` ⇒ AC-3 的「旧绿」是**必然**的 |
| 3 | 旧实现**不读** `decl.plane_binding`（`:830` 的 V6-c 只在 `status == "unavailable"` 生效;M-2 模式下 SKIP）| ⇒ AC-12 的「旧绿」是**平凡**的（连 `plane_binding` 都不读 ⇒ 不可能红）|

⇒ 正确形态 = **同一份探针源码**（只调两个**签名在旧/新版本完全相同**的 API）+ **同一份毒化输入文件**，分别由旧/新 header 编译。
⛔ **探针不得引用 `per_network_orderings`** —— 否则旧 header 编译不过 ⇒ 取证退化为「同实现内自证」。

---

## 1 · 探针（已入仓，可复现）

**文件**: `test/probes/c2_oldnew_probe.cc`（88 行）

**只允许的两个调用**（签名在 `0ceb0860` 与当前 HEAD **完全相同**）:

```cpp
cep::load_vc_binding_snapshot(<protocol_instances>, <fabric>)
cep::evaluate_shared_mode_certificate(decl, snap, cep::default_untested_registry(), detail)
```

**用法**（三个参数显式给出，⛔ 不做路径拼接 —— 拼接会在「只毒化一侧」时拼错）:

```
probe_oldnew <repo_root> <protocol_instances> <declaration>
```

| 场景 | protocol_instances | declaration |
|---|---|---|
| 基线 | 仓内真实 | 仓内真实 |
| AC-3 | **影子**（毒化 ordering）| 仓内真实 |
| AC-12 | 仓内真实 | **影子**（毒化 `plane_binding`）|

---

## 2 · 复现命令（逐条实测）

### 2.1 建旧实现 worktree

```bash
cd /workspace/project/CppTLM
git worktree add -f /tmp/opencode/c2-old 0ceb0860     # A1 之前的最后一个 commit
```

### 2.2 编译两版探针（**同一份源码**）

```bash
# 旧实现
g++ -std=c++17 -I/tmp/opencode/c2-old/include \
    -I/workspace/project/CppTLM/external/json \
    test/probes/c2_oldnew_probe.cc -o /tmp/opencode/probe_old

# 新实现
g++ -std=c++17 -I/workspace/project/CppTLM/include \
    -I/workspace/project/CppTLM/external/json \
    test/probes/c2_oldnew_probe.cc -o /tmp/opencode/probe_new
```

**实测**: 两版**均编译成功** ⇒ 这本身证明探针**版本无关**（旧 header 无 `per_network_orderings` 也能编过）。

### 2.3 造毒化输入文件

**AC-3 的影子 `protocol_instances`** —— 只改一处（`mesif` 在 `ualink_fabric` 上 `vc3`）:

```python
# 实测差异实例 = ['mesif']（唯一）
mesif.ordering_assumption.ualink_fabric["3"]: "p2p_ordered" → "unordered"
```

**AC-12 的影子 `collective_planes`** —— 只插一个键（`chi_rnf` **不在** `plane_binding` 里 ⇒ 真插键）:

```python
plane_binding["chi_rnf"] = "FabricCollectivePlane"
# 插后: {"moesi_gpu": null, "mesif": null, "nvlink_4": "LocalCollectivePlane",
#        "ualink_4": "FabricCollectivePlane", "chi_rnf": "FabricCollectivePlane"}
```

### 2.4 跑并排

---

## 3 · AC-3 并排结果（值不一致）

**毒化**: `mesif.ordering_assumption.ualink_fabric["3"]: p2p_ordered → unordered`

| 场景 | 旧实现（`0ceb0860`）| 新实现（当前 HEAD）|
|---|---|---|
| **① 基线**（真实输入）| `VERDICT=PASS` | `VERDICT=PASS` |
| **② 同一毒化输入** | **`VERDICT=PASS`** | **`VERDICT=FAIL`** |

**新实现的 FAIL detail（逐字）**:

```
DETAIL=[CP-03-VC-CERTIFICATE/R1-b] 网络 `ualink_fabric` 的实例 `mesif` 在 `vc3` 上的排序假设**与其它实例不一致** ⇒ 混排语义漂移
```

⇒ ✅ **「旧绿 / 新红」并排成立**，且红因**精确归因** `[CP-03-VC-CERTIFICATE/R1-b]`。

**旧实现为何必绿（诚实表述）**: 旧 header 的 `grep -c ordering_assumption` = **0** ⇒ 它**根本不读**该字段
⇒ 无论 `ordering_assumption` 怎么改，它的 verdict 都**不可能**变化 ⇒ **「旧绿」是结构性必然，不是「旧实现检查得更宽松」**。

---

## 4 · AC-12 并排结果（`plane_binding` 插键不在 scope）

**毒化**: `plane_binding["chi_rnf"] = "FabricCollectivePlane"`

| 场景 | 旧实现（`0ceb0860`）| 新实现（当前 HEAD）|
|---|---|---|
| **① 基线**（真实输入）| `VERDICT=PASS` | `VERDICT=PASS` |
| **② 同一毒化输入** | **`VERDICT=PASS`** | **`VERDICT=FAIL`** |

**新实现的 FAIL detail（逐字）**:

```
DETAIL=[CP-03-VC-CERTIFICATE/R1-c] 实例 `chi_rnf` 绑定 plane `FabricCollectivePlane` 且其 `vc_binding` 含网络 `compute_noc` ⇒ 但 `(FabricCollectivePlane, compute_noc)` **不在 `certificate_scope`** 内 ⇒ M-2 覆盖范围**不完备**
```

⇒ ✅ **并排成立**;红因**含**「不在 `certificate_scope`」且 **⛔ 不含**「plane 未登记」⇒ 与 s-2 的归因**可区分**（AC-5 的硬要求）。

**⚠️ 旧实现为何必绿 —— 这条「旧绿」是**平凡**的（必须逐字写明，⛔ 不得包装成「旧实现也检查了但更宽松」）**:
旧实现在 **M-2 模式**（`status == "shared_no_isolation"`）下**根本不读** `decl.plane_binding`
（V6-c 的同类检查只在 `status == "unavailable"` 时生效,该模式下 SKIP）
⇒ 它连「plane 有没有登记」「在不在 scope」都**不看** ⇒ 对 `plane_binding` 的任何改动**不可能**让它的 verdict 变化。

---

## 5 · 结论

| AC | 判据 | 结果 |
|:--:|---|:--:|
| **AC-3** | 同一毒化输入:旧 PASS / 新 FAIL(`R1-b`)| ✅ **成立** |
| **AC-12** | 同一毒化输入:旧 PASS / 新 FAIL(`R1-c`)| ✅ **成立** |
| — | 两版**同一份探针源码均编译成功** ⇒ 版本无关 | ✅ **成立** |
| — | 旧实现的「旧绿」性质已**逐字写明**（AC-3 = 结构必然 / AC-12 = **平凡**）| ✅ **成立** |

---

## 6 · 与「真实 gate 路径」的分工

| 层 | 载体 | 证明什么 |
|---|---|---|
| **直调层**（本文档）| `evaluate_shared_mode_certificate` | 判据的**逐条归因**正确 + 旧/新差异 |
| **真实 gate 层**（`A4.3` 的用例）| `run_collective_plane_contract` | 判据在**真实装配路径**上可触达、折进 `CP-03` 行、`report.lines.size() == kContractRuleCount`（= 6）|

> ⚠️ **两层不可互相替代**（AC-15 的实质）:只做直调层 ⇒ 可能「判据只在测试里生效」;只做真实 gate 层 ⇒ 归因粒度不足。
