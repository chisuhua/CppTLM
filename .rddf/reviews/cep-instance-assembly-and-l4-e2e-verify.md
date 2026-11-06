# 验收报告 · `cpptlm-cep-ualink-instance-assembly`(P2)+ `cpptlm-cep-moesi-gpu-l4-e2e`(P3)

- **验收对象**:两个 change(P2 装配闭环 + P3 L4 E2E)
- **验收角色**:verifier(2026-11-06,用户口令「按建议顺序实施」链内)
- **判据来源**:两个 change 的 `proposal.md` **AC-1 ~ AC-6** + delta spec 的 IAG-01/02、MGE-01~04 场景
- **分流判定**:`.rddf/state/iteration.json` **不存在** ⇒ P3 archive gate 未触发 ⇒ ⛔ 不走 `rdd-verifier` skill,按本地 1~5 步验收
- **结论**:**PASS**(0 失败项 · 2 条 SHOULD_FIX 遗留 · 已用 **1** 轮)

---

## 一、AC 逐条判定

### P2 `cpptlm-cep-ualink-instance-assembly`

| AC | 判定 | 证据 |
|---|---|---|
| AC-1 `ualink_4` plane 绑定非空且已登记 | ✅ | `collective_planes.json` 的 `plane_binding["ualink_4"] == "FabricCollectivePlane"`;`find_collective_plane("FabricCollectivePlane") != nullptr`(注册表 3 条) |
| AC-2 拓扑挂载(bridge + 模块在拓扑 + id 唯一) | ✅ | `configs/cep_ualink_pe_minimal.json`:`cep.bridges[0].protocol_instance == "ualink_4"`、`modules == [ualink0, ualink1]`、`bridge_id = 3`;装配闸正控 PASS |
| AC-3 装配闸正控 + 负控按 tag 变红 | ✅ | `[cep][assembly]` = **3 cases / 48 assertions** 全 PASS;负控覆盖 `[IAG-01]`~`[IAG-07]` + ④′(未 required 且 null ⇒ 合法) + ④″(键缺失 ⇒ 红) |
| AC-4 V6-c M-1 夹具修正 + **新增反向断言** | ✅ | `test_cep_nvlink_sharp.cc` NVA-08 与 `scripts/test/cep_nvlink_sharp_probe.cc` **同型**修正;反向断言「M-1 + 真实 fabric 载体 ⇒ FAIL」在两处均生效;反向验证 **10 条**全绿 |
| AC-5 0 回归 + 闸族 + 冻结面 | ✅ | `ctest --test-dir build -j4` ⇒ **156/156**;`docs_sync` / `doc_ref` / `decl_alignment` / `freeze(41 零 diff)` 全绿 |
| AC-6 文档同步 + 台账 + 归档 | ⏳ **待归档后复验**(见 §四) | Entry #258 已落(授权范围 = `07 号` §5.1.1 UALink 行 + `ADR-CEP-12` SU + `ADR-CEP-04` SU) |

### P3 `cpptlm-cep-moesi-gpu-l4-e2e`

