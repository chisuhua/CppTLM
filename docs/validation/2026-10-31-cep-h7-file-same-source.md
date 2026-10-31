# H7 对拍 —— 文件同源主对拍档实测(A-1 形态)

> **日期**:2026-10-31 · **档位**:主对拍档(文件同源) · **状态**:✅ 实测完成
> **结论**:**H7 在【声明范围:均匀随机负载】内成立**;
> ⚠️ **不得**表述为「H7 已验证」(per `docs/architecture/14-coherence-parity-baseline.md` §4.4 的表述纪律)
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md` Entry #120
> **判据规格**:`14 号` §4.4(封闭枚举表 + 按 class 分列 + 分层措辞)
> **前序报告**:`-parity-first-round.md`(常数档)· `-same-source-trace.md`(同源 trace 档)·
> `-main-tier.md` / `-main-tier-a.md`(主对拍档,口径 A)· `-scope-qualified-conclusion.md`(A-2 收口)

---

## 1. 摘要

本轮把 H7 从「**A-2 范围限定声明**」(只声明适用范围、**未测出边界**)
推进到「**A-1 鉴别力自检**」形态 —— **边界被实测、失效被观测**:

| 层 | 判定对象 | 证据 | 结论 |
|---|---|---|---|
| **模型本体** | 「固定命中率参考模型」在**给定 h** 时的延迟预测 | Entry #117 §⑤:`h_0 = h_m` 时最大偏差 **4.40%** | ✅ **准确**(h 对齐下) |
| **估算规则 R-B** | 「**从 workload 特征估 h**」这条规则 | 本轮正控:`breaking` 流上误差 **15.54pp / 72.34pp** | ❌ **仅均匀随机成立** |

⇒ **唯一允许的表述**:
> 「H7 在**【声明范围:均匀随机负载】内成立**;在**非均匀负载**上**估算规则 R-B 失效**
> (⇒ early-stage 无实测 h 时该规则不可用),而**模型本体**在 h 对齐下仍准确(偏差 ≤ 4.4%)。」

⚠️ **禁止**:「H7 已验证」/「H7 成立」(不带范围限定)/「**参考模型**对非均匀负载不可用」
(⇒ 把**规则失效**误写到**模型本体**上)。

---

## 2. 本轮推翻了 3 条已写入台账的结论

### 2.1 「gem5 v24 `ProtoInputStream` 提前 EOF = 上游缺陷」—— ❌ 错

台账 Entry #115 写入的该结论**被实测推翻**:

| 证据 | 内容 |
|---|---|
| C++ 复现 | 用 gem5 **完全相同**的读法(`ReadVarint32` + `PushLimit` + `ParseFromCodedStream`)<br>两种底层流实现(`IstreamInputStream` / `FileInputStream`)都**读满 1000/1001 条** |
| gem5 内 DIAG | `read` 读到 #800 成功、**从未出现失败** |
| 修正后验证 | `MAX_TICK` 改为「条数 ×(名义间隔 + 单次响应时间上界)」⇒<br>n=100 → 100/100 · n=1000 → 1000/1000 · n=100000 → **100000/100000** |

⇒ **真根因 = 对拍探针的 tick 预算不足**:
名义 tick 间隔 10000 ticks,但**实际**每条耗时 ≈ **10100 ticks**(含背压)
⇒ `MAX_TICK = 1.02e7` 只够发 ~965 条 ⇒ 表现为「gem5 只读到 N 条 < 名义条数」
⇒ **看起来像** trace 读取截断。

### 2.2 「gem5 v25 已重写 `src/proto/protoio.cc` ⇒ 提前 EOF 根因已消除」—— ❌ 错

台账 Entry #119 §⑤ 的该结论**被实测推翻**:v25 的**读侧** `ProtoInputStream::read`
与 v24 **逐字相同**(此前误把**写侧**改动当成了读侧修复)。

⇒ **决定性验证**:**上游原版 v25(零本地补丁)**读满 **100000/100000** 条
⇒ v25 的本地补丁(读侧改非零拷贝 + `block_size` 覆盖整文件)**非必需**
⇒ 已把 `external/gem5-v25/src/proto/` **还原为上游原版**并重建验证
(`diff -rq` 与 tarball 一致;DIAG 打印随之清零)。

### 2.3 「v25 + 补丁是解除 H7 限定的技术前提」—— ❌ 不成立

解封的真实前提 = 「能喂**非均匀离线 trace**」⇒ 靠 `createTrace` 读**文件**(v24 即支持)
⇒ **与 gem5 版本无关**。
⇒ v25 在本路径上**价值为 0**;保留 v25 只因**已构建并验证过**,而非「它修好了什么」。
⇒ ⚠️ **v24 未复测**(其 build 已删,重建约 1.5h/4GB)⇒ 「v24 同样可用」属**推断**,非直接实测。

---

## 3. 本轮发现并修掉的 6 个缺陷

| # | 缺陷 | 现象 | 修法 |
|---|---|---|---|
| **D-1** | trace 探针漏算 `avg_latency_ns` | P-2 的分母为 `None` ⇒ 分析器崩 | 按 `gem5_linear_probe.py` **同口径**补算(命中=派生 / 未命中=实测) |
| **D-2** | 流名含 `.` | `Path(out).with_suffix(".csv")` 把 `.25` 当扩展名剥掉 ⇒ 产物找不到 | 流名改 `HC_hot0p25`(并加注释说明该约束) |
| **D-3** | gem5 **丢弃非 mem range 的包** | warn `suppressed N packets with non-memory addresses` ⇒ gem5 访问数 25128 vs 本仓 100000 | `gem5.memory.range` 16MiB → **32MiB**(覆盖双峰流冷区) |
| **D-4** | 产物**版本错标** | `gem5.version` 停在 `v24.1.0.0` 而实际跑 v25 ⇒ 全部产物错标 | 唯一真源 = `gem5.versions.default.version`;三个探针改读它 + **启动自检**(不一致 ⇒ 报错退出) |
| **D-5** | **名义条数断言是死代码** | 探针写 `sequence.nominal_accesses`、rec 工具读 `seq.get("num_accesses")` ⇒ **键名不匹配** ⇒ 断言恒跳过;且 trace 探针**根本没有 `sequence` 段** | 键名对齐 + trace 探针补 `sequence` 段 + **断言按 driver 分档**(见 §5) |
| **D-6** | **verdict 聚合自相矛盾** | 「规则失效型流」由**观测结果反推**(`not p3_ok`),而 `passed` 又要求**全部**流通过 ⇒ 合取 ⇒ **A-1 一旦满足,`passed` 恒 False** | 按 class 分列重构(见 §4) |

⇒ D-6 是**最阻断**的一条:不修则重跑产物必然是「H7 在口径 A 下不成立」的错误文案 + exit 1。

### 3.1 门禁复跑时又暴露的 4 条(同一根因族:**口径/路径未单一真源**)

| # | 缺陷 | 现象 | 修法 |
|---|---|---|---|
| **D-7** | 本仓侧**读写条数统计失真** | `n_reads = all_read ? issued : 0` ⇒ **混合读写**的 trace 下完全失真(实测 trace 含 20% 写 ⇒ 报 `r=100000/w=0` ⇒ **P-1b 假红**) | 按**实际消费的序列**逐行统计(`csv_rows` 的 `is_write`) |
| **D-8** | 3 个脚本**未注入 P-5 字段** | `run_parity_trace.sh` / `run_parity.sh` 未导出 `PARITY_VERIFICATION_MODE` / `_SCOPE` ⇒ 本仓侧输出空 ⇒ **P-5 假红**;且注入块里用了**未定义**的 `$CFG` | 三个脚本统一注入(取值来自 `parity_config.json` 的 `verification` 段) |
| **D-9** | `build_gem5.sh --check` 用**相对路径** | ctest 从 `build/` 目录调用 ⇒ 解析到 `build/external/...` ⇒ 误报「二进制不存在」⇒ **e2e 假红** | 全部改为**以仓库根 `$ROOT` 为基准的绝对路径** |
| **D-10** | 4 个脚本**硬编码 v24 的 gem5 路径** | v24 build 删除后 ⇒ e2e **静默 SKIP**(exit 77)⇒ **门禁失守**(ctest 显示 Skipped 而非 Failed) | gem5 路径统一从 `gem5.versions.default` 推导(唯一真源) |

⇒ D-9 / D-10 揭示了一个**治理级教训**:**「SKIP」不等于「PASS」** ——
e2e 因第三方资产缺席而 SKIP 时,必须另有机制证明「资产在时它真会跑」
本轮由「手动跑脚本 ⇒ 全绿」补上了这一环。

---

## 4. 判据的「按 class 分列」重构(per `14 号` §4.4)

### 4.1 封闭枚举表(profile → class)

| profile(声明) | class | 语义 | 角色 |
|---|---|---|---|
| `uniform` | **conforming** | 均匀覆盖 `[base, base+W)`(与 gem5 `createRandom` 同语义) | **必须全过**(= H7 判定域) |
| `hot_cold` | **breaking** | 双峰(热区全命中 + 冷区全不命中) | **必须 FAIL**(正控必须激发) |
| `fixed_ratio` | **观察** | 80% 内 + 20% 外(足迹 **1MiB+W**)⇒ R-B 误差随 W/C 连续 | **不参与判定** |

⚠️ **分类由「声明」决定,不由观测结果反推** ⇒ 标准**不可证伪**的风险被消除
(否则一条真均匀流若真挂了会被自动改判为 breaking)。

### 4.2 为什么 `fixed_ratio` 是「观察」而不是「breaking」
它的 R-B 误差随 W/C **连续变化** ⇒ 某些参数下误差 < 5pp
⇒ 若列为 breaking 则**正控不激发 ⇒ 误红**;若列为 conforming 则**语义错误**。

### 4.3 `passed` 的新定义
```
passed = (conforming 非空) ∧ (conforming 全过 P-2 ∧ P-3)
       ∧ [ (breaking ≥ 1 ∧ breaking 全部正控激发)      ← A-1
         | (breaking = 0 ∧ applicability ∧ limitation) ] ← A-2
