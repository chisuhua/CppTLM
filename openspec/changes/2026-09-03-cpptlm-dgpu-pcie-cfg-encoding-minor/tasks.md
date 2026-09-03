# Tasks — PCIe Config Space 地址编码修复

> **配套**: [proposal.md](./proposal.md)

## T1: 地址解码实现

- [ ] T1.1 定位 AXI→PCIe Config Space 的唯一地址解码入口
- [ ] T1.2 实现 `cfg_offset = (awaddr >> 2) & 0x3f`，使用定宽无符号转换
- [ ] T1.3 明确区分 Config Space 请求与 BAR/MMIO 请求，避免 BAR 路径误解码
- [ ] T1.4 保持 `PcieEndpointTLM` 与 `include/abi/cpptlm_emulator.h` ABI 冻结

## T2: 测试

- [ ] T2.1 增加低 2 bit 变化不影响 offset 的单元测试
- [ ] T2.2 增加 offset 0、4、末端合法值与越界值测试
- [ ] T2.3 修正 `test_pcie_endpoint_ip_full_e2e.cc` 配置请求构造方式
- [ ] T2.4 验证 `test_pcie_endpoint_ip_full_e2e_config` 与 `_bar`
- [ ] T2.5 运行 `[pcie]` 与 `[axi]` 回归，确认无新增失败

## T3: 文档与验收

- [ ] T3.1 更新 `AGENTS.md` 已知问题，移除已修复的 config/bar known-fail 描述
- [ ] T3.2 更新相关架构/验证文档中的地址编码说明
- [ ] T3.3 `openspec validate 2026-09-03-cpptlm-dgpu-pcie-cfg-encoding-minor --strict` PASS
- [ ] T3.4 运行完整构建与必要的 CTest 验证
- [ ] T3.5 archive change + 同步 main specs
