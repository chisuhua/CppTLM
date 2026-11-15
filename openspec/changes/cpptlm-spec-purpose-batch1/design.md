# design: cpptlm-spec-purpose-batch1

> **角色**: planner 起草（**Purpose 属契约内容 ⇒ planner 写域**）· builder 落地 + 验证
> **上游**: `proposal.md` + improvement · 台账 **Entry #441**
> **metis 预审**: 7 MUST + 8 SHOULD + 5 NIT **全处置**（其中 5 条推翻初稿事实，见 Entry #441 ③）

---

## D-0 · Purpose 写作规则（批 3~5 的可复用模板）

| # | 规则 | 理由 |
|:--:|---|---|
| **R-1** | **答「为什么存在 / 边界在哪」**，⛔ **不复述 Requirement 清单** | Purpose 与 Requirements 段职能重叠即 slop（metis S-1）⇒ 每份 **≤2 句 + ≤160 字符** |
| **R-2** | ⛔ **不写过程叙述**（「被审查确认的…」「用于替代早期提案的越界做法」「v1.1 复用既有」式括注） | 过程属于台账/change，不属于能力描述（metis S-2）⇒ 硬禁词：`被审查确认`/`早期提案`/`v1\.\d`/`本规范覆盖 A、B、C` |
| **R-3** | **只写现在时能力**；spec 内含 `Superseded`/`物理删除` 标记 ⇒ ⛔ **移出批次**，单独做存在性裁定 | superseded 写现状即**立假账**（metis E-1）⇒ 机判：`grep -l 'Superseded\|物理删除'` |
| **R-4** | **不写技术字面量**（`>>`、`& 0x`、`0x[0-9a-f]+`、`A::B`） | 那是 Requirement 的活；也避免 LCS 撞车（metis P0-4） |
| **R-5** | **单 Requirement 的 spec** ⇒ 改写为「**为什么需要这条交付判据**」起句 + 归因 JSON 记 `waiver: single-requirement` | 单条时「判据是什么」必然复述 Requirement（metis E-3） |
| **R-6** | **同源派生 spec** ⇒ 各自锚定「**哪条 Requirement 是它的独占内容**」；无独占内容 ⇒ 登记残余（合并/退役），⛔ 不硬写 | 4 份同源 spec 互相重复（metis E-2）⇒ 本批命中 `pcie-host-test-flr-rebar` ⇒ **R-11** |
| **R-7** | **单物理行**（⛔ 不用硬换行） | 折叠换行后 `purpose_chars` 才可比；与 `cep-*` 43 份惯例一致（metis E-4） |

## D-1 · 16 份 Purpose 草稿（canonical）

> 每条 = `## Purpose` 段的**完整替换文本**（单物理行）。**事实依据** = 来源 change 的 proposal「为什么」段 + 该 spec 的 Requirement 标题。

