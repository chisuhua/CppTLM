# H7 对拍基准 —— 同源 trace 档首轮实测报告

> **日期**:2026-10-31 · **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md` **Entry #114**
> **方法规格**:`docs/architecture/14-coherence-parity-baseline.md`(§4.1 访问流契约 + §4.2 模型对齐表 + §4.4 判据)
> **状态**:✅ **同源 trace 档跑通,5/5 判据全绿** —— ⚠️ **H7 仍未验证 · R7 维持 🟡 中**

---

## 0. 一句话结论

**「两侧消费同一条离线 trace」这条契约首次被真正执行并验证**:
访问数 **1000 = 1000**、读写条数 **793/207 = 793/207 逐条相同** ⇒
两侧确实在消费**同一条**访问流;路径延迟常数偏差 **0% / 4%**(<15%)⇒ PASS。

⚠️ **但这仍不构成 H7 的验证** —— 验的是「同源访问流下的口径一致性」,
**不是**「stub cache 性能模型足以支撑 early-stage 取舍判断」⇒ **R7 不降级**。

---

## 1. 本轮解决的三个硬阻塞

| # | 阻塞 | 根因(实测 gem5 v24 源码) | 解法 |
|---|---|---|---|
| 1 | gem5 二进制**无 protobuf** ⇒ `PyTrafficGen.createTrace` 不可用 | 二进制构建于 `protoc` 安装**之前** | 增量重建 ⇒ `HAVE_PROTOBUF = True`(已验证 `PROTOC` 动作产出 `packet.pb.cc`) |
| 2 | **无同源 trace 生成器** | `14 号` §4.1 早已指定 `scripts/cep/parity_trace_gen.py` 但**从未建** | 新建 ⇒ 产出 **CSV**(本仓侧)+ **gem5 protobuf**(gem5 侧),同一次 LCG 序列 |
| 3 | 本仓侧**无法注入 trace** | `TrafficGenTLM` 的 `trace_` 是**构造函数写死的 4 条**,无 setter | 加 `set_trace()`(该文件**不在 M1 冻结面**) |

---

## 2. gem5 v24 侧的五处「实测才发现」的 API 事实

> 这些都是**照文档/直觉写必踩**的坑,已全部固化为脚本注释。

| # | 现象 | 根因 | 处置 |
|---|---|---|---|
| 1 | `panic: Input file ... is not a valid gem5 proto format` | gem5 的 proto 流**不是裸序列化**:文件头 **4B magic `0x356d6567`(LE)** + 每条消息 **Varint32 长度前缀** | trace 生成器按 gem5 `src/proto/protoio.cc` 的格式写 |
| 2 | `Assertion 'nextElement.isValid()' failed` | `Packet.cmd` 必须取 gem5 **`MemCmd` 枚举值**(`InvalidCmd=0, ReadReq=1, ReadResp=2, ReadRespWithInvalidate=3, WriteReq=4`)⇒ 我最初用「0=读/1=写」⇒ 0 落成 `InvalidCmd` | 写 `ReadReq=1 / WriteReq=4` |
| 3 | `panic: ... saw InvalidateReq` | coherent `Cache` 对**整行写**转成 `MemCmd::InvalidateReq` ⇒ `SimpleMemory` 只接受读/写 ⇒ panic | 换 **`NoncoherentCache`**(官方 `cache_partitioning.py` 同款)+ 访问大小 **8B < cache line 64B** |
| 4 | `fatal: ... spent 1000000000 ticks without making progress` | `BaseTrafficGen` 的 `progress_check` 默认 **1ms = 1e9 ticks**;trace 播完后 generator 空转 ⇒ 先撞 progress_check | `progress_check` 调到「trace 总时长 × 4」+ `m5.setMaxTick()` 贴 trace 末尾 |
| 5 | 一个包都不发(`numPackets == 0`) | ① `PyTrafficGen` 必须 `start([gen])`(只调 `createTrace` 不调 `start` 不发包)② `system.mem_ranges` **必须显式设置**(否则 `isMemAddr` 恒 false ⇒ 包被丢弃)③ `SimpleMemory` 要挂进 `system.memories` | 三处都补齐 |

**另**:`system.clk_domain` 必须在 **`System` 构造时**传入(默认子对象 `dvfs_handler` 的
`sys_clk_domain = Parent.clk_domain` 在 unproxy 阶段就要解析)⇒ 否则报
`Can't resolve proxy 'clk_domain' ... from 'system.dvfs_handler'`。

---

## 3. 同源性证据(本轮最硬的产出)

