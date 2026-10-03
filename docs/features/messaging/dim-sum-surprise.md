# Dim sum surprise

Feature id: `dim-sum-surprise` · Category: Messages, language and voice

## Behaviour

The current source can show a randomly chosen dim sum dish, named in English and Cantonese, in a non-blocking auto-dismissing card (`Material::DimSum`, `src/gui/material/MaterialDimSum.cpp`). Source checks cover first run, quiet or inactive state, visible message bars and explicit suppression. These source paths are not current built-interface verification of every required exclusion.

The required startup behavior is a fresh **1% chance per launch**, at most one display in that launch, and a persisted off switch honored absolutely. It must not appear during first run, an error, an update or an active task. It must not gate startup, steal focus or delay usability; quiet and reduced-motion settings apply.

## Configuration

The implementation currently sets `OddsDenominator = 10`, yielding a 10% draw when eligible. `canShow()` deliberately ignores the retired `GUI_DimSumSurprise` preference. Consequently, the required 1% frequency and functioning persisted off switch are **not implemented by this source**. `s_drawn` and `s_shown` track the launch draw and display; their complete runtime behavior still needs acceptance evidence.

## Failure modes

The 10% denominator and ignored off preference conflict with the current requirement. Frequency, disabled/restart behavior, at-most-once behavior, and first-run/error/update/mid-task exclusions remain incomplete until corrected and exercised in a real build. This documentation correction changes no runtime behavior.

## Security considerations

The card uses bundled local image assets without a runtime image fetch. Required evidence must exclude private data and must not use a user profile to force a draw. Meaningful dish names, bilingual copy, local assets, focus behavior and automatic dismissal remain part of acceptance.

## Verification

Existing parity captures suppress the card explicitly and cannot prove its display behavior. The capability inventory separately names `one-percent-per-launch-draw`, `persisted-off-switch`, `at-most-once-per-launch` and `no-mid-task-flow`; focused inventory checks prove those obligations cannot silently disappear. These checks do not prove the runtime meets them. Behavioral tests and candidate-bound built interaction evidence remain pending.

## Suggested articles

- [Language modes and the voice catalogue](../messaging/language-modes.md)
- [Dim sum release code names](../delivery/release-code-name.md)
