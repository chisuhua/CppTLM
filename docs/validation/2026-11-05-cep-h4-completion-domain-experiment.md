# CEP 实验：H4（完成域判别规则容纳 coherence + collective）首轮结果

**日期**:2026-11-05 · **角色**:architect（形态裁决）· builder（装置实现 + 产出）· planner（表落）
**性质**:假设验证实验（`obj-cep-platform.md §9.1` 的 H4）
**装置**:`test/cep_completion_domain_model.hh`（新建）+ `test/test_cep_h4_completion_domain.cc`（新建）
**关联**:台账 **Entry #222**（形态裁决）· **#221**（H1/H5 登记）· **#218**（§9.1 改造 + 预登记载体）
· `ADR-CEP-01` **§3.2**（两套完成纪律不能混用同一分析工具）· **§6.2 代价 3**（分类错误 = 静默死锁）
· `ADR-CEP-06` **SU#1** / **SU#2** · `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`（同族先例）

---

## 0. 一句话结论

**H4 判 ✅ 成立**（模型级）—— 正控两类流同实验内**无死锁**（coherence 8/8、collective 8/8 全部完成），
反事实负控（coherence 被错标 group-scoped）⇒ **死锁**（两端均完成 0/0）⇒ **完成纪律混用 = 静默死锁**在模型上可判定。

⚠️ **本结论是模型级**（见 §4 口径限定），⛔ **不是**协议级证明。

---

## 1. 判据来源（逐字）

| 假设 | 验证手段 | 成功判据 |
|---|---|---|
| **H4** | 同实验内 coherence 流 + collective 原语 | **两类流同实验内无死锁** |

> ⚠️ **依赖观测列已订正**(2026-11-05,per 台账 **Entry #222** ①):原写「判别由 `evaluate_completion_domain` 承担」
> 是 **Entry #218 拆表时的层次归属错误** —— 该函数是 **CP-05 契约面**判据(`std::ifstream` 读 coherence 侧**源文件**、
> 查 8 个 group-scoped 标识 ⇒ **零运行期含量**),而 H4 判据是**运行期**命题。
> ⇒ 二者**互补守护面**(CP-05 守源文本扫描面 / H4 守运行期共存面),**非替代**。

---

## 2. 实验装置（新建文件，均在 `test/`，**不入** `include/tlm/cep/`）

| 文件 | 内容 |
|---|---|
| `test/cep_completion_domain_model.hh` | `CompletionDomain{RequesterScoped, GroupScoped}` + `CompletionDomainModel` —— 两类流同实验推进,按各自**完成纪律**决定何时完成 |
| `test/test_cep_h4_completion_domain.cc` | 正控 + **反事实负控** + 区分性 + 口径限定的机器可查锚 |

**死锁机制 = 循环等待**(per `ADR-CEP-01 §6.2` 代价 3):
- collective 成员的**到达**依赖 coherence 请求**先退休**(跨平面 ordering/visibility,per `ADR-CEP-03 §3`);
- coherence 请求若被错标为 group-scoped,其**完成**又依赖「全组到达」
- ⇒ **coherence 等 collective 到达,collective 等 coherence 退休** ⇒ 循环等待 ⇒ 死锁。

**正控**(充分性):coherence = requester-scoped(单点 ack,不依赖 collective)+ collective = group-scoped
⇒ 无循环 ⇒ 两类流共存且全部完成。
**负控**(必要性 · **装置合格的前提**):coherence = group-scoped ⇒ 死锁。
⛔ **负控不是「coherence 也 blocking」** —— 那是「更多 blocking 更易死锁」的废话(h6 领域)。

---

## 3. H4 结果

### 3.1 正控(requester-scoped coherence + group-scoped collective)

| 指标 | 实测 | 判据 |
|---|---:|---|
| coherence 完成数 | **8/8** | 全部完成 |
| collective 完成数 | **8/8** | 全部完成 |
| 死锁 | **否** | 两类流同实验内无死锁 ✅ |
| 完成语义混淆 | **否** | 正控无混淆 |
| `collective_stalls` | **0** | 隔离 VC ⇒ collective 的等待不落在 coherence 的 VC 上 |

### 3.2 反事实负控(coherence 错标 group-scoped)

| 指标 | 实测 | 判据 |
|---|---:|---|
| 死锁 | **是** | 分类错误 ⇒ 静默死锁 ✅ |
| coherence 完成数 | **0/8** | 循环等待 |
| collective 完成数 | **0/8** | 循环等待(**两端都不动** —— 这是死锁的直接证据) |
| 完成语义混淆 | **是** | 负控成因标记 |
| `collective_stalls` | **0** | ⚠️ **隔离 VC(带宽充足)下仍死锁 ⇒ 排除「带宽竞争」解释** ⇒ 死锁成因是**完成语义**,非资源竞争 |

### 3.3 判据分支的测量状态