| 证据 | CppTLM 侧 | gem5 侧 | 一致 |
|---|---|---|---|
| 访问数 | **1000** | **1000** | ✅ 严格相等 |
| 读条数 | **793** | **793** | ✅ 逐条相同 |
| 写条数 | **207** | **207** | ✅ 逐条相同 |
| trace 文件 | `trace.csv`(sha256 `1cf40f1b…`) | `trace.pb`(同一次 LCG 生成) | ✅ 同源 |

⇒ **P-1b**(读写条数逐条一致)是「同源」最直接的证据 —— 比「访问数相等」强得多。

---

## 4. 判据结果

| 判据 | 内容 | 本仓 | gem5 | 结论 |
|---|---|---|---|---|
| **P-1** | 访问数两侧**严格相等** | 1000 | 1000 | ✅ |
| **P-1b** | 读/写条数两侧**逐条一致** | r=793,w=207 | r=793,w=207 | ✅ |
| **P-4a** | 命中延迟常数偏差 < 15% | 5.0 ns | 5.0 ns(派生) | ✅ 0% |
| **P-4b** | 未命中延迟常数偏差 < 15% | 50.0 ns | **48.0 ns(实测)** | ✅ 4.0% |
| **P-2** | 未命中率偏差(±5pp) | 0.8530 | 0.8720 | **观察值 1.90pp** —— ⚠️ **不参与 verdict** |

### 4.1 P-2 为什么只作登记、不参与判定

`14 号` §4.2 已登记:本仓 `CacheTLM` 的命中判定是「该地址**曾被命中/写入**」
(`include/tlm/cache_tlm.hh`:`cache_lines_.count(addr) > 0`)= **无限容量 + 精确地址 map**;
gem5 是 **16KiB/2-way/LRU** 的「容量内命中」⇒ **命中判据结构性不同** ⇒
命中率**不可直接判**。观察到的 1.90pp 偏差**很小**(且本仓侧命中率略**低**),
但**不能据此说「容量模型已对齐」** —— 真要对齐需本仓侧引入「等容量 + 同替换」目录视图(未做)。

### 4.2 4% 延迟偏差的归因(per §4.5)

- **D-3 时基归一**:gem5 的 tag/data/response 是 **cycle**、存储是 **ns**,本仓统一 cycle
  ⇒ 折算后 gem5 天然略低(48ns vs 50ns);
- **D-1 容量模型** / **D-4 协议开销**:**不适用**(classic cache 无 coherence 栈);
- ⇒ 归因落在 **D-3**,在 15% 容差内,**不构成模型缺陷信号**。

---

## 5. 复现步骤

```bash
# ① gem5(第三方基准,不入库;需 protoc + libprotobuf-dev)
bash scripts/cep/parity/build_gem5.sh

# ② 同源 trace 档端到端(生成 trace → gem5 回放 → CppTLM 回放 → 比较)
bash scripts/cep/parity/run_parity_trace.sh

# ③ 常数档端到端
bash scripts/cep/parity/run_parity.sh

# ④ 判据逻辑自测(不需要 gem5)
python3 scripts/cep/parity/selftest_parity_compare.py   # 13/13

# ⑤ ctest
ctest --test-dir build -R cep_parity --output-on-failure   # 4 项(1 项按设计 skip)
```

**新增产物**:`scripts/cep/parity/{parity_trace_gen.py, gem5_trace_probe.py, run_parity_trace.sh}`
· `test/test_cep_parity_cpptlm_side.cc` · `include/tlm/traffic_gen_tlm.hh` 的 `set_trace()`。

---

## 6. 残余与下一步

| # | 项 | 状态 |
|---|---|---|
| 1 | **H7 仍未验证** | ⚠️ 本轮验的是「同源访问流下的口径一致性」,**不是** H7 |
| 2 | **R7 维持 🟡 中** | 降级条件 = P-1 ~ P-5 全达标;本轮达成 **P-1 / P-1b / P-4a / P-4b**;**P-3 / P-5 未做** |
| 3 | **P-3(容量对齐档)** | 需本仓侧引入「等容量 + 同替换」目录视图 ⇒ **新增建模组件** ⇒ 条件分支(命中归因 D-1 才启动) |
| 4 | **P-5** | `14 号` §4.4 未展开 P-5 的具体定义 ⇒ 待补 |
| 5 | **命中延迟是派生值** | v24 classic cache 无命中延迟实测统计 ⇒ P-4a 依赖配置解析式,非独立实测 |
| 6 | **Ruby coherence 栈基准仍缺位** | v24 无 `build/RUBY`;CPU-less 也驱动不了 Ruby(`Sequencer.in_ports` 是 `VectorResponsePort`) |

---

> **诚实边界**:本报告**不**声称 H7 成立、**不**声称 R7 可降级。
> 本轮确立的是「**两侧确实在消费同一条访问流**」—— 这是 `14 号` §4.1 的契约首次被**机器验证**。
