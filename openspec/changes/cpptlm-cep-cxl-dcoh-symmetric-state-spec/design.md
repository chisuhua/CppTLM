# Design — cpptlm-cep-cxl-dcoh-symmetric-state-spec

> **口径**: 台账 **Entry #154**(4 项 architect 裁决)· **#153**(Task 0 普查 + 验证矩阵)· **#122**(CXL 口径裁决)
> **范围**: **1 个新增 header-only 载体** + 判据 —— **零 ADR 正文改动 / 零既有代码改动 / 零 ABI**

---

## 1. 根因与前提

### 1.1 为什么需要**第 0 个 change**(规约层)

平台已落地的 5 个实例全部是 **host-centric 或中心化目录**语义;CXL 3.0 对称(DCOH)是
「**设备侧自带一致性代理 + SF/目录 + 可对 host 发起反向 snoop**」的第四种拓扑
⇒ 实现层若先落,必然**自推**语义(重蹈 #9「看似完整实则杜撰」的覆辙)
⇒ **规约层先行**(照 #9 / #10 / #11 先例)。

### 1.2 规约层必须**钉死**的 5 项(普查已给)

| # | 项 | 结论 | 可锁? |
|---|---|---|---|
| 1 | 设备侧状态空间 | **3 套**:`DevCacheState`(4:M/E/S/I)· `DcohViewState`(3:I/S/A)· `HomeDirState`(3:I/S/E) | ✅ |
| 2 | 事件词表 | CXL.mem **6** 通道(4 基础 + `S2M BISnp` + `M2S BIRsp`)+ CXL.cache 双向各 3;⭐ `device_initiated_snoop_set == {BISnp}` | ✅ |
| 3 | `perm` 投影 | **按域分离**:仅 `DevCacheState` → 4→4;DCOH/home 的 3 态**不入** `perm` | ✅ |
| 4 | 「对称」定义 | 窄义 **5 谓词**合取(P1~P5) | ✅ |
| 5 | 编译期断言 | 见 §3 | ✅ |

### 1.3 已被**反证**因而**不可锁**的 1 项

`aggregation_at` 的**二元** ⇒ 裁决为**恒 `HOME`**(per Entry #154 ①)
⇒ ✅ **可锁**(若未裁决则只能标 pending)。

---

## 2. 方案对比(载体形态)

| # | 方案 | 优点 | 代价 | 判定 |
|---|---|---|---|---|
| **A** | **单一载体 + 3 套状态空间并存** | 一个文件交付 | ⚠️ 三套语义共处 ⇒ **一文件两真相源**风险;`perm` 投影的**来源域**须显式标注 | ❌ |
| **B** | **3 个载体(各一套状态空间)** | 每套**单一真相源**;编译期断言可**逐套**独立 | 交付面 3 倍;跨套的一致性(如 DCOH 视图 ↔ 缓存态)**无编译期保证** | ❌ 过度分散 |
| **C** | **单一载体 + 3 套状态空间 + 显式「域分离」约束**(编译期断言三套**元素集合互不相交** + `perm` 只从 `DevCacheState` 投影) | 一个文件;**编译期**保证「不合并」与「perm 不折叠」;跨套关系用**文档 + 逐格 basis** 表达 | 跨套关系**无编译期锁**(登记为已知边界) | ✅ **裁决采用** |

⇒ 另参 architect 裁决 #4:载体侧的 **adapter** 形态 = **独立 `CxlDcohAdapter`**(不扩 `CxlCacheAdapter`)
⇒ 本 change 只管**规约载体**,adapter 归第 1 个 change。

---

## 3. 编译期断言设计(V-1 ~ V-6)

```
// V-1 三套状态空间**不合并**
kDevCacheStates == 4  ∧  kDcohViewStates == 3  ∧  kHomeDirStates == 3
∧ (DevCache ∩ DcohView) == ∅  ∧ (DevCache ∩ HomeDir) == ∅  ∧ (DcohView ∩ HomeDir) == ∅
// ⚠️ 元素集合用**枚举名**比较(`I` 在三套里都出现 ⇒ 必须**按域**比较,不是按名字)

// V-2 通道
kCxlMemChannels == 6   // 4 基础(M2S Req/RwD + S2M NDR/DRS)+ S2M BISnp + M2S BIRsp
kCxlCacheChannelsPerDirection == 3   // Req / Rsp / Data
// ⚠️ **不写死 opcode**(R-S1 pending)

// V-3 ⭐ 对称判别式
device_initiated_snoop_set == { BISnp }      // 非空 ∧ size == 1
// ⚠️ **禁止沿用队列 #10 的「设备侧无 snoop 发起」断言**(那是 CXL 2.0 的形态)

// V-4 `perm` 只从缓存代理域投影
perm_rows == 4  ∧  (∄ F 行)  ∧  (DcohView / HomeDir 的元素**未**出现在 perm_rows)

// V-5 窄义「对称」5 谓词
cxl3_symmetric(p) := P1 ∧ P2 ∧ P3 ∧ P4 ∧ P5
//   P1 ∃ device-originated BI snoop channel(S2M BISnp),target = host
//   P2 hosting device 实现覆盖其 HDM 区间的 SF / 目录
//   P3 硬件一致性跨 ≥ 2 个独立 host
//   P4 P2P 访问不经 host CPU
//   P5 被 snoop 的是 caching host,**不是** peer device cache
// ⚠️ **禁止**写成 peer-to-peer 全对称(公开来源明写 CXL 3.0 **不满足**)

// V-6 local / remote
channel_direction(local) ∩ channel_direction(remote) == ∅     // 通道级
// ⚠️ **事件种类级不相交不成立**(两侧都有 snoop 种类)⇒ 断言只锁**通道级**
```

---

## 4. 逐格 `basis` 的**两轴规则**(照 #10 §8.2)

| 轴 | 取值 | 含义 |
|---|---|---|
| `kind`(证据强度) | `public-direct` · `public-analogical` · `self-derived` | **禁止升格** |
| `pending`(钉版强度) | `true` / `false` | `pending=false` SHALL **同时**满足 ① `pin=strong` **且** ② **许可已人工确认** ⇒ 本轮仅 **S5 / S7**(CC-BY 4.0)可支撑 |

⇒ 载体每格 `basis` = `{kind, pending, ref}`;`ref` **指向锚点摘要条目号**(**不**指向规范原文)
⇒ **10 项 pending**(R-S1 ~ R-S9)逐条登记 ⇒ **禁止推理填空**。

---

## 5. 后果 / 代价

| 面 | 后果 |
|---|---|
| **载体** | 新增 `include/tlm/cep/cxl_dcoh_state_spec.hh`(header-only)· **零既有代码改动** |
| **可锁结构** | 5 项载体形态决定项中的 **4 项** + 聚合点(经裁决)⇒ **6 组**编译期断言(V-1 ~ V-6) |
| **未锁** | opcode / 字段宽度 / credit / 排序 / block 粒度 / SF 精度 / transient / 错误语义 ⇒ **10 项 pending** |
| **跨套关系** | ⚠️ DCOH 视图 ↔ 缓存态 / home 目录 ↔ 缓存态的**对应关系**无编译期锁 ⇒ 已知边界(登记) |
| **合规** | 载体**零**规范正文引用;`basis.ref` 只指向**锚点摘要** ⇒ AI 不接触规范 |
| **下游** | 第 1 个 change 的 `CxlDcohAdapter` **只消费**本载体 ⇒ 规约一改,实现**编译期即红**(零副本原则) |
| **KPI** | 1 slot(规约层归档后才建实现层 ⇒ 串行) |

---

## 6. 冻结面

- `ADR-CEP-*` / `protocol_instance.schema.json` / `cep-sharer-tracker-base` —— **零改动**
- 新增载体**不在** M1 冻结集内 ⇒ **0 件** ⇒ **无需两步授权** —— ⚠️ **须 Phase 0 实测确认**
