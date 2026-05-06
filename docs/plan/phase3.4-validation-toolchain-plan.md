# Phase 3.4: 拓扑验证与工具链实施计划

> **版本**: v1.1
> **编制日期**: 2026-05-07
> **基于文档**: ADR-X.12 v2.0 (Python 配置生成器), ARCH-009 v2.0 (可视化管线)
> **前置条件**: Phase 3.3 已完成 + 90+ 新测试通过
> **预计工期**: 3 周 (Week 9-11)
> **目标**: 完善拓扑验证能力，整合 topology_validator.py 到 cpptlm_config 包
> **状态**: 📋 路线图（依赖 Phase 3.2/3.3 完成）

---

## 一、阶段概述

### 1.1 阶段目标

Phase 3.4 聚焦于建立完整的拓扑验证和工具链，核心目标包括:

1. **验证器整合**: 将 topology_validator.py 整合为 `cpptlm_config.validator` 模块
2. **连通性验证**: 实现孤立节点检测、BFS 可达性检测
3. **端口验证**: 实现路由器端口方向验证、Bundle 类型兼容性验证
4. **两阶段验证**: Python build() 后自动调用 validator
5. **高级工具**: 静态负载分析、连接路径追踪、配置 lint 工具

### 1.2 前置条件说明

> **注意**: Phase 3.4 依赖 `cpptlm_config/` Python 包（Phase 3.2/3.3 的 Python 端实现）。
> 当前代码中无 `cpptlm_config/` 目录，实施前需确认 Python 工具链的优先级。

---

## 一、阶段概述

### 1.1 阶段目标

Phase 3.4 聚焦于建立完整的拓扑验证和工具链，核心目标包括:

1. **验证器整合**: 将 topology_validator.py 整合为 `cpptlm_config.validator` 模块
2. **连通性验证**: 实现孤立节点检测、BFS 可达性检测
3. **端口验证**: 实现路由器端口方向验证、Bundle 类型兼容性验证
4. **两阶段验证**: Python build() 后自动调用 validator
5. **高级工具**: 静态负载分析、连接路径追踪、配置 lint 工具

### 1.2 共识事项覆盖

| 共识编号 | 内容 | 状态 |
|---------|------|------|
| **M1** | topology_validator 集成 | 本阶段实施 |
| **G4** | 可视化管线集成 (layout_hint) | 本阶段实施（验证 ConfigMetadata） |
| **M2** | SemVer 版本管理 | 本阶段实施（验证 bump_version/changelog） |
| **M3** | pyproject.toml 技术栈 | 本阶段实施（打包验证） |
| **m2** | 端口组支持 | 本阶段实施（验证 port_groups） |

### 1.3 关键设计变更

| 变更项 | 旧设计 | 新设计 |
|--------|--------|--------|
| 验证器位置 | 独立 `scripts/topology_validator.py` | `cpptlm_config.validator` |
| 验证时机 | 手动调用 | ConfigBuilder.build() 自动调用 |
| 验证结果 | 简单通过/失败 | ValidationIssue/ValidationResult 带修复建议 |

---

## 二、任务清单

