#!/usr/bin/env bash
# scripts/cep/parity/build_gem5.sh
#   —— **gem5 对拍基准的构建入口**(本仓侧可复现脚本;支持**多版本**)
#
# 目的:把「第三方对拍基准 = gem5」从「一次性手工操作」变成**可复现的项目构建步骤**
#       (per 台账 Entry #113 / `docs/architecture/cep/parity-baseline.md` §3.1:
#        主基准 = gem5 ⇒ 满足 M3「对拍基准外置」—— 第三方既有资产,本仓不维护其实现)。
#
# ── 版本钉版(可复现性硬要求)──────────────────────────────────────────
#   gem5 **v24.1.0.0**
#     tarball : https://codeload.github.com/gem5/gem5/tar.gz/refs/tags/v24.1.0.0
#     sha256  : ef5373ec9e514a15dbb1cc6dbc6771182771f1ec48ece1123e0beb876eca8b32
#     目录    : external/gem5
#   gem5 **v25.0.0.0**  ← **文件同源路径可用**(见下)
#     tarball : https://codeload.github.com/gem5/gem5/tar.gz/refs/tags/v25.0.0.0
#     sha256  : 99c942cbc3711cc280545f35f013cfafbcb43a791cd5aebf7d03ac391ba74724
#     目录    : external/gem5-v25
#   ⚠️ **不用 submodule**:github 大传输在本机 TLS 中断(`GnuTLS recv error (-110)`),
#      `git ls-remote` 可通但 `git clone` 失败 ⇒ 改用 codeload tarball + SHA256 钉版。
#      语义等价(submodule 也是钉 commit),但**更抗网络故障**且**更易校验**。
#
# ── 版本选择(⚠️ **2026-10-31 更正**,per 台账 Entry #120)───────────────
#   此前本节写的是「**v24 的 `ProtoInputStream` 有提前 EOF 缺陷** ⇒ 文件同源不可用
#   ⇒ 需 v25;v25 已重写 `protoio.cc` ⇒ 根因消除」—— **该结论已被实测推翻**:
#   · C++ 复现程序按 gem5 **完全相同**的读法(`ReadVarint32`+`PushLimit`+
#     `ParseFromCodedStream`)可读满 1000/1001 条;
#   · gem5 内 DIAG 显示 `read` **从未失败**;
#   · **上游原版 v25(零本地补丁)** 可读满 **100000/100000** 条。
#   ⇒ **真根因 = 对拍探针的 tick 预算不足**(名义 tick 间隔 vs 实际每条含背压)
#     ⇒ 与 gem5 版本**无关** ⇒ **v24 同样可用**(未复测,属推断)。
#   ⇒ **v25 的价值 = 0**(在本路径上);保留 v25 作为默认是因为**已构建并验证过**,
#     而非因为它「修好了什么」。
#   ⇒ **解封 H7 的真实前提** = 「能喂**非均匀离线 trace**」⇒ 只需 `createTrace`
#     能读 trace 文件(v24 即支持)⇒ **与版本解耦**。
#   ⚠️ **本仓当前对 v25 源码零本地补丁**(2026-10-31 已还原上游原版并重建验证)
#      ⇒ `diff -rq` 与 tarball 一致;若将来需要打补丁,**必须**在本文件登记
#      补丁标记 + 理由(可复现性要求)。
#
# ── 版本差异(实测核对)───────────────────────────────────────────────
#   · `MemCmd` 枚举 **v24 与 v25 一致**(`InvalidCmd=0, ReadReq=1, ReadResp=2,
#     ReadRespWithInvalidate=3, WriteReq=4`)⇒ `Packet.cmd` 取值**无需改动**;
#   · v25 的 `src/base/random.cc` **已含 `#include <algorithm>`** ⇒ **不需要本地补丁**
#     (v24 需要,已登记在下方以备回退)。
#
# ── 变体 ────────────────────────────────────────────────────────────
#   `build/NULL/gem5.opt` —— gem5 v24/v25 走 **Kconfig**,`build_opts/` 下**没有 `RUBY`**
#   (实测 24 个变体,含 `NULL` / `NULL_ALL_RUBY` / `NULL_MESI_Two_Level` 等,**无 `RUBY`**);
#   `build/NULL` 的 defconfig = `RUBY=y` + `PROTOCOL="MI_example"` ⇒ **含 Ruby 协议栈**。
#
# 用法:
#   bash scripts/cep/parity/build_gem5.sh              # 构建默认版本(config 指定)
#   bash scripts/cep/parity/build_gem5.sh --version v25.0.0.0
#   bash scripts/cep/parity/build_gem5.sh --check      # 只校验现状(供 CI / ctest 用)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CFG="$ROOT/scripts/cep/parity/parity_config.json"

log()  { printf '[build_gem5] %s\n' "$*"; }
fail() { printf '[build_gem5] ✗ %s\n' "$*" >&2; exit 1; }

# ── 版本表(唯一真源 = parity_config.json 的 `gem5.versions`)────────────
read_cfg() {
    python3 -c "
import json
v = json.load(open('$CFG'))['gem5']['versions']
print(v['$1']['$2'])
"
}

VERSION="$(read_cfg default version)"
if [[ "${1:-}" == "--version" ]]; then
    VERSION="$2"
    shift 2
