# CodeQL R419 — security triage for alerts #10–#28

Scope: `dev/ps5-sparse-jit` (October 2026). The GitHub CodeQL *default setup* scans every readable vendored dependency, including upstream generator and test helpers that are neither executed nor shipped as Eden Encore application logic. The original `vendor/sources/` trees and `vendor/objects/` archives are exact SHA-pinned inputs. Do not silently modify original vendor files or release hashes to satisfy scanner findings.

## First-party fixes (5 alerts)
- #23: `tools/ps-glyph-mod-discovery.py` — verify DuckDuckGo DNS-label boundaries before honoring `uddg` redirect metadata; reject lookalike hosts such as `evilduckduckgo.com`. Preserve the separate destination hostname allowlist and strip tracking parameters.
- #22: `tools/check-ps-glyph-curl-search.py` — validate parsed search endpoint schemes, hosts, paths and query fields instead of matching URL text prefixes.
- #19–#21: `tools/check-guest-network-isolation.py` — assert **whole, uncommented denylist entries**, not accidental substrings. Production network DNS filtering is not changed.
- Extend `check-ps-glyph-mod-discovery.py` with lookalike-origin regression examples and run offline tests in CI.

## Vendored source alerts (14 alerts)
- #12–#18: Boost MPL `libs/mpl/preprocessed/pp.py` regexes (upstream formatter).
- #10–#11: Boost Litre `tools/litre/cplusplus.py` temporary-file handling (upstream harness).
- #26–#27: Mesa `bin/nir-test-runner.py` overly broad character class (upstream test runner).
- #24–#25: HarfBuzz `hb_test_tools.py` overly broad character class (upstream test).
- #28: vendored `python/run_tests.py` overly broad character class (upstream test).

These findings are not evidence that the native PS5 emulator executes these tools. They nevertheless remain relevant if a maintainer separately runs upstream scripts with untrusted inputs. The narrow `.github/codeql/codeql-config.yml` exclusions avoid false security confidence from modifying byte-pinned dependencies; all **first-party** tools, `headless`, active renderer forks and runtime code remain eligible for analysis. Do not exclude all of `vendor/`.

**Activation required on GitHub (personal repository):** This repository is owned by the personal account `niakw`. GitHub's `github-codeql-config-file` custom repository property is created through organization properties and cannot be assumed to exist for a personal repository. The code-scanning configuration here must therefore be consumed by the new `.github/workflows/codeql-advanced.yml` workflow using `github/codeql-action/init@v4` with `config-file` set explicitly.

**One GitHub settings switch remains:** Repository Settings → Advanced Security → CodeQL analysis → menu → **Switch to advanced** → **Disable CodeQL** (disables the old *default* setup only). The tracked advanced workflow then scans `actions`, `python`, and `c-cpp` on pushes and weekly; it keeps all emulator and active-fork code eligible and excludes only the five pinned third-party generator/test scripts. The first-party Python findings are real code fixes; the 14 upstream findings are **scope exclusions**, not upstream security repairs. Allow a complete advanced analysis to run before asserting that alerts are closed. Official guide: https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/configure-code-scanning/configuring-advanced-setup-for-code-scanning

Verification gates: syntax compilation, offline domain/canonical-URL unit tests, the existing broad `encore-core-preflight`, and the next scheduled CodeQL scan. Source-level tests are not equivalent to a completed CodeQL run. No new PS5 binary build is necessary for security-only Python and scanning-config changes.

## 30 octobre 2026 — nouvelles alertes C/C++ #1–#16

- **#13–#16, RADV `radv_cmd_buffer.c` :** quatre indexations GPU effectuaient la multiplication sur 32 bits avant conversion en `uint64_t`. La source native active sous `vendor/encore-active/ps5-mesa` multiplie désormais `(uint64_t)draw->firstIndex * index_size`. Le générateur `tools/patch-radv-wsi.py` conserve la parité du fork.
- **#5–#12, RADV `radv_meta_clear.c` :** huit alertes CMASK, FMASK, DCC et HTILE. Dix expressions de taille/décalage ont été élargies en 64 bits **avant** multiplication. Fork natif et générateur synchronisés, archives Mesa non modifiées.
- **#1–#4, NanoVG `vendor/sources/opengl-nanovg/example/stb_image_write.h` :** exemple tiers archivé, **non livré dans le moteur PS5**. Ces multiplications peuvent déborder pour des dimensions extrêmes. Ne pas modifier le snapshot épinglé, dont les octets servent de preuve de provenance. Le setup avancé CodeQL exclut les archives `vendor/sources` mais conserve `vendor/encore-active`.
- **Gates de régression :** `tools/check-ps5-radv-64bit-arithmetic-host.py` (sources et transformateur + C UBSan O0/O2), `tools/check-vendored-radv-fork.py` (oracle exact), fast gate, prévalidation, CodeQL même SHA. Ne pas annoncer les 16 alertes fermées sans preuve CodeQL.

**Configuration GitHub :** tant que le *default setup* CodeQL reste actif, il peut continuer à scanner `vendor/sources` et à afficher les quatre cas d'exemple, voire d'anciennes copies de Mesa. La bascule vers le workflow avancé se fait dans les paramètres du dépôt selon la procédure déjà documentée. C'est une exclusion de code non livré, pas une réparation du composant tiers pour un autre usage.
