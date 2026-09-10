# Design: cpptlm-stage-1-4-2-1

## 1.4 电源管理

```cpp
// pcie_endpoint_ip.cc
class PcieEndpointIP {
public:
    void set_power_state(PciePowerState state);  // D0/D3hot/D3cold
    void enable_aspm(AspmLevel level);  // L0s/L1
    bool read_pm_cap(uint16_t offset, uint32_t* val);
};
```

## 2.1 P2P + Resizable BAR

```cpp
// pcie_bypass_mux.cc
int p2p_dma_route(uint32_t src_bdf, uint32_t dst_bdf, uint64_t addr, size_t len);
// ARI routing + ACS check
```

## 风险

- **低风险**：电源状态切换相对隔离
- **中风险**：P2P ACS 路由需硬件兼容性验证
