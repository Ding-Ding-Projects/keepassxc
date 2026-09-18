# Website release provenance

The website's downloadable version is projected from a published stable release, its `build-provenance.json`, and its `update-manifest-v1.json`. The generator rejects disagreements before writing `site/release.json`.

Run `node scripts/site-release-data.mjs RELEASE_JSON PROVENANCE_JSON MANIFEST_JSON RECEIPT_JSON site/release.json`. Prefer the fetch command below, which resolves and peels the release tag through the GitHub CLI. Manual inputs must include `targetCommit`, resolved from that tag, plus all three attached metadata files obtained with explicit asset patterns. The generator makes no network request and consumes at most one MiB per input, including UTF-8 BOM handling for the existing PowerShell-produced assets.

Validation requires a stable version, x64 `KeePassXC.Material` identity, a source commit, a recorded UTC build timestamp, agreement between executable hashes, exact project-owned release URLs, unique required assets, and matching package byte counts. The output omits build-machine paths. It records the build timestamp from provenance, never the generator's clock.

The release tag must resolve to the exact recorded source commit. The attached installer receipt must report `NotSigned` and match the source, version, architecture, installer byte count and package hash. This is metadata consistency validation. It does not prove installer execution, drag behavior, updater behavior, or authenticity. Packages remain unsigned. A future website view must render an unavailable state when this metadata is missing or invalid and must not invent a version or timestamp.

`node scripts/test-site-release-data.mjs` verifies a valid projection, private build-path omission, BOM handling, the byte limit, and nineteen invalid-input cases plus a missing-receipt case. `node scripts/fetch-site-release-data.mjs OUTPUT_JSON [TAG]` retrieves the selected published metadata through the GitHub CLI and applies the same validation. Set `KPXC_REFRESH_RELEASE=1` when building to retrieve the latest stable release directly into generated output, leaving tracked source unchanged. Missing or inconsistent release metadata fails publication instead of presenting invented downloads.

The Pages workflow refreshes on main-branch changes, manual dispatch, and successful completion of the main-branch delivery workflow. Publication creates no source commit, so it cannot trigger a source-push loop. Focused rendering evidence exists; the Pages deployment for `33a1fec98bdb988b8fa59728b8f954829dfd3e38` completed successfully in [run 34285869684](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34285869684).

The website build includes license and notice files for its locally bundled runtime dependencies in `licenses/`. Build tooling is not loaded by the visitor's browser.