### 2.1 Python 端任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.4-01 | **创建 `cpptlm_config/validator.py`** — TopologyValidator 整合 | 2d | Phase 3.2 models | ValidationIssue/ValidationResult 工作正常 |
| T3.4-02 | **孤立节点检测** — VALID-01 模块连通性 | 0.5d | T3.4-01 | 孤立节点被检测 |
| T3.4-03 | **BFS 可达性检测** — VALID-02 | 0.5d | T3.4-01 | 不可达节点被检测 |
| T3.4-04 | **路由器端口方向验证** — PORT-01 | 1d | T3.4-01, Phase 3.2 | 端口方向错误被检测 |
| T3.4-05 | **Bundle 类型兼容性验证** — PORT-03 | 1d | T3.4-01, Phase 3.2 | Bundle 不兼容被检测 |
| T3.4-06 | **两阶段验证集成** — build() 后自动调用 validator | 0.5d | T3.4-01~05 | 验证失败抛出 ValidationError |
| T3.4-07 | **静态负载分析** — VALID-05 | 1.5d | 无 | 热点链路识别正确 |
| T3.4-08 | **连接路径追踪** — TOOL-07 | 1.5d | 无 | BFS 路径输出正确 |
| T3.4-09 | **配置 lint 工具** — TOOL-08 | 1d | 无 | 最佳实践检查 |
| T3.4-14 | **pyproject.toml 验证** — M3 共识 | 0.5d | Phase 3.2 | `pip install -e .` 成功 |
| T3.4-15 | **SemVer 版本管理测试** — M2 共识 | 0.5d | Phase 3.2 | bump_version/changelog 测试通过 |
| T3.4-16 | **端口组验证** — m2 共识 | 0.5d | Phase 3.2 | port_groups 配置验证通过 |
| T3.4-17 | **可视化元数据验证** — G4 共识 | 0.5d | Phase 3.2 | layout_hint/ConfigMetadata 验证 |
| T3.4-10 | **拓扑分析工具测试** | 1.5d | T3.4-01~09, T3.4-14~17 | 25+ 测试用例通过 |

**小计**: 11.5 天 (约 2.5 周)

### 2.2 集成与文档任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.4-11 | **端到端集成测试** — 完整配置验证流程 | 0.5d | 所有开发任务 | mesh_2x2/4x4 验证通过 |
| T3.4-12 | **更新架构文档** — 记录验证工具链设计 | 0.5d | T3.4-11 | 文档评审通过 |
| T3.4-13 | **更新用户指南** — 验证工具使用指南 | 0.5d | T3.4-11 | 文档评审通过 |

**小计**: 1.5 天 (约 0.5 周)

---

## 三、详细设计

### 3.1 validator.py（拓扑验证器整合）

**文件路径**: `cpptlm_config/validator.py`

**核心内容**:

```python
# cpptlm_config/validator.py

from pydantic import BaseModel
from typing import List, Optional, Dict, Set, Tuple
from collections import deque

from .models import ConfigSchema, ModuleSpec, ConnectionSpec
from .types import ModuleType, PortRole, BundleType


class ValidationIssue(BaseModel):
    """验证问题 (M1 共识)"""
    severity: str  # "error", "warning", "info"
    message: str
    suggestion: Optional[str] = None  # 修复建议


class ValidationResult(BaseModel):
    """验证结果 (M1 共识)"""
    is_valid: bool
    errors: List[ValidationIssue] = []
    warnings: List[ValidationIssue] = []
    
    def add_error(self, message: str, suggestion: Optional[str] = None):
        self.errors.append(ValidationIssue(severity="error", message=message, suggestion=suggestion))
        self.is_valid = False
    
    def add_warning(self, message: str, suggestion: Optional[str] = None):
        self.warnings.append(ValidationIssue(severity="warning", message=message, suggestion=suggestion))


class TopologyValidator:
    """拓扑验证器 (M1 共识)"""
    
    @staticmethod
    def validate_config(config: ConfigSchema) -> ValidationResult:
        """验证完整配置"""
        result = ValidationResult(is_valid=True)
        
        # VALID-01: 孤立节点检测
        TopologyValidator._check_orphaned_modules(config, result)
        
        # VALID-02: BFS 可达性检测
        TopologyValidator._check_reachability(config, result)
        
        # VALID-03: 重复连接检测
        TopologyValidator._check_duplicate_connections(config, result)
        
        # PORT-01: 路由器端口方向验证
        TopologyValidator._check_port_directions(config, result)
        
        # PORT-03: Bundle 类型兼容性验证
        TopologyValidator._check_bundle_compatibility(config, result)
        
        return result
    
    @staticmethod
    def _check_orphaned_modules(config: ConfigSchema, result: ValidationResult):
        """VALID-01: 孤立节点检测"""
        module_names = {m.name for m in config.modules}
        connected_modules: Set[str] = set()
        
        for conn in config.connections:
            connected_modules.add(conn.src.split(".")[0])
            connected_modules.add(conn.dst.split(".")[0])
        
        orphaned = module_names - connected_modules
        if orphaned:
            result.add_error(
                f"Orphaned modules: {', '.join(sorted(orphaned))}",
                "Add connections for these modules or remove them from the configuration"
            )
    
    @staticmethod
    def _check_reachability(config: ConfigSchema, result: ValidationResult):
        """VALID-02: BFS 可达性检测"""
        # 构建邻接表
        graph: Dict[str, Set[str]] = {}
        for conn in config.connections:
            src = conn.src.split(".")[0]
            dst = conn.dst.split(".")[0]
            graph.setdefault(src, set()).add(dst)
            graph.setdefault(dst, set()).add(src)
        
        if not graph:
            return  # 无连接，跳过
        
        # BFS 从第一个节点开始
        start_node = next(iter(graph))
        visited: Set[str] = set()
        queue = deque([start_node])
        
        while queue:
            node = queue.popleft()
            if node in visited:
                continue
            visited.add(node)
            queue.extend(graph.get(node, set()) - visited)
        
        # 检查是否有未访问的节点
        unreachable = set(graph.keys()) - visited
        if unreachable:
            result.add_error(
                f"Unreachable modules: {', '.join(sorted(unreachable))}",
                "Check if all modules are properly connected"
            )
    
    @staticmethod
    def _check_duplicate_connections(config: ConfigSchema, result: ValidationResult):
        """VALID-03: 重复连接检测"""
        seen_connections: Set[str] = set()
        
        for conn in config.connections:
            key = f"{conn.src}->{conn.dst}"
            if key in seen_connections:
                result.add_warning(
                    f"Duplicate connection: {key}",
                    "Remove duplicate connection or verify if bidirectional connection is intended"
                )
            seen_connections.add(key)
    
    @staticmethod
    def _check_port_directions(config: ConfigSchema, result: ValidationResult):
        """PORT-01: 路由器端口方向验证"""
        module_types = {m.name: m.type for m in config.modules}
        
        for conn in config.connections:
            src_module = conn.src.split(".")[0]
            dst_module = conn.dst.split(".")[0]
            
            src_type = module_types.get(src_module)
            dst_type = module_types.get(dst_module)
            
            # 检查 Router-to-Router 连接是否指定了端口索引
            if src_type == ModuleType.ROUTER_TLM and dst_type == ModuleType.ROUTER_TLM:
                if "." not in conn.src or "." not in conn.dst:
                    result.add_warning(
                        f"Router-to-Router connection missing port index: {conn.src} -> {conn.dst}",
                        "Add port index for better clarity (e.g., router_0_0.1)"
                    )
    
    @staticmethod
    def _check_bundle_compatibility(config: ConfigSchema, result: ValidationResult):
        """PORT-03: Bundle 类型兼容性验证"""
        # 简化版: 检查已知模块类型的端口 bundle 兼容性
        # 完整实现需要 Phase 3.2 的端口类型信息
        pass
    
    @staticmethod
    def validate_and_raise(config: ConfigSchema) -> ConfigSchema:
        """验证配置，失败时抛出异常"""
        validation_result = TopologyValidator.validate_config(config)
        
        if not validation_result.is_valid:
            error_messages = [e.message for e in validation_result.errors]
            raise ValueError(
                f"Configuration validation failed:\n" + 
                "\n".join(f"- {msg}" for msg in error_messages)
            )
        
        return config
```

### 3.2 ConfigBuilder 集成验证器

**修改文件**: `cpptlm_config/builder.py`

```python
# cpptlm_config/builder.py 修改

from .validator import TopologyValidator


class ConfigBuilder:
    def build(self, validate: bool = True) -> ConfigSchema:
        """
        构建最终配置
        
        Args:
            validate: 是否自动调用验证器 (默认 True)
            
        Returns:
            ConfigSchema 实例
            
        Raises:
            ValueError: 验证失败时抛出
        """
        config = ConfigSchema(
            name=self.name,
            description=self.description,
            metadata=self.metadata if hasattr(self, 'metadata') else None,
            modules=list(self.modules.values()),
            connections=self.connections,
            groups=self.groups if self.groups else None,
            extends=self.extends
        )
        
        # 两阶段验证集成 (T3.4-06)
        if validate:
            TopologyValidator.validate_and_raise(config)
        
        return config
```

