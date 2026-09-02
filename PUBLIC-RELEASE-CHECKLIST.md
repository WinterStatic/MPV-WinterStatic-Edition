# Public release checklist

This file is a release-engineering checklist, not legal advice.

Project repository: https://github.com/WinterStatic/MPV-WinterStatic-Edition

## Frontend

- Confirm all version metadata matches the intended release.
- Include `LICENSE` (GPL-3.0-or-later), `README.md`, source, build scripts, icons, manifest, and resources.
- Build from a clean tree and test the resulting Full Portable folder on Windows 10/11.

## Recommended Full Portable release build

Run:

```text
build-native.bat release
```

This performs the normal build and then creates a matching runtime-source bundle
from the exact MSYS2 packages represented by the bundled DLLs. Source collection
is deliberately not part of normal development builds because the official source
archives can be large and require internet access.

A successful 0.4.5 release-mode build should produce:

- `MPV-WinterStatic-Edition-0.4.5-portable/` (or a numeric-suffixed folder if the canonical folder was locked)
- `MPV-WinterStatic-Edition-0.4.5-runtime-source/`
- `MPV-WinterStatic-Edition-0.4.5-Runtime-Source.zip` when MSYS2 `bsdtar` is available

If release source collection fails, do not publish that Full Portable binary until
the matching source bundle has been completed successfully.

## Bundled libmpv runtime

The Full Portable build collects runtime DLLs from MSYS2 MINGW64.

- Keep `libmpv/THIRD-PARTY-NOTICE.txt`.
- Keep the generated `libmpv/RUNTIME-MANIFEST.txt`.
- Keep the generated `libmpv/RUNTIME-PACKAGES.tsv`.
- Keep the generated `libmpv/licenses/` directory.
- Check that `RUNTIME-MANIFEST.txt` contains no unresolved DLL ownership that needs investigation.
- Check that `RUNTIME-PACKAGES.tsv` contains a source base for every represented package.
- Do not silently replace the tested libmpv runtime with an arbitrary newer build immediately before release.

## Matching runtime source

`collect-runtime-source.sh` uses the exact installed package version plus pacman's
recorded package base (`%BASE%`) to fetch MSYS2's official Source-Only Tarball from:

```text
https://mirror.msys2.org/mingw/sources/
```

For the generated runtime-source bundle:

- Keep `SOURCE-MANIFEST.txt`.
- Keep `SOURCE-PACKAGES.tsv` and its SHA-256 values.
- Keep the copied `RUNTIME-MANIFEST.txt` and `RUNTIME-PACKAGES.tsv`.
- Keep all downloaded `.src.tar.zst` archives.
- Keep `.sig` files where available; signature verification status is recorded but an unavailable/unverifiable signature does not delete otherwise complete source.
- Attach the completed runtime-source archive to the same GitHub Release as the corresponding Full Portable binary.
- Prefer retaining the source asset indefinitely with that release rather than depending on a third-party mirror remaining unchanged forever.

The collector intentionally includes source archives for every MSYS2 package
represented by the runtime DLLs. This is more conservative and less error-prone
than trying to maintain a hand-written list of only copyleft dependencies.

## Suggested release assets

1. **Full Portable** — frontend plus the tested `libmpv/` runtime.
2. **Runtime Source** — the generated `MPV-WinterStatic-Edition-0.4.5-Runtime-Source.zip`.
3. **Repository source** — GitHub's repository/tag source archive, containing the frontend and build scripts.
4. **Frontend-only** (optional) — for advanced users who intentionally supply a compatible libmpv runtime.

Once a stable runtime binary asset exists on a GitHub Release, the missing-runtime
dialog can later be extended to download that project-owned, known-compatible
runtime into `libmpv/`. Keeping the asset URL under the project's control avoids
depending on an unstable third-party "latest" archive.
