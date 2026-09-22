# ArchForge 拆仓实施计划：dGPU SoC 设计 + NSA 文档独立项目

> **版本**: v2.1 · **日期**: 2026-09-20 · **策略变更**: B+（仓内硬化）→ **C-archforge**（新建 ArchForge 设计仓）
> **状态**: ✅ Oracle + Metis 双审查通过（MINOR_ISSUES 已修复）
> **关联 Oracle**: `ses_f3e2a22d8ffemsW7U7FWGaRxPn`（原始 B+ 评估） + `ses_f38119273ffe17UCgim0dHEPZi` + `ses_f38116420ffeMhdoT5KMrrotz8`（双审查）

---

## 策略变更说明

原始 B+ 计划（v1.0）采用"仓内硬化"方式，在单仓内通过 4 步（MVP 组件迁移 → CMake 双库 → docs 漂移修复 → NSA 子目录化）达成"拆分就绪"状态。经 Oracle 评估后确认：

- B+ 在 CppTLM 内部的 4 步仍然有效，但仅是 **过渡状态**
- **当前决策**: 直接创建独立设计仓 `/workspace/project/ArchForge/`，将 dGPU SoC 全部设计文档（`docs/soc_arch/`）及 NSA 领域设计迁移至新仓
- CppTLM 保留：全部源代码（`include/` `src/` `test/`）+ 核心框架文档（`docs/architecture/` `docs/adr/`）+ 实施计划

### 分仓原则

| 维度 | CppTLM（实现仓） | ArchForge（设计仓） |
|------|------------------|---------------------|
| **核心内容** | TLM 仿真框架 + PCIe EP 全链路 + GPGPU 实现 | dGPU SoC 架构设计 + IP 模块微架构 + NSA fabric |
| **文档** | `docs/architecture/`（核心框架）+ `docs/adr/`（通用） | `docs/soc_arch/architecture/` + `docs/soc_arch/adr/` |
| **源文件** | `include/` `src/` `test/` — 全部代码 | 无代码（纯设计文档仓） |
| **配置文件** | CMake, JSON 拓扑, test.sh | 无（仅 README + 目录结构） |
| **实现计划** | `docs/implementation/` | 由 CppTLM 推进会引用 |

### 迁移总量

| 目录 | 文件数 | 大小 | 内容 |
|------|--------|------|------|
| `docs/soc_arch/architecture/` | 43 份 | ~1.2 MB | SoC 架构文档（00-overview ~ 31-nsa） |
| `docs/soc_arch/adr/` | 27 份 | ~600 KB | ADR-SOC-01~24 + README + 附属 |
| `docs/soc_arch/modules/` | 53 份 | ~700 KB | IP 模块微架构设计 |
| `docs/soc_arch/specs/` | 1 份 | ~300 KB | apu-soc-design.md |
| `docs/soc_arch/roadmap/` | 8 份 | ~100 KB | 路线图 |
| **合计** | **~132 份** | **~2.9 MB** | **全部 dGPU SoC 设计文档** |

其中 NSA 子集（22-31 架构 + ADR-SOC-22/23/24）约 14 份，随迁不单独列出。

---

## §0 前置准备（Step 0）

### 0.1 环境快照

- [ ] 确认 HEAD = `7e15124e`
- [ ] 处理脏工作区（当前 23 处 M/??，含 NSA 在途 + v2.0 计划更新）：