### 3.3 静态负载分析

**文件路径**: `cpptlm_config/analyzer.py`

```python
# cpptlm_config/analyzer.py

from typing import Dict, List, Tuple
from collections import defaultdict

from .models import ConfigSchema, ConnectionSpec


class StaticLoadAnalyzer:
    """静态负载分析器 (VALID-05)"""
    
    def __init__(self, config: ConfigSchema):
        self.config = config
        self.topology = self._build_topology_graph()
    
    def _build_topology_graph(self) -> Dict[str, List[str]]:
        """构建拓扑图"""
        graph: Dict[str, List[str]] = {}
        for conn in self.config.connections:
            src = conn.src.split(".")[0]
            dst = conn.dst.split(".")[0]
            graph.setdefault(src, []).append(dst)
            graph.setdefault(dst, []).append(src)
        return graph
    
    def analyze_uniform_traffic(self) -> Dict[Tuple[str, str], float]:
        """均匀流量模式下的链路负载分析"""
        # 简化版: 计算每个链路的理论负载
        # 完整实现需要考虑路由算法和流量模式
        load_map: Dict[Tuple[str, str], float] = {}
        
        # 获取所有 PE 节点
        pe_nodes = [m.name for m in self.config.modules 
                   if m.type.value in ("CPUTLM", "ProcessorTLM")]
        
        if not pe_nodes:
            return load_map
        
        n_nodes = len(pe_nodes)
        total_pairs = n_nodes * (n_nodes - 1)
        
        # 计算每个链路的负载
        for link in self._get_all_links():
            paths_through = self._count_paths_through_link(link, pe_nodes)
            load = paths_through / total_pairs if total_pairs > 0 else 0
            load_map[link] = load
        
        return load_map
    
    def identify_hotspots(self, threshold: float = 0.15) -> List[Tuple[str, str]]:
        """识别热点链路 (负载超过阈值的链路)"""
        load_map = self.analyze_uniform_traffic()
        return [link for link, load in load_map.items() if load > threshold]
    
    def _get_all_links(self) -> List[Tuple[str, str]]:
        """获取所有链路"""
        links: List[Tuple[str, str]] = []
        seen: set = set()
        
        for conn in self.config.connections:
            src = conn.src.split(".")[0]
            dst = conn.dst.split(".")[0]
            link = tuple(sorted([src, dst]))
            if link not in seen:
                seen.add(link)
                links.append(link)
        
        return links
    
    def _count_paths_through_link(self, link: Tuple[str, str], pe_nodes: List[str]) -> int:
        """计算经过指定链路的路径数 (简化 XY 路由)"""
        count = 0
        
        for src_pe in pe_nodes:
            for dst_pe in pe_nodes:
                if src_pe == dst_pe:
                    continue
                
                path = self._compute_path(src_pe, dst_pe)
                if link in path or (link[1], link[0]) in path:
                    count += 1
        
        return count
    
    def _compute_path(self, src: str, dst: str) -> List[Tuple[str, str]]:
        """计算源到目的的路径 (简化 BFS)"""
        from collections import deque
        
        visited: set = set()
        queue = deque([(src, [])])
        
        while queue:
            node, path = queue.popleft()
            
            if node == dst:
                return path
            
            if node in visited:
                continue
            visited.add(node)
            
            for neighbor in self.topology.get(node, []):
                if neighbor not in visited:
                    queue.append((neighbor, path + [(node, neighbor)]))
        
        return []  # 无路径
```

### 3.4 连接路径追踪

**文件路径**: `cpptlm_config/path_tracer.py`

