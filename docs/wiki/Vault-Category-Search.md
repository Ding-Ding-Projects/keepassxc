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

Source regression cases cover tag selection preservation, independent folder text, invalid patterns, clear behavior, metadata matches, secret exclusion, placeholder exclusion, protected notes, UUID and expiry filtering, and legacy engine compatibility. These cases require a compiled native test run before acceptance. Native interaction, supported language/theme/scale geometry and representative screenshots remain pending until a current source-bound build is available.

Additional focused cases cover rejected tab-pattern retention, tab-query reset, language-switch query and builder ownership, regex case sensitivity, and capture-profile configuration isolation. The existing source-style check passed after these repairs. Native execution remains pending. Capture runs use a dedicated verification identifier with isolated instance and standard paths, ignore inherited configuration overrides and portable mode, and reject explicit configuration overrides before application startup.

## Related

- [Search bars and registry](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/search-bar-every-surface.md)
- [Regex builder](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/regex-builder.md)
- [Browser-style tabs](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/navigation/tabs.md)