```bash
# 在途 NSA + MVP 设计文档（未跟踪）：
git add docs/soc_arch/adr/ADR-SOC-22-nsa-fabric-address-format.md
git add docs/soc_arch/adr/ADR-SOC-23-nsa-vs-plan-b-selection.md
git add docs/soc_arch/adr/ADR-SOC-24-capability-based-multitenant-isolation.md
git add docs/soc_arch/architecture/21-dist-scale-up-topology-b.md
git add docs/soc_arch/architecture/22-nsa-fabric-address-spec.md
git add docs/soc_arch/architecture/23-dist-scale-up-topology.md
git add docs/soc_arch/architecture/24-host-gpu-pcie-ifc.md
git add docs/soc_arch/architecture/25-nsa-hardware.md
git add docs/soc_arch/architecture/26-gsp-rm-firmware.md
git add docs/soc_arch/architecture/27-nsa-capability.md
git add docs/soc_arch/architecture/28-cxl-3-fabric.md
git add docs/soc_arch/architecture/29-nsa-evolution-roadmap.md
git add docs/soc_arch/architecture/30-nsa-comprehensive-supplement.md
git add docs/soc_arch/architecture/31-nsa-fault-injection-plan.md
git add docs/validation/2026-09-19-cpptlm-nsa-scale-up-oracle-pass.md
git add docs/implementation/14-boundary-hardening-stage0.md
git add docs/implementation/README.md

# 活跃 openspec changes（在途）
git add openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/
git add openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/
git add openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/

# 已完成修改（ADR-SOC-21 等现有文件）
git add docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md
git add docs/soc_arch/adr/README.md
git add docs/soc_arch/architecture/21-dma-backends-mvp.md
git add docs/soc_arch/architecture/21-fabric-switch-mvp.md
git add docs/soc_arch/architecture/21-microarch-ifc-mvp.md
git add docs/soc_arch/architecture/21-soc-topology-mvp.md
git add docs/soc_arch/architecture/21-tee-udd-mvp.md

git commit -m "chore(snapshot): pre-archforge in-flight NSA + MVP docs + ADR-22/23/24"

# 或：git stash -u（全部暂存）
```

**不可跳过**：设计文档含大量 git mv 操作，脏工作区将导致 `git mv` 失败或丢历史。

- [ ] 确认 `build/` 构建产物最新
- [ ] 记录基线：

```bash
./build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3   # 期望 44498 assertions 全绿
openspec validate --changes --strict                          # 期望 10/10 PASS
```

### 0.2 建立临时分支

```bash
git checkout -b feature/archforge-split
```

### 0.3 活跃面清单

| 面 | 约束 |
|---|---|
| openspec 活跃 changes | `2026-09-19-cpptlm-dgpu-gmmu-mvp` / `-mas-soc-topology-mvp` / `-nsa-scale-up-umbrella`（引用 `docs/soc_arch/` → 拆仓后需 VIRTUAL_PATHS 或跨仓引用） |
| 回归标签 | `[pcie]` `[axi]` `[chstream]` `[gmmu]` `[io_dma]` `[e2e]` `[cfg-encoding]` |
| **冻结面（零修改）** | `include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` |

### 0.4 关键路径依赖

**ArchForge 拆仓前提条件**（必须保证的顺序）：

1. ✅ Step 0 → Step 1：ArchForge 目录必须先存在，才能接收文件
2. ✅ Step 1 → Step 2：copy 操作无顺序依赖，但清理前必须确认 copy 完整
3. ⚠️ Step 2（O）→ build 验证：copy 不影响 CppTLM 构建（只操作 docs/）
4. ⚠️ Step 2 → Step 3：必须在 Step 2 copy 确认后，再删 CppTLM 源（回滚链）
5. ⚠️ Step 3 → Step 4：docs_sync_check 白名单必须先扩展（否则 git rm 后 pre-commit 报错）
6. ⚠️ Step 4 → Step 5：AGENTS.md 必须先更新引用，再改测试/CI

---

## §1 Step 1: 创建 ArchForge 项目骨架

**目标**: 在 `/workspace/project/ArchForge/` 创建独立项目结构，能接收并展示 dGPU SoC 设计文档。

### 1.1 目录结构

```
/workspace/project/ArchForge/
├── README.md              # 项目说明 + 与 CppTLM 关系
├── .gitignore
├── docs/                  # 全部设计文档
│   ├── soc_arch/          # dGPU SoC 架构（从 CppTLM 迁入）
│   │   ├── adr/           # ADR-SOC-01~24
│   │   ├── architecture/  # 00-overview ~ 31-nsa
│   │   ├── modules/       # IP 模块微架构
│   │   ├── specs/         # apu-soc-design.md
│   │   └── roadmap/       # 路线图
│   └── nsa/               # 🆕 NSA 领域概览（可选交叉索引）
├── CROSS_REPO.md           # 与 CppTLM 的跨仓引用说明
└── .omo/                   # OpenCode 配置
    └── AGENTS.md
```

### 1.2 操作（幂等版）

**前置检查**：ArchForge 骨架可能已存在（占位 README + 初始 commit `fbda279`）。脚本必须幂等：

