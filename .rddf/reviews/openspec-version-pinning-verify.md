# 验收报告 · **I-8**（`openspec` 版本 pinning）

- **验收对象**：登记册 §6.4 队列 2 的 **I-8** · 实施路径 §6.9 阶段 ④
- **验收角色**：builder 侧验收 · **日期**：2026-11-21 · **已用轮次**：**1/3**

## 一、完成判据

| 判据（登记册 I-8 行） | 判定 | 证据 |
|---|:--:|---|
| CI 配置锁版本 | ✅ | `.github/workflows/ci.yml` 的安装步骤**从锁文件读**：`OS_VER="$(cat .openspec-cli-version)"` ⇒ `npm install -g "@fission-ai/openspec@${OS_VER}"`；⛔ workflow 内**不再有**硬编码版本号 |
| `openspec --version` 与锁定值一致 | ✅ | `bash scripts/test/openspec_version_check.sh` → `✓ openspec 版本锁定一致:锁值 = CI 安装版本 = 本机 CLI = 1.14.3`（rc=0）|

## 二、改动前的真实缺口（**为什么需要它**）

- 版本号**只**硬编码在 `.github/workflows/ci.yml`（`@1.14.3`）⇒ 仓内**无任何机器面**记录「本地该用哪个版本」
- ⇒ 本地与 CI 漂移**无人发现**（直到 baseline 实测口径不一致才暴露）
- 且 `scripts/test/cep_spec_strict_check.sh` 自记「⚠️ 工具 major 漂移会改变本闸语义 ⇒ 须走独立裁决（残余 R-6）」⇒ 漂移风险**已被登记但无机器保障**

## 三、落地形态

| 落点 | 形态 |
|---|---|
| `.openspec-cli-version`（**新建** · 仓根）| **单一真源**：一行 semver（`1.14.3`）|
| `.github/workflows/ci.yml` | 安装步骤改为**从锁文件读**（⛔ 不再内嵌版本号）|
| `scripts/test/openspec_version_check.sh`（**新建**）| 闸三条判据：① 锁文件存在且是合法 semver ② CI **从锁文件读**且**内无硬编码版本号** ③ 本机 CLI 版本 == 锁值（CLI 缺席 ⇒ `[SKIP]`，属外部条件）|
| `test/CMakeLists.txt` | ctest 注册 `openspec_version_check` |

## 四、反向 fixture（**判据有鉴别力**）

| 变异 | 结果 |
|---|---|
| 锁值改成 `1.99.9` | → `✗ openspec CLI 版本 1.14.3 != 锁值 1.99.9 ⇒ 版本漂移`，rc=**1** ⇒ 还原后 rc=0 |
| CI 改回硬编码 `@fission-ai/openspec@1.14.3` | → `✗ CI 内仍存在硬编码的 openspec 版本号（应只经 ${OS_VER} 读取）`，rc=**1** ⇒ 还原后 rc=0 |

## 五、队列顺序变更（**随本单元发生 · 已显式登记**）

I-6 / I-7 阻塞于 architect 裁决（I-6：schema 13→14 形态 + ratify；I-7：Q-a `view_of` 纯查询 API 形态）
⇒ **一次性**把两者移到队列末尾，I-8 前移 ⇒ 新序：

```
I-0 → I-1 → I-9 → I-5 → I-2 → I-3 → I-4 → I-6a → I-8 → I-10 → I-11 → I-12 → I-6(阻塞) → I-7(阻塞)
```

- 同步面：登记册 §6.4 行序 + `scripts/test/rddf_stage_gate.sh` 的 `QUEUE2`（主数组 + **2 处** self-test 副本）+ §6.9 阶段 ③④ 描述
- 机关复验：三路径 rc=0；`P-2a 下一项 (I-10)`
- ⚠️ 登记册 §6.4 的 I-6 / I-7 行**前置列**已加阻塞原因 + 裁决请求指针（`.rddf/reviews/i6-adjudication-request-2026-11-21.md`）

## 六、遗留清扫

| 检查项 | 结果 |
|---|---|
| 反 fixture 的临时改动是否还原 | ✅ 两处均已还原（闸 rc=0 复验）|
| 新增行中的 TODO / FIXME | **0** |
| 范围纪律 | 零 C++ 改动；零冻结面 diff；零 ADR diff；`openspec/specs/**` 零 diff |
| 既有债 | `format --check` 的 21 个既有 C++ 债不变（本单元零 C++ 改动）|

## 七、已用轮次

**1/3**（无返工轮）。
