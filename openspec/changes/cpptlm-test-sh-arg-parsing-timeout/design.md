# Design — cpptlm-test-sh-arg-parsing-timeout v1.0

> **状态**: 🟡 **Proposed**(2026-10-31)· **来源**: H7 对拍收尾轮暴露的既有缺陷(台账 Entry #120 §⑨)
> **配套**: [`proposal.md`](proposal.md) · [`specs/ci-test-entrypoint/spec.md`](specs/ci-test-entrypoint/spec.md) · [`tasks.md`](tasks.md)
> **估时**: 0.5 天 · **冻结面**: 零改动

---

## D-α 归因更正:「递归」⇒「**触发了全量编译**」

**问题**:台账 Entry #120 §⑨ 登记的根因是「递归」(第 3 项检查跑 `./test.sh --mode off --build`
⇒ 内部跑全量 ctest ⇒ 含它自己)。

**该归因未经实测**,是**推断**。本轮实测证伪:

```
$ BUILD_DIR=/tmp/opencode/__probe_bd bash -x ./test.sh --mode off --build
=== 构建模式: off (/tmp/opencode/__probe_bd) ===
[1/459] Building CXX object src/CMakeFiles/cpptlm_core.dir/core/coherence_domain.cc.o
[2/459] Building CXX object src/CMakeFiles/cpptlm_core.dir/core/connection_resolver.cc.o
...
```

**推理链**:
1. `[1/459]` ⇒ cmake **创建**了 `BUILD_DIR` 目录并**从零编译 459 个目标**;
2. 编译尚未结束即被 ctest 的 1500s 超时打断 ⇒ 命令**从未执行**到 `test.sh:222` 的
   `ctest --test-dir "$BUILD_DIR" --output-on-failure -E cute` ⇒ **「递归」不成立**;
3. 副作用:每次跑该检查**残留一个半成品构建树**(实测残留 `CMakeCache.txt` / `build.ninja` /
   `bin/` / `Testing/` / `CTestTestfile.cmake`)。

