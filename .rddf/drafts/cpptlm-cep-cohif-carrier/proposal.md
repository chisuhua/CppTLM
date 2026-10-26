# cpptlm-cep-cohif-carrier: CohIf 载体 —— 业务 Bundle + ChStream 端口面 + ProtocolBridge 特化

> **状态**: 📋 **Draft**(待队列 #6 归档释放 KPI slot 后落盘到 `openspec/changes/`)
> **⚠️ 未建 change 目录的因由**:CEP 内部 Proposed 并发 ≤ 1(per `ADR-CEP-00 §2 决策 3` / `obj §5.4` / `obj §6.3 D1`)。实测 `openspec list --json` = **1 个 active**(队列 #6 `cpptlm-cep-moesi-gpu-adapter`)=> **slot 已被占用**。先例:台账「补建 OpenSpec change | ❌ … 令 CEP 内部 Proposed 并发 = 2,**违规**」。**转正命令**见文末。
> **队列**: `obj-cep-platform §4.0` 队列 **#7**(Stage 1 / 2028-Q2)· 排在 #6 之后、#8 `cpptlm-cep-chi-rnf-subset` 之前
> **关联 ADR**: **`ADR-CEP-19`**(✅ v1.0 已签发 2026-10-27,per 台账 Entry #74)· `ADR-CEP-07` · `ADR-CEP-12` · `ADR-CEP-16` · `ADR-CEP-07A`
> **规划输入**: `.rddf/improvements/cpptlm-cep-cohif-carrier.md`(5 段草稿)
> **冻结面**: 本 change **零**冻结面改动(载体文件与守门脚本均不在 M1 冻结集)⇒ **无需两步授权**
> **估时**: 6 周(3 项交付 + 1 宏 + 1 钩子扩展)

---

## Why

CohIf(canonical 10 流,`ADR-CEP-02`)至今**没有承载形态**。三个断点(逐 `file:line` 实测,per `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md` v3):

| # | 断点 | 证据 |
|---|------|------|
| **C1** | CEP 零 ChStream 承载体 | `grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/` → **空**(7 个 `.hh`);`stub_cache_core.hh:154` 实为普通 `class StubCacheCore` —— 而 `ADR-CEP-07:64/67` + `07 号 §6.3` 明写二者**都应是** `ChStreamModuleBase` |
| **C2** | 「canonical 10 流的 Bundle」是悬空引用,**同源两处** | `ADR-CEP-07A:49` + `ADR-CEP-16:45`;而 `ADR-CEP-02` 全文 `Bundle` **零命中**、`07 号 §5.4` 业务 Bundle 列表(7 项)**无它**、`include/bundles/` 17 个 `.hh` grep `coherence` **零命中** |
| **C3** | 唯一的 Bundle 化承诺绑在集合通信队列 | `ADR-CEP-03:225` 推迟至 P3(`cpptlm-cep-nvlink-sharp-adapter`)⇒ 主干流无人认领 |

**根因不是缺设计,而是「既有设计从未被一个队列认领」** —— 四份设计齐全、0 个队列:coherence Bundle 字段定义(`06-bis §5.2~§5.4`)/ `ProtocolBridge` 双层类形态(`06 号 §4.4`)/ `register_protocol_bridge` 挂载钩子(`06 号 §9.3`,**write-only 死钩子**)/ 装配路径裁决(`06-bis §3.4` 候选 C 与 `ADR-CEP-16:46/:86` 反向抵消,净结果 = 无路径)。

**为什么必须排在 #6 之后、#8 之前**:队列 #6 的装配验证当前**只能是逻辑级**(`stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动)⇒ 其 `S1-2` 无法在真实链路上验证;若载体排在 #8 之后 ⇒ **#8 继承无载体装配模式** ⇒ 载体债复制一遍。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `include/bundles/cep_coherence_bundles_tlm.hh` | `bundles::CohIfBundle` —— 业务 Bundle,**只覆盖 5 条 RN-side 流**,`flow_id` 区分;**无网络层字段**;`BUNDLE_FIELD_WIDTH` 编译期锁 |
| `include/tlm/cep/protocol_engine_base.hh` | ProtocolEngine 基类 —— `ChStreamModuleBase` 派生 + `Input/OutputStreamAdapter<CohIfBundle>`;持 `ProtocolBridgeEr*` |
| `include/framework/protocol_bridge.hh` | `ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>` + `AbstractProtocolBridge`;**首次落地** |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/chstream_register.hh` | 新增 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` —— **同时**做 `registerObject` **与** `registerBidirectionalPortAdapter`;`static_assert(n > 1)` |
| `include/core/module_factory.hh:313-315` | `register_protocol_bridge` 扩展为 `(bridge_name, factory_lambda)`(死钩子 → 活钩子);**不新建** factory 抽象类 |
| `test/test_cep_canonical_invariants.cc` | **零 diff**(新增 invariant 数 = 0) |
| `scripts/test/cep_decl_reverse_verify.sh` | 登记本 change 的反向 fixture 条目(队列 #6 的 ⑥ 收窄条目已在其中) |

### 冻结面

**零改动**。⇒ 无需两步授权;冻结集成员数**不变**(40)。

---

## Non-Goals

| 不做 | 理由 |
|---|---|
| 协议语义建模(6×6 表的内容) | 归 ArchForge |
| 各协议实例的实现 | 队列 #6 / #8 / #9 |
| 跨域组合 | `ADR-CEP-13`(人工声明 + 实验证伪) |
| per-packet `tlm_extension` | `06 号 §4.5` = 后续阶段 |
| `coherence_bundles_tlm.hh` 路径 | `ADR-CEP-07A` 决策 1 第 3 项明令「不创建」 |
| **动 `StubCacheCore`** | 四条互斥 ⇒ 决策 9 显式延期到 `cpptlm-cep-stub-cache-chstream-surface` |
| 新增 invariant 编号 | 15 组 66 条硬编码;`ADR-CEP-01`/`ADR-CEP-03` 在 15 组里无组 |
| 新增 StreamAdapter 子类 / factory 抽象层 | `01 号 §1.1`「不需新增抽象层」 |

---

## Capabilities

### 新增 capability:`cep-cohif-carrier`

载体面 —— `CohIfBundle` 字段契约 + ProtocolEngine 的 ChStream 端口面 + JSON 装配连通性。

### 新增 capability:`cep-protocol-bridge-contracts`

跨实例复用的平台级契约面 —— `ProtocolBridge` 可逆性 + per-bridge 元数据 + 「注册 + 消费成对」纪律。后续 7 个 adapter change **复用**该机制,只填各自的行。

> **与队列 #6 的 `cep-adapter-schema-contracts` 不同层**:后者是**协议实例声明层**的契约(`perm` / `perm_upgrade` / 6→3 映射表 / ⑥ 镜像闸);本 change 的是**特化桥层**。⇒ 两者 capability 名不同、判据不重叠(CC-06 **引用**队列 #6 的 AC-04,不重立)。

---

## Acceptance

见 2 份 delta spec 的 Scenario 清单(共 **6** 条 = 3 + 3,编号对齐 `ADR-CEP-19` 的 CC-01~CC-06 —— 但该 ADR **只引用编号、不定义判据**,定义真源在 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md §6.4`;其中 **2 处为 planner 增补待追认**(CC-05 ④ / CC-06 ②),完整口径与追认路径见 `design.md` 的 `D-η`)。**判据退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**(per 队列 #5 `Entry #68` 的红绿分离纪律)。

**DoD 引用**:`obj-cep-platform §6.2` B1~B8 + §6.3 D1~D3 + `ADR-CEP-00 §2 决策 3` + `ADR-CEP-19` 决策 1~9。

---

## 依赖与风险

| 项 | 类型 | 缓解 |
|---|------|------|
| **KPI slot 未释放** | **硬前置阻塞** | 队列 #6 归档后转正;4 件套已 staged 于 `.rddf/drafts/`,转正是 `git mv` + 删标注 + `openspec validate` |
| `06 号 §3.2` 归类表须同步第三类 | 契约面 | 由 **builder** 同步(归类表在真源 1,不在冻结集) |
| `module_factory.cc` 核心路径改动 | 框架层 | 6 标签回归 + `ctest` 全量 |
| `N=1` 落 single 分支 ⇒ `bind_ports` 空 shim 静默不绑定 | 正确性 | 宏内 `static_assert(n > 1)`;CC-04 覆盖 |
| 6×6 状态机不做跨仓逐格镜像 ⇒ 除 `state_transition[2][1]==3` 外漂移无人发现 | 一致性 | 缺口已在 `ADR-CEP-19` 决策层诚实登记;**本 change 不重复登记** |
| Stub↔PE 腿延期 ⇒ 「ChStream 承载业务 bundle」业务侧证明不完整 | 范围 | 延期项**有 owner 与前置**,非「以后再说」;本队列闭合 **PE→网络** 段 + PE 输入侧类型契约 |

---

## 转正命令(slot 释放后)

```bash
# 前置:openspec list --json 实测 CEP 内部 active == 0(队列 #6 已归档)
git mv .rddf/drafts/cpptlm-cep-cohif-carrier openspec/changes/cpptlm-cep-cohif-carrier
# 删各文件顶部的「未建 change 目录」标注段(状态行改 🟡 Proposed)
openspec validate cpptlm-cep-cohif-carrier --strict   # 期望 rc=0
# 台账追加 Entry(记录转正 + KPI 0/3 → 1/3)
```

---

## 关联

- 台账:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #74**(排队 + 授权)· 本 change 的转正 Entry 待建
- 架构重审:`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3)
- 决策:`docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md`(✅ v1.0 已签发)
- 变更件:`.rddf/drafts/cpptlm-cep-cohif-carrier/`(proposal / design / 2 份 delta spec)
