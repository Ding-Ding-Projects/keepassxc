# Current feature coverage

This assessment starts at source commit `8997ed9ced49c15c3333ae298d8e1e048cce29b1`. The current maintainer catalogue contains 105 contracts. The explicit mapping is [canonical-contract-crosswalk.json](../canonical-contract-crosswalk.json). It uses public contract identities and distinguishes application, website, repository and documented exclusions. Five identifiers use public terminology: public-screenshot-gallery, wiki-website-sync, layout-matrix, mobile-site and shared-instruction-single-file-editions. A mapped family is a place to assess the requirement, never proof that every part of the contract exists.

## Recorded baseline

The existing feature inventory has 178 rows across 89 families and two aggregate platforms. The capability inventory has 998 contracts and zero accepted evidence rows. The surface registry has 106 surfaces. Both registries identify assessment commit `07ce4dc158bcd7eb735cc309f009c531731f3133`, so their statuses must be reconciled with current source before acceptance.

Recorded application rows comprise 49 missing, 37 partial and 3 unverified. Recorded website rows comprise 85 missing, 1 partial and 3 unverified. These are historical inventory states, not a fresh implementation verdict. Current search work can supersede older missing rows only after its actual implementation and required evidence are incorporated.

The current extension registers a server-management family with 47 explicit capability requirements and native/visitor surface records. The resulting inventory has 90 families, 180 platform rows, 108 surfaces and 1,045 capability contracts. The sparse accepted-evidence ledger still has zero rows. Historical assessment metadata remains historical; new source units do not retroactively verify it.

## Strengthened requirements in the current review

The refreshed catalogue still contains 105 contracts. The current review includes the following requirements within existing families as well as the new server manager:

- Each appearance editor needs a complete Animation and Transition tab: renderer-supported properties, tracks, keyframes, timelines, easing, playback, triggers, sequencing and interruption, with real consumers rather than stored values alone. Reduced-motion rendering preserves authored values separately. Acceptance needs timed recordings or sampled-render evidence as well as still images.
- Each capable interactive control needs optional guidance immediately above that control. Simple controls receive a contextual step; complicated workflows receive validated progression, Back, Cancel and recovery. Every rendered element needs a unique authoritative explanation covering actual purpose, defaults, consequences, validation and recovery. Explicit per-element inventories and omission regressions cover both requirements.
- Every current presentation screenshot reference needs inspected replacement evidence after an interface-changing delivery. This includes application, website, README, articles, wiki, gallery and download presentation. A stale-current-image negative regression must reject old images presented as current, while preserving historical originals and provenance.
- Personal-vocabulary uploads require one compatible versioned schema across native and browser surfaces: UTF-8 input at most 256 KiB, JSON depth 8, a dynamic entry inventory, keys up to 160 Unicode code points and values up to 1,000. Duplicate members and invalid Unicode are rejected before any replacement applies. The older native and browser limits and their incompatible entry shapes were independently identified; their repairs and runtime acceptance remain separate obligations.
- Workflow documentation requires generated explanatory imagery and accessible interactive, animated website equivalents. A generic screenshot-evidence capability does not enumerate this obligation, and a static picture does not establish the interactive behavior.
- Server-management descriptors must cover version-specific configuration and extension compatibility, with explicit unsupported states and negative regressions that reject replacing rich controls with generic textboxes. The current 47-capability family does not prove that those descriptors or all four execution targets are complete.

These are implementation and acceptance obligations, not newly accepted capabilities. The existing family mapping must be expanded to their individual consumers and evidence before completion.

## Explicit coverage gaps

The `minecraft-server-manager` contract had no existing product family at the recorded baseline. The current source adds a bounded owned local Java server workspace with official version discovery, verified local JAR intake and owned process controls. Its [implementation article](../servers/local-java-server-manager.md) lists the remaining targets and management capabilities. Native compilation and lifecycle acceptance remain pending. This first unit does not satisfy the complete server contract.

The shared presentation-mode foundation now includes a serialized local record, live watchers, local credential verification, effective neutral English and removal of owned settings controls. Its independently reviewed literal-name repair preserves markup-shaped names and ampersands as visible text. All 34 catalogue entries are integrated. Global routes, previously rendered surfaces, other products and native acceptance remain incomplete; see [the feature article](../interface/school-mode.md).

The current source also adds a [bounded motion authoring workspace](../interface/motion-authoring.md), expanded search-control explanations and a public-photo provider. Motion solo eligibility and atomic preset import were independently reviewed after their source repairs. Localized motion catalogue entries are integrated in source. These changes require focused native checks, real interaction and current capture evidence before acceptance. School-mode suppression, full motion authoring and comprehensive per-element guidance remain incomplete.

The catalogue was fetched and reconciled again during the October 11 refresh. An independent exact comparison passed for all 105 identities, including the five public identifier projections. No new identity was missing from the crosswalk. Requirements strengthened within existing identities still require individual implementation and acceptance.

The follow-up independent review also found that the generic guided-form and element-motion capability rows understate their current contracts, and the capability authority does not enumerate workflow diagrams. These inventory gaps remain incomplete. The current native vocabulary source updates bounds and parsing, and corrects localized rejection copy; its added fixtures and real upload workflow are not yet verified. The browser schema repair is a separate pending source unit.

Several repository delivery contracts have no corresponding family in the current UI-focused capability inventory. Their empty mapping is an explicit requirement for a new assessed row, not an exemption. Shared-status-service-only, instruction-corpus-only and Roblox-game-only requirements have individual exclusion reasons in the crosswalk.

## Current presentation and acceptance

Refresh screenshots and descriptions in the README, documentation and website when the built interface changes. Preserve original evidence and bind replacements to the actual source, executable, state, viewport, scale, theme, language, privacy review and capture method. Historical screenshots and collection-only matrices do not establish current visual acceptance.

For every applicable row, assess all required surfaces independently. Record implementation, article, localized copy, persisted state when applicable, focused positive and negative regression results, actual built interaction and inspected capture evidence. Retain incomplete status for missing or unverified proof. Source presence, a family mapping or a sibling surface never supplies the missing verdict.

## Limits

This document and its crosswalk are an inventory reconciliation, not complete feature implementation or runtime acceptance. No new build, test, native launch or publication was performed to author this slice. Existing release, installer and deployment evidence keeps its original scope and source binding.
