## Tasks

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」。
> **范围纪律**：⛔ 不改证伪逻辑 · ⛔ 不扩语料 · ⛔ 不定义 `rfe`/`fr`/`ws` · ⛔ 零冻结面改动（AC-5）· ⛔ 阶段 2 的改名不在本 change。

### Task 1: 头文件消歧常量 + 诚实边界段升级

- [x] `include/tlm/cep/litmus_falsifier.hh` 顶部「诚实边界」段**升级**：含**顶点形态**（`(instance, network, vc)`）+ **判据对象**（环存在性）+ **与内存模型 litmus 的区别**（否定语境）三要素（AC-3）
- [x] 同文件新增两个常量：`kFalsifierScope = "channel-dependency-graph"` · `kFalsifierIsMemoryModelLitmus = false`（AC-1）
- [x] ⛔ 既有实现（找环 / 语料解析 / 判定字段）**逐字不变**

### Task 2: 测试断言追加（⛔ 既有断言段逐字不变）

- [x] `test/test_cep_litmus_falsification.cc` **文件尾部**追加 `// == disambiguation additions ==` 分隔行 + `static_assert(!tlm::cep::kFalsifierIsMemoryModelLitmus)` + `kFalsifierScope` 逐字相等断言（AC-1 / AC-4）
- [x] ⛔ 既有 4 个 `TEST_CASE` 逐字不变（AC-4）

### Task 3: 禁用表述闸（新）

- [x] 新建 `scripts/test/cep_litmus_naming_check.sh`：扫描 3 文件；命中「memory-model litmus」/「herd7」/「RISC-V litmus」/「内存模型 litmus」且**同一行**无否定标记 ⇒ 红，红因码 `litmus-scope-claim`（AC-2）
- [x] 闸内**自测三路**：① 肯定句 ⇒ 红 ② 「不是…」⇒ 绿 ③ 「与…的区别是…」⇒ 红（对**影子副本**操作 ⇒ 仓内零改动）
- [x] probe TU 反 fixture：`kFalsifierIsMemoryModelLitmus` 改 `true` ⇒ 编译失败**且**命中签名（AC-1）
- [x] ⛔ 闸**只读** `openspec/specs/cep-litmus-falsification/spec.md`，⛔ 不改它（AC-5）

### Task 4: 注册与登记

- [x] `test/CMakeLists.txt` 注册 `cep_litmus_naming_check`（ctest）
- [x] `scripts/README.md` §CEP 契约守门脚本 **守门表** 新增 1 行

### Task 5: 验收与收口

- [x] `bash scripts/test/cep_litmus_falsification_check.sh` rc=0（AC-4 逻辑等价）
- [x] `bash scripts/test/cep_litmus_naming_check.sh` rc=0（AC-1/2/3）
- [x] `ctest --test-dir build -R cep` 全绿（AC-6）· 全部 `scripts/test/cep_*.sh` rc=0（AC-6）
- [x] `bash scripts/test/cep_docs_sync_check.sh --strict` rc=0（AC-6）
- [x] AC-5 硬查：`git diff --name-only` 不含 `openspec/specs/cep-litmus-falsification/spec.md` · `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md`
- [x] 计数同步：登记册 §0.1（闸 50→51 · 闸族 64→65）· `test_cep_*.cc` 78→79 · 归档 171→172 · CEP 归档 55→56 · **spec 总数 139 不变**（零 delta）· KPI 活口径 1/3（实施期）
- [x] 台账 Entry（`obj-cep-platform.md`）登记本单元
- [x] verifier 本地验收报告落 `.rddf/reviews/cep-litmus-falsifier-naming-disambiguation-verify.md`
