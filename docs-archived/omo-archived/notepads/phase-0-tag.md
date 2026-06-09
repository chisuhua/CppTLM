# Phase 0 Findings: Tag v2.1.0-pre-cleanup + CHANGELOG Init

**Date:** 2026-06-05
**Branch:** main
**Status:** COMPLETE

## Tag Operations

### Created
- Tag: v2.1.0 (annotated)
- Message: 功能快照：升级前保留点
- Tagger: Chi Suhua <chisuhua@gmail.com>
- Commit: 255b4800070556c881272f9a09be6c654165960c (add ONBOARDING.md)
- Push result: SUCCESS - new tag v2.1.0 -> v2.1.0 (EXIT_CODE=0)
- Remote verified: git ls-remote --tags origin shows tag + peeled commit

## CHANGELOG

### Created File
- Path: /workspace/project/CppTLM/CHANGELOG.md (8 lines, 8 insertions)
- Content: Matches spec verbatim (Unreleased section, 3 pending items)

### Commit
- Hash: 195ba81
- Message: chore(release): tag v2.1.0-pre-cleanup + initial CHANGELOG
- Style: SEMANTIC (matches repo's dominant type(scope): convention)
- Position: Ahead of v2.1.0 tag by 1 commit (intentional)

## Verification Results

| Check | Expected | Actual | Pass |
|-------|----------|--------|------|
| git tag -l v2.1* | v2.1.0 | v2.1.0 | YES |
| git log v2.1.0 -1 --format=H | valid hash | 255b4800070556c881272f9a09be6c654165960c | YES |
| git push origin v2.1.0 | success | EXIT_CODE=0 | YES |
| CHANGELOG.md exists at root | yes | yes (8 lines) | YES |
| git status clean (modulo untracked) | clean | only .understand-anything/ + build-asan/ untracked | YES |

## Important Sequencing Note

- v2.1.0 tag -> commit 255b4800 (pre-cleanup snapshot)
- HEAD -> 195ba81 (CHANGELOG commit, AHEAD of tag)

This sequencing is intentional per the task design: the tag freezes the pre-upgrade code state; CHANGELOG.md is the first artifact in the upgrade pipeline.

## Local State After Phase 0

```
main branch: 195ba81 (HEAD) <- 255b4800 (tag: v2.1.0) <- 14acd9a <- ...
origin/main: 255b4800 (1 commit behind - CHANGELOG not yet pushed to main)
```

The CHANGELOG commit is NOT pushed to origin/main (only the tag was pushed). Pushing the main commit is intentionally deferred - Phase 1a+ will determine whether to amend or add follow-up commits.

## Untracked Files (Pre-existing, Not Touched)

- .understand-anything/ - knowledge graph artifacts
- build-asan/ - AddressSanitizer build output

Per task spec: git status clean (except original untracked .understand-anything/ build-asan/)

## Next Phase Unblocked

Phase 1a can begin. The tag v2.1.0 provides a stable rollback point for any upgrade-time changes (tlm_stub multi-extension, USE_SYSTEMC removal, MemoryV2 fix).
