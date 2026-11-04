# tasks.md — UALink INC 实现层

> **change**: `cpptlm-cep-ualink-inc`(队列 #14 **实现层**)· **估时**: **3 周**
> **配套**: `proposal.md` · `design.md` · `specs/cep-ualink-inc/spec.md` · `specs/cep-collective-plane-contract/spec.md` · `specs/cep-collective-plane-base/spec.md`

---

## Phase 0 —— 治理前置

- [ ] 0.1 台账登记本 change 的**范围收窄**(用户裁决 2026-11-04:`ualink_4` **不**声明)⇒ improvement 的「计数链 7 → 8」AC 标为**移交**
- [ ] 0.2 **冻结面清单**逐件列名(3 份)登记到台账 ⇒ 归档时走两步授权
- [ ] 0.3 `openspec validate cpptlm-cep-ualink-inc --strict` rc=0

---

## Phase 1 —— 载体

- [x] 1.1 `include/tlm/cep/ualink_adapter.hh`(新)—— 零自有表副本 + 无 `transition_*` + 无 `kInstanceId`
- [x] 1.2 `include/tlm/cep/fabric_collective_plane.hh`(新)—— 数据路径真可达 + C-3 互斥
- [x] 1.3   ⇒ `Status::ControlPlaneConflict = 12`(追加在末尾 ⇒ 既有数值不变)+ `status_name()` + `kCollectivePlaneRegistry` 3 行 + `CollectivePlaneRegistryEntry` 第三字段 `enforces_control_plane_exclusion`
`include/tlm/cep/collective_plane.hh` —— `Status::ControlPlaneConflict`(+`status_name`)+ `kCollectivePlaneRegistry` 登记 + `CollectivePlaneRegistryEntry` 第三字段
- [x] 1.4 编译通过(`-Wall -Wextra` 零告警)

---

## Phase 2 —— 判据面(契约)

- [x] 2.1 `VcBindingSnapshot` 扩展 `per_network_mappings`(逐实例 `class → vc_id`)
- [x] 2.2 `evaluate_vc_reservation`:`status` 值域 **2 值** + `certificate_scope` 校验
- [x] 2.3   ⇒ `evaluate_shared_mode_certificate(decl, snap, untested, detail)`(C-1′/C-2/C-3/C-4)+ 归因前缀**精确**(`[CP-03-VC-CERTIFICATE/C-1′]` / `[CP-09-IC1]` / `[CP-03-VC-CERTIFICATE/C-3]` / `[CP-03-VC-CERTIFICATE/C-4]`)
`evaluate_shared_mode_certificate`(V6-d):C-1′ / C-2 / C-3 / C-4 + **精确前缀**
- [x] 2.4   ⇒ `UntestedRegistry` + `default_untested_registry()`(**消费**规约载体,⛔ 不复制 U-1/U-2)
`UntestedRegistry` + `default_untested_registry()`(消费规约载体,⛔ 不复制)
- [x] 2.5 `evaluate_vc_precondition`(V6-c)在 `shared_no_isolation` 下的**适用条件**改写
- [x] 2.6 声明面:`collective_planes.json`(`status` + `certificate_scope`)+ `collective_planes.schema.json`(enum + 属性 + if/then)

---

## Phase 3 —— 判据(测试)

- [x] 3.1 `test/test_cep_ualink_inc.cc`(新)—— P-1 ~ P-8 正向判据(每条负向断言配正控)
- [x] 3.2   ⇒ 驱动脚本 **13/13** 全绿(归因 id 双向覆盖自检 + 基线正控 + **11 组 / 11 条** fixture)
**11 组 / 11 条**反向 fixture 驱动脚本(`scripts/test/cep_ualink_inc_reverse_verify.sh`)—— 双分支(影子目录优先)+ 归因**精确相等**
- [x] 3.3 **归因 id 元测试**(双向覆盖):fixture 期望集合 ⟺ 登记集合
- [x] 3.4   ⇒ ctest **147 → 148**
`ctest` 注册

---

## Phase 4 —— 文档 + 冻结面 + 归档

- [x] 4.1 `scripts/README.md` 守门表 + **反向 fixture 登记**节
- [ ] 4.2 07 号 §5.1.1 UALink 行 + §11 Collective Plane 行(**冻结面**)
- [ ] 4.3 冻结面两步授权:台账 Entry(逐件列名)→ commit → `--update --authorized-by` → commit → `--update` → commit
- [ ] 4.4 `openspec archive cpptlm-cep-ualink-inc -y`(delta 合并进 3 份 main spec / 07 号)
- [ ] 4.5 KPI 回填(**0/3 → 1/3 → 0/3**)+ 台账 Entry
- [ ] 4.6 收口:`ctest` 全绿 + 门禁 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0