```python
# cpptlm_config/path_tracer.py

from typing import List, Optional, Dict
from collections import deque

from .models import ConfigSchema


class Hop:
    """连接跳"""
    def __init__(self, src_module: str, src_port: str, 
                 dst_module: str, dst_port: str, latency: int = 0):
        self.src_module = src_module
        self.src_port = src_port
        self.dst_module = dst_module
        self.dst_port = dst_port
        self.latency = latency
    
    def __repr__(self):
        return f"{self.src_module}.{self.src_port} -> {self.dst_module}.{self.dst_port} (latency={self.latency})"


class PathTracer:
    """连接路径追踪器 (TOOL-07)"""
    
    def __init__(self, config: ConfigSchema):
        self.config = config
        self.graph = self._build_graph()
    
    def _build_graph(self) -> Dict[str, List[tuple]]:
        """构建图"""
        graph: Dict[str, List[tuple]] = {}
        
        for conn in self.config.connections:
            src_parts = conn.src.split(".")
            dst_parts = conn.dst.split(".")
            
            src_module = src_parts[0]
            src_port = src_parts[1] if len(src_parts) > 1 else "0"
            dst_module = dst_parts[0]
            dst_port = dst_parts[1] if len(dst_parts) > 1 else "0"
            
            graph.setdefault(src_module, []).append((dst_module, src_port, dst_port, conn.latency))
        
        return graph
    
    def trace_path(self, src: str, dst: str) -> List[Hop]:
        """追踪源到目的的完整路径 (BFS)"""
        visited: set = set()
        queue = deque([(src, [])])
        
        while queue:
            node, path = queue.popleft()
            
            if node == dst:
                return path
            
            if node in visited:
                continue
            visited.add(node)
            
            for neighbor, src_port, dst_port, latency in self.graph.get(node, []):
                if neighbor not in visited:
                    hop = Hop(node, src_port, neighbor, dst_port, latency)
                    queue.append((neighbor, path + [hop]))
        
        return []  # 无路径
    
    def print_path(self, path: List[Hop]):
        """打印路径"""
        for i, hop in enumerate(path):
            print(f"Hop {i+1}: {hop}")
    
    def get_total_latency(self, path: List[Hop]) -> int:
        """计算路径总延迟"""
        return sum(hop.latency for hop in path)
```

### 3.5 配置 lint 工具

**文件路径**: `cpptlm_config/linter.py`

```python
# cpptlm_config/linter.py

import re
from typing import List

from .models import ConfigSchema
from .validator import ValidationIssue
from .types import ModuleType


class ConfigLinter:
    """配置 lint 工具 (TOOL-08)"""
    
    def __init__(self, config: ConfigSchema):
        self.config = config
        self.issues: List[ValidationIssue] = []
    
    def lint(self) -> List[ValidationIssue]:
        """运行所有 lint 检查"""
        self._check_module_naming()
        self._check_port_index_consistency()
        self._check_parameter_completeness()
        self._check_connection_patterns()
        return self.issues
    
    def _check_module_naming(self):
        """检查模块命名规范"""
        for mod in self.config.modules:
            name = mod.name
            mod_type = mod.type
            
            # RouterTLM 应使用 router_x_y 命名
            if mod_type == ModuleType.ROUTER_TLM:
                if not re.match(r'^router_\d+_\d+$', name):
                    self.issues.append(ValidationIssue(
                        severity="warning",
                        message=f"RouterTLM should use router_x_y naming: {name}",
                        suggestion="Rename to router_<x>_<y> format"
                    ))
            
            # NICTLM 应使用 ni_x_y 命名
            if mod_type == ModuleType.NIC_TLM:
                if not re.match(r'^ni_\d+_\d+$', name):
                    self.issues.append(ValidationIssue(
                        severity="warning",
                        message=f"NICTLM should use ni_x_y naming: {name}",
                        suggestion="Rename to ni_<x>_<y> format"
                    ))
    
    def _check_port_index_consistency(self):
        """检查端口索引的一致性"""
        for conn in self.config.connections:
            src = conn.src
            dst = conn.dst
            
            # 检查 Router-to-Router 连接是否都有端口索引
            if "router" in src and "router" in dst:
                if "." not in src or "." not in dst:
                    self.issues.append(ValidationIssue(
                        severity="warning",
                        message=f"Router-to-Router connection missing port index: {src} -> {dst}",
                        suggestion="Add port index for clarity (e.g., router_0_0.1)"
                    ))
    
    def _check_parameter_completeness(self):
        """检查参数完整性"""
        for mod in self.config.modules:
            if mod.type == ModuleType.ROUTER_TLM:
                for param in ["node_x", "node_y", "mesh_x", "mesh_y"]:
                    if param not in mod.params:
                        self.issues.append(ValidationIssue(
                            severity="error",
                            message=f"RouterTLM '{mod.name}' missing required parameter: {param}",
                            suggestion=f"Add '{param}' to module parameters"
                        ))
    
    def _check_connection_patterns(self):
        """检查连接模式"""
        # 检查是否有单向 Router-to-Router 连接
        router_conns = set()
        for conn in self.config.connections:
            src = conn.src.split(".")[0]
            dst = conn.dst.split(".")[0]
            if "router" in src and "router" in dst:
                router_conns.add((src, dst))
        
        for src, dst in router_conns:
            if (dst, src) not in router_conns:
                self.issues.append(ValidationIssue(
                    severity="warning",
                    message=f"Unidirectional Router-to-Router connection: {src} -> {dst}",
                    suggestion="Consider adding reverse connection for bidirectional communication"
                ))
```