| # | spec | Purpose（canonical） | 事实依据 |
|:--:|---|---|---|
| 1 | `pcie-axi-datapath-hardening` | 约束 AXI 与 PCIe 之间数据通路的保真：读写方向的判别必须来自请求本身的语义而非字段组合的启发式；BAR 存储必须尊重访问粒度与字节使能；配置空间与 BAR 的边界必须精确，使落在配置区间的请求不会被静默当作 BAR 访问。 | 来源 `2026-09-03-cpptlm-dgpu-pcie-axi-datapath-hardening` proposal（3 项 P1 潜伏缺陷：判别启发式 / 粒度踩踏 / 边界覆盖缺口）· Requirement `axi-rw-discrimination`·`bar-store-4byte-granularity`·`cfg-bar-boundary` |
| 2 | `pcie-cfg-encoding-minor` | 把 AXI 侧地址到 PCIe 配置空间的解码对齐到 PCIe 的编码约定，避免配置解码污染 BAR 语义；BAR、内存映射、端点端口构成与对外 ABI 冻结面均不变。 | 来源 `2026-09-03-cpptlm-dgpu-pcie-cfg-encoding-minor` proposal（只修地址解码边界）· Requirement `pci-config-address-decoding`·`config-e2e-regression` |
| 3 | `pcie-flr` | 定义功能级复位在设备侧的处理契约：复位请求须按 PF 与 VF 两级分别处理，复位之后设备与各功能的状态必须保持一致。本能力复用端点中已存在的复位实现，不引入新的对外入口。 | 来源 `2026-10-09-cpptlm-pcie-host-test-flr-rebar` proposal（FLR 已实现于 `PcieEndpointIP`）· Requirement `flr-tlp-handler`·`flr-state-consistency` |
| 4 | `pcie-host-test-flr-rebar` | 作为宿主侧 PCIe 测试夹具能力的归档载体，本规范汇总了测试基础设施、功能级复位与可调整尺寸地址空间三项契约的宿主侧验证要求；三者的独立判据分别见各自规范。 | 来源同上 · Requirement `pcie-host-test-infrastructure`·`pcie-flr`·`pcie-resizable-bar-runtime`（⚠️ 三者均为兄弟 spec 的同名复制 ⇒ **无独占内容** ⇒ 登记 **R-11**） |
| 5 | `pcie-host-test-infrastructure` | 定义宿主进程内 PCIe 测试夹具的基础设施契约：测试桩的生命周期与构造签名必须统一，报文须在宿主与被测之间往返一致，状态触发须直接驱动既有的实现入口，并规定该基础设施如何扩展到既有测试套件而不重复其覆盖面。 | 来源同上 · Requirement `host-pcie-test-stub-lifecycle`·`tlp-packet-round-trip`·`state-trigger`·`extension-to-existing-3-test-suites` |
| 6 | `pcie-ip-integration` | 为什么需要这条交付判据：各 PCIe IP 部件单独就绪并不等于它们在同一拓扑中真正接通，本判据要求集成结果可被端到端用例验证，而不是仅靠单模块用例各自通过。 | 来源 `2026-09-03-2027-02-09-cpptlm-dgpu-pcie-ip-integration` proposal · Requirement `integration-delivery`（**单条 ⇒ `waiver: single-requirement`**，按 R-5 起句） |
| 7 | `pcie-ip-microarch` | 划定 PCIe 端点微架构各分层的可观察行为边界与彼此衔接：链路层与其管理包及流控、编码层及其延迟模型、物理层数字控制与旁路切换、虚拟功能池，以及 AXI 侧的流适配与映射。各层的内部实现不在本规范约束范围内。 | 来源 `2026-09-03-2026-09-01-cpptlm-dgpu-pcie-ip-microarch`（7 阶段）· Requirement 11 条 |
| 8 | `pcie-resizable-bar-runtime` | 定义可调整尺寸地址空间在运行时的行为契约：尺寸变更须遵循停用、重新编程、再启用的顺序，尺寸以 64 位表达，且变更后配置空间所描述的尺寸与实际路由必须保持一致。 | 来源 `2026-10-09-cpptlm-pcie-host-test-flr-rebar` proposal（disable/reprogram/enable 状态机已实现）· Requirement `resizable-bar-runtime-protocol`·`resizable-bar-routing-consistency`·`resizable-bar-config-sync` |
| 9 | `pcie-slice-prerequisites` | 固化 dGPU 板级 SoC 切片落地前必须成立的前置条件：报文结构的字节级布局由仓内编译期断言守护，地址翻译回调在越界、抛异常与重入下不得导致仿真崩溃，中断状态机的迁移必须可判定，门铃队列须在并发提交下保持有序，设备引擎与后门直访路径互不污染。 | 来源 `2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites` proposal（该需求实际作用是仓内布局守卫）· Requirement 5 条（⚠️ 其 `:14` 引用已删除的 `include/cudart/abi_guards.h` ⇒ 登记 **R-12**） |
| 10 | `pcie-tlp-wire-datapath` | 定义 PCIe 传输层报文的线格式与数据通路契约：编解码的字段布局、完成方与请求方各自的引擎行为、旁路通路上的报文搬运、被测用的模拟 IP，以及各协议 profile 下经 PCIe 路径的路由选择。 | 来源 `2026-09-16-cpptlm-pcie-tlp-wire-datapath` · Requirement 8 条 |
| 11 | `cpptlm-pcie-endpoint-ip-json-config` | 划定 PcieEndpointIP 可被配置真正驱动的参数面：此前写入但未被消费的参数会在装配时消失，使仓库自带示例与实际行为不一致。本规范定义哪些键会被消费、缺省取值为何，以及无法识别的键与越界取值如何以告警形式暴露，并在回退到缺省取值后继续运行。 | 来源 `2027-02-09-cpptlm-pcie-endpoint-ip-json-config` proposal（4 组参数只消费 2 组；⚠️ 实测越界取值是 **warning + 回退**，**非拒绝** ⇒ 已改写）· Requirement `PcieEndpointIP JSON Configuration Surface` |
| 12 | `cpptlm-pcie-ep-foundation` | 锁定宿主侧委托 dGPU 板卡仿真时 PCIe 端点侧的真实语义：配置空间与内存映射的读写、寄存器回调、地址翻译回调与消息中断的初始化与清除，这些入口都必须返回真实数据与真实翻译结果，而不是桩值、空实现或伪装成功。 | 来源 `2026-09-09-cpptlm-pcie-ep-foundation` proposal（7 项逐条对照表）· Requirement 13 条 |
| 13 | `cpptlm-stage-1-1-fixes` | 把 PCIe 端点基础能力中的四个具体缺陷固化为可验证契约：配置空间读写不得返回「未实现」，内存映射读必须真正填充调用方缓冲区，后门读未命中必须返回明确的「不存在」错误，写入路径的同步或异步语义必须在实现与文档之间保持唯一口径。 | 来源 `2026-09-10-2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` proposal（#3/#5/#6/#7）· Requirement 4 条 |
| 14 | `cpptlm-stage-1-2-msix` | 定义消息中断从注册到触发的完整链路契约：初始化、更新与清除三入口所登记的中断回调必须能被真实的触发路径调用到；此前回调虽已接线却全仓无调用方，中断实际从未发生。修复中断链的同时须保持既有 ABI 契约不变。 | 来源 `2026-09-10-cpptlm-stage-1-2-msix` proposal · Requirement `MSI-X trigger_irq_async 真实实现`·`Existing ABI contract preserved` |
| 15 | `cpptlm-stage-1-4-2-1` | 覆盖 dGPU 侧 PCIe 的电源管理与拓扑增强两层能力：电源管理要求设备能在工作态与低功耗态之间切换并对状态变化作出正确响应；拓扑与隔离增强要求对等路由、访问控制与可调整尺寸的地址空间在配置与运行时都可被正确识别与使用。 | 来源 `2026-09-10-cpptlm-stage-1-4-2-1` proposal（阶段 1.4 电源 + 阶段 2.1 P2P/ACS/ReBAR；⚠️ **ACS 是隔离不是能效** ⇒ 定性已订正）· Requirement 4 条 |
| 16 | `cpptlm-stage-1-4-2-1-ue-extensions-unblock` | 打通宿主入口路径到 PCIe 端点实施层之间被冻结在旧实现上的断点：此前的配置访问命中的是只带消息中断能力的旧端点实现，导致电源能力、电源状态拦截、访问控制扩展、可调整尺寸扩展与省电状态接线全部落不到实施层。 | 来源 `2026-09-15-cpptlm-stage-1-4-2-1-ue-extensions-unblock` proposal（ABI 路径实际命中旧端点实现）· Requirement A-1~A-5 |

