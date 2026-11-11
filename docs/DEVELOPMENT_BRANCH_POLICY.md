# Eden Encore — main-only development policy

**Effective 30 October 2026:** [`niakw/ProsperoEden-Encore`](https://github.com/niakw/ProsperoEden-Encore) is an independent GitHub repository, and **`main` is both its default branch and the only branch authorized for new development**.

This supersedes the original October 13 policy that used `dev/ps5-sparse-jit`. Its obsolete branch reference has been **removed after merging its five final commits into `main`**; the commits and PR #5 remain accessible in Git history. Do **not** recreate, push to, merge, fast-forward, cherry-pick into, open new PRs against, or run new development automation on that old development branch. No parallel F07 branch or fork may become the source of truth.

## Rules for changes

1. Fetch and verify the current **`main` HEAD** before every write. Concurrent F07 fixes must not be overwritten.
2. Commit directly to `main`, preserving its existing commit history. Use a safe expected SHA or other optimistic-concurrency check when updating refs.
3. Do not force-push or rewrite `main`. Do not resurrect the old history containing the oversized `vendor/` source tree.
4. Keep the pinned `vendor` Git submodule in the original path; its repository is `niakw/Prospero.Eden-Encore-vendor`. Ordinary clones must initialize submodules.
5. Test the exact commit changed. Source/host checks and CI are distinct from native PS5 compilation and from **hardware validation on firmware 13.60**.
6. Do not launch a final native PS5 release build until the F07 optimization work is signed off. A green source test or a successful screenshot does not justify a release.
7. Avoid using a secondary development branch for documentation, security, launcher, Vulkan or performance fixes. Existing secondary branches can remain for historical reference but must not receive new work.

**All automation prompts, badges, README build links and issue templates must point to `main`.** GitHub's old branch-filtered runs remain usable as historical proof, not as the current active pipeline.

## Migration checkpoints (historical)

- **13 October 2026:** the former repository `niakw/Prospero.Eden-Encore` consolidated R419–R428 on its then-primary `dev/ps5-sparse-jit` branch. Older [CI evidence](https://github.com/niakw/Prospero.Eden-Encore/actions/runs/38380839924) describes the state **at that date only**.
- **30 October 2026:** the active code and branches, release tags, release assets, and pinned `vendor` were moved to independent `niakw/ProsperoEden-Encore`. The old fork was reduced to a historical redirection containing a Git submodule link to the new repository.
- **30 October 2026:** Avatar PR #1 and the main synchronization PR #4 were merged; `main` became the only active branch for subsequent work by explicit maintainer instruction.

The archived fork's stars and historic release/issue metadata remain distinct from the new repository's GitHub metadata. The Git history rewrite and mirror are documented separately in [repository history compaction](REPO_HISTORY_COMPACTION_2026-10-30.md). Historical document references to `dev/ps5-sparse-jit` or older SHAs do **not** override this policy.

## Current references

- [Official repository and README](../README.md)
- [Install and build instructions](../INSTALL.md)
- [Contributing](../CONTRIBUTING.md)
- [Accessibility](../ACCESSIBILITY.md)
- [Security](../SECURITY.md)
- [F07 source/hardware validation tracker](FC27_F07_MASTER_TRACKER.md)
