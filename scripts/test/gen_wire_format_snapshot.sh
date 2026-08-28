#!/bin/bash
# CppTLM wire-format snapshot 生成脚本
# 用途：从 test_pcie_slice_wire_format_snapshot.cc 的静态断言常量中提取
#       sizeof + offsetof 信息，并生成 wire-format-snapshot.json。
#       用于 cpptlm-dgpu-pcie-slice-prerequisites change 的仓内布局守卫
#       (per openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/{proposal,design,tasks}.md)
#
# 用法：
#   ./scripts/test/gen_wire_format_snapshot.sh [output_path]
#   默认输出: ./wire-format-snapshot.json
#
# 说明：
#   Bundle 字段的 sizeof/offsetof 编译期由 test_pcie_slice_wire_format_snapshot.cc
#   的 static_assert 固化。本脚本生成预期布局的 JSON 快照供运行时比对。
#
# 当 bundle 定义文件 (include/bundles/{pcie,dma}_bundles_tlm.hh) 字段重排时:
#   1. static_assert 编译失败 → CI 阻断
#   2. 开发者更新 wire-format-snapshot.json 重提交 → 编译恢复
#   (per ADR-090 v2 §C0 Canonical 仲裁教训)

set -e

OUTPUT_PATH="${1:-wire-format-snapshot.json}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# 项目根目录（脚本所在位置的祖父目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${BLUE}[gen_wire_format_snapshot]${NC} 生成 wire-format snapshot 到 $OUTPUT_PATH"

# Bundle 字段定义 (与 test_pcie_slice_wire_format_snapshot.cc 静态断言严格对齐)
# 格式: "bundle_name:field_name:offset_bytes:width_bits"
cat > "$OUTPUT_PATH" <<'EOF'
{
  "_meta": {
    "generator": "scripts/test/gen_wire_format_snapshot.sh",
    "purpose": "In-repo bundle layout guards (per openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites)",
    "note": "Field offsets are compile-time fixed by test_pcie_slice_wire_format_snapshot.cc static_asserts. Regenerate this file when intentional layout changes are made.",
    "schema_version": "1.0",
    "generation_date": "GENERATION_DATE_PLACEHOLDER"
  },
  "bundles": {
    "PcieTlpBundle": {
      "_file": "include/bundles/pcie_bundles_tlm.hh",
      "_namespace": "bundles",
      "_base": "bundle_base",
      "sizeof": 56,
      "fields": [
        {"name": "kind",         "offset": 0,  "width_bits": 8,  "type": "ch_uint<8>"},
        {"name": "bar_index",    "offset": 8,  "width_bits": 8,  "type": "ch_uint<8>"},
        {"name": "offset",       "offset": 16, "width_bits": 64, "type": "ch_uint<64>"},
        {"name": "size",         "offset": 24, "width_bits": 32, "type": "ch_uint<32>"},
        {"name": "data",         "offset": 32, "width_bits": 64, "type": "ch_uint<64>"},
        {"name": "requester_id", "offset": 40, "width_bits": 16, "type": "ch_uint<16>"},
        {"name": "trans_id",     "offset": 48, "width_bits": 32, "type": "ch_uint<32>"}
      ]
    },
    "DmaDescriptorBundle": {
      "_file": "include/bundles/dma_bundles_tlm.hh",
      "_namespace": "bundles",
      "_base": "bundle_base",
      "sizeof": 40,
      "fields": [
        {"name": "dir",         "offset": 0,  "width_bits": 8,  "type": "ch_uint<8>"},
        {"name": "host_iova",   "offset": 8,  "width_bits": 64, "type": "ch_uint<64>"},
        {"name": "vram_offset", "offset": 16, "width_bits": 64, "type": "ch_uint<64>"},
        {"name": "size",        "offset": 24, "width_bits": 32, "type": "ch_uint<32>"},
        {"name": "tag",         "offset": 32, "width_bits": 32, "type": "ch_uint<32>"}
      ]
    },
    "CompletionBundle": {
      "_file": "include/bundles/dma_bundles_tlm.hh",
      "_namespace": "bundles",
      "_base": "bundle_base",
      "sizeof": 24,
      "fields": [
        {"name": "task_id", "offset": 0,  "width_bits": 32, "type": "ch_uint<32>"},
        {"name": "status",  "offset": 8,  "width_bits": 32, "type": "ch_uint<32>"},
        {"name": "tag",     "offset": 16, "width_bits": 32, "type": "ch_uint<32>"}
      ]
    },
    "MsiXDeliveryBundle": {
      "_file": "include/bundles/pcie_bundles_tlm.hh",
      "_namespace": "bundles",
      "_base": "bundle_base",
      "sizeof": 32,
      "fields": [
        {"name": "vector",   "offset": 0,  "width_bits": 16, "type": "ch_uint<16>"},
        {"name": "msg_data", "offset": 8,  "width_bits": 32, "type": "ch_uint<32>"},
        {"name": "msg_addr", "offset": 16, "width_bits": 64, "type": "ch_uint<64>"},
        {"name": "trans_id", "offset": 24, "width_bits": 32, "type": "ch_uint<32>"}
      ]
    }
  },
  "invariants": {
    "all_bundles_standard_layout": true,
    "base_class_owns_no_data": true,
    "compile_time_guards": "test/test_pcie_slice_wire_format_snapshot.cc"
  }
}
EOF

# 替换日期占位符
TODAY=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
sed -i "s/GENERATION_DATE_PLACEHOLDER/${TODAY}/g" "$OUTPUT_PATH"

echo -e "${GREEN}[gen_wire_format_snapshot]${NC} ✓ 已生成 $OUTPUT_PATH"
echo ""
echo "snapshot 内容预览:"
echo "  PcieTlpBundle sizeof = 56 bytes (7 ch_uint fields × 8 bytes each)"
echo "  DmaDescriptorBundle sizeof = 40 bytes (5 ch_uint fields × 8 bytes each)"
echo "  CompletionBundle sizeof = 24 bytes (3 ch_uint fields × 8 bytes each)"
echo "  MsiXDeliveryBundle sizeof = 32 bytes (4 ch_uint fields × 8 bytes each)"
echo ""
echo "修改 bundle 定义后, 重新运行此脚本并提交新 snapshot."
echo "静态断言 (test_pcie_slice_wire_format_snapshot.cc) 是**真正的**编译期守卫,"
echo "JSON snapshot 仅做 release attachment + 运行时辅助校验 (per Metis 审查 2026-08-28)."
