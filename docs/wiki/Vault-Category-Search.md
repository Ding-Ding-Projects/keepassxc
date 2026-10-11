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

## Verification status

Root `build.bat /s` completed successfully at `0ffb00b9ae963e7629bacc2e2903c5df4a104490`. All five focused native checks passed: entry search, configuration, search registry, vault filters and tabs. The staged executable and every stage-receipt hash were independently verified. This establishes that source candidate's build and focused behavior checks, not complete GUI acceptance.

A genuine synthetic vault capture was obtained at 1440 by 860, English and light theme. It was rejected for language acceptance because the capture route wrote the language configuration without notifying already-constructed controls. Repair `49a2547bab915b2ad3a6a1d218f8282602a54e41` uses the normal Voice setter and adds scheduled production-route fixtures for English, Cantonese and bilingual transitions. Its exact root build is running; those new fixtures and the full native matrix remain pending.

The separate documentation reader at `51521199478ac825a8cfdede991543cd939e622f` passed real index checks (47 initial articles, three vault matches, zero synthetic no-match results and 47 after clearing), 48 bounded language/theme/viewport/scale captures and five keyboard checks. Its processes and empty hidden desktops were closed with proof. Pixel review covers captured portions, not full accessibility or deployment. Independent semantic review read all 63 English/Cantonese pairs; the narrow wording finding and inherited source inconsistencies were repaired in `fcf3737e55c0a61d7ea16407c30c7f32aebd4ea5`.

Capture runs use a dedicated verification identifier with isolated instance and standard paths, ignore inherited configuration overrides and portable mode, and reject explicit configuration overrides before application startup. Only synthetic data was used. Broader native workflow, minimum-size, theme, language and scale acceptance remains incomplete. No new release is authorized.

## Related

- [Search bars and registry](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/search-bar-every-surface.md)
- [Regex builder](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/regex-builder.md)
- [Browser-style tabs](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/navigation/tabs.md)
