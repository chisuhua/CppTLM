#!/usr/bin/env bash
# scripts/cep/parity/build_gem5.sh
#   —— **gem5 对拍基准的构建入口**(本仓侧可复现脚本)
#
# 目的:把「第三方对拍基准 = gem5」从「一次性手工操作」变成**可复现的项目构建步骤**
#       (per 台账 Entry #113 / `docs/architecture/14-coherence-parity-baseline.md` §3.1:
#        主基准 = gem5 ⇒ 满足 M3「对拍基准外置」—— 第三方既有资产,本仓不维护其实现)。
#
# ── 版本钉死(可复现性硬要求)──────────────────────────────────────────
#   gem5 版本 : v24.1.0.0
#   tarball   : https://codeload.github.com/gem5/gem5/tar.gz/refs/tags/v24.1.0.0
#   sha256    : ef5373ec9e514a15dbb1cc6dbc6771182771f1ec48ece1123e0beb876eca8b32
#   ⚠️ **不用 submodule**:github 大传输在本机 TLS 中断(`GnuTLS recv error (-110)`),
#      `git ls-remote` 可通但 `git clone` 失败 ⇒ 改用 codeload tarball + SHA256 钉版。
#      语义等价(submodule 也是钉 commit),但**更抗网络故障**且**更易校验**。
#
# ── 为什么变体是 `build/NULL` 而不是 `build/RUBY` ─────────────────────
#   `docs/architecture/14-...` §2/§3 写的是 `scons build/RUBY/gem5.opt`,
#   但 **gem5 v24 走 Kconfig**,`build_opts/` 下**没有 `RUBY` 变体**
#   (实测 `ls build_opts/` ⇒ 24 个变体,含 `NULL` / `NULL_ALL_RUBY` /
#    `NULL_MESI_Two_Level` / `X86_MESI_Two_Level` 等,**无 `RUBY`**)。
#   `build/NULL` 的 defconfig = `RUBY=y` + `PROTOCOL="MI_example"`
#   ⇒ **含 Ruby 协议栈**(MI_example 协议)⇒ 是 v24 下**最贴近裁决意图**的可用变体。
#   ⚠️ 已知局限(必须诚实登记,见台账 Entry #113):
#      · MI_example 是 gem5 的**最简协议**(非 CHI / 非 CXL)⇒ 对拍的是
#        **classic cache 的路径延迟常数**,**不是** Ruby coherence 栈的协议开销;
#      · v24 classic cache **无命中延迟实测统计** ⇒ 命中延迟是
#        `tag+data+response` 的**解析式派生值**,不是实测值。
#      ⇒ 本档**只验证「口径与建模语义」**,**不验证 H7**(R7 不降级)。
#
# 用法:
#   bash scripts/cep/parity/build_gem5.sh            # 构建(缺依赖则先装)
#   bash scripts/cep/parity/build_gem5.sh --check    # 只校验现状(供 CI / ctest 用)
#
# 环境变量:
#   GEM5_DIR      gem5 源码目录(默认 external/gem5)
#   GEM5_VARIANT  构建变体(默认 NULL)
#   GEM5_JOBS     并行度(默认 4)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
GEM5_DIR="${GEM5_DIR:-$ROOT/external/gem5}"
GEM5_VARIANT="${GEM5_VARIANT:-NULL}"
GEM5_JOBS="${GEM5_JOBS:-4}"

GEM5_VERSION="v24.1.0.0"
GEM5_TARBALL_URL="https://codeload.github.com/gem5/gem5/tar.gz/refs/tags/${GEM5_VERSION}"
GEM5_SHA256="ef5373ec9e514a15dbb1cc6dbc6771182771f1ec48ece1123e0beb876eca8b32"
GEM5_BUILD_DIR="$GEM5_DIR/build/$GEM5_VARIANT"

# ── 本地源码补丁登记(GCC 13 兼容性)───────────────────────────────────
# gem5 v24.1.0.0 的 `src/base/random.cc` 使用 `std::remove_if` 却未 include
# `<algorithm>` ⇒ GCC 13 下编译失败(`'remove_if' is not a member of 'std'`)。
# 这是**上游源码缺陷**而非本仓改动 ⇒ 以**本地补丁**形式应用,并在此登记。
GEM5_PATCH_MARK="// PATCH(gem5-v24.1.0.0 + GCC13): std::remove_if 需要 <algorithm>"

