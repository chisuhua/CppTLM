# docs/validation/ — 验证报告

> **定位**: 验证与实验的**产出报告**层。每份 = 一次实验 / 一次 oracle 评审的结论与证据。
> **与真源的关系**: 验证报告是**证据**，不是**判据定义** —— 判据定义在 `docs/adr/` 与 `docs/architecture/`；报告引用判据，不重述判据。
> **归档规则**: 结论已被后续报告取代或对应工作流已收口 ⇒ 迁 [`../archive/validation/`](../archive/validation/)。

---

## 目录

### CEP 实验报告（H1-H8 · H7 对拍 · E2E 覆盖）

| 文档 | 主题 |
|------|------|
| [`2026-11-04-cep-h2-h3-h6-experiments.md`](./2026-11-04-cep-h2-h3-h6-experiments.md) | H2 / H3 / H6 实验结论 |
| [`2026-11-04-cep-protocol-e2e-coverage.md`](./2026-11-04-cep-protocol-e2e-coverage.md) | 协议 E2E 覆盖度 |
| [`2026-11-05-cep-b4-evidence-device.md`](./2026-11-05-cep-b4-evidence-device.md) | B4 证据装置 |
| [`2026-11-05-cep-h1-h5-conclusion-registration.md`](./2026-11-05-cep-h1-h5-conclusion-registration.md) | H1 / H5 结论登记 |
| [`2026-11-05-cep-h4-completion-domain-experiment.md`](./2026-11-05-cep-h4-completion-domain-experiment.md) | H4 完成域实验 |
| [`2026-11-05-cep-h8-ab-toolchain.md`](./2026-11-05-cep-h8-ab-toolchain.md) | H8 A/B 工具链判定 |

### H7 对拍基准（gem5 Ruby 同源比对）

| 文档 | 主题 |
|------|------|
| [`2026-10-31-cep-h7-file-same-source.md`](./2026-10-31-cep-h7-file-same-source.md) | 文件同源比对装置 |
| [`2026-10-31-cep-h7-parity-first-round.md`](./2026-10-31-cep-h7-parity-first-round.md) | 对拍第一轮 |
| [`2026-10-31-cep-h7-parity-main-tier.md`](./2026-10-31-cep-h7-parity-main-tier.md) | 对拍主档 |
| [`2026-10-31-cep-h7-parity-main-tier-a.md`](./2026-10-31-cep-h7-parity-main-tier-a.md) | 对拍主档（变体 A） |
| [`2026-10-31-cep-h7-parity-same-source-trace.md`](./2026-10-31-cep-h7-parity-same-source-trace.md) | 同源 trace |
| [`2026-10-31-cep-h7-scope-qualified-conclusion.md`](./2026-10-31-cep-h7-scope-qualified-conclusion.md) | 带范围限定的结论 |

### 原始产物

[`raw/`](./raw/) —— 实验原始 JSON 快照（`main_W{4096,8192,16384,32768,131072}.json`）。
⚠️ **机器消费**：被 `.rddf/roadmap/objectives/obj-cep-platform.md` 按路径引用 ⇒ **不移动、不改名**。

---

## 相关

- **判据定义**：[`../architecture/cep/parity-baseline.md`](../architecture/cep/parity-baseline.md)（H7 对拍基准规格）· [`../adr/ADR-CEP-06-experiment-protocol.md`](../adr/ADR-CEP-06-experiment-protocol.md)（实验输出协议）
- **实验工具**：[`../../scripts/cep/`](../../scripts/cep/)（含 `parity/` 对拍探针）
- **契约闸**：[`../../scripts/test/cep_experiment_run.sh`](../../scripts/test/cep_experiment_run.sh) · [`../../scripts/test/cep_freeze_check.sh`](../../scripts/test/cep_freeze_check.sh)

---

**维护**: builder（产出）· architect（结论裁决）
