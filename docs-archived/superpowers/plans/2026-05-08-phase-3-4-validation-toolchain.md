# Phase 3.4 Implementation Plan — Validation Toolchain

> **版本**: 1.0
> **日期**: 2026-05-08
> **状态**: 📋 路线图（依赖 cpptlm_config 完成，Phase 3.2 Python 端实施后启动）
> **关联**: TGMS Phase 3.4, ADR-X.9 (端口类型系统), ADR-X.10 (参数框架), ADR-X.12 (Python 配置生成器)

---

## 术语

| 术语 | 定义 |
|------|------|
| **Phase 3.4** | 拓扑验证阶段（Validation Toolchain），与 ADR-X.11 一致的阶段编号 |
| **cpptlm_config** | Python 配置生成器包（ADR-X.12），位于项目根 `cpptlm_config/` |
| **TopologyAdapter** | 连接 `topology_generator.py` 和 `ConfigBuilder` 的适配器 |
| **两阶段验证** | Python 侧 ConfigBuilder.build() 基础验证 + C++ 侧 ModuleFactory 完整验证 |

**阶段编号说明**（与 ADR-X.9/11 一致）：
- **Phase 3+**：Phase 3.1-3.4 的统称
- **Phase 3.2**：端口管理阶段（已完成 cpptlm_config 包创建）
- **Phase 3.3**：配置增强阶段（已实施 ParamRule JSON + ParamParser + param_errors）
- **Phase 3.4**：拓扑验证阶段（本 plan 的实施阶段）

---

## 上下文

### 前置条件

Phase 3.4 依赖 cpptlm_config Python 包（Phase 3.2 Python 端实施）必须完成。

**已完成（2026-05-07）**：
- `cpptlm_config/` 包结构（`__init__.py`, `types.py`, `models.py`, `pyproject.toml`）
- Pydantic v2 模型：PortSpec, PortGroupSpec, ModulePortSpec, ConfigMetadata, ConfigSchema
- 端口类型枚举：RouterPort, NICPort, PortRole, BundleType, PortGroupBundleType
- SemVer 版本管理（ConfigMetadata.bump_version）
- layout_hint 字段（PortSpec.layout_hint）
- Python 单元测试（`test/python/test_port_types.py`，15+ 测试用例）

**待完成（Phase 3.4 启动前）**：
- pydantic 安装验证（`pip install -e cpptlm_config/`）
- `test/python/test_port_types.py` 运行通过

### ADR-X.12 决策 7 回顾

ADR-X.12 决策 7 指定：

> **拓扑验证器整合为 cpptlm_config.validator 模块**，两阶段验证：
> 1. Python 侧：ConfigBuilder.build() 时自动调用基础验证
> 2. C++ 侧：ModuleFactory 实例化前调用完整验证

### ADR-X.12 决策 3 回顾

ADR-X.12 决策 3 指定：

> **封装包装** topology_generator → TopologyAdapter 复用现有拓扑生成逻辑

---

## 目标

将 `scripts/topology_validator.py` 重构为 `cpptlm_config/validator.py`，实现：
1. **两阶段验证**：Python 侧（ConfigBuilder.build() 时）+ C++ 侧（ModuleFactory 实例化前）
2. **TopologyAdapter**：复用 `topology_generator.py` 逻辑，集成到 ConfigBuilder
3. **示例脚本**：3 个端到端示例（mesh_2x2, mesh_4x4_validated, hierarchical）
4. **C++ 集成**：Python 验证结果反馈到 C++ 配置流程

---

## 实施计划

### Step 1: 创建 cpptlm_config/validator.py

**文件**：新建 `cpptlm_config/validator.py`

