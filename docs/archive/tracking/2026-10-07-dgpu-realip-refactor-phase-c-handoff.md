# dGPU-RealIP-Refactor Phase C Session Handoff

**生成时间**: 2026-10-07
**HEAD commit**: `3da5eb5a` (Phase A.2 落地后)
**分支**: `feature/dgpu-realip-refactor`

---

## 1. 项目愿景(未变)

CppTLM dGPU SoC 从 TLM 抽象 god-class(`DGpuBoard` + `PcieMemoryDevice`)重构为**真实物理 IP 模块边界对齐**的 TLM SoC,目标:RTL team 可对照每个 TLM 类直接实现硅片。完整规划见 `docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` (修订后 2300+ 行)。

**核心 IP 边界映射**(Oracle `bg_c60653e0` §4):
- `PcieControllerIP` — DWC i5122 对应(BAR decode + iATU)
- `PcieAxiBridgeIP` — EP 内 composite(非 JSON 顶层,已注销)
- `GpuInternalFabric` — wraps `crossbar_tlm`
- `VramControllerTLM` — REGISTER_CHSTREAM + 顶层 JSON 接 `dgpu_board.vram_storage_`

---

## 2. 本 Session 已完成(全部提交 + tag 化)

| Phase | 内容 | Commit | Tag |
|-------|------|--------|-----|
| (前 sessions) | Phase 1-7 PCIe EP 微架构 | `e9b1b..` + `385c7c35` + `e59004b9` | `pr-b-phase-gate-fix-b2-v2` + `pr-b-phase-gate-fix-b2-b3` |
| (前 session) | B1 组合 refactor | `ad13991d` | `pr-b-phase-gate-fix-b1` |
| (前 session) | P3 Amendment (Oracle+Metis 双独立评审) | `8919afa3` | (no tag — 修订) |
| (本 session) | Phase B 并行: T23A + T22b + T27 | `139534ac` | (no tag — 修订) |
| **(本 session)** | **Phase A.2: P0 双阻塞 + P1 5 项 + P2 3 项漂移修复** | **`3da5eb5a`** | (no tag — 修订) |

**当前 tags**:
- `p0-wide-type-foundation`, `p1-wire-format-canonicalization`
- `p2-pr-a-facades`, `p2-pr-b-scaffolding`, `p2-pr-b-stage-b-c`, `p2-pr-b-stage-d`
- `p3-task22-scaffolding`
- `pr-b-phase-gate-fix-b1`, `pr-b-phase-gate-fix-b2-b3`, `pr-b-phase-gate-fix-b2-v2`

---

## 3. BLOCKER 状态(全收)

| BLOCKER | 状态 | commit/位置 |
|---------|------|------------|
| **B1** PcieControllerIP 继承 EP 而非组合 | ✅ **RESOLVED** | `ad13991d` 组合 refactor + `139534ac` T22b doc + `3da5eb5a` 短名注册 |
| **B2** facade verdict 未验证 | ✅ RESOLVED | `dgpu_board_shell.cc:319-353` (mmio_read) + `:433-467` (mmio_write) |
| **B3** BAR寄存器非规范语义 | ✅ RESOLVED | `pcie_endpoint_ip.cc:106-122` (写 0) + `:138-156` (读 `bar_sizes_[idx]`) |
| iATU 建模 | ⏸️ 后续 (中优先级) | — |
| TLP↔AXI bridge 边界 | ⏸️ 后续 (中优先级) | — |

**PR-B Phase Gate:全 3 个 BLOCKER 收 ✅ → UNBLOCKED + Phase A.2 P0 双阻塞解 → Phase C 可启动**

---

## 4. Oracle 评分卡(Phase A.2 后基线)

| 模块 | P3 freeze (10-06) | B2 v2 (10-07 早) | Phase B 后 (10-07 中) | **Phase A.2 后** | Phase C 后期望 |
|------|------|------|------|------|------|
| PcieControllerIP | 5 | 6 | 7 | **7** | 7.5 |
| PcieAxiBridgeIP | 4 | 5 | 6 | **6** | 6.5 |
| GpuInternalFabric | 3 | 4 | 4.5 | **4.5** | 6 (T25A.1 装配实现后) |
| VramControllerTLM | 6 | 6 | 6.5 | **6.5** | 7.5 (T23C backing 注入后) |
| DGpuBoard | 3 | 5 | 5 | **5** | 5.5 |
| PcieBarRouter | 8 | 5 | 7 | **7** | 7.5 |
| **平均** | **4.8** | **5.2** | **6.0** | **6.0** | **~6.8** |

