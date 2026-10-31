# cpptlm-test-sh-arg-parsing-timeout: `test_sh_arg_parsing` 超时修复 —— 让「全量 ctest 全绿」成为可成立的验收口径

> **状态**: 🟡 **Proposed**(2026-10-31)· **来源**: H7 对拍收尾轮(台账 Entry #120 §⑨)暴露的既有缺陷
> **立项依据**: **用户裁决 U2**(2026-10-31):「建独立 change 修」
> **队列**: **不占 CEP 队列**(非 CEP 平台能力;属**测试基础设施**修复)⇒ 不新增 `obj-cep-platform §4.0` 队列行
> **KPI**: OpenSpec Proposed 并发 **0/3 → 1/3**(per `AGENTS.md`「Proposed 存量 ≤3」)
> **估时**: **0.5 天**(1 个脚本 + 1 个 ctest 判据 + 台账 1 条 dated 更正)
> **冻结面**: **零改动**(40 件 M1 manifest 不含 `scripts/test/` 与 `test.sh`)
> **关联**: 台账 `obj-cep-platform.md` Entry #120 §⑨(既有缺陷登记 + dated 更正)· `scripts/test/test_sh_arg_parsing.sh` · `test.sh`

---

## Why

### 现状:「全量 ctest 全绿」这个验收口径**永远无法成立**

`ctest --test-dir build -j4` 的 125 条测试中,`test_sh_arg_parsing` **超时 1500s**
⇒ 每次全量回归都必须**排除**它才能得到「全绿」
⇒ 于是「全量 ctest 绿」这句话在本仓**不可证伪** —— 真回归与超时混在一起,只能靠人肉排除。

### 更要紧的是:**原先登记的根因是错的**(实测证伪)

台账 Entry #120 §⑨ 登记的根因是「**递归**」:

> 「该测试的第 3 项检查跑 `./test.sh --mode off --build` ⇒ `test.sh` 内部跑全量 ctest
> ⇒ 全量 ctest 里又含它自己 ⇒ 无限递归」

**实测证伪**(`BUILD_DIR=/tmp/opencode/__probe_bd bash -x ./test.sh --mode off --build`):

```
[1/459] Building CXX object src/CMakeFiles/cpptlm_core.dir/core/coherence_domain.cc.o
[2/459] Building CXX object src/CMakeFiles/cpptlm_core.dir/core/connection_resolver.cc.o
...
```

⇒ **超时发生在编译阶段**,`[1/459]` 表示 cmake 在一个**全新目录**里从零配置并编译 **459 个目标**
⇒ 命令**从未走到** `test.sh:222` 的 `ctest --test-dir ...` ⇒ **「递归」不成立**。

⇒ 且每次跑该检查都会**残留一个半成品构建树**(实测 `/tmp/opencode/__no_such_build_dir__/` 下
留下 `CMakeCache.txt` / `build.ninja` / `bin/` / `Testing/`)。

### 为什么必须先纠正归因、再修

若按「递归」去修(例如给 `test.sh` 加排除自身的机制),会**改错地方**:
「递归」暗示要改 `test.sh` 的 ctest 调用;而真根因是「**这个自检不该触发任何构建**」
⇒ 正确修法在**测试脚本**里,且**零生产代码改动**。

⇒ 本 change 的第一件事就是**把归因改正**(append-only:加 dated 注记,不改写 Entry #120 原文)。

---

## What Changes

### 变更 1(核心):第 3 项检查的 `BUILD_DIR` 从「**不存在的目录**」改为「**已存在的文件**」

`scripts/test/test_sh_arg_parsing.sh` 的第 3 项检查,其目的是证明:

> 「`--build` **单独使用**时**解析阶段必须放行**(不报 `--build` 与 `--test-only` 互斥错)」
> ⇒ 原实现用「一个**不存在的** `BUILD_DIR`」让命令在**构建阶段**失败。

**问题**:不存在的目录 ⇒ cmake **创建**它 ⇒ 从零**全量编译 459 个目标** ⇒ 1500s+ 超时。

**改法**:让 `BUILD_DIR` 指向一个**已存在的普通文件** ⇒ cmake 在**配置阶段**立刻失败:

```
$ BUILD_DIR=/etc/hostname ./test.sh --mode off --build
=== 构建模式: off (/etc/hostname) ===
CMake Error: Unable to (re)create the private pkgRedirects directory:
/etc/hostname/CMakeFiles/pkgRedirects
(exit 1)
```

⚠️ **实测确认该报错不含「互斥」字样**(`grep -c 互斥` = 0)⇒ **断言语义保持不变**
(仍然证明「解析阶段放行」),而耗时从 **1500s+ → 秒级**。

⇒ **改动量 = 1 个文件、1 行**,**零生产代码改动**(`test.sh` 不动)。

### 变更 2:新增第 4 项检查作为**反向 fixture**(防回退)

新增一条断言:**第 3 项检查不得产生任何构建产物**(指定路径下不得出现新目录/文件)
⇒ 若有人把 `BUILD_DIR` 改回「不存在的目录」,该断言**因正确原因红**
(它会看到 cmake 真的创建了目录并开始编译)。

### 变更 3:台账 Entry #120 §⑨ 的**错误归因**加 dated 更正

把「递归」更正为「**触发了全量编译(459 目标)**」⇒ 追加 dated 注记,**不改写原文**(append-only)。

---

## Acceptance

| # | 判据 | 机器可判方式 |
|---|------|-------------|
| **AC-1** | 第 3 项检查 SHALL **秒级**完成 | 单项断言:`elapsed < 30s` |
| **AC-2** | 第 3 项检查 SHALL **仍能抓住**「`--build` 恒报互斥错」的回归 | 断言「输出**不含**互斥字样」+「命令**非零**退出」⇒ 若 `test.sh` 回归到恒报互斥 ⇒ 该检查红 |
| **AC-3** | 第 3 项检查 SHALL **不产生任何构建产物** | 断言「指定 `BUILD_DIR` 路径下未出现新目录/文件」 |
| **AC-4** | **反向 fixture**:若 `BUILD_DIR` 形态回退为「不存在的目录」⇒ AC-3 SHALL **因正确原因红** | 第 4 项检查显式构造「不存在的目录」形态并断言「产生了构建目录」⇒ 该检查自身红 |
| **AC-5** | 全量 `ctest --test-dir build -j4`(**不排除任何测试**)SHALL **全绿** | ctest 退出码 0;**当前因该测试超时而失败** |
| **AC-6** | 台账 Entry #120 §⑨ 的错误归因 SHALL 以 **dated 注记**更正(原文不改写) | 文本审阅 + `cep_docs_sync_check` rc=0 |

⇒ **判据纪律映射**(per 本仓 4 条铁律):
- ① 纯负向断言须有正控 ⇒ AC-2(负向)+ AC-1/AC-3(正向)配对;
- ② 每条判据有**可执行反向 fixture** 且**因正确原因红** ⇒ AC-4;
- ③ 编译期锁不得只以「编译红」充当 ctest 判据 ⇒ 本 change 无编译期锁;**AC-5 是 ctest 判据**;
- ④ fixture 状态变迁后必须重新基线化 ⇒ 第 4 项检查是**新增** ⇒ 实施时先跑通基线。

---

## Capabilities

| capability | 状态 | 说明 |
|---|---|---|
| **`ci-test-entrypoint`** | **ADDED** | CI 测试入口的**可执行契约**:`test.sh` 的参数解析自检 SHALL 不得触发构建 / 不得超时;自检本身 SHALL 保有鉴别力 |

⇒ **无 MODIFIED / 无 REMOVED** ⇒ 不触碰任何既有 main spec ⇒ 归档时**只新增**一份 delta spec。

---

## Impact

| 维度 | 影响 |
|---|---|
| **代码** | `scripts/test/test_sh_arg_parsing.sh` **1 个文件**(改 1 行 + 加 1 项检查)⇒ **零生产代码改动** |
| **测试** | `ctest --test-dir build -j4` 从「必须排除 1 条才绿」变成「**全绿**」⇒ 验收口径恢复可证伪 |
| **冻结面(M1, 40 件)** | **零改动**(`scripts/test/` 与 `test.sh` 均不在 manifest 内) |
| **台账** | Entry #120 §⑨ 加 **1 条 dated 更正**(append-only)⇒ 不改写任何历史 entry |
| **OpenSpec KPI** | Proposed 并发 **0/3 → 1/3** ⇒ 未超上限 |
| **不做** | 不改 `test.sh`(生产脚本)· 不加解析阶段退出钩子 · 不改 `test/CMakeLists.txt` 的其他条目 · **不 push** |
