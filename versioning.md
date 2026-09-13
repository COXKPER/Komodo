# Versioning

Komodo uses a **three-part version** managed here inside the Komodo tree.

## Format

```
(total-commit).(changes).(updates)
```

| Field   | Meaning                                                      | Source                                |
|---------|--------------------------------------------------------------|---------------------------------------|
| `total` | Total number of commits in the Komodo history                | `git rev-list --count HEAD` |
| `changes`| Number of feature changes since the last update bump         | Manual count across release notes     |
| `updates`| Maintenance releases (hotfixes, packaging, docs, backports) | Manual counter, incremented per release |

## Current version

```
630.0.0
```

- `total`   = 630 — the current count of `Komodo` commits
- `changes` = 0 — no feature changes consolidated yet
- `updates` = 0 — no maintenance release yet

## How to bump

1. **Feature / major change** → increment `changes`.
2. **Maintenance release** (hotfix, packaging, docs-only) → increment `updates`
   **and** snapshot `changes` into the release notes, then reset `changes` to `0`.
3. **Never** edit `total` manually — it is derived from `git rev-list --count`.

`KERNEL_VERSION` in `include/kernel/komodo.h` is updated in lockstep
with this file. During development the kernel simply reports the current
`total`, so the version always maps to a specific commit.

## Rationale

- `total` ties any running kernel directly to a git commit count, so the boot
  banner (`Komodo version <ver>`) is greppable against history.
- `changes`/`updates` give the project freedom to advertise feature growth
  without churning the commit count.
- A `checksum` slot was considered but deliberately omitted in software; the
  format stays three parts unless packaging later needs a content digest.