**内容**：
```python
# cpptlm_config/validator.py
"""拓扑验证器 - cpptlm_config validator module

两阶段验证:
1. Python 侧基础验证 (ValidationStage.PYTHON)
2. C++ 侧完整验证 (ValidationStage.CPP)

参考: scripts/topology_validator.py 重构 + ADR-X.12 决策 7
"""

from pydantic import BaseModel
from typing import List, Optional, Set, Dict
from collections import defaultdict, deque
import json

class ValidationIssue(BaseModel):
    """验证问题"""
    severity: str  # "error", "warning", "info"
    code: str      # "VALID-01", "PORT-01", etc.
    message: str
    suggestion: Optional[str] = None

class ValidationResult(BaseModel):
    """验证结果"""
    is_valid: bool = True
    errors: List[ValidationIssue] = []
    warnings: List[ValidationIssue] = []

    def add_error(self, code: str, message: str, suggestion: Optional[str] = None):
        self.errors.append(ValidationIssue(
            severity="error", code=code, message=message, suggestion=suggestion
        ))
        self.is_valid = False

    def add_warning(self, code: str, message: str, suggestion: Optional[str] = None):
        self.warnings.append(ValidationIssue(
            severity="warning", code=code, message=message, suggestion=suggestion
        ))

class TopologyValidator:
    """拓扑验证器 - 重构自 scripts/topology_validator.py"""

    ROUTER_PORT_DIR = {0: 'NORTH', 1: 'EAST', 2: 'SOUTH', 3: 'WEST', 4: 'LOCAL'}

    def __init__(self, config_data: Dict):
        self.config = config_data
        self.result = ValidationResult()

    # === VALID-01: Module connectivity ===
    def validate_connectivity(self) -> "TopologyValidator":
        """所有连接的模块必须在 modules 中定义"""
        module_names = {m['name'] for m in self.config.get('modules', [])}
        undefined_srcs: Set[str] = set()
        undefined_dsts: Set[str] = set()

        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and src not in module_names:
                undefined_srcs.add(src)
            if dst and dst not in module_names:
                undefined_dsts.add(dst)

        if undefined_srcs or undefined_dsts:
            self.result.add_error(
                code="VALID-01",
                message=f"Undefined modules in connections: {sorted(undefined_srcs | undefined_dsts)}",
                suggestion="Add these modules to the modules list or fix connection src/dst"
            )
        return self

    # === VALID-02: BFS reachability ===
    def validate_reachability(self) -> "TopologyValidator":
        """从任一终端可到达所有其他终端"""
        adj = defaultdict(list)
        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and dst:
                adj[src].append(dst)

        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}
        terminals = [n for n, t in module_types.items() if 'Processor' in t or 'NICTLM' in t or 'CPU' in t]

        if len(terminals) < 2:
            return self

        start = terminals[0]
        visited: Set[str] = set()
        queue = deque([start])

        while queue:
            node = queue.popleft()
            if node in visited:
                continue
            visited.add(node)
            for neighbor in adj[node]:
                if neighbor not in visited:
                    queue.append(neighbor)

        unreachable = [t for t in terminals if t not in visited]
        if unreachable:
            self.result.add_error(
                code="VALID-02",
                message=f"Unreachable terminals from {start}: {unreachable}",
                suggestion="Check connectivity graph"
            )
        return self

    # === PORT-01: Router port directions ===
    def validate_port_directions(self) -> "TopologyValidator":
        """验证 Router 端口方向与物理布局一致（仅当能解析坐标时）"""
        import re
        def parse_router_coords(name: str):
            m = re.match(r'router_(\d+)_(\d+)', name)
            return (int(m.group(1)), int(m.group(2))) if m else None

        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}

        for conn in self.config.get('connections', []):
            src, dst = conn.get('src', ''), conn.get('dst', '')
            src_mod, src_port = src.split('.')[0], src.split('.')[1] if '.' in src else None
            dst_mod, dst_port = dst.split('.')[0], dst.split('.')[1] if '.' in dst else None

            src_type = module_types.get(src_mod, '')
            dst_type = module_types.get(dst_mod, '')

            if 'Router' not in src_type or 'Router' not in dst_type:
                continue
            if not src_port or not dst_port:
                continue

            try:
                sp, dp = int(src_port), int(dst_port)
            except ValueError:
                continue

            src_coords = parse_router_coords(src_mod)
            dst_coords = parse_router_coords(dst_mod)
            if not src_coords or not dst_coords:
                continue

            sx, sy = src_coords
            dx, dy = dst_coords
            expected: Optional[tuple] = None

            if dx == sx + 1 and dy == sy:
                expected = (1, 3)  # EAST(1) -> WEST(3)
            elif dx == sx - 1 and dy == sy:
                expected = (3, 1)  # WEST(3) -> EAST(1)
            elif dx == sx and dy == sy + 1:
                expected = (2, 0)  # SOUTH(2) -> NORTH(0)
            elif dx == sx and dy == sy - 1:
                expected = (0, 2)  # NORTH(0) -> SOUTH(2)

            if expected and (sp, dp) != expected:
                self.result.add_warning(
                    code="PORT-01",
                    message=f"{src_mod}.{sp}({self.ROUTER_PORT_DIR.get(sp, '?')}) -> "
                            f"{dst_mod}.{dp}({self.ROUTER_PORT_DIR.get(dp, '?')}): "
                            f"expected port pair {expected}",
                    suggestion=f"For {src_mod}->{dst_mod}, use port pair {expected}"
                )
        return self

    # === PORT-03: Bundle type compatibility ===
    def validate_bundle_types(self) -> "TopologyValidator":
        """验证 Router-Local(4) ↔ NI-Network(1) 连接"""
        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}

        for conn in self.config.get('connections', []):
            src, dst = conn.get('src', ''), conn.get('dst', '')
            src_mod, src_port = src.split('.')[0], src.split('.')[1] if '.' in src else None
            dst_mod, dst_port = dst.split('.')[0], dst.split('.')[1] if '.' in dst else None

            src_type = module_types.get(src_mod, '')
            dst_type = module_types.get(dst_mod, '')

            # Router-Local(4) must connect to NI-Network(1)
            if 'Router' in src_type and 'NetworkInterface' in dst_type:
                if src_port and dst_port:
                    try:
                        if not (int(src_port) == 4 and int(dst_port) == 1):
                            self.result.add_error(
                                code="PORT-03",
                                message=f"Router-Local(4) must connect to NI-Network(1), "
                                        f"got {src_mod}.{src_port} -> {dst_mod}.{dst_port}",
                                suggestion=f"Use port 4 on Router and port 1 on NICTLM"
                            )
                    except ValueError:
                        pass

            # NI-Network(1) must connect to Router-Local(4)
            if 'NetworkInterface' in src_type and 'Router' in dst_type:
                if src_port and dst_port:
                    try:
                        if not (int(src_port) == 1 and int(dst_port) == 4):
                            self.result.add_error(
                                code="PORT-03",
                                message=f"NI-Network(1) must connect to Router-Local(4), "
                                        f"got {src_mod}.{src_port} -> {dst_mod}.{dst_port}",
                                suggestion=f"Use port 1 on NICTLM and port 4 on Router"
                            )
                    except ValueError:
                        pass
        return self

    def validate(self) -> ValidationResult:
        """执行所有验证"""
        self.validate_connectivity()
        self.validate_reachability()
        self.validate_port_directions()
        self.validate_bundle_types()
        return self.result

    @staticmethod
    def from_json_file(path: str) -> "TopologyValidator":
        """从 JSON 文件加载配置"""
        with open(path, 'r', encoding='utf-8') as f:
            content = f.read()
        # Strip // comments for JSON compatibility
        lines = [l for l in content.split('\n') if not l.strip().startswith('//')]
        data = json.loads('\n'.join(lines))
        return TopologyValidator(data)
```

