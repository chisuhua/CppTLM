# Tasks: Fix DPRINTF Consistency

## 1. Analysis

- [ ] 1.1 审计 DPRINTF 使用

## 2. Implementation

- [ ] 2.1 统一日志格式

## 3. Verification

- [ ] 3.1 构建验证

## Analysis Note (P2.D)

Audit of DPRINTF usage across codebase:
- DPRINTF(module, fmt, ...) macro defined in sim_core.hh
- Usage pattern: DPRINTF(MODULE_NAME, "message\n")
- Module names: CONN, MODULE, CPU, VC, ROUTER, etc.
- Format consistency: "[PREFIX] message" pattern used

Conclusion: DPRINTF macro is used consistently. The current implementation
does not require changes - it follows a consistent pattern already.
