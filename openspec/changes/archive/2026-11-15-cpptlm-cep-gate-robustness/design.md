# design: cpptlm-cep-gate-robustness

> **角色**: builder · **上游**: `proposal.md` + improvement v2（metis 7 MUST + 5 SHOULD + 5 NIT 全处置）· 台账 **Entry #436**
> **范围**: 闸族可靠性 + 覆盖义务 · **零实现层改动 · 零 ADR 改动**

## D-1 · 影子路径 = **`mktemp -d` + 所有权标志**（⛔ 不用 `$$`）

```bash
_base="${TMPDIR:-/tmp/opencode}"; mkdir -p "$_base" 2>/dev/null || true
if [ -n "${SHADOW_ROOT:-}" ]; then
    SHADOW="$SHADOW_ROOT"; _own_shadow=0        # 调用方给的目录:⛔ 不删
else
    SHADOW="$(mktemp -d "$_base/<gate>.XXXXXX")"; _own_shadow=1
fi
_cleanup_shadow() { [ "${_own_shadow:-0}" = 1 ] && rm -rf -- "$SHADOW"; }
trap _cleanup_shadow EXIT
echo "[shadow] $SHADOW"
```

| 取舍 | 理由 |
|---|---|
| ⛔ 不用 `$$` | 容器场景各容器共享 hostPath 的 `/tmp`，而 PID namespace 各自从 1 起 ⇒ **PID 会撞** |
| ⛔ 不删调用方目录 | `trap rm -rf "$SHADOW"` 无条件执行会删掉 `SHADOW_ROOT` 给的目录（现有唯一调用方 `cep_adr_verification_carriers_reverse_verify.sh` 传的是自己的 `$TMP/...`）|
| `${TMPDIR:-/tmp/opencode}` | CI 无 `/tmp/opencode`（`cep_gate_harness.py:34` 明文反对硬编码） |
| 只 `EXIT` trap | SIGKILL 无解；靠 `mktemp` 唯一性兜底残留 |
| `echo "[shadow] $SHADOW"` | 人复现需要可预测路径；AC-02 用它断言「两实例路径不同」 |
| `--list` 分支前置 | 3 个闸（`adr_structural` / `decision_block` / `dod_reading`）用 `exec` 返回 ⇒ **替换进程则 EXIT trap 不触发** ⇒ 每次 `--list` 泄漏一个 mktemp 目录。故 `--list` 分支必须在创建影子目录**之前** |

`SHADOW_ROOT` **语义不变**（仍是「完整路径覆盖」）。两个原以 `SHADOW_ROOT` 为工作变量的闸处理不同：
- `cep_tilelink_engine_failclosed`：下游 27 处引用 `$SHADOW_ROOT/...` ⇒ 初始化后补 `SHADOW_ROOT="$SHADOW"` 保持下游不变（仅此一处）。
- `cep_source_effect_check`：旧默认值虽写在 `SHADOW_ROOT` 上，但**下游全部用 `$SHADOW`**（28 处）⇒ 无需别名行（改造后 `$SHADOW_ROOT` 仅存于那行赋值判断）。

## D-2 · 覆盖义务判据 = **语料 → 快照**（与 R-d 既有方向互补）

- **谓词**：`section2(strip_fences(text)) is not None` —— **复用现函数**。⛔ 不新写正则：围栏内的 `## 2` 不得算章（`SIG_DANGER_FENCE` 证明这种围栏确实存在）。
- **关系**：`{语料中含 §2 章的 ADR-CEP-*.md}` ⊆ `{快照 keys}`。
- **签名**：`决策块覆盖缺失`（新稳定签名），携带文件名 + 登记义务提示。
- **双向**：既有段管「快照 → 语料」（文件缺失 / 计数偏离），新判据管「语料 → 快照」。
- ⚠️ **计数仍是闸外期望值**（人工登记 + 台账 Entry）；只有 **key 集**被机器推导。
- ⚠️ **同批改 3 处**：docstring 职责边界 · `--list` 判据清单 · spec（D-5）。

## D-3 · 双向 fixture（开火 + **不误报**）

| # | 影子根动作 | 期望 |
|:--:|---|---|
| **A** | 新增 `ADR-CEP-30-<probe>.md`（`## 2` + 一条 `### 决策 1`） | `check.py --root` **rc=1** 且输出含 `决策块覆盖缺失` **与**文件名 |
| **B** | 新增 `ADR-CEP-31-<probe>.md`（**`## 决策` 无编号**，复刻 `ADR-CEP-21` 形态） | 该判据**不红**（⛔ 不误报） |

⚠️ 命名理由（metis N-18）：R-c 的 `SPLIT_RE` 只匹配 `^ADR-CEP-0[123]` ⇒ `30`/`31` 不撞；写进脚本注释。

⚠️ 两条 fixture 都**不碰真实快照**（毒化面在语料侧）⇒ 无需 `--update`。

## D-4 · stale「22 份」按**指代表**订正（⛔ 不盲替换）

| 文件:行 | 所指 | 22 → |
|---|---|---|
| `cep_decision_block_check.py:11`/`:237`/`:504` · `.sh:10` · `scripts/README.md:73` · `test/CMakeLists.txt:286` | 快照键数 | **23** |
| `cep_freeze_check.sh:10` | 磁盘 `ADR-CEP-*.md` 文件数 | **24** |
| `cep_adr_structural_check.py:8` | `decision_adrs` 分母（磁盘 − 3） | **21 / 24** |
| `cep_docs_sync_check.sh:4` | `ADR_LIST` 条数 | 不写数字（以数组实际为准；W4 后 **24**） |

⚠️ `grep` 必须 `--include=*.py --include=*.sh --include=*.md --include=*.txt`（`__pycache__/*.pyc` 含「22 份」）。

## D-5 · 1 份 delta = **整条 Requirement 重发**（448 字符 / 9 Scenario 逐字保留）

三处定点改（已生成，见 change 目录）：
1. `spec.md:30` 自陈：`不提供「覆盖完整性」判据` → **提供**「覆盖义务」判据（但 ⛔ 不自动维护计数）；
2. 「R-d 段 SHALL 按四步口径计数…」Scenario 追加 **覆盖义务** AND bullet（含签名）；
3. 「判据 SHALL 声明其不可约残余」Scenario 追加 **④**（`## 决策` 无编号形态不在射程，先例 `ADR-CEP-21`）。

## D-6 · 覆盖集补齐（`docs_sync` 的 `ADR_LIST`）

`:50` 实测 **22 条**（末位 `ADR-CEP-NUMBER-MAP`；`-19` 已于 Entry #74 登记）⇒ 补 `-20`/`-21` ⇒ **24**（== 磁盘全量）。⚠️ 初稿写「20 条 / 末位 `-18` / 缺 3 份」为误读数组，实施时实测订正（proposal AC-11 + improvement W4 已同步）。⚠️ 该文件 `:72` 自述「漏登记 = 新 ADR **静默脱检**」⇒ 这是**功能性修复**，不是文档订正。

## D-7 · 边界（零放宽）

⛔ `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff · ⛔ **闸数 45 不变** · ⛔ 不新增 `INV-CEP-*` · ⛔ 不把 4 个自建工作区迁 helper（它们复制 `include/tlm/cep` 副本 / 探针目录，**非 ADR 语料**，不在 `cep-invariant-gate/spec.md:13` 射程内）· ⛔ 不改计数口径 · ⛔ 不动非 `cep-*` 的红 spec。