```bash
if [ ! -d /workspace/project/ArchForge/.git ]; then
  echo "[Step 1.2] ArchForge 未初始化，执行完整骨架创建"
  mkdir -p /workspace/project/ArchForge/docs/soc_arch/{adr,architecture,modules,specs,roadmap}
  cd /workspace/project/ArchForge
  git init
  git checkout -b main
  echo "[Step 1.2] 完成"
else
  echo "[Step 1.2] ArchForge 已存在，跳过骨架创建 (current HEAD: $(git rev-parse --short HEAD))"
fi
```

**若 .git 存在但分支不是 main**：`git branch -m master main`（git init 默认 master 分支）

**若 README.md 已存在且与 §1.3 模板一致**：跳过覆写；否则用 §1.3 内容替换

### 1.3 `README.md` 内容

```markdown
# ArchForge — dGPU SoC Architecture & Design Repository

> **本仓是 CppTLM 的设计伴侣仓**，存有全部 dGPU SoC 架构设计文档。
> **源代码实现** → [CppTLM](../CppTLM/)（`include/tlm/`, `src/tlm/`）

## 范围

- **dGPU SoC 架构**: `docs/soc_arch/architecture/`（00-overview ~ 31-nsa，43 份）
- **架构决策记录**: `docs/soc_arch/adr/`（ADR-SOC-01~24，27 份）
- **IP 模块微架构**: `docs/soc_arch/modules/`（53 份设计文档）
- **设计规格**: `docs/soc_arch/specs/`
- **路线图**: `docs/soc_arch/roadmap/`

## 与 CppTLM 的关系

```
ArchForge（设计仓）            CppTLM（实现仓）
┌──────────────────┐          ┌──────────────────┐
│ docs/soc_arch/   │  ← 引用 → │ code + tests     │
│ （架构/ADR/模块） │          │ core framework   │
│ 系统级设计决策    │          │ 实现级设计文档     │
└──────────────────┘          └──────────────────┘
```

CppTLM 的 `AGENTS.md` / `docs/` 中引用 `docs/soc_arch/` 的路径，通过 **VIRTUAL_PATHS** 机制保持可读（详见 `CppTLM/scripts/test/docs_sync_check.sh`）。

## 维护约定

- **设计变更**：修改文档内容 → 在 ArchForge 仓中提交 PR
- **实现跟踪**：CppTLM 中的 openspec change 若影响设计 → 注明跨仓引用
- **回滚一致性**：跨仓变更必须同步（单仓回滚不影响另一仓状态）
```

### 1.4 验证

```bash
ls -la /workspace/project/ArchForge/
git -C /workspace/project/ArchForge log --oneline  # 期望: 1 commit (initial)
```

### 1.5 时间估算

目录创建 + git init + README + .gitignore ≈ **30 分钟**。

---

## §2 Step 2: dGPU SoC 设计文档迁移（从 CppTLM 复制至 ArchForge）

**目标**: 将 CppTLM 中 `docs/soc_arch/` 下全部设计文档（~132 份，~2.9 MB）镜像到 ArchForge。

### 2.1 复制全套文档

```bash
# 从 CppTLM 拷贝到 ArchForge（保留目录结构，cp -a 保留 mtime）
cp -a /workspace/project/CppTLM/docs/soc_arch /workspace/project/ArchForge/docs/
```

### 2.2 在 ArchForge 中创建跨仓 README

```bash
cat > /workspace/project/ArchForge/docs/README.md << 'EOF'
# ArchForge 设计文档

> **本目录是 CppTLM 设计伴侣仓的内容子集**，迁移自 `CppTLM/docs/soc_arch/`。
> 实现层文档和架构仍位于 `CppTLM/docs/architecture/`。

## 目录说明

| 子目录 | 内容 | 文件数 |
|--------|------|--------|
| `soc_arch/architecture/` | dGPU SoC 架构设计（00-overview ~ 31-nsa） | 43 |
| `soc_arch/adr/` | SoC 架构决策记录（ADR-SOC-01~24） | 27 |
| `soc_arch/modules/` | IP 模块微架构设计 | 53 |
| `soc_arch/specs/` | 设计规格 | 1 |
| `soc_arch/roadmap/` | 路线图 | 8 |
EOF
```

