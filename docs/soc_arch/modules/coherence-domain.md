# coherence-domain 微架构文档

> **类别**: Coherence > Domain · **状态**: ✅ 已实施（基础设施级 + v1.0 dGPU SoC 战略补充）
> **Header**: `include/core/coherence_domain.hh`
> **注册**: **未注册到 ModuleFactory**（基础设施，**非 TLM 模块**）
> **蓝图来源**: gem5 `src/mem/ruby/slicc_interface/AbstractController.hh`（简化版）+ Ruby MOESI Hammer（per `docs/research/gem5-soc-survey.md` §2.5）
> **首版 commit**: Phase 4.2（v2.1 路径同步，2026-04-30 附近）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略 + ADR-SOC-09 D4 双 vendor coherence)
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.6
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §5
> - Phase 7.C: [`roadmap.md`](../../../roadmap.md) §Phase 7.C（最高风险）
> - **L7 Coherence 子系统架构**: [`docs/soc_arch/architecture/09-coherence-protocol.md`](../architecture/09-coherence-protocol.md) — 完整 v1.0 设计
> - **关联 ADR**:
>   - [`ADR-SOC-01-coherence-protocol-strategy.md`](../../adr/ADR-SOC-01-coherence-protocol-strategy.md) — 分步走策略 (✅ Accepted + Status Update)
>   - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D4 — v1.0 双 vendor coherence 跨域桥接

---

## 1. 设计目标

`CoherenceDomain` 是 CppTLM v2.1 的**跨 cache 一致性域基础设施**（Phase 4.2 引入 + Phase 4.3 扩展）。**与 gem5 对位**: `gem5::RubySystem`（抽象基类，简化版）。

**v1.0 dGPU SoC 战略补充**(per `00-overview.md` v3.1 PASS §4-bis R23-R24 + ADR-SOC-09 D4):
- **共享 MOESI/GPU 6 状态 × 6 事件**:v1.0 MVP 基础(per `coherence-protocol.md` + ADR-SOC-01)
- **v1.0 跨域桥接基础**:CPU↔GPU coherence 基础;v1.1 完整版追加完整 snoop filter + 优化(per `00-overview` §4-bis R24)
- **NVIDIA + AMD 双 vendor 跨域**:NVIDIA 域(USRI 路径)+ AMD 域(Infinity Fabric 路径)

**核心特性**（来自 `coherence_domain.hh:14-69`）：
- `Protocol` 枚举：`MESI` / `MOESI`（**未实现 `MSI` / `MESIF` / `CHI`**——待 v2.2 扩展）
- 域成员管理（`std::vector<std::string> members_`）
- Snoop fanout 数值（`int snoop_fanout_`）
- Home node 查找（`lookup_home_node(addr)`）
- 跨域桥接（`register_bridge(target_domain, bridge_name)`）
- `DomainRegistry` 静态单例（`std::unordered_map<std::string, std::shared_ptr<CoherenceDomain>>`）

**注意**: `CoherenceDomain` **不是 TLM 模块**——它**不继承** `ChStreamModuleBase`，**不挂载** StreamAdapter，**不通过** REGISTER_CHSTREAM 注册。它是**配置层基础设施**（`SimObject` 派生但无端口/无 tick 业务逻辑）。

## 2. 架构概览

### 2.1 类层次

```
SimObject (基类)
   └── CoherenceDomain (Phase 4.2)
       ├── Protocol protocol_  (MESI / MOESI)
       ├── members_           (域成员模块名列表)
       ├── snoop_fanout_      (fan-out 数值)
       └── bridge_map_        (target_domain → bridge_name)
```

### 2.2 静态注册表

```
DomainRegistry::get_domains() (static)
   ↓
   std::unordered_map<std::string, std::shared_ptr<CoherenceDomain>>
   ↓
   register_domain() / domain_exists() / get_domain() / clear()
```

## 3. 接口（Public API）

