# cpptlm-dgpu-pcie-ip-microarch: 顶层 Umbrella 任务

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md) · [`decisions.md`](./decisions.md) · [`roadmap.md`](./roadmap.md) · [`specs/`](./specs/)
> **状态**: 📋 Tasks — 2026-09-01
> **原则**: 每个 phase 子 change 走 TDD 5 步结构(Write failing test → Verify fail → Implement → Verify pass → Commit)

---

## 顶层 Umbrella 任务(本 change 本身)

| ID | 任务 | 状态 | Owner | 验证 |
|---|---|---|---|---|
| **UM-T1** | 撰写 proposal.md(Why / What / Acceptance Gate) | ✅ | Sisyphus | 文件存在 + 内容完整 |
| **UM-T2** | 撰写 design.md(微架构 + 4 张 ASCII 数据流图) | ✅ | Sisyphus | 文件存在 + 4 张图 |
| **UM-T3** | 撰写 decisions.md(Q1-Q17 决策点,9 原 + 8 Oracle 新增) | ✅ | Sisyphus | 17 项决策完整 |
| **UM-T4** | 撰写 roadmap.md(7 阶段详细路线图) | ✅ | Sisyphus | 7 阶段 + 7 个子 change 描述 |
| **UM-T5** | 撰写 specs/pcie-ip-microarch/spec.md(9 个 ADDED Requirements) | ✅ | Sisyphus | 9 个 Requirement |
| **UM-T6** | 撰写 tasks.md(本文件) | ✅ | Sisyphus | TDD 5 步结构 |
| **UM-T7** | 撰写 README.md(导航 + Phase 1 启动指南) | ⏳ | Sisyphus | 文件存在 |
| **UM-T8** | OpenSpec 评审 + umbrella archive | ⏳ | Team | `openspec validate` PASS |
| **UM-T9** | 立即启动 Phase 1 子 change 创建 | ⏳ | Sisyphus | Phase 1 folder 创建 |

---

## Phase 1 子 Change 详细任务(`cpptlm-dgpu-pcie-link-layer-and-fc`)

**OpenSpec Change folder**: `openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/`
**工期**: 4 周(W1-W4)
**优先级**: 🔴 P0

### Phase 1 文件清单

```
openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/
├── proposal.md                          # Phase 1 提案
├── design.md                            # Phase 1 设计
├── specs/
│   └── link-layer-and-fc/
│       └── spec.md                      # Phase 1 spec (LINK_LAYER + FLOW_CONTROL_TOKEN_BUCKET)
└── tasks.md                             # Phase 1 详细 TDD 5 步
```

### T-P1-1: 初始化 Phase 1 Change folder

| 子任务 | 详情 |
|---|---|
| ⏳ | 创建 `openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/` |
| ⏳ | 拷贝模板:`proposal.md` / `design.md` / `tasks.md` / `specs/link-layer-and-fc/spec.md` |
| ⏳ | 提交:`chore(pcie-ll-fc): initialize phase 1 OpenSpec change folder` |

### T-P1-2: 创建 `PcieDllpBundle`(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_dllp_bundle.cc`(测试 `PcieDllpBundle` 字段序列化 + type 判定) |
| ⏳ 2 | 跑 `ctest -R test_pcie_dllp_bundle` → **FAIL**(头文件不存在) |
| ⏳ 3 | 创建 `include/bundles/pcie_dllp_bundles_tlm.hh`:`PcieDllpBundle` 结构(per [design.md §6.2](../design.md)) |
| ⏳ 4 | 跑 `ctest -R test_pcie_dllp_bundle` → **PASS** |
| ⏳ 5 | 提交:`feat(pcie-dllp): add PcieDllpBundle with type+vc+credit fields` |

**TDD 5 步结构关键点**:
- 测试必须**先写**,验证**确实失败**(`expected FAIL but build FAILED` 不算)
- 实现必须**最小化**(只满足测试)
- Commit message 用 `feat(pcie-...):` 前缀(项目约定)

### T-P1-3: 创建 `FcTokenBucket` 类(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_link_layer_fc_token_bucket.cc`(测试 consume / update / over-consume / deadlock 防护) |
| ⏳ 2 | 跑 `ctest -R test_pcie_link_layer_fc_token_bucket` → **FAIL**(头文件不存在) |
| ⏳ 3 | 创建 `include/tlm/pcie/pcie_flow_control_token_bucket.hh` + `.cc`(per [decisions.md §Q2](../decisions.md)) |
| ⏳ 4 | 跑测试 → **PASS** |
| ⏳ 5 | 提交:`feat(pcie-fc): FcTokenBucket class with P/NP/Cpl buckets` |