fi
# ⚠️ **绝对路径**(D-4 同源的口径问题):`--check` 会被 ctest 从 `build/` 目录调用
#    ⇒ 相对路径会解析到 `build/external/...` ⇒ 误报「二进制不存在」⇒ e2e 假红。
#    ⇒ 全部以仓库根 `$ROOT` 为基准的**绝对路径**。
GEM5_DIR="$ROOT/$(read_cfg "$VERSION" dir)"
GEM5_TARBALL_URL="$(read_cfg "$VERSION" tarball_url)"
GEM5_SHA256="$(read_cfg "$VERSION" sha256)"
GEM5_VARIANT="${GEM5_VARIANT:-NULL}"
GEM5_JOBS="${GEM5_JOBS:-4}"
GEM5_BUILD_DIR="$GEM5_DIR/build/$GEM5_VARIANT"

# ⚠️ v24 需要本地补丁(GCC13):上游 `src/base/random.cc` 用 `std::remove_if` 却未 include
#    `<algorithm>` ⇒ **上游源码缺陷**(非本仓改动)⇒ 以**本地补丁**形式应用并登记。
GEM5_PATCH_MARK="// PATCH(gem5-v24.1.0.0 + GCC13): std::remove_if 需要 <algorithm>"

check_mode=0
[[ "${1:-}" == "--check" ]] && check_mode=1

# ── ① 检查构建产物 ───────────────────────────────────────────────────
if [[ $check_mode -eq 1 ]]; then
    bin="$GEM5_BUILD_DIR/gem5.opt"
    if [[ ! -x "$bin" ]]; then
        fail "gem5 二进制不存在或不可执行:$bin(请先跑本脚本的构建模式)"
    fi
    ver="$(strings "$bin" 2>/dev/null | grep -oE '24\.1\.0\.0|25\.0\.0\.0' | head -1 || true)"
    log "✓ gem5 二进制就绪:$bin"
    log "  版本标识:${ver:-未知} · 变体:$GEM5_VARIANT"
    exit 0
fi

# ── ② 依赖检查(缺失则给出可执行的安装指引)───────────────────────────
if ! python3 -c 'import SCons' >/dev/null 2>&1; then
    log "安装 SCons(注意版本!见下)"
    # ⚠️ **SCons 4.11.1 不可用**:其 `SConf.CheckLibWithHeader` 传 `call=` 给
    #    `Conftest.CheckLib`,而后者的签名不接受该关键字 ⇒ 检查被**吞成 False**
    #    ⇒ gem5 报「Did not find needed zlib compression library」(假阴性)。
    # ⇒ 必须钉 **scons==4.5.2**。
    python3 -m pip install "scons==4.5.2"
fi
if ! command -v protoc >/dev/null 2>&1; then
    fail "缺 protoc(gem5 的 protobuf trace 支持需要它)⇒ sudo apt-get install -y protobuf-compiler libprotobuf-dev"
fi

# ── ③ 取源码(tarball + SHA256 钉版)──────────────────────────────────
if [[ ! -d "$GEM5_DIR/src" ]]; then
    tmp="$(mktemp -d /tmp/opencode/gem5-dl-XXXXXX)"
    log "下载 gem5 $VERSION ..."
    curl -sSL -o "$tmp/gem5.tar.gz" "$GEM5_TARBALL_URL" || fail "下载失败:$GEM5_TARBALL_URL"
    got="$(sha256sum "$tmp/gem5.tar.gz" | cut -d' ' -f1)"
    [[ "$got" == "$GEM5_SHA256" ]] || fail "SHA256 不匹配:期望 $GEM5_SHA256 实得 $got"
    log "✓ SHA256 校验通过:$GEM5_SHA256"
    tar xzf "$tmp/gem5.tar.gz" -C "$tmp"
    mkdir -p "$(dirname "$GEM5_DIR")"
    rm -rf "$GEM5_DIR"
    mv "$tmp/gem5-${VERSION#v}" "$GEM5_DIR"
    rm -rf "$tmp"
fi

# ── ④ 应用本地补丁(幂等;**仅 v24 需要**)─────────────────────────────
rnd="$GEM5_DIR/src/base/random.cc"
if [[ -f "$rnd" ]] && ! grep -qF "$GEM5_PATCH_MARK" "$rnd" \
   && ! grep -qF "#include <algorithm>" "$rnd"; then
    log "应用 GCC13 兼容性补丁:$rnd"
    python3 - "$rnd" "$GEM5_PATCH_MARK" <<'PYEOF'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
s = p.read_text(encoding="utf-8")
i = s.index("#include")
p.write_text(s[:i] + sys.argv[2] + "\n#include <algorithm>\n" + s[i:], encoding="utf-8")
PYEOF
fi

# ── ⑤ 构建 ─────────────────────────────────────────────────────────
log "构建 $VERSION / $GEM5_VARIANT 变体(并行度 $GEM5_JOBS)—— 首次约 30~90 min ..."
(cd "$GEM5_DIR" && scons "build/$GEM5_VARIANT/gem5.opt" -j"$GEM5_JOBS" --ignore-style) \
    || fail "gem5 构建失败(见上)"
[[ -x "$GEM5_BUILD_DIR/gem5.opt" ]] || fail "构建结束但二进制不存在"
log "✓ 完成:$GEM5_BUILD_DIR/gem5.opt"