```cpp
enum class Protocol { MESI, MOESI };

class CoherenceDomain : public SimObject {
public:
    CoherenceDomain(const std::string& name, EventQueue* eq);
    ~CoherenceDomain() override = default;

    // 配置 setter
    bool set_protocol(Protocol p);
    bool set_members(const std::vector<std::string>& members);
    bool set_snoop_fanout(int fanout);

    // 查询
    bool is_member(const std::string& id) const;
    std::vector<std::string> get_snoop_targets() const;
    std::string lookup_home_node(uint64_t addr) const;

    // 跨域桥接
    void register_bridge(const std::string& target_domain,
                        const std::string& bridge_name);
    bool has_bridge_to(const std::string& target_domain) const;
    const std::string& get_bridge(const std::string& target_domain) const;

    void tick() override {}  // 空实现（配置层无业务逻辑）
};

class DomainRegistry {
public:
    static void register_domain(const std::string& name,
                                std::shared_ptr<CoherenceDomain> domain);
    static bool domain_exists(const std::string& name);
    static std::shared_ptr<CoherenceDomain> get_domain(const std::string& name);
    static void clear();
};
```

## 4. 行为流程

**CoherenceDomain 不执行业务逻辑**——它**只承载配置数据**：
- `set_*()` setter 存储数据
- `is_member()` / `get_snoop_targets()` / `lookup_home_node()` 是查询接口
- `register_bridge()` / `has_bridge_to()` 是跨域映射接口
- `tick()` 是**空实现**（`coherence_domain.hh:37`）

**实际"一致性行为"在 Phase 7.C 才实施**——届时 CacheTLM 会通过 `CoherenceDomain` 查询 snoop targets 并发 snoop 请求。

## 5. Bundle 字段使用

**无 Bundle 字段**——CoherenceDomain 是**纯配置层**，不传输事务数据。

## 6. 统计

**无 StatGroup**（基础设施无业务行为 → 无统计需求）。

## 7. 蓝图（未来演进）

### 7.1 Phase 7.C 应用

调研 §2.6 + Phase 7.C 蓝图：CoherenceDomain 升级路径：
- 真实 snoop 协议实现（MESI/MOESI 状态机）
- `CacheTLM` 与 `CoherenceDomain` 集成：snoop callback + `lookup_home_node()` 真实查询
- 6×6 状态转换表（`I→S`/`I→M`/`S→M`/`M→I`/`S→I`/等）

### 7.2 蓝图增强

- **更多协议**：CHI / MESIF / 目录协议
- **跨域桥接真实实现**：当前 `bridge_map_` 仅字符串映射，Phase 7.C 真正连接两端 CoherenceDomain
- **Domain tree**：层级化子域（与 `gem5::RubySystem` 多级对齐）
- **统计**：域内 snoop 数量 / 跨域流量

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **`tick()` 空实现**——v2.1 完全无一致性行为 | 高 | 高 | Phase 7.C 实施前仅"配置层"，v2.2 业务逻辑 |
| R2 | **仅 2 个协议**（MESI/MOESI） | 中 | 中 | v2.2 加 CHI/MESIF 枚举 |
| R3 | **`get_snoop_targets()` 返回空 `std::vector`**（v0 实现） | 高 | 中 | Phase 7.C 基于 `members_` + snoop_fanout_ 真实计算 |
| R4 | **`bridge_map_` 仅字符串映射**——无实际桥接 | 高 | 中 | Phase 7.C 接 `ProtocolBridge` 模块 |
| R5 | **`DomainRegistry` 静态单例**——线程不安全 | 中 | 中 | 当前单线程仿真无影响；v2.2 加锁 |
| R6 | **无单元测试** | 高 | 中 | 已有 `test/test_domain_boundary.cc` 测字符串 API；Phase 7.C 加语义测试 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单元测试 | ✅ | `test/test_domain_boundary.cc`（字符串 API + 跨域 bridge） |
| 配置层 setter/getter | ✅ | `set_protocol()` / `set_members()` / `set_snoop_fanout()` |
| Home node 查找 | ✅ | `lookup_home_node()` 真实逻辑（基于 members 列表） |
| DomainRegistry 静态 API | ✅ | `register_domain()` / `domain_exists()` / `get_domain()` / `clear()` |
| 真实一致性行为 | ❌ v0 空 | 见 R1，Phase 7.C 蓝图 |
| 真实跨域桥接 | ❌ 字符串映射 | 见 R4，Phase 7.C 蓝图 |

## 10. 修订历史

- **Phase 4.2 (2026-04-30 附近)**: CoherenceDomain 初版（基础设施，仅 TDD stub）
- **Phase 4.3 (2026-05)**: 扩展（`register_bridge` / `has_bridge_to` 跨域桥接 + `DomainRegistry` 静态单例）
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B2 批次）
- **Phase 7.C (未来)**: 与 CacheTLM 集成（最高风险）
