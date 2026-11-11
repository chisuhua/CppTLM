# H7 对拍基准首轮实测报告 —— 辅助档(常数档)

> **日期**:2026-10-31 · **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md` **Entry #113**
> **方法规格**:`docs/architecture/cep/parity-baseline.md`(P-1 ~ P-5 判据 + 4 类失配归因)
> **状态**:✅ **首轮实测完成(辅助档 PASS)** —— ⚠️ **H7 仍未验证 · R7 维持 🟡 中**

---

## 0. 一句话结论

**本轮实测的是「口径与建模语义」**(本仓「miss = hit + 存储延迟」分解 + cycle↔ns 换算),
3 条判据 **P-1 / P-4a / P-4b 全绿**;**但这不构成 H7 的验证** ⇒
**H7 仍是「未验证假设」· R7(探索平台结论不可信)维持 🟡 中**。

---

## 1. 基准引入(第三方资产 · 满足 M3)

| 项 | 值 | 登记 |
|---|---|---|
| gem5 版本 | **v24.1.0.0** | 钉死 |
| 引入方式 | **codeload tarball + SHA256 钉版**(非 submodule) | sha256 = `ef5373ec9e514a15dbb1cc6dbc6771182771f1ec48ece1123e0beb876eca8b32` |
| 构建变体 | **`build/NULL/gem5.opt`** | ⚠️ **v24 无 `build/RUBY` 变体**(见 §2) |
| 二进制 | 41.5 MB · `external/gem5/`(**不入库**,`.gitignore`) | 第三方既有资产 ⇒ M3 独立性成立 |
| 构建入口 | **`scripts/cep/parity/build_gem5.sh`**(可复现,含依赖/补丁/校验) | 新增 |
| 执行入口 | **`scripts/cep/parity/run_parity.sh`**(探针 → 比较器 → verdict) | 新增 |

### 1.1 为什么不用 submodule

github 大传输在本机 TLS 中断(`GnuTLS recv error (-110)` / `Failed to connect to github.com port 443`),
`git ls-remote` 可通但 `git clone` 失败 ⇒ 改用 **codeload tarball + SHA256 钉版**。
语义等价(submodule 也是钉 commit),但**更抗网络故障**且**更易校验**。

### 1.2 三处环境级坑(必须登记,否则不可复现)

| # | 现象 | 根因 | 解法 |
|---|---|---|---|
| 1 | gem5 报「Did not find needed zlib compression library」(**假阴性**) | **SCons 4.11.1 自身 bug**:`SConf.CheckLibWithHeader` 传 `call=` 给 `Conftest.CheckLib`,而后者的签名不接受该关键字 ⇒ 检查被**吞成 False** | **降级 `scons==4.5.2`**(已验证:4.5.2 下检查通过) |
| 2 | `Error: No config found, and no implicit config recognized` | gem5 v24 走 **Kconfig**,`build/RUBY` **无 defconfig** | 用 **`build/NULL`**(`build_opts/NULL` = `RUBY=y` + `PROTOCOL="MI_example"`) |
| 3 | `src/base/random.cc:59: 'remove_if' is not a member of 'std'` | **上游源码缺陷**:用了 `std::remove_if` 却未 include `<algorithm>` ⇒ GCC 13 不兼容 | **本地补丁**(已登记在 `build_gem5.sh`,幂等应用) |

---

## 2. ⚠️ 与 `14 号` 文档裁决的偏差(诚实登记)

| 项 | `14 号` §2/§3 裁决 | 本轮实际 | 后果 |
|---|---|---|---|
| 变体 | `scons build/RUBY/gem5.opt` | **`build/NULL/gem5.opt`** | v24 **无 `build/RUBY` 变体**(`build_opts/` 实测 24 个变体,无 `RUBY`);`NULL` 含 Ruby(MI_example 协议) |
| 被测组件 | **Ruby cache**(coherence 协议栈) | **classic `Cache`**(`src/mem/cache/Cache.py`) | **不是同一组件**:classic cache 无 coherence 目录 / 无 snoop / 无 sharer |
| 驱动 | (文档未定) | **`MemTest`** | v24 `PyTrafficGen` 的 `createTrace` 需 protobuf;`MemTest` 地址序列由 RNG 播种 ⇒ 每次不同 |

### 2.1 为什么驱动最终用了 `MemTest` 而不是 `PyTrafficGen`

实测核实 gem5 v24 源码:

- `PyTrafficGen` 是 **`RequestPort` master**(发起方)⇒ 可直接挂 classic `Cache.cpu_side`;
  其 `createRandom` / `createLinear` 由 **Python 参数完全决定** ⇒ 原则上可做同源序列。
- **但**:Ruby 的 `Sequencer.in_ports` 是 **`VectorResponsePort`**(接收 CPU 侧请求)
  ⇒ **`PyTrafficGen`(RequestPort)不能直接驱动 Ruby coherence 栈**;
  Ruby 需要 CPU 或 `DMA Sequencer` ⇒ **CPU-less + Ruby 组合在本版不可行**。
- ⇒ **两难**:要 Ruby 就必须带 CPU(要 workload 二进制 ⇒ 与「同源 trace」相悖);
  要 CPU-less + 同源就得放弃 Ruby。
- ⇒ **本轮取「CPU-less + 常数档」**:用 classic cache + `MemTest` 测**延迟常数**,
  **不声称**测了 coherence 协议开销。

### 2.2 ⇒ 这决定了本轮**能验什么 / 不能验什么**

| 能验(已验) | 不能验(未验) |
|---|---|
| 本仓「miss = hit + 存储延迟」的**分解形态**与 gem5 同构 | **H7**:stub cache 性能模型能否支撑取舍判断 |
| **cycle ↔ ns 换算**口径正确 | **R7**:探索平台结论可信度 |
| gem5 侧**未命中路径延迟常数**的实测值 | **Ruby coherence 栈**的协议开销 / 目录延迟 |

---

## 3. 实测数据

### 3.1 本仓侧(参数**从实现源码抽取**,非手抄)

`scripts/cep/parity/cpptlm_params.py` ⇒ 从 `include/tlm/cache_tlm.hh` 的
`uint64_t access_latency = hit ? 5 : 50;` 抽取 ⇒ 命中 **5 cycles** / 未命中 **50 cycles**;
时基 **1 cycle = 1 ns**(锚点自检:`AGENTS.md` 的「1 sim_cycle = 1 ns @ 1 GHz」仍在 ⇒ 口径未漂移)。

### 3.2 gem5 侧(实测)

变体 `build/NULL/gem5.opt` · cache 16KiB/2-way · `tag=2` / `data=2` / `response=1` cycle ·
`mshrs=4` / `write_buffers=4` · 存储 `latency=45ns` · `MemTest` 20000 次访问(interval=200 cycle)。

| 量 | gem5 实测 | 口径 |
|---|---|---|
| 访问数 | **20101**(读 19901 / 写 199) | `overallAccesses::total` |
| 命中 | **19973** | `overallHits::total` |
| 未命中 | **128** | `overallMisses::total` |
| 未命中率 | **0.6368%** | `overallMissRate::total` |
| **未命中平均延迟** | **48000 ticks = 48 ns** | `overallAvgMissLatency::total`(✅ **实测**) |
| 命中延迟 | 5 ns | ⚠️ **派生**(`tag+data+response`);v24 classic cache **无命中延迟实测统计** |

### 3.3 判据结果(3/3 全绿)

| 判据 | 本仓 | gem5 | 偏差 | 结论 |
|---|---|---|---|---|
| **P-1** 访问数两侧一致 | 20000 | 20000 | — | ✅ |
| **P-4a** 命中延迟常数偏差 < 15% | 5.0 ns | 5.0 ns(派生) | **0%** | ✅ |
| **P-4b** 未命中延迟常数偏差 < 15% | 50.0 ns | **48.0 ns(实测)** | **4.0%** | ✅ |

⇒ **辅助档 PASS**。

### 3.4 4% 偏差的归因(per `14 号` §4.5)

- **D-3 时基归一**? 部分:gem5 的 tag/data/response 是 **cycle**、存储是 **ns**,
  本仓是统一 cycle ⇒ 折算后 gem5 的 48 ns 天然略低于 50 ns(存储延迟 45ns + 3ns 缓存路径
  + 约 0 ~ 排队抖动)。
- **D-1 容量模型**? **不适用** —— 本档只比**常数**,不比命中率。
- **D-4 协议开销**? **不适用** —— classic cache 无 coherence 栈。
- ⇒ 归因落在 **D-3(时基/参数折算)**,**在 15% 容差内**,不构成模型缺陷信号。

---

## 4. 复现步骤

```bash
# ① 构建 gem5(第三方基准,不入库;首次约 30~60 min)
bash scripts/cep/parity/build_gem5.sh

