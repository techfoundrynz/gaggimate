---
name: sync-upstream
description: Rebase this fork's patch stack onto jniebuhr/gaggimate locally. Use when asked to sync with upstream, reapply our commits on top of upstream, resolve rebase conflicts against upstream, or pull in upstream changes.
---

# Sync this fork with upstream

`techfoundrynz/gaggimate` is a fork that carries a small patch stack on top of
`jniebuhr/gaggimate`. Keep it a **rebased stack**, never a merge: `git log --oneline
upstream/master..master` must always show exactly what this fork adds.

The same thing runs unattended via `.github/workflows/sync-upstream.yml`
(workflow_dispatch). Do it locally when that hits conflicts, or when you want to
build before pushing.

## Remotes

- `origin` → `techfoundrynz/gaggimate` (the fork; push here)
- `upstream` → `jniebuhr/gaggimate` (fetch only)

Both branches are `master`. There is no `main` — a rename would silently stop
`build-nightly.yml`, which triggers on pushes to `master`.

## Procedure

```bash
git config rerere.enabled true      # once; upstream churns the files we patch
git config rerere.autoUpdate true
git fetch upstream master
git rebase upstream/master master
```

Then verify, before pushing:

```bash
# 1. the OTA redirect still points at our fork
sed -n 's/^const String RELEASE_URL = "\([^"]*\)".*/\1/p' src/display/plugins/WebUIPlugin.h
# expect: https://github.com/techfoundrynz/gaggimate/releases/

# 2. the CI upload guard survived
grep -q 'UPDATE_SERVER_HOST:-' .github/actions/upload-firmware/action.yml

# 3. both firmwares still build
pio run -e controller -e display
```

Push. It is often a plain fast-forward (if the fork was behind upstream anyway);
only force when the rebase actually moved existing commits:

```bash
git push origin master              # try this first
git push --force-with-lease origin master
```

Never plain `--force`. Pushing `master` fires `build-nightly.yml`, which
force-updates the `nightly` release devices pull from.

## Resolving conflicts

Upstream sometimes solves the same problem we did, independently. When a conflict
is two designs for one job rather than two edits to one line, **adopt upstream's
and delete ours** — it shrinks the patch and removes the conflict permanently.
That is how `AccessoryBus` + `std::recursive_mutex` gave way to upstream's
`SoftWireBus`, and how three of our changed files went to zero delta.

Before writing a resolution, check what our commit actually changed:

```bash
git diff <our-commit>~1 <our-commit> -- <file>
```

Often it is only plumbing upstream has since done properly, and the answer is
`git checkout upstream/master -- <file>`.

## House rules for this fork

- Conventional commits, matching upstream: `fix:`, `feat:`, `chore:`, `ci(scope):`.
- Prefer upstream's idioms over ours: braced `if` bodies, `std::mutex` in
  `src/display/`, FreeRTOS semaphores in `lib/`, file-static helpers in `.cpp`
  rather than new private members in a header.
- Keep the stack minimal. Anything upstreamable should become a PR instead of a
  permanent patch.
- Fixups: `git commit --fixup=<sha>` then `GIT_EDITOR=true git rebase -i
  --autosquash upstream/master`. **Every autosquash rewrites all SHAs** — re-read
  them from `git log` rather than reusing one from earlier in the conversation.

## Environment notes (Windows dev box)

- `pio` is at `~/.platformio/penv/Scripts/pio.exe`; it is not on `PATH`.
- No host `gcc`/`g++`, so `pio test -e native` cannot build here. CI only runs
  `test_ota_*` anyway (`scripts/ota_testbench.sh`).
- No `clang-format` locally, so `scripts/format.sh` cannot be run here.
- Git Bash `grep -oP` fails ("PCRE supports only unibyte and UTF-8 locales") —
  use `sed -n 's/.../\1/p'` instead.