---

### Step 2: 创建 cpptlm_config/topology_adapter.py

**文件**：新建 `cpptlm_config/topology_adapter.py`

**内容**：
```python
# cpptlm_config/topology_adapter.py
"""TopologyAdapter - 连接 topology_generator.py 和 ConfigBuilder

参考: ADR-X.12 决策 3 (封装包装)

用法:
    from cpptlm_config import ConfigBuilder, TopologyAdapter

    builder = TopologyAdapter.from_mesh(
        rows=4, cols=4,
        builder=ConfigBuilder(name="mesh_4x4")
    )
    config = builder.build()
    config.save("configs/mesh_4x4.json")
"""

import sys
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from cpptlm_config.builder import ConfigBuilder

try:
    from scripts.topology_generator import TopologyGenerator
except ImportError:
    TopologyGenerator = None

class TopologyAdapter:
    """将 TopologyGenerator 的输出适配到 ConfigBuilder"""

    @staticmethod
    def from_mesh(rows: int, cols: int, builder: "ConfigBuilder") -> "ConfigBuilder":
        """从 Mesh 拓扑生成配置"""
        if TopologyGenerator is None:
            raise ImportError(
                "topology_generator.py not found. "
                "Ensure scripts/topology_generator.py is in the search path."
            )

        gen = TopologyGenerator(name=f"mesh_{rows}x{cols}")
        gen.add_mesh(rows, cols)

        from cpptlm_config.models import ModuleSpec, ConnectionSpec
        from cpptlm_config.types import ModuleType

        # 将拓扑图转换为模块
        for node, attrs in gen.graph.nodes(data=True):
            module_type_str = attrs.get("type", "")
            # Map abstract type to CppTLM type
            cpp_type = TopologyGenerator.CPPTLM_TYPE_MAP.get(module_type_str, module_type_str)
            try:
                mod_type = ModuleType(cpp_type)
            except ValueError:
                mod_type = ModuleType.ROUTER_TLM  # default fallback

            builder.add_module(ModuleSpec(
                name=node,
                type=mod_type,
                params=attrs.get("params", {})
            ))

        # 将拓扑图转换为连接
        for src, dst, attrs in gen.graph.edges(data=True):
            builder.add_connection(ConnectionSpec(
                src=src,
                dst=dst,
                latency=attrs.get("latency", 0),
                bandwidth=attrs.get("bandwidth", None)
            ))

        return builder
```