| 判据分支 | 依赖观测 | 测量状态 |
|---|---|---|
| 两类流同实验内无死锁 | 正控的 `deadlock_violations` + 完成数 | ✅ **已测量**(8/8 + 8/8 + 死锁=否) |
| **判别规则确实改变共存结果**(因果) | 负控 vs 正控的差 | ✅ **已测量**(负控死锁 + 两端 0/8) |

---

## 4. 口径限定（⚠️ 必须随任何引用重复）

1. **模型级**,非协议级 —— 是**模型**上的完成语义对比,⛔ **不得**表述为「已证明协议层无误分类」。
2. **非完整仲裁器** —— 无 credit 环流 / 无多级仲裁 / 无跨 VC 依赖图
   (完整仲裁属 **Stage 4+**,per `ADR-CEP-09` / `objective §8.1` R4)。
3. **`ADR-CEP-09` SU 2026-11-02 §④** 的「`GrantAck` 可能被 `Grant` 饿死**未建模**」——
   本实验**不覆盖** ⇒ 引用时必须**原样带上**。
4. **隔离 VC 前提** —— 正控用隔离 VC(VC 隔离由 **H6** 承担,H4 在其**前提下**测完成域维度);
   若用共享 VC,h6 已证带宽竞争死锁会污染正控 ⇒ 无法孤立完成域效果。
5. **与 CP-05 互补非替代** —— CP-05 `evaluate_completion_domain` 守**源文本扫描面**(读 coherence 侧源文件查
   group-scoped 标识);本实验守**运行期共存面**。
6. **负控是装置合格的前提** —— 若去掉负控,本实验退化为「两类流同实验没死锁」= **H6 换名**
   ⇒ ⛔ **负控红不了 ⇒ 装置不合格**。

---

## 5. 反向 fixture(判据「因正确原因红」的实测)

| # | 注入 | 实测红因 |
|---|---|---|
| 1 | 去掉模型的循环等待依赖(`coherence_all_retired` 恒 true) | `FAILED: 负控必须死锁:否则装置没建模「完成语义混淆 → 死锁」机制` + 区分性用例同红 ⇒ **证明负控确实在检验死锁机制本身** |

⇒ **负控真能抓装置缺陷**:移除机制后负控红不了并被**指名**。

---

## 6. 本轮顺带暴露的 1 项实质遗漏(由预登记闸抓出)

**H6 的预登记 out-of-scope 清单漏了 `coherence_stalls` 与 `memory_violations`** ——
实测 h6 的这两个 metric **无产出者**(h6 只测 deadlock + collective_stalls)⇒ 本应标 `not-collected`,
但 Entry #218 定的清单不含它们 ⇒ tools 判据④ 判红。
⇒ **已补正**(真源 ② 表 + 机器投影同改,per `ADR-CEP-06` SU#2 ② 门禁② 的 drift 约束)。
⇒ ⚠️ **这正是预登记闸的用途**:它把「清单与实测的脱节」变成**可判红**的,而非靠人工核对发现。

---

## 7. 4 件套链路

```bash
bash scripts/test/cep_experiment_run.sh
# → experimental/run-<ts>-h2h3h4h6/{h2,h3,h4,h6}/ 各 4 件套
#   simulation_config.json + trace.h5 + metrics.json + report.html
#   render_report.py --check 逐目录通过
```

`trace.h5` 由 `h5py` 写为**真实 HDF5**,内容为 **run 级摘要**(本模型不产逐周期信号),
文件 attributes 显式标注 `grain = "run-summary"` 与说明文字 ⇒ **不冒充** cycle-level trace。

**h4 的 metric 语义**:`deadlock_violations` = **正控**的实测(应为 0)——
负控的死锁是**设计预期的证据**,写进 `verification_scope` 文本,**不进** metric 槽位
(否则 tools 的 IC3 闸会把 h4 判成「实验 fail」⇒ 掩盖正控的结论)。

⚠️ **`verification_scope` 的载体性质**(per `ADR-CEP-06` SU#3 §②B(i)):散文指针属 **SU#2 §①「被试者自证面」**
家族 —— 仅作**指针**,⛔ 不作证据。H4 负控因果的**机器锚** = 同 PR 的 `[cep][h4]` 测试
(含「负控未死锁 ⇒ emit 拒绝产出」的产出层硬约束)+ §5 反向 fixture,二者独立于散文。

---

## 8. Verdict

| 假设 | verdict | 计数影响 |
|---|---|---|
| **H4** | ✅ **成立**(模型级,per §4 口径限定) | §6.2 **B7** 计数 **+1** |

⇒ **B7 计数由 6/8 → 7/8**(H1 / H2 / H3 / H5 / H6 / H7 + 本次 H4)。
⇒ **H8 仍未测量**(A/B 数据未产)⇒ **B7 仍未达成**。

**复现**:
```bash
cmake --build build -j4 --target cpptlm_tests
./build/bin/cpptlm_tests "[cep][h4]"                    # 4 cases / 15 assertions
bash scripts/test/cep_experiment_run.sh                 # 4 组 4 件套 + 契约校验
```
