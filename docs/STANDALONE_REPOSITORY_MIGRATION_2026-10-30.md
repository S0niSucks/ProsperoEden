# Eden Encore — standalone repository migration (2026-10-30)

## Official repository

- **Active independent repository:** https://github.com/niakw/ProsperoEden-Encore (`fork: false`)
- **Historical fork (README redirect, original stars and discussions preserved):** https://github.com/niakw/Prospero.Eden-Encore
- **Separate pinned vendor submodule, unchanged:** https://github.com/niakw/Prospero.Eden-Encore-vendor
- **Original complete historical recovery bundle:** https://github.com/niakw/Prospero.Eden-Encore-vendor/releases/tag/encore-original-history-20261030

The old fork retains its discussions and GitHub stars, which cannot be transferred to this independent repository. Its README/README.fr.md now redirect contributors here, and its two scheduled Actions workflows have been disabled. Old pull request #21 was closed as migrated to the independent [Avatar PR #1](https://github.com/niakw/ProsperoEden-Encore/pull/1).

## Mirrored assets and source

- **Four development branches:** `dev/ps5-sparse-jit`, `feat/graphics-network-achievements`, `fix/ps5-launcher-avatar-recovery`, `research/ns2-ps5-compatibility`.
- The default `main` branch is a **fast-forward alias of the active `dev/ps5-sparse-jit`** source, primarily for the GitHub landing page and normal repository search. It is not a separate engine-release mode. Future F07 edits happen on dev; advance main only with a verified fast-forward and never force-push.
- **Six historical tags** have been copied with their original names.
- **Two GitHub Releases** (`encore-R1-56293953`, `encore-test-56293953`) and their four assets were copied without native compilation or binary rebuilding. The uploaded assets have SHA-256 values matching the original releases byte-for-byte.
- The `vendor` gitlink remains `8c354e6714558c3c88f7ca98d23551168cb680ea` on the engine branch; branch-specific snapshots remain in the vendor repository.

### Exact tree verification

The standalone migration initially reproduced the exact original Git tree for all four branches. Before standalone-link updates, the following Git tree SHAs were verified equal to their old-repository branch tip trees:

| Branch | Exact source Git tree SHA |
| --- | --- |
| dev/ps5-sparse-jit | `045cce1693fdfeca05d8f86ad849bd20060d8186` |
| feat/graphics-network-achievements | `90b30c22347dee6d2ce0398e53e7ed598c94481b` |
| fix/ps5-launcher-avatar-recovery | `10476dfa96a0e1e740805c8134769fe5ff48676a` |
| research/ns2-ps5-compatibility | `a7019a84e2248b89d3b7eef4e3c49f88720d0698` |

**Git commit SHAs are different**, since the GitHub Actions installation token did not allow importing previous `.github/workflows` paths directly. Historical workflow files and their changes were preserved under `docs/encore-workflow-history/` during the history import, then the actual `.github/workflows` trees were restored from their exact archived Git tree IDs. Current branch files were made byte-identical to their originals before targeted repository URL updates. Older migrated tag commits retain relocated workflow-history paths, whereas the actual original tags/commits remain recoverable from the old fork and archival bundle.

## Post-import fixes

- README/README.fr.md CI and release links point to the independent repository.
- The security issue template links to the current SECURITY.md.
- Glyph-research persistent source location and selected clone instructions now use the new name.
- The launcher About screen source URL was changed **only as a string literal** (old Git blob `96afbbbae783279625e88c18e39ec1115f2b4730`, new blob `1f48fc883e2930704275b16e9333e33b1793d517`); the audit manifest was repinned with byte-for-byte diff evidence.
- `MEMORY.md` now identifies this repository as official while retaining evidence links to historical PRs/CI of the old fork.
- The recurring F07 automation now targets this repository exclusively.

## Use the new repository

```bash
git clone --branch dev/ps5-sparse-jit --single-branch https://github.com/niakw/ProsperoEden-Encore.git
cd ProsperoEden-Encore
git submodule sync --recursive
git submodule update --init --recursive vendor
```

Do not merge old Git history from cached clones into the new independent repository. GitHub repository About metadata and the archived flag of the former fork are separate repository settings, not part of the Git commit history. They are not transferred by a Git push and require a repository-admin settings change if desired.

**Build policy:** F07 host/prebuild optimizations continue. No final PS5 release rebuild is authorized by this migration.
