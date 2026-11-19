# docs/cross_repo/ — 跨仓镜像文档

> **定位**: **跨仓协作的镜像件** —— 同一份协议/契约在多个仓（PTX-EMU / ArchForge）各存一份，本目录存 CppTLM 侧的镜像。
> **权威性**: **镜像不是真源**。每份文件顶部标注**权威 Spec 路径**；发生分歧时以权威 Spec 为准。
> **归档规则**: 对应跨仓协议已闭环（ack 完成 + 双仓落地）⇒ 迁 [`../archive/tracking/`](../archive/tracking/)。

---

## 目录

| 文档 | 主题 | 权威 Spec |
|------|------|-----------|
| [`HSK-9-2027-02-09-cpptlm-sm-rewrite.md`](./HSK-9-2027-02-09-cpptlm-sm-rewrite.md) | `ICOMPUTE_API_VERSION=1` SM 重构版（CppTLM 端镜像） | [`../superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`](../superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md) |
| [`HSK-10-cpptlm-tlp-wire-datapath.md`](./HSK-10-cpptlm-tlp-wire-datapath.md) | TLP 线数据通路径 | 见文件顶部声明 |
| [`HSK-11-cpptlm-abi-slimming.md`](./HSK-11-cpptlm-abi-slimming.md) | ABI 精简（第一轮） | 见文件顶部声明 |
| [`HSK-12-cpptlm-abi-secondary-slimming.md`](./HSK-12-cpptlm-abi-secondary-slimming.md) | ABI 精简（第二轮） | 见文件顶部声明 |

---

## 维护规则

1. **镜像不独立演进** —— 权威 Spec 变更时**先改权威源**，再同步镜像。
2. **每份必须标注权威路径** —— 否则读者无法判断哪份说了算。
3. **不要在镜像里新增决策** —— 决策归 `docs/adr/`。

---

**维护**: builder（同步）· architect（权威源判定）