### 2.3 ArchForge 首次提交

```bash
cd /workspace/project/ArchForge
git add .
git commit -m "feat(init): import dGPU SoC architecture design docs from CppTLM" \
  -m "来源: CppTLM @ 7e15124e" \
  -m "迁移范围: docs/soc_arch/ (architecture + adr + modules + specs + roadmap)"
```

### 2.4 验证完整性

```bash
cd /workspace/project/ArchForge

# 文件数匹配
echo "arch files: $(ls docs/soc_arch/architecture/ | wc -l)"        # 期望 43
echo "adr files: $(ls docs/soc_arch/adr/ | wc -l)"                  # 期望 27
echo "module files: $(ls docs/soc_arch/modules/ | wc -l)"           # 期望 53
echo "spec files: $(ls docs/soc_arch/specs/ | wc -l)"               # 期望 1
echo "roadmap files: $(ls docs/soc_arch/roadmap/ | wc -l)"          # 期望 8

# 与 CppTLM 对比
diff -rq /workspace/project/CppTLM/docs/soc_arch /workspace/project/ArchForge/docs/soc_arch \
  | grep -v "Only in /workspace/project/ArchForge"                   # 期望: 无差异

# 关键文档可读性检查
head -3 /workspace/project/ArchForge/docs/soc_arch/architecture/00-overview.md
```

### 2.5 从 CppTLM 中删除

```bash
cd /workspace/project/CppTLM

# git rm 全部迁移文件（保留 git history）
git rm -r docs/soc_arch

# 保留占位 README（说明这些文档已移至 ArchForge）
mkdir -p docs/soc_arch
cat > docs/soc_arch/README.md << 'EOF'
# dGPU SoC 架构设计文档 — 已迁移

> **这些文档已移至独立的 ArchForge 设计仓库**（`/workspace/project/ArchForge/`）。
> 
> 此目录保留仅为兼容现有路径引用（VIRTUAL_PATHS 机制）。
> 如需修改设计文档，请前往 `/workspace/project/ArchForge/` 操作。

## 迁移内容

| 子目录 | 文件数 | 新位置 |
|--------|--------|--------|
| `architecture/` | 43 | `/workspace/project/ArchForge/docs/soc_arch/architecture/` |
| `adr/` | 27 | `/workspace/project/ArchForge/docs/soc_arch/adr/` |
| `modules/` | 53 | `/workspace/project/ArchForge/docs/soc_arch/modules/` |
| `specs/` | 1 | `/workspace/project/ArchForge/docs/soc_arch/specs/` |
| `roadmap/` | 8 | `/workspace/project/ArchForge/docs/soc_arch/roadmap/` |

**迁移时间**: 2026-09-20 · **来源 commit**: `7e15124e`
EOF

git add docs/soc_arch/README.md
git commit -m "refactor(docs): migrate dGPU SoC design docs to ArchForge repo

docs/soc_arch/ (architecture + adr + modules + specs + roadmap) moved
to independent repository /workspace/project/ArchForge/.
CppTLM retains only a placeholder README with migration info.
docs_sync_check.sh VIRTUAL_PATHS extended to keep AGENTS.md / docs/
references readable across repos.

Scope: ~132 files, ~2.9 MB
New repo: /workspace/project/ArchForge/ (init @ 7e15124e parent)"
```

### 2.6 验证 CppTLM 构建不受影响

```bash
cd /workspace/project/CppTLM
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3   # 期望 44498 assertions 全绿
```

> **注意**: 此步仅操作 docs/，不影响源文件/CMake/测试，构建不应受影响。

### 2.7 回滚方案

```bash
# CppTLM 侧：回滚 Step 2.5 的删除提交（假设是 HEAD，实际情况取 commit sha）
git revert <step2-delete-commit-sha>

# ArchForge 侧：回滚（若需要）
git -C /workspace/project/ArchForge revert <archforge-import-commit-sha>
# 或回到初始骨架：
git -C /workspace/project/ArchForge reset --hard fbda279
```

### 2.8 时间估算

复制 + 验证 30min + CppTLM 删除 15min + git commit 5min + 构建回归 30min + 回滚文档 30min ≈ **2 小时**。

---

## §3 Step 3: CppTLM 内部交叉引用修复 + VIRTUAL_PATHS 白名单