---

### Step 3: 更新 cpptlm_config/__init__.py 导出 validator 和 TopologyAdapter

**文件**：修改 `cpptlm_config/__init__.py`

添加导出：
```python
from .validator import TopologyValidator, ValidationResult, ValidationIssue
from .topology_adapter import TopologyAdapter
```

---

### Step 4: 创建示例脚本

#### 4a: cpptlm_config/examples/mesh_2x2.py

**文件**：新建 `cpptlm_config/examples/mesh_2x2.py`

```python
#!/usr/bin/env python3
"""生成 2x2 Mesh NoC 配置（简单示例）"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config import ConfigBuilder, TopologyAdapter

def main():
    builder = TopologyAdapter.from_mesh(
        rows=2, cols=2,
        builder=ConfigBuilder(
            name="mesh_2x2",
            description="2x2 Mesh NoC with TLM modules"
        )
    )

    config = builder.build()
    config.save("configs/mesh_2x2.json")
    print(f"Generated: configs/mesh_2x2.json")

if __name__ == "__main__":
    main()
```

#### 4b: cpptlm_config/examples/mesh_4x4_validated.py

**文件**：新建 `cpptlm_config/examples/mesh_4x4_validated.py`

```python
#!/usr/bin/env python3
"""生成 4x4 Mesh NoC 配置（带两阶段验证）"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config import ConfigBuilder, TopologyAdapter, TopologyValidator

def main():
    builder = TopologyAdapter.from_mesh(
        rows=4, cols=4,
        builder=ConfigBuilder(
            name="mesh_4x4_full",
            description="4x4 Mesh NoC with full validation"
        )
    )

    config = builder.build()

    # Python 侧验证
    validator = TopologyValidator(config.to_json_dict())
    result = validator.validate()

    if not result.is_valid:
        print("Validation FAILED:")
        for issue in result.errors:
            print(f"  [{issue.code}] {issue.message}")
            if issue.suggestion:
                print(f"    Suggestion: {issue.suggestion}")
        sys.exit(1)

    for issue in result.warnings:
        print(f"  WARNING [{issue.code}]: {issue.message}")

    config.save("configs/mesh_4x4_full.json")
    print(f"Generated and validated: configs/mesh_4x4_full.json")

if __name__ == "__main__":
    main()
```

#### 4c: cpptlm_config/examples/hierarchical.py

**文件**：新建 `cpptlm_config/examples/hierarchical.py`

```python
#!/usr/bin/env python3
"""继承配置示例"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config import ConfigBuilder, ModuleSpec, ModuleType

def main():
    # 继承基础配置，只覆盖需要修改的参数
    builder = ConfigBuilder(
        name="mesh_4x4_high_freq",
        description="4x4 Mesh with higher CPU frequency"
    )
    builder.set_extends("configs/mesh_4x4_full.json")

    builder.add_module(ModuleSpec(
        name="cpu_0_0",
        type=ModuleType.CPU_TLM,
        params={"frequency": 2000}
    ))

    config = builder.build()
    config.save("configs/mesh_4x4_high_freq.json")
    print(f"Generated override config: configs/mesh_4x4_high_freq.json")

if __name__ == "__main__":
    main()
```

---

### Step 5: 更新 scripts/topology_validator.py 为轻量包装

**文件**：修改 `scripts/topology_validator.py`

将其改为对 `cpptlm_config.validator.TopologyValidator` 的轻量包装：

