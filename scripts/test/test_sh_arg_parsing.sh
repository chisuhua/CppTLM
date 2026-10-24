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
# 本脚本断言 3 件事:
#   ① `--build-only --build` 仍**互斥报错**(显式冲突必须仍被拦)
#   ② `--test-only --build` 仍**互斥报错**
#   ③ `--build` **单独**使用时**不**报互斥错(解析阶段必须放行 → 用不存在的
#      BUILD_DIR 让它在**构建阶段**失败,以此证明解析阶段已通过)
#
# 用法:bash scripts/test/test_sh_arg_parsing.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

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

# ③ --build 单独:解析阶段必须**放行**(故意用不存在的 BUILD_DIR 让它在构建阶段失败)
out="$(BUILD_DIR=/tmp/opencode/__no_such_build_dir__ ./test.sh --mode off --build 2>&1)"
if printf '%s' "$out" | grep -qF "互斥"; then
  printf "  \033[0;31m✗\033[0m %-46s --build 单独使用仍报互斥错\n" "--build 单独 → 解析放行"
  FAIL=$((FAIL + 1))
else
  printf "  \033[0;32m✓\033[0m %-46s 解析阶段放行(失败点在构建阶段,符合预期)\n" "--build 单独 → 解析放行"
  PASS=$((PASS + 1))
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo -e "\033[0;32m✅ test.sh 参数解析回归锁全部通过\033[0m ($PASS 项)"
  exit 0
fi
echo -e "\033[0;31m❌ test.sh 参数解析回归锁有 $FAIL 项失败\033[0m (通过 $PASS 项)"
exit 1