**目标**: CppTLM 中所有指向 `docs/soc_arch/` 的引用标记为 VIRTUAL_PATHS（不损坏现存链接），清除漂移。

### 3.1 扩展 `scripts/test/docs_sync_check.sh` 的 VIRTUAL_PATHS

**关键发现**（Metis 审查报告）: `docs_sync_check.sh` 的 `is_virtual_path()` 使用 `[[ "$path" == *"$vp"* ]]` **子串匹配**（非 glob 模式），且 `PATH_REGEX` **不匹配 `.md` 文件**。因此：

- `docs/soc_arch/*.md` 引用**根本不被检查**，VIRTUAL_PATHS 扩展对 checker 而言**不是强阻塞**
- 但作为"文档化声明"仍保留，避免未来扩展时突然失败

**正确条目形式**（子串匹配，故必须去 `*`）：

```bash
# docs/soc_arch/ 全目录为虚拟路径（已迁移至 /workspace/project/ArchForge/）
# 注意: 是子串匹配而非 glob，必须用纯前缀（去 *）
"docs/soc_arch/architecture/"  # 覆盖 architecture 下所有文件
"docs/soc_arch/adr/"           # 覆盖 adr 下所有文件
"docs/soc_arch/modules/"       # 覆盖 modules 下所有文件
"docs/soc_arch/specs/"         # 覆盖 specs 下所有文件
"docs/soc_arch/roadmap/"       # 覆盖 roadmap 下所有文件
```

**清理历史特定条目**：现有 `VIRTUAL_PATHS` 中已有 19 条指向已删除的特定文件（如 `docs/soc_arch/modules/gpu-kernellaunch.md`）。新前缀条目覆盖它们，建议清理避免维护负担。

### 3.2 修复 CppTLM 端交叉引用（分级策略）

经 Oracle 实测，引用 `docs/soc_arch` 的文件**远超**计划初版列出的 6 个。按优先级分三级处理：

#### 🔴 P0 必须修改

| 文件 | 行/位置 | 操作 |
|------|---------|------|
| `AGENTS.md` STRUCTURE 节 | 约 80-95（`docs/soc_arch/adr/`、`docs/soc_arch/architecture/`、`docs/soc_arch/modules/`、`docs/soc_arch/roadmap/` 4 行） | 改为指向 `../ArchForge/`，加注「设计仓」 |
| `AGENTS.md` WHERE TO LOOK 节 | `7 阶段 roadmap` 引用行（197） | 同上 |
| `AGENTS.md` STRUCTURE 节 | `★ dGPU SoC 子项目 ADR` 等描述段 | 删除（已迁出） |
| `AGENTS.md` PHASE STATE 节 | NSA 描述行 | 改为指向 ArchForge |

#### 🟡 P1 推荐修改（注释/链接过期但不影响功能）

| 文件 | 操作 |
|------|------|
| `docs/cross_repo/` 4 份 HSK 文档 | 添加一行历史引用注记：`> 注: docs/soc_arch/ 已迁至 ArchForge 仓` |
| `docs/roadmap/` 3 份文件 | 同上模式 |
| `docs/superpowers/` 5+ 份 | 同上 |
| `docs/validation/` 2 份（含在途 `2026-09-19-cpptlm-nsa-scale-up-oracle-pass.md`） | 同上 + 保留在 CppTLM（验证闭环） |

#### 🟢 P2 仅文档注释修复（不破坏）

| 文件 | 操作 |
|------|------|
| `include/tlm/gpu/{gpu_soc_tlm,i_compute_device,instruction_descriptor,streaming_multiprocessor}.hh` | 头部注释加 `> Note: design doc → ArchForge` |
| `include/bundles/sm_bundles_tlm.hh` | 同上 |
| `include/tlm/pcie/pcie_mock_ip.hh` | 同上 |
| `src/tlm/gpu/streaming_multiprocessor_tlm.cc` | 同上 |
| `examples/generate_apu_soc.py` | docstring 中加注 |

### 3.3 `.rddf/` 引用断裂决策（Metis P0-1）

经审查，`.rddf/` 目录下 26 处 `docs/soc_arch/` 引用（`roadmap/phases/phase-7.md`、`phase-9.md`、`plans/2027-09-17-...`、`hub-pr-text-cpptlm-abi-secondary-slimming.md` 等）。

