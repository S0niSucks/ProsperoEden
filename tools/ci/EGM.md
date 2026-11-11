# Release tooling and artifact gates

## Overview

Host-side release assembly, staged-artifact checks and provenance bookkeeping. These contracts describe scripts and assertions, not a demonstrated console boot or game qualification. `[check-staged-app.py:expected]` `[write-crash-provenance.py:write_manifest]`

## Interface

- [make-dist.py](./make-dist.py) accepts `[--image-only] APP_DIR`; full mode emits the release ZIP, runtime-only FTP ZIP, package image, checksums and required release notes. [package-image.sh](./package-image.sh) provides image packing and `--prepare` tooling setup. `[make-dist.py:image_only]` `[make-dist.py:ftp_files]` `[package-image.sh:--prepare]`
- [check-staged-app.py](./check-staged-app.py) accepts an app directory, defaulting to `build/release/PPSA99008`; checks inventory, identity, legal/assets, all 29 catalogs and selected binary/source markers. [check-ftp-archive-root.py](./check-ftp-archive-root.py) validates installable ZIP layout. `[check-staged-app.py:expected]` `[check-staged-app.py:param]` `[check-ftp-archive-root.py:inspect]`
- Release gates cover [metadata](./check-release-metadata.py), [licenses/SFX](./check-licenses.py), [compiled experiments](./check-release-experiments.py), [final source selection](./check-final-one-profile-source.py), [engine ownership](./check-final-engine-owner-contracts.py), [bootstrap completeness](./check-native-boilerplate-setup.py) and [repository hygiene](./check-repo-hygiene.py). The [development-artifact audit](./check-all-on-test-binary.py) is distinct, not interchangeable release evidence. `[check-release-metadata.py:required]` `[check-licenses.py:expected_sounds]` `[check-release-experiments.py:REQUIRED]` `[check-final-one-profile-source.py:release]` `[check-final-engine-owner-contracts.py:expected]` `[check-native-boilerplate-setup.py:expected]` `[check-repo-hygiene.py:errors]` `[check-all-on-test-binary.py:audit]`
- [derive-encore-build-label.py](./derive-encore-build-label.py) derives the maximum matching `R` revision from first-parent commit subjects. [write-crash-provenance.py](./write-crash-provenance.py) writes an ELF/app hash receipt; [diagnose-image-repro.py](./diagnose-image-repro.py) reports differing image bytes. `[derive-encore-build-label.py:build_label]` `[write-crash-provenance.py:write_manifest]` `[diagnose-image-repro.py:scan_bytes]`
- [build-release.sh](./build-release.sh) invokes the checkout's release build; [link-sibling-deps.sh](./link-sibling-deps.sh) prepares selected dependency links. `[build-release.sh:EDEN_DEV_CHECKOUT]` `[link-sibling-deps.sh:ln]`

## Invariants

- Both distribution modes run the [name/path privacy preflight](../network_privacy.py) before deleting `dist` or invoking image tooling. Inputs must remain quiescent: this is not secret-content scanning or a race-proof sandbox. `[make-dist.py:preflight_export_tree]` `[../network_privacy.py:private_network_path]` `[../network_privacy.py:export_path_metadata]`
- Full-mode ZIP entries use fixed timestamps/modes. Only the FTP variant excludes `legal/` and `ui/lang/*.txt`. Full mode packs the image twice at the same destination, refuses differing hashes and restores the first image; image-only skips this reproducibility pass, ZIPs, notes and checksum files. `[make-dist.py:ftp_files]` `[make-dist.py:repro]` `[make-dist.py:image_only]`
- Artifact markers, profile assertions, a new provenance receipt and image-tool verification are not proof of native execution, compatibility or correspondence to an older crash report. `[check-staged-app.py:expected]` `[check-release-experiments.py:verify]` `[write-crash-provenance.py:text_size]`
- Run assertion-dependent Python gates with assertions enabled: no `-O`, `-OO`, or enabled `PYTHONOPTIMIZE`. Many staged inventory, identity, catalog, and binary/source checks are `assert` predicates and disappear under optimization; [check-staged-app.py](./check-staged-app.py) does not refuse optimized mode. Remaining file reads can still fail, so successful exit under optimization is not validation evidence. `[check-staged-app.py:expected]` `[check-staged-app.py:param]`

## Side Effects

- Full distribution assembly deletes/recreates `dist`, writes artifacts and may copy an unstripped ELF to `build/symbols`; it is not an all-or-nothing transaction. `[make-dist.py:dist]` `[make-dist.py:repro]`
- Image preparation may fetch/setup tooling and clean caches; packing runs external tools. License verification runs the SFX generator in a temporary directory. Dependency-link preparation can partially change links before refusal. `[package-image.sh:--prepare]` `[check-licenses.py:expected_sounds]` `[link-sibling-deps.sh:ln]`

## Verification

- From the repository root: `python3 tools/ci/check-staged-app.py build/release/PPSA99008` checks an already staged artifact. `[check-staged-app.py:app]`
- Repository-root `make test` fetches missing dependencies and builds the host core; `make release` builds/packages and invokes distribution assembly. Commands are documentary only; none was executed here. `[Makefile:test]` `[Makefile:release]`