log()  { printf '[build_gem5] %s\n' "$*"; }
fail() { printf '[build_gem5] ✗ %s\n' "$*" >&2; exit 1; }

check_mode=0
[[ "${1:-}" == "--check" ]] && check_mode=1

# ── ① 检查构建产物 ───────────────────────────────────────────────────
if [[ $check_mode -eq 1 ]]; then
    bin="$GEM5_BUILD_DIR/gem5.opt"
    if [[ ! -x "$bin" ]]; then
        fail "gem5 二进制不存在或不可执行:$bin(请先跑本脚本的构建模式)"
    fi
    # ⚠️ gem5 v24 **无 `--version` 选项**(实测报 `no such option: --version`);
    #    版本号只在**运行时**的 banner 里打印 ⇒ 用 `--help` 触发不了,
    #    改为「跑一个最小脚本」或直接读二进制内嵌字符串。
    ver="$(strings "$bin" 2>/dev/null | grep -oE '^gem5 version [0-9]+\.[0-9]+\.[0-9]+' | head -1 || true)"
    [[ -z "$ver" ]] && ver="$(strings "$bin" 2>/dev/null | grep -oE '24\.1\.0\.0' | head -1 || true)"
    log "✓ gem5 二进制就绪:$bin"
    log "  版本标识:${ver:-未知}"
    log "  变体:$GEM5_VARIANT"
    exit 0
fi

# ── ② 依赖检查(缺失则给出可执行的安装指引)───────────────────────────
need_pip=0
python3 -c 'import SCons' >/dev/null 2>&1 || need_pip=1
if [[ $need_pip -eq 1 ]]; then
    log "安装 SCons(注意版本!见下)"
    # ⚠️ **SCons 4.11.1 不可用**:其 `SConf.CheckLibWithHeader` 传 `call=` 给
    #    `Conftest.CheckLib`,而后者的签名不接受该关键字 ⇒ 检查被**吞成 False**
    #    ⇒ gem5 报「Did not find needed zlib compression library」(假阴性)。
    # ⇒ 必须钉 **scons==4.5.2**。
    python3 -m pip install "scons==4.5.2"
fi

# ── ③ 取源码(tarball + SHA256 钉版)──────────────────────────────────
if [[ ! -d "$GEM5_DIR/src" ]]; then
    tmp="$(mktemp -d /tmp/opencode/gem5-dl-XXXXXX)"
    log "下载 gem5 $GEM5_VERSION ..."
    curl -sSL -o "$tmp/gem5.tar.gz" "$GEM5_TARBALL_URL" || fail "下载失败:$GEM5_TARBALL_URL"
    got="$(sha256sum "$tmp/gem5.tar.gz" | cut -d' ' -f1)"
    [[ "$got" == "$GEM5_SHA256" ]] || fail "SHA256 不匹配:期望 $GEM5_SHA256 实得 $got"
    log "✓ SHA256 校验通过:$GEM5_SHA256"
    tar xzf "$tmp/gem5.tar.gz" -C "$tmp"
    mkdir -p "$(dirname "$GEM5_DIR")"
    rm -rf "$GEM5_DIR"
    mv "$tmp/gem5-${GEM5_VERSION#v}" "$GEM5_DIR"
    rm -rf "$tmp"
fi

# ── ④ 应用本地补丁(幂等)────────────────────────────────────────────
rnd="$GEM5_DIR/src/base/random.cc"
if [[ -f "$rnd" ]] && ! grep -qF "$GEM5_PATCH_MARK" "$rnd"; then
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
log "构建 $GEM5_VARIANT 变体(并行度 $GEM5_JOBS)—— 首次约 30~60 min ..."
(cd "$GEM5_DIR" && scons "build/$GEM5_VARIANT/gem5.opt" -j"$GEM5_JOBS" --ignore-style) \
    || fail "gem5 构建失败(见上)"
[[ -x "$GEM5_BUILD_DIR/gem5.opt" ]] || fail "构建结束但二进制不存在"
log "✓ 完成:$GEM5_BUILD_DIR/gem5.opt"
