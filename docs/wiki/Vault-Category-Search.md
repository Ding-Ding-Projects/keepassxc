# Vault category search

The vault workspace provides separate searches for saved entries, folders, open database tabs, tags, and entry field labels or attachment names. Each category owns its query. Existing search bars are reused rather than duplicated.

## Entries and privacy

The entry search matches unprotected title, username, URL and notes, plus tags and group names. It does not resolve placeholders while matching, so a username containing a password placeholder cannot make the underlying password searchable. Password and custom-attribute value searches are excluded from the database GUI. Existing expiry, health, UUID, attachment-name and presence filters remain available. Other search-engine callers retain their established behavior.

Search text remains in memory. Vault context changes, locking and closing clear category queries and reset regex modes. Queries are not added to settings, exports or diagnostic output by these controls. This is not a promise of forensic memory erasure.

## Folders, tabs and tags

The folder search keeps matching descendants and their ancestors visible. Clearing it restores rows without editing the database. The tab-strip search button opens the all-tabs selector even when no tabs overflow; choosing a result activates its stable runtime identity.

Tag search narrows available chips without changing selected tags. Selected chips remain visible and removable even when their names do not match. Clearing tag search restores available chips while preserving selection. Tag regex execution uses the existing bounded matcher. Invalid or unsafe tag patterns leave prior results standing with an inline message.

The tab selector also evaluates a complete candidate result set before replacing its rows. Invalid, blocked or timed-out expressions retain the previous rows. Locking or closing a vault resets and closes an open selector; normal descriptor changes refresh it without changing tab order. Search labels and inline messages use the English/Cantonese voice catalogue, including bilingual mode. Changing language preserves the current query and regex-builder owner. Regex flags apply independently to each category.

## Entry details

The detail search matches field labels and attachment filenames only. It does not match displayed field values, password values or attachment contents. Invalid regex keeps the previous rows visible.

## Current continuation, October 11, 2026

Native candidate `8bb91c55791e54667541e03b8772c808243b168b` passed root `build.bat /s -WithGuiTests` and complete staged-file validation. Direct offscreen suites passed 12 entry-searcher, 11 tab and 11 vault-filter cases, with no failures or skips. Metadata exclusion boundaries, opaque selector configurations and search availability without attachments are covered by those compiled fixtures.

Later candidate `7d074ca71461a318bab17844f7fe48305d06ffe4` passed the same root entrypoint and complete stage validation. Its 11 vault-filter cases passed, including repeated entry-content refresh under a detail query and last-valid filtering while current regex input is invalid. Earlier unchanged-suite results remain bound to their original source.

Current synthetic native captures confirm visible detail search without attachments, retained field-label filtering through a real selection refresh and an opaque selector. The opened group guidance still overlaps its Return control; the visible finding remains open despite the passing earlier fixture. Distinct-tab activation and complete current lock/close acceptance remain unverified. A stronger constrained-sidebar source repair is being reviewed separately. No complete native matrix is claimed.

Reader source `9c40436844d4d6edde7b4a839eeb80d42b6600f9` passed its actual root website-only build, eleven focused checks, four index queries and the search article route. Nineteen genuine frames were independently inspected: sixteen desktop-English matrix frames and three narrow bilingual focused frames. The matrix stopped at its existing ownership-query deadline, leaving eighty required frames unrun. Current owned process trees and exact desktops were normally closed; the isolated profile and an earlier uncertain desktop association remain retained. Strict audit and socket-refusal proof remain unverified.

The refreshed documentation at `f10ce6dc060322a2a0a54a2c6ee1c1a7d176d2b1` passed root `build.bat /s -WebsiteOnly`, with all twenty output hashes verified. This later build is not covered by the earlier browser captures or a current hosted-deployment verdict.

All 105 requirement identities remain mapped. The authority requires 1,119 capabilities, 108 surfaces and 120,852 cells; zero cells are accepted. Full local-model management, the eight-category converter, server targets, motion authoring, shared restricted mode and other universal workflows remain incomplete. Remote main is `f5949912caf3ebfe7c9ca9430d2141b4ebd21810`. Task preservation is not completed delivery. No new release or destructive cleanup is claimed.
## Historical continuation snapshot

Root `build.bat /s` later passed at `cdb8862bd2637b30464abae8ad13888885def592`, with independently verified stage provenance. Nine of twelve selected CTest suites passed; voice failed and two did not start. Separate direct search-registry and tab checks passed. Later SSH, vocabulary and voice source repairs require their own native execution. The GUI-enabled root candidate `4f5b3eb9` failed on an ambiguous list append; repair `e202657b88df87c5e36bc4fa03da3c5dbf2f3d69` is building through `build.bat /s -WithGuiTests`. Earlier evidence below remains historical.

The current requirements authority maps all 105 identities and records 1,119 capabilities across 120,852 required surface cells. Its 95 focused checks passed, with zero accepted cells. Complete server targets, motion authoring, guidance and universal per-surface acceptance remain incomplete.

Reader source `86e06cd96928f29487c422fe4f039b5364f4f6ba` collected 384 unique article/layout tuples, all independently inspected. Three surrounding English resource buttons were retained as a finding and subsequently repaired in source. Reader preservation `7a82d0ebb823e9f0d4d6e6b2162edc22c9ea89cc` combines current native and browser repairs, including schema/cache, trusted upload wiring and four internal documentation routes. Focused parser/storage and upload-wiring checks pass. Generated documentation bindings and genuine current browser interaction remain pending; old captures do not prove these changes. Owned browser/server processes and empty desktops were closed with receipts.

Remote main remains `906e7b3230595b0abf73ea479955c50418d7d0b1`. Task branches are preservation, not completed delivery. No new release or destructive cleanup has occurred.

## Historical verification status

Root `build.bat /s` completed successfully at `0ffb00b9ae963e7629bacc2e2903c5df4a104490`. All five focused native checks passed: entry search, configuration, search registry, vault filters and tabs. The staged executable and every stage-receipt hash were independently verified. This establishes that source candidate's build and focused behavior checks, not complete GUI acceptance.

A genuine synthetic vault capture was obtained at 1440 by 860, English and light theme. It was rejected for language acceptance because the capture route wrote the language configuration without notifying already-constructed controls. Repair `49a2547bab915b2ad3a6a1d218f8282602a54e41` uses the normal Voice setter and adds scheduled production-route fixtures for English, Cantonese and bilingual transitions. Its exact root build is running; those new fixtures and the full native matrix remain pending.

The separate documentation reader at `51521199478ac825a8cfdede991543cd939e622f` passed real index checks (47 initial articles, three vault matches, zero synthetic no-match results and 47 after clearing), 48 bounded language/theme/viewport/scale captures and five keyboard checks. Its processes and empty hidden desktops were closed with proof. Pixel review covers captured portions, not full accessibility or deployment. Independent semantic review read all 63 English/Cantonese pairs; the narrow wording finding and inherited source inconsistencies were repaired in `fcf3737e55c0a61d7ea16407c30c7f32aebd4ea5`.

Capture runs use a dedicated verification identifier with isolated instance and standard paths, ignore inherited configuration overrides and portable mode, and reject explicit configuration overrides before application startup. Only synthetic data was used. Broader native workflow, minimum-size, theme, language and scale acceptance remains incomplete. No new release is authorized.

## Related

- [Search bars and registry](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/search-bar-every-surface.md)
- [Regex builder](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/regex-builder.md)
- [Browser-style tabs](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/navigation/tabs.md)
