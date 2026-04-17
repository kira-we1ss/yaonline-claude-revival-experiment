---
name: code-review
description: Review a diff or set of changed files for correctness, Qt4→Qt5 regressions, memory safety, and code quality before committing
license: MIT
compatibility: opencode
---

## What I do

Review code changes with a focus on:

1. **Correctness** — does it actually fix/implement what it claims?
2. **Qt4→Qt5 regressions** — deprecated APIs, changed signal/slot semantics, model/view API differences, palette behavior
3. **Memory safety** — dangling pointers, use-after-free, QObject ownership issues (esp. `parent=0` + reparent patterns)
4. **Signal/slot ordering** — Qt5 changed destruction order; `qobject_cast` during `destroyed()` returns null; `static_cast` needed
5. **Proxy model safety** — Qt5 `QSortFilterProxyModel` requires source set before any query; `layoutChanged` wipes index mapping
6. **Thread safety** — signals crossing thread boundaries, `deleteLater` vs direct deletion
7. **Code quality** — unnecessary includes, dead code introduced, missing null guards

## When to use me

- After implementing a fix or feature, before committing
- When a subagent produced changes you want a second opinion on
- After a crash fix — to verify the fix is complete and doesn't introduce new issues

## Output format

- **Summary**: one sentence on what the change does
- **Verdict**: ✅ Approved / ⚠️ Approved with notes / ❌ Needs changes
- **Issues found**: numbered list with file:line references, severity (critical/major/minor)
- **Suggestions**: optional improvements that aren't blocking

## How to invoke

Pass the diff or describe what changed. I'll review it.

Example:
> "Review the fix in mucaffiliationsproxymodel.cpp — we moved setDynamicSortFilter into setSourceModel."