| AC | 判定 | 证据 |
|---|---|---|
| AC-1 引擎已注册 | ✅ | `include/chstream_register.hh` 含 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED("MoesiGpuProtocolEngine", …)`(⚠️ 位于 `REGISTER_ALL` **宏体内** ⇒ 带续行符);配置声明该类型 |
| AC-2 10 流逐条记账 | ✅ | `[cep][moesi][engine]` MGE-02:`flow_events[0..9]` **各为 1**(5 条经 `dispatch_cohif` + 5 条**直调** `on_*`,因 `kCohIfFlowCount == 5`);`unsupported_flow_rejected == 0` |
| AC-3 表驱动迁移 + 补偿拒绝路径 | ✅ | MGE-03:迁移真值**取自载体表**(`MoesiGpuAdapter::transition`)⇒ 表漂移即红;MGE-04:`O + Wr` 无 tracker ⇒ 返回 `O` + 地址**未被登记**(`state_of == I` · `line_count == 0`)+ `compensation_skipped == 1` + `compensation_queries == 0` + `adapter_transitions` 不增 |
| AC-4 装配闸 + channel 闸 | ✅ | MGE-05:装配闸 PASS(`moesi_gpu` 的 plane 绑定为 null = **不启用 collective** = 合法 ⇒ 不判红)+ `run_channel_gate` 对 `moesi_gpu` 有逐 VC verdict |
| AC-5 0 回归 + 闸族 | ✅ | 同上:ctest **156/156** + 4 闸全绿 |
| AC-6 coverage §3-A 闭合 + 台账 + 归档 | ⏳ **待归档后复验**(见 §四) | `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` 表格行 + §3-A + 结论段已改;Entry #259 已落 |

## 二、判据的**鉴别力**抽查(避免「恒真通过」)

| 判据 | 鉴别力来源 | 抽查结果 |
|---|---|---|
| 装配闸 `[IAG-01]`~`[IAG-07]` | 7 条负控 + 2 条边界负控 | ✅ 逐条按 tag 变红 |
| `CellKind::Free` 校验器(SRK-05) | 合成 1×1 矩阵:合法通过 / 掩码·ref·basis 三违规必红 | ✅(P1 轮次) |
| MGE-04 补偿拒绝 | 无 tracker ⇒ 迁移被拒 | ✅ 断言 `line_count == 0` + `adapter_transitions` 不增 |
| V6-c 反向断言 | M-1 + 真实 fabric 载体 ⇒ FAIL | ✅ 测试与探针**两处**生效 |

## 三、遗留(SHOULD_FIX,不阻塞)

1. **UALink 专用 coherence 引擎缺失** —— `ualink_4` 的 `request`/`completion` 现由 **Null** 实现承载 ⇒ 「装配」已闭合、**「流语义」未闭合** ⇒ 后续 change(已登记 Entry #258 §④/#259 §④);
2. **`nvlink_4` 有 plane 绑定但无拓扑挂载** —— 不在装配闸职责内(闸只对「配置中挂载了的实例」提要求)⇒ 独立遗留,未修;
3. ⚠️ **本轮 reviewer 不可用**:`subagent` 额度用尽(oracle/critic/explore 均无法调用)⇒ 本次验收**未使用外部审查者**,改由 verifier 角色**本地 1~5 步**逐条取证(每条 AC 均有工具证据)。⚠️ 因此 **P2/P3 的架构方向未获独立交叉审查** ⇒ 建议下一轮(额度恢复后)对「装配闸的三层证据形状」与「MOESI-GPU 只记账边界」补一次 critic 复核。

## 四、失败项与路由

**无失败项** ⇒ ⛔ 无需回 builder / planner。

## 五、已用轮次

第 **1** 轮(2026-11-06)⇒ **PASS**。

## 六、归档后复验(2026-11-06 归档后补验 —— 全部 ✅)

- [x] `openspec/specs/cep-instance-assembly/spec.md` 含 `cep-instance-assembly` requirement + IAG-01/02(归档合并 **+1 requirement**)
- [x] `openspec/specs/cep-moesi-gpu-l4-e2e/spec.md` 含 `cep-moesi-gpu-l4-e2e` requirement + MGE-01~04(归档合并 **+1 requirement**)
- [x] KPI 回填 **2/3 → 0/3**(5 处活文档:`obj §5.2` · `phase-10` · `umbrella` · `nvlink-sharp`(2 处)· `roadmap.md`);`openspec list --json` = **0 active**
- [x] `cep_doc_ref_check` 的 VIRTUAL_PATHS 已登记两个归档前路径
- [x] 两个归档件强制入库(与 114 个已跟踪归档件先例一致)

⇒ **AC-6(P2 / P3)PASS** ⇒ 两个 change 验收 **PASS**(0 失败项 · 2 条 SHOULD_FIX · 1 轮)。归档回执见台账 **Entry #260**。