**决策**: **不修复 `.rddf/` 内引用**（保留历史）

**理由**:
- `.rddf/` 不在 `docs_sync_check.sh` 默认扫描范围（脚本只扫 `AGENTS.md`、`docs/ONBOARDING.md`、`roadmap.md`、`scripts/README.md`）
- `openspec validate` 不检查 `.rddf/` 内路径引用
- `.rddf/` 是 RDDF workflow 内部状态文件，迁移完成后运行 `rddf doctor` 即可重新生成
- **关键验证**: `git rm` 后执行 `openspec validate --changes --strict` 应仍 10/10 PASS

**若验证失败**: 才考虑在 `.rddf/plans/*.md` 中加迁移注记或使用 VIRTUAL_PATHS 局部扩展。

### 3.4 AGENTS.md 新增约束

在 `ANTI-PATTERNS` 节新增：

```markdown
### ❌ docs/soc_arch 直接修改
**理由**: dGPU SoC 设计文档已迁至独立 ArchForge 仓（`/workspace/project/ArchForge/`）。CppTLM 中的 `docs/soc_arch/` 仅为占位 README。
**修改流向**: 设计变更 → ArchForge 仓 PR；实现变更 → CppTLM 仓 PR。
**已知豁免**: `VIRTUAL_PATHS` 包含 `docs/soc_arch/*` 前缀，允许跨仓引用继续工作。
```

### 3.5 验证

```bash
./scripts/test/docs_sync_check.sh --strict   # 期望 PASS
openspec validate --changes --strict          # 期望 10/10 PASS（关键验证 .rddf/ 决策）
./scripts/test/docs_sync_check.sh --strict 2>&1 | grep -c "docs/soc_arch"   # 期望: 0 错误行
```

### 3.6 回滚方案

```bash
git revert <step3-sha>
```

### 3.7 时间估算

`VIRTUAL_PATHS` 扩展 30min + 核心文档引用修复 45min + AGENTS.md 更新 15min + 验证 15min ≈ **1.75 小时**。

---

## §4 Step 4: 活跃 OpenSpec Change 跨仓适配

**目标**: 确保 3 个活跃 openspec changes 在 CppTLM 中仍可引用 `docs/soc_arch/` 路径。

### 4.1 活跃 changes 清单

| Change | Paths affected | 策略 |
|--------|---------------|------|
| `2026-09-19-cpptlm-dgpu-gmmu-mvp/` | `docs/soc_arch/architecture/20-gmmu-mvp.md` | VIRTUAL_PATHS（不修改文件内路径） |
| `2026-09-19-cpptlm-mas-soc-topology-mvp/` | `docs/soc_arch/architecture/21-soc-topology-mvp.md` | VIRTUAL_PATHS |
| `2026-09-19-cpptlm-nsa-scale-up-umbrella/` | `docs/soc_arch/architecture/22-*`–`31-*`, ADR-SOC-22/23/24 | VIRTUAL_PATHS |

### 4.2 操作

不修改 openspec change 文件内容。仅确保 `docs_sync_check.sh` 的 `VIRTUAL_PATHS` 覆盖这些 change 中引用的所有路径。

### 4.3 验证

```bash
# 扫描活跃 changes 中所有反引号路径
grep -rn 'docs/soc_arch' openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/ | wc -l
grep -rn 'docs/soc_arch' openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/ | wc -l
grep -rn 'docs/soc_arch' openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/ | wc -l

# 确认每条路径在 VIRTUAL_PATHS 中都有匹配条目
./scripts/test/docs_sync_check.sh --strict   # 期望 PASS
```

### 4.4 时间估算

路径扫描 + 白名单确认 30min + 验证 15min ≈ **45 分钟**。

---

## §5 Step 5: 跨仓联邦协议 + CI 适配

**目标**: 建立 CppTLM 与 ArchForge 之间的最小跨仓协作规范。

### 5.1 `CROSS_REPO.md` 在 CppTLM 中

在 CppTLM 根目录创建：

```markdown
# CppTLM ↔ ArchForge 跨仓联邦

## 关系
- **CppTLM**（本仓）: TLM 仿真框架 + PCIe EP + GPGPU 实现
- **ArchForge**（设计仓, `/workspace/project/ArchForge/`）: dGPU SoC 设计文档

## 引用规则
1. CppTLM 中的 `docs/` 可引用 ArchForge 路径（通过 VIRTUAL_PATHS）
2. [ ] 实现变更影响设计 → 更新 ArchForge 仓后再提交 CppTLM
3. [ ] 设计变更牵涉实现 → 在 CppTLM 中创建 openspec change

## 已迁移路径
- `docs/soc_arch/` → `ArchForge/docs/soc_arch/`
```

### 5.2 CI 适配

```bash
# docs_sync_check.sh 已扩展 VIRTUAL_PATHS（Step 3），无需额外修改
# 如需检查 ArchForge 存在性，可增加：
if [ ! -d /workspace/project/ArchForge/docs/soc_arch ]; then
  echo "WARNING: ArchForge design repo not cloned at /workspace/project/ArchForge/"
  echo "Design docs from docs/soc_arch/ are unavailable locally."
fi
```

### 5.3 时间估算

CROSS_REPO.md 创建 15min + CI 适配 15min ≈ **30 分钟**。

---

## §6 总览时间表

| Step | 内容 | 估时 | 风险 | 划分 |
|------|------|------|------|------|
| 0 | 前置准备（快照、分支、基线） | 1h | 🟢 | 批次 A |
| 1 | 创建 ArchForge 项目骨架（幂等） | 0.5h | 🟢 | 批次 A |
| 2 | 迁移 dGPU SoC 设计文档（~132 份） | 2h | 🟢 | 批次 A |
| 3 | CppTLM 内部交叉引用修复 + VIRTUAL_PATHS | 1.75h | 🟡 | 批次 B |
| 4 | OpenSpec changes 跨仓适配 | 0.25h | 🟢 | 批次 B |
| 5 | 跨仓联邦协议 + CI 适配 | 0.5h | 🟢 | 批次 C |
| **合计** | | **~6h** | | **3 批次** |

> **修正**：原 v2.0 估算 5.75h，经 Oracle + Metis 双审查，Step 0（23 项脏工作区处理）+ Step 3（22+ 处引用修复）被低估，整体上调至 ~6h。

与原始 B+ 计划相比：
- 共省去了 Step 1 MVP 迁移（2.5h）和 Step 2 CMake 双库（3.5-4.5h）—— 因为仅迁移设计文档（非代码）不需要这些
- 新增了 ArchForge 创建（0.5h）+ 跨仓适配（1.75h）
- **净减 5-7 小时**，这是选择"先迁设计文档"路线的效率收益

### 验证节奏

- **批次 A 后**: `./build/bin/cpptlm_tests --reporter compact | tail -3`（构建不受影响）
- **批次 B 后**: `./scripts/test/docs_sync_check.sh --strict`
- **批次 C 后**: `openspec validate --changes --strict` + `./test.sh --mode off --quick`

---

## §7 CppTLM 内部代码硬化（后续批次，可并行）

以下两项是 CppTLM 仓内的代码级清理，与 ArchForge 拆仓正交。建议在批次 A-C 完成后、作为独立 PR 并行执行。

| 步骤 | 内容 | 估时 | 风险 | 说明 |
|------|------|------|------|------|
| 代码 Step A | MVP 组件迁移（`tlm/gpu/` → `tlm/pcie/` + shim） | 2.5h | 🟢 | 消除反向 include |
| 代码 Step B | CMake 双库拆分（`cpptlm_pcie` + `cpptlm_dgpu`） | 3.5-4.5h | 🟡 ODR | 独立于文档迁移 |

这两步在打完 "预置" commit 后即可在执行批次 B/C 的同一 session 中以背景任务并行推进。

---

## §8 风险登记册

| 风险 | Step | 等级 | 缓解策略 |
|------|------|------|----------|
| **ArchForge 路径硬编码** `/workspace/project/ArchForge/`（README、占位、CROSS_REPO、CI） | 1/5 | 🟡 | 改为相对路径 `../ArchForge/` 或环境变量 `ARCHFORGE_ROOT`；CI 中通过环境注入 |
| **Step 1 在已存在 ArchForge 上 `git init` 报错** | 1 | 🟡 | §1.2 改为幂等检查脚本（`if [ -d .git ]; then skip; fi`） |
| **VIRTUAL_PATHS 通配符 `*` 在子串匹配中无效** | 3 | 🟡 | §3.1 改为纯前缀条目（去 `*`）；说明匹配逻辑 |
| Copy 不完整或差异未发现 | 2 | 🟢 | `diff -rq` 对比验证 + 关键文档抽样检查 |
| AGENTS.md 引用的 `docs/soc_arch/` 路径大量断裂（22+ 处而非 6 处） | 3 | 🟡 | §3.2 改为三级策略：P0 必改 / P1 注记 / P2 注释 |
| `.rddf/` 内 26 处 `docs/soc_arch` 引用断裂 | 3 | 🟡 | §3.3 决策不修复，验证 `openspec validate --strict` 仍 PASS |
| 活跃 openspec change 路径引用断裂（实际不在检查范围内） | 4 | 🟢 | `openspec validate` 不检查文档链接；无需动作 |
| 脏工作区致 git rm 失败/丢历史 | 0 | 🟡 | §0.1 先一次性 commit 所有在途变更 |
| **`BREAKING:` 提交前缀被 CI 误判为 major bump** | 2 | 🟢 | §2.5 已改用 `refactor(docs):` 标准格式 |
| 团队对跨仓工作流不适应 | 5 | 🟡 | 最简化联邦协议（CROSS_REPO.md），不强求同步 |
| ArchForge 仓无人维护（孤仓） | 5 | 🟡 | CppTLM 开发者仍需修改 `docs/soc_arch/` 时 → CppTLM 内留快速链接 |
| ArchForge 侧无 markdown 链接检查 CI | 5 | 🟢 | 后续在 ArchForge 加 `lychee` 或类似轻量检查 |
| 回归基线变化（44498 非全绿） | 2 | 🟢 | 仅操作 docs/，构建不受影响；非本文档导致的问题分清后再报告 |
| 时间估算偏乐观（计划 5.75h，实际 ~7h） | 全部 | 🟢 | 实际执行时按步骤展开后再细化 |

---

## §9 建议 Commit 策略

| 序号 | 批次 | Commit message | 内容 |
|------|------|---------------|------|
| 1 | A | `chore(snapshot): pre-archforge in-flight NSA + MVP docs + ADR-22/23/24` | 脏工作区落地 |
| 2 | A | `feat(archforge): init design repo scaffold at /workspace/project/ArchForge/` | ArchForge 骨架 |
| 3 | A | `refactor(docs): migrate dGPU SoC design docs (~132 files) to ArchForge` | 全迁移 |
| 4 | B | `docs(cross-ref): fix soc_arch references + extend VIRTUAL_PATHS` | 引用修复 |
| 5 | B | `chore(openspec): verify active changes compatibility with ArchForge split` | Change 适配 |
| 6 | C | `docs(federation): add CROSS_REPO.md + ArchForge CI check` | 联邦协议 |

---

## §10 C 拆分后续触发条件（ArchForge 扩张评估）

ArchForge 目前仅为纯设计仓。以下条件触发时，可考虑将更多内容移入：

| 条件 | 说明 |
|------|------|
| ArchForge 获独立团队 | 设计团队与实现团队分属不同贡献者 |
| 新 SoC 变体加入 | 多代 dGPU SoC 架构版本共存 → 设计仓成长 |
| 跨仓引用 > 10/季度 | 设计→实现的交叉引用进入高频 → 正式联邦 |
| 第三方引用 ArchForge | 外部单位引用设计文档（非 CppTLM 代码） |

---

## 维护记录

| 日期 | 版本 | 修订说明 |
|------|------|----------|
| 2026-09-20 | v1.0 | B+ 边界硬化阶段 0 首版（4 步，3 PR，10-12h） |
| 2026-09-20 | v2.0 | 策略变更：B+ → C-archforge，创建独立设计仓迁移全部 dGPU SoC 设计文档（~132 份） |
| 2026-09-20 | v2.1 | Oracle + Metis 双审查修复：Step 1 幂等化（ArchForge 已存在）/ VIRTUAL_PATHS 通配符修正（去 `*`）/ 引用修复清单扩展为 P0/P1/P2 三级 / `.rddf/` 决策明确 / `BREAKING:` 改为 `refactor(docs):` / 风险登记册增 6 项 |