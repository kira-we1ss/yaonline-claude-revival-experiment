---
name: brainstorm
description: Structured brainstorming for technical problems — explore multiple approaches, trade-offs, and pick the best path before writing any code
license: MIT
compatibility: opencode
---

## What I do

When loaded, I help you think through a problem **before** diving into implementation.

1. **Restate the problem** — confirm understanding of what needs to be solved
2. **Generate 3–5 candidate approaches** — different angles, not just variations on one idea
3. **Analyze trade-offs** — for each approach: pros, cons, complexity, risk
4. **Recommend one path** — with clear reasoning, accounting for the project's constraints
5. **Identify unknowns** — flag anything that needs investigation before coding starts

## When to use me

- You have a non-trivial bug where the root cause isn't obvious
- You need to add a feature and there are multiple valid architectures
- You're about to do a large refactor and want to validate the approach
- You're stuck and need to think out loud

## How to invoke

Just describe the problem. I'll run through the framework above.

Example:
> "The XMPP reconnect logic causes an infinite loop for non-Yandex domains. Brainstorm approaches to fix this without breaking Yandex PDD support."

## Output format

- Numbered list of approaches
- Pros/cons table or inline trade-off notes
- Bold **Recommendation** section
- Bulleted **Unknowns / Risks** section
