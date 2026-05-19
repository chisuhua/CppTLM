# CppTLM 剩余工作清单

> **生成日期**: 2026-05-19
> **基于**: `plans/debt-remediation-plan.md`, `plans/implementation_plan_v2.1.md`, `plans/phase7-completion-plan.md`

---

## 一、已完成项目

### P0 债务 (全部完成)

| 项目 | 状态 | Commit |
|------|------|--------|
| P0.1 PortPair 内存泄漏修复 | ✅ | `fb94bc9` |
| P0.2 删除 noc_builder.py, noc_mesh.py | ✅ | `7841631` |
| P0.3 wildcard.hh 空 catch 块修复 | ✅ | `8f71654` |

### P1 债务 (部分完成)

| 项目 | 状态 | Commit |
|------|------|--------|
| P1.1 routing_algo_ 裸指针 → unique_ptr | ✅ | `c78f7fe` |
| P1.3 src_port 传播修复 | ✅ | `fdc7375` |

### Dashboard 功能 (全部完成)

| 项目 | 状态 | Commit |
|------|------|--------|
| FastAPI 服务器 + SSE 实时推送 | ✅ | `ee3180f` |
| 静态 HTML 文件提取 | ✅ | `4977724` |
| Svelte 拓扑编辑器 | ✅ | `fe983bd` |
| .gitignore 更新 | ✅ | `09614ce` |

---

## 二、剩余工作

### P1 债务（继续）

#### P1.2: connection_resolver.cc latency 注入 ⏳

**文件**: `src/core/connection_resolver.cc:44`

**问题**: `latency` 参数被 `(void)` 压制，JSON 配置中的连接延迟完全失效。

**影响**: 仿真精度受影响，连接延迟配置无效。

**TDD 步骤**:

1. **RED**: 编写测试——连接配置中设置 `"latency": 5`，断言端口延迟等于 5
2. **GREEN**: 修改代码，将 latency 传播到 createPortFunc lambda
3. **REFACTOR**: 确认默认行为不变（latency=0 时现有测试仍通过）

**预估工时**: 4-6 小时

**验证**:
```bash
./build/bin/cpptlm_tests "[connection]" --output-on-failure
```

---

#### P1.4: 清理冗余配置文件 ⏳

**文件**: `configs/` 下的重复文件

**需删除** (保留 `_tlm` 版本):

| 应删除 | 保留 |
|--------|------|
| `configs/mesh_2x2.json` | `configs/mesh_2x2_tlm.json` |
| `configs/hierarchical_2x2.json` | `configs/hierarchical_2x2_tlm.json` |
| `configs/ring_8.json` | `configs/ring_8_tlm.json` |

**注意**: v2.1 架构中将去 `_tlm` 后缀化

**预估工时**: 1-2 小时

**验证**:
```bash
./build/bin/cpptlm_tests --output-on-failure
```

---

### P2 债务

#### P2.A: 合并 linter 到 validator ⏳

**文件**: `scripts/linter.py` → `cpptlm_config/validator.py`

**迁移检查项**:
- `W001`: 自环检测（`src == dst`）
- `W002`: 重复连接检测

**步骤**:
1. 在 `validator.py` 中添加 W001/W002 检查
2. 验证 `python scripts/topology_validator.py configs/mesh_2x2_tlm.json` 输出不变
3. 删除 `scripts/linter.py`

**预估工时**: 2-3 小时

---

#### P2.B: C++ 风格现代化 ⏳

**范围**: `include/utils/mem_exts.hh` 等文件

**内容**: `typedef` → `using` 别名

**预估工时**: 1-2 小时

---

#### P2.C: Legacy modules 审计 ⏳

**文件**: `include/modules/legacy/`

**内容**:
- 审计 Legacy 模块使用情况
- 添加 deprecation 标记
- 文档说明新代码应使用 `include/tlm/`

**预估工时**: 1-2 小时

---

#### P2.D: 日志一致性修复 ⏳

**内容**:
- 统一 DPRINTF 使用格式
- 修复不一致的日志级别

**预估工时**: 1-2 小时

---

### 其他工作

#### TODO 注释清理 ⏳

**文件**: `src/traffic_main.cpp`

**内容**:
```cpp
// TODO: v2.1 架构升级后实现流量生成模块
// TODO: v2.1 架构升级后启用
```

**说明**: 流量生成模块 (traffic_gen_tlm) 相关，待 v2.1 架构决策

---

## 三、工时汇总

| 类别 | 工时 |
|------|------|
| P1.2 (latency 注入) | 4-6 小时 |
| P1.4 (配置文件清理) | 1-2 小时 |
| P2.A (linter 合并) | 2-3 小时 |
| P2.B-D | 4-6 小时 |
| TODO 清理 | 1 小时 |

**总计**: 约 12-17 小时

---

## 四、执行建议

### 推荐顺序

```
P1.2 (latency)      → 最关键，影响仿真精度
P1.4 (configs)      → 简单清理
P2.A (linter)       → 可并行
P2.B-D              → 可并行
```

### 验证命令

```bash
# 编译
cmake --build build -j$(nproc)

# 全量测试
./build/bin/cpptlm_tests --output-on-failure

# Python 测试
python -m pytest test/python/ -v
```

---

## 五、已提交 Commit 记录

```
09614ce chore: update .gitignore for dashboard artifacts and planning files
fe983bd feat(editor): add Svelte-based topology editor
4977724 refactor(dashboard): extract static HTML files and template loader
ee3180f feat(dashboard): add FastAPI server with SSE real-time streaming
0dca50c fix(cli): implement --generate-only flag and improve dashboard server
7841631 chore(python): remove dead noc_builder.py and noc_mesh.py
8f71654 fix(wildcard): log regex errors instead of swallowing
c78f7fe refactor(router): routing_algo_ to unique_ptr
fb94bc9 fix(memory): use unique_ptr for PortPair in module_factory
fdc7375 fix(link): propagate src_port from flit bundle
```