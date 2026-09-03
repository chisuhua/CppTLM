#!/bin/bash
# ci_e2e_test.sh — cpptlm_sim CLI 端到端验证（兼容入口）
#
# 迁移说明: 原 100 行独立实现已折叠进 scripts/test/test_off.sh 的 run_e2e()
#（--help 冒烟 + 权威 TLM CLI E2E 配置 + PASS/FAIL 计数 + FAILED 列表全部保留），
# 本文件退化为薄包装以保证既有调用方（scripts/README.md / CI）向后兼容。
# 如需扩展 E2E 用例，请编辑 test_off.sh 的 run_e2e() cases 数组。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec "$SCRIPT_DIR/test_off.sh" --e2e "$@"