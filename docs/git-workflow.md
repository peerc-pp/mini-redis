# Git Configuration and Workflow

## 1. Verify Git

```bash
git --version
git config --global --get user.name
git config --global --get user.email
```

If identity is missing, configure it once for the current user:

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

Use the email associated with the GitHub account if commits should appear on that profile.

## 2. Recommended Global Defaults

```bash
git config --global init.defaultBranch master
git config --global fetch.prune true
git config --global rerere.enabled true
```

- `init.defaultBranch`: makes new repositories start on `master`.
- `fetch.prune`: removes local references to remote branches that were deleted.
- `rerere.enabled`: remembers previously resolved conflicts.

Do not set `core.autocrlf` globally just for this repository. The tracked `.gitattributes` file
already enforces LF for C++, CMake, Markdown, and shell files while preserving CRLF for Windows
scripts.

## 3. Project-Local Configuration

This repository uses:

```bash
git config --local core.autocrlf false
git config --local core.safecrlf warn
git config --local pull.rebase true
git config --local rebase.autoStash true
git config --local fetch.prune true
git config --local commit.template .gitmessage
```

Inspect the effective configuration and its source:

```bash
git config --list --show-origin
```

Configuration precedence is system < global < local. Local values only affect this repository.

## 4. Create the First Commit

Review before staging:

```bash
git status
git diff --check
```

Stage the scaffold and inspect exactly what will be committed:

```bash
git add .
git diff --cached --stat
git diff --cached
```

Create the initial commit:

```bash
git commit -m "chore: initialize project scaffold"
```

Do not use `git add .` mechanically in later work. Prefer explicit files or `git add -p`.

## 5. Establish Permanent Branches

After the first commit exists:

```bash
git switch -c develop
```

The repository then has:

- `master`: releasable history.
- `develop`: integrated development history.
- `feature/*`: short-lived feature branches.
- `bugfix/*`: normal bug fixes.
- `hotfix/*`: urgent fixes created from `master`.
- `release/*`: release preparation.

## 6. Daily Feature Workflow

Create a feature branch from current `develop`:

```bash
git switch develop
git pull --rebase origin develop
git switch -c feature/blocking-echo-server
```

Develop in small logical commits:

```bash
git status
git add src/net tests/unit
git commit -m "feat(net): add blocking echo server"
```

Synchronize before merge:

```bash
git fetch origin
git rebase origin/develop
```

Merge through a Pull Request with a non-fast-forward merge. Do not rebase shared `develop` or
`master` history.

## 7. Conventional Commits

Format:

```text
<type>(<scope>): <imperative subject>
```

Examples:

```text
feat(net): add epoll poller
fix(protocol): preserve partial bulk strings
test(storage): cover rehash during deletion
perf(aof): batch append writes
docs(architecture): document request lifecycle
chore: update development tooling
```

Useful scopes for this project are `base`, `net`, `protocol`, `server`, `storage`,
`persistence`, `bench`, and `build`.

## 8. Connect a GitHub Remote

Create an empty GitHub repository without an auto-generated README, then run:

```bash
git remote add origin git@github.com:<username>/mini-redis.git
git remote -v
git push -u origin master
git push -u origin develop
```

SSH authentication check:

```bash
ssh -T git@github.com
```

Never commit `.env`, keys, tokens, database files, AOF files, or local settings. The repository
`.gitignore` covers common cases, but `git status` remains the final check.
