#!/usr/bin/env bash
# scripts/test/test_sh_arg_parsing.sh —— `test.sh` 参数解析的回归锁
#
# 为什么需要它:2026-10-24 CI 实测发现 **`test.sh --build` 完全不可用** ——
#   `ACTION` 的默认值就是 `"test"`,而 `--build` 的互斥检查写的是
#   `if [[ "$ACTION" == "test" ]]` → 永远撞上「--build 与 --test-only 互斥」→
#   `--build` 恒报错(而 AGENTS.md 明写「./test.sh --build ≡ ./build.sh && ./test.sh」)。
#   CI 的 PTX-EMU job 正是在这里挂的(它需要 --build 才能先构建再测)。
#
# 修法:区分「**默认** ACTION=test」与「**显式** --test-only」→ 引入
#   `EXPLICIT_BUILD_ONLY` / `EXPLICIT_TEST_ONLY` 两个标志。
#
# 本脚本断言 4 件事:
#   ① `--build-only --build` 仍**互斥报错**(显式冲突必须仍被拦)
#   ② `--test-only --build` 仍**互斥报错**
#   ③ `--build` **单独**使用时**不**报互斥错(解析阶段必须放行 → 让它在
#      **配置阶段**失败,以此证明解析阶段已通过)
#   ④ ③ **不得产生任何构建产物**(否则该自检会拖到 1500s+ ⇒ 全量 ctest 超时)
#
# ⚠️ **③ 的 BUILD_DIR 为什么必须是「已存在的普通文件」而不是「不存在的目录」**
#   (2026-10-31 实测根因,per openspec change `cpptlm-test-sh-arg-parsing-timeout`):
#   · **不存在的目录** ⇒ cmake **创建**它并从零**全量编译 459 个目标**
#     ⇒ 该自检耗时 1500s+ ⇒ 全量 `ctest` 下**超时**(⇒ 「全量 ctest 全绿」不可证伪);
#     且每次残留一个半成品构建树。
#   · **已存在的文件** ⇒ cmake 在**配置阶段**立刻失败(秒级),
#     且失败输出**不含**「互斥」字样 ⇒ **③ 的断言语义保持不变**
#     (仍然证明「解析阶段放行」)。
#
# 用法:bash scripts/test/test_sh_arg_parsing.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

# ⚠️ 临时目录:优先仓内约定(`/tmp/opencode`)⇒ 不存在则回退到系统 TMPDIR
#    (该脚本会在**别的机器 / CI** 上跑 ⇒ 不能假定 `/tmp/opencode` 存在)。
_tmp_base="/tmp/opencode"
[ -d "$_tmp_base" ] || _tmp_base="${TMPDIR:-/tmp}"
# ⚠️ **探针前缀**:③ 的产物全部落在这个目录下 ⇒ ④ 靠扫描它来判定
#    「③ 是否产生了构建产物」(反向 fixture 的观测点)。
_probe_prefix="${_tmp_base}/__sh_arg_probe_$$"
rm -rf "$_probe_prefix"
mkdir -p "$_probe_prefix"

PASS=0
FAIL=0

check() {   # $1=名字 $2=期望子串(空=不要求) $3=命令
  local name="$1" want="$2"; shift 2
  local out rc
  out="$("$@" 2>&1)"; rc=$?
  # ⚠️ 必须 `grep -qF --`:期望消息以 `--` 开头,否则 grep 把它当选项(实测踩过)
  if [ -n "$want" ] && ! printf '%s' "$out" | grep -qF -- "$want"; then
    printf "  \033[0;31m✗\033[0m %-46s 未出现期望消息: %s\n" "$name" "$want"
    printf '%s\n' "$out" | head -3 | sed 's/^/      · /'
    FAIL=$((FAIL + 1)); return
  fi
  printf "  \033[0;32m✓\033[0m %-46s rc=%s\n" "$name" "$rc"
  PASS=$((PASS + 1))
}

echo -e "\033[1mtest.sh 参数解析 —— 回归锁\033[0m"

# ① 显式 --build-only 与 --build 互斥
check "--build-only --build → 互斥报错" \
      "--build 与 --build-only 互斥" \
      ./test.sh --mode off --build-only --build

# ② 显式 --test-only 与 --build 互斥
check "--test-only --build → 互斥报错" \
      "--build 与 --test-only 互斥" \
      ./test.sh --mode off --test-only --build

# ③ --build 单独:解析阶段必须**放行**
#    ⚠️ BUILD_DIR 指向一个**已存在的普通文件** ⇒ cmake 在**配置阶段**秒级失败
#       (若用「不存在的目录」⇒ cmake 会创建它并**全量编译 459 个目标** ⇒ 1500s+ 超时;
#        详见本文件头部说明)。
: > "$_probe_prefix/not_a_dir"    # 造一个**已存在的普通文件**(非目录)
out="$(BUILD_DIR="$_probe_prefix/not_a_dir" ./test.sh --mode off --build 2>&1)"
rc=$?
if printf '%s' "$out" | grep -qF "互斥"; then
  printf "  \033[0;31m✗\033[0;31m%s\033[0m --build 单独使用仍报互斥错\n" "--build 单独 → 解析放行"
  FAIL=$((FAIL + 1))
elif [ "$rc" -eq 0 ]; then
  printf "  \033[0;31m✗\033[0;31m%s\033[0m --build 单独使用**意外成功**\n" "--build 单独 → 解析放行"
  printf '%s\n' "$out" | head -3 | sed 's/^/      · /'
  FAIL=$((FAIL + 1))
else
  printf "  \033[0;32m✓\033[0m %-46s 解析阶段放行(失败点在配置阶段,符合预期) rc=%s\n" \
         "--build 单独 → 解析放行" "$rc"
  PASS=$((PASS + 1))
fi

# ④ **反向 fixture**:③ **不得产生任何构建产物**
#    ⚠️ 判据纪律铁律 ②「每条判据有可执行反向 fixture 且**因正确原因红**」——
#       本项观测 ③ 的副作用:若 ③ 的形态回退为「不存在的目录」,cmake 会**创建**
#       该目录 ⇒ 本项**因「产生了构建产物」而红**(⇒ 提醒形态已回退)。
#    ⇒ 探针目录下**只允许**有 ③ 自己造的普通文件,**不得**出现任何目录。
_probe_dirs="$(find "$_probe_prefix" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)"
if [ "$_probe_dirs" -gt 0 ]; then
  printf "  \033[0;31m✗\033[0;31m%s\033[0m ③ 产生了 %s 个构建目录 ⇒ 形态回退(会全量编译/超时)\n" \
         "③ 无构建产物" "$_probe_dirs"
  find "$_probe_prefix" -mindepth 1 -maxdepth 1 -type d | sed 's/^/      · /'
  FAIL=$((FAIL + 1))
else
  printf "  \033[0;32m✓\033[0m %-46s 探针目录下无构建目录 ⇒ 形态安全\n" "③ 无构建产物"
  PASS=$((PASS + 1))
fi
rm -rf "$_probe_prefix"

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo -e "\033[0;32m✅ test.sh 参数解析回归锁全部通过\033[0m ($PASS 项)"
  exit 0
fi
echo -e "\033[0;31m❌ test.sh 参数解析回归锁有 $FAIL 项失败\033[0m (通过 $PASS 项)"
exit 1
