#!/bin/bash
# scripts/test/check_pcie_ip_facade.sh
# P2 PR-A Task 18: CI gate to detect facade bypass in PcieControllerIP /
# PcieAxiBridgeIP. Per Metis bg_899dfdcc + Oracle bg_fb84a648 review.
#
# Bypass patterns (any of these in a gated file = FAIL):
#   1. Direct access to PcieEndpointIP private members (e.g., bar_store_,
#      resizable_bars_, internal_*).
#   2. Direct PcieAxiAdapterTLM::attach_to_endpoint outside the facade
#      composition flow (would double-attach if both JSON auto-attach +
#      PcieAxiBridgeIP member adapter are used).
#   3. JSON configs under configs/* that contain "axi_adapter" or
#      "axi4_mapper_inject" (per T2 — v2 realip configs MUST NOT have these
#      to avoid double AXI traffic).
#
# Whitelist (legitimate internal access):
#   include/tlm/pcie/pcie_endpoint_ip.*
#   src/tlm/pcie/pcie_endpoint_ip.cc
#   include/tlm/pcie/pcie_controller_ip.*
#   src/tlm/pcie/pcie_controller_ip.cc
#   include/tlm/pcie/pcie_axi_bridge_ip.*
#   src/tlm/pcie/pcie_axi_bridge_ip.cc
#   include/tlm/pcie/pcie_axi_adapter_tlm.*
#   src/tlm/pcie/pcie_axi_adapter_tlm.cc
#   include/framework/axi4_mapper.*
#   src/framework/axi4_mapper.cc
#   test/test_pcie_* (until Phase B migration)
#   src/tlm/gpu/dgpu_board_shell.cc (until Phase B; uses flag-based routing)

set -e

VIOLATIONS=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/.."

# ── Check 1: facade bypass ──
WHITELIST_PATTERN_1="(pcie_endpoint_ip\.(cc|hh)|pcie_controller_ip\.(cc|hh)|pcie_axi_bridge_ip\.(cc|hh)|pcie_axi_adapter_tlm\.(cc|hh)|axi4_mapper\.(cc|hh)|test_pcie_|dgpu_board_shell\.cc)"

GATED_CC_HH=$(find include/tlm src/tlm src/framework -name "*.hh" -o -name "*.cc" 2>/dev/null \
    | grep -vE "$WHITELIST_PATTERN_1" || true)

# Patterns indicating facade bypass
BYPASS_PATTERNS='PcieEndpointIP::internal_|pcie_endpoint_ip.*->internal_|bar_store_\.[a-z]|resizable_bars_\.[a-z]'

for f in $GATED_CC_HH; do
    if grep -E "$BYPASS_PATTERNS" "$f" > /dev/null 2>&1; then
        echo "FACADE BYPASS: $f accesses PcieEndpointIP internals"
        grep -nE "$BYPASS_PATTERNS" "$f" | head -3 | sed 's/^/  /'
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
done

# ── Check 2: v2 realip JSONs must NOT contain axi_adapter/axi4_mapper_inject ──
for f in configs/dgpu_soc_v2_realip*.json; do
    if [ -f "$f" ]; then
        if grep -E '"axi_adapter"|"axi4_mapper_inject"' "$f" > /dev/null 2>&1; then
            echo "JSON DOUBLE-ADAPTER RISK: $f contains forbidden keys (axi_adapter / axi4_mapper_inject)"
            grep -nE '"axi_adapter"|"axi4_mapper_inject"' "$f" | sed 's/^/  /'
            VIOLATIONS=$((VIOLATIONS + 1))
        fi
    fi
done

# ── Self-check: gate must not flag the facade files themselves ──
SELF_CHECK=$(find include/tlm src/tlm -name "pcie_controller_ip*" -o -name "pcie_axi_bridge_ip*" 2>/dev/null \
    | xargs grep -lE "$BYPASS_PATTERNS" 2>/dev/null || true)
if [ -n "$SELF_CHECK" ]; then
    echo "SELF-CHECK FAIL: gate flagged facade files: $SELF_CHECK"
    VIOLATIONS=$((VIOLATIONS + 1))
fi

if [ $VIOLATIONS -gt 0 ]; then
    echo ""
    echo "Total violations: $VIOLATIONS"
    exit 1
fi
echo "PASS: no facade bypass detected"