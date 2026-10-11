# Vault search guidance

Six controls across five search categories offer optional one-step guidance directly above their input. The entry opens a non-modal inline panel. The direct field stays editable. Return closes the panel, preserves the query and returns keyboard focus to that field. Neither editing nor changing language reopens dismissed guidance. Guidance never performs a separate search or persists a query.

## Hand-maintained control inventory

| Control identifier | Authoritative guidance key | Complexity and unique scope |
| --- | --- | --- |
| `vault.entries` | `search.guidance.entries` | Simple, one step: current database/group entry metadata, excluded secret/custom fields, selected-tag interaction |
| `vault.groups` | `search.guidance.groups` | Simple, one step: sidebar folder names and ancestor visibility, without selecting folders |
| `vault.tags` | `search.guidance.tags` | Simple, one step: tag choices, preserving selected tags and their entry filtering |
| `vault.group-scope` | `search.guidance.group-scope` | Simple, one step: menu group choices, selection changes actual entry scope |
| `tabs.open` | `search.guidance.tabs` | Simple, one step: displayed labels of open tabs; filtering never closes, reorders or changes stable identity |
| `vault.attachments` | `search.guidance.details` | Simple, one step: detail labels/attachment names, excluding values and bytes |

Each explanation states purpose, empty/default case-insensitive behavior, example, regex choices, clear effects, invalid-pattern recovery and unavailable data. English and Cantonese copy resolve through the existing voice catalogue; bilingual combines both. The guidance entry identifies its exact target. Return explicitly keeps current input. This single explanatory step has no intermediate input, separate execution, Back state or rollback claim.

## Verification and limitations

`TestMaterialVaultFilters::contextualGuidanceInventoryAndDismissal` independently lists all six required controls and rejects omission of each row. It checks optional opening, unchanged query, localized accessible entry, dismissal, no reopening after language/query changes, and destruction of a field while its disabled guidance host survives. `TestMaterialTabs::searchableOverflow` additionally checks guidance preserves the query and the filtered result runtime identity before activation. Execution remains pending. Native focus return, above-control geometry, minimum viewport, display-scale/theme matrix and screen-reader acceptance remain unverified.

Each category also owns six discoverable help actions, identified by `<category>.help.input`, `.clear`, `.regex`, `.builder`, `.open` and `.done`. The command palette discovers these live actions through its existing action-tree inventory. Choosing one opens the existing optional explanation with that child's unique contextual help and preserves the query. The input, regex, builder, guidance entry and Return controls also expose their explanation as a tooltip. Help explains empty clear controls and compact fields that have no separate regex toggle rather than pretending every child is available.

The focused check independently lists those six child identities, resolves each action, checks distinct explanations, unchanged input and dismissal. Native action discovery and interaction remain unverified. The panel does not yet provide searchable article navigation or a separate above-control wizard entry for every child. Those shared capabilities remain incomplete. The inventory covers only these six search inputs across entries, folders, open tabs, tags and entry details. Group-scope guidance supplements the folder category. Other vault controls and application surfaces remain outside this inventory.

## Narrow sidebar guidance entries

The optional guidance entry uses a dedicated wrapping Material text-button subclass. It keeps
its complete contextual label and accessible name, allocates height from its available width,
and retains the existing minimum touch height, focus ring, button state colours and keyboard
activation. The shared button implementation is unchanged. Text wraps at word boundaries and
can break a long unspaced run when necessary; it is not silently cut or replaced with an ellipsis.
Opening/dismissing guidance, query state and builder ownership retain their existing behaviour.

`TestMaterialVaultFilters::guidanceEntriesWrapWithinNarrowHosts` contains the hand-maintained
group/tag cases at a 230-pixel host width in English, Cantonese and bilingual modes. Its negative
recreates the former full-label minimum-width button escaping that host. The repaired cases check
mapped child containment, complete text/accessibility names, height-for-width and touch height,
keyboard activation, dismissal and query preservation. These source fixtures have not been built
or executed. Native pixel, keyboard, accessibility, language/theme/scale and minimum-viewport
acceptance remain pending for this repair; earlier captures retain their original source binding.
