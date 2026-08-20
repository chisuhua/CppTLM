 1. 背景：PCIe 本质是 不保序 的（尤其是 posted writes）
在理解 strong-ordered store 之前，必须明确：PCIe 原生不保证跨地址的写顺序。
尤其：
● PCIe posted write：只要写 TLP 进入 RC / Switch buffer，就算“完成”，不保证什么时候到达设备，也不保证到达顺序。
● 如果你跨 aperture（例如：从 GPU HBM → system memory → PCIe BAR → NIC），PCIe 交换机会把 TLP 在不同路径排队，顺序完全不可控。
因此要：
● 软件 fence（例如 GPU 内存栅栏、CPU mfence、dummy read）
● 或者硬件 flush
来实现 global ordering。
NVIDIA 在这个专利里，就是 把软件 fence 的工作，卸载给 MMU + ordering pipe 自动完成。

🧠 2. 专利里的 strong ordered store 是如何保序的？
核心实现机制分成 两个阶段：

① Pre-translation ordering（地址转换前的顺序）
目标：保证 strong-order 的地址翻译本身不会乱序。
原文描述如下：
● strong store 的地址转换必须等待所有 weak-ordered store 完成 translation
● MMU 的 pre-translation ordering module 对 strong store 挂起，直到所有之前的 weak ordered 操作都“完成 translation”
➡️ 这样保证：在地址翻译阶段，weak → strong 的顺序不会乱序。
但是仅有翻译有序是不够的，还需要 跨 aperture 可见性顺序。

② Post-translation ordering（地址转换之后的顺序控制）
这是 PCIe strong ordering 的关键：
专利中明确指出：
当 strong-ordered store 目标 aperture 与之前的 store 不同时（如从 memory → PCIe BAR），必须执行 “aperture-switch I/O flush”。
即：
1. MMU 发现 strong store 目标物理地址属于 PCIe aperture
2. 并且之前存在访问另一个 aperture 的 posted store
3. 则必须执行：
📌 dummy read / non-posted read flush
也就是：
● 发出一个 non-posted read TLP
● PCIe 协议强制所有之前 posted writes 在返回 read completion 前必须完成
➡️ 这就是 PCIe 上能用的唯一硬件级强顺序保证机制（PCIe本身不提供“store barrier”，只能用 non-posted read 作为 flush）。
专利严格使用此机制：
strong-ordered store =
➡️ 在跨 aperture 时强制执行 dummy read flush➡️ 等待 completion➡️ 再发 strong-ordered write
因此：
strong-ordered store 的可见性保证，本质上是靠 自动化的 PCIe non-posted read flush 实现的。

🔍 3. strong-ordered write 的完整硬件路径（根据专利）
根据专利的时序，可以画成这样（文字版）：
weak store → posted write（入 PCIe 交换机队列） → (不保证顺序)

strong store → 触发 aperture-switch 检查
              → 发现 aperture 不一致（memory→PCIe）
              → MMU 生成 dummy non-posted read
              → PCIe switch 强制 flush 全部 posted writes
              → read completion 到达 → 顺序保证
              → strong store 发出（PCIe posted 或 non-posted）

这才是 PCIe strong ordering 的真正实现链路。

📉 4. 代是什么？（硬件 & 性能代价）
① PCIe non-posted read 延迟：巨大
PCIe read completion 往返时延：
● PCIe Gen5 x16：~250–350ns
● 多级 PCIe switch：400–600ns
● 跨 CPU root complex：>700ns
➡️ 即：每个 strong-ordered store 可能要付出几百纳秒到上微秒级延迟。
这是它的最大成本。

② MMU 增加两个 ordering pipeline
● pre-translation ordering module
● post-translation ordering module
需要：
● 额外状态机
● strong store 队列
● weak store 队列完成度跟踪
● aperture tracker
● “pending posted write” bit
● flush dependency bitmap
➡️ 成本不大，但逻辑变复杂。

③ 需要全系统可见性追踪（尤其是多 SM、多 GPU）
为了防止乱序，必须：
● 每 SM（或每 GPU MMU）维护 store ordering token
● aperture history
● outstanding posted writes
这是 state space 最大的一块成本。

④ 会降低吞吐（flush 是 global serialization）
在 flush 过程中：
● 所有 posted write 无法并行排队
● 写带宽会出现 “全局停顿（global stall）”
在 NIC doorbell/wqe 写等场景里尤为明显。

🧩 5. 为什么 NVIDIA 要硬件实现 strong-ordered store？
因为：
● GPU→NIC WQE submission
● GPU doorbell write
● GPU→CPU signal
● GPU→GPU control write
● NVLink AMPERE/Hopper 的 command packet write
都需要保证：
数据 → WQE → doorbell
必须按顺序可见

如果用软件 fence：
● GPU stall 数万周期
● 代价太高
NVIDIA 用硬件 MMU 自动完成 flush，即：
用硬件代替软件 memory fence，从而减少 GPU stall。
这正是这个专利的核心动机。

🏁 最终总结：
PCIe strong ordering 在本专利中的真正实现方式：
1. 使用 MMU ordering pipeline（pre & post translation）保证翻译有序
2. 使用 aperture-switch 检查判断是否需要 flush
3. 若需要 → 发出 non-posted read（dummy read flush）
4. 通过 PCIe 协议强制 flush 掉之前所有 posted writes
5. read completion 回来后，发 strong store
➡️ 这是 PCIe 上唯一能真正实现“strong order”的方式。
