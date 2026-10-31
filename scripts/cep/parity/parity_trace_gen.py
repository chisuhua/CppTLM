#!/usr/bin/env python3
"""scripts/cep/parity/parity_trace_gen.py
   —— H7 对拍基准的**同源 trace 生成器**(两侧消费**同一条**访问流)

为什么需要它(per `docs/architecture/14-coherence-parity-baseline.md` §4.1):
   · 本仓 `TrafficGenTLM` 的 RNG 是**内部种子** ⇒ 两侧各自生成 ⇒ **无法同源**;
   · gem5 `MemTest` 的地址序列由 `Random::genRandom()` 播种 ⇒ **每次运行不同**;
   ⇒ 唯一正解 = **离线生成一条确定性访问序列,两侧读同一文件**。

产物(两种格式,同一份内容):
   ① `<out>.csv`      —— 本仓侧格式:`addr,is_write,size`(纯文本,`14 号` §4.1 规定);
   ② `<out>.pb`       —— gem5 侧格式:`ProtoMessage.PacketHeader` + `ProtoMessage.Packet`
                          (per gem5 `src/proto/packet.proto`;由 `protoc --python_out` 生成
                           的 `packet_pb2` 序列化)。

确定性保证:
   · 地址序列用**固定 LCG 种子**生成(不用 `random` 模块 ⇒ 不受 Python 版本影响);
   · 工作集大小**显式可配** ⇒ 命中/未命中比由**工作集**控制(per §4.2);
   · 访问大小**必须 < cache line**(默认 8B)—— 等于 line 大小时会被判为「整行写」
     ⇒ gem5 cache 转成 `InvalidateReq` ⇒ `SimpleMemory` panic;
   · 同样的 `(seed, working_set, num_accesses, ...)` ⇒ **逐字节相同**的输出(可 sha256 校验)。

用法:
   python3 scripts/cep/parity/parity_trace_gen.py --out build/parity/trace \\
       --seed 42 --num-accesses 100000 --working-set 65536
"""
import argparse
import hashlib
import json
import os
import pathlib
import sys

# ── 确定性 LCG(参数取自 Numerical Recipes 的常用常数;**不用** random 模块)──
LCG_A = 1664525
LCG_C = 1013904223
LCG_M = 1 << 32


def lcg_stream(seed, count):
    """产出 `count` 个 [0, 1) 的确定性伪随机数(LCG / 固定种子)。"""
    state = seed & 0xFFFFFFFF
    for _ in range(count):
        state = (LCG_A * state + LCG_C) & 0xFFFFFFFF
        yield state / float(LCG_M)


