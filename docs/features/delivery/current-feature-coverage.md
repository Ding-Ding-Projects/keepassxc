# Current feature coverage

This assessment starts at source commit `8997ed9ced49c15c3333ae298d8e1e048cce29b1`. The current maintainer catalogue contains 105 contracts. The explicit mapping is [canonical-contract-crosswalk.json](../canonical-contract-crosswalk.json). It uses public contract identities and distinguishes application, website, repository and documented exclusions. Five identifiers use public terminology: public-screenshot-gallery, wiki-website-sync, layout-matrix, mobile-site and shared-instruction-single-file-editions. A mapped family is a place to assess the requirement, never proof that every part of the contract exists.

## Recorded baseline

The existing feature inventory has 178 rows across 89 families and two aggregate platforms. The capability inventory has 998 contracts and zero accepted evidence rows. The surface registry has 106 surfaces. Both registries identify assessment commit `07ce4dc158bcd7eb735cc309f009c531731f3133`, so their statuses must be reconciled with current source before acceptance.

Recorded application rows comprise 49 missing, 37 partial and 3 unverified. Recorded website rows comprise 85 missing, 1 partial and 3 unverified. These are historical inventory states, not a fresh implementation verdict. Current search work can supersede older missing rows only after its actual implementation and required evidence are incorporated.

## Strengthened requirements in the current review

The refreshed catalogue still contains 105 contracts. The current review includes the following requirements within existing families as well as the new server manager:

- Each appearance editor needs a complete Animation and Transition tab: renderer-supported properties, tracks, keyframes, timelines, easing, playback, triggers, sequencing and interruption, with real consumers rather than stored values alone. Reduced-motion rendering preserves authored values separately. Acceptance needs timed recordings or sampled-render evidence as well as still images.
- Each capable interactive control needs optional guidance immediately above that control. Simple controls receive a contextual step; complicated workflows receive validated progression, Back, Cancel and recovery. Every rendered element needs a unique authoritative explanation covering actual purpose, defaults, consequences, validation and recovery. Explicit per-element inventories and omission regressions cover both requirements.
- Every current presentation screenshot reference needs inspected replacement evidence after an interface-changing delivery. This includes application, website, README, articles, wiki, gallery and download presentation. A stale-current-image negative regression must reject old images presented as current, while preserving historical originals and provenance.

These are implementation and acceptance obligations, not newly accepted capabilities. The existing family mapping must be expanded to their individual consumers and evidence before completion.

## Explicit coverage gaps

The `minecraft-server-manager` contract has no existing product family. A bounded search of the material interface and website source found no corresponding implementation. It remains an application delivery gap, not a working feature or a delegated external tool.

Several repository delivery contracts have no corresponding family in the current UI-focused capability inventory. Their empty mapping is an explicit requirement for a new assessed row, not an exemption. Shared-status-service-only, instruction-corpus-only and Roblox-game-only requirements have individual exclusion reasons in the crosswalk.

## Current presentation and acceptance

Refresh screenshots and descriptions in the README, documentation and website when the built interface changes. Preserve original evidence and bind replacements to the actual source, executable, state, viewport, scale, theme, language, privacy review and capture method. Historical screenshots and collection-only matrices do not establish current visual acceptance.

For every applicable row, assess all required surfaces independently. Record implementation, article, localized copy, persisted state when applicable, focused positive and negative regression results, actual built interaction and inspected capture evidence. Retain incomplete status for missing or unverified proof. Source presence, a family mapping or a sibling surface never supplies the missing verdict.

## Limits

This document and its crosswalk are an inventory reconciliation, not complete feature implementation or runtime acceptance. No new build, test, native launch or publication was performed to author this slice. Existing release, installer and deployment evidence keeps its original scope and source binding.