**决策**:归因更正为「**该自检触发了全量编译(459 目标)**」;台账以 **dated 注记**更正
(append-only,**不改写** Entry #120 原文)。

**理由**:本轮 H7 收尾轮的核心教训就是「**归因未实测就写入台账**」——本轮已推翻 3 条旧结论
(#115 的「v24 提前 EOF」· #119 的「v25 重写读侧」· 「v25 是解封前提」)⇒ **同款错误不得再犯第二次**。

**代价**:无(台账只追加)。

**落点**:`.rddf/roadmap/objectives/obj-cep-platform.md` Entry #120 §⑨;判据 AC-6。

---

## D-β 第 3 项检查的 `BUILD_DIR`:**不存在的目录** ⇒ **已存在的文件**

**问题**:该检查的意图是证明「`--build` **单独使用**时**解析阶段放行**」
(否则 `--build` 恒报「与 `--test-only` 互斥」—— 这正是 2026-10-24 `6a990316` 修的 bug)。

原实现用「一个**不存在的** `BUILD_DIR`」制造失败:

```bash
out="$(BUILD_DIR=/tmp/opencode/__no_such_build_dir__ ./test.sh --mode off --build 2>&1)"
```

⚠️ **不存在的目录** ⇒ cmake **创建**它 ⇒ **全量编译** ⇒ 1500s+ 超时(D-α)。

**决策**:`BUILD_DIR` 指向一个**已存在的普通文件** ⇒ cmake 在**配置阶段**立刻失败:

```
$ BUILD_DIR=/etc/hostname ./test.sh --mode off --build
=== 构建模式: off (/etc/hostname) ===
CMake Error: Unable to (re)create the private pkgRedirects directory:
/etc/hostname/CMakeFiles/pkgRedirects
(exit 1)
```

⚠️ **实测确认该输出不含「互斥」字样**(`grep -c 互斥` = 0)⇒ **断言语义保持**:
「输出不含互斥字样」仍成立 ⇒ **解析阶段放行**这一结论仍被证明。

**为什么用「文件」而不是「目录」**:

| 候选 | 行为 | 评价 |
|---|---|---|
| **不存在的目录**(现状) | cmake 创建 ⇒ 全量编译 ⇒ 超时 | ❌ |
| **已存在的文件** | cmake 配置阶段立刻失败(秒级)⇒ 无构建产物 | ✅ **采纳** |
| 已存在的**目录** | cmake 在该目录**配置** ⇒ 仍会跑 configure + build | ❌ 仍慢 |
| 不可写的目录 | 权限报错 ⇒ 可能更早失败 | ⚠️ 依赖运行环境权限 ⇒ 脆弱 |

**为什么不用「给 `test.sh` 加解析阶段退出钩子」**(更「干净」但 scope 大):
那要在**生产脚本**里加一个只为测试存在的开关 ⇒ 生产路径引入测试专用分支 ⇒
违反「改动最小化 + 生产脚本不夹带测试钩子」⇒ **不采纳**;
登记为**备选**(若将来 `test.sh` 重构后「配置文件当 BUILD_DIR」不再可靠)。

**代价**:`BUILD_DIR` 的语义从「一个应当不存在的构建目录」变成「一个应当不是目录的路径」
⇒ 需在脚本注释里写明**为什么**;这是**测试脚本内的一行**,不外溢。

**落点**:`scripts/test/test_sh_arg_parsing.sh` 第 3 项检查;判据 AC-1 / AC-2 / AC-3。

---

## D-γ 新增第 4 项检查作为**反向 fixture**(防回退)

**问题**:D-β 的修法是「把一个**魔法路径**换成另一个**魔法路径**」⇒ 若有人日后把
`BUILD_DIR` 改回「不存在的目录」,**没有任何东西会报警**(超时只会表现为「CI 慢」)。

**决策**:新增第 4 项检查 —— **「自检不得产生任何构建产物」**:

```
④ BUILD_DIR 指向**已存在的文件** ⇒ 必须**秒级**失败且**不产生**构建目录
   ⇒ 断言:① 失败发生在配置阶段(输出含 CMake Error)② 指定路径下**未出现**新目录/文件
   ⇒ ⚠️ 该检查**自带反向 fixture**:若形态回退为「不存在的目录」⇒ cmake 会创建该目录
     ⇒ 断言②**因正确原因红**
```

**为什么必须是「可执行的反向 fixture」而不是「注释提醒」**:
本仓判据铁律 ② 要求「每条判据有可执行反向 fixture 且**因正确原因红**」
⇒ 注释提醒不产生任何机器信号 ⇒ 不满足纪律。

**代价**:多一次「秒级」的 cmake 失败调用(可忽略)。

**落点**:`scripts/test/test_sh_arg_parsing.sh` 第 4 项检查;判据 AC-3 / AC-4。

---

## D-δ 「全量 ctest 全绿」作为**可证伪**的验收口径

**问题**:本仓多条文档/台账把「全量 `ctest` 绿」当作验收断言
⇒ 而该断言当前**恒不成立**(必须排除 `test_sh_arg_parsing`)⇒ 变成一句**不可证伪的话**。

**决策**:本 change 的完成条件包含 **`ctest --test-dir build -j4` 不排除任何测试即全绿**(AC-5)。

**理由**:一个**需要人工排除**才能成立的「全绿」比「明确红」更危险 ——
它训练读者忽略排除项 ⇒ 真回归被连带掩盖。

**落点**:实施期验收;判据 AC-5。

---

## D-ε 与 H7 对拍 change 的**边界**

本 change **只**修 `test_sh_arg_parsing` 的超时;
H7 对拍 change(台账 Entry #120)已完成并结项 ⇒ 本 change **不**触碰
`scripts/cep/parity/**` · `14 号` · `test/test_cep_parity_cpptlm_side.cc` · 任何 ADR。

⚠️ **唯一的跨 change 交互** = 台账 Entry #120 §⑨ 的**归因 dated 更正**(D-α)
⇒ 该动作**属于本 change**(因为归因错误是本 change 的对象)⇒ 不算越界。

---

## 已排除的替代方案

| 方案 | 排除理由 |
|---|---|
| 按「递归」归因去改 `test.sh`(加排除自身机制) | **归因被实测证伪**(D-α)⇒ 会改错地方;且改**生产脚本** |
| 给 `test.sh` 加 `CPPTLM_PARSE_ONLY` 之类的解析阶段退出钩子 | 生产路径引入测试专用分支 ⇒ scope 大于收益(登记为备选) |
| 第 3 项检查改用 `--build-only` | `--build-only` 单独使用**仍会真的构建** ⇒ 同样慢 |
| 让第 3 项检查跑一个**子集** ctest | 该检查的断言是「解析阶段放行」,与 ctest 无关 ⇒ 换 ctest 语义不匹配 |
| 直接**删掉**第 3 项检查 | 它是 2026-10-24 `6a990316` 那个真 bug 的**回归锁** ⇒ 删掉 = 丢保护 |
| 把该测试从 ctest 里**移除** | 同上 ⇒ 且会让「全量 ctest 全绿」变成**排除法**的假象 ⇒ 与 D-δ 相悖 |