**⛔ 已移出批 1**：`cpptlm-d1-p1-pipeline-scoreboard` —— spec 自带 `Status: SUPERSEDED` + `include/cudart/` 整个目录不存在 ⇒ 写现状即立假账 ⇒ 登记 **R-10**（存在性裁定）。

## D-2 · 归因载体（`test/spec_purpose_batch1_attribution.json`）

⚠️ **`checks` 四布尔由脚本生成**（人手填 = 自证 ⇒ 判别力为零，metis S-5）⇒ 该文件由 `scripts/test/cep_spec_purpose_batch1_check.py` 依据下述**原始输入**生成。

```json
{
  "schema": "spec-purpose-attribution/v2",
  "batch": "batch1",
  "archive_semantics": "completed-direct-main-spec",
  "items": [
    {
      "spec": "pcie-flr",
      "claim_kind": "current",
      "source_change": "2026-10-09-cpptlm-pcie-host-test-flr-rebar",
      "source_evidence": "openspec/changes/archive/2026-10-09-cpptlm-pcie-host-test-flr-rebar/proposal.md",
      "requirement_titles": ["flr-tlp-handler (v1.1 复用既有)", "flr-abi-entry (v1.1 删除)", "flr-state-consistency (v1.1 修 spec 矛盾)"],
      "purpose_text": "定义功能级复位在设备侧的处理契约：……",
      "waiver": null,
      "result_commit": "<产物 commit sha>"
    }
  ]
}
```

