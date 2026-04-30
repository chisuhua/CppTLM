# include/utils/ — 工具类

**域**: 配置/布局/正则/动态加载（8 文件）
**作用**: 辅助工具：JSON 解析、力导向布局、正则匹配、模块分组

## 文件

| 文件 | 行数 | 作用 |
|------|------|------|
| `config_utils.hh` | 50+ | JSON 配置解析辅助 |
| `json_includer.hh` | 100+ | JSON include 机制（$ref 支持） |
| `module_group.hh` | 100+ | 模块分组和层级管理 |
| `wildcard.hh` | 50+ | 通配符匹配（* 和 ?） |
| `regex_matcher.hh` | 100+ | 正则表达式匹配 |
| `dynamic_loader.hh` | 50+ | 动态库加载接口（dlopen/dlsym） |
| `force_directed_layout.hh` | 300+ | 力导向布局算法（可视化） |
| `topology_dumper.hh` | 300+ | 拓扑导出（Graphviz/Mermaid） |

## 核心工具

### ConfigUtils
- `parseConfig(json)` — 解析顶层配置
- `validateSchema(config)` — 验证必需字段

### JsonIncluder
- `$ref` 支持 — 引用其他 JSON 文件（如 `"$ref": "base.json"`）
- 递归解析 include

### Wildcard/RegexMatcher
- `matchWildcard(pattern, name)` — glob 风格匹配
- `matchRegex(pattern, name)` — 正则表达式匹配
- **用于连接解析**: `"cache.*" → 匹配所有以 cache 开头的模块`

### ModuleGroup
- 分层结构支持：`{ "name": "group0", "members": ["mod1", "mod2"] }`
- 组内连接快捷方式

## 约定

- 工具类头文件在 `include/utils/`，实现部分在 `src/utils/`
- `dynamic_loader.cc` 是唯一 utils 实现文件
- `force_directed_layout` 和 `topology_dumper` 用于可视化/调试

## 注意事项

- 配置解析相关工具被 `ModuleFactory::instantiateAll()` 使用
- Wildcard/Regex 用于 `ConnectionResolver` 匹配连接模式