```
⇒ ⚠️ **A-2 只在 `breaking` 为空时适用** —— 否则「正控未激发」会被 A-2 兜底**掩盖**
(该 bug 由新增反控 ⑫ 实测抓到并已修)。

---

## 5. 条数契约(P-1)按 driver 分档

⚠️ `nominal_accesses` **不是所有 driver 的条数契约**:

| driver / 档位 | 条数来源 | P-1 判定 |
|---|---|---|
| **trace 回放**(文件同源档) | trace 文件本身(1 条 = 1 个包) | **必须精确相等** |
| **gem5 `createRandom`**(参数同源对照档) | `duration`(条数由 `duration`/`tick_budget` 决定)⇒ 实测可达名义的 **17~20 倍** | **不做条数断言**(超发是该路径的**设计行为**,不是失控) |

⇒ 若统一施加条数上界断言 ⇒ 参数同源对照档**每条流恒红** ⇒ 不可复现。
⇒ Entry #118 §⑤ 宣称的「序列完整性硬断言兜底」**从未生效**(D-5)⇒ 已如实登记。

---

## 6. Entry #118 的 5 条流:复核结论 = **数字成立**

原始产物已快照入 `docs/validation/raw/main_W16384.json` 等 5 份(同目录)。

| W | gem5 实际 range | 实际 accesses | misses | `h_m` | C/W |
|---|---|---|---|---|---|
| 4096 | `[0x100000,0x101000)` | 1999994 | 69 | 0.999965 | 1.0 |
| 8192 | `[0x100000,0x102000)` | 1999987 | 130 | 0.999935 | 1.0 |
| 16384 | `[0x100000,0x104000)` | 1999982 | **258** | 0.999871 | 1.0 |
| 32768 | `[0x100000,0x108000)` | 1949459 | 974058 | 0.500344 | 0.5 |
| 131072 | `[0x100000,0x120000)` | 1753058 | 1532998 | 0.125529 | 0.125 |

⇒ **W 标签与 gem5 实际地址范围完全一致** ⇒ `h_m ≈ C/W` 是**解析解**而非造假:
**连续**地址块 + 模索引 + **2-way** ⇒ W=C 时每组恰好 2 块 = 相联度
⇒ 完美装下、永不驱逐 ⇒ `h_m ≈ 1`(misses=258 ≈ 256 块 warmup + 2);
W=2C ⇒ 每组 4 块、LRU 保 2 MRU ⇒ h=0.5;W=8C ⇒ 16 块/组 ⇒ h=0.125。
⚠️ 「W=C ⇒ h≈0」是**线性循环扫描**的情形(Entry #118 报告 §③ 自己也写了
「线性是 LRU 最坏情况」),**不是**均匀随机。

⇒ **决定性对照**:本轮新增「真 uniform」profile 在文件同源路径下 W=32768
⇒ `h_m = 0.501570`(误差 0.157pp)vs Entry #118 的 0.500344 ⇒ **两路径一致**
⇒ 证明本轮此前的 P-3 FAIL 是 **workload 语义不一致**造成的假象。

### 6.1 复核同时发现 2 处口径缺陷
① `nominal_accesses`=1e5 而实际 `accesses`=1.75e6~2.0e6(17~20×)
⇒ **P-1 在参数同源档实际未受约束**(两侧都跑 2e6 ⇒ 侥幸相等)⇒ 见 §5;
② **R-B 规则成立前提未声明**(均匀随机 + 稳态 + 连续块模索引 + 2-way)
⇒ **P-3 在此组合下不验证替换策略** ⇒ 前提已补入 `14 号` §4.4。

---

## 7. 主对拍实测结果(文件同源,5 条流 × 1e5)

两侧**读同一份 CSV**(gem5 回放由它生成的 `.pb`)⇒ 地址序列指纹(FNV-1a 64bit)**逐条核对通过**。

| 流 | class | W(B) | `h_m` | `h_0` | 误差pp | P-3 | `L_gem5` | `L_model` | 偏差 | P-2 |
|---|---|---|---|---|---|---|---|---|---|---|
| W16384 | conforming | 16384 | 0.9974 | 1.0000 | 0.26 | ✓ | 5.110 ns | 5.000 | 2.15% | ✓ |
| W32768 | conforming | 32768 | 0.5016 | 0.5000 | 0.16 | ✓ | 26.432 ns | 27.500 | 4.04% | ✓ |
| W131072 | conforming | 131072 | 0.1242 | 0.1250 | 0.08 | ✓ | 42.659 ns | 44.375 | 4.02% | ✓ |
| HC_hot0p25 | **breaking** | 1052672 | 0.1710 | 0.0156 | **15.54** | **✗(正控激发)** | 40.647 ns | 49.300 | 21.29% | — |
| HC_hot0p75 | **breaking** | 1052672 | 0.7389 | 0.0156 | **72.34** | **✗(正控激发)** | 16.226 ns | 49.300 | 203.84% | — |

⇒ **P-2a = A-1** ✓(breaking 2 条,正控全部激发)
⇒ **verdict = PASS** ⇒「H7 在【声明范围:均匀随机负载】内成立」
⇒ ⚠️ 这是 **D-6 修复的实证**:旧实现下该场景 `passed` **恒 False**。

### 7.1 一条值得登记的真实 cache 效应
`HC_hot0p25` 的 `h_m = 0.1710` **低于**声明的 `hot_ratio = 0.25`
⇒ 冷区流量**在组内驱逐了热区行**(热区 64 blocks 只占 128 组中的 64 组,
冷区扫全部组)⇒ ⇒ **`hot_ratio` 不能直接当 `h_0` 用**。

---

## 8. 判据自测(反控)

`scripts/cep/parity/selftest_parity_compare.py`:**37 项全绿**(原 23 项 + 新增 14 项)。

新增 5 条(per critic 指定):

| # | 反控 | 断言 |
|---|---|---|
| ⑩ | `conforming` 流 P-2 挂 | ⇒ FAIL |
| ⑪ | 零 `breaking` 流 | ⇒ 形态 = A-2 |
| ⑫ | `breaking` 流**意外通过** P-3 | ⇒ FAIL(正控未激发)且**不得**判为 A-1 |
| ⑬ | `conforming` 流 P-3 挂 | ⇒ FAIL 且**不得**被计入 `breaking` |
| ⑭ | A-1 形态下 conforming 全过 + breaking 正控激发 | ⇒ **PASS**(D-6 的正解) |

⇒ ⚠️ 反控 ⑫ 在实现中**真的抓到一个 bug**(A-2 兜底掩盖正控失效)⇒ 已修。

---

## 9. 复现命令

```bash
# 文件同源主对拍(5 条流:3 conforming + 2 breaking)
PARITY_NUM_ACCESSES=100000 bash scripts/cep/parity/run_parity_main_trace.sh