**结论**: Phase C 完成后期望 ~6.8/10(仅"边界结构对齐",非引脚级 RTL 对照 — 那是下一个 change 的事)

---

## 5. Phase C 9 子任务清单(修订后,依序)

**依赖序**: T23A ✅ → T23B.0 ✅ → T23B → T23C → T25A → T25A.1 → T25B → T26 → T28

| # | 子任务 | 工时 | 状态 | 关键位置 | 验收 |
|---|--------|------|------|----------|------|
| ✅ | T23A 删死引用 | 0.5d | **DONE** | `configs/dgpu_soc_v2_realip_p2.json:17` | json.load PASS + 类文件保留 + 注释保留 |
| ✅ | T23B.0 短名注册 | 0.1d | **DONE** | `include/modules_cluster.hh:84-95` | `[controller_ip][p2]` + `[fabric]` 全绿 (8/34 + 7/13) |
| ⏳ | T23B JSON 嵌套重建 | 0.5d | 待启动 | 重建 `dgpu_soc_v2_realip_p2.json` 为 DGpuSoc 嵌套 | `getInternalInstance("pcie_ctrl"/"fabric"/"vram_ctrl"/"axi_bridge") != nullptr` |
| ⏳ | T23C backing 注入 | 0.5d | 待启动 | `dgpu_board_shell.cc:708-745` `bind_memory_backings()` 加 vram_ctrl 分支 | 注入后 `vram_ctrl->read()` 返 vram_storage_ 数据; 未注入返 -ENODEV |
| ⏳ | T25A topology 装配 | 0.5d | 待启动 | `test/test_realip_equivalence.cc` | 4 模块 getInternalInstance 非空 + attach 完成(独立测试先绿) |
| ⏳ | **T25A.1** JSON 装配实现 | 0.5d | 待启动 | `gpu_internal_fabric.hh` 覆写 `simulate_instantiate` 或实现 `attach_composition()` 读 JSON children | 走 ModuleFactory JSON 路径的装配测试绿 |
| ⏳ | T25B dual-path equivalence smoke | 1d | 待启动 | `test/test_realip_equivalence.cc` | 5-10 真实刺激 byte-identical(legacy flags vs controller_ip_ facade) |
| ⏳ | T26 demo + CMake | 0.5d | 待启动 | `examples/demo_dgpu_soc_realip.cpp` + `examples/CMakeLists.txt` | demo target build + 实例化 4 模块 |
| ⏳ | T28 全量 gate | 1d + 0.5d buffer | 待启动 | 7 条可执行命令(见 §6) | 全 PASS |

**总工时**: 5.2-6.2d (原 2.5d vs Phase C 实际 5.2-6.2d,**3 倍修正**)

---

## 6. T28 gate 命令清单(修订后,7 条)

```bash
./build/bin/cpptlm_tests "[controller_ip][p2]"        # 8/34 (B1 + T23B.0)
./build/bin/cpptlm_tests "[fabric]"                  # 7/13 (T22b + T23B.0)
./build/bin/cpptlm_tests "[realip][p3]"              # T23B 装配 + T23C backing 注入
./build/bin/cpptlm_tests "[equivalence][p3]"         # T25B 5-10 刺激双轨 diff
./build/bin/cpptlm_tests "[dgpu_soc_timing]"         # MUST NOT 1 验证锚点 37 cases (ADR-DGPU-11)
./build/bin/cpptlm_tests "[minimal_dgpu_soc]"        # 12/69 零回归
./build/bin/cpptlm_tests "[pcie-memory]"             # 24/69 零回归
./build/bin/cpptlm_tests "[sdma]"                    # 61/12906 零回归
./test.sh --mode off                                 # OFF 路径全量
./test.sh --mode ptx-emu                             # PTX-EMU 路径(若 submodule 已初始化)
```

