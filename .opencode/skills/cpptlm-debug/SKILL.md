---
name: cpptlm-debug
description: Systematically debug CppTLM test failures using file-based logging, 5-layer diagnostic funnel, 4-step fix verification, and 6-step response loop template. Use when test fails, fix appears not to work, or message-passing chain breaks. Triggers on phrases like "test fail", "fix doesn't work", "X not received", "response lost", "where is it failing", "diagnose".
license: MIT
metadata:
  author: CppTLM Team
  version: "1.0"
  source: "P0-5b complete fix (PR #16, 2026-06-14)"
  rootCauses: 6
---

# CppTLM Systematic Debugging

**Source**: P0-5b complete fix found 6 independent root causes via this methodology.
**Test improvement**: 602/604 → 604/604 by applying these techniques.

---

## When to Use This Skill

**Auto-trigger** when you encounter:
- Test fails but you don't know which step in the chain is broken
- Fix is applied, build succeeds, test still fails
- "X is sent but Y never receives it" (message passing)
- Multi-port / multi-module response path issues
- StreamAdapter / ChStreamPort / PacketPool behavior is suspicious
- User asks "diagnose", "find the bug", "why is X failing"

**Don't use** for:
- Compilation errors (use normal error reading)
- Simple typo fixes
- "Where is X defined" (use Grep/LSP)

---

## Phase 0 — Decide: Diagnose vs Guess-Fix

**HARD RULE**: Do NOT start modifying code without diagnosis.

```
if "test fail" and "no clear root cause" {
    STOP. Don't try-fix loops.
    Switch to file-based logging.
    Apply Phase 1.
}
```

**Anti-pattern** (costly): 改代码 → 跑 test → fail → 改代码 → 跑 test → fail
**Correct pattern** (cheap): 加日志 → 一次 build → 看全貌 → 定位根因 → 修

---

## Phase 1 — 5-Layer Diagnostic Funnel

Work top-down, **one hypothesis at a time**:

```
L1 (黑盒) — 事件有没有发生?
    ↓ grep test output for "expected X, got Y"
L2 (灰盒) — 链路第 1 步调用了吗?
    ↓ 在入口函数加 count log
L3 (白盒) — 内部状态值是多少?
    ↓ log "valid=%d, data=%p, len=%u" 等
L4 (对比) — A 路径通, B 路径不通,差异在哪?
    ↓ 拆差异,聚焦差异点
L5 (验证) — 修复真的在 binary 里吗?
    ↓ 4-step verification (Phase 2)
```

**每个 L 只问一个假设,验证一个检查点**。不要在 L2 同时验证 L3 L4。

---

## Phase 2 — 4-Step Fix Verification (after any code change)

**MUST run all 4** before claiming "fix is in":

```bash
# Step 1: 改对位置了吗?
git diff --stat <file>

# Step 2: binary 新于源文件?
stat -c '%y %n' build/bin/cpptlm_tests src/xxx.cc
# Expected: src.xxx.cc mtime ≥ binary mtime

# Step 3: 修复 marker 在 binary 里?
strings build/bin/cpptlm_tests | grep -c "<unique_marker>"
# Expected: ≥ 1

# Step 4: 修复真的在执行?
# 在修复前后各加 log,跑 test,对比输出
```

**Critical**: `time cmake --build build` 看实际耗时,**build 中断后必须 rebuild**,跑旧 binary = 自欺欺人。

---

## Phase 3 — File-Based Logging Template

**WHY**: Test framework (Catch2) suppresses stdout/stderr. `printf` 在测试中看不到。

**Template**:
```cpp
// 在函数入口/关键检查点
static FILE* diag = fopen("/tmp/cpputlm_<feature>.log", "a");
if (diag) {
    fprintf(diag, "[<FuncName>] <context>: <vars>=%d <more>=%p\n", ...);
    fflush(diag);  // ← 必须!否则 crash 时丢日志
}
```

**Path convention**: `/tmp/cpputlm_<feature>_<step>.log`
- `cpputlm_recvreq.log` — ChStreamTargetPort::recvReq 调用
- `cpputlm_simpleport_send.log` — SimplePort::send 行为
- `cpputlm_xbar_tick.log` — CrossbarTLM::tick 状态
- `cpputlm_isa_process.log` — InputStreamAdapter::process

**Cleanup** (MUST run after test passes):
```bash
grep -l "static FILE\* diag" include/ -r
# 找到所有诊断文件,edit 移除 fopen/fprintf 块
```

---

## Phase 4 — 6-Step Response Loop Template

When "A → B → A" message chain breaks, add count log at **each of 6 steps**:

```cpp
// Step 1: A 发送请求
// In A's adapter tick():
fprintf(diag, "[A.send] count=%lu valid=%d port=%p\n", count, valid, port);

// Step 2: B 接收请求
// In ChStreamTargetPort::recvReq on B's req_in:
fprintf(diag, "[B.recvReq] name=%s pkt=%p type=%d\n", name, pkt, type);

// Step 3: B 写响应
// In B's tick() after processing:
fprintf(diag, "[B.writeResp] valid=%d data=%lu\n", valid, txn_id);

// Step 4: B 发送响应
// In B's adapter tick() (send call):
fprintf(diag, "[B.sendResp] out=%p sent=%d\n", out, sent);

// Step 5: A 接收响应
// In ChStreamTargetPort::recvReq on A's resp_in:
fprintf(diag, "[A.recvResp] name=%s pkt=%p type=%d\n", name, pkt, type);

// Step 6: A 消费响应
// In A's tick() consuming:
fprintf(diag, "[A.consumeResp] valid=%d txn=%lu\n", valid, txn_id);
```

**Which step has count=0 = root cause location**.

---

## Phase 5 — Multi-Root Cause Analysis