def gen_accesses_hot_cold(seed, num_accesses, hot_bytes, cold_bytes,
                          base_addr, line_size, write_percent, hot_ratio):
    """**双峰(非均匀)**访问分布 —— 用于**证伪**「工作集/容量」规则 R-B。

    ⚠️ **为什么需要它**(per `14 号` §4.4 的 P-2a A-1 鉴别力自检):
    规则 R-B 估 `h_0 = min(1, C/W)`(`W` = **整体**工作集)⇒ 它**只对均匀随机最优**。
    本生成器构造「**热区全命中 + 冷区全不命中**」的双峰分布:
      · 热区 `hot_bytes` ≤ cache 容量 ⇒ 第 2 遍起**全命中**;
      · 冷区 `cold_bytes` ≫ cache 容量 ⇒ **全不命中**;
      · 按 `hot_ratio` 比例交替访问 ⇒ 实际命中率 ≈ `hot_ratio`;
    ⇒ 而 R-B 用「整体工作集」估 ⇒ `h_0 ≈ min(1, C/(hot+cold))` ≪ `hot_ratio`
    ⇒ **估算误差 ≫ 5pp** ⇒ 构成「**规则失效型**」流 ⇒ **A-1 可满足**。
    """
    hot_lines = max(1, hot_bytes // line_size)
    cold_lines = max(1, cold_bytes // line_size)
    cold_base = base_addr + (1 << 24)   # 远离热区 ⇒ 必然不在 cache 内
    randoms = lcg_stream(seed, num_accesses * 3)
    for _ in range(num_accesses):
        pick = next(randoms)
        offset = int(next(randoms) * 1000000) % 1000000
        is_write = next(randoms) < (write_percent / 100.0)
        if pick < hot_ratio:
            addr = base_addr + (offset % hot_lines) * line_size
        else:
            addr = cold_base + (offset % cold_lines) * line_size
        yield addr, is_write, line_size


def gen_accesses(seed, num_accesses, working_set_bytes, base_addr, line_size,
                 write_percent):
    """生成访问序列。

    命中/未命中比的控制(**per §4.2**):本仓 `CacheTLM` 的命中判定是
    「该地址**曾被访问过**」(精确地址 map,无限容量)⇒ 因此
    **命中率 ≈ 「落在已访问地址集合内的比例」**,而不是容量/替换的结果。
    ⇒ 本生成器用「工作集内随机 + 工作集外随机」的比例来**显式控制**该比例:
      · `working_set_bytes` 内的地址 ⇒ 命中(第二次及以后);
      · 其余 ⇒ 未命中(首次见到即 miss)。
    """
    ws_lines = max(1, working_set_bytes // line_size)
    out_ws = (1 << 20)  # 工作集外区域:距工作集足够远 ⇒ 必然未命中
    # ⚠️ 每条访问需要 **2 个** LCG 取样(① 命中区判定 + ② 区内偏移),
    #    读写由**第 3 个**取样决定 ⇒ 必须抽 `num_accesses * 3` 个随机数,
    #    否则访问条数会少于 `num_accesses`(早期版本用 `i % 3` 分支 ⇒ 丢 2/3 数据)。
    randoms = lcg_stream(seed, num_accesses * 3)
    for _ in range(num_accesses):
        in_ws = next(randoms) < 0.8
        offset = int(next(randoms) * ws_lines) * line_size
        addr = base_addr + offset if in_ws else base_addr + out_ws + offset
        is_write = next(randoms) < (write_percent / 100.0)
        yield addr, is_write, line_size


def write_csv(path, accesses):
    with open(path, "w", encoding="utf-8") as f:
        f.write("addr,is_write,size\n")
        for addr, is_write, size in accesses:
            f.write(f"{addr},{'1' if is_write else '0'},{size}\n")


# ── gem5 的 proto **二进制流**格式(per `src/proto/protoio.cc`)──────────
# ⚠️ **不是裸序列化**!gem5 的 `ProtoOutputStream`/`ProtoInputStream` 约定:
#     ① 文件头 4 字节 **magic number** = `0x356d6567`,**小端**(LE);
#     ② 每条消息前跟一个 **Varint32 长度前缀**,再跟消息体;
#     ③ `TraceGen::InputStream::init()` 会**单独先读一条 `PacketHeader`**,
#        且校验 `header.tick_freq == sim_clock::Frequency`
#        ⇒ **tick_freq 必须等于 gem5 的全局频率**(build/NULL = 1THz = 1e12);
#     ④ 每条 `Packet` 的 `tick` 字段决定**回放时刻**(相对 header 起点)。
#   (早期版本直接写裸序列化 ⇒ gem5 报
#    `panic: Input file ... is not a valid gem5 proto format.` —— 已由本段修正。)
GEM5_PROTO_MAGIC = 0x356D6567


def _write_varint32(buf, value):
    """protobuf Varint32 编码(小端 7-bit/组)。"""
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            buf.append(byte | 0x80)
        else:
            buf.append(byte)
            break


def write_protobuf(path, accesses, obj_id, tick_freq, tick_interval=100000):
    """写 gem5 的 `PacketHeader` + `Packet` 序列(per gem5 `src/proto/packet.proto` + `protoio.cc`)。

    ⚠️ 需要 `protoc` 生成的 `packet_pb2` 模块:
       `protoc --python_out=<dir> --proto_path=external/gem5/src/proto \
         external/gem5/src/proto/packet.proto`
    """
    # `packet_pb2` 的位置由 `--python_out` 决定 ⇒ 允许用环境变量
    # `PARITY_PB2_DIR` 显式指定(默认:与 trace 同目录,再回退到 `build/parity`)。
    candidates = [
        pathlib.Path(os.environ["PARITY_PB2_DIR"]) if os.environ.get("PARITY_PB2_DIR") else None,
        path.parent,
        pathlib.Path(__file__).resolve().parents[3] / "build" / "parity",
    ]
    for cand in candidates:
        if cand and (cand / "packet_pb2.py").exists():
            sys.path.insert(0, str(cand))
            break
    try:
        import packet_pb2  # type: ignore
    except ImportError:
        raise SystemExit(
            "✗ 未找到 `packet_pb2`(gem5 的 Packet protobuf 绑定)。\n"
            "  生成方式(注意 --proto_path,否则会生成嵌套目录):\n"
            "    protoc --python_out=build/parity --proto_path=external/gem5/src/proto \\\n"
            "      external/gem5/src/proto/packet.proto\n"
            "  (需先安装 protobuf-compiler;gem5 侧构建入口 = scripts/cep/parity/build_gem5.sh)"
        )

    header = packet_pb2.PacketHeader(obj_id=obj_id, tick_freq=tick_freq)

    def encode(msg):
        """Varint32 长度前缀 + 消息体。"""
        body = msg.SerializeToString()
        prefix = bytearray()
        _write_varint32(prefix, len(body))
        return bytes(prefix) + body

    # ① magic(小端 4B)
    out = bytearray()
    out += GEM5_PROTO_MAGIC.to_bytes(4, "little")
    # ② header(独立一条)
    out += encode(header)
    # ③ packet 序列
    #    ⚠️ `tick` 决定回放时刻 ⇒ 每条递增。
    #    **间隔必须足够大**:gem5 `TraceGen::nextPacketTick()` 返回
    #    `max(tickOffset + nextElement.tick, curTick())` ⇒ 间隔过密会让请求
    #    在 cache 的 MSHR(`mshrs=4`)上排队 ⇒ 前面的请求迟迟拿不到响应 ⇒
    #    后面的请求被 `max_outstanding_reqs` / MSHR 阻塞 ⇒ 事件循环看似卡死。
    #    ⇒ 默认 **10000 tick = 10ns** @1THz(远大于 miss 延迟 ~48ns?否 ⇒ 仍偏密,
    #       故取 100000 tick = 100ns,见 `--tick-interval`)。
    tick = 0
    for addr, is_write, size in accesses:
        pkt = packet_pb2.Packet(
            tick=tick,
            # ⚠️ `cmd` 取值必须是 gem5 的 **`MemCmd` 枚举值**
            #    (per `src/mem/packet.hh`:`InvalidCmd=0, ReadReq=1, ReadResp=2,
            #     ReadRespWithInvalidate=3, WriteReq=4, ...`)
            #    ⇒ 早期版本用「0=读 / 1=写」⇒ 0 落成 `InvalidCmd`
            #    ⇒ gem5 断言 `nextElement.isValid()` 失败而 abort。
            cmd=4 if is_write else 1,  # ReadReq=1 / WriteReq=4
            addr=addr,
            size=size,
            pkt_id=0,
            pc=0,
        )
        out += encode(pkt)
        tick += tick_interval

    with open(path, "wb") as f:
        f.write(bytes(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="输出基名(产出 <out>.csv 与 <out>.pb)")
    ap.add_argument("--seed", type=int, default=42, help="LCG 种子(默认 42)")
    ap.add_argument("--num-accesses", type=int, default=100000,
                    help="访问条数(主对拍口径要求 ≥ 1e5)")
    ap.add_argument("--working-set", type=int, default=65536, help="工作集字节数")
    ap.add_argument("--base-addr", type=lambda s: int(s, 0), default=0x100000,
                    help="工作集基址")
    ap.add_argument("--line-size", type=int, default=8,
                    help="访问大小(字节)—— ⚠️ **必须 < gem5 的 cache line(64B)**;"
                         "等于 line 大小时 `Packet::isWholeLineWrite()` 为真"
                         "(per `src/mem/packet.hh`:cmd==WriteReq && offset==0 && size==blk_size)"
                         "⇒ cache 把整行写转成 `MemCmd::InvalidateReq` ⇒ `SimpleMemory` panic")
    ap.add_argument("--write-percent", type=float, default=20.0, help="写访问占比 %%")
    ap.add_argument("--profile", default="uniform", choices=["uniform", "hot_cold"],
                    help="访问分布形态:uniform(均匀,规则 R-B 最优 ⇒ 证伪不了)/"
                         "hot_cold(双峰,**规则失效型** ⇒ 满足 P-2a 的 A-1 鉴别力自检)")
    ap.add_argument("--hot-bytes", type=int, default=4096,
                    help="hot_cold 形态下的热区字节数(应 ≤ cache 容量)")
    ap.add_argument("--cold-bytes", type=int, default=1048576,
                    help="hot_cold 形态下的冷区字节数(应 ≫ cache 容量)")
    ap.add_argument("--hot-ratio", type=float, default=0.5,
                    help="hot_cold 形态下访问热区的比例(⇒ 实际命中率 ≈ hot_ratio)")
    ap.add_argument("--obj-id", default="parity_trace",
                    help="gem5 protobuf 头里的 obj_id")
    ap.add_argument("--tick-freq", type=int, default=1000000000000,
                    help="gem5 tick 频率(1THz,与 build/NULL 的全局频率一致)")
    ap.add_argument("--tick-interval", type=int, default=100000,
                    help="相邻访问的 tick 间隔(默认 100000 = 100ns @1THz)—— "
                         "**过密会让请求堆在 cache 的 MSHR 上 ⇒ 事件循环看似卡死**")
    args = ap.parse_args()

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    csv_path = out.with_suffix(".csv")
    pb_path = out.with_suffix(".pb")

    if args.profile == "hot_cold":
        accesses = list(gen_accesses_hot_cold(
            args.seed, args.num_accesses, args.hot_bytes, args.cold_bytes,
            args.base_addr, args.line_size, args.write_percent, args.hot_ratio,
        ))
    else:
        accesses = list(gen_accesses(
            args.seed, args.num_accesses, args.working_set, args.base_addr,
            args.line_size, args.write_percent,
        ))

    write_csv(csv_path, accesses)
    write_protobuf(pb_path, accesses, args.obj_id, args.tick_freq, args.tick_interval)

    # ③ **meta.json** —— trace 的**元数据单一真源**:
    #    消费侧(gem5 探针 / 比较器)**只**从这里读「条数 / tick 间隔」,
    #    **不**从配置值猜 ⇒ 避免「配置说 20000、文件里其实 1000 条」这类漂移
    #    (早期版本正踩过:探针的 `progress_check` 用配置值算 ⇒ 与实际 trace 不符
    #     ⇒ trace 放大到 1e5 条时会撞上 `progress_check` 而 fatal)。
    meta_path = out.with_suffix(".meta.json")
    meta = {
        "num_accesses": len(accesses),
        "tick_interval": args.tick_interval,
        "tick_freq": args.tick_freq,
        "seed": args.seed,
        "profile": args.profile,
        "working_set_bytes": args.working_set,
        "hot_bytes": args.hot_bytes,
        "cold_bytes": args.cold_bytes,
        "hot_ratio": args.hot_ratio,
        "base_addr": args.base_addr,
        "access_size": args.line_size,
        "write_percent": args.write_percent,
        "csv": csv_path.name,
        "pb": pb_path.name,
        "csv_sha256": hashlib.sha256(csv_path.read_bytes()).hexdigest(),
        "pb_sha256": hashlib.sha256(pb_path.read_bytes()).hexdigest(),
    }
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")

    for p in (csv_path, pb_path, meta_path):
        data = p.read_bytes()
        print(f"[trace] {p} · {len(accesses)} 条 · sha256={hashlib.sha256(data).hexdigest()[:16]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