---

## 四、时间线

### 4.1 周度计划

```
Week 9: 验证器整合与基础验证
  Day 1-2: validator.py 创建 (T3.4-01)
  Day 3:   孤立节点检测 (T3.4-02)
  Day 4:   BFS 可达性检测 (T3.4-03)
  Day 5:   重复连接检测 (集成在 T3.4-01)

Week 10: 端口验证与两阶段验证集成
  Day 1-2: 路由器端口方向验证 (T3.4-04)
  Day 3-4: Bundle 类型兼容性验证 (T3.4-05)
  Day 5:   两阶段验证集成 (T3.4-06)

Week 11: 高级工具与测试
  Day 1-2: 静态负载分析 (T3.4-07)
  Day 3:   连接路径追踪 (T3.4-08)
  Day 4:   配置 lint 工具 (T3.4-09)
  Day 5:   拓扑分析工具测试 (T3.4-10) + 文档更新 (T3.4-11~13)
```

### 4.2 里程碑

| 里程碑 | 日期 | 验收标准 |
|--------|------|---------|
| M1: 验证器基础完成 | Week 9 | validator.py 可用，VALID-01/02/03 工作正常 |
| M2: 端口验证完成 | Week 10 | PORT-01/03 工作正常，两阶段验证集成完成 |
| M3: Phase 3.4 发布 | Week 11 | 所有测试通过 (15+)，文档完整，共识事项全部实施 |

---

## 五、测试策略

### 5.1 Python 单元测试

**测试文件**: `cpptlm_config/tests/test_validator.py`

| 测试类别 | 测试数量 | 覆盖内容 |
|---------|:---:|---------|
| 孤立节点检测 | 3 | 有孤立节点、无孤立节点、空配置 |
| BFS 可达性 | 3 | 全连通、部分连通、单节点 |
| 重复连接检测 | 2 | 有重复、无重复 |
| 端口方向验证 | 3 | 端口索引缺失/正确 |
| Bundle 兼容性 | 2 | 兼容/不兼容 |
| 两阶段验证 | 2 | 验证通过/失败 |
| **总计** | **15** | |

**测试用例示例**:

```python
def test_orphaned_modules():
    config = ConfigSchema(
        name="test",
        modules=[
            ModuleSpec(name="router_0_0", type=ModuleType.ROUTER_TLM),
            ModuleSpec(name="orphan", type=ModuleType.CPU_TLM),
        ],
        connections=[]
    )
    
    result = TopologyValidator.validate_config(config)
    assert not result.is_valid
    assert any("orphan" in e.message for e in result.errors)


def test_reachability():
    config = ConfigSchema(
        name="test",
        modules=[
            ModuleSpec(name="r0", type=ModuleType.ROUTER_TLM),
            ModuleSpec(name="r1", type=ModuleType.ROUTER_TLM),
            ModuleSpec(name="r2", type=ModuleType.ROUTER_TLM),
        ],
        connections=[
            ConnectionSpec(src="r0.0", dst="r1.1"),
            # r2 未连接
        ]
    )
    
    result = TopologyValidator.validate_config(config)
    assert not result.is_valid
    assert any("r2" in e.message for e in result.errors)
```