**Pattern**: 修 1 个 bug → test 仍 fail,但失败模式变了 = 还有根因。

**Strategy**:
```
if "fix 1 didn't fully resolve" {
    1. DON'T try-fix loop again
    2. Add logs at ALL 6 response loop steps + key internal states
    3. ONE build, observe all failure modes simultaneously
    4. Categorize failures by WHICH step they fail at
    5. Each category = 1 independent root cause
    6. Design N fixes together (independent or ordered)
}
```

**Real example from P0-5b** (6 root causes discovered sequentially):
| # | Fix attempt | New symptom | Real root cause |
|---|-------------|-------------|-----------------|
| 1 | PacketPool data_length | port->send still 0 | (real) payload len=0 |
| 2 | port_idx field | recvReq 0 for port 1-3 | (real) type-size mismatch |
| 3 | unsigned type | same symptom | (real) response path size check |
| 4 | vector size check | resp_in 0 | (real) send() clears valid |
| 5 | valid preservation | still fail | (real) one more type issue |
| 6 | full fix | ALL PASS | — |

**Lesson**: When sequential fix-reveal cycle happens, **add ALL diagnostic points upfront**, not one-at-a-time. Each build is 5+ minutes.

---

## Phase 6 — C++ Specific Gotchas

### Gotcha 1: virtual override type matching
```cpp
class Base { virtual void f(int); };
class Derived : public Base { void f(unsigned); };  // ❌ HIDE, not override
```
- **Always** use `= override` keyword to force compile-time check
- `std::size_t` ≠ `unsigned` on 64-bit Linux (size_t = unsigned long)
- `int` ≠ `long` ≠ `unsigned` ≠ `unsigned long`

### Gotcha 2: unordered_map::count semantics
```cpp
unordered_map<string, vector<X>> m;
m.count(key);          // returns 0 or 1 (key existence)
m.count(key) > idx;    // ❌ wrong — only true when idx=0
m[key].size() > idx;   // ✅ correct
```

### Gotcha 3: reset() ordering
```cpp
acquire() {
    payload->reset();     // reset #1
    pkt->reset();         // internally calls payload->reset() again
    set_data_length(256); // ❌ placed BEFORE pkt->reset, gets wiped
}
```
- Fix MUST be after the LAST reset call
- Better design: `Packet::reset` shouldn't reset payload

### Gotcha 4: port->send return value
```cpp
bool sent = port->send(pkt);
if (sent) {
    stream_valid_ = false;  // only clear on success
    return true;
}
// failed: keep valid, release packet, return false
```

---

## Phase 7 — Architectural Patterns (CppTLM Specific)

### PortPair binding rules
- MasterPort = Initiator (sends req, receives resp via `recvResp`)
- SlavePort = Target (receives req via `recvReq`, sends resp)
- `PortPair(a, b)` → `a.bind(this, 0)`, `b.bind(this, 1)`
- `SimplePort::send`: `other = (my_side == 0) ? side_b : side_a; other->recv(pkt)`

### StreamAdapter 3-layer abstraction
```
ChStreamModuleBase    ← module interface (req_in/req_out/resp_in/resp_out)
  ↓ via
ChStreamAdapterFactory  ← registers multi-port template instances
  ↓ creates
StreamAdapterBase*    ← runtime polymorphic adapter
  ↓ holds
OutputStreamAdapter<BundleT> / InputStreamAdapter<BundleT>  ← ch_stream protocol
```

**Trap**: Multi-port ChStreamTargetPort needs `port_idx` to know which port received the request.

### Packet type routing
```cpp
enum PacketType { PKT_REQ, PKT_RESP, PKT_STREAM_DATA, PKT_CREDIT_RETURN };
```
- `pkt->type == PKT_REQ` → route to `req_in` adapter
- `pkt->type == PKT_RESP` → route to `resp_in` adapter
- **Single-arg `process_request_input(pkt)` always routes to port_idx=0** (legacy bug)

---

## Diagnostic Script Templates

### Template 1: Verify fix is in binary
```bash
FIX_MARKER="P0-5b-fix-marker-string"
strings build/bin/cpptlm_tests | grep -c "$FIX_MARKER"
# Expected: ≥ 1
```

### Template 2: Count events at each response step
```bash
for log in /tmp/cpputlm_*.log; do
    echo "=== $log ==="
    grep -c "key_event" "$log"
done
```

### Template 3: Cross-reference two logs
```bash
comm -12 <(grep -oE "0x[0-9a-f]+" log1 | sort -u) \
        <(grep -oE "0x[0-9a-f]+" log2 | sort -u)
```

### Template 4: Find unbound (NO-PAIR) ports
```bash
grep "NO PAIR" /tmp/diag.log | grep -oE "this=0x[0-9a-f]+" | sort | uniq -c
```

---

## Anti-Patterns (DO NOT)

- ❌ Try-fix loop without diagnosis
- ❌ Use `printf` for debug (suppressed by test framework)
- ❌ Skip 4-step verification (especially Step 2 binary timestamp)
- ❌ Commit diagnostic code (cleanup IMMEDIATELY after test pass)
- ❌ Assume "1 root cause explains everything"
- ❌ Add logs one-at-a-time across multiple builds (waste 5+ min/build)
- ❌ Override virtual function without `= override` keyword
- ❌ Modify `unordered_map::count() > N` as size check

---

## Success Criteria

- [ ] Test passes (asserted by run, not by build)
- [ ] All 4 verification steps pass
- [ ] All diagnostic code removed (`grep -l "static FILE\* diag" include/ -r` returns empty)
- [ ] PR created with clear "N root causes" framing
- [ ] CI green (all 4 checks: Debug/ASan, Debug, Release, code-format)
- [ ] Local final test count: `All tests passed (N assertions in M test cases)`