**测试场景覆盖**(Oracle 修订 #2):
- ✅ 基础 consume: token 充足时返回 true,扣减 token
- ✅ 基础 update: UpdateFC DLLP 后 token 增加(唯一补充路径)
- ✅ 过载 consume: token 不足时返回 false,不扣减
- ❌ **删除**: Refill rate 自动补充(违背 PCIe FC 语义,Oracle Top-2)
- ✅ Capacity 上限: token 不能超过 capacity
- ✅ Deadlock 防护: credit 耗尽 → 发送阻塞 → UpdateFC 到达 → 恢复(非自动 refill)
- ✅ Per-VF 桶隔离: VF0 耗尽不影响 VF1
- ✅ 单调性: token 只增不减(单调非减,per PCIe spec)

### T-P1-4: 创建 `PcieLinkLayer` 模块(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_link_layer_dllp.cc`(测试 DLLP gen/parse + dispatch) |
| ⏳ 2 | 跑测试 → **FAIL** |
| ⏳ 3 | 创建 `include/tlm/pcie/pcie_link_layer_tlm.hh` + `src/tlm/pcie/pcie_link_layer_tlm.cc`(per [design.md §4](../design.md)) |
| ⏳ 4 | 实现 Tx Path(Rx TLP → DLLP ACK 生成 → 128b/130b 透明编码预留) |
| ⏳ 5 | 实现 Rx Path(从 PIPE 4-signal 占位读取 DLLP → parse → 分发) |
| ⏳ 6 | 实现 DLLP dispatch: ACK/NAK/InitFC/UpdateFC/NOP 路由 |
| ⏳ 7 | 跑测试 → **PASS** |
| ⏳ 8 | 提交:`feat(pcie-ll): PcieLinkLayer with DLLP gen/parse/dispatch` |

**测试场景覆盖**:
- ✅ ACK DLLP 生成(per 12-bit Seq)
- ✅ NAK DLLP 生成(触发 retry)
- ✅ InitFC1/InitFC2 解析
- ✅ UpdateFC 解析(更新 FcTokenBucket)
- ✅ NOP DLLP(链路维护)
- ✅ DLLP type 校验(未知 type 丢弃)

### T-P1-5: 链路错误注入接口(Q15 新增,Phase 1 必需)

**理由**:P1-G4 "ACK/NAK 重传场景"需要模拟丢包;Oracle Q15 揭示无注入接口则 TDD 无法写测试。Link Layer 必须暴露 `link_error_injector_t` API。

**范围**(per [decisions.md Q15](./decisions.md)):
- `inject_nak(seq_num)` — 模拟接收 NAK DLLP
- `inject_dllp_loss()` — 模拟 DLLP 丢包
- `inject_tlp_loss(seq_num)` — 模拟 TLP 丢包

**TDD 5 步**:

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_link_layer_error_injector.cc`(测试 3 个 inject 方法 + 默认 disable) |
| ⏳ 2 | 跑 `ctest -R test_pcie_link_layer_error_injector` → **FAIL** |
| ⏳ 3 | 在 `PcieLinkLayer` 内加 `link_error_injector_t` 结构 + 3 个 inject 方法 |
| ⏳ 4 | 默认 disable(无副作用),通过 JSON `params.link_error_injection.enabled=true` 启用 |
| ⏳ 5 | 跑测试 → **PASS** |
| ⏳ 6 | 提交:`feat(pcie-ll): link_error_injector_t API for ACK/NAK retry TDD` |

**测试场景覆盖**:
- ✅ 默认 disabled:LL 行为不变,无副作用
- ✅ `inject_nak(seq=N)`:收到 seq=N 时模拟 NAK,触发 retry
- ✅ `inject_dllp_loss()`:DLLP 静默丢失,触发超时重传
- ✅ `inject_tlp_loss(seq=N)`:TLP seq=N 静默丢失,触发重传
- ✅ 启用 + 注入后链路仍正常恢复(test 注入可关)

---

### T-P1-6: 下行(host→EP)Rx 链路层(Q17 新增,Phase 1 必需)

**理由**:P1 实现**双向**链路层;EP 收方向做 DLLP/TLP 分流 + 生成 ACK DLLP 发回 host + 收方向 retry buffer。原 §4 数据流图只画 slave_in→Tx,实现 half-duplex 风险(Oracle Q17)。

**TDD 5 步**:

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_link_layer_downstream_rx.cc`(测试下行 Rx: DLLP/TLP 分流 + Rx ACK 生成 + 收 retry buffer) |
| ⏳ 2 | 跑 `ctest -R test_pcie_link_layer_downstream_rx` → **FAIL** |
| ⏳ 3 | 在 `PcieLinkLayer` 内加 Rx Path(独立于 Tx Path):128b/130b 解码 → DLLP/TLP 判别 → DLLP 送 ACK/NAK 处理,TLP 送 §3 事务层 |
| ⏳ 4 | 实现 Rx ACK 生成(EP 收 TLP 后生成 ACK DLLP,seq=last_received;累积确认) |
| ⏳ 5 | 实现 Rx retry buffer(per-vc/per-vf,接收重传场景) |
| ⏳ 6 | 跑测试 → **PASS** |
| ⏳ 7 | 提交:`feat(pcie-ll): bidirectional link layer with downstream Rx path` |

**测试场景覆盖**:
- ✅ 下行 TLP:host→EP TLP 解析后送 §3(伪 §3 mock)
- ✅ 下行 DLLP:InitFC1/InitFC2/UpdateFC/NOP 解析 + ACK DLLP 生成
- ✅ Rx ACK:累积确认语义(seq=Ack_ACK_seq)
- ✅ Rx retry buffer:Tx retry → 等待 ACK → 清到 seq=N
- ✅ 双向同时:Tx 和 Rx 路径不阻塞

---

### T-P1-8: ACK/NAK 重传逻辑(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_link_layer_ack_nak.cc`(测试重传场景) |
| ⏳ 2 | 跑测试 → **FAIL** |
| ⏳ 3 | 在 `PcieLinkLayer` 内实现 Retry Buffer + Sequence Number tracking |
| ⏳ 4 | 实现 ACK → 清 Retry Buffer(累积确认,清到 ack seq);NAK → 重发 last NACK 之后所有 TLP |
| ⏳ 5 | 跑测试 → **PASS** |
| ⏳ 6 | 提交:`feat(pcie-ll): ACK/NAK retry buffer with seq number tracking` |

**测试场景覆盖**:
- ✅ ACK 接收 → 清 retry buffer
- ✅ NAK 接收(seq=X)→ 重发所有 seq >= X 的 TLP
- ✅ 重发后 ACK → retry buffer 清空
- ✅ 重复 ACK 幂等
- ✅ 12-bit seq wrap(4095 → 0)

### T-P1-6: 集成到现有 `PcieEndpointTLM`(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_endpoint_with_ll.cc`(测试集成后行为) |
| ⏳ 2 | 跑测试 → **FAIL** |
| ⏳ 3 | 在 `src/tlm/gpu/pcie_endpoint_tlm.cc` 集成 `PcieLinkLayer`(composition) |
| ⏳ 4 | 修改 `PcieEndpointTLM` 增加内部 LL 调用路径 |
| ⏳ 5 | 跑测试 → **PASS** |
| ⏳ 6 | 跑全量 `ctest -R pcie` 无回归 |
| ⏳ 7 | 提交:`feat(pcie-endpoint): integrate PcieLinkLayer into PcieEndpointTLM` |

### T-P1-9: 23 ABI 兼容性边界锁定

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 在 `include/tlm/pcie/pcie_endpoint_ip.hh` 顶部加 `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏(无 `#error`,per Oracle Top-1;项目约定 .hh 后缀,AGENTS.md) |
| ⏳ 2 | 给 `PcieEndpointIP` 新增 public 方法加 `noexcept` 或明确异常规范(不修改旧 `PcieEndpointTLM` 现有方法) |
| ⏳ 3 | 给 `PcieEndpointIP` 查询方法加 `[[nodiscard]]` |
| ⏳ 4 | ⚠️ **删除**: 给旧 `PcieEndpointTLM` 加 `[[deprecated]]`(Oracle Top-9: PcieEndpointIP 尚不存在,deprecated 会铺满编译警告,推迟到 Phase 4 新类落地后) |
| ⏳ 5 | 提交:`chore(pcie-abi): lock ABI v2 with version macro + noexcept + [[nodiscard]]` |

### T-P1-10: Phase 1 Acceptance Gate 验证(Oracle C7 修订: 7 个测试文件)

| 子任务 | 验证方法 |
|---|---|
| ⏳ **P1-G1** | `cmake --build build` 通过 + **7 个新测试 PASS**(Oracle C7 修订: 4 → 7,因 Q15 + Q17 新增 2 个) |
| ⏳ **P1-G2** | `ctest -R test_pcie_link_layer_dllp` PASS |
| ⏳ **P1-G3** | `ctest -R test_pcie_link_layer_fc_token_bucket` PASS(无 refill 测试,per Q2) |
| ⏳ **P1-G4** | `ctest -R test_pcie_link_layer_ack_nak` PASS |
| ⏳ **P1-G4b** | `ctest -R test_pcie_link_layer_error_injector` PASS(Q15 新增) |
| ⏳ **P1-G4c** | `ctest -R test_pcie_link_layer_downstream_rx` PASS(Q17 新增) |
| ⏳ **P1-G5** | `ctest -R test_pcie_endpoint_with_ll` PASS |
| ⏳ **P1-G6** | 23 ABI 静态检查 PASS(不碰 `cpptlm_emulator.h`,新头加宏,旧类不动) |
| ⏳ **P1-G7** | 全量 `ctest -j$(nproc)` 无回归 |
| ⏳ **P1-G8** | `openspec validate 2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc` PASS |

**Phase 1 测试文件清单**(7 个,新增):

| 测试文件 | 验证 |
|---|---|
| `test_pcie_dllp_bundle` | PcieDllpBundle 字段序列化(6 种 kind,3 组 credit) |
| `test_pcie_link_layer_fc_token_bucket` | FC Token Bucket(consume/update/over-consume/单调性/per-VF,无 refill) |
| `test_pcie_link_layer_dllp` | Link Layer DLLP gen/parse/dispatch |
| `test_pcie_link_layer_error_injector` | Q15: 3 个 inject 方法 |
| `test_pcie_link_layer_downstream_rx` | Q17: 下行 Rx DLLP/TLP 分流 + Rx ACK |
| `test_pcie_link_layer_ack_nak` | ACK/NAK 重传(累积确认) |
| `test_pcie_endpoint_with_ll` | 集成测试(无回归) |

### T-P1-11: Phase 1 Archive

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | `openspec archive 2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc` |
| ⏳ 2 | 更新 [`roadmap.md`](../roadmap.md) Phase 1 标记为 ✅ |
| ⏳ 3 | 启动 Phase 2 子 change 创建 |

---

## Phase 2-7 子任务(摘要)

完整任务清单在每个子 change 启动时细化。这里只列概要:

| Phase | 关键 TDD 步骤 | 验证 |
|---|---|---|
| **P2** Encoding Latency | 5 步: 测试 → fail → 实现 → pass → commit | `test_pcie_encoding_latency` PASS |
| **P3** PHY Digital Ctrl | 5 步: LTSSM FSM / Eq / Hot-plug / Bypass Mux 各自 5 步 | 4 个测试 PASS |
| **P4** SR-IOV VF Pool | 5 步: VF routing / per-VF config / per-VF MSI-X / ARI | 4 个测试 PASS |
| **P5** AXI Stream Adapter | 5 步: Axi4Bundle / AXI Adapter basic / 64-byte burst | 3 个测试 PASS |
| **P6** AXI4Mapper | 5 步: 基本 / Outstanding / Out-of-order / 集成 | 4 个测试 PASS |
| **P7** Host Bypass + RC | 5 步: Bypass basic / Software bring-up / E2E demo | 3 个测试 PASS + demo run |

---

## 跨 Phase 持续任务

| ID | 任务 | 频率 | Owner |
|---|---|---|---|
| **CP-T1** | `scripts/test/docs_sync_check.sh --strict` PASS | 每个 PR | Sisyphus |
| **CP-T2** | `./scripts/build/format.sh` 格式化 | 每个 PR | Sisyphus |
| **CP-T3** | 全量 `ctest -j$(nproc)` 无回归 | 每个 PR | Sisyphus |
| **CP-T4** | `openspec-gate` 检查(commit 前) | 每个 commit | Sisyphus |
| **CP-T5** | 更新 `AGENTS.md` / `ONBOARDING.md` | 每 phase 结束 | Sisyphus |
| **CP-T6** | 更新 `roadmap.md` 进度标记 | 每 phase 结束 | Sisyphus |

---

## TDD 5 步纪律(per `superpowers:test-driven-development` skill)

每个模块的实施**必须**严格按以下顺序:

1. **Write failing test** — 先写测试,确保测试**真能测试该功能**
2. **Verify fail** — 跑测试验证**确实失败**(因功能未实现,而非编译错误)
3. **Implement** — 写**最小化**代码让测试通过
4. **Verify pass** — 跑测试 + 全量回归,确认无副作用
5. **Commit** — 提交,message 格式:`feat(scope): description`

**禁止**:
- ❌ 跳过步骤 2(不验证失败就实现,易写出"巧合通过"的测试)
- ❌ 步骤 3 写超出测试范围的代码(过度实现)
- ❌ 步骤 5 用 AI slop 风格 commit message

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — 等待 Phase 1 启动实施