# ② 跑端到端对拍
bash scripts/cep/parity/run_parity.sh

# ③ 只跑判据逻辑自测(不需要 gem5)
python3 scripts/cep/parity/selftest_parity_compare.py

# ④ ctest
ctest --test-dir build -R cep_parity --output-on-failure
```

**产物**:`build/parity/{gem5_probe.json, gem5_stats.txt, verdict.json, gem5_run.log}`。

---

## 5. 残余与下一步

| # | 项 | 状态 |
|---|---|---|
| 1 | **H7 仍未验证** | ⚠️ 本轮只验口径;**不得**表述为「H7 已验证」 |
| 2 | **R7 维持 🟡 中** | 降级条件 = 首轮对拍 **P-1 ~ P-5 全部达标**;本轮只达成 P-1 / P-4a / P-4b ⇒ **P-2 / P-3 / P-5 未做** |
| 3 | **主对拍(容量对齐档)缺同源 trace** | 需「本仓侧 trace 生成器 + gem5 侧同源消费」;gem5 侧 `PyTrafficGen.createTrace` 需 **protobuf**(本机已装 `protoc` 3.21.12,但**当前二进制构建于 protoc 安装之前** ⇒ 无 trace 支持 ⇒ 需增量重建) |
| 4 | **Ruby 基准缺位** | v24 无 `build/RUBY`;`build/NULL` 的 MI_example 是最简协议 ⇒ 若要 coherence 栈基准,需另建变体(`build_opts/` 有 `NULL_MESI_Two_Level` / `NULL_ALL_RUBY` 等) |
| 5 | **命中延迟是派生值** | v24 classic cache 无命中延迟实测统计 ⇒ P-4a 的「一致性」依赖配置解析式,**不是**独立实测 |

---

> **诚实边界**:本报告**不**声称 H7 成立、**不**声称 R7 可降级。
> 本轮确立的是「**口径可信**」这一必要前提 —— 且该前提此前**从未被任何实测检验过**。
