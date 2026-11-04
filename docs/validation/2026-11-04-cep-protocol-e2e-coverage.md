# CEP 逐协议端到端测试覆盖梳理（2026-11-04）

**目的**：回答「14 个 change 归档后，6 个协议族各自的验证到底做到了哪一层」。
**方法**：逐文件实测（`TEST_CASE` 名称 + 装配/驱动/抵达断言计数），不按文件名推测。

---

## 1. 覆盖层次定义（本文使用的口径）

| 层 | 含义 | 可机器判定的标志 |
|---|---|---|
| **L1 契约** | constexpr 判定、表逐格比对、文本扫描、schema 校验 | 无实例、无事务 |
| **L2 声明** | `protocol_instances.json` 的实例声明字段齐备且与 traits 一致 | 读 JSON |
| **L3 装配** | `ModuleFactory::instantiateAll(cfg)` + JSON 配置实例化 | 有 factory / config |
| **L4 事务级 E2E** | `submit → tick → 对端收到`（ChStream wire 真实穿过） | 有 `tick()` + 抵达断言 |
| **L5 实验级** | metric 采集 + 假设结论 | `metrics.json` + 结论文档 |

⚠️ **L1 ≠ L4**。`test_*=0 装配 / 0 抵达` 的协议即使有 500 行测试，也只覆盖 L1。

---

## 2. 覆盖矩阵（实测）

| 协议族 | L1 契约 | L2 声明 | L3 装配 | L4 事务 E2E | L5 实验 | 运行时引擎 |
|---|:---:|:---:|:---:|:---:|:---:|---|
| **CHI RN-F** | ✅ `test_cep_chi_rnf_subset.cc`(1322 行) | ✅ | ✅ CR-10③ | ✅ `submit → tick → 分派` | — | `chi_rnf_protocol_engine.hh`(648) |
| **MESIF** | ✅ `_adapter` / `_state_spec` / `_ratification` | ✅ | ✅ `instantiateAll` | ✅ `submit_local_request → tick → 对端` | — | `mesif_protocol_engine.hh`(667) |
| **CohIf 载体** | ✅ `test_cep_cohif_carrier.cc` | ✅ | ✅ `_json_assembly`(11 处) | ✅ | — | `CohIfNullProtocolEngine`（**Null 语义**） |
| **MOESI-GPU** | ✅ `_adapter`(442) / `_compensation`(324) | ✅ | ⚠️ 仅 per-network 装配期校验 | ❌ | — | `moesi_gpu_protocol_engine.hh`(306) |
| **CXL 2.0 非对称** | ✅ `test_cep_cxl_cache_adapter.cc`(209) + `_state_spec`(451) | ✅ `cxl_2_0` | ❌ | ❌ | — | ❌ **无** |
| **CXL 3.0 对称 DCOH** | ✅ `_adapter`(188) / `_tracker`(355) / `_state_spec`(329) | ✅ | ❌ | ❌ | — | ❌（`DcohSymmetric` tracker 有行为面） |
| **TileLink-C** | ✅ `_adapter`(450) / `_state_spec`(488) | ✅ | ❌ | ❌ | — | ❌（`TreeManager` 有行为面） |
| **NVLink SHARP** | ✅ `test_cep_nvlink_sharp.cc`(479) / `_state_spec`(489) | ✅ `nvlink_4` | ❌ | ❌ | — | ❌（`BroadcastNull` tracker） |
| **UALink INC** | ✅ `test_cep_ualink_inc.cc`(501) / `_state_spec`(361) | ⚠️ **`ualink_4` 未声明** | ❌ | ❌（`plane.send/recv` 为 API 级） | — | ❌（`FabricCollectivePlane` 行为面） |

**补充**：L5 实验只有本轮新增的 H2/H3/H6（`test_cep_h2_h3_experiments.cc` /
`test_cep_h6_vc_isolation.cc`），且其协议范围是「home 侧目录策略 + VC 仲裁」，**不对应**任何单一协议族。

---

## 3. 缺口分类（按性质，不按协议）

### A. 有运行时引擎、但无事务级 E2E
- **MOESI-GPU**：`moesi_gpu_protocol_engine.hh` 存在（306 行），但测试只有 adapter 契约与
  per-network 装配期校验，无 `instantiate → submit → tick → 抵达`。
- 性质：**E2E 测试缺口**（实现已在，缺驱动）。

### B. 有 tracker 行为、无协议引擎、无事务 E2E
- **CXL 3.0 DCOH**（`DcohSymmetric`）、**TileLink-C**（`TreeManager`）、**NVLink**（`BroadcastNull`）。
- 性质：**载体形态缺口**。三者都有真实的目录/跟踪行为，但没有把它们接进一条事务路径。
- 说明：oracle 2026-11-04 的裁决是「不补协议引擎」——理由是规约载体无转移表真源，
  补引擎只会诱发推理填空。⇒ 该缺口**不建议**用「造引擎」补，应记为**已知范围限制**。

### C. 只有 adapter 契约，无运行时载体
- **CXL 2.0 非对称**（`CxlCacheAdapter`）：连 tracker 都无独立行为实现（复用 MESIF 机制）。
- 性质：**实现深度缺口**，与 DoD §6.2 B3「6 个 ProtocolEngine 实施完成」的表述**不符**。

### D. 声明面缺口
- **`ualink_4` 未声明**（用户裁决 2026-11-04，选项 A）：`flow_support` / `perm` /
  `sharer_tracker.impl` 取值不可判定 ⇒ 不声明。
- 后果：UALink 的实例级装配不可达（L3/L4 天然不可做）。

### E. 反向 fixture 覆盖（这层反而是最齐的）
- 6 个协议族**全部**有 `cep_*_reverse_verify.sh` 并注册进 ctest。
- ⇒ 结论：**「判据有没有牙」覆盖良好；「判据有没有真实路径可跑」覆盖不足**。

---

## 4. 结论

1. **契约层（L1/L2）与反向 fixture 是全面覆盖的** —— 14 个 change 的交付在这一层是扎实的。
2. **事务级 E2E（L4）只有 3 条路径**：CHI RN-F、MESIF、CohIf（Null 语义）。
3. **DoD §6.2 B3「6 个 ProtocolEngine」与实现不符**：运行时实际 = 3 个 RN 侧引擎
   （`chi_rnf` / `mesif` / `moesi_gpu`）+ 1 个 Null 外壳；CXL DCOH / TileLink-C / NVLink / UALink
   **只有 adapter 契约**。该口径差异需 DRI 裁决：
   - 选项 ① 把 B3 重新定义为「6 协议族的 **adapter 契约**实施完成」（则已达成）；
   - 选项 ② 承认 B3 未达成，登记为待补项。
4. **不建议**为消灭缺口 B/C 而批量造引擎（oracle 已论证 ⇒ 推理填空风险）。
   若要补 E2E，优先补 **A（MOESI-GPU）** —— 实现已存在，只缺驱动。

---

## 5. 复现本梳理

```bash
# 装配/抵达/驱动 三维计数
for f in test/test_cep_*.cc; do
  printf "%-44s asm=%s arr=%s tick=%s\n" "$(basename $f)" \
    "$(grep -cE 'instantiateAll|ModuleFactory|JSON 装配' $f)" \
    "$(grep -cE '抵达|到达|对端|received' $f)" \
    "$(grep -cE '\.tick\(\)' $f)"
done
```