注: T28 全量回归已知系统 load 约束 (handoff §9 #2: load 8-12 时 build hang, 需 `-j1` + 避开高峰段),折算 0.5d buffer。

---

## 7. 关键文件位置(精确定位)

### 7.1 已落地(P0 / Phase B / Phase A.2)

| 文件 | 关键行 | 作用 |
|------|--------|------|
| `include/modules_cluster.hh` | 84-95 | **T23B.0 短名注册**: `_reg_pciecontrollerip_short` + `_reg_gpuinternalfabric_short` |
| `include/tlm/pcie/pcie_controller_ip.{hh,cc}` | 20 + 11 + 38 | B1 组合 refactor (`: public SimModule`) |
| `include/tlm/cluster/gpu_internal_fabric.hh` | 32-50 | T22b 修复后 (include `crossbar_tlm.hh`, global `CrossbarTLM*`) |
| `include/tlm/vram_controller_tlm.hh` | 全文 | Phase 4 T4 已实现 (touch NOT allowed per MUST NOT 1) |
| `include/tlm/pcie/pcie_axi_bridge_ip.{hh,cc}` | 全文 | 类保留 (MUST NOT 2), REGISTRATION REMOVED (modules_cluster.hh:69) |
| `configs/dgpu_soc_v2_realip_p2.json` | 5 + 17 | T23A 后 (死引用删 + _note 更新) |
| `src/tlm/gpu/dgpu_board_shell.cc` | 93 + 267 + 308-353 + 433-467 + 708-745 | B2 v2 闸门 + T23C 目标 `bind_memory_backings()` |
| `docs/designs/dgpu-soc/architecture.md` | §2.4 (line 145-180) | 4-IP 边界表 (Phase A.2 hash + pending 修正后) |
| `test/test_pin_contract_pcie_controller_ip.cc` | 全文 | B1 验证 (6/31) |
| `test/test_pin_contract_gpu_internal_fabric.cc` | 全文 | T22b 验证 (6/11) |
| `test/test_pin_contract_short_name_registration.cc` | 全文 | T23B.0 验证 (3/5) |
| `scripts/test/check_pcie_ip_facade.sh` | line 54 | P2 PR-A grep-gate (PcieAxiBridgeIP check) |

### 7.2 Phase C 目标位置(精确定位)

| 文件 | 关键行 | 目标 |
|------|--------|------|
| `configs/dgpu_soc_v2_realip_p2.json` | 全文重写 | T23B: 嵌套 DGpuSoc (`{"modules":[{"type":"DGpuSoc","modules":[4 IP...]}]}`) |
| `src/tlm/gpu/dgpu_board_shell.cc` | 708-745 | T23C: `bind_memory_backings()` 加 vram_ctrl 分支 |
| `test/test_realip_equivalence.cc` | 新建 | T25A: topology 装配测试 (4 模块 getInternalInstance 非空) |
| `include/tlm/cluster/gpu_internal_fabric.hh` | 全文 | T25A.1: 覆写 `simulate_instantiate` 或实现 `attach_composition()` |
| `test/test_realip_equivalence.cc` | 扩展 | T25B: dual-path 5-10 刺激 byte-identical |
| `examples/demo_dgpu_soc_realip.cpp` | 新建 | T26: demo (4 模块实例化证据) |
| `examples/CMakeLists.txt` | 新增 | T26: 显式 `add_executable(demo_dgpu_soc_realip ...)` |

---

## 8. 必读评审 (per plan + ADR + spec)

### 8.1 必读文档

- `docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` **末尾 SU "P3 Amendment" + "P3 Amendment Phase A.2"** 两段(详细 T22/T23/T25 + 修订 1-6 + 9 子任务表)
- `docs/superpowers/plans/2026-10-07-dgpu-realip-refactor-session-handoff.md` §3 BLOCKER 表(B1/B2/B3 全 RESOLVED)
- `docs/superpowers/specs/2026-10-06-dgpu-realip-refactor-design.md`:
  - §6.1 PcieControllerIP(BAR decode 语义)
  - §6.2 PcieAxiBridgeIP(EP composite lazy_init)
  - §6.3 GpuInternalFabric(端口契约:4 ports 默认)
  - §6.4 VramControllerTLM(keep REGISTER_CHSTREAM, NOT SimModule)
  - §10.4 Equivalence scaffold (P3, smoke-only) — **T25B 范围声明**
  - §11.4 P3 acceptance (5 条)
- `docs/designs/dgpu-soc/architecture.md`:
  - §2.4 4-IP 边界表
  - **§7.3 JSON 层级约束 (硬约束:顶层 modules[0] 必须是 DGpuSoc)**
  - **§8 Inv-1~5 (vram_storage_ 唯一所有权 + pointer 稳定性)**
- 5 个业务 ADR:
  - `docs/adr/ADR-DGPU-05-vram-storage-ownership.md` (决策 2: vram_storage_ 所有权归 DGpuBoard)
  - `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md` (AxiMemBundle vs PcieTlpBundle 边界)
  - `docs/adr/ADR-DGPU-07-minimal-soc-evolution-seam.md` (D3 seam 插入点)
  - `docs/adr/ADR-DGPU-08-abi-freeze-policy.md` (23 ABI 冻结)
  - `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` (范围)
  - `docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md` (决策 4: VramControllerTLM 与 MemoryTLM 二选一同位置替换 — MUST NOT 1 验证)

### 8.2 必看(参考 JSON 实例化)

- `configs/dgpu_soc_timing_v1.json` — 已落地的 **DGpuSoc 嵌套 + VramControllerTLM 顶层引用**实例,作为 T23B 的参考模板
- `include/tlm/gpu/dgpu_soc.hh` — DGpuSoc SimModule 容器(已存在,注册于 `modules_cluster.hh:55`)

---

## 9. 必知风险(避免重新踩坑)

### 9.1 P0 双阻塞已解(本 Session Phase A.2)

1. ✅ **短名注册缺失已修**(`modules_cluster.hh:84-95`),JSON `"type": "PcieControllerIP"` 和 `"GpuInternalFabric"` 现在可解析。**不要再 touch 这两行注册**(per MUST NOT 3 修订)。
2. ✅ **MUST NOT #3 措辞修订**:允许按 `_reg_pcieendpointip_short` precedent 新增短名注册行;不允许改任何**已存在**注册宏的语义。

### 9.2 仍存的 8 个 AI failure points (Phase C 实施中避开)

3. **JSON 平铺结构 cp 后静默丢弃** — 架构 §7.3 硬约束:**顶层 `modules[0]` 必须是 DGpuSoc**,否则 `simulate_instantiate` 只处理 modules[0] 其余静默丢弃。**T23B 验收必须实际 instantiate 验证 4 模块都出现**(不能只 `json.load` PASS)。
4. **JSON _note 与 description 一致性** — `dgpu_soc_v2_realip_p2.json:3` description 已修(Phase A.2),T23B 重写时不要重引入 stale 描述。
5. **PcieAxiBridgeIP 三态保留**(MUST NOT 2):类文件 `pcie_axi_bridge_ip.{hh,cc}` 保留 + `modules_cluster.hh:69` 注释保留 + JSON 顶层**不实例化**。T23B JSON 中不能加 `"type": "PcieAxiBridgeIP"` 顶层模块条目。
6. **VramControllerTLM 类不动**(MUST NOT 1):仅 JSON 引用 + backing 注入,验证 `[dgpu_soc_timing]` 37 cases 零回归(ADR-DGPU-11 决策 4)。
7. **dormant-gate trap**:`bind_memory_backings()` 若只加 vram_ctrl 分支但 JSON 没有 vram_ctrl 实例,分支永不触发 → silent-pass。T23C 必须有"未注入返 -ENODEV"反向断言。
8. **`attach_composition()` 未实现**(T25A.1 任务):`gpu_internal_fabric.hh:45-46` 注释说 PT-PR-C 会走 attach_composition 读 JSON children,但方法不存在。T22b 测试锁的是直接 C++ attach,**生产路径未覆盖**。T25A.1 必须实现 simulate_instantiate 覆写或 attach_composition 方法。
9. **T25B 范围声明(per plan §修订 2)**:VramControllerTLM **不在 P3 等价性数据路径**。T25B 的 5-10 刺激 (BAR0/BAR1/BAR2 aperture 写读) 全部经 `PcieControllerIP → PcieEndpointIP → PcieMemoryDevice → vram_storage_` (D-AXI v1.4 host BAR2 fast-path)。**fabric→vram_ctrl 链属 D3/D4**,本期不验证 end-to-end 数据路径,仅 T23C 验证对象级 read() 契约。误以为 T25B 覆盖 4-IP 全链将构成 silent-scope drift。
10. **vacuous tag 风险**(修订 3 后):`[realip][p3]` + `[equivalence][p3]` 是 T23B/T25B 新引入的 tag,**目前 0 命中**。T28 gate 命令若先于 T23B/T25B 跑会 vacuous pass。必须确保 T23B/T25B 在 commit 前给测试打上正确 tag。

### 9.3 系统环境风险

11. **build hang 风险** (handoff §9 #2 来自上 session):系统 load 8-12 时反复 terminate。**用 `-j1` + 较长 timeout**(已验证),避开 11:00-15:00 系统高峰。Phase C 6.2d 工时包含 0.5d buffer。
12. **二进制 vs source mtime** (per cpptlm-debug SKILL.md 4 件套):每次 build 后验证 `binary strings | grep -c "Strangler gate"` = 2 + 关键修改 obj 时间戳新于 source。

### 9.4 frozen header / ABI 不可变性

- 4 frozen header 零触碰:`include/tlm/gpu/pcie_endpoint_tlm.h`, `include/tlm/gpu/pcie_display_device.hh`, `include/bundles/pcie_bundles_tlm.hh`, `include/abi/cpptlm_emulator.h`
- 23 ABI 冻结零新增
- Strangler gate 不得回退 `assert()`

---

## 10. 启动命令(Phase C 启动前)

```bash
# 1. 验证当前状态
cd /workspace/project/CppTLM
git log --oneline feature/dgpu-realip-refactor ^main | head -5   # 应见 ad13991d → 8919afa3 → 139534ac → 3da5eb5a
git status --short                                                # 应 clean
git tag -l | grep -E "^(p0|p1|p2|p3|pr-b)"                        # 应见 10 个 tag

# 2. 验证 build + 短名注册 + B1 全绿(负载低时跑)
uptime                                                            # load avg < 8 否则 -j1 + 避开
cd build && timeout 600 make -j1 cpptlm_tests                    # cpptlm_tests 必 cpptlm_core
cd .. && ./build/bin/cpptlm_tests "[controller_ip][p2]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[fabric]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[pcie]" 2>&1 | tail -3                  # 433/36682 全链路
strings build/bin/cpptlm_tests | grep -c "Strangler gate"        # 应 = 2
bash scripts/test/docs_sync_check.sh --strict 2>&1 | tail -3      # 应 0 missing

# 3. 启动 Phase C 子任务(建议依 9 子任务顺序)
# T23B JSON 嵌套重建 → T23C → T25A → T25A.1 → T25B → T26 → T28
# 每个子任务 commit 后跑对应 tag 验证

# 4. 完成时打最终 tag
git tag -a dgpu-realip-refactor-completion -m "..."
```

---

## 11. Phase C DoD(完成定义)

`dgpu-realip-refactor-completion` tag 落地的验收清单(全部满足):

- [ ] T23B + T23C + T25A + T25A.1 + T25B + T26 + T28 七子任务各自验收标准通过(共 7 commits)
- [ ] T28 七条 gate 命令全 PASS(`[controller_ip][p2]` + `[fabric]` + `[realip][p3]` + `[equivalence][p3]` + `[dgpu_soc_timing]` + `[minimal_dgpu_soc]` + `[pcie-memory]` + `[sdma]`)
- [ ] 三条铁律:`4 frozen header` 零 diff; `23 ABI` 零新增; `strings build/bin/cpptlm_tests | grep -c "Strangler gate"` = 2
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS
- [ ] Oracle 新评分卡平均 ≥ 6.5 且无模块 < 5(GpuInternalFabric 必须 ≥ 5.5 凭 T25A.1 装配实现证据)
- [ ] plan `2026-10-06-dgpu-realip-refactor.md` 追加最终 Status Update 段(P3 完成)
- [ ] 后续 pin-contract 工作(legacy flag 删除 + DGpuBoard 瘦身 + 端口契约代码化)已作为**独立 OpenSpec change** 提案(不阻塞本 change archive)

---

## 12. 后续 (Phase C 完成后)

- 最终 tag `dgpu-realip-refactor-completion` + PR handoff
- 本 handoff doc 完成使命后 `git mv plans/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md plans/archive/`
- 启动下一个 OpenSpec change:`dgpu-realip-pin-contract`(legacy flag 删除 + DGpuBoard 瘦身 + 端口契约代码化,预估 8-12d)

---

**维护**: 本 handoff 由 2026-10-07 session 自动生成(基于 Oracle bg_1deb23a0 + Metis bg_06a27d8b pre-Phase-C 评审)。下一轮 Session 入口必读;完成后 archive 至 `docs/superpowers/plans/archive/`。