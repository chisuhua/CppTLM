#!/bin/bash
# Verify GpuComputeUnitTLM 旧类型不再存在 (Task 9 rename)
# Per architecture/15 §15.7.2 决策: rename 而非 deprecated shim
# 排除本测试文件自身的注释引用
set -e
HITS=$(grep -rE "tlm::GpuComputeUnitTLM|\"GpuComputeUnitTLM\"|class GpuComputeUnitTLM" \
    include/ src/ \
    test/test_*.cc 2>/dev/null \
    | grep -v "test_sm_top_no_legacy_type.sh" \
    || true)
if [ -n "$HITS" ]; then
    echo "FAIL: GpuComputeUnitTLM references found:"
    echo "$HITS"
    exit 1
fi
echo "PASS: no GpuComputeUnitTLM references"