| 字段 | 用途 | 机判 |
|---|---|---|
| `claim_kind` | `current` / `historical` / `superseded` | **互斥校验**：spec 内含 `Superseded`/`物理删除` ⇒ ⛔ 不得为 `current` |
| `source_evidence` | 依据文件路径 | `os.path.isfile` == True **且** 关键字命中 |
| `requirement_titles` | 该 spec 的全部 `### Requirement:` 标题 | 从 spec 自动抽取 |
| `purpose_text` | 实际落地文本 | 从 spec 自动抽取 |
| `waiver` | `single-requirement` / null | Requirement 数 == 1 ⇒ 必须有 waiver **且** Purpose 以「为什么需要这条交付判据」起句 |
| `result_commit` | 落地 commit | 回填 |
| `checks` | **脚本生成**，不手填 | 见 D-3 校验项 |

## D-3 · 校验项（由脚本重算，AC-03③/④/⑤ 的机判形式）

| # | 校验 | 判据 |
|:--:|---|---|
| **C-1** | 非占位 | Purpose 段内 `TBD`/`TODO` 计数 = 0（**限段**：`sed -n '/## Purpose/,/## Requirements/p'`） |
| **C-2** | 长度 | `len(purpose_text) ≥ 50` **且** `≤ 160` **且** **单物理行**（无 `\n`） |
| **C-3** | **不复述 Requirement** | 规范化（去 `**`/`` ` ``/空白/标点）后，Purpose 与该 spec **全部 Requirement 正文**的 **LCS < 12 字符**（CJK 计 1）；`waiver: single-requirement` ⇒ 阈值 **16** |
| **C-4** | 无过程叙述 | ⛔ 含 `被审查确认`/`早期提案`/`v\d\.\d`/`本规范覆盖`/`Phase \d`/`Task \d+`/`Entry #\d+`/`本 change`/日期 ⇒ 红 |
| **C-5** | 无技术字面量 | ⛔ 含 `>>`/`& 0x`/`0x[0-9a-f]+`/`[A-Za-z_]+::[A-Za-z_]+` ⇒ 红 |
| **C-6** | **不声称不存在的能力** | Purpose 中的英文标识符集合 ⊆ Requirement 标题 token ∪ 仓内 `include/` `src/` `configs/` 可 grep 到的符号；否则该条须显式声明 `claim_kind != current` |
| **C-7** | 归因闭合 | `source_evidence` 全部 `isfile` == True |

## D-4 · 归档语义（⚠️ 归档前**硬门 T3.0**，两条互斥出口）

实测（本轮影子根实验）：① delta 的 `## Purpose` **被忽略** ⇒ 工具要求直接编辑 main spec ⇒ **0 份 delta 是唯一路径**；② change 无 delta 时归档器要求 `.openspec.yaml` 设 `skip_specs: true` ⇒ 实测归档**无警告** + main spec **零改动**。

⚠️ **可判别性缺口**（metis A-4）：归档后目录形态「无 `specs/` + `skip_specs: true`」与 ⏸ 暂存归档**结构同形** ⇒ 半年后 grep `skip_specs` 无法区分。

⇒ **T3.0 硬门**（architect 裁决，二选一，⛔ 不许两条都做）：
- **出口 A**：`skip_specs: true` + **proposal 顶部**加「**✅ 已完成归档 · 主 spec 直改（无 delta）**」标记行 + 归因 JSON `archive_semantics: "completed-direct-main-spec"`。
- **出口 B**：**不走**归档器，按 doc-only change 手工 `mv` 进 `archive/`，台账显式记「**未走归档器**，理由：Purpose 无法走 delta（工具明文）」。

⚠️ **失败代价（须先认清）**：两条都不行 ⇒ 本 change 无法归档 ⇒ **KPI 卡在 1/3** ⇒ 后续提案全被 3/3 上限阻塞。

## D-5 · 落地方式与预期 diff 形状

实测 16 份结构**完全统一**：`## Purpose` 在 line 3 · 占位在 line 4 · `## Requirements` 在 line 5。

⇒ **预期 diff 形状（AC-04 写死）**：**16 个文件 · 每文件恰 1 个 hunk · 删 1 行增 1 行 · 不新增空行 · Purpose 单物理行**。
⇒ 替换用正则定位 `## Purpose\n(.*?)\n## Requirements` **整段替换** ⇒ **Requirements 段逐字节不动**。