# gem5 就绪检查(唯一真源 = parity_config.json 的 gem5.versions)
bash scripts/cep/parity/build_gem5.sh --check

# 判据自测(37 项)
python3 scripts/cep/parity/selftest_parity_compare.py
ctest --test-dir build -R cep_parity

# 参数同源对照档(Entry #118 的来源;条数不做断言)
bash scripts/cep/parity/run_parity_main.sh

# 门禁
bash scripts/test/cep_freeze_check.sh
bash scripts/test/cep_docs_sync_check.sh
bash scripts/test/cep_doc_ref_check.sh
bash scripts/test/cep_gate_selftest.sh
bash scripts/test/docs_sync_check.sh --strict
```

---

## 10. 残余与局限(诚实登记)

| # | 项 | 状态 |
|---|---|---|
| 1 | **D-4 协议开销不对齐** | 🟡 已知上限 ⇒ 长期口径:本平台是**模型级**延迟比较,**不是**周期精确验证 |
| 2 | **P-2 的 15% 阈值未实测标定** | 🟡 取自 `§9.1 H7` 的既有口径;本轮实测偏差 2.15~4.04%(余量充足)⇒ 阈值**偏保守但未标定** |
| 3 | **「声明 profile ≠ 已验证分布语义」** | 🟡 分类按声明 ⇒ 由**正控 margin** 兜底(当前 15.54pp / 72.34pp ≫ 5pp = 3~14 倍);未来新增近边界参数流须在流声明里写明 `expected_error_floor_pp` |
| 4 | **gem5 侧「实际消费序列」指纹未做** | 🟡 当前指纹从 **CSV** 重算 ⇒ 证的是「文件可读」而非「gem5 按序消费」;条数相等 + `hits+misses=accesses` + 读写条数交叉已够用 |
| 5 | **v24 未复测** | 🟡 「v24 同样可用」为**推断**(MAX_TICK 根因 + C++ 复现双证);v24 build 已删,重建约 1.5h/4GB |
| 6 | **R-B 只在「连续块 + 2-way + 稳态」下成立** | 🟡 换替换策略(1-way/4-way/MRU)结论会变 ⇒ **P-3 不验证替换策略** ⇒ 已写入 `14 号` §8 #14 |
| 7 | **`fixed_ratio` profile 未参与任何判定** | 🟡 保留仅为守住 #114 起同源 trace 档的字节级基线(同 seed 同参数 ⇒ sha256 不变) |
| 8 | ⚠️ **既有缺陷(非本轮引入)**:`test_sh_arg_parsing` 全量 ctest 时**超时** | 根因 = **递归**(该测试跑 `./test.sh --mode off --build` ⇒ 内部跑全量 ctest ⇒ 含它自己)⇒ 引入于 2026-10-24(commit `6a990316`)⇒ 本轮**未触碰** `test.sh` ⇒ **不属本轮 scope** ⇒ 留待独立 change;⚠️ 登记以免污染「全量 ctest 绿」的断言 |