```python
#!/usr/bin/env python3
"""CppTLM Topology Validator - Task 3.4

本文件现在是 cpptlm_config.validator.TopologyValidator 的轻量包装。
新增验证逻辑应添加到 cpptlm_config/validator.py。

用法:
    python3 scripts/topology_validator.py configs/mesh_2x2.json
    python3 scripts/topology_validator.py configs/mesh_2x2.json -v
"""

import sys
import os

# Add project root to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config.validator import TopologyValidator

def main():
    import argparse
    parser = argparse.ArgumentParser(description='CppTLM Topology Validator')
    parser.add_argument('config', nargs='?', default='configs/mesh_2x2.json')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    validator = TopologyValidator.from_json_file(args.config)
    result = validator.validate()

    print(f"\n[TopologyValidator] {args.config}")
    print("=" * 50)

    # Print results
    all_passed = result.is_valid
    for issue in result.errors:
        print(f"  FAIL: [{issue.code}] {issue.message}")
        if issue.suggestion:
            print(f"    Suggestion: {issue.suggestion}")
    for issue in result.warnings:
        print(f"  WARN: [{issue.code}] {issue.message}")

    if all_passed:
        print("  ALL VALIDATIONS PASSED")
    else:
        print("  VALIDATION FAILED")

    print("=" * 50)
    sys.exit(0 if all_passed else 1)

if __name__ == "__main__":
    main()
```

---

### Step 6: 运行验证

```bash
# 1. 安装 cpptlm_config
pip install -e cpptlm_config/

# 2. 运行 Python 单元测试
python3 -m pytest test/python/test_port_types.py -v

# 3. 运行示例脚本
python3 cpptlm_config/examples/mesh_2x2.py
python3 cpptlm_config/examples/mesh_4x4_validated.py

# 4. 运行 topology_validator.py
python3 scripts/topology_validator.py configs/mesh_2x2.json -v
```

---

## 任务汇总

| Step | 任务 | 文件 | 依赖 |
|------|------|------|------|
| 1 | 创建 validator.py | cpptlm_config/validator.py | cpptlm_config models |
| 2 | 创建 topology_adapter.py | cpptlm_config/topology_adapter.py | Step 1, topology_generator.py |
| 3 | 更新 __init__.py 导出 | cpptlm_config/__init__.py | Step 1, Step 2 |
| 4a | 创建 mesh_2x2.py 示例 | cpptlm_config/examples/mesh_2x2.py | Step 2 |
| 4b | 创建 mesh_4x4_validated.py 示例 | cpptlm_config/examples/mesh_4x4_validated.py | Step 1, Step 4a |
| 4c | 创建 hierarchical.py 示例 | cpptlm_config/examples/hierarchical.py | Step 4b |
| 5 | 重构 scripts/topology_validator.py | scripts/topology_validator.py | Step 1 |
| 6 | 运行验证 | - | Step 1-5 |

---

## 风险与权衡

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| topology_generator.py API 变更 | TopologyAdapter 可能失效 | 使用 try/import 兼容降级 |
| networkx/pydot 未安装 | TopologyAdapter 失效 | 检查导入并提示安装 |
| C++ 验证与 Python 验证规则不一致 | 两阶段验证产生不同结果 | 保持规则文档同步 |

---

## 验证标准

1. **Python 单元测试通过**: `test/python/test_port_types.py` 所有测试通过
2. **示例脚本成功**: `mesh_2x2.py` 和 `mesh_4x4_validated.py` 生成有效 JSON
3. **topology_validator.py 兼容**: 现有 CLI 接口不变，调用 cpptlm_config.validator
4. **向后兼容**: 生成的 JSON 格式与现有 C++ 解析器完全兼容
5. **文档完整**: 每个示例脚本包含 docstring 说明用途

---

## 下一步

Phase 3.4 完成后，项目进入 **Phase 4**（如适用）。

Phase 3.4 完成标准：
- [ ] cpptlm_config/validator.py 创建并包含 VALID-01/02, PORT-01/03
- [ ] cpptlm_config/topology_adapter.py 创建并可从 mesh 生成配置
- [ ] cpptlm_config/__init__.py 导出 validator 和 TopologyAdapter
- [ ] 3 个示例脚本可执行并生成有效 JSON
- [ ] scripts/topology_validator.py 重构为轻量包装
- [ ] pydantic 安装并通过单元测试

---

**变更记录**:

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-08 | 初始版本，定义 Phase 3.4 验证工具链实施计划 |
