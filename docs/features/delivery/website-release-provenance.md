# Website release provenance

The document selects `./favicon.ico` with an explicit icon link. The normal build
copies the existing project-owned `share/windows/keepassxc.ico` into that output
path without conversion or a network dependency. The focused built-output check
verifies its ICO header and exact byte identity. This repairs the missing asset
behind the captured baseline's local favicon 404; a fresh-browser runtime pass
must still verify the rebuilt request and absence of a console error.

The website's downloadable version is projected from a published stable release, its `build-provenance.json`, and its `update-manifest-v1.json`. The generator rejects disagreements before writing `site/release.json`.

Run `node scripts/site-release-data.mjs RELEASE_JSON PROVENANCE_JSON MANIFEST_JSON RECEIPT_JSON site/release.json`. Prefer the fetch command below, which resolves and peels the release tag through the GitHub CLI. Manual inputs must include `targetCommit`, resolved from that tag, plus all three attached metadata files obtained with explicit asset patterns. The generator makes no network request and consumes at most one MiB per input, including UTF-8 BOM handling for the existing PowerShell-produced assets.

Validation requires a stable version, x64 `KeePassXC.Material` identity, a source commit, a recorded UTC build timestamp, agreement between executable hashes, exact project-owned release URLs, unique required assets, and matching package byte counts. The output omits build-machine paths. It records the build timestamp from provenance, never the generator's clock.

The release tag must resolve to the exact recorded source commit. The attached installer receipt must report `NotSigned` and match the source, version, architecture, installer byte count and package hash. This is metadata consistency validation. It does not prove installer execution, drag behavior, updater behavior, or authenticity. Packages remain unsigned. A future website view must render an unavailable state when this metadata is missing or invalid and must not invent a version or timestamp.

`node scripts/test-site-release-data.mjs` verifies a valid projection, private build-path omission, BOM handling, the byte limit, and nineteen invalid-input cases plus a missing-receipt case. `node scripts/fetch-site-release-data.mjs OUTPUT_JSON [TAG]` retrieves the selected published metadata through the GitHub CLI and applies the same validation. Set `KPXC_REFRESH_RELEASE=1` when building to retrieve the latest stable release directly into generated output, leaving tracked source unchanged. Missing or inconsistent release metadata fails publication instead of presenting invented downloads.

The Pages workflow refreshes on main-branch changes, manual dispatch, and successful completion of the main-branch delivery workflow. Publication creates no source commit, so it cannot trigger a source-push loop. Focused rendering evidence exists; the Pages deployment for `33a1fec98bdb988b8fa59728b8f954829dfd3e38` completed successfully in [run 34285869684](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/34285869684).

The website build includes license and notice files for its locally bundled runtime dependencies in `licenses/`. Build tooling is not loaded by the visitor's browser.

## Locally delivered documentation and wiki

The website build now packages every Markdown article and category index under `docs/features/`, together with the complete checked-in wiki snapshot under `docs/wiki/`, into `documentation.json`. The documentation reader renders those articles inside the existing documentation workspace. Known article, category, wiki and heading links stay within the deployed website. Clearly labelled original-source links remain available alongside the full content.

Refresh the snapshot with `node scripts/sync-wiki-snapshot.mjs WIKI_CHECKOUT` after fetching, reviewing and committing the project wiki. The command verifies the wiki origin and clean state, records its revision, page inventory, content hashes and source blob identifiers, and refuses silently retired pages. It does not fetch or prove freshness itself. Compare the recorded revision with the wiki's remote tip before publication. The build rejects missing, altered or duplicate snapshot pages. Historical acceptance statements remain historical.

Markdown is parsed by pinned, locally bundled Marked and sanitized by pinned DOMPurify before insertion. Article retrieval has a 512 KiB byte bound and a 15-second deadline. The build rejects documentation images until a reviewed local asset mapping is supplied, rather than silently publishing missing or external images. The current delivered article inventory contains no images. Documentation text is not a source of executable instructions.

`node scripts/test-site-documentation.mjs` exercises recursive article inclusion, complete wiki inventory, changed content, missing inventory records, invalid blob provenance, unmapped images and restored-source success. Passing this producer check does not establish rendered layout, keyboard navigation, deployed routes or live freshness; those require the built browser workflow and live readback.