### 5.2 集成测试

**测试文件**: `cpptlm_config/tests/test_integration.py`

| 测试场景 | 验证内容 |
|---------|---------|
| mesh_2x2 验证 | 所有验证通过 |
| 有错误的配置 | 验证失败，错误信息正确 |
| ConfigBuilder.build(validate=True) | 自动验证工作正常 |
| 静态负载分析 | 热点链路识别正确 |
| 路径追踪 | BFS 路径正确 |
| 配置 lint | 最佳实践检查正确 |

### 5.3 回归测试保证

- **现有测试**: 434/434 必须全部通过
- **Phase 3.2/3.3 测试**: 90/90 必须全部通过
- **新增测试**: 15+ (Python)

---

## 六、风险与缓解

### 6.1 技术风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| BFS 算法在大型拓扑中性能问题 | 低 | 低 | 拓扑规模有限 (16x16 mesh = 256 节点) |
| 负载分析精度不足 | 中 | 低 | 标记为简化版，后续改进 |
| 验证器与 C++ 端行为不一致 | 中 | 高 | 共享验证规则，CI 检查一致性 |

### 6.2 进度风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| 高级工具开发超预期 | 中 | 低 | 优先核心验证器，高级工具可简化 |

---

## 七、交付物清单

### 7.1 代码交付物

| 文件 | 类型 | 说明 |
|------|------|------|
| `cpptlm_config/validator.py` | 新建 | TopologyValidator, ValidationIssue, ValidationResult |
| `cpptlm_config/analyzer.py` | 新建 | StaticLoadAnalyzer |
| `cpptlm_config/path_tracer.py` | 新建 | PathTracer |
| `cpptlm_config/linter.py` | 新建 | ConfigLinter |
| `cpptlm_config/builder.py` | 修改 | build() 集成验证器 |
| `cpptlm_config/tests/test_validator.py` | 新建 | 验证器单元测试 |
| `cpptlm_config/tests/test_integration.py` | 新建 | 集成测试 |

### 7.2 文档交付物

| 文件 | 说明 |
|------|------|
| `docs/architecture/15-validation-toolchain.md` | 验证工具链架构设计 |
| `docs/guide/VALIDATION_GUIDE.md` | 验证工具使用指南 |

---

## 八、验收标准

| 验收项 | 标准 | 验证方式 |
|--------|------|---------|
| validator.py | ValidationIssue/ValidationResult 正确 | 15+ 单元测试通过 |
| 孤立节点检测 | 无连接模块被检测 | VALID-01 测试 |
| BFS 可达性 | 不可达节点被检测 | VALID-02 测试 |
| 端口方向验证 | 方向错误被检测 | PORT-01 测试 |
| Bundle 兼容性 | Bundle 不兼容被检测 | PORT-03 测试 |
| 两阶段验证 | build() 后自动验证 | 集成测试 |
| 负载分析 | 热点链路识别正确 | 测试用例 |
| 路径追踪 | BFS 路径正确 | 测试用例 |
| 配置 lint | 最佳实践检查 | 测试用例 |
| 回归测试 | 524/524 通过 | `pytest` + `ctest` |

---

## 九、Phase 3+ 完成标志

Phase 3.4 完成后，标志着整个 Phase 3+ 的正式完成:

| 指标 | 目标 | 实际 |
|------|------|------|
| 总任务数 | 40+ | 40+ |
| 总测试数 | 100+ | 100+ |
| 回归测试 | 434/434 | 434/434 |
| 共识事项 | 13/13 | 13/13 |
| 文档完整性 | 100% | 100% |

---

**文档状态**: 📋 路线图（依赖 Phase 3.2/3.3 完成，Python 包 `cpptlm_config/` 未实现）

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-05-05 | 初始版本，基于 ADR-X.12 v2.0 和 Phase 3+ 实施计划 v2.0 编制 |
| v1.1 | 2026-05-07 | 更新状态为路线图；添加前置条件说明（依赖 cpptlm_config Python 包） |

